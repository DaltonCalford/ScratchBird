/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		cch.cpp
 *	DESCRIPTION:	Disk cache manager
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 * 2001.07.06 Sean Leyne - Code Cleanup, removed "#ifdef READONLY_DATABASE"
 *                         conditionals, as the engine now fully supports
 *                         readonly databases.
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 */
#include "firebird.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../jrd/jrd.h"
#include "../jrd/que.h"
#include "../jrd/lck.h"
#include "../jrd/ods.h"
#include "../jrd/os/pio.h"
#include "../jrd/cch.h"
#include "iberror.h"
#include "../jrd/lls.h"
#include "../jrd/sdw.h"
#include "../jrd/tra.h"
#include "../jrd/sbm.h"
#include "../jrd/nbak.h"
#include "../common/gdsassert.h"
#include "../jrd/cch_proto.h"
#include "../jrd/err_proto.h"
#include "../yvalve/gds_proto.h"
#include "../common/isc_proto.h"
#include "../common/isc_s_proto.h"
#include "../jrd/jrd_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/ods_proto.h"
#include "../jrd/os/pio_proto.h"
#include "../jrd/sdw_proto.h"
#include "../jrd/shut_proto.h"
#include "../common/ThreadStart.h"
#include "../jrd/tra_proto.h"
#include "../common/config/config.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/classes/MsgPrint.h"
#include "../jrd/CryptoManager.h"
#include "../common/utils_proto.h"
#include "../jrd/PageToBufferMap.h"

#ifndef CDS_UNAVAILABLE
// Use lock-free lists in hash table implementation
#define HASH_USE_CDS_LIST
#endif


#ifdef HASH_USE_CDS_LIST
#include <cds/container/michael_kvlist_dhp.h>
#include "../jrd/InitCDSLib.h"
#endif


using namespace Jrd;
using namespace Ods;
using namespace ScratchBird;

// In the superserver mode, no page locks are acquired through the lock manager.
// Instead, a latching mechanism is used.  So the calls to lock subsystem for
// database pages in the original code should not be made, lest they should cause
// any undesirable side-effects.  The following defines help us achieve that.

#ifdef CCH_DEBUG
#include <stdarg.h>
IMPLEMENT_TRACE_ROUTINE(cch_trace, "CCH")
#endif

#ifdef SUPERSERVER_V2
#define CACHE_READER
#endif


static inline void PAGE_LOCK_RELEASE(thread_db* tdbb, BufferControl* bcb, Lock* lock)
{
	if (!(bcb->bcb_flags & BCB_exclusive))
	{
		CCH_TRACE(("LCK RLS %" SQUADFORMAT, lock->getKey()));
		LCK_release(tdbb, lock);
	}
}

static inline void PAGE_LOCK_ASSERT(thread_db* tdbb, BufferControl* bcb, Lock* lock)
{
	if (!(bcb->bcb_flags & BCB_exclusive))
		LCK_assert(tdbb, lock);
}

static inline void PAGE_LOCK_RE_POST(thread_db* tdbb, BufferControl* bcb, Lock* lock)
{
	if (!(bcb->bcb_flags & BCB_exclusive))
	{
		CCH_TRACE(("LCK REP %" SQUADFORMAT, lock->getKey()));
		LCK_re_post(tdbb, lock);
	}
}

enum LatchState
{
	lsOk,
	lsTimeout,
	lsPageChanged
};

static void adjust_scan_count(WIN* window, bool mustRead);
static int blocking_ast_bdb(void*);
#ifdef CACHE_READER
static void prefetch_epilogue(Prefetch*, FbStatusVector *);
static void prefetch_init(Prefetch*, thread_db*);
static void prefetch_io(Prefetch*, FbStatusVector *);
static void prefetch_prologue(Prefetch*, SLONG *);
#endif
static void cacheBuffer(Attachment* att, BufferDesc* bdb);
static void check_precedence(thread_db*, WIN*, PageNumber);
static void clear_precedence(thread_db*, BufferDesc*);
static void down_grade(thread_db*, BufferDesc*, int high = 0);
static bool expand_buffers(thread_db*, ULONG);
static BufferDesc* get_buffer(thread_db*, const PageNumber, SyncType, int);
static int get_related(BufferDesc*, PagesArray&, int, const ULONG);
static ULONG get_prec_walk_mark(BufferControl*);
static LockState lock_buffer(thread_db*, BufferDesc*, const SSHORT, const SCHAR);
static ULONG memory_init(thread_db*, BufferControl*, ULONG);
static void page_validation_error(thread_db*, win*, SSHORT);
static void purgePrecedence(BufferControl*, BufferDesc*);
static SSHORT related(BufferDesc*, const BufferDesc*, SSHORT, const ULONG);
static int write_buffer(thread_db*, BufferDesc*, const PageNumber, const bool, FbStatusVector* const,
	const bool);
static bool write_page(thread_db*, BufferDesc*, FbStatusVector* const, const bool);
static bool set_diff_page(thread_db*, BufferDesc*);
static void clear_dirty_flag_and_nbak_state(thread_db*, BufferDesc*);

static BufferDesc* get_dirty_buffer(thread_db*);


static inline void insertDirty(BufferControl* bcb, BufferDesc* bdb)
{
	if (bdb->bdb_dirty.que_forward != &bdb->bdb_dirty)
		return;

	Sync dirtySync(&bcb->bcb_syncDirtyBdbs, "insertDirty");
	dirtySync.lock(SYNC_EXCLUSIVE);

	if (bdb->bdb_dirty.que_forward != &bdb->bdb_dirty)
		return;

	bcb->bcb_dirty_count++;
	QUE_INSERT(bcb->bcb_dirty, bdb->bdb_dirty);
}

static inline void removeDirty(BufferControl* bcb, BufferDesc* bdb)
{
	if (bdb->bdb_dirty.que_forward == &bdb->bdb_dirty)
		return;

	Sync dirtySync(&bcb->bcb_syncDirtyBdbs, "removeDirty");
	dirtySync.lock(SYNC_EXCLUSIVE);

	if (bdb->bdb_dirty.que_forward == &bdb->bdb_dirty)
		return;

	fb_assert(bcb->bcb_dirty_count > 0);

	bcb->bcb_dirty_count--;
	QUE_DELETE(bdb->bdb_dirty);
	QUE_INIT(bdb->bdb_dirty);
}

static void flushDirty(thread_db* tdbb, SLONG transaction_mask, const bool sys_only);
static void flushAll(thread_db* tdbb, USHORT flush_flag);
static void flushPages(thread_db* tdbb, USHORT flush_flag, BufferDesc** begin, FB_SIZE_T count);

static void recentlyUsed(BufferDesc* bdb);
static void requeueRecentlyUsed(BufferControl* bcb);


const ULONG MIN_BUFFER_SEGMENT = 65536;

// Given pointer a field in the block, find the block

#define BLOCK(fld_ptr, type, fld) (type*)((SCHAR*) fld_ptr - offsetof(type, fld))

const int PRE_SEARCH_LIMIT	= 256;
const int PRE_EXISTS		= -1;
const int PRE_UNKNOWN		= -2;

namespace Jrd
{

#ifdef HASH_USE_CDS_LIST

template <typename T>
class ListNodeAllocator
{
public:
	typedef T value_type;

	ListNodeAllocator() {};

	template <class U>
	constexpr ListNodeAllocator(const ListNodeAllocator<U>&) noexcept {}

	T* allocate(std::size_t n);
	void deallocate(T* p, std::size_t n);

private:
};

struct BdbTraits : public cds::container::michael_list::traits
{
	typedef ListNodeAllocator<int> allocator;
	//typedef std::less<PageNumber> compare;
};

typedef cds::container::MichaelKVList<cds::gc::DHP, PageNumber, BufferDesc*, BdbTraits> BdbList;

#endif // HASH_USE_CDS_LIST


class BCBHashTable
{
#ifdef HASH_USE_CDS_LIST
	using chain_type = BdbList;
#else
	using chain_type = que;
#endif

public:
	BCBHashTable(MemoryPool& pool, ULONG count) :
		m_pool(pool),
		m_count(0),
		m_chains(nullptr)
	{
		resize(count);
	}

	~BCBHashTable()
	{
		clear();
	}

	void resize(ULONG count);
	void clear();

	BufferDesc* find(const PageNumber& page) const;

	// tries to put bdb into hash slot by page
	// if succeed, removes bdb from old slot, if necessary, and returns NULL
	// else, returns BufferDesc that is currently occupies target slot
	BufferDesc* emplace(BufferDesc* bdb, const PageNumber& page, bool remove);

	void remove(BufferDesc* bdb);
private:
	ULONG hash(const PageNumber& pageno) const
	{
		return pageno.getPageNum() % m_count;
	}

	MemoryPool& m_pool;
	ULONG m_count;
	chain_type* m_chains;
};

}


void CCH_clean_page(thread_db* tdbb, PageNumber page)
{
/**************************************
 *  C C H _ c l e a n _ p a g e
 **************************************
 *
 * Functional description
 *  Clear dirty status and dependencies. Buffer must be unused.
 *  If buffer with given page number is not found - it is OK, do nothing.
 *  Used to remove dirty pages from cache after releasing temporary objects.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();

	fb_assert(page.getPageNum() > 0);
	fb_assert(page.isTemporary());
	if (!page.isTemporary())
		return;

	BufferControl* bcb = dbb->dbb_bcb;
	BufferDesc* bdb = NULL;
	{
#ifndef HASH_USE_CDS_LIST
		Sync bcbSync(&bcb->bcb_syncObject, "CCH_clean_page");
		bcbSync.lock(SYNC_SHARED);
#endif

		bdb = bcb->bcb_hashTable->find(page);
		if (!bdb)
			return;

		fb_assert(bdb->bdb_use_count == 0);
		if (!bdb->addRefConditional(tdbb, SYNC_EXCLUSIVE))
			return;
	}

	// temporary pages should have no precedence relationship
	if (!QUE_EMPTY(bdb->bdb_higher))
		purgePrecedence(bcb, bdb);

	fb_assert(QUE_EMPTY(bdb->bdb_higher));
	fb_assert(QUE_EMPTY(bdb->bdb_lower));

	if (!QUE_EMPTY(bdb->bdb_lower) || !QUE_EMPTY(bdb->bdb_higher))
	{
		bdb->release(tdbb, true);
		return;
	}

	if (bdb->bdb_flags & (BDB_dirty | BDB_db_dirty))
	{
		bdb->bdb_difference_page = 0;
		bdb->bdb_transactions = 0;
		bdb->bdb_mark_transaction = 0;

		if (!(bdb->bdb_bcb->bcb_flags & BCB_keep_pages))
			removeDirty(dbb->dbb_bcb, bdb);

		bdb->bdb_flags &= ~(BDB_must_write | BDB_system_dirty | BDB_db_dirty);
		clear_dirty_flag_and_nbak_state(tdbb, bdb);
	}

	{
		Sync lruSync(&bcb->bcb_syncLRU, "CCH_release");
		lruSync.lock(SYNC_EXCLUSIVE);

		if (bdb->bdb_flags & BDB_lru_chained)
			requeueRecentlyUsed(bcb);

		QUE_DELETE(bdb->bdb_in_use);
		QUE_APPEND(bcb->bcb_in_use, bdb->bdb_in_use);
	}

	bdb->release(tdbb, true);
}


int CCH_down_grade_dbb(void* ast_object)
{
/**************************************
 *
 *	C C H _ d o w n _ g r a d e _ d b b
 *
 **************************************
 *
 * Functional description
 *	Down grade the lock on the database in response to a blocking
 *	AST.
 *
 **************************************/
	Database* dbb = static_cast<Database*>(ast_object);

	try
	{
		Lock* const lock = dbb->dbb_lock;

		AsyncContextHolder tdbb(dbb, FB_FUNCTION);

		dbb->dbb_ast_flags |= DBB_blocking;

		// Process the database shutdown request, if any

		if (SHUT_blocking_ast(tdbb, true))
			return 0;

		SyncLockGuard dsGuard(&dbb->dbb_sync, SYNC_EXCLUSIVE, FB_FUNCTION);

		// If we are already shared, there is nothing more we can do.
		// If any case, the other guy probably wants exclusive access,
		// and we can't give it anyway

		if ((lock->lck_logical == LCK_SW) || (lock->lck_logical == LCK_SR))
		{
			// Fake conversion to the same level as we already own.
			// This trick re-enables the AST delivery.
			LCK_convert(tdbb, lock, lock->lck_logical, LCK_NO_WAIT);
			return 0;
		}

		if (dbb->dbb_flags & DBB_bugcheck)
		{
			LCK_convert(tdbb, lock, LCK_SW, LCK_WAIT);
			dbb->dbb_ast_flags &= ~DBB_blocking;
			return 0;
		}

		// If we are supposed to be exclusive, stay exclusive

		if ((dbb->dbb_flags & DBB_exclusive) || dbb->isShutdown(shut_mode_single))
			return 0;

		// Assert any page locks that have been requested, but not asserted

		dbb->dbb_ast_flags |= DBB_assert_locks;

		BufferControl* bcb = dbb->dbb_bcb;
		if (bcb)
		{
			SyncLockGuard bcbSync(&bcb->bcb_syncObject, SYNC_EXCLUSIVE, FB_FUNCTION);
			bcb->bcb_flags &= ~BCB_exclusive;

			for (auto blk : bcb->bcb_bdbBlocks)
			{
				for (BufferDesc* bdb = blk.m_bdbs; bdb < blk.m_bdbs + blk.m_count; bdb++)
				{
					// Acquire EX latch to avoid races with LCK_release (called by CCH_release)
					// or LCK_lock (by lock_buffer) in main thread. Take extra care to avoid
					// deadlock with CCH_handoff. See CORE-5436.

					Sync sync(&bdb->bdb_syncPage, FB_FUNCTION);

					while (!sync.lockConditional(SYNC_EXCLUSIVE))
					{
						SyncUnlockGuard bcbUnlock(bcbSync);
						Thread::sleep(1);
					}

					PAGE_LOCK_ASSERT(tdbb, bcb, bdb->bdb_lock);
				}
			}
		}

		// Down grade the lock on the database itself

		if (lock->lck_physical == LCK_EX)
			LCK_convert(tdbb, lock, LCK_PW, LCK_WAIT);	// This lets waiting cache manager in first
		else if (lock->lck_physical == LCK_PW)
			LCK_convert(tdbb, lock, LCK_SW, LCK_WAIT);
		else
			fb_assert(lock->lck_physical == 0);

		dbb->dbb_ast_flags &= ~DBB_blocking;
	}
	catch (const ScratchBird::Exception&)
	{} // no-op

	return 0;
}


bool CCH_exclusive(thread_db* tdbb, USHORT level, SSHORT wait_flag, ScratchBird::Sync* guard)
{
/**************************************
 *
 *	C C H _ e x c l u s i v e
 *
 **************************************
 *
 * Functional description
 *	Get exclusive access to a database.  If we get it, return true.
 *	If the wait flag is FALSE, and we can't get it, give up and
 *	return false. There are two levels of database exclusivity: LCK_PW
 *	guarantees there are no normal users in the database while LCK_EX
 *	additionally guarantees background database processes like the
 *	shared cache manager have detached.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();

	if (dbb->dbb_flags & DBB_shared)
	{
		if (!CCH_exclusive_attachment(tdbb, level, wait_flag, guard))
			return false;
	}

	Lock* lock = dbb->dbb_lock;
	if (!lock)
		return false;

	dbb->dbb_flags |= DBB_exclusive;

	switch (level)
	{
	case LCK_PW:
		if (lock->lck_physical >= LCK_PW || LCK_convert(tdbb, lock, LCK_PW, wait_flag))
			return true;
		break;

	case LCK_EX:
		if (lock->lck_physical == LCK_EX || LCK_convert(tdbb, lock, LCK_EX, wait_flag))
			return true;
		break;

	default:
		break;
	}

	// Clear the status vector, as our callers check the return value
	// and throw custom exceptions themselves
	fb_utils::init_status(tdbb->tdbb_status_vector);

	// If we are supposed to wait (presumably patiently),
	// but can't get the lock, generate an error

	if (wait_flag == LCK_WAIT)
		ERR_post(Arg::Gds(isc_deadlock));

	dbb->dbb_flags &= ~DBB_exclusive;

	return false;
}


bool CCH_exclusive_attachment(thread_db* tdbb, USHORT level, SSHORT wait_flag, Sync* exGuard)
{
/**************************************
 *
 *	C C H _ e x c l u s i v e _ a t t a c h m e n t
 *
 **************************************
 *
 * Functional description
 *	Get exclusive access to a database. If we get it, return true.
 *	If the wait flag is FALSE, and we can't get it, give up and
 *	return false.
 *
 **************************************/
	const int CCH_EXCLUSIVE_RETRY_INTERVAL = 10;	// retry interval in milliseconds

	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	Sync dsGuard(&dbb->dbb_sync, FB_FUNCTION);

	const bool exLock = dbb->dbb_sync.ourExclusiveLock();
	if (!exLock)
		dsGuard.lock(level != LCK_none ? SYNC_EXCLUSIVE : SYNC_SHARED);
	else
		fb_assert(exGuard);

	Jrd::Attachment* const attachment = tdbb->getAttachment();

	if (attachment->att_flags & ATT_exclusive)
		return true;

	attachment->att_flags |= (level == LCK_none) ? ATT_attach_pending : ATT_exclusive_pending;

	const SLONG timeout = (wait_flag == LCK_WAIT) ? 1L << 30 : (-wait_flag * 1000 / CCH_EXCLUSIVE_RETRY_INTERVAL);

	// If requesting exclusive database access, then re-position attachment as the
	// youngest so that pending attachments may pass.

	if (level != LCK_none)
	{
		for (Jrd::Attachment** ptr = &dbb->dbb_attachments; *ptr; ptr = &(*ptr)->att_next)
		{
			if (*ptr == attachment)
			{
				*ptr = attachment->att_next;
				break;
			}
		}
		attachment->att_next = dbb->dbb_attachments;
		dbb->dbb_attachments = attachment;

		if (!exLock)
			dsGuard.downgrade(SYNC_SHARED);
	}

	for (SLONG remaining = timeout; remaining >= 0; remaining -= CCH_EXCLUSIVE_RETRY_INTERVAL)
	{
		try
		{
			bool found = false;
			for (Jrd::Attachment* other_attachment = attachment->att_next; other_attachment;
				 other_attachment = other_attachment->att_next)
			{
				if (level == LCK_none)
				{
					// Wait for other attachments requesting exclusive access
					if (other_attachment->att_flags & (ATT_exclusive | ATT_exclusive_pending))
					{
						found = true;
						break;
					}

					// Forbid multiple attachments in single-user maintenance mode
					if (other_attachment != attachment && dbb->isShutdown(shut_mode_single))
					{
						found = true;
						break;
					}
				}
				else
				{
					// Requesting exclusive database access
					found = true;
					if (other_attachment->att_flags & ATT_exclusive_pending)
					{
						if (wait_flag == LCK_WAIT)
							ERR_post(Arg::Gds(isc_deadlock));

						attachment->att_flags &= ~ATT_exclusive_pending;
						return false;
					}

					break;
				}
			}

			if (!found)
			{
				if (level != LCK_none)
					attachment->att_flags |= ATT_exclusive;

				attachment->att_flags &= ~(ATT_exclusive_pending | ATT_attach_pending);
				return true;
			}

			// Our thread needs to sleep for CCH_EXCLUSIVE_RETRY_INTERVAL milliseconds.

			if (remaining >= CCH_EXCLUSIVE_RETRY_INTERVAL)
			{
				SyncUnlockGuard unlock(exLock ? (*exGuard) : dsGuard);
				tdbb->reschedule();
				Thread::sleep(CCH_EXCLUSIVE_RETRY_INTERVAL);
			}

		} // try
		catch (const Exception&)
		{
			attachment->att_flags &= ~(ATT_exclusive_pending | ATT_attach_pending);
			throw;
		}
	}

	attachment->att_flags &= ~(ATT_exclusive_pending | ATT_attach_pending);
	return false;
}


bool CCH_expand(thread_db* tdbb, ULONG number)
{
/**************************************
 *
 *	C C H _ e x p a n d
 *
 **************************************
 *
 * Functional description
 *	Expand the cache to at least a given number of buffers.  If
 *	it's already that big, don't do anything.
 *
 **************************************/
	SET_TDBB(tdbb);

	return expand_buffers(tdbb, number);
}


pag* CCH_fake(thread_db* tdbb, WIN* window, int wait)
{
/**************************************
 *
 *	C C H _ f a k e
 *
 **************************************
 *
 * Functional description
 *	Fake a fetch to a page.  Rather than reading it, however,
 *	zero it in memory.  This is used when allocating a new page.
 *
 * input
 *	wait:	1 => Wait as long as necessary to get the latch.
 *				This can cause deadlocks of course.
 *			0 => If the latch can't be acquired immediately,
 *				or an IO would be necessary, then give
 *				up and return 0.
 *			<negative number> => Latch timeout interval in seconds.
 *
 * return
 *	pag pointer if successful.
 *	NULL pointer if timeout occurred (only possible if wait <> 1).
 *	NULL pointer if wait=0 and the faked page would have to be
 *			before reuse.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();
	BufferControl *bcb = dbb->dbb_bcb;

	CCH_TRACE(("FAKE    %d:%06d", window->win_page.getPageSpaceID(), window->win_page.getPageNum()));

	// if there has been a shadow added recently, go out and
	// find it before we grant any more write locks

	if (dbb->dbb_ast_flags & DBB_get_shadows)
		SDW_get_shadows(tdbb);

	BufferDesc* bdb = get_buffer(tdbb, window->win_page, SYNC_EXCLUSIVE, wait);
	if (!bdb)
		return NULL;			// latch timeout occurred

	fb_assert(bdb->bdb_page == window->win_page);

	// If a dirty orphaned page is being reused - better write it first
	// to clear current precedences and checkpoint state. This would also
	// update the bcb_free_pages field appropriately.

	if (bdb->bdb_flags & (BDB_dirty | BDB_db_dirty))
	{
		// If the caller didn't want to wait at all, then
		// return 'try to fake an other page' to the caller.

		if (!wait)
		{
			bdb->release(tdbb, true);
			return NULL;
		}

		if (!write_buffer(tdbb, bdb, bdb->bdb_page, true, tdbb->tdbb_status_vector, true))
		{
			CCH_unwind(tdbb, true);
		}
	}
	else if (QUE_NOT_EMPTY(bdb->bdb_lower))
	{
		// Clear residual precedence left over from AST-level I/O.
		Sync syncPrec(&bcb->bcb_syncPrecedence, "CCH_fake");
		syncPrec.lock(SYNC_EXCLUSIVE);
		clear_precedence(tdbb, bdb);
	}

	// Here the page must not be dirty and have no backup lock owner
	fb_assert((bdb->bdb_flags & (BDB_dirty | BDB_db_dirty)) == 0);
	fb_assert(bdb->bdb_page == window->win_page);

	bdb->bdb_flags &= BDB_lru_chained;	// yes, clear all except BDB_lru_chained
	bdb->bdb_flags |= (BDB_writer | BDB_faked);
	bdb->bdb_scan_count = 0;

	if (!(bcb->bcb_flags & BCB_exclusive))
		lock_buffer(tdbb, bdb, LCK_WAIT, pag_undefined);

	MOVE_CLEAR(bdb->bdb_buffer, (SLONG) dbb->dbb_page_size);
	bdb->bdb_buffer->pag_pageno = window->win_page.getPageNum();
	window->win_buffer = bdb->bdb_buffer;
	window->win_bdb = bdb;
	window->win_flags = 0;
	CCH_MARK(tdbb, window);

	return bdb->bdb_buffer;
}


pag* CCH_fetch(thread_db* tdbb, WIN* window, int lock_type, SCHAR page_type, int wait,
	const bool read_shadow)
{
/**************************************
 *
 *	C C H _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Fetch a specific page.  If it's already in cache,
 *	so much the better.
 *
 * input
 *	wait:	1 => Wait as long as necessary to get the latch.
 *				This can cause deadlocks of course.
 *			0 => If the latch can't be acquired immediately,
 *				give up and return 0.
 *	      		<negative number> => Latch timeout interval in seconds.
 *
 * return
 *	PAG if successful.
 *	NULL pointer if timeout occurred (only possible if wait <> 1).
 *
 **************************************/
	SET_TDBB(tdbb);

	const LockState lockState = CCH_fetch_lock(tdbb, window, lock_type, wait, page_type);
	BufferDesc* bdb = window->win_bdb;
	SyncType syncType = (lock_type >= LCK_write) ? SYNC_EXCLUSIVE : SYNC_SHARED;

	switch (lockState)
	{
	case lsLocked:
		CCH_TRACE(("FE PAGE %d:%06d", window->win_page.getPageSpaceID(), window->win_page.getPageNum()));
		CCH_fetch_page(tdbb, window, read_shadow);	// must read page from disk
		if (syncType != SYNC_EXCLUSIVE)
			bdb->downgrade(syncType);
		break;

	case lsLatchTimeout:
	case lsLockTimeout:
		return NULL;			// latch or lock timeout
	}

	adjust_scan_count(window, lockState == lsLocked);

	// Validate the fetched page matches the expected type

	if (bdb->bdb_buffer->pag_type != page_type && page_type != pag_undefined)
		page_validation_error(tdbb, window, page_type);

	return window->win_buffer;
}


LockState CCH_fetch_lock(thread_db* tdbb, WIN* window, int lock_type, int wait, SCHAR page_type)
{
/**************************************
 *
 *	C C H _ f e t c h _ l o c k
 *
 **************************************
 *
 * Functional description
 *	Fetch a latch and lock for a specific page.
 *
 * input
 *
 *	wait:
 *	LCK_WAIT (1)	=> Wait as long a necessary to get the lock.
 *		This can cause deadlocks of course.
 *
 *	LCK_NO_WAIT (0)	=>
 *		If the latch can't be acquired immediately, give up and return -2.
 *		If the lock can't be acquired immediately, give up and return -1.
 *
 *	<negative number>	=> Lock (latch) timeout interval in seconds.
 *
 * return
 *	0:	fetch & lock were successful, page doesn't need to be read.
 *	1:	fetch & lock were successful, page needs to be read from disk.
 *	-1:	lock timed out, fetch failed.
 *	-2:	latch timed out, fetch failed, lock not attempted.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();
	BufferControl* const bcb = dbb->dbb_bcb;

	// if there has been a shadow added recently, go out and
	// find it before we grant any more write locks

	if (dbb->dbb_ast_flags & DBB_get_shadows)
		SDW_get_shadows(tdbb);

	// Look for the page in the cache.

	BufferDesc* bdb = get_buffer(tdbb, window->win_page,
		((lock_type >= LCK_write) ? SYNC_EXCLUSIVE : SYNC_SHARED), wait);

	if (wait != 1 && bdb == 0)
		return lsLatchTimeout; // latch timeout

	fb_assert(bdb->bdb_page == window->win_page);
	if (!(bdb->bdb_flags & BDB_read_pending))
		fb_assert(bdb->bdb_buffer->pag_pageno == window->win_page.getPageNum());
	else
		fb_assert(bdb->ourExclusiveLock() || bdb->bdb_lock && bdb->bdb_lock->lck_logical == LCK_none);

	if (lock_type >= LCK_write)
		bdb->bdb_flags |= BDB_writer;

	window->win_bdb = bdb;
	window->win_buffer = bdb->bdb_buffer;

	if (bcb->bcb_flags & BCB_exclusive)
		return (bdb->bdb_flags & BDB_read_pending) ? lsLocked : lsLockedHavePage;

	// lock_buffer returns 0 or 1 or -1.
	const LockState lock_result = lock_buffer(tdbb, bdb, wait, page_type);

	return lock_result;
}

void CCH_fetch_page(thread_db* tdbb, WIN* window, const bool read_shadow)
{
/**************************************
 *
 *	C C H _ f e t c h _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Fetch a specific page.  If it's already in cache,
 *	so much the better.  When "compute_checksum" is 1, compute
 * 	the checksum of the page.  When it is 2, compute
 *	the checksum only when the page type is nonzero.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	BufferDesc* bdb = window->win_bdb;
	BufferControl* bcb = bdb->bdb_bcb;

	FbStatusVector* const status = tdbb->tdbb_status_vector;

	pag* page = bdb->bdb_buffer;
	bdb->bdb_incarnation = ++bcb->bcb_page_incarnation;

	tdbb->bumpStats(RuntimeStatistics::PAGE_READS);

	PageSpace* pageSpace = dbb->dbb_page_manager.findPageSpace(bdb->bdb_page.getPageSpaceID());
	fb_assert(pageSpace);

	jrd_file* file = pageSpace->file;
	const bool isTempPage = pageSpace->isTemporary();

	/*
	We will read a page, and if there is an I/O error we will try to
	use the shadow file, and try reading again, for a maximum of
	3 tries, before it gives up.

	The read_shadow flag is set to false only in the call to
	FETCH_NO_SHADOW, which is only called from validate
	code.

	read_shadow = false -> IF an I/O error occurs give up (exit
	the loop, clean up, and return). So the caller,
	validate in most cases, can know about it and attempt
	to remedy the situation.

	read_shadow = true -> IF an I/O error occurs attempt
	to rollover to the shadow file.  If the I/O error is
	persistant (more than 3 times) error out of the routine by
	calling CCH_unwind, and eventually punting out.
	*/

	class Pio : public CryptoManager::IOCallback
	{
	public:
		Pio(jrd_file* f, BufferDesc* b, bool tp, bool rs, PageSpace* ps)
			: file(f), bdb(b), isTempPage(tp),
			  read_shadow(rs), pageSpace(ps)
		{ }

		bool callback(thread_db* tdbb, FbStatusVector* status, Ods::pag* page)
		{
			Database *dbb = tdbb->getDatabase();
			int retryCount = 0;

			while (!PIO_read(tdbb, file, bdb, page, status))
	 		{
				if (isTempPage || !read_shadow)
					return false;

				if (!CCH_rollover_to_shadow(tdbb, dbb, file, false))
 					return false;

				if (file != pageSpace->file)
					file = pageSpace->file;
				else
				{
					if (retryCount++ == 3)
					{
						gds__log("IO error loop Unwind to avoid a hang\n");
						return false;
					}
				}
 			}

			return true;
		}
	private:
		jrd_file* file;
		BufferDesc* bdb;
		bool isTempPage;
		bool read_shadow;
		PageSpace* pageSpace;
	};

	const auto bm = dbb->dbb_backup_manager;
	BackupManager::StateReadGuard stateGuard(tdbb);
	const auto backupState = bm->getState();
	fb_assert(backupState != Ods::hdr_nbak_unknown);

	ULONG diff_page = 0;
	if (!isTempPage && backupState != Ods::hdr_nbak_normal)
	{
		diff_page = bm->getPageIndex(tdbb, bdb->bdb_page.getPageNum());
		NBAK_TRACE(("Reading page %d:%06d, state=%d, diff page=%d",
			bdb->bdb_page.getPageSpaceID(), bdb->bdb_page.getPageNum(), (int) backupState, diff_page));
	}

	// In merge mode, if we are reading past beyond old end of file and page is in .delta file
	// then we maintain actual page in difference file. Always read it from there.
	if (isTempPage || backupState == Ods::hdr_nbak_normal || !diff_page)
	{
		fb_assert(bdb->bdb_page == window->win_page);

		NBAK_TRACE(("Reading page %d:%06d, state=%d, diff page=%d from DISK",
			bdb->bdb_page.getPageSpaceID(), bdb->bdb_page.getPageNum(), (int) backupState, diff_page));

		// Read page from disk as normal
		Pio io(file, bdb, isTempPage, read_shadow, pageSpace);
		if (!dbb->dbb_crypto_manager->read(tdbb, status, page, &io))
		{
			if (read_shadow && !isTempPage)
			{
				PAGE_LOCK_RELEASE(tdbb, bcb, bdb->bdb_lock);
				CCH_unwind(tdbb, true);
			}
		}
		fb_assert(bdb->bdb_page == window->win_page);
		fb_assert(bdb->bdb_buffer->pag_pageno == window->win_page.getPageNum() ||
			bdb->bdb_buffer->pag_type == pag_undefined &&
			bdb->bdb_buffer->pag_generation == 0 &&
			bdb->bdb_buffer->pag_scn == 0);
	}
	else
	{
		NBAK_TRACE(("Reading page %d, state=%d, diff page=%d from DIFFERENCE",
			bdb->bdb_page, bak_state, diff_page));
		if (!bm->readDifference(tdbb, diff_page, page))
		{
			PAGE_LOCK_RELEASE(tdbb, bcb, bdb->bdb_lock);
			CCH_unwind(tdbb, true);
		}

		if (page->pag_type == 0 && page->pag_generation == 0 && page->pag_scn == 0)
		{
			// We encountered a page which was allocated, but never written to the
			// difference file. In this case we try to read the page from database. With
			// this approach if the page was old we get it from DISK, and if the page
			// was new IO error (EOF) or BUGCHECK (checksum error) will be the result.
			// Engine is not supposed to read a page which was never written unless
			// this is a merge process.
			NBAK_TRACE(("Re-reading page %d, state=%d, diff page=%d from DISK",
				bdb->bdb_page, (int) backupState, diff_page));

			Pio io(file, bdb, false, read_shadow, pageSpace);
			if (!dbb->dbb_crypto_manager->read(tdbb, status, page, &io))
			{
				if (read_shadow)
				{
					PAGE_LOCK_RELEASE(tdbb, bcb, bdb->bdb_lock);
					CCH_unwind(tdbb, true);
				}
			}
		}
	}

	bdb->bdb_flags &= ~(BDB_not_valid | BDB_read_pending);
	window->win_buffer = bdb->bdb_buffer;
}


void CCH_forget_page(thread_db* tdbb, WIN* window)
{
/**************************************
 *
 *	C C H _ f o r g e t _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Page was faked but can't be written on disk. Most probably because
 *	of out of disk space. Release page buffer and others resources and
 *	unlink page from various queues
 *
 **************************************/
	SET_TDBB(tdbb);
	BufferDesc* bdb = window->win_bdb;
	Database* dbb = tdbb->getDatabase();

	if (window->win_page != bdb->bdb_page || bdb->bdb_buffer->pag_type != pag_undefined)
		return;	// buffer was reassigned or page was reused

	window->win_bdb = NULL;

	if (bdb->bdb_flags & BDB_io_error)
		dbb->dbb_flags &= ~DBB_suspend_bgio;

	clear_dirty_flag_and_nbak_state(tdbb, bdb);
	BufferControl* bcb = dbb->dbb_bcb;

	removeDirty(bcb, bdb);

	// remove from LRU list
	{
		SyncLockGuard lruSync(&bcb->bcb_syncLRU, SYNC_EXCLUSIVE, FB_FUNCTION);
		requeueRecentlyUsed(bcb);
		QUE_DELETE(bdb->bdb_in_use);
	}

	// remove from hash table and put into empty list
#ifndef HASH_USE_CDS_LIST
	{
		SyncLockGuard bcbSync(&bcb->bcb_syncObject, SYNC_EXCLUSIVE, FB_FUNCTION);
		bcb->bcb_hashTable->remove(bdb);
		QUE_INSERT(bcb->bcb_empty, bdb->bdb_que);
		bcb->bcb_inuse--;
	}
#else
	bcb->bcb_hashTable->remove(bdb);

	{
		SyncLockGuard syncEmpty(&bcb->bcb_syncEmpty, SYNC_EXCLUSIVE, FB_FUNCTION);
		QUE_INSERT(bcb->bcb_empty, bdb->bdb_que);
		bcb->bcb_inuse--;
	}
#endif

	bdb->bdb_flags = 0;

	if (tdbb->tdbb_flags & TDBB_no_cache_unwind)
		bdb->release(tdbb, true);
}


void CCH_fini(thread_db* tdbb)
{
/**************************************
 *
 *	C C H _ f i n i
 *
 **************************************
 *
 * Functional description
 *	Shut down buffer operation.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	SyncLockGuard dsGuard(&dbb->dbb_sync, SYNC_EXCLUSIVE, FB_FUNCTION);

	BufferControl* const bcb = dbb->dbb_bcb;

	if (!bcb)
		return;

	delete bcb->bcb_hashTable;

	for (auto blk : bcb->bcb_bdbBlocks)
	{
		for (ULONG i = 0; i < blk.m_count; i++)
		{
			BufferDesc& bdb = blk.m_bdbs[i];

			if (bdb.bdb_lock)
				bdb.bdb_lock->~Lock();
			bdb.~BufferDesc();
		}
	}

	bcb->bcb_bdbBlocks.clear();
	bcb->bcb_count = 0;

	while (bcb->bcb_memory.hasData())
		bcb->bcb_bufferpool->deallocate(bcb->bcb_memory.pop());

	BufferControl::destroy(bcb);
	dbb->dbb_bcb = NULL;
}


void CCH_flush(thread_db* tdbb, USHORT flush_flag, TraNumber tra_number)
{
/**************************************
 *
 *	C C H _ f l u s h
 *
 **************************************
 *
 * Functional description
 *	Flush all buffers.  If the release flag is set,
 *	release all locks.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();

	// note that some of the code for btc_flush()
	// replicates code in the for loop
	// to minimize call overhead -- changes should be made in both places

	if (flush_flag & (FLUSH_TRAN | FLUSH_SYSTEM))
	{
		const ULONG transaction_mask = tra_number ? 1L << (tra_number & (BITS_PER_LONG - 1)) : 0;
		bool sys_only = false;
		if (!transaction_mask && (flush_flag & FLUSH_SYSTEM))
			sys_only = true;

#ifdef SUPERSERVER_V2
		BufferControl* bcb = dbb->dbb_bcb;
		//if (!dbb->dbb_wal && A && B) becomes
		//if (true && A && B) then finally (A && B)
		if (!(dbb->dbb_flags & DBB_force_write) && transaction_mask)
		{
			dbb->dbb_flush_cycle |= transaction_mask;
			if (!(bcb->bcb_flags & BCB_writer_active))
				bcb->bcb_writer_sem.release();
		}
		else
#endif
			flushDirty(tdbb, transaction_mask, sys_only);
	}
	else
		flushAll(tdbb, flush_flag);

	//
	// Check if flush needed
	//
	const int max_unflushed_writes = dbb->dbb_config->getMaxUnflushedWrites();
	const time_t max_unflushed_write_time = dbb->dbb_config->getMaxUnflushedWriteTime();
	bool max_num = (max_unflushed_writes >= 0);
	bool max_time = (max_unflushed_write_time >= 0);

	bool doFlush = false;

	PageSpace* pageSpaceID = dbb->dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
	jrd_file* main_file = pageSpaceID->file;

	// Avoid flush while creating and restoring database

	const Jrd::Attachment* att = tdbb->getAttachment();
	const bool dontFlush = (dbb->dbb_flags & DBB_creating) ||
		(dbb->isShutdown() && att && (att->att_flags & (ATT_creator | ATT_system)));

	if (!(main_file->fil_flags & FIL_force_write) && (max_num || max_time) && !dontFlush)
	{
		const time_t now = time(0);

		SyncLockGuard guard(&dbb->dbb_flush_count_mutex, SYNC_EXCLUSIVE, FB_FUNCTION);

		// If this is the first commit set last_flushed_write to now
		if (!dbb->last_flushed_write)
			dbb->last_flushed_write = now;

		const bool forceFlush = (flush_flag & FLUSH_ALL);

		// test max_num condition and max_time condition
		max_num = max_num && (dbb->unflushed_writes == max_unflushed_writes);
		max_time = max_time && (now - dbb->last_flushed_write > max_unflushed_write_time);

		if (forceFlush || max_num || max_time)
		{
			doFlush = true;
			dbb->unflushed_writes = 0;
			dbb->last_flushed_write = now;
		}
		else
		{
			dbb->unflushed_writes++;
		}
	}

	if (doFlush)
	{
		PIO_flush(tdbb, main_file);

		for (Shadow* shadow = dbb->dbb_shadow; shadow; shadow = shadow->sdw_next)
			PIO_flush(tdbb, shadow->sdw_file);

		BackupManager* bm = dbb->dbb_backup_manager;
		if (bm && !bm->isShutDown())
		{
			BackupManager::StateReadGuard stateGuard(tdbb);
			const auto backupState = bm->getState();
			if (backupState == Ods::hdr_nbak_stalled || backupState == Ods::hdr_nbak_merge)
				bm->flushDifference(tdbb);
		}
	}

	// take the opportunity when we know there are no pages
	// in cache to check that the shadow(s) have not been
	// scheduled for shutdown or deletion

	SDW_check(tdbb);
}

void CCH_flush_ast(thread_db* tdbb)
{
/**************************************
 *
 *	C C H _ f l u s h _ a s t
 *
 **************************************
 *
 * Functional description
 *	Flush all buffers coming from database file.
 *	Should be called from AST
 *
 **************************************/
	SET_TDBB(tdbb);

	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = dbb->dbb_bcb;

	if (bcb->bcb_flags & BCB_exclusive)
		CCH_flush(tdbb, FLUSH_ALL, 0);
	else
	{
		SyncLockGuard bcbSync(&bcb->bcb_syncObject, SYNC_SHARED, FB_FUNCTION);

		// Do some fancy footwork to make sure that pages are
		// not removed from the btc tree at AST level.  Then
		// restore the flag to whatever it was before.
		const bool keep_pages = bcb->bcb_flags & BCB_keep_pages;
		bcb->bcb_flags |= BCB_keep_pages;

		for (auto blk : bcb->bcb_bdbBlocks)
		{
			for (BufferDesc* bdb = blk.m_bdbs; bdb < blk.m_bdbs + blk.m_count; bdb++)
			{
				if (bdb->bdb_flags & (BDB_dirty | BDB_db_dirty))
					down_grade(tdbb, bdb, 1);
			}
		}

		if (!keep_pages)
			bcb->bcb_flags &= ~BCB_keep_pages;
	}
}

bool CCH_free_page(thread_db* tdbb)
{
/**************************************
 *
 *	C C H _ f r e e _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Check if the cache is below its free pages
 *	threshold and write a page on the LRU tail.
 *
 **************************************/

	// Called by VIO/garbage_collector() when it is idle to
	// help quench the thirst for free pages.

	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = dbb->dbb_bcb;

	if (dbb->readOnly())
		return false;

	BufferDesc* bdb;

	if ((bcb->bcb_flags & BCB_free_pending) && (bdb = get_dirty_buffer(tdbb)))
	{
		if (write_buffer(tdbb, bdb, bdb->bdb_page, true, tdbb->tdbb_status_vector, true))
			return true;

		CCH_unwind(tdbb, false);
	}

	return false;
}


SLONG CCH_get_incarnation(WIN* window)
{
/**************************************
 *
 *	C C H _ g e t _ i n c a r n a t i o n
 *
 **************************************
 *
 * Functional description
 *	Get page incarnation associated with buffer.
 *
 **************************************/
	return window->win_bdb->bdb_incarnation;
}


void CCH_get_related(thread_db* tdbb, PageNumber page, PagesArray &lowPages)
{
/**************************************
 *
 *	C C H _ g e t _ r e l a t e d
 *
 **************************************
 *
 * Functional description
 *	Collect all pages, dependent on given page (i.e. all pages which must be
 *  written after given page). To do it, walk low part of precedence graph
 *  starting from given page and put its numbers into array.
 *
 **************************************/

	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = dbb->dbb_bcb;

#ifndef HASH_USE_CDS_LIST
	Sync bcbSync(&bcb->bcb_syncObject, "CCH_get_related");
	bcbSync.lock(SYNC_SHARED);
#endif

	BufferDesc* bdb = bcb->bcb_hashTable->find(page);
#ifndef HASH_USE_CDS_LIST
	bcbSync.unlock();
#endif

	if (bdb)
	{
		Sync precSync(&bcb->bcb_syncPrecedence, "CCH_get_related");
		precSync.lock(SYNC_EXCLUSIVE);

		const ULONG mark = get_prec_walk_mark(bcb);
		get_related(bdb, lowPages, PRE_SEARCH_LIMIT, mark);
	}
}


pag* CCH_handoff(thread_db*	tdbb, WIN* window, ULONG page, int lock, SCHAR page_type,
	int wait, const bool release_tail)
{
/**************************************
 *
 *	C C H _ h a n d o f f
 *
 **************************************
 *
 * Functional description
 *	Follow a pointer handing off the lock.  Fetch the new page
 *	before retiring the old page lock.
 *
 * input
 *	wait:	1 => Wait as long as necessary to get the latch.
 *				This can cause deadlocks of course.
 *			0 => If the latch can't be acquired immediately,
 *				give up and return 0.
 *	      		<negative number> => Latch timeout interval in seconds.
 *
 * return
 *	PAG if successful.
 *	0 if a latch timeout occurred (only possible if wait <> 1).
 *		The latch on the fetched page is downgraded to shared.
 *		The fetched page is unmarked.
 *
 **************************************/

	// The update, if there was one, of the input page is complete.
	// The cache buffer can be 'unmarked'.  It is important to
	// unmark before CCH_unwind is (might be) called.

	SET_TDBB(tdbb);

	CCH_TRACE(("HANDOFF %d:%06d->%06d, %s",
		window->win_page.getPageSpaceID(), window->win_page.getPageNum(), page, (lock >= LCK_write) ? "EX" : "SH"));

	BufferDesc *bdb = window->win_bdb;

	// unmark
	if (bdb->bdb_writers == 1 && (bdb->bdb_flags & BDB_marked))
	{
		bdb->bdb_flags &= ~BDB_marked;
		bdb->unLockIO(tdbb);
	}

	// If the 'from-page' and 'to-page' of the handoff are the
	// same and the latch requested is shared then downgrade it.

	if ((window->win_page.getPageNum() == page) && (lock == LCK_read))
	{
		if (bdb->ourExclusiveLock())
			bdb->downgrade(SYNC_SHARED);

		return window->win_buffer;
	}

	WIN temp = *window;
	window->win_page = PageNumber(window->win_page.getPageSpaceID(), page);

	LockState must_read;
	if (bdb->bdb_bcb->bcb_flags & BCB_exclusive)
	{
		// This prevents a deadlock with the precedence queue, as shown by
		// mwrite mwrite1 2 mwrite2 2 test.fdb

		const int wait2 = bdb->ourExclusiveLock() ? LCK_NO_WAIT : wait;
		must_read = CCH_fetch_lock(tdbb, window, lock, wait2, page_type);
		if (must_read == lsLatchTimeout && wait2 == LCK_NO_WAIT)
		{
			bdb->downgrade(SYNC_SHARED);
			must_read = CCH_fetch_lock(tdbb, window, lock, wait, page_type);
		}
	}
	else
		must_read = CCH_fetch_lock(tdbb, window, lock, wait, page_type);

	// Latch or lock timeout, return failure.

	if (must_read == lsLatchTimeout || must_read == lsLockTimeout)
	{
		*window = temp;
		CCH_RELEASE(tdbb, window);
		return NULL;
	}

	if (release_tail)
		CCH_RELEASE_TAIL(tdbb, &temp);
	else
		CCH_RELEASE(tdbb, &temp);

	if (must_read != lsLockedHavePage)
		CCH_fetch_page(tdbb, window, true);

	bdb = window->win_bdb;

	if (lock != LCK_write && must_read != lsLockedHavePage)
	{
		if (bdb->ourExclusiveLock())
			bdb->downgrade(SYNC_SHARED);
	}

	adjust_scan_count(window, must_read == lsLocked);

	// Validate the fetched page matches the expected type

	if (bdb->bdb_buffer->pag_type != page_type && page_type != pag_undefined)
		page_validation_error(tdbb, window, page_type);

	return window->win_buffer;
}


void CCH_init(thread_db* tdbb, ULONG number)
{
/**************************************
 *
 *	C C H _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize the cache.  Allocate buffers control block,
 *	buffer descriptors, and actual buffers.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();
	const bool shared = (dbb->dbb_flags & DBB_shared);

	CCH_TRACE(("INIT    %s", dbb->dbb_filename.c_str()));

	// Check for database-specific page buffers

	if (dbb->dbb_page_buffers)
		number = dbb->dbb_page_buffers;

	// Enforce page buffer cache constraints

	if (number < MIN_PAGE_BUFFERS)
		number = MIN_PAGE_BUFFERS;

	if (number > MAX_PAGE_BUFFERS)
		number = MAX_PAGE_BUFFERS;

	const ULONG count = number;

	// Allocate and initialize buffers control block
	BufferControl* bcb = BufferControl::create(dbb);
	while (true)
	{
		try
		{
			bcb->bcb_hashTable = FB_NEW_POOL(*bcb->bcb_bufferpool)
				BCBHashTable(*bcb->bcb_bufferpool, number);
			break;
		}
		catch (const ScratchBird::Exception& ex)
		{
			ex.stuffException(tdbb->tdbb_status_vector);

			number -= number >> 2;

			if (number < MIN_PAGE_BUFFERS)
				ERR_post(Arg::Gds(isc_cache_too_small));
		}
	}

	dbb->dbb_bcb = bcb;
	bcb->bcb_page_size = dbb->dbb_page_size;
	bcb->bcb_database = dbb;
	bcb->bcb_flags = shared ? BCB_exclusive : 0;
	//bcb->bcb_flags = BCB_exclusive;	// TODO detect real state using LM

	QUE_INIT(bcb->bcb_in_use);
	QUE_INIT(bcb->bcb_dirty);
	bcb->bcb_dirty_count = 0;
	QUE_INIT(bcb->bcb_empty);

	// initialization of memory is system-specific

	bcb->bcb_count = memory_init(tdbb, bcb, number);
	bcb->bcb_free_minimum = (SSHORT) MIN(bcb->bcb_count / 4, 128);

	if (bcb->bcb_count < MIN_PAGE_BUFFERS)
		ERR_post(Arg::Gds(isc_cache_too_small));

	// Log if requested number of page buffers could not be allocated.

	if (count != bcb->bcb_count)
	{
		gds__log("Database: %s\n\tAllocated %ld page buffers of %ld requested",
			 tdbb->getAttachment()->att_filename.c_str(), bcb->bcb_count, count);
	}

	if (dbb->dbb_lock->lck_logical != LCK_EX)
		dbb->dbb_ast_flags |= DBB_assert_locks;
}


void CCH_init2(thread_db* tdbb)
{
	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = dbb->dbb_bcb;

	// Avoid running CCH_init2() in 2 parallel threads
	ScratchBird::MutexEnsureUnlock guard(bcb->bcb_threadStartup, FB_FUNCTION);
	guard.enter();

	if (!(bcb->bcb_flags & BCB_exclusive) || (bcb->bcb_flags & (BCB_cache_writer | BCB_writer_start)))
		return;

#ifdef CACHE_READER
	if (gds__thread_start(cache_reader, dbb, THREAD_high, 0, 0))
	{
		ERR_bugcheck_msg("cannot start thread");
	}

	{ // scope
		Database::Checkout dcoHolder(dbb, FB_FUNCTION);
		dbb->dbb_reader_init.enter();
	}
#endif

	const Attachment* att = tdbb->getAttachment();
	if (!(dbb->dbb_flags & DBB_read_only) && !(att->att_flags & ATT_security_db))
	{
		// writer startup in progress
		bcb->bcb_flags |= BCB_writer_start;
		guard.leave();

		try
		{
			bcb->bcb_writer_fini.run(bcb);
		}
		catch (const Exception&)
		{
			bcb->bcb_flags &= ~BCB_writer_start;
			ERR_bugcheck_msg("cannot start cache writer thread");
		}

		bcb->bcb_writer_init.enter();
	}
}


void CCH_mark(thread_db* tdbb, WIN* window, bool mark_system, bool must_write)
{
/**************************************
 *
 *	C C H _ m a r k
 *
 **************************************
 *
 * Functional description
 *	Mark a window as dirty.
 *
 **************************************/
	BufferDesc* bdb = window->win_bdb;
	BLKCHK(bdb, type_bdb);

	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	tdbb->bumpStats(RuntimeStatistics::PAGE_MARKS);

	BufferControl* bcb = dbb->dbb_bcb;

	if (!(bdb->bdb_flags & BDB_writer))
		BUGCHECK(208);			// msg 208 page not accessed for write

	CCH_TRACE(("MARK    %d:%06d", window->win_page.getPageSpaceID(), window->win_page.getPageNum()));

	// A LATCH_mark is needed before the BufferDesc can be marked.
	// This prevents a write while the page is being modified.

	if (!(bdb->bdb_flags & BDB_marked))
		bdb->lockIO(tdbb);

	fb_assert(bdb->ourIOLock());

	// Allocate difference page (if in stalled mode) before mark page as dirty.
	// It guarantees that disk space is allocated and page could be written later.

	if (!set_diff_page(tdbb, bdb))
	{
		clear_dirty_flag_and_nbak_state(tdbb, bdb);

		bdb->unLockIO(tdbb);
		CCH_unwind(tdbb, true);
	}

	fb_assert(dbb->dbb_backup_manager->getState() != Ods::hdr_nbak_unknown);

	bdb->bdb_incarnation = ++bcb->bcb_page_incarnation;

	// mark the dirty bit vector for this specific transaction,
	// if it exists; otherwise mark that the system transaction
	// has updated this page

	int newFlags = 0;
	TraNumber number;
	jrd_tra* transaction = tdbb->getTransaction();
	if (transaction && (number = transaction->tra_number))
	{
		if (!(tdbb->tdbb_flags & TDBB_sweeper))
		{
			const ULONG trans_bucket = number & (BITS_PER_LONG - 1);
			bdb->bdb_transactions |= (1L << trans_bucket);
			if (number > bdb->bdb_mark_transaction)
				bdb->bdb_mark_transaction = number;
		}
	}
	else
		newFlags |= BDB_system_dirty;

	if (mark_system)
		newFlags |= BDB_system_dirty;

	/// if (bcb->bcb_flags & BCB_exclusive)
	newFlags |= BDB_db_dirty;

	if (must_write || dbb->dbb_backup_manager->databaseFlushInProgress())
		newFlags |= BDB_must_write;

	bdb->bdb_flags |= newFlags;

	if (!(tdbb->tdbb_flags & TDBB_sweeper) || (bdb->bdb_flags & BDB_system_dirty))
		insertDirty(bcb, bdb);

	bdb->bdb_flags |= BDB_marked | BDB_dirty;
}


void CCH_must_write(thread_db* tdbb, WIN* window)
{
/**************************************
 *
 *	C C H _ m u s t _ w r i t e
 *
 **************************************
 *
 * Functional description
 *	Mark a window as "must write".
 *
 **************************************/
	SET_TDBB(tdbb);

	BufferDesc* bdb = window->win_bdb;
	BLKCHK(bdb, type_bdb);

	if (!(bdb->bdb_flags & BDB_marked) || !(bdb->bdb_flags & BDB_dirty)) {
		BUGCHECK(208);			// msg 208 page not accessed for write
	}

	bdb->bdb_flags |= BDB_must_write | BDB_dirty;
	fb_assert((bdb->bdb_flags & BDB_nbak_state_lock) ||
			  PageSpace::isTemporary(bdb->bdb_page.getPageSpaceID()));
}


void CCH_precedence(thread_db* tdbb, WIN* window, ULONG pageNum)
{
	const USHORT pageSpaceID = pageNum > FIRST_PIP_PAGE ?
		window->win_page.getPageSpaceID() : DB_PAGE_SPACE;

	CCH_precedence(tdbb, window, PageNumber(pageSpaceID, pageNum));
}


void CCH_tra_precedence(thread_db* tdbb, WIN* window, TraNumber traNum)
{
	/*
	if (traNum <= tdbb->getDatabase()->dbb_last_header_write)
	{
		return;
	}
	*/
	check_precedence(tdbb, window, PageNumber(TRANS_PAGE_SPACE, traNum));
}

void CCH_precedence(thread_db* tdbb, WIN* window, PageNumber page)
{
/**************************************
 *
 *	C C H _ p r e c e d e n c e
 *
 **************************************
 *
 * Functional description
 *	Given a window accessed for write and a page number,
 *	establish a precedence relationship such that the
 *	specified page will always be written before the page
 *	associated with the window.
 *
 *	If the "page number" is negative, it is really a transaction
 *	id.  In this case, the precedence relationship is to the
 *	database header page from which the transaction id was
 *	obtained.  If the header page has been written since the
 *	transaction id was assigned, no precedence relationship
 *	is required.
 *
 **************************************/
	// If the page is zero, the caller isn't really serious

	if (page.getPageNum() == 0)
		return;

	// no need to support precedence for temporary pages
	if (page.isTemporary() || window->win_page.isTemporary())
		return;

	check_precedence(tdbb, window, page);
}


#ifdef CACHE_READER
void CCH_prefetch(thread_db* tdbb, SLONG* pages, SSHORT count)
{
/**************************************
 *
 *	C C H _ p r e f e t c h
 *
 **************************************
 *
 * Functional description
 *	Given a vector of pages, set corresponding bits
 *	in global prefetch bitmap. Initiate an asynchronous
 *	I/O and get the cache reader reading in our behalf
 *	as well.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = dbb->dbb_bcb;

	if (!count || !(bcb->bcb_flags & BCB_cache_reader))
	{
		// Caller isn't really serious.
		return;
	}

	// Switch default pool to permanent pool for setting bits in prefetch bitmap.
	Jrd::ContextPoolHolder context(tdbb, bcb->bcb_bufferpool);

	// The global prefetch bitmap is the key to the I/O coalescense mechanism which dovetails
	// all thread prefetch requests to minimize sequential I/O requests.
	// It also enables multipage I/O by implicitly sorting page vector requests.

	SLONG first_page = 0;
	for (const SLONG* const end = pages + count; pages < end;)
	{
		const SLONG page = *pages++;
		if (page)
		{
			SBM_set(tdbb, &bcb->bcb_prefetch, page);
			if (!first_page)
				first_page = page;
		}
	}

	// Not likely that the caller's page vector was empty but check anyway.

	if (first_page)
	{
		prf prefetch;

		prefetch_init(&prefetch, tdbb);
		prefetch_prologue(&prefetch, &first_page);
		prefetch_io(&prefetch, tdbb->tdbb_status_vector);
		prefetch_epilogue(&prefetch, tdbb->tdbb_status_vector);
	}
}


bool CCH_prefetch_pages(thread_db* tdbb)
{
/**************************************
 *
 *	C C H _ p r e f e t c h _ p a g e s
 *
 **************************************
 *
 * Functional description
 *	Check the prefetch bitmap for a set
 *	of pages and read them into the cache.
 *
 **************************************/

	// Placeholder to be implemented when predictive prefetch is
	// enabled. This is called by VIO/garbage_collector() when it
	// is idle to help satisfy the prefetch demand.

	return false;
}
#endif // CACHE_READER


bool set_diff_page(thread_db* tdbb, BufferDesc* bdb)
{
	Database* const dbb = tdbb->getDatabase();
	BackupManager* const bm = dbb->dbb_backup_manager;

	// Temporary pages don't write to delta and need no SCN
	PageSpace* pageSpace = dbb->dbb_page_manager.findPageSpace(bdb->bdb_page.getPageSpaceID());
	fb_assert(pageSpace);
	if (pageSpace->isTemporary())
		return true;

	// Take backup state lock
	if (!(tdbb->tdbb_flags & TDBB_backup_write_locked))
	{
		const AtomicCounter::counter_type oldFlags = bdb->bdb_flags.exchangeBitOr(BDB_nbak_state_lock);
		if (!(oldFlags & BDB_nbak_state_lock))
		{
			NBAK_TRACE(("lock state for dirty page %d:%06d",
				bdb->bdb_page.getPageSpaceID(), bdb->bdb_page.getPageNum()));

			bm->lockStateRead(tdbb, LCK_WAIT);
		}
	}
	else
		fb_assert(bdb->bdb_page == HEADER_PAGE_NUMBER);

	if (bdb->bdb_page != HEADER_PAGE_NUMBER)
	{
		// SCN of header page is adjusted in nbak.cpp
		if (bdb->bdb_buffer->pag_scn != bm->getCurrentSCN())
		{
			bdb->bdb_buffer->pag_scn = bm->getCurrentSCN(); // Set SCN for the page

			// At PAG_set_page_scn() below we could dirty SCN page and thus acquire
			// nbackup state lock recursively. Since RWLock allows it, we are safe.
			// Else we should release just taken state lock before call of
			// PAG_set_page_scn() and acquire it again. If current SCN changes meanwhile
			// we should repeat whole process again...

			win window(bdb->bdb_page);
			window.win_bdb = bdb;
			window.win_buffer = bdb->bdb_buffer;
			PAG_set_page_scn(tdbb, &window);
		}

		fb_assert(bdb->bdb_buffer->pag_scn == bm->getCurrentSCN());
	}

	// Determine location of the page in difference file and write destination
	// so BufferDesc AST handlers and write_page routine can safely use this information

	const auto backupState = bm->getState();

	if (backupState == Ods::hdr_nbak_normal)
		return true;

	switch (backupState)
	{
	case Ods::hdr_nbak_stalled:
		bdb->bdb_difference_page = bm->getPageIndex(tdbb, bdb->bdb_page.getPageNum());
		if (!bdb->bdb_difference_page)
		{
			bdb->bdb_difference_page = bm->allocateDifferencePage(tdbb, bdb->bdb_page.getPageNum());
			if (!bdb->bdb_difference_page)
				return false;
			NBAK_TRACE(("Allocate difference page %d for database page %d",
				bdb->bdb_difference_page, bdb->bdb_page));
		}
		else
		{
			NBAK_TRACE(("Map existing difference page %d to database page %d",
				bdb->bdb_difference_page, bdb->bdb_page));
		}
		break;

	case Ods::hdr_nbak_merge:
		bdb->bdb_difference_page = bm->getPageIndex(tdbb, bdb->bdb_page.getPageNum());
		if (bdb->bdb_difference_page)
		{
			NBAK_TRACE(("Map existing difference page %d to database page %d (write_both)",
				bdb->bdb_difference_page, bdb->bdb_page));
		}
		break;
	}

	return true;
}


void CCH_release(thread_db* tdbb, WIN* window, const bool release_tail)
{
/**************************************
 *
 *	C C H _ r e l e a s e
 *
 **************************************
 *
 * Functional description
 *	Release a window. If the release_tail
 *	flag is true then make the buffer
 *	least-recently-used.
 *
 **************************************/
	SET_TDBB(tdbb);

	BufferDesc* const bdb = window->win_bdb;
	BLKCHK(bdb, type_bdb);

	BufferControl* bcb = bdb->bdb_bcb;

	CCH_TRACE(("RELEASE %d:%06d", window->win_page.getPageSpaceID(), window->win_page.getPageNum()));

	// A large sequential scan has requested that the garbage
	// collector garbage collect. Mark the buffer so that the
	// page isn't released to the LRU tail before the garbage
	// collector can process the page.

	if (window->win_flags & WIN_large_scan && window->win_flags & WIN_garbage_collect)
	{
		bdb->bdb_flags |= BDB_garbage_collect;
		window->win_flags &= ~WIN_garbage_collect;
	}

	const bool mustWrite = (bdb->bdb_flags & BDB_must_write) ||
		bcb->bcb_database->dbb_backup_manager->databaseFlushInProgress();

	if (bdb->bdb_writers == 1 || bdb->bdb_use_count == 1 ||
		(bdb->bdb_writers == 0 && mustWrite))
	{
		const bool marked = bdb->bdb_flags & BDB_marked;
		bdb->bdb_flags &= ~(BDB_writer | BDB_marked | BDB_faked);

		if (marked)
			bdb->unLockIO(tdbb);

		if (mustWrite)
		{
			// Downgrade exclusive latch to shared to allow concurrent share access
			// to page during I/O.

			bdb->downgrade(SYNC_SHARED);

			if (!write_buffer(tdbb, bdb, bdb->bdb_page, false, tdbb->tdbb_status_vector, true))
			{
				insertDirty(bcb, bdb);
				CCH_unwind(tdbb, true);
			}
		}
	}

	if (bdb->bdb_use_count == 1)
	{
		if (bdb->bdb_flags & BDB_no_blocking_ast)
		{
			if (bdb->bdb_flags & (BDB_db_dirty | BDB_dirty))
			{
				if (!write_buffer(tdbb, bdb, bdb->bdb_page, false, tdbb->tdbb_status_vector, true))
				{
					// Reassert blocking AST after write failure with dummy lock convert
					// to same level. This will re-enable blocking AST notification.

					if (!(bcb->bcb_flags & BCB_exclusive))
					{
						ThreadStatusGuard temp_status(tdbb);
						LCK_convert_opt(tdbb, bdb->bdb_lock, bdb->bdb_lock->lck_logical);
					}

					CCH_unwind(tdbb, true);
				}
			}

			PAGE_LOCK_RELEASE(tdbb, bcb, bdb->bdb_lock);
			bdb->bdb_flags &= ~BDB_no_blocking_ast;
			bdb->bdb_ast_flags &= ~BDB_blocking;
		}

		// Make buffer the least-recently-used by queueing it to the LRU tail

		if (release_tail)
		{
			if ((window->win_flags & WIN_large_scan && bdb->bdb_scan_count > 0 &&
					!(--bdb->bdb_scan_count) && !(bdb->bdb_flags & BDB_garbage_collect)) ||
				(window->win_flags & WIN_garbage_collector && bdb->bdb_flags & BDB_garbage_collect &&
					!bdb->bdb_scan_count))
			{
				if (window->win_flags & WIN_garbage_collector)
					bdb->bdb_flags &= ~BDB_garbage_collect;

				{ // bcb_syncLRU scope
					Sync lruSync(&bcb->bcb_syncLRU, "CCH_release");
					lruSync.lock(SYNC_EXCLUSIVE);

					if (bdb->bdb_flags & BDB_lru_chained)
					{
						requeueRecentlyUsed(bcb);
					}

					QUE_DELETE(bdb->bdb_in_use);
					QUE_APPEND(bcb->bcb_in_use, bdb->bdb_in_use);
				}

				if ((bcb->bcb_flags & BCB_cache_writer) &&
					(bdb->bdb_flags & (BDB_dirty | BDB_db_dirty)) )
				{
					insertDirty(bcb, bdb);

					bcb->bcb_flags |= BCB_free_pending;
					if (!(bcb->bcb_flags & BCB_writer_active))
					{
						bcb->bcb_writer_sem.release();
					}
				}
			}
		}
	}

	bdb->release(tdbb, true);
	window->win_bdb = NULL;
}


void CCH_release_exclusive(thread_db* tdbb)
{
/**************************************
 *
 *	C C H _ r e l e a s e _ e x c l u s i v e
 *
 **************************************
 *
 * Functional description
 *	Release exclusive access to database.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	dbb->dbb_flags &= ~DBB_exclusive;

	Jrd::Attachment* attachment = tdbb->getAttachment();
	if (attachment)
		attachment->att_flags &= ~ATT_exclusive;

	if (dbb->dbb_ast_flags & DBB_blocking)
		LCK_re_post(tdbb, dbb->dbb_lock);
}


bool CCH_rollover_to_shadow(thread_db* tdbb, Database* dbb, jrd_file* file, const bool inAst)
{
/**************************************
 *
 *	C C H _ r o l l o v e r _ t o _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *	An I/O error has been detected on the
 *	main database file.  Roll over to use
 *	the shadow file.
 *
 **************************************/

	SET_TDBB(tdbb);

	// Is the shadow subsystem yet initialized
	if (!dbb->dbb_shadow_lock)
		return false;

	// hvlad: if there are no shadows can't rollover
	// this is a temporary solution to prevent 100% CPU load
	// in write_page in case of PIO_write failure
	if (!dbb->dbb_shadow)
		return false;

	// notify other process immediately to ensure all read from sdw
	// file instead of db file
	return SDW_rollover_to_shadow(tdbb, file, inAst);
}


void CCH_shutdown(thread_db* tdbb)
{
/**************************************
 *
 *	C C H _ s h u t d o w n
 *
 **************************************
 *
 * Functional description
 *	Shutdown database physical page locks.
 *
 **************************************/
	Database* const dbb = tdbb->getDatabase();
	BufferControl* const bcb = dbb->dbb_bcb;

	if (!bcb)
		return;

#ifdef CACHE_READER
	// Shutdown the dedicated cache reader for this database

	if (bcb->bcb_flags & BCB_cache_reader)
	{
		bcb->bcb_flags &= ~BCB_cache_reader;
		dbb->dbb_reader_sem.release();
		dbb->dbb_reader_fini.enter();
	}
#endif

	// Wait for cache writer startup to complete

	while (bcb->bcb_flags & BCB_writer_start)
		Thread::yield();

	// Shutdown the dedicated cache writer for this database

	if (bcb->bcb_flags & BCB_cache_writer)
	{
		bcb->bcb_flags &= ~BCB_cache_writer;
		bcb->bcb_writer_sem.release(); // Wake up running thread
		bcb->bcb_writer_fini.waitForCompletion();
	}

	SyncLockGuard bcbSync(&bcb->bcb_syncObject, SYNC_EXCLUSIVE, FB_FUNCTION);

	// Flush and release page buffers

	if (bcb->bcb_count)
	{
		try
		{
			if (dbb->dbb_flags & DBB_bugcheck)
				LongJump::raise();

			CCH_flush(tdbb, FLUSH_FINI, 0);
		}
		catch (const Exception&)
		{
			for (auto blk : bcb->bcb_bdbBlocks)
			{
				BufferDesc* bdb = blk.m_bdbs;
				const BufferDesc* const end = blk.m_bdbs + blk.m_count;
				for (; bdb < end; bdb++)
				{

					if (dbb->dbb_flags & DBB_bugcheck)
					{
						bdb->bdb_flags &= ~BDB_db_dirty;
						clear_dirty_flag_and_nbak_state(tdbb, bdb);
					}

					PAGE_LOCK_RELEASE(tdbb, bcb, bdb->bdb_lock);
				}
			}
		}
	}

	// close the database file and all associated shadow files

	dbb->dbb_page_manager.closeAll();
	SDW_close();
}

void CCH_unwind(thread_db* tdbb, const bool punt)
{
/**************************************
 *
 *	C C H _ u n w i n d
 *
 **************************************
 *
 * Functional description
 *	Synchronously unwind cache after I/O or lock error.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();

	// CCH_unwind is called when any of the following occurs:
	// - IO error
	// - journaling error => obsolete
	// - bad page checksum => obsolete
	// - wrong page type
	// - page locking (not latching) deadlock

	BufferControl* bcb = dbb->dbb_bcb;
	if (!bcb || (tdbb->tdbb_flags & TDBB_no_cache_unwind))
	{
		if (punt)
			ERR_punt();

		return;
	}

	// A cache error has occurred. Scan the cache for buffers
	// which may be in use and release them.

	for (FB_SIZE_T n = 0; n < tdbb->tdbb_bdbs.getCount(); ++n)
	{
		BufferDesc *bdb = tdbb->tdbb_bdbs[n];
		if (bdb)
		{
			if (bdb->bdb_flags & BDB_marked)
				BUGCHECK(268);	// msg 268 buffer marked during cache unwind

			if (bdb->ourIOLock())
			{
				bdb->unLockIO(tdbb);
			}
			else
			{
				if (bdb->ourExclusiveLock())
					bdb->bdb_flags &= ~(BDB_writer | BDB_faked | BDB_must_write);

				bdb->release(tdbb, true);
			}
		}
	}

	/***
	bcb_repeat* tail = bcb->bcb_rpt;
	for (const bcb_repeat* const end = tail + bcb->bcb_count; tail < end; tail++)
	{
		BufferDesc* bdb = tail->bcb_bdb;
		if (!bdb->bdb_use_count) {
			continue;
		}
		if (bdb->bdb_io == tdbb) {
			release_bdb(tdbb, bdb, true, false, false);
		}
		if (bdb->bdb_exclusive == tdbb)
		{
			if (bdb->bdb_flags & BDB_marked) {
				BUGCHECK(268);	// msg 268 buffer marked during cache unwind
			}

			bdb->bdb_flags &= ~(BDB_writer | BDB_faked | BDB_must_write);
			release_bdb(tdbb, bdb, true, false, false);
		}

		// hvlad : as far as I understand thread can't hold more than two shared latches
		// on the same bdb, so findSharedLatch below will not be called many times

		SharedLatch* latch = findSharedLatch(tdbb, bdb);
		while (latch)
		{
			release_bdb(tdbb, bdb, true, false, false);
			latch = findSharedLatch(tdbb, bdb);
		}

#ifndef SUPERSERVER
		const pag* const page = bdb->bdb_buffer;
		if (page->pag_type == pag_header || page->pag_type == pag_transactions)
		{
			++bdb->bdb_use_count;
			clear_dirty_flag_and_nbak_state(tdbb, bdb);
			bdb->bdb_flags &= ~(BDB_writer | BDB_marked | BDB_faked | BDB_db_dirty);
			PAGE_LOCK_RELEASE(tdbb, bcb, bdb->bdb_lock);
			--bdb->bdb_use_count;
		}
#endif
	}
	***/

	tdbb->tdbb_flags |= TDBB_cache_unwound;

	if (punt)
		ERR_punt();
}


bool CCH_validate(WIN* window)
{
/**************************************
 *
 *	C C H _ v a l i d a t e
 *
 **************************************
 *
 * Functional description
 *	Give a page a quick once over looking for unhealthyness.
 *
 **************************************/
	BufferDesc* bdb = window->win_bdb;

	// If page is marked for write, checksum is questionable

	if ((bdb->bdb_flags & (BDB_dirty | BDB_db_dirty)))
		return true;

	return (bdb->bdb_buffer->pag_pageno == bdb->bdb_page.getPageNum());
}


bool CCH_write_all_shadows(thread_db* tdbb, Shadow* shadow, BufferDesc* bdb, Ods::pag* page,
	FbStatusVector* status, const bool inAst)
{
/**************************************
 *
 *	C C H _ w r i t e _ a l l _ s h a d o w s
 *
 **************************************
 *
 * Functional description
 *	Compute a checksum and write a page out to all shadows
 *	detecting failure on write.
 *	If shadow is null, write to all shadows, otherwise only to specified
 *	shadow.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();

	Shadow* sdw = shadow ? shadow : dbb->dbb_shadow;

	if (!sdw)
		return true;

	bool result = true;
	ScratchBird::UCharBuffer spare_buffer;

	if (bdb->bdb_page == HEADER_PAGE_NUMBER)
	{
		Ods::pag* newPage = (pag*) spare_buffer.getBuffer(dbb->dbb_page_size);
		memcpy(newPage, page, HDR_SIZE);
		page = newPage;
		memset((UCHAR*) page + HDR_SIZE, 0, dbb->dbb_page_size - HDR_SIZE);
	}
	page->pag_pageno = bdb->bdb_page.getPageNum();

	for (; sdw; sdw = sdw->sdw_next)
	{
		// don't bother to write to the shadow if it is no longer viable

		/* Fix for bug 7925. drop_gdb fails to remove secondary file if
		   the shadow is conditional. Reason being the header page not
		   being correctly initialized.

		   The following block was not being performed for a conditional
		   shadow since SDW_INVALID expanded to include conditional shadow

		   -Sudesh  07/10/95
		   old code --> if (sdw->sdw_flags & SDW_INVALID)
		 */

		if ((sdw->sdw_flags & SDW_INVALID) && !(sdw->sdw_flags & SDW_conditional))
			continue;

		if (bdb->bdb_page == HEADER_PAGE_NUMBER)
		{
			// fixup header for shadow file
			jrd_file* shadow_file = sdw->sdw_file;
			header_page* header = (header_page*) page;

			PageSpace* pageSpaceID = dbb->dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
			const UCHAR* q = (UCHAR *) pageSpaceID->file->fil_string;
			header->hdr_data[0] = HDR_end;
			header->hdr_end = HDR_SIZE;

			PAG_add_header_entry(tdbb, header, HDR_root_file_name,
								 (USHORT) strlen((const char*) q), q);

			header->hdr_flags |= hdr_active_shadow;
			header->hdr_header.pag_pageno = bdb->bdb_page.getPageNum();
		}

		// This condition makes sure that PIO_write is performed in case of
		// conditional shadow only if the page is Header page
		//
		// -Sudesh 07/10/95

		if ((sdw->sdw_flags & SDW_conditional) && bdb->bdb_page != HEADER_PAGE_NUMBER)
			continue;

		// if a write failure happens on an AUTO shadow, mark the
		// shadow to be deleted at the next available opportunity when we
		// know we don't have a page fetched

		if (!PIO_write(tdbb, sdw->sdw_file, bdb, page, status))
		{
			if (sdw->sdw_flags & SDW_manual)
				result = false;
			else
			{
				sdw->sdw_flags |= SDW_delete;
				if (!inAst && SDW_check_conditional(tdbb))
				{
					if (SDW_lck_update(tdbb, 0))
					{
						SDW_notify(tdbb);
						CCH_unwind(tdbb, false);
						SDW_dump_pages(tdbb);
						ERR_post(Arg::Gds(isc_deadlock));
					}
				}
			}
		}
		// If shadow was specified, break out of loop

		if (shadow) {
			break;
		}
	}

	return result;
}


static void adjust_scan_count(WIN* window, bool mustRead)
{
/**************************************
 *
 *	a d j u s t _ s c a n _ c o u n t
 *
 **************************************/
	BufferDesc* bdb = window->win_bdb;

	// If a page was read or prefetched on behalf of a large scan
	// then load the window scan count into the buffer descriptor.
	// This buffer scan count is decremented by releasing a buffer
	// with CCH_RELEASE_TAIL.

	// Otherwise zero the buffer scan count to prevent the buffer
	// from being queued to the LRU tail.

	if (window->win_flags & WIN_large_scan)
	{
		if (mustRead || (bdb->bdb_flags & BDB_prefetch) || bdb->bdb_scan_count < 0)
			bdb->bdb_scan_count = window->win_scans;
	}
	else if (window->win_flags & WIN_garbage_collector)
	{
		if (mustRead)
			bdb->bdb_scan_count = -1;

		if (bdb->bdb_flags & BDB_garbage_collect)
			window->win_flags |= WIN_garbage_collect;
	}
	else if (window->win_flags & WIN_secondary)
	{
		if (mustRead)
			bdb->bdb_scan_count = -1;
	}
	else
	{
		bdb->bdb_scan_count = 0;
		if (bdb->bdb_flags & BDB_garbage_collect)
			bdb->bdb_flags &= ~BDB_garbage_collect;
	}
}


static int blocking_ast_bdb(void* ast_object)
{
/**************************************
 *
 *	b l o c k i n g _ a s t _ b d b
 *
 **************************************
 *
 * Functional description
 *	Blocking AST for buffer control blocks.  This is called at
 *	AST (signal) level to indicate that a lock is blocking another
 *	process.  If the BufferDesc* is in use, set a flag indicating that the
 *	lock is blocking and return.  When the BufferDesc is released at normal
 *	level the lock will be down graded.  If the BufferDesc* is not in use,
 *	write the page if dirty then downgrade lock.  Things would be
 *	much hairier if UNIX supported asynchronous IO, but it doesn't.
 *	WHEW!
 *
 **************************************/
	// CVC: I assume we need the function call but not the variable.
	/*ThreadSync* const thread = */ThreadSync::getThread("blocking_ast_bdb");

	BufferDesc* const bdb = static_cast<BufferDesc*>(ast_object);

	try
	{
		BufferControl* const bcb = bdb->bdb_bcb;
		fb_assert(!(bcb->bcb_flags & BCB_exclusive));

		Database* const dbb = bcb->bcb_database;
		fb_assert(dbb);

		AsyncContextHolder tdbb(dbb, FB_FUNCTION);

		// Do some fancy footwork to make sure that pages are
		// not removed from the btc tree at AST level. Then
		// restore the flag to whatever it was before.

		const bool keep_pages = (bcb->bcb_flags & BCB_keep_pages) != 0;
		bcb->bcb_flags |= BCB_keep_pages;

		down_grade(tdbb, bdb);

		if (!keep_pages)
			bcb->bcb_flags &= ~BCB_keep_pages;

		if (tdbb->tdbb_status_vector->getState() & IStatus::STATE_ERRORS)
			iscDbLogStatus(dbb->dbb_filename.c_str(), tdbb->tdbb_status_vector);
	}
	catch (const ScratchBird::Exception&)
	{
		return -1;
	} // no-op

    return 0;
}


// Remove cleared precedence blocks from high precedence queue
static void purgePrecedence(BufferControl* bcb, BufferDesc* bdb)
{
	Sync precSync(&bcb->bcb_syncPrecedence, "purgePrecedence");
	precSync.lock(SYNC_EXCLUSIVE);

	QUE que_prec = bdb->bdb_higher.que_forward, next_prec;
	for (; que_prec != &bdb->bdb_higher; que_prec = next_prec)
	{
		next_prec = que_prec->que_forward;

		Precedence* precedence = BLOCK(que_prec, Precedence, pre_higher);
		if (precedence->pre_flags & PRE_cleared)
		{
			QUE_DELETE(precedence->pre_higher);
			QUE_DELETE(precedence->pre_lower);
			precedence->pre_hi = (BufferDesc*) bcb->bcb_free;
			bcb->bcb_free = precedence;
		}
	}
}

// Collect pages modified by given or system transaction and write it to disk.
// See also comments in flushPages.
static void flushDirty(thread_db* tdbb, SLONG transaction_mask, const bool sys_only)
{
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = dbb->dbb_bcb;
	ScratchBird::HalfStaticArray<BufferDesc*, 1024> flush;

	{  // dirtySync scope
		Sync dirtySync(&bcb->bcb_syncDirtyBdbs, "flushDirty");
		dirtySync.lock(SYNC_EXCLUSIVE);

		QUE que_inst = bcb->bcb_dirty.que_forward, next;
		for (; que_inst != &bcb->bcb_dirty; que_inst = next)
		{
			next = que_inst->que_forward;
			BufferDesc* bdb = BLOCK(que_inst, BufferDesc, bdb_dirty);

			if (!(bdb->bdb_flags & BDB_dirty))
			{
				removeDirty(bcb, bdb);
				continue;
			}

			if ((transaction_mask & bdb->bdb_transactions) ||
				(bdb->bdb_flags & BDB_system_dirty) ||
				(!transaction_mask && !sys_only) ||
				(!bdb->bdb_transactions))
			{
				flush.add(bdb);
			}
		}
	}

	flushPages(tdbb, FLUSH_TRAN, flush.begin(), flush.getCount());
}


// Collect pages modified by garbage collector or all dirty pages or release page
// locks - depending of flush_flag, and write it to disk.
// See also comments in flushPages.
static void flushAll(thread_db* tdbb, USHORT flush_flag)
{
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = dbb->dbb_bcb;
	ScratchBird::HalfStaticArray<BufferDesc*, 1024> flush(bcb->bcb_dirty_count);

	const bool all_flag = (flush_flag & FLUSH_ALL) != 0;
	const bool sweep_flag = (flush_flag & FLUSH_SWEEP) != 0;
	const bool release_flag = (flush_flag & FLUSH_RLSE) != 0;
	const bool write_thru = release_flag;

	{
		Sync bcbSync(&bcb->bcb_syncObject, FB_FUNCTION);
		if (!bcb->bcb_syncObject.ourExclusiveLock())
			bcbSync.lock(SYNC_SHARED);

		for (auto blk : bcb->bcb_bdbBlocks)
		{
			for (ULONG i = 0; i < blk.m_count; i++)
			{
				BufferDesc* bdb = &blk.m_bdbs[i];

				if (bdb->bdb_flags & (BDB_db_dirty | BDB_dirty))
				{
					if (bdb->bdb_flags & BDB_dirty)
						flush.add(bdb);
					else if (bdb->bdb_flags & BDB_db_dirty)
					{
						// pages modified by sweep\garbage collector are not in dirty list
						const bool dirty_list = (bdb->bdb_dirty.que_forward != &bdb->bdb_dirty);

						if (all_flag || (sweep_flag && !dirty_list))
							flush.add(bdb);
					}
				}
				else if (release_flag)
				{
					bdb->addRef(tdbb, SYNC_EXCLUSIVE);

					if (bdb->bdb_use_count > 1)
						BUGCHECK(210);	// msg 210 page in use during flush

					PAGE_LOCK_RELEASE(tdbb, bcb, bdb->bdb_lock);
					bdb->release(tdbb, false);
				}
			}
		}
	}

	flushPages(tdbb, flush_flag, flush.begin(), flush.getCount());
}


// Used in qsort below
extern "C" {
	static int cmpBdbs(const void* a, const void* b)
	{
		const BufferDesc* bdbA = *(BufferDesc**) a;
		const BufferDesc* bdbB = *(BufferDesc**) b;

		if (bdbA->bdb_page > bdbB->bdb_page)
			return 1;

		if (bdbA->bdb_page < bdbB->bdb_page)
			return -1;

		return 0;
	}
} // extern C


// Write array of pages to disk in efficient order.
// First, sort pages by their numbers to make writes physically ordered and
// thus faster. At every iteration of while loop write pages which have no high
// precedence pages to ensure order preserved. If after some iteration there are
// no such pages (i.e. all of not written yet pages have high precedence pages)
// then write them all at last iteration (of course write_buffer will also check
// for precedence before write).
static void flushPages(thread_db* tdbb, USHORT flush_flag, BufferDesc** begin, FB_SIZE_T count)
{
	FbStatusVector* const status = tdbb->tdbb_status_vector;
	const bool all_flag = (flush_flag & FLUSH_ALL) != 0;
	const bool release_flag = (flush_flag & FLUSH_RLSE) != 0;
	const bool write_thru = release_flag;

	qsort(begin, count, sizeof(BufferDesc*), cmpBdbs);

	MarkIterator<BufferDesc*> iter(begin, count);

	FB_SIZE_T written = 0;
	bool writeAll = false;

	while (!iter.isEmpty())
	{
		bool found = false;
		for (; !iter.isEof(); ++iter)
		{
			BufferDesc* bdb = *iter;
			fb_assert(bdb);
			if (!bdb)
				continue;

			bdb->addRef(tdbb, release_flag ? SYNC_EXCLUSIVE : SYNC_SHARED);

			BufferControl* bcb = bdb->bdb_bcb;
			if (!writeAll)
				purgePrecedence(bcb, bdb);

			if (writeAll || QUE_EMPTY(bdb->bdb_higher))
			{
				if (release_flag)
				{
					if (bdb->bdb_use_count > 1)
						BUGCHECK(210);	// msg 210 page in use during flush
				}

				if (!all_flag || bdb->bdb_flags & (BDB_db_dirty | BDB_dirty))
				{
					if (!write_buffer(tdbb, bdb, bdb->bdb_page, write_thru, status, true))
						CCH_unwind(tdbb, true);
				}

				// release lock before losing control over bdb, it prevents
				// concurrent operations on released lock
				if (release_flag)
					PAGE_LOCK_RELEASE(tdbb, bcb, bdb->bdb_lock);

				bdb->release(tdbb, !release_flag && !(bdb->bdb_flags & BDB_dirty));

				iter.mark();
				found = true;
				written++;
			}
			else
			{
				bdb->release(tdbb, false);
			}
		}

		if (!found)
			writeAll = true;

		iter.rewind();
	}

	fb_assert(count == written);
}


#ifdef CACHE_READER
void BufferControl::cache_reader(BufferControl* bcb)
{
/**************************************
 *
 *	c a c h e _ r e a d e r
 *
 **************************************
 *
 * Functional description
 *	Prefetch pages into cache for sequential scans.
 *	Use asynchronous I/O to keep two prefetch requests
 *	busy at a time.
 *
 **************************************/
	Database* dbb = bcb->bcb_database;
	Database::SyncGuard dsGuard(dbb);

	FbLocalStatus status_vector;

	// Establish a thread context.
	ThreadContextHolder tdbb(status_vector);

	// Dummy attachment needed for lock owner identification.
	tdbb->setDatabase(dbb);
	Jrd::Attachment* const attachment = Attachment::create(dbb, nullptr);
	tdbb->setAttachment(attachment);
	attachment->att_filename = dbb->dbb_filename;
	Jrd::ContextPoolHolder context(tdbb, bcb->bcb_bufferpool);

	// This try block is specifically to protect the LCK_init call: if
	// LCK_init fails we won't be able to accomplish anything anyway, so
	// return, unlike the other try blocks further down the page.

	BufferControl* bcb = NULL;

	try {

		LCK_init(tdbb, LCK_OWNER_attachment);
		TRA_init(attachment);
		bcb = dbb->dbb_bcb;
		bcb->bcb_flags |= BCB_cache_reader;
		dbb->dbb_reader_init.post();	// Notify our creator that we have started
	}
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(status_vector);
		iscDbLogStatus(dbb->dbb_filename.c_str(), status_vector);
		return 0;
	}

	try {

	// Set up dual prefetch control blocks to keep multiple prefetch
	// requests active at a time. Also helps to prevent the cache reader
	// from becoming dedicated or locked into a single request's prefetch demands.

	prf prefetch1, prefetch2;
	prefetch_init(&prefetch1, tdbb);
	prefetch_init(&prefetch2, tdbb);

	while (bcb->bcb_flags & BCB_cache_reader)
	{
		bcb->bcb_flags |= BCB_reader_active;
		bool found = false;
		SLONG starting_page = -1;
		prf* next_prefetch = &prefetch1;

		if (dbb->dbb_flags & DBB_suspend_bgio)
		{
			Database::Checkout dcoHolder(dbb, FB_FUNCTION);
			dbb->dbb_reader_sem.tryEnter(10);
			continue;
		}

		// Make a pass thru the global prefetch bitmap looking for something
		// to read. When the entire bitmap has been scanned and found to be
		// empty then wait for new requests.

		do {
			if (!(prefetch1.prf_flags & PRF_active) &&
				SBM_next(bcb->bcb_prefetch, &starting_page, RSE_get_forward))
			{
				found = true;
				prefetch_prologue(&prefetch1, &starting_page);
				prefetch_io(&prefetch1, status_vector);
			}

			if (!(prefetch2.prf_flags & PRF_active) &&
				SBM_next(bcb->bcb_prefetch, &starting_page, RSE_get_forward))
			{
				found = true;
				prefetch_prologue(&prefetch2, &starting_page);
				prefetch_io(&prefetch2, status_vector);
			}

			prf* post_prefetch = next_prefetch;
			next_prefetch = (post_prefetch == &prefetch1) ? &prefetch2 : &prefetch1;

			if (post_prefetch->prf_flags & PRF_active)
				prefetch_epilogue(post_prefetch, status_vector);

			if (found)
			{
				// If the cache writer or garbage collector is idle, put
				// them to work prefetching pages.
#ifdef CACHE_WRITER
				if ((bcb->bcb_flags & BCB_cache_writer) && !(bcb->bcb_flags & BCB_writer_active))
					dbb_writer_sem.release();
#endif
#ifdef GARBAGE_THREAD
				if ((dbb->dbb_flags & DBB_garbage_collector) && !(dbb->dbb_flags & DBB_gc_active))
					dbb->dbb_gc_sem.release();
#endif
			}
		} while (prefetch1.prf_flags & PRF_active || prefetch2.prf_flags & PRF_active);

		// If there's more work to do voluntarily ask to be rescheduled.
		// Otherwise, wait for event notification.
		BufferDesc* bdb;
		if (found)
			JRD_reschedule(tdbb, true);
		else if (bcb->bcb_flags & BCB_free_pending && (bdb = get_dirty_buffer(tdbb)))
		{
			// In our spare time, help writer clean the cache.

			write_buffer(tdbb, bdb, bdb->bdb_page, true, status_vector, true);
		}
		else
		{
			bcb->bcb_flags &= ~BCB_reader_active;
			Database::Checkout dcoHolder(dbb, FB_FUNCTION);
			dbb->dbb_reader_sem.tryEnter(10);
		}
		bcb = dbb->dbb_bcb;
	}

	LCK_fini(tdbb, LCK_OWNER_attachment);
	Jrd::Attachment::destroy(attachment);	// no need saving warning error strings here
	tdbb->setAttachment(NULL);
	bcb->bcb_flags &= ~BCB_cache_reader;
	dbb->dbb_reader_fini.post();

	}	// try
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(status_vector);
		iscDbLogStatus(dbb->dbb_filename.c_str(), status_vector);
	}
	return 0;
}
#endif


void BufferControl::cache_writer(BufferControl* bcb)
{
/**************************************
 *
 *	c a c h e _ w r i t e r
 *
 **************************************
 *
 * Functional description
 *	Write dirty pages to database to maintain an adequate supply of free pages.
 *
 **************************************/
	FbLocalStatus status_vector;
	Database* const dbb = bcb->bcb_database;

	try
	{
		UserId user;
		user.setUserName("Cache Writer");

		Jrd::Attachment* const attachment = Jrd::Attachment::create(dbb, nullptr);
		RefPtr<SysStableAttachment> sAtt(FB_NEW SysStableAttachment(attachment));
		attachment->setStable(sAtt);
		attachment->att_filename = dbb->dbb_filename;
		attachment->att_user = &user;

		BackgroundContextHolder tdbb(dbb, attachment, &status_vector, FB_FUNCTION);
		Jrd::Attachment::UseCountHolder use(attachment);

		try
		{
			LCK_init(tdbb, LCK_OWNER_attachment);
			PAG_header(tdbb, true);
			PAG_attachment_id(tdbb);
			TRA_init(attachment);

			Monitoring::publishAttachment(tdbb);

			sAtt->initDone();

			bcb->bcb_flags |= BCB_cache_writer;
			bcb->bcb_flags &= ~BCB_writer_start;

			// Notify our creator that we have started
			bcb->bcb_writer_init.release();

			while (bcb->bcb_flags & BCB_cache_writer)
			{
				bcb->bcb_flags |= BCB_writer_active;
#ifdef CACHE_READER
				SLONG starting_page = -1;
#endif

				if (dbb->dbb_flags & DBB_suspend_bgio)
				{
					EngineCheckout cout(tdbb, FB_FUNCTION);
					bcb->bcb_writer_sem.tryEnter(10);
					continue;
				}

#ifdef SUPERSERVER_V2
				// Flush buffers for lazy commit
				SLONG commit_mask;
				if (!(dbb->dbb_flags & DBB_force_write) && (commit_mask = dbb->dbb_flush_cycle))
				{
					dbb->dbb_flush_cycle = 0;
					btc_flush(tdbb, commit_mask, false, status_vector);
				}
#endif

				if (bcb->bcb_flags & BCB_free_pending)
				{
					BufferDesc* const bdb = get_dirty_buffer(tdbb);
					if (bdb)
					{
						write_buffer(tdbb, bdb, bdb->bdb_page, true, &status_vector, true);
						attachment->mergeStats();
					}
				}

				// If there's more work to do voluntarily ask to be rescheduled.
				// Otherwise, wait for event notification.

				if ((bcb->bcb_flags & BCB_free_pending) || dbb->dbb_flush_cycle)
					JRD_reschedule(tdbb, true);
#ifdef CACHE_READER
				else if (SBM_next(bcb->bcb_prefetch, &starting_page, RSE_get_forward))
				{
					// Prefetch some pages in our spare time and in the process
					// garbage collect the prefetch bitmap.
					prf prefetch;

					prefetch_init(&prefetch, tdbb);
					prefetch_prologue(&prefetch, &starting_page);
					prefetch_io(&prefetch, status_vector);
					prefetch_epilogue(&prefetch, status_vector);
				}
#endif
				else
				{
					bcb->bcb_flags &= ~BCB_writer_active;
					EngineCheckout cout(tdbb, FB_FUNCTION);
					bcb->bcb_writer_sem.tryEnter(10);
				}
			}
		}
		catch (const ScratchBird::Exception& ex)
		{
			ex.stuffException(&status_vector);
			iscDbLogStatus(dbb->dbb_filename.c_str(), &status_vector);
			// continue execution to clean up
		}

		Monitoring::cleanupAttachment(tdbb);
		attachment->releaseLocks(tdbb);
		LCK_fini(tdbb, LCK_OWNER_attachment);

		attachment->releaseRelations(tdbb);
	}	// try
	catch (const ScratchBird::Exception& ex)
	{
		bcb->exceptionHandler(ex, cache_writer);
	}

	bcb->bcb_flags &= ~BCB_cache_writer;

	try
	{
		if (bcb->bcb_flags & BCB_writer_start)
		{
			bcb->bcb_flags &= ~BCB_writer_start;
			bcb->bcb_writer_init.release();
		}
	}
	catch (const ScratchBird::Exception& ex)
	{
		bcb->exceptionHandler(ex, cache_writer);
	}
}


void BufferControl::exceptionHandler(const ScratchBird::Exception& ex, BcbThreadSync::ThreadRoutine*)
{
	FbLocalStatus status_vector;
	ex.stuffException(&status_vector);
	iscDbLogStatus(bcb_database->dbb_filename.c_str(), &status_vector);
}


static void cacheBuffer(Attachment* att, BufferDesc* bdb)
{
	if (att)
	{
		if (!att->att_bdb_cache)
			att->att_bdb_cache = FB_NEW_POOL(*att->att_pool) PageToBufferMap(*att->att_pool);

		att->att_bdb_cache->put(bdb);
	}
}


static void check_precedence(thread_db* tdbb, WIN* window, PageNumber page)
{
/**************************************
 *
 *	c h e c k _ p r e c e d e n c e
 *
 **************************************
 *
 * Functional description
 *	Given a window accessed for write and a page number,
 *	establish a precedence relationship such that the
 *	specified page will always be written before the page
 *	associated with the window.
 *
 *	If the "page number" is negative, it is really a transaction
 *	id.  In this case, the precedence relationship is to the
 *	database header page from which the transaction id was
 *	obtained.  If the header page has been written since the
 *	transaction id was assigned, no precedence relationship
 *	is required.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = dbb->dbb_bcb;

	// If this is really a transaction id, sort things out

	switch(page.getPageSpaceID())
	{
		case DB_PAGE_SPACE:
			break;

		case TRANS_PAGE_SPACE:
			if (page.getPageNum() <= tdbb->getDatabase()->dbb_last_header_write)
				return;
			page = PageNumber(DB_PAGE_SPACE, 0);
			break;

		default:
			fb_assert(false);
			return;
	}

	// In the past negative value, passed here, meant not page, but transaction number.
	// When we finally move to 32 (not 31) bit page numbers, this should be removed,
	// but currently I add:
	fb_assert(!(page.getPageNum() & 0x80000000));
	// to help detect cases, when something possibly negative is passed.
	// AP - 2011.

	// Start by finding the buffer containing the high priority page

#ifndef HASH_USE_CDS_LIST
	Sync bcbSync(&bcb->bcb_syncObject, FB_FUNCTION);
	bcbSync.lock(SYNC_SHARED);
#endif

	BufferDesc* high = bcb->bcb_hashTable->find(page);
#ifndef HASH_USE_CDS_LIST
	bcbSync.unlock();
#endif

	if (!high)
		return;

	// Found the higher precedence buffer.  If it's not dirty, don't sweat it.
	// If it's the same page, ditto.

	if (!(high->bdb_flags & BDB_dirty) || (high->bdb_page == window->win_page))
		return;

	BufferDesc* low = window->win_bdb;

	if ((low->bdb_flags & BDB_marked) && !(low->bdb_flags & BDB_faked))
		BUGCHECK(212);	// msg 212 CCH_precedence: block marked

	// If already related, there's nothing more to do. If the precedence
	// search was too complex to complete, just write the high page and
	// forget about establishing the relationship.

	Sync precSync(&bcb->bcb_syncPrecedence, "check_precedence");
	precSync.lock(SYNC_EXCLUSIVE);

	if (QUE_NOT_EMPTY(high->bdb_lower))
	{
		const ULONG mark = get_prec_walk_mark(bcb);
		const SSHORT relationship = related(low, high, PRE_SEARCH_LIMIT, mark);
		if (relationship == PRE_EXISTS)
			return;

		if (relationship == PRE_UNKNOWN)
		{
			precSync.unlock();
			const PageNumber high_page = high->bdb_page;
			if (!write_buffer(tdbb, high, high_page, false, tdbb->tdbb_status_vector, true))
				CCH_unwind(tdbb, true);

			return;
		}
	}

	// Check to see if we're going to create a cycle or the precedence search
	// was too complex to complete.  If so, force a write of the "after"
	// (currently fetched) page.  Assuming everyone obeys the rules and calls
	// precedence before marking the buffer, everything should be ok

	while (QUE_NOT_EMPTY(low->bdb_lower))
	{
		const ULONG mark = get_prec_walk_mark(bcb);
		const SSHORT relationship = related(high, low, PRE_SEARCH_LIMIT, mark);
		if (relationship == PRE_EXISTS || relationship == PRE_UNKNOWN)
		{
			precSync.unlock();
			const PageNumber low_page = low->bdb_page;
			if (!write_buffer(tdbb, low, low_page, false, tdbb->tdbb_status_vector, true))
				CCH_unwind(tdbb, true);

			precSync.lock(SYNC_EXCLUSIVE);
		}
		else
			break;
	}

	// We're going to establish a new precedence relationship.  Get a block,
	// fill in the appropriate fields, and insert it into the various ques

	Precedence* precedence = bcb->bcb_free;
	if (precedence)
		bcb->bcb_free = (Precedence*) precedence->pre_hi;
	else
		precedence = FB_NEW_POOL(*bcb->bcb_bufferpool) Precedence;

	precedence->pre_low = low;
	precedence->pre_hi = high;
	precedence->pre_flags = 0;
	QUE_INSERT(low->bdb_higher, precedence->pre_higher);
	QUE_INSERT(high->bdb_lower, precedence->pre_lower);

	// explicitly include high page in system transaction flush process
	if (low->bdb_flags & BDB_system_dirty && high->bdb_flags & BDB_dirty)
		high->bdb_flags |= BDB_system_dirty;
}



static void clear_precedence(thread_db* tdbb, BufferDesc* bdb)
{
/**************************************
 *
 *	c l e a r _ p r e c e d e n c e
 *
 **************************************
 *
 * Functional description
 *	Clear precedence relationships to lower precedence block.
 *
 **************************************/
	SET_TDBB(tdbb);

	if (QUE_EMPTY(bdb->bdb_lower))
		return;

	BufferControl* const bcb = bdb->bdb_bcb;
	Sync precSync(&bcb->bcb_syncPrecedence, "clear_precedence");
	if (!bcb->bcb_syncPrecedence.ourExclusiveLock())
		precSync.lock(SYNC_EXCLUSIVE);

	// Loop thru lower precedence buffers.  If any can be downgraded,
	// by all means down grade them.

	while (QUE_NOT_EMPTY(bdb->bdb_lower))
	{
		QUE que_inst = bdb->bdb_lower.que_forward;
		Precedence* precedence = BLOCK(que_inst, Precedence, pre_lower);
		BufferDesc* low_bdb = precedence->pre_low;
		QUE_DELETE(precedence->pre_higher);
		QUE_DELETE(precedence->pre_lower);

		precedence->pre_hi = (BufferDesc*) bcb->bcb_free;
		bcb->bcb_free = precedence;
		if (!(precedence->pre_flags & PRE_cleared))
		{
			if (low_bdb->bdb_ast_flags & BDB_blocking)
				PAGE_LOCK_RE_POST(tdbb, bcb, low_bdb->bdb_lock);
		}
	}
}


static void down_grade(thread_db* tdbb, BufferDesc* bdb, int high)
{
/**************************************
 *
 *	d o w n _ g r a d e
 *
 **************************************
 *
 * Functional description
 *	A lock on a page is blocking another process.  If possible, downgrade
 *	the lock on the buffer.  This may be called from either AST or
 *	regular level.  Return true if the down grade was successful.  If the
 *	down grade was deferred for any reason, return false.
 *
 **************************************/
	SET_TDBB(tdbb);

	const bool oldBlocking = (bdb->bdb_ast_flags.exchangeBitOr(BDB_blocking) & BDB_blocking);
	Lock* lock = bdb->bdb_lock;
	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = bdb->bdb_bcb;

	if (dbb->dbb_flags & DBB_bugcheck)
	{
		PAGE_LOCK_RELEASE(tdbb, bcb, lock);
		bdb->bdb_ast_flags &= ~BDB_blocking;

		clear_dirty_flag_and_nbak_state(tdbb, bdb);
		return; // true;
	}

	// If the BufferDesc is in use and, being written or already
	// downgraded to read, mark it as blocking and exit.

	bool justWrite = false;

	if (bdb->isLocked() || !bdb->addRefConditional(tdbb, SYNC_EXCLUSIVE))
	{
		if (!high)
			return; // false;

		// hvlad: buffer is in use and we can't downgrade its lock. But if this
		// buffer blocks some lower precedence buffer, it is enough to just write
		// our (high) buffer to clear precedence and thus allow blocked (low)
		// buffer to be downgraded. IO lock guarantees that there is no buffer
		// modification in progress currently and it is safe to write it right now.
		// No need to mark our buffer as blocking nor to change state of our lock.

		bdb->lockIO(tdbb);
		if (!(bdb->bdb_flags & BDB_dirty))
		{
			bdb->unLockIO(tdbb);
			return; // true
		}

		if (!oldBlocking)
			bdb->bdb_ast_flags &= ~BDB_blocking;

		justWrite = true;
	}

	// If the page isn't dirty, the lock can be quietly downgraded.

	if (!(bdb->bdb_flags & BDB_dirty))
	{
		bdb->bdb_ast_flags &= ~BDB_blocking;
		LCK_downgrade(tdbb, lock);
		bdb->release(tdbb, false);
		return; // true;
	}

	bool in_use = false, invalid = false;

	if (bdb->bdb_flags & BDB_not_valid)
		invalid = true;

	// If there are higher precedence guys, see if they can be written.

	while (QUE_NOT_EMPTY(bdb->bdb_higher))
	{ // syncPrec scope
		Sync syncPrec(&bcb->bcb_syncPrecedence, "down_grade");
		syncPrec.lock(SYNC_EXCLUSIVE);

		bool found = false;
		for (QUE que_inst = bdb->bdb_higher.que_forward; que_inst != &bdb->bdb_higher;
			 que_inst = que_inst->que_forward)
		{
			Precedence* precedence = BLOCK(que_inst, Precedence, pre_higher);
			if (precedence->pre_flags & PRE_cleared)
				continue;

			if (invalid)
			{
				precedence->pre_flags |= PRE_cleared;
				continue;
			}

			BufferDesc* blocking_bdb = precedence->pre_hi;
			if (blocking_bdb->bdb_flags & BDB_dirty)
			{
				found = true;
				syncPrec.unlock();
				down_grade(tdbb, blocking_bdb, high + 1);

				if ((blocking_bdb->bdb_flags & BDB_dirty) && !(precedence->pre_flags & PRE_cleared))
					in_use = true;

				if (blocking_bdb->bdb_flags & BDB_not_valid)
				{
					invalid = true;
					in_use = false;
					que_inst = bdb->bdb_higher.que_forward;
				}

				break;
			}
		}

		// If any higher precedence buffer can't be written, mark this buffer as blocking and exit.

		if (in_use)
		{
			if (justWrite)
				bdb->unLockIO(tdbb);
			else
				bdb->release(tdbb, false);

			return; // false;
		}

		if (!found)
			break;
	} // syncPrec scope

	// Everything is clear to write this buffer.  Do so and reduce the lock

	if (!justWrite)
		bdb->lockIO(tdbb);

	const bool written = !(bdb->bdb_flags & BDB_dirty) ||
		write_page(tdbb, bdb, tdbb->tdbb_status_vector, true);

	if (!justWrite)
		bdb->unLockIO(tdbb);

	if (invalid || !written)
	{
		bdb->bdb_flags |= BDB_not_valid;
		clear_dirty_flag_and_nbak_state(tdbb, bdb);
		bdb->bdb_ast_flags &= ~BDB_blocking;
		TRA_invalidate(tdbb, bdb->bdb_transactions);
		bdb->bdb_transactions = 0;
		PAGE_LOCK_RELEASE(tdbb, bcb, bdb->bdb_lock);
	}
	else if (!justWrite)
	{
		bdb->bdb_ast_flags &= ~BDB_blocking;
		LCK_downgrade(tdbb, lock);
	}

	// Clear precedence relationships to lower precedence buffers.  Since it
	// isn't safe to tweak the que pointers from AST level, just mark the precedence
	// links as cleared.  Somebody else will clean up the precedence blocks.

	while (QUE_NOT_EMPTY(bdb->bdb_lower))
	{ // syncPrec scope
		Sync syncPrec(&bcb->bcb_syncPrecedence, "down_grade");
		syncPrec.lock(SYNC_EXCLUSIVE);

		bool found = false;
		for (QUE que_inst = bdb->bdb_lower.que_forward; que_inst != &bdb->bdb_lower;
			 que_inst = que_inst->que_forward)
		{
			Precedence* precedence = BLOCK(que_inst, Precedence, pre_lower);
			if (precedence->pre_flags & PRE_cleared)
				continue;

			BufferDesc* blocking_bdb = precedence->pre_low;
			if (bdb->bdb_flags & BDB_not_valid) {
				blocking_bdb->bdb_flags |= BDB_not_valid;
			}

			precedence->pre_flags |= PRE_cleared;
			if ((blocking_bdb->bdb_flags & BDB_not_valid) || (blocking_bdb->bdb_ast_flags & BDB_blocking))
			{
				found = true;
				syncPrec.unlock();
				down_grade(tdbb, blocking_bdb);
				break;
			}
		}

		if (!found)
			break;
	} // syncPrec scope

	bdb->bdb_flags &= ~BDB_not_valid;

	if (justWrite)
		bdb->unLockIO(tdbb);
	else
		bdb->release(tdbb, false);

	return; // true;
}


static bool expand_buffers(thread_db* tdbb, ULONG number)
{
/**************************************
 *
 *	e x p a n d _ b u f f e r s
 *
 **************************************
 *
 * Functional description
 *	Expand the cache to at least a given number of buffers.
 *	If it's already that big, don't do anything.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();
	BufferControl* const bcb = dbb->dbb_bcb;

	if (number <= bcb->bcb_count || number > MAX_PAGE_BUFFERS)
		return false;

	SyncLockGuard syncBcb(&bcb->bcb_syncObject, SYNC_EXCLUSIVE, FB_FUNCTION);

	if (number <= bcb->bcb_count)
		return false;

	// Expand hash table only if there is no concurrent attachments
	if ((tdbb->getAttachment()->att_flags & ATT_exclusive) || !(bcb->bcb_flags & BCB_exclusive))
		bcb->bcb_hashTable->resize(number);

	SyncLockGuard syncEmpty(&bcb->bcb_syncEmpty, SYNC_EXCLUSIVE, FB_FUNCTION);
	ULONG allocated = memory_init(tdbb, bcb, number - bcb->bcb_count);

	bcb->bcb_count += allocated;
	bcb->bcb_free_minimum = (SSHORT) MIN(bcb->bcb_count / 4, 128);	// 25% clean page reserve

	return true;
}


static BufferDesc* get_dirty_buffer(thread_db* tdbb)
{
	// This code is only used by the background I/O threads:
	// cache writer, cache reader and garbage collector.

	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = dbb->dbb_bcb;
	int walk = bcb->bcb_free_minimum;
	int chained = walk;

	Sync lruSync(&bcb->bcb_syncLRU, FB_FUNCTION);
	lruSync.lock(SYNC_SHARED);

	for (QUE que_inst = bcb->bcb_in_use.que_backward;
		 que_inst != &bcb->bcb_in_use; que_inst = que_inst->que_backward)
	{
		BufferDesc* bdb = BLOCK(que_inst, BufferDesc, bdb_in_use);

		if (bdb->bdb_flags & BDB_lru_chained)
		{
			if (!--chained)
				break;
			continue;
		}

		if (bdb->bdb_use_count || (bdb->bdb_flags & BDB_free_pending))
			continue;

		if (bdb->bdb_flags & BDB_db_dirty)
		{
			//tdbb->bumpStats(RuntimeStatistics::PAGE_FETCHES); shouldn't it be here?
			return bdb;
		}

		if (!--walk)
			break;
	}

	if (!chained)
	{
		lruSync.unlock();
		lruSync.lock(SYNC_EXCLUSIVE);
		requeueRecentlyUsed(bcb);
	}
	else
		bcb->bcb_flags &= ~BCB_free_pending;

	return NULL;
}


static BufferDesc* get_oldest_buffer(thread_db* tdbb, BufferControl* bcb)
{
/**************************************
 * Function description:
 *       Get candidate for preemption
 *       Found page buffer must have SYNC_EXCLUSIVE lock.
 **************************************/

	int walk = bcb->bcb_free_minimum;
	BufferDesc* bdb = nullptr;

	Sync lruSync(&bcb->bcb_syncLRU, FB_FUNCTION);
	if (bcb->bcb_lru_chain.load() != NULL)
	{
		lruSync.lock(SYNC_EXCLUSIVE);
		requeueRecentlyUsed(bcb);
		lruSync.downgrade(SYNC_SHARED);
	}
	else
		lruSync.lock(SYNC_SHARED);

	for (QUE que_inst = bcb->bcb_in_use.que_backward;
		 que_inst != &bcb->bcb_in_use;
		 que_inst = que_inst->que_backward)
	{
		bdb = nullptr;

		// get the oldest buffer as the least recently used -- note
		// that since there are no empty buffers this queue cannot be empty

		if (bcb->bcb_in_use.que_forward == &bcb->bcb_in_use)
			BUGCHECK(213);	// msg 213 insufficient cache size

		BufferDesc* oldest = BLOCK(que_inst, BufferDesc, bdb_in_use);

		if (oldest->bdb_flags & BDB_lru_chained)
			continue;

		if (oldest->bdb_use_count || !oldest->addRefConditional(tdbb, SYNC_EXCLUSIVE))
			continue;

		/*if (!writeable(oldest))
		{
			oldest->release(tdbb, true);
			continue;
		}*/

		bdb = oldest;
		if (!(bdb->bdb_flags & (BDB_dirty | BDB_db_dirty)) || !walk)
			break;

		if (!(bcb->bcb_flags & BCB_cache_writer))
			break;

		bcb->bcb_flags |= BCB_free_pending;
		if (!(bcb->bcb_flags & BCB_writer_active))
			bcb->bcb_writer_sem.release();

		bdb->release(tdbb, true);
		bdb = nullptr;
		--walk;
	}

	lruSync.unlock();

	if (!bdb)
		return nullptr;

	// If the buffer selected is dirty, arrange to have it written.

	if (bdb->bdb_flags & (BDB_dirty | BDB_db_dirty))
	{
		const bool write_thru = (bcb->bcb_flags & BCB_exclusive);
		if (!write_buffer(tdbb, bdb, bdb->bdb_page, write_thru, tdbb->tdbb_status_vector, true))
		{
			bdb->release(tdbb, true);
			CCH_unwind(tdbb, true);
		}
	}

	// If the buffer is still in the dirty tree, remove it.
	// In any case, release any lock it may have.

	removeDirty(bcb, bdb);

	// Cleanup any residual precedence blocks.  Unless something is
	// screwed up, the only precedence blocks that can still be hanging
	// around are ones cleared at AST level.

	if (QUE_NOT_EMPTY(bdb->bdb_higher) || QUE_NOT_EMPTY(bdb->bdb_lower))
	{
		Sync precSync(&bcb->bcb_syncPrecedence, "get_buffer");
		precSync.lock(SYNC_EXCLUSIVE);

		while (QUE_NOT_EMPTY(bdb->bdb_higher))
		{
			QUE que2 = bdb->bdb_higher.que_forward;
			Precedence* precedence = BLOCK(que2, Precedence, pre_higher);
			QUE_DELETE(precedence->pre_higher);
			QUE_DELETE(precedence->pre_lower);
			precedence->pre_hi = (BufferDesc*)bcb->bcb_free;
			bcb->bcb_free = precedence;
		}

		clear_precedence(tdbb, bdb);
	}

	return bdb;
}


static BufferDesc* get_buffer(thread_db* tdbb, const PageNumber page, SyncType syncType, int wait)
{
/**************************************
 *
 *	g e t _ b u f f e r
 *
 **************************************
 *
 * Functional description
 *	Get a buffer.  If possible, get a buffer already assigned
 *	to the page.  Otherwise get one from the free list or pick
 *	the least recently used buffer to be reused.
 *
 * input
 *	page:		page to get
 *	syncType:	type of lock to acquire on the page.
 *	wait:	1 => Wait as long as necessary to get the lock.
 *				This can cause deadlocks of course.
 *			0 => If the lock can't be acquired immediately,
 *				give up and return 0;
 *			<negative number> => Latch timeout interval in seconds.
 *
 * return
 *	BufferDesc pointer if successful.
 *	NULL pointer if timeout occurred (only possible is wait <> 1).
 *		if cache manager doesn't have any pages to write anymore.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = dbb->dbb_bcb;
	Attachment* att = tdbb->getAttachment();

	if (att && att->att_bdb_cache)
	{
		if (BufferDesc* bdb = att->att_bdb_cache->get(page))
		{
			if (bdb->addRef(tdbb, syncType, wait))
			{
				if (bdb->bdb_page == page)
				{
					recentlyUsed(bdb);
					tdbb->bumpStats(RuntimeStatistics::PAGE_FETCHES);
					return bdb;
				}

				bdb->release(tdbb, true);
				att->att_bdb_cache->remove(page);
			}
			else
			{
				fb_assert(wait <= 0);
				if (bdb->bdb_page == page)
					return nullptr;
			}
		}
	}

	while (true)
	{
		BufferDesc* bdb = nullptr;
		bool is_empty = false;
		while (!bdb)
		{
			// try to get already existing buffer
			{
#ifndef HASH_USE_CDS_LIST
				SyncLockGuard bcbSync(&bcb->bcb_syncObject, SYNC_SHARED, FB_FUNCTION);
#endif
				bdb = bcb->bcb_hashTable->find(page);
			}

			if (bdb)
			{
				// latch page buffer if it's been found
				if (!bdb->addRef(tdbb, syncType, wait))
				{
					fb_assert(wait <= 0);
					return nullptr;
				}

				// ensure the found page buffer is still for the same page after latch
				if (bdb->bdb_page == page)
				{
					recentlyUsed(bdb);
					tdbb->bumpStats(RuntimeStatistics::PAGE_FETCHES);
					cacheBuffer(att, bdb);
					return bdb;
				}

				// leave the found page buffer and try another one
				bdb->release(tdbb, true);
				bdb = nullptr;
				continue;
			}

			// try empty list
			if (QUE_NOT_EMPTY(bcb->bcb_empty))
			{
				SyncLockGuard bcbSync(&bcb->bcb_syncEmpty, SYNC_EXCLUSIVE, FB_FUNCTION);
				if (QUE_NOT_EMPTY(bcb->bcb_empty))
				{
					QUE que_inst = bcb->bcb_empty.que_forward;
					QUE_DELETE(*que_inst);
					QUE_INIT(*que_inst);
					bdb = BLOCK(que_inst, BufferDesc, bdb_que);

					bcb->bcb_inuse++;
					is_empty = true;
				}
			}

			if (bdb)
				bdb->addRef(tdbb, SYNC_EXCLUSIVE);
			else
			{
				bdb = get_oldest_buffer(tdbb, bcb);
				if (!bdb)
				{
					Thread::yield();
				}
				else if (bdb->bdb_page == page)
				{
					bdb->downgrade(syncType);
					recentlyUsed(bdb);
					tdbb->bumpStats(RuntimeStatistics::PAGE_FETCHES);
					cacheBuffer(att, bdb);
					return bdb;
				}
			}
		}

		fb_assert(bdb->ourExclusiveLock());

		// we have either empty buffer or candidate for preemption
		// try to put it into target hash chain
		while (true)
		{
			BufferDesc* bdb2 = nullptr;

			{
#ifndef HASH_USE_CDS_LIST
				SyncLockGuard bcbSync(&bcb->bcb_syncObject, SYNC_EXCLUSIVE, FB_FUNCTION);
#endif
				bdb2 = bcb->bcb_hashTable->emplace(bdb, page, !is_empty);
				if (!bdb2)
				{
					bdb->bdb_page = page;
					bdb->bdb_flags &= BDB_lru_chained; // yes, clear all except BDB_lru_chained
					bdb->bdb_flags |= BDB_read_pending;
					bdb->bdb_scan_count = 0;
					if (bdb->bdb_lock)
						bdb->bdb_lock->lck_logical = LCK_none;

#ifndef HASH_USE_CDS_LIST
					bcbSync.unlock();
#endif

					if (!(bdb->bdb_flags & BDB_lru_chained))
					{
						Sync syncLRU(&bcb->bcb_syncLRU, FB_FUNCTION);
						if (syncLRU.lockConditional(SYNC_EXCLUSIVE))
						{
							QUE_DELETE(bdb->bdb_in_use);
							QUE_INSERT(bcb->bcb_in_use, bdb->bdb_in_use);
						}
						else
							recentlyUsed(bdb);
					}
					tdbb->bumpStats(RuntimeStatistics::PAGE_FETCHES);
					cacheBuffer(att, bdb);
					return bdb;
				}
			}

			// here we hold lock on bdb and ask for lock on bdb2
			// to avoid deadlock, don't wait for bdb2 unless bdb was empty

			const int wait2 = is_empty ? wait : 0;
			if (bdb2->addRef(tdbb, syncType, wait2))
			{
				if (bdb2->bdb_page != page)
				{
					bdb2->release(tdbb, true);
					continue;
				}
				recentlyUsed(bdb2);
				tdbb->bumpStats(RuntimeStatistics::PAGE_FETCHES);
				cacheBuffer(att, bdb2);
			}
			else
				bdb2 = nullptr;

			bdb->release(tdbb, true);
			if (is_empty)
			{
				SyncLockGuard syncEmpty(&bcb->bcb_syncEmpty, SYNC_EXCLUSIVE, FB_FUNCTION);
				QUE_INSERT(bcb->bcb_empty, bdb->bdb_que);
				bcb->bcb_inuse--;
			}

			if (!bdb2 && wait > 0)
				break;

			return bdb2;
		}
	}

	// never get here
	fb_assert(false);
}

static ULONG get_prec_walk_mark(BufferControl* bcb)
{
/**************************************
 *
 *	g e t _ p r e c _ w a l k _ m a r k
 *
 **************************************
 *
 * Functional description
 *  Get next mark for walking precedence graph.
 *
 **************************************/
	fb_assert(bcb->bcb_syncPrecedence.ourExclusiveLock());

	if (++bcb->bcb_prec_walk_mark == 0)
	{
		SyncLockGuard bcbSync(&bcb->bcb_syncObject, SYNC_SHARED, FB_FUNCTION);

		for (auto blk : bcb->bcb_bdbBlocks)
		{
			for (ULONG i = 0; i < blk.m_count; i++)
				blk.m_bdbs[i].bdb_prec_walk_mark = 0;
		}

		bcb->bcb_prec_walk_mark = 1;
	}
	return bcb->bcb_prec_walk_mark;
}


static int get_related(BufferDesc* bdb, PagesArray &lowPages, int limit, const ULONG mark)
{
/**************************************
 *
 *	g e t _ r e l a t e d
 *
 **************************************
 *
 * Functional description
 *  Recursively walk low part of precedence graph of given buffer and put
 *  low pages numbers into array.
 *
 **************************************/
	BufferControl* bcb = bdb->bdb_bcb;
	fb_assert(bcb->bcb_syncPrecedence.ourExclusiveLock());

	const struct que* base = &bdb->bdb_lower;
	for (const struct que* que_inst = base->que_forward; que_inst != base;
		 que_inst = que_inst->que_forward)
	{
		const Precedence* precedence = BLOCK(que_inst, Precedence, pre_lower);
		if (precedence->pre_flags & PRE_cleared)
			continue;

		BufferDesc* low = precedence->pre_low;
		if (low->bdb_prec_walk_mark == mark)
			continue;

		if (!--limit)
			return 0;

		const SLONG lowPage = low->bdb_page.getPageNum();
		FB_SIZE_T pos;
		if (!lowPages.find(lowPage, pos))
			lowPages.insert(pos, lowPage);

		if (QUE_NOT_EMPTY(low->bdb_lower))
		{
			limit = get_related(low, lowPages, limit, mark);
			if (!limit)
				return 0;
		}
		else
			low->bdb_prec_walk_mark = mark;
	}

	bdb->bdb_prec_walk_mark = mark;
	return limit;
}


static LockState lock_buffer(thread_db* tdbb, BufferDesc* bdb, const SSHORT wait,
	const SCHAR page_type)
{
/**************************************
 *
 *	l o c k _ b u f f e r
 *
 **************************************
 *
 * Functional description
 *	Get a lock on page for a buffer.  If the lock ever slipped
 *	below READ, indicate that the page must be read.
 *
 * input:
 *	wait: LCK_WAIT = 1	=> Wait as long a necessary to get the lock.
 *	      LCK_NO_WAIT = 0	=> If the lock can't be acquired immediately,
 *						give up and return -1.
 *	      <negative number>		=> Lock timeout interval in seconds.
 *
 * return: 0  => buffer locked, page is already in memory.
 *	   1  => buffer locked, page needs to be read from disk.
 *	   -1 => timeout on lock occurred, see input parameter 'wait'.
 *
 **************************************/
	SET_TDBB(tdbb);
	BufferControl* const bcb = bdb->bdb_bcb;
	fb_assert(!(bcb->bcb_flags & BCB_exclusive));

	const USHORT lock_type = (bdb->bdb_flags & (BDB_dirty | BDB_writer)) ? LCK_write : LCK_read;

	CCH_TRACE(("FE LOCK %d:%06d, %s", bdb->bdb_page.getPageSpaceID(), bdb->bdb_page.getPageNum(),
		(lock_type >= LCK_write) ? "EX" : "SH" ));

	Lock* const lock = bdb->bdb_lock;

	if (lock->lck_logical >= lock_type)
		return lsLockedHavePage;

	TEXT errmsg[MAX_ERRMSG_LEN + 1];

	ThreadStatusGuard tempStatus(tdbb);

	if (lock->lck_logical == LCK_none)
	{
		// Prevent header and TIP pages from generating blocking AST
		// overhead. The promise is that the lock will unconditionally
		// be released when the buffer use count indicates it is safe to do so.

		if (page_type == pag_header || page_type == pag_transactions)
		{
			fb_assert(lock->lck_ast == blocking_ast_bdb);
			fb_assert(lock->lck_object == bdb);
			lock->lck_ast = 0;
			lock->lck_object = NULL;
		}
		else {
			fb_assert(lock->lck_ast != NULL);
		}

		bdb->bdb_page.getLockStr(lock->getKeyPtr());
		if (LCK_lock_opt(tdbb, lock, lock_type, wait))
		{
			if (!lock->lck_ast)
			{
				// Restore blocking AST to lock block if it was swapped out.
				// Flag the BufferDesc so that the lock is released when the buffer is released.

				fb_assert(page_type == pag_header || page_type == pag_transactions);
				lock->lck_ast = blocking_ast_bdb;
				lock->lck_object = bdb;
				bdb->bdb_flags |= BDB_no_blocking_ast;
			}

			return lsLocked;
		}

		if (!lock->lck_ast)
		{
			fb_assert(page_type == pag_header || page_type == pag_transactions);
			lock->lck_ast = blocking_ast_bdb;
			lock->lck_object = bdb;
		}

		// Case: a timeout was specified, or the caller didn't want to wait, return the error.

		if ((wait == LCK_NO_WAIT) || ((wait < 0) && (tempStatus->getErrors()[1] == isc_lock_timeout)))
		{
			bdb->release(tdbb, false);
			return lsLockTimeout;
		}

		// Case: lock manager detected a deadlock, probably caused by locking the
		// BufferDesc's in an unfortunate order.  Nothing we can do about it, return the
		// error, and log it to firebird.log.

		FbStatusVector* const status = tempStatus.restore();

		fb_msg_format(0, FB_IMPL_MSG_FACILITY_JRD_BUGCHK, 216, sizeof(errmsg), errmsg,
			MsgFormat::SafeArg() << bdb->bdb_page.getPageNum() << (int) page_type);
		ERR_append_status(status, Arg::Gds(isc_random) << Arg::Str(errmsg));
		ERR_log(FB_IMPL_MSG_FACILITY_JRD_BUGCHK, 216, errmsg);	// // msg 216 page %ld, page type %ld lock denied

		// CCH_unwind releases all the BufferDesc's and calls ERR_punt()
		// ERR_punt will longjump.

		CCH_unwind(tdbb, true);
	}

	// Lock requires an upward conversion.  Sigh.  Try to get the conversion.
	// If it fails, release the lock and re-seize. Save the contents of the
	// status vector just in case

	const LockState must_read = (lock->lck_logical < LCK_read) ? lsLocked : lsLockedHavePage;

	if (LCK_convert_opt(tdbb, lock, lock_type))
		return must_read;

	if (wait == LCK_NO_WAIT)
	{
		bdb->release(tdbb, true);
		return lsLockTimeout;
	}

	if (LCK_lock(tdbb, lock, lock_type, wait))
		return lsLocked;

	// Case: a timeout was specified, or the caller didn't want to wait, return the error.

	if ((wait < 0) && (tempStatus->getErrors()[1] == isc_lock_timeout))
	{
		bdb->release(tdbb, false);
		return lsLockTimeout;
	}

	// Case: lock manager detected a deadlock, probably caused by locking the
	// BufferDesc's in an unfortunate order.  Nothing we can do about it, return the
	// error, and log it to firebird.log.

	FbStatusVector* const status = tempStatus.restore();

	fb_msg_format(0, FB_IMPL_MSG_FACILITY_JRD_BUGCHK, 215, sizeof(errmsg), errmsg,
					MsgFormat::SafeArg() << bdb->bdb_page.getPageNum() << (int) page_type);
	ERR_append_status(status, Arg::Gds(isc_random) << Arg::Str(errmsg));
	ERR_log(FB_IMPL_MSG_FACILITY_JRD_BUGCHK, 215, errmsg);	// msg 215 page %ld, page type %ld lock conversion denied

	CCH_unwind(tdbb, true);

	return lsError;		// Added to get rid of Compiler Warning
}


static ULONG memory_init(thread_db* tdbb, BufferControl* bcb, ULONG number)
{
/**************************************
 *
 *	m e m o r y _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize memory for the cache.
 *	Return number of buffers allocated.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	ULONG buffers = 0;
	const size_t page_size = dbb->dbb_page_size;
	UCHAR* memory = nullptr;
	UCHAR* lock_memory = nullptr;
	const UCHAR* memory_end = nullptr;
	BufferDesc* tail = nullptr;

	const size_t lock_key_extra = PageNumber::getLockLen() > Lock::KEY_STATIC_SIZE ?
		PageNumber::getLockLen() - Lock::KEY_STATIC_SIZE : 0;

	const size_t lock_size = (bcb->bcb_flags & BCB_exclusive) ? 0 :
		FB_ALIGN(sizeof(Lock) + lock_key_extra, alignof(Lock));

	while (number)
	{
		if (!memory)
		{
			// Allocate memory block big enough to accommodate BufferDesc's, Lock's and page buffers.

			ULONG to_alloc = number;

			while (true)
			{
				const size_t memory_size = (sizeof(BufferDesc) + lock_size + page_size) * (to_alloc + 1);

				fb_assert(memory_size > 0);
				if (memory_size < MIN_BUFFER_SEGMENT)
				{
					// Diminishing returns
					return buffers;
				}

				try
				{
					memory = (UCHAR*) bcb->bcb_bufferpool->allocate(memory_size ALLOC_ARGS);
					memory_end = memory + memory_size;
					break;
				}
				catch (ScratchBird::BadAlloc&)
				{
					// Either there's not enough virtual memory or there is
					// but it's not virtually contiguous. Let's find out by
					// cutting the size in half to see if the buffers can be
					// scattered over the remaining virtual address space.
					to_alloc >>= 1;
				}
			}
			bcb->bcb_memory.push(memory);

			tail = (BufferDesc*) FB_ALIGN(memory, alignof(BufferDesc));

			BufferControl::BDBBlock blk;
			blk.m_bdbs = tail;
			blk.m_count = to_alloc;
			bcb->bcb_bdbBlocks.push(blk);

			// Allocate buffers on an address that is an even multiple
			// of the page size (rather the physical sector size.) This
			// is a necessary condition to support raw I/O interfaces.
			memory = (UCHAR*) (blk.m_bdbs + to_alloc);
			if (!(bcb->bcb_flags & BCB_exclusive))
			{
				lock_memory = FB_ALIGN(memory, lock_size);
				memory = (UCHAR*) (lock_memory + lock_size * to_alloc);
			}
			memory = FB_ALIGN(memory, page_size);

			fb_assert(memory_end >= memory + page_size * to_alloc);
		}

		tail = ::new(tail) BufferDesc(bcb);

		if (!(bcb->bcb_flags & BCB_exclusive))
		{
			tail->bdb_lock = ::new(lock_memory)
				Lock(tdbb, PageNumber::getLockLen(), LCK_bdb, tail, blocking_ast_bdb);

			lock_memory += lock_size;
		}

		tail->bdb_buffer = (pag*) memory;
		memory += bcb->bcb_page_size;

		QUE_INSERT(bcb->bcb_empty, tail->bdb_que);
		tail++;

		buffers++;				// Allocated buffers
		number--;				// Remaining buffers

		// Check if memory segment has been exhausted.

		if (memory + page_size > memory_end)
		{
			const auto blk = bcb->bcb_bdbBlocks.end() - 1;
			const BufferDesc* bdb = blk->m_bdbs;

			if (!(bcb->bcb_flags & BCB_exclusive))
			{
				// first lock block is after last BufferDesc
				fb_assert((char*) bdb->bdb_lock >= (char*) tail);

				// first page buffer is after last lock block
				fb_assert((char*) bdb->bdb_buffer >= (char*) tail[-1].bdb_lock + lock_size);
			}
			else
			{
				// first page buffer is after last BufferDesc
				fb_assert((char*) bdb->bdb_buffer >= (char*) tail);
			}

			memory = nullptr;
		}
	}

	return buffers;
}


static void page_validation_error(thread_db* tdbb, WIN* window, SSHORT type)
{
/**************************************
 *
 *	p a g e _ v a l i d a t i o n _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	We've detected a validation error on fetch.  Generally
 *	we've detected that the type of page fetched didn't match the
 *	type of page we were expecting.  Report an error and
 *	get out.
 *	This function will only be called rarely, as a page validation
 *	error is an indication of on-disk database corruption.
 *
 **************************************/
	SET_TDBB(tdbb);
	BufferDesc* bdb = window->win_bdb;
	const pag* page = bdb->bdb_buffer;

	PageSpace* pages =
		tdbb->getDatabase()->dbb_page_manager.findPageSpace(bdb->bdb_page.getPageSpaceID());

	ERR_build_status(tdbb->tdbb_status_vector,
					 Arg::Gds(isc_db_corrupt) << Arg::Str(pages->file->fil_string) <<
					 Arg::Gds(isc_page_type_err) <<
					 Arg::Gds(isc_badpagtyp) << Arg::Num(bdb->bdb_page.getPageNum()) <<
												pagtype(type) <<
												pagtype(page->pag_type));
	// We should invalidate this bad buffer.
	CCH_unwind(tdbb, true);
}


#ifdef CACHE_READER
static void prefetch_epilogue(Prefetch* prefetch, FbStatusVector* status_vector)
{
/**************************************
 *
 *	p r e f e t c h _ e p i l o g u e
 *
 **************************************
 *
 * Functional description
 *	Stall on asynchronous I/O completion.
 *	Move data from prefetch buffer to database
 *	buffers, compute the checksum, and release
 *	the latch.
 *
 **************************************/
	if (!(prefetch->prf_flags & PRF_active))
		return;

	thread_db* tdbb = prefetch->prf_tdbb;
	Database* dbb = tdbb->getDatabase();

	prefetch->prf_piob.piob_wait = TRUE;
	const bool async_status = PIO_status(dbb, &prefetch->prf_piob, status_vector);

	// If there was an I/O error release all buffer latches acquired for the prefetch request.

	if (!async_status)
	{
		BufferDesc** next_bdb = prefetch->prf_bdbs;
		for (USHORT i = 0; i < prefetch->prf_max_prefetch; i++)
		{
			if (*next_bdb)
				release_bdb(tdbb, *next_bdb, true, false, false);

			next_bdb++;
		}
		prefetch->prf_flags &= ~PRF_active;
		return;
	}

	const SCHAR* next_buffer = prefetch->prf_io_buffer;
	BufferDesc** next_bdb = prefetch->prf_bdbs;

	for (USHORT i = 0; i < prefetch->prf_max_prefetch; i++)
	{
		if (*next_bdb)
		{
			pag* page = (*next_bdb)->bdb_buffer;
			if (next_buffer != reinterpret_cast<char*>(page))
				memcpy(page, next_buffer, (ULONG) dbb->dbb_page_size);

			if (page->pag_pageno == (*next_bdb)->bdb_page.getPageNum())
			{
				(*next_bdb)->bdb_flags &= ~(BDB_read_pending | BDB_not_valid);
				(*next_bdb)->bdb_flags |= BDB_prefetch;
			}

			release_bdb(tdbb, *next_bdb, true, false, false);
		}

		next_buffer += dbb->dbb_page_size;
		next_bdb++;
	}

	prefetch->prf_flags &= ~PRF_active;
}


static void prefetch_init(Prefetch* prefetch, thread_db* tdbb)
{
/**************************************
 *
 *	p r e f e t c h _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize prefetch data structure.
 *	Most systems that allow access to "raw" I/O
 *	interfaces want the buffer address aligned.
 *
 **************************************/
	Database* dbb = tdbb->getDatabase();

	prefetch->prf_tdbb = tdbb;
	prefetch->prf_flags = 0;
	prefetch->prf_max_prefetch = PREFETCH_MAX_TRANSFER / dbb->dbb_page_size;
	prefetch->prf_aligned_buffer =
		(SCHAR*) (((U_IPTR) &prefetch->prf_unaligned_buffer + MIN_PAGE_SIZE - 1) &
		~((U_IPTR) MIN_PAGE_SIZE - 1));
}


static void prefetch_io(Prefetch* prefetch, FbStatusVector* status_vector)
{
/**************************************
 *
 *	p r e f e t c h _ i o
 *
 **************************************
 *
 * Functional description
 *	Queue an asynchronous I/O to read
 *	multiple pages into prefetch buffer.
 *
 **************************************/
	thread_db* tdbb = prefetch->prf_tdbb;
	Database* dbb = tdbb->getDatabase();

	if (!prefetch->prf_page_count)
		prefetch->prf_flags &= ~PRF_active;
	else
	{
		// Get the cache reader working on our behalf too

		if (!(dbb->dbb_bcb->bcb_flags & BCB_reader_active))
			dbb->dbb_reader_sem.post();

		const bool async_status =
			PIO_read_ahead(dbb, prefetch->prf_start_page, prefetch->prf_io_buffer,
						   prefetch->prf_page_count, &prefetch->prf_piob, status_vector);
		if (!async_status)
		{
			BufferDesc** next_bdb = prefetch->prf_bdbs;
			for (USHORT i = 0; i < prefetch->prf_max_prefetch; i++)
			{
				if (*next_bdb)
					release_bdb(tdbb, *next_bdb, true, false, false);

				next_bdb++;
			}
			prefetch->prf_flags &= ~PRF_active;
		}
	}
}


static void prefetch_prologue(Prefetch* prefetch, SLONG* start_page)
{
/**************************************
 *
 *	p r e f e t c h _ p r o l o g u e
 *
 **************************************
 *
 * Functional description
 *	Search for consecutive pages to be prefetched
 *	and latch them for I/O.
 *
 **************************************/
	thread_db* tdbb = prefetch->prf_tdbb;
	Database* dbb = tdbb->getDatabase();
	BufferControl* bcb = dbb->dbb_bcb;

	prefetch->prf_start_page = *start_page;
	prefetch->prf_page_count = 0;
	prefetch->prf_flags |= PRF_active;

	BufferDesc** next_bdb = prefetch->prf_bdbs;
	for (USHORT i = 0; i < prefetch->prf_max_prefetch; i++)
	{
		*next_bdb = 0;
		if (SBM_clear(bcb->bcb_prefetch, *start_page) &&
			(*next_bdb = get_buffer(tdbb, *start_page, LATCH_shared, 0)))
		{
			if ((*next_bdb)->bdb_flags & BDB_read_pending)
				prefetch->prf_page_count = i + 1;
			else
			{
				release_bdb(tdbb, *next_bdb, true, false, false);
				*next_bdb = 0;
			}
		}

		next_bdb++;
		(*start_page)++;
	}

	// Optimize non-sequential list prefetching to transfer directly to database buffers.

	BufferDesc* bdb;
	if (prefetch->prf_page_count == 1 && (bdb = prefetch->prf_bdbs[0]))
		prefetch->prf_io_buffer = reinterpret_cast<char*>(bdb->bdb_buffer);
	else
		prefetch->prf_io_buffer = prefetch->prf_aligned_buffer;

	// Reset starting page for next bitmap walk

	--(*start_page);
}
#endif // CACHE_READER


static SSHORT related(BufferDesc* low, const BufferDesc* high, SSHORT limit, const ULONG mark)
{
/**************************************
 *
 *	r e l a t e d
 *
 **************************************
 *
 * Functional description
 *	See if there are precedence relationships linking two buffers.
 *	Since precedence graphs can become very complex, limit search for
 *	precedence relationship by visiting a presribed limit of higher
 *	precedence blocks.
 *
 **************************************/
	const struct que* base = &low->bdb_higher;

	for (const struct que* que_inst = base->que_forward; que_inst != base; que_inst = que_inst->que_forward)
	{
		if (!--limit)
			return PRE_UNKNOWN;

		const Precedence* precedence = BLOCK(que_inst, Precedence, pre_higher);
		if (!(precedence->pre_flags & PRE_cleared))
		{
			if (precedence->pre_hi->bdb_prec_walk_mark == mark)
				continue;

			if (precedence->pre_hi == high)
				return PRE_EXISTS;

			if (QUE_NOT_EMPTY(precedence->pre_hi->bdb_higher))
			{
				limit = related(precedence->pre_hi, high, limit, mark);
				if (limit == PRE_EXISTS || limit == PRE_UNKNOWN)
					return limit;
			}
			else
				precedence->pre_hi->bdb_prec_walk_mark = mark;
		}
	}

	low->bdb_prec_walk_mark = mark;
	return limit;
}


#ifdef NOT_USED_OR_REPLACED
static inline bool writeable(BufferDesc* bdb)
{
/**************************************
 *
 *	w r i t e a b l e
 *
 **************************************
 *
 * Functional description
 *	See if a buffer is writeable.  A buffer is writeable if
 *	neither it nor any of it's higher precedence cousins are
 *	marked for write.
 *  This is the starting point of recursive walk of precedence
 *  graph. The writeable_mark member is used to mark already seen
 *  buffers to avoid repeated walk of the same sub-graph.
 *  Currently this function can't be called from more than one
 *  thread simultaneously. When SMP will be implemented we must
 *  take additional care about thread-safety.
 *
 **************************************/
	if (bdb->bdb_flags & BDB_marked)
		return false;

	if (QUE_EMPTY(bdb->bdb_higher))
		return true;

	BufferControl* bcb = bdb->bdb_bcb;

	Sync syncPrec(&bcb->bcb_syncPrecedence, "writeable");
	syncPrec.lock(SYNC_EXCLUSIVE);

	const ULONG mark = get_prec_walk_mark(bcb);
	return is_writeable(bdb, mark);
}


static bool is_writeable(BufferDesc* bdb, const ULONG mark)
{
/**************************************
 *
 *	i s _ w r i t e a b l e
 *
 **************************************
 *
 * Functional description
 *	See if a buffer is writeable.  A buffer is writeable if
 *	neither it nor any of it's higher precedence cousins are
 *	marked for write.
 *
 **************************************/

	// If there are buffers that must be written first, check them, too.

	for (const que* queue = bdb->bdb_higher.que_forward;
		queue != &bdb->bdb_higher; queue = queue->que_forward)
	{
		const Precedence* precedence = BLOCK(queue, Precedence, pre_higher);

		if (!(precedence->pre_flags & PRE_cleared))
		{
			BufferDesc* high = precedence->pre_hi;

			if (high->bdb_flags & BDB_marked)
				return false;

			if (high->bdb_prec_walk_mark != mark)
			{
				if (QUE_EMPTY(high->bdb_higher))
					high->bdb_prec_walk_mark = mark;
				else if (!is_writeable(high, mark))
					return false;
			}
		}
	}

	bdb->bdb_prec_walk_mark = mark;
	return true;
}
#endif	// NOT_USED_OR_REPLACED


static int write_buffer(thread_db* tdbb,
						BufferDesc* bdb,
						const PageNumber page,
						const bool write_thru,
						FbStatusVector* const status, const bool write_this_page)
{
/**************************************
 *
 *	w r i t e _ b u f f e r
 *
 **************************************
 *
 * Functional description
 *	Write a dirty buffer.  This may recurse due to
 *	precedence problems.
 *
 * input:  write_this_page
 *		= true if the input page needs to be written
 *			before returning.  (normal case)
 *		= false if the input page is being written
 *			because of precedence.  Only write
 *			one page and return so that the caller
 *			can re-establish the need to write this
 *			page.
 *
 * return: 0 = Write failed.
 *         1 = Page is written.  Page was written by this
 *     		call, or was written by someone else, or the
 *		cache buffer is already reassigned.
 *	   2 = Only possible if write_this_page is false.
 *		This input page is not written.  One
 *		page higher in precedence is written
 *		though.  Probable action: re-establich the
 * 		need to write this page and retry write.
 *
 **************************************/
	SET_TDBB(tdbb);
#ifdef SUPERSERVER_V2
	Database* const dbb = tdbb->getDatabase();
#endif

	bdb->lockIO(tdbb);
	if (bdb->bdb_page != page)
	{
		bdb->unLockIO(tdbb);
		return 1;
	}

	if ((bdb->bdb_flags & BDB_marked) && !(bdb->bdb_flags & BDB_faked))
		BUGCHECK(217);	// msg 217 buffer marked for update

	if (!(bdb->bdb_flags & BDB_dirty) && !(write_thru && bdb->bdb_flags & BDB_db_dirty))
	{
		bdb->unLockIO(tdbb);
		clear_precedence(tdbb, bdb);
		return 1;
	}

	// If there are buffers that must be written first, write them now.

	BufferControl *bcb = bdb->bdb_bcb;
	if (QUE_NOT_EMPTY(bdb->bdb_higher))
	{
		Sync syncPrec(&bcb->bcb_syncPrecedence, "write_buffer");

		while (true)
		{
			syncPrec.lock(SYNC_EXCLUSIVE);

			if (QUE_EMPTY(bdb->bdb_higher))
			{
				syncPrec.unlock();
				break;
			}

			QUE que_inst = bdb->bdb_higher.que_forward;
			Precedence* precedence = BLOCK(que_inst, Precedence, pre_higher);
			if (precedence->pre_flags & PRE_cleared)
			{
				QUE_DELETE(precedence->pre_higher);
				QUE_DELETE(precedence->pre_lower);
				precedence->pre_hi = (BufferDesc*) bcb->bcb_free;
				bcb->bcb_free = precedence;

				syncPrec.unlock();
			}
			else
			{
				bdb->unLockIO(tdbb);

				BufferDesc* hi_bdb = precedence->pre_hi;
				const PageNumber hi_page = hi_bdb->bdb_page;

				int write_status = 0;

				syncPrec.unlock();
				write_status = write_buffer(tdbb, hi_bdb, hi_page, write_thru, status, false);

				if (write_status == 0)
					return 0;		// return IO error

				if (!write_this_page)
					return 2;		// caller wants to re-establish the need for this write after one precedence write

				bdb->lockIO(tdbb);
				if (bdb->bdb_page != page)
				{
					bdb->unLockIO(tdbb);
					return 1;
				}
			}
		}
	}

#ifdef SUPERSERVER_V2
	// Header page I/O is deferred until a dirty page, which was modified by a
	// transaction not updated on the header page, needs to be written. Note
	// that the header page needs to be written with the target page latched to
	// prevent younger transactions from modifying the target page.

	if (page != HEADER_PAGE_NUMBER && bdb->bdb_mark_transaction > dbb->dbb_last_header_write)
	{
		TRA_header_write(tdbb, dbb, bdb->bdb_mark_transaction);
	}
#endif

	// Unless the buffer has been faked (recently re-allocated), write out the page

	bool result = true;
	if ((bdb->bdb_flags & BDB_dirty || (write_thru && bdb->bdb_flags & BDB_db_dirty)) &&
		!(bdb->bdb_flags & BDB_marked))
	{
		result = write_page(tdbb, bdb, status, false);
	}

	bdb->unLockIO(tdbb);
	if (result)
		clear_precedence(tdbb, bdb);

	if (!result)
		return 0;

	if (!write_this_page)
		return 2;

	return 1;
}


static bool write_page(thread_db* tdbb, BufferDesc* bdb, FbStatusVector* const status, const bool inAst)
{
/**************************************
 *
 *	w r i t e _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Do actions required when writing a database page,
 *	including journaling, shadowing.
 *
 **************************************/

	CCH_TRACE(("WRITE   %d:%06d", bdb->bdb_page.getPageSpaceID(), bdb->bdb_page.getPageNum()));

	// hvlad: why it is needed in Vulcan ???
	//Sync syncWrite(&bcb->bcb_syncPageWrite, "write_page");
	//syncWrite.lock(SYNC_EXCLUSIVE);

	if (bdb->bdb_flags & BDB_not_valid)
	{
		ERR_build_status(status, Arg::Gds(isc_buf_invalid) << Arg::Num(bdb->bdb_page.getPageNum()));
		return false;
	}

	Database* const dbb = tdbb->getDatabase();
	pag* const page = bdb->bdb_buffer;

	// Before writing db header page, make sure that
	// the next_transaction > oldest_active transaction
	if (bdb->bdb_page == HEADER_PAGE_NUMBER)
	{
		const auto header = (const header_page*) page;

		const TraNumber next_transaction = header->hdr_next_transaction;
		const TraNumber oldest_transaction = header->hdr_oldest_transaction;
		const TraNumber oldest_active = header->hdr_oldest_active;

		if (next_transaction)
		{
			if (oldest_transaction > next_transaction)
				BUGCHECK(267);	// next transaction older than oldest transaction

			if (oldest_active > next_transaction)
				BUGCHECK(266);	// next transaction older than oldest active
		}
	}

	page->pag_generation++;
	bool result = true;

	//if (!dbb->dbb_wal || write_thru) becomes
	//if (true || write_thru) then finally if (true)
	// I won't wipe out the if() itself to allow my changes be verified easily by others
	if (true)
	{
		tdbb->bumpStats(RuntimeStatistics::PAGE_WRITES);

		// write out page to main database file, and to any
		// shadows, making a special case of the header page
		BackupManager* bm = dbb->dbb_backup_manager;
		const auto backupState = bm->getState();

		/// ASF: Always true: if (bdb->bdb_page.getPageNum() >= 0)
		{
			fb_assert(backupState != Ods::hdr_nbak_unknown);
			page->pag_pageno = bdb->bdb_page.getPageNum();

#ifdef NBAK_DEBUG
			// We cannot call normal trace functions here as they are signal-unsafe
			// "Write page=%d, dir=%d, diff=%d, scn=%d"
			char buffer[1000], *ptr = buffer;
			strcpy(ptr, "NBAK, Write page ");
			ptr += strlen(ptr);
			gds__ulstr(ptr, bdb->bdb_page.getPageNum(), 0, 0);
			ptr += strlen(ptr);

			strcpy(ptr, ", backup_state=");
			ptr += strlen(ptr);
			gds__ulstr(ptr, backupState, 0, 0);
			ptr += strlen(ptr);

			strcpy(ptr, ", diff=");
			ptr += strlen(ptr);
			gds__ulstr(ptr, bdb->bdb_difference_page, 0, 0);
			ptr += strlen(ptr);

			strcpy(ptr, ", scn=");
			ptr += strlen(ptr);
			gds__ulstr(ptr, bdb->bdb_buffer->pag_scn, 0, 0);
			ptr += strlen(ptr);

			gds__trace(buffer);
#endif
			PageSpace* pageSpace =
				dbb->dbb_page_manager.findPageSpace(bdb->bdb_page.getPageSpaceID());
			fb_assert(pageSpace);
			const bool isTempPage = pageSpace->isTemporary();

			if (!isTempPage &&
				(backupState == Ods::hdr_nbak_stalled ||
					(backupState == Ods::hdr_nbak_merge && bdb->bdb_difference_page)))
			{
				if (!dbb->dbb_backup_manager->writeDifference(tdbb, status,
						bdb->bdb_difference_page, bdb->bdb_buffer))
				{
					bdb->bdb_flags |= BDB_io_error;
					dbb->dbb_flags |= DBB_suspend_bgio;
					return false;
				}
			}

			if (!isTempPage && backupState == Ods::hdr_nbak_stalled)
			{
				// We finished. Adjust transaction accounting and get ready for exit
				if (bdb->bdb_page == HEADER_PAGE_NUMBER)
				{
					const auto header = (const header_page*) page;
					dbb->dbb_last_header_write = header->hdr_next_transaction;
				}
			}
			else
			{
				// We need to write our pages to main database files

				class Pio : public CryptoManager::IOCallback
				{
				public:
					Pio(jrd_file* f, BufferDesc* b, bool ast, bool tp, PageSpace* ps)
						: file(f), bdb(b), inAst(ast), isTempPage(tp), pageSpace(ps)
					{ }

					bool callback(thread_db* tdbb, FbStatusVector* status, Ods::pag* page)
					{
						Database* dbb = tdbb->getDatabase();

						while (!PIO_write(tdbb, file, bdb, page, status))
						{
							if (isTempPage || !CCH_rollover_to_shadow(tdbb, dbb, file, inAst))
							{
								bdb->bdb_flags |= BDB_io_error;
								dbb->dbb_flags |= DBB_suspend_bgio;
								return false;
							}

							file = pageSpace->file;
						}

						if (bdb->bdb_page == HEADER_PAGE_NUMBER)
						{
							const auto header = (const header_page*) page;
							dbb->dbb_last_header_write = header->hdr_next_transaction;
						}

						if (dbb->dbb_shadow && !isTempPage)
							return CCH_write_all_shadows(tdbb, 0, bdb, page, status, inAst);

						return true;
					}

				private:
					jrd_file* file;
					BufferDesc* bdb;
					bool inAst;
					bool isTempPage;
					PageSpace* pageSpace;
				};

				Pio io(pageSpace->file, bdb, inAst, isTempPage, pageSpace);
				result = dbb->dbb_crypto_manager->write(tdbb, status, page, &io);
				if (!result && (bdb->bdb_flags & BDB_io_error))
				{
					return false;
				}

			}
		}

		if (result)
			bdb->bdb_flags &= ~BDB_db_dirty;
	}

	if (!result)
	{
		// If there was a write error then idle background threads
		// so that they don't spin trying to write these pages. This
		// usually results from device full errors which can be fixed
		// by someone freeing disk space.

		bdb->bdb_flags |= BDB_io_error;
		dbb->dbb_flags |= DBB_suspend_bgio;
	}
	else
	{
		// clear the dirty bit vector, since the buffer is now
		// clean regardless of which transactions have modified it

		// Destination difference page number is only valid between MARK and
		// write_page so clean it now to avoid confusion
		bdb->bdb_difference_page = 0;
		bdb->bdb_transactions = 0;
		bdb->bdb_mark_transaction = 0;

		if (!(bdb->bdb_bcb->bcb_flags & BCB_keep_pages))
			removeDirty(bdb->bdb_bcb, bdb);

		bdb->bdb_flags &= ~(BDB_must_write | BDB_system_dirty);
		clear_dirty_flag_and_nbak_state(tdbb, bdb);

		if (bdb->bdb_flags & BDB_io_error)
		{
			// If a write error has cleared, signal background threads
			// to resume their regular duties. If someone has freed up
			// disk space these errors will spontaneously go away.

			bdb->bdb_flags &= ~BDB_io_error;
			dbb->dbb_flags &= ~DBB_suspend_bgio;
		}
	}

	return result;
}

static void clear_dirty_flag_and_nbak_state(thread_db* tdbb, BufferDesc* bdb)
{
	const AtomicCounter::counter_type oldFlags = bdb->bdb_flags.exchangeBitAnd(
		~(BDB_dirty | BDB_nbak_state_lock));

	if (oldFlags & BDB_nbak_state_lock)
	{
		NBAK_TRACE(("unlock state for dirty page %d:%06d",
			bdb->bdb_page.getPageSpaceID(), bdb->bdb_page.getPageNum()));

		tdbb->getDatabase()->dbb_backup_manager->unlockStateRead(tdbb);
	}
	else if ((oldFlags & BDB_dirty) && bdb->bdb_page != HEADER_PAGE_NUMBER)
		fb_assert(PageSpace::isTemporary(bdb->bdb_page.getPageSpaceID()));
}

void recentlyUsed(BufferDesc* bdb)
{
	const AtomicCounter::counter_type oldFlags = bdb->bdb_flags.exchangeBitOr(BDB_lru_chained);
	if (oldFlags & BDB_lru_chained)
		return;

	BufferControl* bcb = bdb->bdb_bcb;

#ifdef DEV_BUILD
	volatile BufferDesc* chain = bcb->bcb_lru_chain;
	for (; chain; chain = chain->bdb_lru_chain)
	{
		if (chain == bdb)
			BUGCHECK(-1); // !!
	}
#endif
	for (;;)
	{
		bdb->bdb_lru_chain = bcb->bcb_lru_chain;
		if (bcb->bcb_lru_chain.compare_exchange_strong(bdb->bdb_lru_chain, bdb))
			break;
	}
}


void requeueRecentlyUsed(BufferControl* bcb)
{
	BufferDesc* chain = NULL;

	// Let's pick up the LRU pending chain, if any

	for (;;)
	{
		chain = bcb->bcb_lru_chain;
		if (bcb->bcb_lru_chain.compare_exchange_strong(chain, NULL))
			break;
	}

	if (!chain)
		return;

	// Next, let's flip the order

	BufferDesc* reversed = NULL;
	BufferDesc* bdb;

	while ((bdb = chain) != NULL)
	{
		chain = bdb->bdb_lru_chain;
		bdb->bdb_lru_chain = reversed;
		reversed = bdb;
	}

	while ((bdb = reversed) != NULL)
	{
		reversed = bdb->bdb_lru_chain;
		QUE_DELETE(bdb->bdb_in_use);
		QUE_INSERT(bcb->bcb_in_use, bdb->bdb_in_use);

		bdb->bdb_lru_chain = NULL;
		bdb->bdb_flags &= ~BDB_lru_chained;
	}

	chain = bcb->bcb_lru_chain;
}


BufferControl* BufferControl::create(Database* dbb)
{
	MemoryPool* const pool = dbb->createPool();
	BufferControl* const bcb = FB_NEW_POOL(*pool) BufferControl(*pool, dbb->dbb_memory_stats);
	pool->setStatsGroup(bcb->bcb_memory_stats);
	return bcb;
}


void BufferControl::destroy(BufferControl* bcb)
{
	Database* const dbb = bcb->bcb_database;
	MemoryPool* const pool = bcb->bcb_bufferpool;
	MemoryStats temp_stats;
	pool->setStatsGroup(temp_stats);
	delete bcb;
	dbb->deletePool(pool);
}


bool BufferDesc::addRef(thread_db* tdbb, SyncType syncType, int wait)
{
	if (wait == 1)
		bdb_syncPage.lock(NULL, syncType, FB_FUNCTION);
	else if (!bdb_syncPage.lock(NULL, syncType, FB_FUNCTION, -wait * 1000))
		return false;

	++bdb_use_count;

	if (syncType == SYNC_EXCLUSIVE)
	{
		bdb_exclusive = tdbb;
		++bdb_writers;
	}

	tdbb->registerBdb(this);
	return true;
}


bool BufferDesc::addRefConditional(thread_db* tdbb, SyncType syncType)
{
	if (!bdb_syncPage.lockConditional(syncType, FB_FUNCTION))
		return false;

	++bdb_use_count;

	if (syncType == SYNC_EXCLUSIVE)
	{
		bdb_exclusive = tdbb;
		++bdb_writers;
	}

	tdbb->registerBdb(this);
	return true;
}


void BufferDesc::downgrade(SyncType syncType)
{
	// SH -> SH is no-op
	if (syncType == SYNC_SHARED && !bdb_writers)
		return;

	if (bdb_writers != 1)
		BUGCHECK(296);	// inconsistent latch downgrade call

	// EX -> EX is no-op
	if (syncType == SYNC_EXCLUSIVE)
		return;

	--bdb_writers;

	bdb_exclusive = NULL;

	bdb_syncPage.downgrade(syncType);
}


void BufferDesc::release(thread_db* tdbb, bool repost)
{
	//const SyncType oldState = bdb_syncPage.getState(); Unfinished logic here???

	fb_assert(!(bdb_flags & BDB_marked) || bdb_writers > 1);

	if (!tdbb->clearBdb(this))
		return;

	--bdb_use_count;

	if (bdb_writers)
	{
		if (--bdb_writers == 0)
			bdb_exclusive = NULL;

		bdb_syncPage.unlock(NULL, SYNC_EXCLUSIVE);
	}
	else
		bdb_syncPage.unlock(NULL, SYNC_SHARED);

	if (repost && !isLocked() && (bdb_ast_flags & BDB_blocking))
	{
		PAGE_LOCK_RE_POST(tdbb, bdb_bcb, bdb_lock);
	}
}


void BufferDesc::lockIO(thread_db* tdbb)
{
	bdb_syncIO.lock(NULL, SYNC_EXCLUSIVE, FB_FUNCTION);

	fb_assert(!bdb_io_locks && bdb_io != tdbb || bdb_io_locks && bdb_io == tdbb);

	bdb_io = tdbb;
	bdb_io->registerBdb(this);
	++bdb_io_locks;
	++bdb_use_count;
}


void BufferDesc::unLockIO(thread_db* tdbb)
{
	fb_assert(bdb_io && bdb_io == tdbb);
	fb_assert(bdb_io_locks > 0);

	if (!bdb_io->clearBdb(this))
		return;

	--bdb_use_count;

	if (--bdb_io_locks == 0)
		bdb_io = NULL;

	bdb_syncIO.unlock(NULL, SYNC_EXCLUSIVE);
}


namespace Jrd {

/// class BCBHashTable

void BCBHashTable::resize(ULONG count)
{
	const ULONG old_count = m_count;
	chain_type* const old_chains = m_chains;

	chain_type* new_chains = FB_NEW_POOL(m_pool) chain_type[count];
	m_count = count;
	m_chains = new_chains;

#ifndef HASH_USE_CDS_LIST
	// Initialize all new new_chains
	for (chain_type* que = new_chains; que < new_chains + count; que++)
		QUE_INIT(*que);
#endif

	if (!old_chains)
		return;

	const chain_type* const old_end = old_chains + old_count;

	// Move any active buffers from old hash table to new
	for (chain_type* old_tail = old_chains; old_tail < old_end; old_tail++)
	{
#ifndef HASH_USE_CDS_LIST
		while (QUE_NOT_EMPTY(*old_tail))
		{
			QUE que_inst = old_tail->que_forward;
			BufferDesc* bdb = BLOCK(que_inst, BufferDesc, bdb_que);
			QUE_DELETE(*que_inst);
			QUE mod_que = &new_chains[hash(bdb->bdb_page)];
			QUE_INSERT(*mod_que, *que_inst);
		}
#else
		while (!old_tail->empty())
		{
			auto n = old_tail->begin();
			old_tail->erase(n->first);				// bdb_page

			chain_type* new_chain = &m_chains[hash(n->first)];
			new_chain->insert(n->first, n->second);	// bdb_page, bdb
		}
#endif
	}

	delete[] old_chains;
}

void BCBHashTable::clear()
{
	if (!m_chains)
		return;

#ifdef HASH_USE_CDS_LIST
	const chain_type* const end = m_chains + m_count;
	for (chain_type* tail = m_chains; tail < end; tail++)
		tail->clear();
#endif

	delete[] m_chains;
	m_chains = nullptr;
	m_count = 0;
}

inline BufferDesc* BCBHashTable::find(const PageNumber& page) const
{
	auto& list = m_chains[hash(page)];

#ifndef HASH_USE_CDS_LIST
	QUE que_inst = list.que_forward;
	for (; que_inst != &list; que_inst = que_inst->que_forward)
	{
		BufferDesc* bdb = BLOCK(que_inst, BufferDesc, bdb_que);
		if (bdb->bdb_page == page)
			return bdb;
	}

#else // HASH_USE_CDS_LIST
	auto ptr = list.get(page);
	if (!ptr.empty())
	{
		fb_assert(ptr->second != nullptr);
#ifdef DEV_BUILD
		// Original libcds have no update(key, value), use this code with it,
		// see also comment in get_buffer()
		while (ptr->second == nullptr)
			cds::backoff::pause();
#endif
		if (ptr->second->bdb_page == page)
			return ptr->second;
	}
#endif

	return nullptr;
}

inline BufferDesc* BCBHashTable::emplace(BufferDesc* bdb, const PageNumber& page, bool remove)
{
#ifndef HASH_USE_CDS_LIST
	// bcb_syncObject should be locked in EX mode

	BufferDesc* bdb2 = find(page);
	if (!bdb2)
	{
		if (remove)
			QUE_DELETE(bdb->bdb_que);

		que& mod_que = m_chains[hash(page)];
		QUE_INSERT(mod_que, bdb->bdb_que);
	}
	return bdb2;
#else // HASH_USE_CDS_LIST

	BufferDesc* bdb2 = nullptr;
	BdbList& list = m_chains[hash(page)];

/*
	// Original libcds have no update(key, value), use this code with it

	auto ret = list.update(page, [bdb, &bdb2](bool bNew, BdbList::value_type& val)
		{
			if (bNew)
				val.second = bdb;
			else
				while (!(bdb2 = val.second))
					cds::backoff::pause();
		},
		true);
*/

	auto ret = list.update(page, [&bdb, &bdb2](bool bNew, BdbList::value_type& val)
		{
			// someone might have put a page buffer in the chain concurrently, so
			// we store it for the further investigation
			if (!bNew)
				bdb2 = val.second;
			else
				val.second = bdb;  // set the new value
		},
		true);
	fb_assert(ret.first);

	// if we have inserted the page buffer that we found (empty or oldest)
	if (bdb2 == nullptr)
	{
		fb_assert(ret.second);
#ifdef DEV_BUILD
		auto p1 = list.get(page);
		fb_assert(!p1.empty() && p1->first == page && p1->second == bdb);
#endif

		if (remove)
		{
			// remove the page buffer from old hash slot
			const PageNumber oldPage = bdb->bdb_page;
			BdbList& oldList = m_chains[hash(oldPage)];

#ifdef DEV_BUILD
			p1 = oldList.get(oldPage);
			fb_assert(!p1.empty() && p1->first == oldPage && p1->second == bdb);
#endif

			const bool ok = oldList.erase(oldPage);
			fb_assert(ok);

#ifdef DEV_BUILD
			p1 = oldList.get(oldPage);
			fb_assert(p1.empty() || p1->second != bdb);
#endif
		}

#ifdef DEV_BUILD
		p1 = list.get(page);
		fb_assert(!p1.empty() && p1->first == page && p1->second == bdb);
#endif
	}
	return bdb2;
#endif
}


void BCBHashTable::remove(BufferDesc* bdb)
{
#ifndef HASH_USE_CDS_LIST
	QUE_DELETE(bdb->bdb_que);
#else
	BdbList& list = m_chains[hash(bdb->bdb_page)];

#ifdef DEV_BUILD
	auto p = list.get(bdb->bdb_page);
	fb_assert(!p.empty() && p->first == bdb->bdb_page && p->second == bdb);
#endif

	list.erase(bdb->bdb_page);
#endif
}


}; // namespace Jrd


#ifdef HASH_USE_CDS_LIST

///	 class ListNodeAllocator<T>

class InitPool
{
public:
	explicit InitPool(MemoryPool&)
		: m_pool(InitCDS::createPool()),
		  m_stats(m_pool->getStatsGroup())
	{ }

	~InitPool()
	{
		// m_pool will be deleted by InitCDS dtor after cds termination
		// some memory could still be not freed until that moment

#ifdef DEBUG_CDS_MEMORY
		char str[256];
		snprintf(str, sizeof(str),
			"CCH list's common pool stats:\n"
			"  usage         = %llu\n"
			"  mapping       = %llu\n"
			"  max usage     = %llu\n"
			"  max mapping   = %llu\n"
			"\n",
			m_stats.getCurrentUsage(),
			m_stats.getCurrentMapping(),
			m_stats.getMaximumUsage(),
			m_stats.getMaximumMapping()
		);
		gds__log(str);
#endif
	}

	void* alloc(size_t size)
	{
		return m_pool->allocate(size ALLOC_ARGS);
	}

private:
	MemoryPool* m_pool;
	MemoryStats& m_stats;
};

static InitInstance<InitPool> initPool;


template <typename T>
T* ListNodeAllocator<T>::allocate(std::size_t n)
{
	return static_cast<T*>(initPool().alloc(n * sizeof(T)));
}

template <typename T>
void ListNodeAllocator<T>::deallocate(T* p, std::size_t /* n */)
{
	// It uses the correct pool stored within memory block itself
	MemoryPool::globalFree(p);
}

#endif // HASH_USE_CDS_LIST

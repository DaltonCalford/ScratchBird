/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		pag.cpp
 *	DESCRIPTION:	Page level ods manager
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
 *
 * Modified by: Patrick J. P. Griffin
 * Date: 11/29/2000
 * Problem:   Bug 116733 Too many generators corrupt database.
 *            DPM_gen_id was not calculating page and offset correctly.
 * Change:    Caculate pgc_gpg, number of generators per page,
 *            for use in DPM_gen_id.
 *
 * 2001.07.06 Sean Leyne - Code Cleanup, removed "#ifdef READONLY_DATABASE"
 *                         conditionals, as the engine now fully supports
 *                         readonly databases.
 *
 * 2001.08.07 Sean Leyne - Code Cleanup, removed "#ifdef READONLY_DATABASE"
 *                         conditionals, second attempt
 *
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "MAC" and "MAC_CP" defines
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "Apollo" port

 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete ports:
 *                          - EPSON, DELTA, IMP, NCR3000, M88K
 *                          - HP9000 s300 and Apollo
 *
 * 2002.10.27 Sean Leyne - Completed removal of obsolete "DG_X86" port
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "UNIXWARE" port
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "Ultrix" port
 * 2002.10.27 Sean Leyne - Code Cleanup, removed obsolete "Ultrix/MIPS" port
 *
 * 2002.10.28 Sean Leyne - Completed removal of obsolete "DGUX" port
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "MPEXL" port
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "DecOSF" port
 * 2002.10.28 Sean Leyne - Code cleanup, removed obsolete "SGI" port
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 */


#include "firebird.h"
#include <stdio.h>
#include <string.h>

#ifdef WIN_NT
#include <process.h>
#endif

#include "../common/config/config.h"
#include "../common/isc_proto.h"
#include "../common/utils_proto.h"
#include "../jrd/jrd.h"
#include "../jrd/pag.h"
#include "../jrd/ods.h"
#include "../jrd/os/pio.h"
#include "../common/os/path_utils.h"
#include "../common/gdsassert.h"
#include "../jrd/lck.h"
#include "../jrd/sdw.h"
#include "../jrd/cch.h"
#include "../jrd/nbak.h"
#include "../jrd/tra.h"
#include "../jrd/vio_debug.h"
#include "../jrd/cch_proto.h"
#include "../jrd/dpm_proto.h"
#include "../jrd/err_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/ods_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/os/pio_proto.h"
#include "../common/isc_f_proto.h"
#include "../jrd/TempSpace.h"
#include "../jrd/extds/ExtDS.h"
#include "../common/classes/DbImplementation.h"
#include "../jrd/CryptoManager.h"

using namespace Jrd;
using namespace Ods;
using namespace ScratchBird;

namespace
{
	const char* const SCRATCH = "fb_table_";
	const int MIN_EXTEND_BYTES = 128 * 1024;	// 128KB

	inline void ensureDbWritable(thread_db* tdbb)
	{
		const auto dbb = tdbb->getDatabase();

		if (dbb->readOnly())
			ERR_post(Arg::Gds(isc_read_only_database));
	}

	class HeaderClumplet
	{
	public:
		HeaderClumplet(header_page* header, USHORT type)
			: m_header(header), m_type(type)
		{}

		UCHAR* find(const UCHAR** clump_end = nullptr) const
		{
			UCHAR* p = m_header->hdr_data;
			UCHAR* q = nullptr;

			for (; *p != HDR_end; p += 2 + p[1])
			{
				if (*p == m_type)
					q = p;
			}

			if (clump_end)
				*clump_end = p;

			return q;
		}

		bool checkSpace(USHORT length) const
		{
			const auto freeSpace = m_header->hdr_page_size - m_header->hdr_end;
			if (freeSpace > 2 + length)
				return true;

			fb_assert(false);
			return false;
		}

		void add(USHORT length, const UCHAR* entry, bool first = false) const
		{
			UCHAR* p = (UCHAR*) m_header + m_header->hdr_end;
			fb_assert(*p == HDR_end);

			if (first)
			{
				p = m_header->hdr_data;
				memmove(p + length + 2, p, m_header->hdr_end - HDR_SIZE + 1);
			}

			fb_assert(m_type <= MAX_UCHAR);
			*p++ = static_cast<UCHAR>(m_type);

			fb_assert(length <= MAX_UCHAR);
			*p++ = static_cast<UCHAR>(length);

			if (length)
			{
				if (entry)
					memcpy(p, entry, length);
				else
					memset(p, 0, length);

				p += length;
			}

			if (!first)
				*p = HDR_end;

			m_header->hdr_end += length + 2;
		}

		void remove(UCHAR* entry, const UCHAR* end) const
		{
			fb_assert(*entry == m_type);

			const USHORT orgLen = entry[1] + 2;
			const UCHAR* const tail = entry + orgLen;
			const USHORT shift = end - tail + 1; // to preserve HDR_end
			memmove(entry, tail, shift);

			m_header->hdr_end -= orgLen;
		}

		bool remove() const
		{
			const UCHAR* clump_end;
			if (const auto entry_p = find(&clump_end))
			{
				remove(entry_p, clump_end);
				return true;
			}

			return false;
		}

	private:
		header_page* const m_header;
		const USHORT m_type;
	};

	//
	// Store a clump to header page (replace if it already exists)
	//

	void storeClump(thread_db* tdbb, USHORT type, USHORT len, const UCHAR* entry)
	{
		ensureDbWritable(tdbb);

		WIN window(HEADER_PAGE_NUMBER);
		pag* page = CCH_FETCH(tdbb, &window, LCK_write, pag_header);
		header_page* header = (header_page*) page;

		const HeaderClumplet clump(header, type);

		const UCHAR* clump_end;
		if (auto entry_p = clump.find(&clump_end))
		{
			// If the same size, overwrite it

			const USHORT orgLen = entry_p[1] + 2;

			if (orgLen - 2 == len)
			{
				entry_p += 2;

				if (len)
				{
					CCH_MARK_MUST_WRITE(tdbb, &window);
					memcpy(entry_p, entry, len);
				}

				CCH_RELEASE(tdbb, &window);
				return;
			}

			// Delete the entry

			// Page is marked must write because of precedence problems.  Later
			// on we may allocate a new page and set up a precedence relationship.
			// This may be the lower precedence page and so it cannot be dirty

			CCH_MARK_MUST_WRITE(tdbb, &window);
			clump.remove(entry_p, clump_end);
		}

		// Add the entry at the end of clumplet list

		if (!clump.checkSpace(len))
		{
			CCH_RELEASE(tdbb, &window);
			Arg::Gds(isc_hdr_overflow).raise();
		}

		CCH_MARK_MUST_WRITE(tdbb, &window);
		clump.add(len, entry);
		CCH_RELEASE(tdbb, &window);
	}

	ULONG ensureDiskSpace(thread_db* tdbb, WIN* pip_window, const PageNumber pageNum, ULONG pipUsed)
	{
		const auto dbb = tdbb->getDatabase();
		PageManager& pageMgr = dbb->dbb_page_manager;
		PageSpace* const pageSpace = pageMgr.findPageSpace(pageNum.getPageSpaceID());

		ULONG newUsed = pipUsed;
		const ULONG sequence = pageNum.getPageNum() / pageMgr.pagesPerPIP;
		const ULONG relative_bit = pageNum.getPageNum() - sequence * pageMgr.pagesPerPIP;

		BackupManager::StateReadGuard stateGuard(tdbb);
		const bool nbak_stalled = dbb->dbb_backup_manager->getState() == Ods::hdr_nbak_stalled;

		USHORT next_init_pages = 1;
		// ensure there are space on disk for faked page
		if (relative_bit + 1 > pipUsed)
		{
			fb_assert(relative_bit >= pipUsed);

			USHORT init_pages = 0;
			if (!nbak_stalled)
			{
				init_pages = 1;
				if (!(dbb->dbb_flags & DBB_no_reserve))
				{
					const int minExtendPages = MIN_EXTEND_BYTES / dbb->dbb_page_size;

					init_pages = sequence ? 64 : MIN(pipUsed / 16, 64);

					// don't touch pages belongs to the next PIP
					init_pages = MIN(init_pages, pageMgr.pagesPerPIP - pipUsed);

					if (init_pages < minExtendPages)
						init_pages = 1;
				}

				if (init_pages < relative_bit + 1 - pipUsed)
					init_pages = relative_bit + 1 - pipUsed;

				//init_pages = FB_ALIGN(init_pages, PAGES_IN_EXTENT);

				next_init_pages = init_pages;

				FbLocalStatus status;
				const ULONG start = sequence * pageMgr.pagesPerPIP + pipUsed;

				init_pages = PIO_init_data(tdbb, pageSpace->file, &status, start, init_pages);
			}

			if (init_pages)
			{
				newUsed += init_pages;
			}
			else
			{
				// PIO_init_data returns zero - perhaps it is not supported,
				// no space left on disk or IO error occurred. Try to write
				// one page and handle IO errors if any.
				WIN window(pageNum);
				CCH_fake(tdbb, &window, 1);
				CCH_must_write(tdbb, &window);
				try
				{
					CCH_RELEASE(tdbb, &window);
				}
				catch (const status_exception&)
				{
					// forget about this page as if we never tried to fake it
					CCH_forget_page(tdbb, &window);

					// normally all page buffers now released by CCH_unwind
					// only exception is when TDBB_no_cache_unwind flag is set
					if (tdbb->tdbb_flags & TDBB_no_cache_unwind)
						CCH_RELEASE(tdbb, pip_window);

					throw;
				}

				newUsed = relative_bit + 1;
			}
		}

		if (!(dbb->dbb_flags & DBB_no_reserve) && !nbak_stalled)
		{
			const ULONG initialized = sequence * pageMgr.pagesPerPIP + pipUsed;

			// At this point we ensure database has at least "initialized" pages
			// allocated. To avoid file growth by few pages when all this space
			// will be used, extend file up to initialized + next_init_pages now
			pageSpace->extend(tdbb, initialized + next_init_pages, false);
		}

		return newUsed;
	}

} // namespace


void PAG_add_header_entry(thread_db* tdbb, header_page* header,
						  USHORT type, USHORT len, const UCHAR* entry)
{
/***********************************************
 *
 *	P A G _ a d d _ h e a d e r _ e n t r y
 *
 ***********************************************
 *
 * Functional description
 *	Add an entry to header page.
 *	This will be used mainly for the shadow header page and adding
 *	secondary files.
 *
 **************************************/
	SET_TDBB(tdbb);
	ensureDbWritable(tdbb);

	const HeaderClumplet clump(header, type);

	if (!clump.find())
	{
		if (!clump.checkSpace(len))
			BUGCHECK(251);

		clump.add(len, entry);
	}
}


bool PAG_replace_entry_first(thread_db* tdbb, header_page* header,
							 USHORT type, USHORT len, const UCHAR* entry)
{
/***********************************************
 *
 *	P A G _ r e p l a c e _ e n t r y _ f i r s t
 *
 ***********************************************
 *
 * Functional description
 *	Replace an entry in the header page so it will become first entry
 *	This will be used mainly for the clumplets used for backup purposes
 *  because they are needed to be read without page lock
 *	RETURNS
 *		true - modified page
 *		false - nothing done
 *
 **************************************/
	SET_TDBB(tdbb);
	ensureDbWritable(tdbb);

	const HeaderClumplet clump(header, type);

	if (clump.remove())
	{
		// If we were asked just to remove item, we finished

		if (!entry)
			return false;
	}

	if (!clump.checkSpace(len))
		BUGCHECK(251);

	clump.add(len, entry, true);
	return true;
}


PAG PAG_allocate_pages(thread_db* tdbb, WIN* window, unsigned cntAlloc, bool aligned)
{
/**************************************
 *
 *	P A G _ a l l o c a t e _ p a g e s
 *
 **************************************
 *
 * Functional description
 *	Allocate number of consecutive pages and fake a read with a write lock for
 *  the first allocated page. If aligned is true, ensure first allocated page
 *  is at extent boundary.
 *	This is the universal sequence when allocating pages.
 *
 **************************************/
	SET_TDBB(tdbb);
	const auto dbb = tdbb->getDatabase();

	PageManager& pageMgr = dbb->dbb_page_manager;
	PageSpace* const pageSpace = pageMgr.findPageSpace(window->win_page.getPageSpaceID());
	fb_assert(pageSpace);

	PAG new_page = NULL;

	// Find an allocation page with something on it

	ULONG sequence = (cntAlloc >= PAGES_IN_EXTENT ? pageSpace->pipWithExtent : pageSpace->pipHighWater);
	for (unsigned toAlloc = cntAlloc; toAlloc; sequence++)
	{
		WIN pip_window(pageSpace->pageSpaceID,
			(sequence == 0) ? pageSpace->pipFirst : sequence * dbb->dbb_page_manager.pagesPerPIP - 1);

		page_inv_page* pip_page = (page_inv_page*) CCH_FETCH(tdbb, &pip_window, LCK_write, pag_pages);

		ULONG firstBit = MAX_ULONG, lastBit = MAX_ULONG;

		ULONG pipUsed = pip_page->pip_used;
		ULONG pipMin = (cntAlloc >= PAGES_IN_EXTENT ? pip_page->pip_min : dbb->dbb_page_manager.pagesPerPIP);
		ULONG pipExtent = MAX_ULONG;

		UCHAR* bytes = 0;
		const UCHAR* end = (UCHAR*) pip_page + dbb->dbb_page_size;

		// Some pages (such as SCN or new PIP pages) could be allocated before requested pages.
		// Remember its numbers to later clear corresponding bits from current PIP.
		HalfStaticArray<ULONG, 8> extraPages;

		const ULONG freeBit = (cntAlloc >= PAGES_IN_EXTENT) ? pip_page->pip_extent : pip_page->pip_min;
		for (bytes = &pip_page->pip_bits[freeBit >> 3]; bytes < end; bytes++)
		{
			if (*bytes == 0)
			{
				toAlloc = cntAlloc;
				continue;
			}

			// 'byte' is not zero, so it describes at least one free page.
			UCHAR bit = 1;
			for (SLONG i = 0; i < 8; i++, bit <<= 1)
			{
				if (!(bit & *bytes))
				{
					toAlloc = cntAlloc;
					continue;
				}

				lastBit = ((bytes - pip_page->pip_bits) << 3) + i;

				const ULONG pageNum = lastBit + sequence * pageMgr.pagesPerPIP;

				// Check if we need new SCN page.
				// SCN pages allocated at every pagesPerSCN pages in database.
				const bool newSCN = (!pageSpace->isTemporary() && (pageNum % pageMgr.pagesPerSCN) == 0);

				// Also check for new PIP page.
				const bool newPIP = (lastBit == pageMgr.pagesPerPIP - 1);
				fb_assert(!(newSCN && newPIP));

				if (newSCN || newPIP)
				{
					window->win_page = pageNum;

					if (lastBit + 1 > pipUsed)
						pipUsed = ensureDiskSpace(tdbb, &pip_window, window->win_page, pipUsed);

					pag* new_page = CCH_fake(tdbb, window, 1);

					if (newSCN)
					{
						scns_page* new_scns_page = (scns_page*) new_page;
						new_scns_page->scn_header.pag_type = pag_scns;
						new_scns_page->scn_sequence = pageNum / pageMgr.pagesPerSCN;
					}

					if (newPIP)
					{
						page_inv_page* new_pip_page = (page_inv_page*) new_page;
						new_pip_page->pip_header.pag_type = pag_pages;
						const UCHAR* end = (UCHAR*) new_pip_page + dbb->dbb_page_size;
						memset(new_pip_page->pip_bits, 0xff, end - new_pip_page->pip_bits);
					}

					CCH_must_write(tdbb, window);
					CCH_RELEASE(tdbb, window);

					extraPages.add(lastBit);

					if (pipMin == lastBit)
						pipMin++;

					toAlloc = cntAlloc;

					if (newSCN)
						continue;

					if (newPIP)
						break;		// we just allocated last bit at current PIP - start again at new PIP
				}

				if (pipMin > lastBit)
					pipMin = lastBit;

				// assume PAGES_IN_EXTENT == 8
				if (i == 7 && *bytes == 0xFF && pipExtent > lastBit - 7)
					pipExtent = lastBit - 7;

				if (toAlloc == cntAlloc)
				{
					// found first page to allocate, check if it aligned at extent boundary
					if (aligned && ((pageNum % PAGES_IN_EXTENT) != 0) )
						continue;

					firstBit = lastBit;
				}

				toAlloc--;
				if (!toAlloc)
					break;
			}

			if (!toAlloc)
				break;
		}

		if (!toAlloc)
		{
			fb_assert(lastBit - firstBit + 1 == cntAlloc);

			if (lastBit + 1 > pipUsed)
			{
				pipUsed = ensureDiskSpace(tdbb, &pip_window,
					PageNumber(pageSpace->pageSpaceID, lastBit + sequence * pageMgr.pagesPerPIP),
					pipUsed);
			}

			CCH_MARK(tdbb, &pip_window);

			for (ULONG i = firstBit; i <= lastBit; i++)
			{
				UCHAR* byte = &pip_page->pip_bits[i / 8];
				int mask = 1 << (i % 8);
				*byte &= ~mask;

#ifdef VIO_DEBUG
				VIO_trace(DEBUG_WRITES_INFO,
					"PAG_allocate:  allocated page %" SLONGFORMAT"\n",
					i + sequence * pageMgr.pagesPerPIP);
#endif
			}

			pipMin = MIN(pipMin, firstBit);
			if (pipMin == firstBit)
				pipMin = lastBit + 1;

			if (pipExtent == MAX_ULONG)
				pipExtent = pip_page->pip_extent;

			// If we found free extent on the PIP page and allocated some pages of it,
			// set free extent mark after just allocated pages
			// assume PAGES_IN_EXTENT == 8 (i.e. one byte of bits at PIP)
			const ULONG extentByte = pipExtent / PAGES_IN_EXTENT;
			if (extentByte >= firstBit / PAGES_IN_EXTENT &&
				extentByte <= lastBit / PAGES_IN_EXTENT)
			{
				pipExtent = FB_ALIGN(lastBit + 1, PAGES_IN_EXTENT);

				const ULONG firstPage = lastBit + sequence * pageMgr.pagesPerPIP;
				const ULONG lastPage  = pipExtent + sequence * pageMgr.pagesPerPIP;
				if (firstPage / pageMgr.pagesPerSCN != lastPage / pageMgr.pagesPerSCN)
				{
					const ULONG scnBit = pipExtent - lastPage % pageMgr.pagesPerSCN;
					if (pip_page->pip_bits[scnBit / 8] & (1 << (scnBit % 8)))
						pipExtent -= PAGES_IN_EXTENT;
				}

				if (pipExtent == pageMgr.pagesPerPIP)
				{
					const UCHAR lastByte = pip_page->pip_bits[pageMgr.bytesBitPIP - 1];
					if (lastByte & 0x80)
						pipExtent--;
				}
			}
		}
		else
		{
			if (pipExtent == MAX_ULONG)
				pipExtent = pageMgr.pagesPerPIP;

			if (cntAlloc == 1)
				pipMin = pageMgr.pagesPerPIP;
		}

		if (pipMin >= pageMgr.pagesPerPIP)
			pageSpace->pipHighWater.compareExchange(sequence, sequence + 1);

		if (pipExtent >= pageMgr.pagesPerPIP)
			pageSpace->pipWithExtent.compareExchange(sequence, sequence + 1);

		if (pipMin != pip_page->pip_min || pipExtent != pip_page->pip_extent ||
			pipUsed != pip_page->pip_used || extraPages.getCount())
		{
			if (toAlloc)
				CCH_MARK(tdbb, &pip_window);

			pip_page->pip_min = pipMin;
			pip_page->pip_extent = pipExtent;
			pip_page->pip_used = pipUsed;

			for (const ULONG *bit = extraPages.begin(); bit < extraPages.end(); bit++)
			{
				UCHAR* byte = &pip_page->pip_bits[*bit / 8];
				const int mask = 1 << (*bit % 8);
				*byte &= ~mask;

#ifdef VIO_DEBUG
				VIO_trace(DEBUG_WRITES_INFO,
					"PAG_allocate:  allocated page %" SLONGFORMAT"\n",
					bit + sequence * pageMgr.pagesPerPIP);
#endif
			}

			if (extraPages.getCount())
				CCH_must_write(tdbb, &pip_window);
		}

		CCH_RELEASE(tdbb, &pip_window);

		if (!toAlloc)
		{
			window->win_page = firstBit + sequence * pageMgr.pagesPerPIP;
			new_page = CCH_fake(tdbb, window, LCK_WAIT);
			fb_assert(new_page);

			CCH_precedence(tdbb, window, pip_window.win_page);
		}
	}

	return new_page;
}


AttNumber PAG_attachment_id(thread_db* tdbb)
{
/******************************************
 *
 *	P A G _ a t t a c h m e n t _ i d
 *
 ******************************************
 *
 * Functional description
 *	Get attachment id.  If don't have one, get one.  As a side
 *	effect, get a lock on it as well.
 *
 ******************************************/
	SET_TDBB(tdbb);
	const auto dbb = tdbb->getDatabase();
	const auto attachment = tdbb->getAttachment();

	// If we've been here before just return the id

	if (attachment->att_id_lock)
		return attachment->att_attachment_id;

	// Get new attachment id

	if (dbb->readOnly())
		attachment->att_attachment_id = dbb->generateAttachmentId();
	else
	{
		WIN window(HEADER_PAGE_NUMBER);
		header_page* header = (header_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_header);

		CCH_MARK(tdbb, &window);

		attachment->att_attachment_id = ++header->hdr_attachment_id;
		dbb->assignLatestAttachmentId(attachment->att_attachment_id);

		CCH_RELEASE(tdbb, &window);
	}

	attachment->initLocks(tdbb);

	return attachment->att_attachment_id;
}


bool PAG_delete_clump_entry(thread_db* tdbb, USHORT type)
{
/***********************************************
 *
 *	P A G _ d e l e t e _ c l u m p _ e n t r y
 *
 ***********************************************
 *
 * Functional description
 *	Gets rid on the entry 'type' from page.
 *
 **************************************/
	SET_TDBB(tdbb);
	ensureDbWritable(tdbb);

	WIN window(HEADER_PAGE_NUMBER);
	pag* page = CCH_FETCH(tdbb, &window, LCK_write, pag_header);
	header_page* header = (header_page*) page;

	const HeaderClumplet clump(header, type);

	const UCHAR* clump_end;
	if (const auto entry_p = clump.find(&clump_end))
	{
		CCH_MARK(tdbb, &window);
		clump.remove(entry_p, clump_end);
		CCH_RELEASE(tdbb, &window);

		return true;
	}

	CCH_RELEASE(tdbb, &window);
	return false;
}


void PAG_format_header(thread_db* tdbb)
{
/**************************************
 *
 *	 P A G _ f o r m a t _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Create the header page for a new file.
 *
 **************************************/
	SET_TDBB(tdbb);
	const auto dbb = tdbb->getDatabase();

	// Initialize header page

	WIN window(HEADER_PAGE_NUMBER);
	header_page* header = (header_page*) CCH_fake(tdbb, &window, 1);
	header->hdr_header.pag_type = pag_header;
	header->hdr_header.pag_scn = 0;
	Guid::generate().copyTo(header->hdr_guid);
	*(ISC_TIMESTAMP*) header->hdr_creation_date = TimeZoneUtil::getCurrentGmtTimeStamp().utc_timestamp;
	// should we include milliseconds or not?
	//TimeStamp::round_time(header->hdr_creation_date->timestamp_time, 0);
	header->hdr_page_size = dbb->dbb_page_size;
	header->hdr_ods_version = ODS_VERSION | ODS_FIREBIRD_FLAG;
	DbImplementation::current.store(header);
	header->hdr_ods_minor = ODS_CURRENT;
	header->hdr_oldest_transaction = 1;
	header->hdr_end = HDR_SIZE;
	header->hdr_data[0] = HDR_end;

	if (dbb->dbb_flags & DBB_DB_SQL_dialect_3)
		header->hdr_flags |= hdr_SQL_dialect_3;

	if (dbb->dbb_flags & DBB_force_write)
		header->hdr_flags |= hdr_force_write;

	dbb->dbb_ods_version = header->hdr_ods_version & ~ODS_FIREBIRD_FLAG;
	dbb->dbb_minor_version = header->hdr_ods_minor;

	dbb->dbb_guid.assign(header->hdr_guid);

	CCH_RELEASE(tdbb, &window);
}


void PAG_format_pip(thread_db* tdbb, PageSpace& pageSpace)
{
/**************************************
 *
 *	 P A G _ f o r m a t _ p i p
 *
 **************************************
 *
 * Functional description
 *	Create a page inventory page to
 *	complete the formatting of a new file
 *	into a rudimentary database.
 *
 **************************************/
	SET_TDBB(tdbb);
	const auto dbb = tdbb->getDatabase();

	// Initialize first SCN's Page
	pageSpace.scnFirst = 0;
	if (!pageSpace.isTemporary())
	{
		pageSpace.scnFirst = FIRST_SCN_PAGE;

		WIN window(pageSpace.pageSpaceID, pageSpace.scnFirst);
		scns_page* page = (scns_page*) CCH_fake(tdbb, &window, 1);

		page->scn_header.pag_type = pag_scns;
		page->scn_sequence = 0;

		CCH_RELEASE(tdbb, &window);
	}

	// Initialize Page Inventory Page
	{
		pageSpace.pipFirst = FIRST_PIP_PAGE;

		WIN window(pageSpace.pageSpaceID, pageSpace.pipFirst);
		page_inv_page* pages = (page_inv_page*) CCH_fake(tdbb, &window, 1);

		pages->pip_header.pag_type = pag_pages;
		pages->pip_used = (pageSpace.scnFirst ? pageSpace.scnFirst : pageSpace.pipFirst) + 1;
		pages->pip_min = pages->pip_used;
		int count = dbb->dbb_page_size - static_cast<int>(offsetof(page_inv_page, pip_bits[0]));

		memset(pages->pip_bits, 0xFF, count);

		pages->pip_bits[0] &= ~(1 | 2);
		if (pageSpace.scnFirst)
			pages->pip_bits[0] &= ~(1 << pageSpace.scnFirst);

		CCH_RELEASE(tdbb, &window);
	}
}


bool PAG_get_clump(thread_db* tdbb, USHORT type, USHORT* inout_len, UCHAR* entry)
{
/***********************************************
 *
 *	P A G _ g e t _ c l u m p
 *
 ***********************************************
 *
 * Functional description
 *	Find 'type' clump
 *		true  - Found it
 *		false - Not present
 *	RETURNS
 *		value of clump in entry
 *		length in inout_len  <-> input and output value to avoid B.O.
 *
 **************************************/
	SET_TDBB(tdbb);

	WIN window(HEADER_PAGE_NUMBER);
	pag* page = CCH_FETCH(tdbb, &window, LCK_read, pag_header);
	header_page* header = (header_page*) page;

	const HeaderClumplet clump(header, type);

	if (auto entry_p = clump.find())
	{
		USHORT old_len = *inout_len;
		*inout_len = entry_p[1];
		entry_p += 2;

		if (*inout_len)
		{
			// Avoid the B.O. but inform the caller the buffer is bigger
			if (*inout_len < old_len)
				old_len = *inout_len;
			memcpy(entry, entry_p, old_len);
		}

		CCH_RELEASE(tdbb, &window);
		return true;
	}

	CCH_RELEASE(tdbb, &window);
	*inout_len = 0;
	return false;
}


void PAG_header(thread_db* tdbb, bool info, const TriState newForceWrite)
{
/**************************************
 *
 *	P A G _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *	Checkout database header page.
 *  Done through the page cache.
 *
 **************************************/
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	const auto attachment = tdbb->getAttachment();
	fb_assert(attachment);

	WIN window(HEADER_PAGE_NUMBER);
	pag* page = CCH_FETCH(tdbb, &window, LCK_read, pag_header);
	header_page* header = (header_page*) page;

	try {

	const TraNumber next_transaction = header->hdr_next_transaction;
	const TraNumber oldest_transaction = header->hdr_oldest_transaction;
	const TraNumber oldest_active = header->hdr_oldest_active;
	const TraNumber oldest_snapshot = header->hdr_oldest_snapshot;

	if (next_transaction)
	{
		if (oldest_active > next_transaction)
			BUGCHECK(266);		// next transaction older than oldest active

		if (oldest_transaction > next_transaction)
			BUGCHECK(267);		// next transaction older than oldest transaction
	}

	if (header->hdr_flags & hdr_SQL_dialect_3)
		dbb->dbb_flags |= DBB_DB_SQL_dialect_3;

	jrd_rel* relation = MET_relation(tdbb, 0);
	RelationPages* relPages = relation->getBasePages();
	if (!relPages->rel_pages)
	{
		// NS: There's no need to reassign first page for RDB$PAGES relation since
		// current code cannot change its location after database creation.
		vcl* vector = vcl::newVector(*relation->rel_pool, 1);
		relPages->rel_pages = vector;
		(*vector)[0] = header->hdr_PAGES;
	}

	dbb->dbb_next_transaction = next_transaction;

	if (!info || dbb->dbb_oldest_transaction < oldest_transaction)
		dbb->dbb_oldest_transaction = oldest_transaction;

	if (!info || dbb->dbb_oldest_active < oldest_active)
		dbb->dbb_oldest_active = oldest_active;

	if (!info || dbb->dbb_oldest_snapshot < oldest_snapshot)
		dbb->dbb_oldest_snapshot = oldest_snapshot;

	dbb->dbb_attachment_id = header->hdr_attachment_id;
	dbb->dbb_creation_date.utc_timestamp = *(ISC_TIMESTAMP*) header->hdr_creation_date;
	dbb->dbb_creation_date.time_zone = TimeZoneUtil::GMT_ZONE;

	const bool readOnly = header->hdr_flags & hdr_read_only;

	if (readOnly)
	{
		// If Header Page flag says the database is ReadOnly, gladly accept it.
		dbb->dbb_flags &= ~DBB_being_opened_read_only;
		dbb->dbb_flags |= DBB_read_only;
	}

	// If hdr_read_only is not set...
	if (!readOnly && (dbb->dbb_flags & DBB_being_opened_read_only))
	{
		// Looks like the Header page says, it is NOT ReadOnly!! But the database
		// file system permission gives only ReadOnly access. Punt out with
		// isc_no_priv error (no privileges)
		ERR_post(Arg::Gds(isc_no_priv) << Arg::Str("read-write") <<
										  Arg::Str("database") <<
										  Arg::Str(attachment->att_filename));
	}

	// Determine the actual FW mode to be used. Use the setting stored on the header page
	// unless something different is explicitly specified in DPB.
	const bool currentForceWrite = (header->hdr_flags & hdr_force_write) != 0;
	const bool forceWrite = newForceWrite.valueOr(currentForceWrite);

	// Adjust the flag inside the database block
	if (forceWrite)
		dbb->dbb_flags |= DBB_force_write;
	else
		dbb->dbb_flags &= ~DBB_force_write;

	// Ensure the file-level FW mode matches the actual FW mode in the database
	const auto pageSpace = dbb->dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
	PIO_force_write(pageSpace->file, forceWrite && !readOnly);

	if (dbb->dbb_backup_manager->getState() != Ods::hdr_nbak_normal)
		dbb->dbb_backup_manager->setForcedWrites(forceWrite);

	if (header->hdr_flags & hdr_no_reserve)
		dbb->dbb_flags |= DBB_no_reserve;

	const auto shutMode = (shut_mode_t) header->hdr_shutdown_mode;
	dbb->dbb_shutdown_mode.store(shutMode, std::memory_order_relaxed);

	const auto replicaMode = (ReplicaMode) header->hdr_replica_mode;
	dbb->dbb_replica_mode.store(replicaMode, std::memory_order_relaxed);

	dbb->dbb_guid.assign(header->hdr_guid);

	// If database in backup lock state...
	if (!info && dbb->dbb_backup_manager->getState() != Ods::hdr_nbak_normal)
	{
		// refetch some data from the header, because it could be changed in the delta file
		// (as initially PAG_init2 reads the header from the main file and these values
		// may be outdated there)
		for (const UCHAR* p = header->hdr_data; *p != HDR_end; p += 2u + p[1])
		{
			switch (*p)
			{
			case HDR_sweep_interval:
				fb_assert(p[1] == sizeof(SLONG));
				memcpy(&dbb->dbb_sweep_interval, p + 2, sizeof(SLONG));
				break;

			case HDR_repl_seq:
				fb_assert(p[1] == sizeof(FB_UINT64));
				memcpy(&dbb->dbb_repl_sequence, p + 2, sizeof(FB_UINT64));
				break;

			}
		}
	}

	}	// try
	catch (const Exception&)
	{
		CCH_RELEASE(tdbb, &window);
		throw;
	}

	CCH_RELEASE(tdbb, &window);
}


void PAG_header_init(thread_db* tdbb)
{
/**************************************
 *
 *	P A G _ h e a d e r _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Checkout the core part of the database header page.
 *  It includes the fields required to setup the I/O layer:
 *    ODS version, page size, page buffers.
 *  Done using a physical page read.
 *
 **************************************/
	SET_TDBB(tdbb);
	const auto dbb = tdbb->getDatabase();

	const auto attachment = tdbb->getAttachment();
	fb_assert(attachment);

	// Allocate a spare buffer which is large enough,
	// and set up to release it in case of error; note
	// that dbb_page_size has not been set yet, so we
	// can't depend on this.
	//
	// Make sure that buffer is aligned on a page boundary
	// and unit of transfer is a multiple of physical disk
	// sector for raw disk access.

	const ULONG ioBlockSize = dbb->getIOBlockSize();
	const ULONG headerSize = MAX(RAW_HEADER_SIZE, ioBlockSize);

	HalfStaticArray<UCHAR, RAW_HEADER_SIZE + PAGE_ALIGNMENT> temp;
	UCHAR* const temp_page = temp.getAlignedBuffer(headerSize, ioBlockSize);

	if (!PIO_header(tdbb, temp_page, headerSize))
		ERR_post(Arg::Gds(isc_bad_db_format) << Arg::Str(attachment->att_filename));

	const auto header = (header_page*) temp_page;

	if (header->hdr_header.pag_type != pag_header || header->hdr_header.pag_pageno != HEADER_PAGE)
		ERR_post(Arg::Gds(isc_bad_db_format) << Arg::Str(attachment->att_filename));

	if (header->hdr_page_size < PAGE_SIZE_BASE || header->hdr_page_size % PAGE_SIZE_BASE != 0)
		ERR_post(Arg::Gds(isc_bad_db_format) << Arg::Str(attachment->att_filename));

	const USHORT ods_version = header->hdr_ods_version & ~ODS_FIREBIRD_FLAG;

	if (!Ods::isSupported(header))
	{
		ERR_post(Arg::Gds(isc_wrong_ods) << Arg::Str(attachment->att_filename) <<
											Arg::Num(ods_version) <<
											Arg::Num(header->hdr_ods_minor) <<
											Arg::Num(ODS_VERSION) <<
											Arg::Num(ODS_CURRENT));
	}

	// Note that if this check is turned on, it should be recoded in order that
	// the Intel platforms can share databases.  At present (Feb 95) it is possible
	// to share databases between Windows and NT, but not with NetWare.  Sharing
	// databases with OS/2 is unknown and needs to be investigated.  The CLASS was
	// initially 8 for all Intel platforms, but was changed after 4.0 was released
	// in order to allow differentiation between databases created on various
	// platforms.  This should allow us in future to identify where databases were
	// created.  Even when we get to the stage where databases created on PC platforms
	// are sharable between all platforms, it would be useful to identify where they
	// were created for debugging purposes.  - Deej 2/6/95
	//
	// Re-enable and recode the check to avoid BUGCHECK messages when database
	// is accessed with engine built for another architecture. - Nickolay 9-Feb-2005

	if (!DbImplementation(header).compatible(DbImplementation::current))
		ERR_post(Arg::Gds(isc_bad_db_format) << Arg::Str(attachment->att_filename));

	if (header->hdr_page_size < MIN_PAGE_SIZE || header->hdr_page_size > MAX_PAGE_SIZE)
		ERR_post(Arg::Gds(isc_bad_db_format) << Arg::Str(attachment->att_filename));

	dbb->dbb_ods_version = ods_version;
	dbb->dbb_minor_version = header->hdr_ods_minor;

	dbb->dbb_page_size = header->hdr_page_size;
	dbb->dbb_page_buffers = header->hdr_page_buffers;
}


void PAG_init(thread_db* tdbb)
{
/**************************************
 *
 *	P A G _ i n i t
 *
 **************************************
 *
 * Functional description
 *	Initialize stuff for page handling.
 *
 **************************************/
	SET_TDBB(tdbb);
	const auto dbb = tdbb->getDatabase();

	PageManager& pageMgr = dbb->dbb_page_manager;
	PageSpace* const pageSpace = pageMgr.findPageSpace(DB_PAGE_SPACE);
	fb_assert(pageSpace);

	pageMgr.bytesBitPIP = Ods::bytesBitPIP(dbb->dbb_page_size);
	pageMgr.pagesPerPIP = Ods::pagesPerPIP(dbb->dbb_page_size);
	pageMgr.pagesPerSCN = Ods::pagesPerSCN(dbb->dbb_page_size);
	pageSpace->pipFirst = FIRST_PIP_PAGE;
	pageSpace->scnFirst = FIRST_SCN_PAGE;

	pageMgr.transPerTIP = Ods::transPerTIP(dbb->dbb_page_size);

	// dbb_ods_version can be 0 when a new database is being created
	fb_assert((dbb->dbb_ods_version == 0) || (dbb->dbb_ods_version >= ODS_VERSION12));
	pageMgr.gensPerPage = Ods::gensPerPage(dbb->dbb_page_size);

	dbb->dbb_dp_per_pp = Ods::dataPagesPerPP(dbb->dbb_page_size);
	dbb->dbb_max_records = Ods::maxRecsPerDP(dbb->dbb_page_size);
	dbb->dbb_max_idx = Ods::maxIndices(dbb->dbb_page_size);

	// Compute prefetch constants from database page size and maximum prefetch
	// transfer size. Double pages per prefetch request so that cache reader
	// can overlap prefetch I/O with database computation over previously prefetched pages.
#ifdef SUPERSERVER_V2
	dbb->dbb_prefetch_sequence = PREFETCH_MAX_TRANSFER / dbb->dbb_page_size;
	dbb->dbb_prefetch_pages = dbb->dbb_prefetch_sequence * 2;
#endif
}


void PAG_init2(thread_db* tdbb)
{
/**************************************
 *
 *	P A G _ i n i t 2
 *
 **************************************
 *
 * Functional description
 *	Read and apply the database header options.
 *
 **************************************/
	SET_TDBB(tdbb);
	const auto dbb = tdbb->getDatabase();

	WIN window(HEADER_PAGE_NUMBER);
	const auto header = (header_page*) CCH_FETCH(tdbb, &window, LCK_read, pag_header);

	for (const UCHAR* p = header->hdr_data; *p != HDR_end; p += 2 + p[1])
	{
		switch (*p)
		{
		case HDR_sweep_interval:
			fb_assert(p[1] == sizeof(SLONG));
			memcpy(&dbb->dbb_sweep_interval, p + 2, sizeof(SLONG));
			break;

		case HDR_repl_seq:
			fb_assert(p[1] == sizeof(FB_UINT64));
			memcpy(&dbb->dbb_repl_sequence, p + 2, sizeof(FB_UINT64));
			break;

		default:
			break;
		}
	}

	CCH_RELEASE(tdbb, &window);
}


SLONG PAG_last_page(thread_db* tdbb)
{
/**************************************
 *
 *	P A G _ l a s t _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Compute the highest page allocated.  This is called by the
 *	shadow stuff to dump a database.
 *
 **************************************/
	SET_TDBB(tdbb);
	const auto dbb = tdbb->getDatabase();

	return PageSpace::lastUsedPage(dbb);
}


void PAG_release_page(thread_db* tdbb, const PageNumber& number, const PageNumber& prior_page)
{
/**************************************
 *
 *	P A G _ r e l e a s e _ p a g e
 *
 **************************************
 *
 * Functional description
 *	Release a page to the free page page.
 *
 **************************************/

	fb_assert(number.getPageSpaceID() == prior_page.getPageSpaceID() ||
			  prior_page == ZERO_PAGE_NUMBER);

	const ULONG pgNum = number.getPageNum();
	PAG_release_pages(tdbb, number.getPageSpaceID(), 1, &pgNum, prior_page.getPageNum());
}


void PAG_release_pages(thread_db* tdbb, USHORT pageSpaceID, int cntRelease,
		const ULONG* pgNums, const ULONG prior_page)
{
/**************************************
 *
 *	P A G _ r e l e a s e _ p a g e s
 *
 **************************************
 *
 * Functional description
 *	Release a few pages to the free page page.
 *
 **************************************/
	SET_TDBB(tdbb);
	const auto dbb = tdbb->getDatabase();

	PageManager& pageMgr = dbb->dbb_page_manager;
	PageSpace* const pageSpace = pageMgr.findPageSpace(pageSpaceID);
	fb_assert(pageSpace);

	WIN pip_window(pageSpaceID, -1);
	page_inv_page* pages = NULL;
	ULONG sequence = 0;

#ifdef VIO_DEBUG
	string dbg = "PAG_release_pages:  about to release pages: ";
#endif

	for (int i = 0; i < cntRelease; i++)
	{
#ifdef VIO_DEBUG
		if (i > 0)
			dbg.append(", ");

		char num[16];
		_ltoa_s(pgNums[i], num, sizeof(num), 10);
		dbg.append(num);
#endif

		const ULONG seq = pgNums[i] / pageMgr.pagesPerPIP;

		if (!pages || seq != sequence)
		{
			if (pages)
			{
				pageSpace->pipHighWater.exchangeLower(sequence);
				if (pages->pip_extent < pageMgr.pagesPerPIP)
					pageSpace->pipWithExtent.exchangeLower(sequence);

				CCH_RELEASE(tdbb, &pip_window);
			}

			sequence = seq;
			pip_window.win_page = (sequence == 0) ?
				pageSpace->pipFirst : sequence * pageMgr.pagesPerPIP - 1;

			pages = (page_inv_page*) CCH_FETCH(tdbb, &pip_window, LCK_write, pag_pages);
			CCH_precedence(tdbb, &pip_window, prior_page);
			CCH_MARK(tdbb, &pip_window);
		}

		const ULONG relative_bit = pgNums[i] % pageMgr.pagesPerPIP;
		UCHAR* byte = &pages->pip_bits[relative_bit >> 3];
		*byte |= 1 << (relative_bit & 7);
		if (*byte == 0xFF)  // assume PAGES_IN_EXTENT == 8
		{
			pages->pip_extent = MIN(pages->pip_extent, relative_bit & ~0x07);
		}
		pages->pip_min = MIN(pages->pip_min, relative_bit);
	}

#ifdef VIO_DEBUG
	VIO_trace(DEBUG_WRITES_INFO, "%s\n", dbg.c_str());
#endif

	pageSpace->pipHighWater.exchangeLower(sequence);

	if (pages->pip_extent < pageMgr.pagesPerPIP)
		pageSpace->pipWithExtent.exchangeLower(sequence);

	if (pageSpace->isTemporary())
	{
		for (int i = 0; i < cntRelease; i++)
			CCH_clean_page(tdbb, PageNumber(pageSpaceID, pgNums[i]));
	}

	CCH_RELEASE(tdbb, &pip_window);
}


void PAG_set_db_guid(thread_db* tdbb, const Guid& guid)
{
/**************************************
 *
 *	P A G _ s e t _ d b _ g u i d
 *
 **************************************
 *
 * Functional description
 *	Set sweep interval.
 *
 **************************************/
 	SET_TDBB(tdbb);
	ensureDbWritable(tdbb);

	WIN window(HEADER_PAGE_NUMBER);
	header_page* header = (header_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_header);
	CCH_MARK_MUST_WRITE(tdbb, &window);

	const auto dbb = tdbb->getDatabase();

	guid.copyTo(header->hdr_guid);
	dbb->dbb_guid = guid;

	CCH_RELEASE(tdbb, &window);
}


void PAG_set_force_write(thread_db* tdbb, bool flag)
{
/**************************************
 *
 *	P A G _ s e t _ f o r c e _ w r i t e
 *
 **************************************
 *
 * Functional description
 *	Turn on/off force write.
 *
 **************************************/
	SET_TDBB(tdbb);
	ensureDbWritable(tdbb);

	WIN window(HEADER_PAGE_NUMBER);
	header_page* header = (header_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_header);
	CCH_MARK_MUST_WRITE(tdbb, &window);

	const auto dbb = tdbb->getDatabase();

	if (flag)
	{
		header->hdr_flags |= hdr_force_write;
		dbb->dbb_flags |= DBB_force_write;
	}
	else
	{
		header->hdr_flags &= ~hdr_force_write;
		dbb->dbb_flags &= ~DBB_force_write;
	}

	CCH_RELEASE(tdbb, &window);

	PageSpace* const pageSpace = dbb->dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
	PIO_force_write(pageSpace->file, flag);

	for (Shadow* shadow = dbb->dbb_shadow; shadow; shadow = shadow->sdw_next)
		PIO_force_write(shadow->sdw_file, flag);

	if (dbb->dbb_backup_manager->getState() != Ods::hdr_nbak_normal)
		dbb->dbb_backup_manager->setForcedWrites(flag);
}


void PAG_set_no_reserve(thread_db* tdbb, bool flag)
{
/**************************************
 *
 *	P A G _ s e t _ n o _ r e s e r v e
 *
 **************************************
 *
 * Functional description
 *	Turn on/off reserving space for versions
 *
 **************************************/
	SET_TDBB(tdbb);
	ensureDbWritable(tdbb);

	WIN window(HEADER_PAGE_NUMBER);
	header_page* header = (header_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_header);
	CCH_MARK_MUST_WRITE(tdbb, &window);

	const auto dbb = tdbb->getDatabase();

	if (flag)
	{
		header->hdr_flags |= hdr_no_reserve;
		dbb->dbb_flags |= DBB_no_reserve;
	}
	else
	{
		header->hdr_flags &= ~hdr_no_reserve;
		dbb->dbb_flags &= ~DBB_no_reserve;
	}

	CCH_RELEASE(tdbb, &window);
}


void PAG_set_db_readonly(thread_db* tdbb, bool flag)
{
/*********************************************
 *
 *	P A G _ s e t _ d b _ r e a d o n l y
 *
 *********************************************
 *
 * Functional description
 *	Set database access mode to readonly OR readwrite
 *
 *********************************************/
	SET_TDBB(tdbb);
	const auto dbb = tdbb->getDatabase();

	WIN window(HEADER_PAGE_NUMBER);
	header_page* header = (header_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_header);

	if (!flag)
	{
		// If the database is transitioning from RO to RW, reset the
		// in-memory Database flag which indicates that the database is RO.
		// This will allow the CCH subsystem to allow pages to be MARK'ed
		// for WRITE operations
		header->hdr_flags &= ~hdr_read_only;
		dbb->dbb_flags &= ~DBB_read_only;

		// Take into account current attachment ID, else next attachment
		// (cache writer, for examle) will get the same att ID and wait
		// for att lock indefinitely.
		Attachment* att = tdbb->getAttachment();
		if (att->att_attachment_id)
			header->hdr_attachment_id = att->att_attachment_id;

		// This is necessary as dbb's Next could be less than OAT.
		// And this is safe as we currently in exclusive attachment and
		// all executed transactions was read-only.
		dbb->dbb_next_transaction = header->hdr_next_transaction;
		dbb->dbb_oldest_transaction = header->hdr_oldest_transaction;
		dbb->dbb_oldest_active = header->hdr_oldest_active;
		dbb->dbb_oldest_snapshot = header->hdr_oldest_snapshot;
	}

	CCH_MARK_MUST_WRITE(tdbb, &window);

	if (flag)
	{
		header->hdr_flags |= hdr_read_only;
		dbb->dbb_flags |= DBB_read_only;
	}

	CCH_RELEASE(tdbb, &window);
}


void PAG_set_db_replica(thread_db* tdbb, ReplicaMode mode)
{
/*********************************************
 *
 *	P A G _ s e t _ d b _ r e p l i c a
 *
 *********************************************
 *
 * Functional description
 *	Set replica mode (none, read-only, read-write)
 *
 *********************************************/
	SET_TDBB(tdbb);
	ensureDbWritable(tdbb);

	WIN window(HEADER_PAGE_NUMBER);
	const auto header = (header_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_header);

	CCH_MARK_MUST_WRITE(tdbb, &window);

	const auto dbb = tdbb->getDatabase();

	switch (mode)
	{
	case REPLICA_NONE:
		header->hdr_replica_mode = hdr_replica_none;
		break;

	case REPLICA_READ_ONLY:
		header->hdr_replica_mode = hdr_replica_read_only;
		break;

	case REPLICA_READ_WRITE:
		header->hdr_replica_mode = hdr_replica_read_write;
		break;

	default:
		fb_assert(false);
	}

	CCH_RELEASE(tdbb, &window);

	dbb->dbb_replica_mode.store(mode, std::memory_order_relaxed);
}


void PAG_set_db_SQL_dialect(thread_db* tdbb, SSHORT flag)
{
/*********************************************
 *
 *	P A G _ s e t _ d b _ S Q L _ d i a l e c t
 *
 *********************************************
 *
 * Functional description
 *	Set database SQL dialect to SQL_DIALECT_V5 or SQL_DIALECT_V6
 *
 *********************************************/
	SET_TDBB(tdbb);
	ensureDbWritable(tdbb);

	WIN window(HEADER_PAGE_NUMBER);
	header_page* header = (header_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_header);

	const auto dbb = tdbb->getDatabase();

	if (flag)
	{
		switch (flag)
		{
		case SQL_DIALECT_V5:

			if ((dbb->dbb_flags & DBB_DB_SQL_dialect_3) || (header->hdr_flags & hdr_SQL_dialect_3))
			{
				// Check the returned value here!
				ERR_post_warning(Arg::Warning(isc_dialect_reset_warning));
			}

			dbb->dbb_flags &= ~DBB_DB_SQL_dialect_3;	// set to 0
			header->hdr_flags &= ~hdr_SQL_dialect_3;	// set to 0
			break;

		case SQL_DIALECT_V6:
			dbb->dbb_flags |= DBB_DB_SQL_dialect_3;	// set to dialect 3
			header->hdr_flags |= hdr_SQL_dialect_3;	// set to dialect 3
			break;

		default:
			CCH_RELEASE(tdbb, &window);
			ERR_post(Arg::Gds(isc_inv_dialect_specified) << Arg::Num(flag) <<
					 Arg::Gds(isc_valid_db_dialects) << Arg::Str("1 and 3") <<
					 Arg::Gds(isc_dialect_not_changed));
			break;
		}
	}

	CCH_MARK_MUST_WRITE(tdbb, &window);

	CCH_RELEASE(tdbb, &window);
}


void PAG_set_page_buffers(thread_db* tdbb, ULONG buffers)
{
/**************************************
 *
 *	P A G _ s e t _ p a g e _ b u f f e r s
 *
 **************************************
 *
 * Functional description
 *	Set database-specific page buffer cache
 *
 **************************************/
	SET_TDBB(tdbb);
	ensureDbWritable(tdbb);

	WIN window(HEADER_PAGE_NUMBER);
	header_page* header = (header_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_header);
	CCH_MARK_MUST_WRITE(tdbb, &window);
	header->hdr_page_buffers = buffers;
	CCH_RELEASE(tdbb, &window);
}


void PAG_set_repl_sequence(thread_db* tdbb, FB_UINT64 sequence)
{
/**************************************
 *
 *	P A G _ s e t _ r e p l _ s e q u e n c e
 *
 **************************************
 *
 * Functional description
 *	Set replication sequence.
 *
 **************************************/
 	SET_TDBB(tdbb);
	storeClump(tdbb, HDR_repl_seq, sizeof(FB_UINT64), (UCHAR*) &sequence);
}


void PAG_set_sweep_interval(thread_db* tdbb, SLONG interval)
{
/**************************************
 *
 *	P A G _ s e t _ s w e e p _ i n t e r v a l
 *
 **************************************
 *
 * Functional description
 *	Set sweep interval.
 *
 **************************************/
 	SET_TDBB(tdbb);
	storeClump(tdbb, HDR_sweep_interval, sizeof(SLONG), (UCHAR*) &interval);
}


// Class PageSpace starts here

PageSpace::~PageSpace()
{
	if (file)
	{
		PIO_close(file);
		delete file;
	}
}

ULONG PageSpace::actAlloc()
{
/**************************************
 *
 * Functional description
 *  Compute actual number of physically allocated pages of database.
 *
 **************************************/

	return PIO_get_number_of_pages(file, dbb->dbb_page_size);
}

ULONG PageSpace::actAlloc(const Database* dbb)
{
	PageSpace* pgSpace = dbb->dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
	return pgSpace->actAlloc();
}

ULONG PageSpace::maxAlloc()
{
/**************************************
 *
 * Functional description
 *	Compute last physically allocated page of database.
 *
 **************************************/
	const ULONG nPages = PIO_get_number_of_pages(file, dbb->dbb_page_size);

	if (maxPageNumber < nPages)
		maxPageNumber = nPages;

	return nPages;
}

ULONG PageSpace::maxAlloc(const Database* dbb)
{
	PageSpace* pgSpace = dbb->dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
	return pgSpace->maxAlloc();
}

bool PageSpace::onRawDevice() const
{
	return (file->fil_flags & FIL_raw_device) != 0;
}

ULONG PageSpace::lastUsedPage()
{
	const PageManager& pageMgr = dbb->dbb_page_manager;
	ULONG pipLast = pipMaxKnown;
	bool moveUp = true;

	if (!pipLast)
	{
		if (!onRawDevice())
		{
			pipLast = (maxAlloc() / pageMgr.pagesPerPIP) * pageMgr.pagesPerPIP;
			pipLast = pipLast ? pipLast - 1 : pipFirst;
			moveUp = false;
		}
	}

	win window(pageSpaceID, pipLast);
	thread_db* tdbb = JRD_get_thread_data();

	while (true)
	{
		pag* page = CCH_FETCH(tdbb, &window, LCK_read, pag_undefined);

		if (moveUp)
		{
			fb_assert(page->pag_type == pag_pages);

			page_inv_page* pip = (page_inv_page*) page;

			if (pip->pip_used != pageMgr.pagesPerPIP)
				break;

			UCHAR lastByte = pip->pip_bits[pageMgr.bytesBitPIP - 1];
			if (lastByte & 0x80)
				break;
		}
		else if (page->pag_type == pag_pages)
			break;

		CCH_RELEASE(tdbb, &window);

		if (moveUp)
		{
			if (pipLast == pipFirst)
				pipLast = pageMgr.pagesPerPIP - 1;
			else
				pipLast += pageMgr.pagesPerPIP;
		}
		else
		{
			if (pipLast > pageMgr.pagesPerPIP)
				pipLast -= pageMgr.pagesPerPIP;
			else if (pipLast == pipFirst)
				return 0;	// can't find PIP page !
			else
				pipLast = pipFirst;
		}

		window.win_page = pipLast;
	}

	page_inv_page* pip = (page_inv_page*) window.win_buffer;

	int last_bit = pip->pip_used;
	int byte_num = last_bit / 8;
	UCHAR mask = 1 << (last_bit % 8);
	while (last_bit >= 0 && (pip->pip_bits[byte_num] & mask))
	{
		if (mask == 1)
		{
			mask = 0x80;
			byte_num--;
			//fb_assert(byte_num > -1); ???
		}
		else
			mask >>= 1;

		last_bit--;
	}

	CCH_RELEASE(tdbb, &window);
	pipMaxKnown = pipLast;

	return last_bit + (pipLast == pipFirst ? 0 : pipLast);
}

ULONG PageSpace::lastUsedPage(const Database* dbb)
{
	PageSpace* pgSpace = dbb->dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
	return pgSpace->lastUsedPage();
}


const UCHAR bitsInByte[256] =
{
//  0000, 0001, 0010, 0011, 0100, 0101, 0110, 0111, 1000, 1001, 1010, 1011, 1100, 1101, 1110, 1111
       0,    1,    1,    2,    1,    2,    2,    3,    1,    2,    2,    3,    2,    3,    3,    4,
// base + 1 0000
       1,    2,    2,    3,    2,    3,    3,    4,    2,    3,    3,    4,    3,    4,    4,    5,
// base + 10 0000
       1,    2,    2,    3,    2,    3,    3,    4,    2,    3,    3,    4,    3,    4,    4,    5,
// base + 11 0000
       2,    3,    3,    4,    3,    4,    4,    5,    3,    4,    4,    5,    4,    5,    5,    6,
// base + 100 0000
       1,    2,    2,    3,    2,    3,    3,    4,    2,    3,    3,    4,    3,    4,    4,    5,
// base + 101 0000
       2,    3,    3,    4,    3,    4,    4,    5,    3,    4,    4,    5,    4,    5,    5,    6,
// base + 110 0000
       2,    3,    3,    4,    3,    4,    4,    5,    3,    4,    4,    5,    4,    5,    5,    6,
// base + 111 0000
       3,    4,    4,    5,    4,    5,    5,    6,    4,    5,    5,    6,    5,    6,    6,    7,
// base + 1000 0000
       1,    2,    2,    3,    2,    3,    3,    4,    2,    3,    3,    4,    3,    4,    4,    5,
// base + 1001 0000
       2,    3,    3,    4,    3,    4,    4,    5,    3,    4,    4,    5,    4,    5,    5,    6,
// base + 1010 0000
       2,    3,    3,    4,    3,    4,    4,    5,    3,    4,    4,    5,    4,    5,    5,    6,
// base + 1011 0000
       3,    4,    4,    5,    4,    5,    5,    6,    4,    5,    5,    6,    5,    6,    6,    7,
// base + 1100 0000
       2,    3,    3,    4,    3,    4,    4,    5,    3,    4,    4,    5,    4,    5,    5,    6,
// base + 1101 0000
       3,    4,    4,    5,    4,    5,    5,    6,    4,    5,    5,    6,    5,    6,    6,    7,
// base + 1110 0000
       3,    4,    4,    5,    4,    5,    5,    6,    4,    5,    5,    6,    5,    6,    6,    7,
// base + 1111 0000
       4,    5,    5,    6,    5,    6,    6,    7,    5,    6,    6,    7,    6,    7,    7,    8
};

ULONG PageSpace::usedPages()
{
	// Walk all PIP pages, count number of pages marked as used

	thread_db* tdbb = JRD_get_thread_data();
	const PageManager& pageMgr = dbb->dbb_page_manager;

	win window(pageSpaceID, pipFirst);
	ULONG used = 0;
	ULONG sequence = 0;

	while (true)
	{
		page_inv_page* pip = (page_inv_page*) CCH_FETCH(tdbb, &window, LCK_read, pag_undefined);
		if (pip->pip_header.pag_type != pag_pages)
		{
			CCH_RELEASE(tdbb, &window);
			break;
		}

		used += pip->pip_min & (~7);
		const UCHAR* bytes = pip->pip_bits + pip->pip_min / 8;
		const UCHAR* const end = pip->pip_bits + pip->pip_used / 8;
		for (; bytes < end; bytes++)
		{
			used += 8 - bitsInByte[*bytes];
		}

		const bool last = pip->pip_used < pageMgr.pagesPerPIP;

		CCH_RELEASE(tdbb, &window);

		if (last)
			break;

		window.win_page = ++sequence * pageMgr.pagesPerPIP - 1;
	}

	return used;
}

ULONG PageSpace::usedPages(const Database* dbb)
{
	PageSpace* pgSpace = dbb->dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
	return pgSpace->usedPages();
}

bool PageSpace::extend(thread_db* tdbb, const ULONG pageNum, const bool forceSize)
{
/**************************************
 *
 * Functional description
 *	Extend database file(s) up to at least pageNum pages. Number of pages to
 *	extend can't be less than hardcoded value MIN_EXTEND_BYTES and more than
 *	configured value "DatabaseGrowthIncrement" (both values in bytes).
 *
 *	If "DatabaseGrowthIncrement" is less than MIN_EXTEND_BYTES then don't
 *	extend file(s)
 *
 *  If forceSize is true, extend file up to pageNum pages (despite of value
 *  of "DatabaseGrowthIncrement") and don't make attempts to extend by less
 *	pages.
 *
 **************************************/
	fb_assert(dbb == tdbb->getDatabase());

	const int MAX_EXTEND_BYTES = dbb->dbb_config->getDatabaseGrowthIncrement();

	if (pageNum < maxPageNumber || MAX_EXTEND_BYTES < MIN_EXTEND_BYTES && !forceSize)
		return true;

	if (pageNum >= maxAlloc())
	{
		const ULONG minExtendPages = MIN_EXTEND_BYTES / dbb->dbb_page_size;
		const ULONG maxExtendPages = MAX_EXTEND_BYTES / dbb->dbb_page_size;
		const ULONG reqPages = pageNum - maxPageNumber + 1;

		ULONG extPages;
		extPages = MIN(MAX(maxPageNumber / 16, minExtendPages), maxExtendPages);
		extPages = MAX(reqPages, extPages);

		while (true)
		{
			const ULONG oldMaxPageNumber = maxPageNumber;
			try
			{
				PIO_extend(tdbb, file, extPages, dbb->dbb_page_size);
				break;
			}
			catch (const status_exception&)
			{
				if (extPages > reqPages && !forceSize)
				{
					fb_utils::init_status(tdbb->tdbb_status_vector);

					// if file was extended, return, else try to extend by less pages

					if (oldMaxPageNumber < maxAlloc())
						return true;

					extPages = MAX(reqPages, extPages / 2);
				}
				else
				{
					gds__log("Error extending file \"%s\" by %lu page(s).\nCurrently allocated %lu pages, requested page number %lu",
						file->fil_string, extPages, maxPageNumber, pageNum);
					return false;
				}
			}
		}

		maxPageNumber = 0;
	}
	return true;
}

ULONG PageSpace::getSCNPageNum(ULONG sequence)
{
/**************************************
 *
 * Functional description
 *	Return the physical number of the Nth SCN page
 *
 *	SCN pages allocated at every pagesPerSCN pages in database and should
 *	not be the same as PIP page (which allocated at every pagesPerPIP pages).
 *  First SCN page number is fixed as FIRST_SCN_PAGE.
 *
 **************************************/
	if (!sequence) {
		return scnFirst;
	}
	return sequence * dbb->dbb_page_manager.pagesPerSCN;
}

ULONG PageSpace::getSCNPageNum(const Database* dbb, ULONG sequence)
{
	PageSpace* pgSpace = dbb->dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
	return pgSpace->getSCNPageNum(sequence);
}

PageSpace* PageManager::addPageSpace(const USHORT pageSpaceID)
{
	PageSpace* newPageSpace = findPageSpace(pageSpaceID);
	if (!newPageSpace)
	{
		newPageSpace = FB_NEW_POOL(pool) PageSpace(dbb, pageSpaceID);
		pageSpaces.add(newPageSpace);
	}

	return newPageSpace;
}

PageSpace* PageManager::findPageSpace(const USHORT pageSpace) const
{
	FB_SIZE_T pos;
	if (pageSpaces.find(pageSpace, pos)) {
		return pageSpaces[pos];
	}

	return 0;
}

void PageManager::delPageSpace(const USHORT pageSpace)
{
	FB_SIZE_T pos;
	if (pageSpaces.find(pageSpace, pos))
	{
		PageSpace* pageSpaceToDelete = pageSpaces[pos];
		pageSpaces.remove(pos);
		delete pageSpaceToDelete;
	}
}

void PageManager::closeAll()
{
	for (FB_SIZE_T i = 0; i < pageSpaces.getCount(); i++)
	{
		if (pageSpaces[i]->file) {
			PIO_close(pageSpaces[i]->file);
		}
	}
}

void PageManager::initTempPageSpace(thread_db* tdbb)
{
	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	fb_assert(tempPageSpaceID == 0);

	if (dbb->dbb_config->getServerMode() != MODE_SUPER)
	{
		Jrd::Attachment* const attachment = tdbb->getAttachment();

		if (!attachment->att_temp_pg_lock)
		{
			Lock* const lock = FB_NEW_RPT(*attachment->att_pool, 0)
				Lock(tdbb, sizeof(SLONG), LCK_page_space);

			while (true)
			{
				const double tmp = rand() * (MAX_USHORT - TEMP_PAGE_SPACE - 1.0) / (RAND_MAX + 1.0);
				lock->setKey(static_cast<SLONG>(tmp) + TEMP_PAGE_SPACE + 1);
				if (LCK_lock(tdbb, lock, LCK_write, LCK_NO_WAIT))
					break;
				fb_utils::init_status(tdbb->tdbb_status_vector);
			}

			attachment->att_temp_pg_lock = lock;
		}

		tempPageSpaceID = (USHORT) attachment->att_temp_pg_lock->getKey();
	}
	else
	{
		tempPageSpaceID = TEMP_PAGE_SPACE;
	}

	addPageSpace(tempPageSpaceID);
}

USHORT PageManager::getTempPageSpaceID(thread_db* tdbb)
{
	fb_assert(tempPageSpaceID != 0);
	if (!tempFileCreated)
	{
		ScratchBird::MutexLockGuard guard(initTmpMtx, FB_FUNCTION);
		if (!tempFileCreated)
		{
			FbLocalStatus status;
			PathName tempDir(dbb->dbb_config->getTempPageSpaceDirectory());
			PathName file_name = TempFile::create(&status, SCRATCH, tempDir);

			if (!status.isSuccess())
			{
				string error;
				error.printf("Database: %s\n\tError creating file in TempTableDirectory \"%s\"",
							 dbb->dbb_filename.c_str(), tempDir.c_str());
				iscLogStatus(error.c_str(), &status);

				file_name = TempFile::create(SCRATCH);
			}

			PageSpace* pageSpaceTemp = dbb->dbb_page_manager.findPageSpace(tempPageSpaceID);
			pageSpaceTemp->file = PIO_create(tdbb, file_name, true, true);
			PAG_format_pip(tdbb, *pageSpaceTemp);

			tempFileCreated = true;
		}
	}
	return tempPageSpaceID;
}

ULONG PAG_page_count(thread_db* tdbb)
{
/*********************************************
 *
 *	P A G _ p a g e _ c o u n t
 *
 *********************************************
 *
 * Functional description
 *	Count pages, used by primary database file
 *	(for nbackup purposes)
 *
 *********************************************/
	Database* const dbb = tdbb->getDatabase();
	Array<UCHAR> temp;
	page_inv_page* pip = reinterpret_cast<Ods::page_inv_page*>
		(temp.getAlignedBuffer(dbb->dbb_page_size, dbb->getIOBlockSize()));

	PageSpace* const pageSpace = dbb->dbb_page_manager.findPageSpace(DB_PAGE_SPACE);
	fb_assert(pageSpace);

	ULONG pageNo = pageSpace->pipFirst;
	const ULONG pagesPerPip = dbb->dbb_page_manager.pagesPerPIP;
	BufferDesc temp_bdb(dbb->dbb_bcb);
	temp_bdb.bdb_buffer = &pip->pip_header;

	for (ULONG sequence = 0; true; pageNo = (pagesPerPip * ++sequence) - 1)
	{
		temp_bdb.bdb_page = pageNo;

		FbLocalStatus status;
		// It's PIP - therefore no need to try to decrypt
		if (!PIO_read(tdbb, pageSpace->file, &temp_bdb, temp_bdb.bdb_buffer, &status))
			status_exception::raise(&status);

		// After PIO_extend the tail of the file might have thousands of zero pages.
		// Recently last PIP might be marked as fully used but the new PIP is not initialized.
		// If nbackup state becomes nbak_stalled in this moment we'll find zero pip in the tail of the file.
		// Fortunatelly it must be the last valuable page and we can rely on its number.
		fb_assert(pip->pip_header.pag_type == pag_pages ||
				  (!pip->pip_header.pag_type && !pip->pip_used) );
		if (pip->pip_used == pagesPerPip)
		{
			// this is not last page, continue search
			continue;
		}

		return pip->pip_used + pageNo - (sequence ? 0  : pageSpace->pipFirst) + 1;
	}

	// compiler warnings silencer
	return 0;
}

void PAG_set_page_scn(thread_db* tdbb, win* window)
{
	Database* dbb = tdbb->getDatabase();
	fb_assert(dbb->dbb_ods_version >= ODS_VERSION12);

	PageManager& pageMgr = dbb->dbb_page_manager;
	PageSpace* const pageSpace = pageMgr.findPageSpace(window->win_page.getPageSpaceID());

	if (pageSpace->isTemporary())
		return;

	const ULONG curr_scn = window->win_buffer->pag_scn;
	const ULONG page_num = window->win_page.getPageNum();
	const ULONG scn_seq = page_num / pageMgr.pagesPerSCN;
	const ULONG scn_slot = page_num % pageMgr.pagesPerSCN;

	const ULONG scn_page = pageSpace->getSCNPageNum(scn_seq);

	if (scn_page == page_num)
	{
		scns_page* page = (scns_page*) window->win_buffer;
		page->scn_pages[scn_slot] = curr_scn;
		return;
	}

	win scn_window(pageSpace->pageSpaceID, scn_page);

	scns_page* page = (scns_page*) CCH_FETCH(tdbb, &scn_window, LCK_write, pag_scns);
	if (page->scn_pages[scn_slot] != curr_scn)
	{
		CCH_MARK(tdbb, &scn_window);
		page->scn_pages[scn_slot] = curr_scn;
	}
	CCH_RELEASE(tdbb, &scn_window);

	CCH_precedence(tdbb, window, scn_page);
}


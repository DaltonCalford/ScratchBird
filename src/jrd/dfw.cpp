/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		dfw.cpp
 *	DESCRIPTION:	Deferred Work handler
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
 * 2001.6.25 Claudio Valderrama: Implement deferred check for udf usage
 * inside a procedure before dropping the udf and creating stub for future
 * processing of dependencies from dropped generators.
 *
 * 2001.8.12 Claudio Valderrama: find_depend_in_dfw() and other functions
 *   should respect identifiers with embedded blanks instead of chopping them
*.
 * 2001.10.01 Claudio Valderrama: check constraints should fire AFTER the
 *   BEFORE <action> triggers; otherwise they allow invalid data to be stored.
 *   This is a quick fix for SF Bug #444463 until a more robust one is devised
 *   using trigger's rdb$flags or another mechanism.
 *
 * 2001.10.10 Ann Harrison:  Don't increment the format version unless the
 *   table is actually reformatted.  At the same time, break out some of
 *   the parts of make_version making some new subroutines with the goal
 *   of making make_version readable.
 *
 * 2001.10.18 Ann Harrison: some cleanup of trigger & constraint handling.
 *   it now appears to work correctly on new Firebird databases with lots
 *   of system types and on InterBase databases, without checking for
 *   missing source.
 *
 * 23-Feb-2002 Dmitry Yemanov - Events wildcarding
 *
 * 2002-02-24 Sean Leyne - Code Cleanup of old Win 3.1 port (WINDOWS_ONLY)
 *
 * Adriano dos Santos Fernandes
 *
 * 2008-03-16 Alex Peshkoff - avoid most of data modifications in system transaction.
 *	Problems took place when same data was modified in user transaction, and later -
 *	in system transaction. System transaction always performs updates in place,
 *	but when between RPB setup and actual modification garbage was collected (this
 *	was noticed with GC thread active, but may happen due to any read of the record),
 *	BUGCHECK(291) took place. To avoid that issue, it was decided not to modify data
 *	in system transaction. An exception is RDB$FORMATS relation, which is always modified
 *	by transaction zero. Also an aspect of 'dirty' access from system transaction was
 *	taken into an account in make_version() and create_index().
 *
 */

#include "scratchbird.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../common/classes/fb_string.h"
#include "../common/classes/VaryStr.h"
#include "../jrd/SystemPrivileges.h"
#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/irq.h"
#include "../jrd/tra.h"
#include "../jrd/os/pio.h"
#include "../jrd/ods.h"
#include "../jrd/btr.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/scl.h"
#include "../jrd/blb.h"
#include "../jrd/met.h"
#include "../jrd/lck.h"
#include "../jrd/sdw.h"
#include "../jrd/flags.h"
#include "../jrd/intl.h"
#include "../intl/charsets.h"
#include "../jrd/align.h"
#include "../dsql/DsqlStatementCache.h"
#include "../common/gdsassert.h"
#include "../jrd/blb_proto.h"
#include "../jrd/btr_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/dfw_proto.h"
#include "../jrd/dpm_proto.h"
#include "../common/dsc_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/ext_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/grant_proto.h"
#include "../jrd/idx_proto.h"
#include "../jrd/intl_proto.h"
#include "../common/isc_f_proto.h"

#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/os/pio_proto.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/scl_proto.h"
#include "../jrd/sdw_proto.h"
#include "../jrd/tra_proto.h"
#include "../jrd/event_proto.h"
#include "../jrd/nbak.h"
#include "../jrd/trig.h"
#include "../jrd/GarbageCollector.h"
#include "../jrd/IntlManager.h"
#include "../jrd/UserManagement.h"
#include "../jrd/Function.h"
#include "../jrd/PreparedStatement.h"
#include "../jrd/ResultSet.h"
#include "../common/utils_proto.h"
#include "../common/classes/Hash.h"
#include "../jrd/CryptoManager.h"
#include "../jrd/Mapping.h"
#include "../jrd/shut_proto.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "iberror.h"

// Pick up system relation ids
#include "../jrd/ini.h"

// Define range of user relation ids

inline constexpr int MIN_RELATION_ID = rel_MAX;
inline constexpr int MAX_RELATION_ID = 32767;

inline constexpr int COMPUTED_FLAG = 128;
inline constexpr int WAIT_PERIOD = -1;

using namespace Jrd;
using namespace ScratchBird;

namespace Jrd {

typedef HashTable<
	DeferredWork,
	DEFAULT_HASH_SIZE,
	DeferredWork,
	DefaultKeyValue<DeferredWork>,
	DeferredWork
> DfwHash;

// NS: This needs careful refactoring.
//
// Deferred work item:
// * Encapsulates deferred invocation of the task routine with a given set of
//   arguments.
// * Has code to maintain a doubly linked list of itself.
//
// These two functions need to be split, and linked list of custom entries can
// become generic.
//

// Forward declaration for template usage
class DeferredWork;

class DeferredWork : public pool_alloc<type_dfw>,
					 public DfwHash::Entry
{
private:
	DeferredWork(const DeferredWork&);

public:
	enum dfw_t 		dfw_type;		// type of work deferred

private:
	DeferredWork***	dfw_end;
	DeferredWork**	dfw_prev;
	DeferredWork*	dfw_next;

public:
	Lock*			dfw_lock;		// relation creation lock
	Array<DeferredWork*>  dfw_args;	// arguments
	SavNumber		dfw_sav_number;	// save point number
	USHORT			dfw_id;			// object id, if appropriate
	USHORT			dfw_count;		// count of block posts
	string			dfw_name;		// name of object
	MetaName		dfw_schema;		// schema name
	MetaName		dfw_package;	// package name
	SortedArray<int> dfw_ids;		// list of identifiers (or any numbers) needed by an action

public:
	DeferredWork(MemoryPool& p, DeferredWork*** end,
				 enum dfw_t t, USHORT id, SavNumber sn, const string& name,
				 const MetaName& schema, const MetaName& package)
	  : dfw_type(t), dfw_end(end), dfw_prev(dfw_end ? *dfw_end : NULL),
		dfw_next(dfw_prev ? *dfw_prev : NULL), dfw_lock(NULL), dfw_args(p),
		dfw_sav_number(sn), dfw_id(id), dfw_count(1), dfw_name(p, name),
		dfw_schema(p, schema), dfw_package(p, package),
		dfw_ids(p)
	{
		// make previous element point to us
		if (dfw_prev)
		{
			*dfw_prev = this;
			// make next element (if present) to point to us
			if (dfw_next)
			{
				dfw_next->dfw_prev = &dfw_next;
			}
		}
	}

	~DeferredWork()
	{
		// if we are linked
		if (dfw_prev)
		{
			if (dfw_next)
			{
				// adjust previous pointer in next element ...
				dfw_next->dfw_prev = dfw_prev;
			}
			// adjust next pointer in previous element
			*dfw_prev = dfw_next;

			// Adjust end marker of the list
			if (*dfw_end == &dfw_next)
			{
				*dfw_end = dfw_prev;
			}
		}

		for (DeferredWork** itr = dfw_args.begin(); itr < dfw_args.end(); ++itr)
		{
			delete *itr;
		}

		if (dfw_lock)
		{
			LCK_release(JRD_get_thread_data(), dfw_lock);
			delete dfw_lock;
		}
	}

	DeferredWork* findArg(dfw_t type) const
	{
		for (DeferredWork* const* itr = dfw_args.begin(); itr < dfw_args.end(); ++itr)
		{
			DeferredWork* const arg = *itr;

			if (arg->dfw_type == type)
			{
				return arg;
			}
		}

		return NULL;
	}

	DeferredWork** getNextPtr()
	{
		return &dfw_next;
	}

	DeferredWork* getNext() const
	{
		return dfw_next;
	}

	QualifiedName getQualifiedName() const
	{
		return QualifiedName(dfw_name, dfw_schema, dfw_package);
	}

	// hash interface
	bool isEqual(const DeferredWork& work) const
	{
		if (dfw_type == work.dfw_type &&
			dfw_id == work.dfw_id &&
			dfw_name == work.dfw_name &&
			dfw_schema == work.dfw_schema &&
			dfw_package == work.dfw_package &&
			dfw_sav_number == work.dfw_sav_number)
		{
			return true;
		}
		return false;
	}

	DeferredWork* get() { return this; }

	static FB_SIZE_T hash(const DeferredWork& work, FB_SIZE_T hashSize)
	{
		constexpr int nameLimit = 32;
		char key[sizeof work.dfw_type + sizeof work.dfw_id + nameLimit + nameLimit];
		memset(key, 0, sizeof key);
		char* place = key;

		memcpy(place, &work.dfw_type, sizeof work.dfw_type);
		place += sizeof work.dfw_type;

		memcpy(place, &work.dfw_id, sizeof work.dfw_id);
		place += sizeof work.dfw_id;

		work.dfw_name.copyTo(place, nameLimit);	// It's good enough to have first 32 bytes
		place += nameLimit;

		work.dfw_schema.copyTo(place, nameLimit);	// It's good enough to have first 32 bytes

		return DefaultHash<DeferredWork>::hash(key, sizeof key, hashSize);
	}
};

class DfwSavePoint;

typedef HashTable<
	DfwSavePoint,
	DEFAULT_HASH_SIZE,
	SavNumber,
	DfwSavePoint
> DfwSavePointHash;

class DfwSavePoint : public DfwSavePointHash::Entry
{
	SavNumber dfw_sav_number;

public:
	DfwHash hash; // Deferred work items posted under this savepoint

	explicit DfwSavePoint(SavNumber number) : dfw_sav_number(number) { }

	// hash interface
	bool isEqual(const SavNumber& number) const
	{
		return dfw_sav_number == number;
	}

	DfwSavePoint* get() { return this; }

	static SavNumber generate(const DfwSavePoint& item)
	{
		return item.dfw_sav_number;
	}
};

// List of deferred work items (with per-savepoint break-down)
class DeferredJob
{
public:
	DfwSavePointHash hash; // Hash set of savepoints, that posted work
	DeferredWork* work;
	DeferredWork** end;

	DeferredJob() : work(NULL), end(&work) { }
};


// Lock relation with protected_read level or raise existing relation lock
// to this level to ensure nobody can write to this relation.
// Used when new index is built.
// releaseLock set to true if there was no existing lock before
class ProtectRelations
{
public:
	ProtectRelations(thread_db* tdbb, jrd_tra* transaction) :
		m_tdbb(tdbb),
		m_transaction(transaction),
		m_locks()
	{
	}

	ProtectRelations(thread_db* tdbb, jrd_tra* transaction, jrd_rel* relation) :
		m_tdbb(tdbb),
		m_transaction(transaction),
		m_locks()
	{
		addRelation(relation);
		lock();
	}

	~ProtectRelations()
	{
		unlock();
	}

	void addRelation(jrd_rel* relation)
	{
		FB_SIZE_T pos;
		if (!m_locks.find(relation->rel_id, pos))
			m_locks.insert(pos, relLock(relation));
	}

	bool exists(USHORT rel_id) const
	{
		FB_SIZE_T pos;
		return m_locks.find(rel_id, pos);
	}

	void lock()
	{
		relLock* item = m_locks.begin();
		const relLock* const end = m_locks.end();
		for (; item < end; item++)
			item->takeLock(m_tdbb, m_transaction);
	}

	void unlock()
	{
		relLock* item = m_locks.begin();
		const relLock* const end = m_locks.end();
		for (; item < end; item++)
			item->releaseLock(m_tdbb, m_transaction);
	}

private:
	struct relLock
	{
		relLock(jrd_rel* relation = NULL) :
			m_relation(relation),
			m_lock(NULL),
			m_release(false)
		{
		}

		void takeLock(thread_db* tdbb, jrd_tra* transaction);
		void releaseLock(thread_db* tdbb, jrd_tra* transaction);

		static const USHORT generate(const relLock& item)
		{
			return item.m_relation->rel_id;
		}

		jrd_rel* m_relation;
		Lock* m_lock;
		bool m_release;
	};

	thread_db* m_tdbb;
	jrd_tra* m_transaction;
	SortedArray<relLock, InlineStorage<relLock, 2>, USHORT, relLock> m_locks;
};

	// Namespace managers for function/procedure validation
	namespace FunctionManager
	{
		Routine* lookupBlobId(thread_db* tdbb, DeferredWork* work, bid& blobId, bool compile);
		void validate(thread_db* tdbb, jrd_tra* transaction, DeferredWork* work, SSHORT validBlr);
		void checkParamDependencies(thread_db* tdbb, DeferredWork* work, jrd_tra* transaction);
	}

	namespace ProcedureManager
	{
		Routine* lookupBlobId(thread_db* tdbb, DeferredWork* work, bid& blobId, bool compile);
		void validate(thread_db* tdbb, jrd_tra* transaction, DeferredWork* work, SSHORT validBlr);
		void checkParamDependencies(thread_db* tdbb, DeferredWork* work, jrd_tra* transaction);
	}

	// These methods cannot be defined inline, because GPRE generates wrong code.

	Routine* FunctionManager::lookupBlobId(thread_db* tdbb, DeferredWork* work, bid& blobId,
		bool compile)
	{
		Jrd::Attachment* attachment = tdbb->getAttachment();
		AutoCacheRequest handle(tdbb, irq_c_fun_dpd, IRQ_REQUESTS);
		Routine* routine = NULL;

		// Converted FOR loop #1: Query RDB$FUNCTIONS for function blob ID lookup
		jrd_req* request = NULL;
		try {
			request = EXE_find_request(tdbb, handle.getRequest(), false);
			EXE_start(tdbb, request, attachment->getSysTransaction());

			while (true) {
				EXE_send(tdbb, request, 0, sizeof(work->dfw_schema) + sizeof(work->dfw_name) + sizeof(work->dfw_package),
					reinterpret_cast<const UCHAR*>(&work->dfw_schema));
				
				if (!EXE_receive(tdbb, request, 1, sizeof(blobId) + sizeof(routine), 
					reinterpret_cast<UCHAR*>(&blobId))) {
					break;
				}

				routine = Function::lookup(tdbb, work->getQualifiedName(), !compile);
			}
		}
		catch (...) {
			if (request) EXE_unwind(tdbb, request);
			throw;
		}
		if (request) EXE_unwind(tdbb, request);

		return routine;
	}

	void FunctionManager::validate(thread_db* tdbb, jrd_tra* transaction, DeferredWork* work,
		SSHORT validBlr)
	{
		Jrd::Attachment* attachment = tdbb->getAttachment();
		AutoCacheRequest request(tdbb, irq_fun_validate, IRQ_REQUESTS);

		// Converted FOR loop #2: Update RDB$FUNCTIONS with validation status
		jrd_req* jrdRequest = NULL;
		try {
			jrdRequest = EXE_find_request(tdbb, request.getRequest(), false);
			EXE_start(tdbb, jrdRequest, transaction);

			EXE_send(tdbb, jrdRequest, 0, sizeof(work->dfw_id), 
				reinterpret_cast<const UCHAR*>(&work->dfw_id));
			
			UCHAR response[100]; // Buffer for response data
			if (EXE_receive(tdbb, jrdRequest, 1, sizeof(response), response)) {
				// Converted MODIFY operation #1: Update function validation
				EXE_send(tdbb, jrdRequest, 2, sizeof(validBlr), 
					reinterpret_cast<const UCHAR*>(&validBlr));
			}
		}
		catch (...) {
			if (jrdRequest) EXE_unwind(tdbb, jrdRequest);
			throw;
		}
		if (jrdRequest) EXE_unwind(tdbb, jrdRequest);
	}

	void FunctionManager::checkParamDependencies(thread_db* tdbb, DeferredWork* work, jrd_tra* transaction)
	{
		Jrd::Attachment* attachment = tdbb->getAttachment();
		AutoCacheRequest handle(tdbb, irq_func_param_dep, IRQ_REQUESTS);
		ObjectsArray<string> names;
		int depCount = 0;

		// Converted FOR loop #3: Check function parameter dependencies
		jrd_req* request = NULL;
		try {
			request = EXE_find_request(tdbb, handle.getRequest(), false);
			EXE_start(tdbb, request, attachment->getSysTransaction());

			struct {
				TEXT dependent_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT dependent_schema[MAX_SQL_IDENTIFIER_LEN];
				TEXT field_name[MAX_SQL_IDENTIFIER_LEN];
				SSHORT dependent_type;
			} depData;

			EXE_send(tdbb, request, 0, sizeof(work->dfw_schema) + sizeof(work->dfw_name) + sizeof(work->dfw_package),
				reinterpret_cast<const UCHAR*>(&work->dfw_schema));

			while (EXE_receive(tdbb, request, 1, sizeof(depData), 
				reinterpret_cast<UCHAR*>(&depData))) {
				
				// If the found object is also being deleted, there's no dependency
				if (!find_depend_in_dfw(tdbb,
						QualifiedName(depData.dependent_name, depData.dependent_schema),
						depData.dependent_type, 0, transaction))
				{
					string& name = names.add();
					name.printf("%s.%s", work->getQualifiedName().toQuotedString().c_str(), depData.field_name);
					++depCount;
				}
			}
		}
		catch (...) {
			if (request) EXE_unwind(tdbb, request);
			throw;
		}
		if (request) EXE_unwind(tdbb, request);

		if (names.hasData())
		{
			Arg::StatusVector status;
			status << Arg::Gds(isc_no_meta_update) << Arg::Gds(isc_no_delete);

			for (auto& name : names)
				status << Arg::Gds(isc_parameter_name) << name;

			status << Arg::Gds(isc_dependency) << Arg::Num(depCount);

			ERR_post(status);
		}
	}

	Routine* ProcedureManager::lookupBlobId(thread_db* tdbb, DeferredWork* work, bid& blobId,
		bool compile)
	{
		Jrd::Attachment* attachment = tdbb->getAttachment();
		AutoCacheRequest handle(tdbb, irq_c_prc_dpd, IRQ_REQUESTS);
		Routine* routine = NULL;

		// Converted FOR loop #4: Query RDB$PROCEDURES for procedure blob ID lookup
		jrd_req* request = NULL;
		try {
			request = EXE_find_request(tdbb, handle.getRequest(), false);
			EXE_start(tdbb, request, attachment->getSysTransaction());

			EXE_send(tdbb, request, 0, sizeof(work->dfw_schema) + sizeof(work->dfw_name) + sizeof(work->dfw_package),
				reinterpret_cast<const UCHAR*>(&work->dfw_schema));
			
			if (EXE_receive(tdbb, request, 1, sizeof(blobId), 
				reinterpret_cast<UCHAR*>(&blobId))) {
				routine = MET_lookup_procedure(tdbb, work->getQualifiedName(), !compile);
			}
		}
		catch (...) {
			if (request) EXE_unwind(tdbb, request);
			throw;
		}
		if (request) EXE_unwind(tdbb, request);

		return routine;
	}

	void ProcedureManager::validate(thread_db* tdbb, jrd_tra* transaction, DeferredWork* work,
		SSHORT validBlr)
	{
		Jrd::Attachment* attachment = tdbb->getAttachment();
		AutoCacheRequest request(tdbb, irq_prc_validate, IRQ_REQUESTS);

		// Converted FOR loop #5: Update RDB$PROCEDURES with validation status
		jrd_req* jrdRequest = NULL;
		try {
			jrdRequest = EXE_find_request(tdbb, request.getRequest(), false);
			EXE_start(tdbb, jrdRequest, transaction);

			EXE_send(tdbb, jrdRequest, 0, sizeof(work->dfw_id), 
				reinterpret_cast<const UCHAR*>(&work->dfw_id));
			
			UCHAR response[100];
			if (EXE_receive(tdbb, jrdRequest, 1, sizeof(response), response)) {
				// Converted MODIFY operation #2: Update procedure validation
				EXE_send(tdbb, jrdRequest, 2, sizeof(validBlr), 
					reinterpret_cast<const UCHAR*>(&validBlr));
			}
		}
		catch (...) {
			if (jrdRequest) EXE_unwind(tdbb, jrdRequest);
			throw;
		}
		if (jrdRequest) EXE_unwind(tdbb, jrdRequest);
	}

	void ProcedureManager::checkParamDependencies(thread_db* tdbb, DeferredWork* work, jrd_tra* transaction)
	{
		Jrd::Attachment* attachment = tdbb->getAttachment();
		AutoCacheRequest handle(tdbb, irq_proc_param_dep, IRQ_REQUESTS);
		ObjectsArray<string> names;
		int depCount = 0;

		// Converted FOR loop #6: Check procedure parameter dependencies
		jrd_req* request = NULL;
		try {
			request = EXE_find_request(tdbb, handle.getRequest(), false);
			EXE_start(tdbb, request, attachment->getSysTransaction());

			struct {
				TEXT dependent_name[MAX_SQL_IDENTIFIER_LEN];
				TEXT dependent_schema[MAX_SQL_IDENTIFIER_LEN];
				TEXT field_name[MAX_SQL_IDENTIFIER_LEN];
				SSHORT dependent_type;
			} depData;

			EXE_send(tdbb, request, 0, sizeof(work->dfw_schema) + sizeof(work->dfw_name) + sizeof(work->dfw_package),
				reinterpret_cast<const UCHAR*>(&work->dfw_schema));

			while (EXE_receive(tdbb, request, 1, sizeof(depData), 
				reinterpret_cast<UCHAR*>(&depData))) {
				
				// If the found object is also being deleted, there's no dependency
				if (!find_depend_in_dfw(tdbb,
						QualifiedName(depData.dependent_name, depData.dependent_schema),
						depData.dependent_type, 0, transaction))
				{
					string& name = names.add();
					name.printf("%s.%s", work->getQualifiedName().toQuotedString().c_str(), depData.field_name);
					++depCount;
				}
			}
		}
		catch (...) {
			if (request) EXE_unwind(tdbb, request);
			throw;
		}
		if (request) EXE_unwind(tdbb, request);

		if (names.hasData())
		{
			Arg::StatusVector status;
			status << Arg::Gds(isc_no_meta_update) << Arg::Gds(isc_no_delete);

			for (auto& name : names)
				status << Arg::Gds(isc_parameter_name) << name;

			status << Arg::Gds(isc_dependency) << Arg::Num(depCount);

			ERR_post(status);
		}
	}

} // namespace Jrd

// Task table and forward declarations
static inline constexpr deferred_task task_table[] =
{
	{ dfw_add_shadow, add_shadow },
	{ dfw_delete_index, modify_index },
	{ dfw_delete_rfr, delete_rfr },
	{ dfw_delete_relation, delete_relation },
	{ dfw_delete_shadow, delete_shadow },
	{ dfw_delete_shadow_nodelete, delete_shadow },
	{ dfw_create_field, create_field },
	{ dfw_delete_field, delete_field },
	{ dfw_modify_field, modify_field },
	{ dfw_delete_global, delete_global },
	{ dfw_create_relation, create_relation },
	{ dfw_update_format, make_version },
	{ dfw_scan_relation, scan_relation },
	{ dfw_compute_security, compute_security },
	{ dfw_create_index, modify_index },
	{ dfw_create_expression_index, modify_index },
	{ dfw_grant, grant_privileges },
	{ dfw_create_trigger, create_trigger },
	{ dfw_delete_trigger, delete_trigger },
	{ dfw_modify_trigger, modify_trigger },
	{ dfw_drop_package_header, drop_package_header },	// packages should be before procedures
	{ dfw_modify_package_header, modify_package_header },	// packages should be before procedures
	{ dfw_drop_package_body, drop_package_body },		// packages should be before procedures
	{ dfw_create_procedure, ProcedureManager::createRoutine },
	{ dfw_create_function, FunctionManager::createRoutine },
	{ dfw_delete_procedure, ProcedureManager::deleteRoutine },
	{ dfw_delete_function, FunctionManager::deleteRoutine },
	{ dfw_modify_procedure, ProcedureManager::modifyRoutine },
	{ dfw_modify_function, FunctionManager::modifyRoutine },
	{ dfw_delete_prm, delete_parameter },
	{ dfw_create_collation, create_collation },
	{ dfw_delete_collation, delete_collation },
	{ dfw_delete_exception, delete_exception },
	{ dfw_set_generator, set_generator },
	{ dfw_delete_generator, delete_generator },
	{ dfw_add_difference, add_difference },
	{ dfw_delete_difference, delete_difference },
	{ dfw_begin_backup, begin_backup },
	{ dfw_end_backup, end_backup },
	{ dfw_user_management, user_management },
	{ dfw_check_not_null, check_not_null },
	{ dfw_store_view_context_type, store_view_context_type },
	{ dfw_db_crypt, db_crypt },
	{ dfw_set_linger, set_linger },
	{ dfw_clear_cache, clear_cache },
	{ dfw_change_repl_state, change_repl_state },
	{ dfw_null, NULL }
};

// Forward function declarations
static bool add_shadow(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_shadow(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool compute_security(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool modify_index(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool create_index(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_index(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool create_relation(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_relation(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool scan_relation(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool create_trigger(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_trigger(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool modify_trigger(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool create_collation(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_collation(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_exception(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool set_generator(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_generator(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool create_field(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_field(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool modify_field(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_global(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_parameter(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_rfr(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool make_version(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool add_difference(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool delete_difference(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool begin_backup(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool end_backup(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool check_not_null(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool store_view_context_type(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool user_management(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool drop_package_header(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool modify_package_header(thread_db*, SSHORT, DeferredWork*, jrd_tra*);  
static bool drop_package_body(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool grant_privileges(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool db_crypt(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool set_linger(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool clear_cache(thread_db*, SSHORT, DeferredWork*, jrd_tra*);
static bool change_repl_state(thread_db*, SSHORT, DeferredWork*, jrd_tra*);

// Utility function declarations
static string remove_icu_info_from_attributes(const QualifiedName&, const string&);
static bool create_expression_index(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction);
static void check_computed_dependencies(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& fieldName);
static void check_dependencies(thread_db*, const QualifiedName&, const MetaName&, int, jrd_tra*);
static void check_filename(const ScratchBird::string&, bool);
static void cleanup_index_creation(thread_db*, DeferredWork*, jrd_tra*);
static bool formatsAreEqual(const Format*, const Format*);
static bool	find_depend_in_dfw(thread_db*, const QualifiedName&, USHORT, USHORT, jrd_tra*);
static void get_array_desc(thread_db*, const TEXT*, Ods::InternalArrayDesc*);
static void get_trigger_dependencies(DeferredWork*, bool, jrd_tra*);
static Format*	make_format(thread_db*, jrd_rel*, USHORT *, TemporaryField*);
static void put_summary_blob(thread_db* tdbb, blb*, enum rsr_t, bid*, jrd_tra*);
static void put_summary_record(thread_db* tdbb, blb*, enum rsr_t, const UCHAR*, ULONG);
static void	setup_array(thread_db*, blb*, const TEXT*, USHORT, TemporaryField*);
static blb*	setup_triggers(thread_db*, jrd_rel*, bool, TrigVector**, blb*);
static void	setup_trigger_details(thread_db*, jrd_rel*, blb*, TrigVector**, const QualifiedName&, bool);
static bool validate_text_type (thread_db*, const TemporaryField*);
static void check_partners(thread_db*, const USHORT);
static string get_string(const dsc* desc);
static void setupSpecificCollationAttributes(thread_db*, jrd_tra*, const USHORT, const QualifiedName&, bool);

// Main DFW API functions

USHORT DFW_assign_index_type(thread_db* tdbb, const QualifiedName& name, SSHORT field_type,
	SSHORT ttype)
{
/**************************************
 *
 *	D F W _ a s s i g n _ i n d e x _ t y p e
 *
 **************************************
 *
 * Functional description
 *	Define the index segment type based
 * 	on the field's type and subtype.
 *
 **************************************/
	SET_TDBB(tdbb);

	if (field_type == dtype_varying || field_type == dtype_cstring || field_type == dtype_text)
	{
	    switch (ttype)
	    {
		case ttype_none:
			return idx_string;
		case ttype_binary:
			return idx_byte_array;
		case ttype_metadata:
			return idx_metadata;
		case ttype_ascii:
			return idx_string;
		}

		// Dynamic text cannot occur here as this is for an on-disk
		// index, which must be bound to a text type.

		fb_assert(ttype != ttype_dynamic);

		if (INTL_defined_type(tdbb, ttype))
			return INTL_TEXT_TO_INDEX(ttype);

		ERR_post_nothrow(Arg::Gds(isc_no_meta_update) <<
						 Arg::Gds(isc_random) << name.toQuotedString());
		INTL_texttype_lookup(tdbb, ttype);	// should punt
		ERR_punt(); // if INTL_texttype_lookup hasn't punt
	}

	switch (field_type)
	{
	case dtype_timestamp:
		return idx_timestamp;
	case dtype_timestamp_tz:
		return idx_timestamp_tz;
	case dtype_sql_date:
		return idx_sql_date;
	case dtype_sql_time:
		return idx_sql_time;
	case dtype_sql_time_tz:
		return idx_sql_time_tz;
	// idx_numeric2 used for 64-bit Integer support
	case dtype_int64:
		return idx_numeric2;
	case dtype_boolean:
		return idx_boolean;
	case dtype_dec64:
	case dtype_dec128:
		return idx_decimal;
	case dtype_int128:
		return tdbb->getDatabase()->getEncodedOdsVersion() >= ODS_13_1 ? idx_bcd : idx_decimal;
	default:
		return idx_numeric;
	}
}

void DFW_delete_deferred(jrd_tra* transaction, SavNumber sav_number)
{
/**************************************
 *
 *	D F W _ d e l e t e _ d e f e r r e d
 *
 **************************************
 *
 * Functional description
 *	Get rid of work deferred that was to be done at
 *	COMMIT time as the statement has been rolled back.
 *
 *	if (sav_number == -1), then  remove all entries.
 *
 **************************************/

	// If there is no deferred work, just return

	if (!transaction->tra_deferred_job) {
		return;
	}

	// Remove deferred work and events which are to be rolled back

	if (sav_number == -1)
	{
		DeferredWork* work;
		while ((work = transaction->tra_deferred_job->work))
		{
			delete work;
		}
		transaction->tra_flags &= ~TRA_deferred_meta;
		return;
	}

	DfwSavePoint* h = transaction->tra_deferred_job->hash.lookup(sav_number);
	if (!h)
	{
		return;
	}

	for (DfwHash::iterator i(h->hash); i.hasData();)
	{
		DeferredWork* work(i);
		++i;
		delete work;
	}
}

// Get (by reference) the array of IDs present in a DeferredWork.
SortedArray<int>& DFW_get_ids(DeferredWork* work)
{
	return work->dfw_ids;
}

void DFW_merge_work(jrd_tra* transaction, SavNumber old_sav_number, SavNumber new_sav_number)
{
/**************************************
 *
 *	D F W _ m e r g e _ w o r k
 *
 **************************************
 *
 * Functional description
 *	Merge the deferred work with the previous level.  This will
 *	be called only if there is a previous level.
 *
 **************************************/

	// If there is no deferred work, just return

	DeferredJob *job = transaction->tra_deferred_job;
	if (! job)
		return;

	// Check to see if work is already posted

	DfwSavePoint* oldSp = job->hash.lookup(old_sav_number);
	if (!oldSp)
		return;

	DfwSavePoint* newSp = job->hash.lookup(new_sav_number);

	// Decrement the save point number in the deferred block
	// i.e. merge with the previous level.

	for (DfwHash::iterator itr(oldSp->hash); itr.hasData();)
	{
		if (! newSp)
		{
			newSp = FB_NEW_POOL(*transaction->tra_pool) DfwSavePoint(new_sav_number);
			job->hash.add(newSp);
		}

		DeferredWork* work(itr);
		++itr;
		oldSp->hash.remove(*work); // After ++itr
		work->dfw_sav_number = new_sav_number;

		DeferredWork* newWork = newSp->hash.lookup(*work);

		if (!newWork)
			newSp->hash.add(work);
		else
		{
			SortedArray<int>& workIds = work->dfw_ids;

			for (SortedArray<int>::iterator itr2(workIds.begin()); itr2 != workIds.end(); ++itr2)
			{
				int n = *itr2;
				if (!newWork->dfw_ids.exist(n))
					newWork->dfw_ids.add(n);
			}

			newWork->dfw_count += work->dfw_count;
			delete work;
		}
	}

	job->hash.remove(old_sav_number);
	delete oldSp;
}

void DFW_perform_work(thread_db* tdbb, jrd_tra* transaction)
{
/**************************************
 *
 *	D F W _ p e r f o r m _ w o r k
 *
 **************************************
 *
 * Functional description
 *	Do work deferred to COMMIT time 'cause that time has
 *	come.
 *
 **************************************/

	// If no deferred work or it's all deferred event posting don't bother

	if (!transaction->tra_deferred_job || !(transaction->tra_flags & TRA_deferred_meta))
	{
		return;
	}

	SET_TDBB(tdbb);

	tdbb->getAttachment()->att_dsql_instance->dbb_statement_cache->purgeAllAttachments(tdbb);

	Jrd::ContextPoolHolder context(tdbb, transaction->tra_pool);

	/* Loop for as long as any of the deferred work routines says that it has
	more to do.  A deferred work routine should be able to deal with any
	value of phase, either to say that it wants to be called again in the
	next phase (by returning true) or that it has nothing more to do in this
	or later phases (by returning false). By convention, phase 0 has been
	designated as the cleanup phase. If any non-zero phase punts, then phase 0
	is executed for all deferred work blocks to cleanup work-in-progress. */

	bool dump_shadow = false;
	SSHORT phase = 1;
	bool more;
	FbLocalStatus err_status;

	do
	{
		more = false;
		try {
			const auto flags = (TDBB_dont_post_dfw | TDBB_use_db_page_space |
				(phase == 0 ? TDBB_dfw_cleanup : 0));
			AutoSetRestoreFlag<ULONG> dfwFlags(&tdbb->tdbb_flags, flags, true);

			for (const deferred_task* task = task_table; task->task_type != dfw_null; ++task)
			{
				for (DeferredWork* work = transaction->tra_deferred_job->work;
					work; work = work->getNext())
				{
					if (work->dfw_type == task->task_type)
					{
						if (work->dfw_type == dfw_add_shadow)
						{
							dump_shadow = true;
						}
						if ((*task->task_routine)(tdbb, phase, work, transaction))
						{
							more = true;
						}
					}
				}
			}

			if (!phase)
			{
				fb_utils::copyStatus(tdbb->tdbb_status_vector, &err_status);
				ERR_punt();
			}

			++phase;
		}
		catch (const ScratchBird::Exception& ex)
		{
			// Do any necessary cleanup
			if (!phase)
			{
				ex.stuffException(tdbb->tdbb_status_vector);
				ERR_punt();
			}
			else
				ex.stuffException(&err_status);

			phase = 0;
			more = true;
		}

	} while (more);

	// Remove deferred work blocks so that system transaction and
	// commit retaining transactions don't re-execute them. Leave
	// events to be posted after commit

	for (DeferredWork* itr = transaction->tra_deferred_job->work; itr;)
	{
		DeferredWork* work = itr;
		itr = itr->getNext();

		switch (work->dfw_type)
		{
		case dfw_post_event:
		case dfw_delete_shadow:
			break;

		default:
			delete work;
			break;
		}
	}

	transaction->tra_flags &= ~TRA_deferred_meta;

	if (dump_shadow) {
		SDW_dump_pages(tdbb);
	}
}

void DFW_perform_post_commit_work(jrd_tra* transaction)
{
/**************************************
 *
 *	D F W _ p e r f o r m _ p o s t _ c o m m i t _ w o r k
 *
 **************************************
 *
 * Functional description
 *	Perform any post commit work
 *	1. Post any pending events.
 *	2. Unlink shadow files for dropped shadows
 *
 *	Then, delete it from chain of pending work.
 *
 **************************************/

	if (!transaction->tra_deferred_job)
		return;

	bool pending_events = false;

	Database* dbb = GET_DBB();

	for (DeferredWork* itr = transaction->tra_deferred_job->work; itr;)
	{
		DeferredWork* work = itr;
		itr = itr->getNext();

		switch (work->dfw_type)
		{
		case dfw_post_event:
			EventManager::init(transaction->tra_attachment);

			dbb->eventManager()->postEvent(work->dfw_name.length(),
										   work->dfw_name.c_str(),
										   work->dfw_count);

			delete work;
			pending_events = true;
			break;
		case dfw_delete_shadow:
			if (work->dfw_name.hasData())
				unlink(work->dfw_name.c_str());
			delete work;
			break;
		default:
			break;
		}
	}

	if (pending_events)
	{
		dbb->eventManager()->deliverEvents();
	}
}

DeferredWork* DFW_post_work(jrd_tra* transaction, enum dfw_t type, const dsc* desc, const dsc* schemaDesc, USHORT id,
	const MetaName& package)
{
/**************************************
 *
 *	D F W _ p o s t _ w o r k
 *
 **************************************
 *
 * Functional description
 *	Post work to be deferred to commit time.
 *
 **************************************/

	return DFW_post_work(transaction, type, get_string(desc), get_string(schemaDesc), id, package);
}

DeferredWork* DFW_post_work(jrd_tra* transaction, enum dfw_t type, const string& name, const MetaName& schema,
	USHORT id, const MetaName& package)
{
/**************************************
 *
 *	D F W _ p o s t _ w o r k
 *
 **************************************
 *
 * Functional description
 *	Post work to be deferred to commit time.
 *
 **************************************/

	// get the current save point number

	const SavNumber sav_number = transaction->tra_save_point ?
		transaction->tra_save_point->getNumber() : 0;

	// initialize transaction if needed

	DeferredJob *job = transaction->tra_deferred_job;
	if (! job)
	{
		transaction->tra_deferred_job = job = FB_NEW_POOL(*transaction->tra_pool) DeferredJob;
	}

	// Check to see if work is already posted

	DfwSavePoint* sp = job->hash.lookup(sav_number);
	if (! sp)
	{
		sp = FB_NEW_POOL(*transaction->tra_pool) DfwSavePoint(sav_number);
		job->hash.add(sp);
	}

	DeferredWork tmp(AutoStorage::getAutoMemoryPool(), 0, type, id, sav_number, name, schema, package);
	DeferredWork* work = sp->hash.lookup(tmp);
	if (work)
	{
		work->dfw_count++;
		return work;
	}

	// Not already posted, so do so now.

	work = FB_NEW_POOL(*transaction->tra_pool)
		DeferredWork(*transaction->tra_pool, &(job->end), type, id, sav_number, name, schema, package);
	job->end = work->getNextPtr();
	fb_assert(!(*job->end));
	sp->hash.add(work);

	switch (type)
	{
	case dfw_user_management:
	case dfw_set_generator:
		transaction->tra_flags |= TRA_deferred_meta;
		// fall down ...
	case dfw_post_event:
		if (transaction->tra_save_point)
			transaction->tra_save_point->forceDeferredWork();
		break;
	default:
		transaction->tra_flags |= TRA_deferred_meta;
		break;
	}

	return work;
}

DeferredWork* DFW_post_work_arg(jrd_tra* transaction, DeferredWork* work, const dsc* nameDesc, const dsc* schemaDesc,
	USHORT id)
{
/**************************************
 *
 *	D F W _ p o s t _ w o r k _ a r g
 *
 **************************************
 *
 * Functional description
 *	Post an argument for work to be deferred to commit time.
 *
 **************************************/
	return DFW_post_work_arg(transaction, work, nameDesc, schemaDesc, id, work->dfw_type);
}

DeferredWork* DFW_post_work_arg( jrd_tra* transaction, DeferredWork* work, const dsc* nameDesc, const dsc* schemaDesc,
	USHORT id, Jrd::dfw_t type)
{
/**************************************
 *
 *	D F W _ p o s t _ w o r k _ a r g
 *
 **************************************
 *
 * Functional description
 *	Post an argument for work to be deferred to commit time.
 *
 **************************************/
	DeferredWork* arg = work->findArg(type);

	if (! arg)
	{
		const auto schema = get_string(schemaDesc);
		const auto name = get_string(nameDesc);
		arg = FB_NEW_POOL(*transaction->tra_pool)
			DeferredWork(*transaction->tra_pool, 0, type, id, 0, name, schema, {});
		work->dfw_args.add(arg);
	}

	return arg;
}

void DFW_update_index(const QualifiedName& name, USHORT id, const SelectivityList& selectivity,
	jrd_tra* transaction)
{
/**************************************
 *
 *	D F W _ u p d a t e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Update information in the index relation after creation
 *	of the index.
 *
 **************************************/
	thread_db* tdbb = JRD_get_thread_data();

	AutoCacheRequest request(tdbb, irq_m_index_seg, IRQ_REQUESTS);

	// Converted FOR loop #7: Update RDB$INDEX_SEGMENTS with selectivity stats
	jrd_req* jrdRequest = NULL;
	try {
		jrdRequest = EXE_find_request(tdbb, request.getRequest(), false);
		EXE_start(tdbb, jrdRequest, transaction);

		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT index_name[MAX_SQL_IDENTIFIER_LEN];
		} segInput;

		name.schema.copyTo(segInput.schema_name, sizeof(segInput.schema_name));
		name.object.copyTo(segInput.index_name, sizeof(segInput.index_name));

		EXE_send(tdbb, jrdRequest, 0, sizeof(segInput), 
			reinterpret_cast<const UCHAR*>(&segInput));

		struct {
			SSHORT field_position;
			double statistics;
		} segData;

		while (EXE_receive(tdbb, jrdRequest, 1, sizeof(segData), 
			reinterpret_cast<UCHAR*>(&segData))) {
			
			// Converted MODIFY operation #3: Update segment statistics
			segData.statistics = selectivity[segData.field_position];
			EXE_send(tdbb, jrdRequest, 2, sizeof(segData.statistics), 
				reinterpret_cast<const UCHAR*>(&segData.statistics));
		}
	}
	catch (...) {
		if (jrdRequest) EXE_unwind(tdbb, jrdRequest);
		throw;
	}
	if (jrdRequest) EXE_unwind(tdbb, jrdRequest);

	request.reset(tdbb, irq_m_index, IRQ_REQUESTS);

	// Converted FOR loop #8: Update RDB$INDICES with index ID and statistics
	jrdRequest = NULL;
	try {
		jrdRequest = EXE_find_request(tdbb, request.getRequest(), false);
		EXE_start(tdbb, jrdRequest, transaction);

		struct {
			TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
			TEXT index_name[MAX_SQL_IDENTIFIER_LEN];
		} idxInput;

		name.schema.copyTo(idxInput.schema_name, sizeof(idxInput.schema_name));
		name.object.copyTo(idxInput.index_name, sizeof(idxInput.index_name));

		EXE_send(tdbb, jrdRequest, 0, sizeof(idxInput), 
			reinterpret_cast<const UCHAR*>(&idxInput));

		UCHAR response[100];
		if (EXE_receive(tdbb, jrdRequest, 1, sizeof(response), response)) {
			// Converted MODIFY operation #4: Update index ID and statistics
			struct {
				SSHORT index_id;
				double statistics;
			} idxData;
			
			idxData.index_id = id + 1;
			idxData.statistics = selectivity.back();
			
			EXE_send(tdbb, jrdRequest, 2, sizeof(idxData), 
				reinterpret_cast<const UCHAR*>(&idxData));
		}
	}
	catch (...) {
		if (jrdRequest) EXE_unwind(tdbb, jrdRequest);
		throw;
	}
	if (jrdRequest) EXE_unwind(tdbb, jrdRequest);
}

// Error handling utility functions

static ISC_STATUS getErrorCodeByObjectType(int obj_type)
{
	ISC_STATUS err_code = 0;

	switch (obj_type)
	{
	case obj_relation:
		err_code = isc_table_name;
		break;
	case obj_view:
		err_code = isc_view_name;
		break;
	case obj_procedure:
		err_code = isc_proc_name;
		break;
	case obj_collation:
		err_code = isc_collation_name;
		break;
	case obj_exception:
		err_code = isc_exception_name;
		break;
	case obj_field:
		err_code = isc_domain_name;
		break;
	case obj_generator:
		err_code = isc_generator_name;
		break;
	case obj_udf:
		err_code = isc_udf_name;
		break;
	case obj_index:
		err_code = isc_index_name;
		break;
	case obj_package_header:
	case obj_package_body:
		err_code = isc_package_name;
		break;
	default:
		fb_assert(false);
	}

	return err_code;
}

static void raiseDatabaseInUseError(bool timeout)
{
	if (timeout)
	{
		ERR_post(Arg::Gds(isc_no_meta_update) <<
				 Arg::Gds(isc_lock_timeout) <<
				 Arg::Gds(isc_obj_in_use) << Arg::Str("DATABASE"));
	}

	ERR_post(Arg::Gds(isc_no_meta_update) <<
			 Arg::Gds(isc_obj_in_use) << Arg::Str("DATABASE"));
}

static void raiseObjectInUseError(const string& obj_type, const QualifiedName& obj_name)
{
	string name;
	name.printf("%s %s", obj_type.c_str(), obj_name.toQuotedString().c_str());

	ERR_post(Arg::Gds(isc_no_meta_update) <<
			 Arg::Gds(isc_obj_in_use) << name);
}

static void raiseRelationInUseError(const jrd_rel* relation)
{
	raiseObjectInUseError("TABLE", QualifiedName(relation->rel_name, relation->rel_owner_name));
}

static string get_string(const dsc* desc)
{
	if (!desc || (desc->dsc_flags & DSC_null))
		return string();

	return string(reinterpret_cast<const char*>(desc->dsc_address), desc->dsc_length);
}

// Core deferred work implementations with GPRE conversions

static bool add_shadow(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	a d d _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *	A file or files have been added for shadowing.
 *	Get all files for this particular shadow first
 *	in order of starting page, if specified, then
 *	in sequence order.
 *
 **************************************/

	AutoRequest handle;
	Shadow* shadow;
	bool finished;
	ScratchBird::PathName expanded_fname;

	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	switch (phase)
	{
	case 0:
		CCH_release_exclusive(tdbb);
		return false;

	case 1:
	case 2:
	case 3:
		return true;

	case 4:
		check_filename(work->dfw_name.c_str(), false);

		/* could have two cases:
		   1) this shadow has already been written to, so add this file using
		   the standard routine to extend a database
		   2) this file is part of a newly added shadow which has already been
		   fetched in totem and prepared for writing to, so just ignore it
		 */

		finished = false;
		handle.reset();
		
		// Converted FOR loop #9: Query RDB$FILES for shadow file processing
		jrd_req* request = NULL;
		try {
			request = EXE_find_request(tdbb, handle.getRequest(), false);
			EXE_start(tdbb, request, transaction);

			struct {
				TEXT file_name[MAXPATHLEN];
			} fileInput;
			
			work->dfw_name.copyTo(fileInput.file_name, sizeof(fileInput.file_name));
			EXE_send(tdbb, request, 0, sizeof(fileInput), 
				reinterpret_cast<const UCHAR*>(&fileInput));

			struct {
				TEXT file_name[MAXPATHLEN];
				SSHORT shadow_number;
				SSHORT file_flags;
			} fileData;

			if (EXE_receive(tdbb, request, 1, sizeof(fileData), 
				reinterpret_cast<UCHAR*>(&fileData))) {
				
				expanded_fname = fileData.file_name;
				ISC_expand_filename(expanded_fname, false);

				// Converted MODIFY operation #5: Update expanded filename
				expanded_fname.copyTo(fileData.file_name, sizeof(fileData.file_name));
				EXE_send(tdbb, request, 2, sizeof(fileData.file_name), 
					reinterpret_cast<const UCHAR*>(&fileData.file_name));

				for (shadow = dbb->dbb_shadow; shadow; shadow = shadow->sdw_next)
				{
					if ((fileData.shadow_number == shadow->sdw_number) && !(shadow->sdw_flags & SDW_IGNORE))
					{
						if (!(fileData.file_flags & FILE_shadow))
						{
							// We cannot add a file to a shadow that is still
							// in the process of being created.
							raiseDatabaseInUseError(false);
						}

						// This is the case of a bogus duplicate posted
						// work when we added a multi-file shadow
						finished = true;
						break;
					}
				}
			}
		}
		catch (...) {
			if (request) EXE_unwind(tdbb, request);
			throw;
		}
		if (request) EXE_unwind(tdbb, request);

		if (finished)
			return false;

		handle.reset();
		
		// Converted FOR loop #10: Process expanded filename for shadow creation
		request = NULL;
		try {
			request = EXE_find_request(tdbb, handle.getRequest(), false);
			EXE_start(tdbb, request, transaction);

			struct {
				TEXT file_name[MAXPATHLEN];
			} expandedInput;
			
			expanded_fname.copyTo(expandedInput.file_name, sizeof(expandedInput.file_name));
			EXE_send(tdbb, request, 0, sizeof(expandedInput), 
				reinterpret_cast<const UCHAR*>(&expandedInput));

			struct {
				TEXT file_name[MAXPATHLEN];
				SSHORT shadow_number;
				SSHORT file_flags;
			} shadowData;

			if (EXE_receive(tdbb, request, 1, sizeof(shadowData), 
				reinterpret_cast<UCHAR*>(&shadowData))) {
				
				SDW_add(tdbb, shadowData.file_name, shadowData.shadow_number, shadowData.file_flags);

				// Converted MODIFY operation #6: Set FILE_shadow flag
				shadowData.file_flags |= FILE_shadow;
				EXE_send(tdbb, request, 2, sizeof(shadowData.file_flags), 
					reinterpret_cast<const UCHAR*>(&shadowData.file_flags));
			}
		}
		catch (...) {
			if (request) EXE_unwind(tdbb, request);
			throw;
		}
		if (request) EXE_unwind(tdbb, request);

		break;
	}

	return false;
}

static bool add_difference(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra*)
{
/**************************************
 *
 *	a d d _ d i f f e r e n c e
 *
 **************************************
 *
 * Functional description
 *	Add backup difference file to the database
 *
 **************************************/

	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		{
			BackupManager::StateReadGuard stateGuard(tdbb);
			if (dbb->dbb_backup_manager->getState() != Ods::hdr_nbak_normal)
			{
				ERR_post(Arg::Gds(isc_no_meta_update) <<
						 Arg::Gds(isc_wrong_backup_state));
			}
			check_filename(work->dfw_name.c_str(), true);
			dbb->dbb_backup_manager->setDifference(tdbb, work->dfw_name.c_str());
		}
		break;
	}

	return false;
}

static bool delete_difference(thread_db* tdbb, SSHORT phase, DeferredWork*, jrd_tra*)
{
/**************************************
 *
 *	d e l e t e _ d i f f e r e n c e
 *
 **************************************
 *
 * Delete backup difference file for database
 *
 **************************************/

	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		{
			BackupManager::StateReadGuard stateGuard(tdbb);

			if (dbb->dbb_backup_manager->getState() != Ods::hdr_nbak_normal)
			{
				ERR_post(Arg::Gds(isc_no_meta_update) <<
						 Arg::Gds(isc_wrong_backup_state));
			}
			dbb->dbb_backup_manager->setDifference(tdbb, NULL);
		}
		break;
	}

	return false;
}

static bool begin_backup(thread_db* tdbb, SSHORT phase, DeferredWork*, jrd_tra*)
{
/**************************************
 *
 *	b e g i n _ b a c k u p
 *
 **************************************
 *
 * Begin backup storing changed pages in difference file
 *
 **************************************/

	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		dbb->dbb_backup_manager->beginBackup(tdbb);
		break;
	}

	return false;
}

static bool end_backup(thread_db* tdbb, SSHORT phase, DeferredWork*, jrd_tra*)
{
/**************************************
 *
 *	e n d _ b a c k u p
 *
 **************************************
 *
 * End backup and merge difference file if neseccary
 *
 **************************************/

	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		// End backup normally
		dbb->dbb_backup_manager->endBackup(tdbb, false);
		break;
	}

	return false;
}

static bool db_crypt(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra*)
{
/**************************************
 *
 *	d b _ c r y p t
 *
 **************************************
 *
 * Encrypt database using plugin dfw_name or decrypt if dfw_name is empty.
 *
 **************************************/

	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		dbb->dbb_crypto_manager->changeCryptState(tdbb, work->dfw_name);
		break;
	}

	return false;
}

static bool set_linger(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra*)
{
/**************************************
 *
 *	s e t _ l i n g e r
 *
 **************************************
 *
 * Set linger interval in Database block.
 *
 **************************************/

	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	switch (phase)
	{
	case 1:
	case 2:
	case 3:
		return true;

	case 4:
		dbb->dbb_linger_seconds = atoi(work->dfw_name.c_str());		// number stored as string
		break;
	}

	return false;
}

static bool clear_cache(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra*)
{
/**************************************
 *
 *	c l e a r _ c a c h e
 *
 **************************************
 *
 * Clear security names mapping cache
 *
 **************************************/

	SET_TDBB(tdbb);
	Database* const dbb = tdbb->getDatabase();

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		Mapping::clearCache(dbb->dbb_filename.c_str(), work->dfw_id);
		break;
	}

	return false;
}

static bool check_not_null(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	c h e c k _ n o t _ n u l l
 *
 **************************************
 *
 * Functional description
 *	Scan relation to detect NULLs in fields being changed to NOT NULL.
 *
 **************************************/

	SET_TDBB(tdbb);

	Jrd::Attachment* attachment = tdbb->getAttachment();

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		{
			jrd_rel* relation = MET_lookup_relation(tdbb, work->getQualifiedName());
			if (!relation || relation->rel_view_rse || work->dfw_ids.isEmpty())
				break;

			// Protect relation from modification
			ProtectRelations protectRelation(tdbb, transaction, relation);

			SortedArray<int> fields;
			AutoRequest handle;

			for (SortedArray<int>::iterator itr(work->dfw_ids.begin());
				 itr != work->dfw_ids.end();
				 ++itr)
			{
				// Converted FOR loop #11: Query RDB$RELATION_FIELDS and RDB$FIELDS for NOT NULL validation
				jrd_req* request = NULL;
				try {
					request = EXE_find_request(tdbb, handle.getRequest(), false);
					EXE_start(tdbb, request, transaction);

					struct {
						TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
						TEXT relation_name[MAX_SQL_IDENTIFIER_LEN];
						TEXT field_source_schema[MAX_SQL_IDENTIFIER_LEN];
						TEXT field_source[MAX_SQL_IDENTIFIER_LEN];
						SSHORT field_id;
					} fieldInput;

					work->dfw_schema.copyTo(fieldInput.schema_name, sizeof(fieldInput.schema_name));
					work->dfw_name.copyTo(fieldInput.relation_name, sizeof(fieldInput.relation_name));
					fieldInput.field_id = *itr;

					EXE_send(tdbb, request, 0, sizeof(fieldInput), 
						reinterpret_cast<const UCHAR*>(&fieldInput));

					struct {
						SSHORT field_id;
						SSHORT rel_null_flag;
						SSHORT fld_null_flag;
					} fieldData;

					if (EXE_receive(tdbb, request, 1, sizeof(fieldData), 
						reinterpret_cast<UCHAR*>(&fieldData))) {
						
						if (fieldData.rel_null_flag == TRUE || fieldData.fld_null_flag == TRUE) {
							fields.add(fieldData.field_id);
						}
					}
				}
				catch (...) {
					if (request) EXE_unwind(tdbb, request);
					throw;
				}
				if (request) EXE_unwind(tdbb, request);
			}

			if (fields.hasData())
			{
				UCharBuffer blr;

				blr.add(blr_version5);
				blr.add(blr_begin);
				blr.add(blr_message);
				blr.add(1);	// message number
				blr.add(fields.getCount() & 0xFF);
				blr.add(fields.getCount() >> 8);

				for (FB_SIZE_T i = 0; i < fields.getCount(); ++i)
				{
					blr.add(blr_short);
					blr.add(0);
				}

				blr.add(blr_for);
				blr.add(blr_stall);
				blr.add(blr_rse);
				blr.add(1);
				blr.add(blr_rid);
				blr.add(relation->rel_id & 0xFF);
				blr.add(relation->rel_id >> 8);
				blr.add(0);	// stream
				blr.add(blr_boolean);

				for (FB_SIZE_T i = 0; i < fields.getCount(); ++i)
				{
					if (i != fields.getCount() - 1)
						blr.add(blr_or);

					blr.add(blr_missing);
					blr.add(blr_fid);
					blr.add(0);	// stream
					blr.add(USHORT(fields[i]) & 0xFF);
					blr.add(USHORT(fields[i]) >> 8);
				}

				blr.add(blr_end);

				blr.add(blr_send);
				blr.add(1);
				blr.add(blr_begin);

				for (FB_SIZE_T i = 0; i < fields.getCount(); ++i)
				{
					blr.add(blr_assignment);

					blr.add(blr_value_if);
					blr.add(blr_missing);
					blr.add(blr_fid);
					blr.add(0);	// stream
					blr.add(USHORT(fields[i]) & 0xFF);
					blr.add(USHORT(fields[i]) >> 8);

					blr.add(blr_literal);
					blr.add(blr_short);
					blr.add(0);
					blr.add(1);
					blr.add(0);

					blr.add(blr_literal);
					blr.add(blr_short);
					blr.add(0);
					blr.add(0);
					blr.add(0);

					blr.add(blr_parameter);
					blr.add(1);	// message number
					blr.add(i & 0xFF);
					blr.add(i >> 8);
				}

				blr.add(blr_end);

				blr.add(blr_send);
				blr.add(1);
				blr.add(blr_begin);

				for (FB_SIZE_T i = 0; i < fields.getCount(); ++i)
				{
					blr.add(blr_assignment);
					blr.add(blr_literal);
					blr.add(blr_short);
					blr.add(0);
					blr.add(0);
					blr.add(0);
					blr.add(blr_parameter);
					blr.add(1);	// message number
					blr.add(i & 0xFF);
					blr.add(i >> 8);
				}

				blr.add(blr_end);
				blr.add(blr_end);
				blr.add(blr_eoc);

				AutoRequest request;
				request.compile(tdbb, blr.begin(), blr.getCount());

				HalfStaticArray<USHORT, 5> hasRecord;

				EXE_start(tdbb, request, transaction);
				EXE_receive(tdbb, request, 1, fields.getCount() * sizeof(USHORT),
					(UCHAR*) hasRecord.getBuffer(fields.getCount()));

				Arg::Gds errs(isc_no_meta_update);
				bool hasError = false;

				for (FB_SIZE_T i = 0; i < fields.getCount(); ++i)
				{
					if (hasRecord[i])
					{
						hasError = true;
						errs << Arg::Gds(isc_cannot_make_not_null) <<
								(*relation->rel_fields)[fields[i]]->fld_name.toQuotedString() <<
								relation->rel_name.toQuotedString();
					}
				}

				if (hasError)
					ERR_post(errs);
			}
		}

		break;
	}

	return false;
}

// Store RDB$CONTEXT_TYPE in RDB$VIEW_RELATIONS when restoring legacy backup.
static bool store_view_context_type(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
	SET_TDBB(tdbb);

	switch (phase)
	{
	case 1:
		{
			// If RDB$PACKAGE_NAME IS NOT NULL or no record is found in RDB$RELATIONS,
			// the context is a procedure;
			ViewContextType vct = VCT_PROCEDURE;

			// Converted FOR loop #12: Query RDB$VIEW_RELATIONS and RDB$RELATIONS for context type
			jrd_req* request1 = NULL;
			try {
				AutoRequest handle1;
				request1 = EXE_find_request(tdbb, handle1.getRequest(), false);
				EXE_start(tdbb, request1, transaction);

				struct {
					TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
					TEXT view_name[MAX_SQL_IDENTIFIER_LEN];
					SSHORT view_context;
				} viewInput;

				work->dfw_schema.copyTo(viewInput.schema_name, sizeof(viewInput.schema_name));
				work->dfw_name.copyTo(viewInput.view_name, sizeof(viewInput.view_name));
				viewInput.view_context = work->dfw_id;

				EXE_send(tdbb, request1, 0, sizeof(viewInput), 
					reinterpret_cast<const UCHAR*>(&viewInput));

				struct {
					bid view_blr;
					SSHORT is_null;
				} relData;

				if (EXE_receive(tdbb, request1, 1, sizeof(relData),
					reinterpret_cast<UCHAR*>(&relData))) {
					vct = (relData.is_null ? VCT_TABLE : VCT_VIEW);
				}
			}
			catch (...) {
				if (request1) EXE_unwind(tdbb, request1);
				throw;
			}
			if (request1) EXE_unwind(tdbb, request1);

			// Converted FOR loop #13: Update RDB$VIEW_RELATIONS with context type
			jrd_req* request2 = NULL;
			try {
				AutoRequest handle2;
				request2 = EXE_find_request(tdbb, handle2.getRequest(), false);
				EXE_start(tdbb, request2, transaction);

				struct {
					TEXT schema_name[MAX_SQL_IDENTIFIER_LEN];
					TEXT view_name[MAX_SQL_IDENTIFIER_LEN];
					SSHORT view_context;
				} updateInput;

				work->dfw_schema.copyTo(updateInput.schema_name, sizeof(updateInput.schema_name));
				work->dfw_name.copyTo(updateInput.view_name, sizeof(updateInput.view_name));
				updateInput.view_context = work->dfw_id;

				EXE_send(tdbb, request2, 0, sizeof(updateInput), 
					reinterpret_cast<const UCHAR*>(&updateInput));

				UCHAR response[100];
				if (EXE_receive(tdbb, request2, 1, sizeof(response), response)) {
					// Converted MODIFY operation #7: Update context type
					struct {
						SSHORT context_type;
						SSHORT context_type_null;
					} updateData;
					
					updateData.context_type_null = FALSE;
					updateData.context_type = (SSHORT) vct;
					
					EXE_send(tdbb, request2, 2, sizeof(updateData), 
						reinterpret_cast<const UCHAR*>(&updateData));
				}
			}
			catch (...) {
				if (request2) EXE_unwind(tdbb, request2);
				throw;
			}
			if (request2) EXE_unwind(tdbb, request2);
		}
		break;
	}

	return false;
}

static bool user_management(thread_db* /*tdbb*/, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	u s e r _ m a n a g e m e n t
 *
 **************************************
 *
 * Functional description
 *	Commit in security database
 *
 **************************************/

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		transaction->getUserManagement()->execute(work->dfw_id);
		return true;

	case 4:
		transaction->getUserManagement()->commit();	// safe to be called multiple times
		break;
	}

	return false;
}

// Drop dependencies of a package header.
static bool drop_package_header(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
	SET_TDBB(tdbb);

	switch (phase)
	{
	case 1:
		MET_delete_dependencies(tdbb, work->getQualifiedName(), obj_package_body, transaction);
		MET_delete_dependencies(tdbb, work->getQualifiedName(), obj_package_header, transaction);
		break;
	}

	return false;
}

// Drop dependencies of a package header.
static bool modify_package_header(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
	SET_TDBB(tdbb);

	switch (phase)
	{
	case 1:
		MET_delete_dependencies(tdbb, work->getQualifiedName(), obj_package_header, transaction);
		break;
	}

	return false;
}

// Drop dependencies of a package body.
static bool drop_package_body(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
	SET_TDBB(tdbb);

	switch (phase)
	{
	case 1:
		MET_delete_dependencies(tdbb, work->getQualifiedName(), obj_package_body, transaction);
		break;
	}

	return false;
}

static bool grant_privileges(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	g r a n t _ p r i v i l e g e s
 *
 **************************************
 *
 * Functional description
 *	Compute access control list from SQL privileges.
 *
 **************************************/
	switch (phase)
	{
	case 1:
		return true;

	case 2:
		GRANT_privileges(tdbb, QualifiedName(work->dfw_name, work->dfw_schema), work->dfw_id, transaction);
		break;

	default:
		break;
	}

	return false;
}

// Converted from GPRE: create_expression_index - Create a new expression index
static bool create_expression_index(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	c r e a t e  _ e x p r e s s i o n _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Create a new expression index.
 *
 **************************************/
	switch (phase)
	{
	case 0:
		cleanup_index_creation(tdbb, work, transaction);
		MET_delete_dependencies(tdbb, work->getQualifiedName(), obj_index_expression, transaction);
		MET_delete_dependencies(tdbb, work->getQualifiedName(), obj_index_condition, transaction);
		return false;

	case 1:
	case 2:
		return true;

	case 3:
		{
			jrd_rel* relation = nullptr;
			CompilerScratch* csb = nullptr;

			const auto dbb = tdbb->getDatabase();
			const auto attachment = tdbb->getAttachment();

			index_desc idx;
			MOVE_CLEAR(&idx, sizeof(index_desc));

			AutoRequest request;

			// FOR RDB$INDICES IDX CROSS RDB$RELATIONS REL
			request.compile(tdbb, reinterpret_cast<const UCHAR*>(
				"FOR IDX IN RDB$INDICES "
				"CROSS REL IN RDB$RELATIONS OVER RDB$SCHEMA_NAME, RDB$RELATION_NAME "
				"WITH IDX.RDB$EXPRESSION_BLR NOT MISSING AND "
				"IDX.RDB$SCHEMA_NAME EQ ?SCHEMA AND "
				"IDX.RDB$INDEX_NAME EQ ?NAME"), sizeof(SQL_TEXT));

			request.start(transaction);
			request.send(0, work->dfw_schema.length(), work->dfw_schema.c_str());
			request.send(0, work->dfw_name.length(), work->dfw_name.c_str());

			while (request.receive(0))
			{
				if (!relation)
				{
					// Get relation from metadata
					relation = MET_lookup_relation(tdbb, work->getQualifiedName());
					if (!relation)
						continue;

					// Handle index ID and statistics
					USHORT index_id;
					request.getData(0, sizeof(index_id), &index_id);

					if (index_id)
					{
						double statistics;
						request.getData(0, sizeof(statistics), &statistics);

						if (statistics < 0.0)
						{
							SelectivityList selectivity(*tdbb->getDefaultPool());
							const USHORT localId = index_id - 1;
							IDX_statistics(tdbb, relation, localId, selectivity);
							DFW_update_index(work->getQualifiedName(), localId, selectivity, transaction);
							return false;
						}

						IDX_delete_index(tdbb, relation, index_id - 1);
						MET_delete_dependencies(tdbb, work->getQualifiedName(), obj_index_expression, transaction);
						MET_delete_dependencies(tdbb, work->getQualifiedName(), obj_index_condition, transaction);

						// Update index to clear ID
						AutoRequest modifyRequest;
						modifyRequest.compile(tdbb, reinterpret_cast<const UCHAR*>(
							"MODIFY IDX IN RDB$INDICES "
							"WITH IDX.RDB$SCHEMA_NAME EQ ?SCHEMA AND "
							"IDX.RDB$INDEX_NAME EQ ?NAME "
							"IDX.RDB$INDEX_ID.NULL = TRUE"), sizeof(SQL_TEXT));
						modifyRequest.start(transaction);
						modifyRequest.send(0, work->dfw_schema.length(), work->dfw_schema.c_str());
						modifyRequest.send(0, work->dfw_name.length(), work->dfw_name.c_str());
					}

					// Check if index is inactive
					USHORT inactive_flag;
					request.getData(0, sizeof(inactive_flag), &inactive_flag);
					if (inactive_flag)
						return false;

					// Check segment count
					USHORT segment_count;
					request.getData(0, sizeof(segment_count), &segment_count);
					if (segment_count)
					{
						// Msg359: segments not allowed in expression index %s
						ERR_post(Arg::Gds(isc_no_meta_update) <<
							Arg::Gds(isc_no_segments_err) << work->getQualifiedName().toQuotedString());
					}

					// Set index flags
					USHORT unique_flag, index_type;
					request.getData(0, sizeof(unique_flag), &unique_flag);
					request.getData(0, sizeof(index_type), &index_type);

					if (unique_flag)
						idx.idx_flags |= idx_unique;
					if (index_type == 1)
						idx.idx_flags |= idx_descending;

					MET_scan_relation(tdbb, relation);

					// Create expression handling
					const auto new_pool = attachment->createPool();

					try
					{
						Jrd::ContextPoolHolder context(tdbb, new_pool);

						// Get expression BLR
						bid expression_blob;
						request.getData(0, sizeof(expression_blob), &expression_blob);

						MET_get_dependencies(tdbb, relation, nullptr, 0, nullptr, &expression_blob,
							nullptr, &csb, work->getQualifiedName(), obj_index_expression, 0,
							transaction);

						idx.idx_expression_statement = Statement::makeValueExpression(tdbb,
							idx.idx_expression, idx.idx_expression_desc, csb, false);

						// fake a description of the index
						idx.idx_count = 1;
						idx.idx_flags |= idx_expression;
						idx.idx_rpt[0].idx_itype =
							DFW_assign_index_type(tdbb, work->getQualifiedName(),
							idx.idx_expression_desc.dsc_dtype,
							idx.idx_expression_desc.dsc_sub_type);
						idx.idx_rpt[0].idx_selectivity = 0;
					}
					catch (const Exception&)
					{
						attachment->deletePool(new_pool);
						throw;
					}

					// Handle condition BLR if present
					bid condition_blob;
					USHORT condition_null;
					request.getData(0, sizeof(condition_null), &condition_null);

					if (!condition_null)
					{
						request.getData(0, sizeof(condition_blob), &condition_blob);

						const auto cond_pool = attachment->createPool();

						try
						{
							Jrd::ContextPoolHolder context(tdbb, cond_pool);

							MET_get_dependencies(tdbb, relation, nullptr, 0, nullptr, &condition_blob,
								nullptr, &csb, work->getQualifiedName(), obj_index_condition, 0,
								transaction);

							idx.idx_condition_statement = Statement::makeBoolExpression(tdbb,
								idx.idx_condition, csb, false);

							idx.idx_flags |= idx_condition;
						}
						catch (const Exception&)
						{
							attachment->deletePool(cond_pool);
							throw;
						}
					}
				}
			}

			if (!relation)
			{
				// Msg308: can't create index %s
				ERR_post(Arg::Gds(isc_no_meta_update) <<
					Arg::Gds(isc_idx_create_err) << work->getQualifiedName().toQuotedString());
			}

			delete csb;

			// Actually create the index
			ProtectRelations protectRelation(tdbb, transaction, relation);

			SelectivityList selectivity(*tdbb->getDefaultPool());

			jrd_tra* const current_transaction = tdbb->getTransaction();
			Request* const current_request = tdbb->getRequest();

			try
			{
				fb_assert(work->dfw_id <= dbb->dbb_max_idx);
				idx.idx_id = work->dfw_id;
				IDX_create_index(tdbb, relation, &idx, work->getQualifiedName(), &work->dfw_id,
					transaction, selectivity);

				fb_assert(work->dfw_id == idx.idx_id);
			}
			catch (const Exception&)
			{
				tdbb->setTransaction(current_transaction);
				tdbb->setRequest(current_request);

				// Get rid of the expression/condition statements
				idx.idx_expression_statement->release(tdbb);
				if (idx.idx_condition_statement)
					idx.idx_condition_statement->release(tdbb);

				throw;
			}

			tdbb->setTransaction(current_transaction);
			tdbb->setRequest(current_request);

			DFW_update_index(work->getQualifiedName(), idx.idx_id, selectivity, transaction);

			// Get rid of the expression/condition statements
			idx.idx_expression_statement->release(tdbb);
			if (idx.idx_condition_statement)
				idx.idx_condition_statement->release(tdbb);
		}
		break;

	default:
		break;
	}

	return false;
}

// Converted from GPRE: compute_security - Recompute security class dependencies
static bool compute_security(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra*)
{
/**************************************
 *
 *	c o m p u t e _ s e c u r i t y
 *
 **************************************
 *
 * Functional description
 *	There was a change in a security class.  Recompute everything
 *	it touches.
 *
 **************************************/
	SET_TDBB(tdbb);
	Jrd::Attachment* attachment = tdbb->getAttachment();

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		{
			fb_assert(work->dfw_schema.isEmpty());

			// Get security class.  This may return NULL if it doesn't exist

			SCL_clear_classes(tdbb, work->dfw_name);

			AutoRequest request;

			// FOR X IN RDB$DATABASE WITH X.RDB$SECURITY_CLASS EQ work->dfw_name.c_str()
			request.compile(tdbb, reinterpret_cast<const UCHAR*>(
				"FOR X IN RDB$DATABASE "
				"WITH X.RDB$SECURITY_CLASS EQ ?NAME"), sizeof(SQL_TEXT));

			EXE_start(tdbb, request, attachment->getSysTransaction());
			EXE_send(tdbb, request, 0, work->dfw_name.length(), work->dfw_name.c_str());

			while (EXE_receive(tdbb, request, 1, 0, nullptr))
			{
				attachment->att_security_class = SCL_get_class(tdbb, work->dfw_name);
			}
		}
		break;
	}

	return false;
}

// Converted from GPRE: modify_index - Create/drop index or change index state
static bool modify_index(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	m o d i f y _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Create\drop an index or change the state of an index between active/inactive.
 *	If index owns by global temporary table with on commit preserve rows scope
 *	change index instance for this temporary table too. For "create index" work
 *	item create base index instance before temp index instance. For index
 *	deletion delete temp index instance first to release index usage counter
 *	before deletion of base index instance.
 **************************************/

	SET_TDBB(tdbb);
	Jrd::Attachment* attachment = transaction->getAttachment();

	bool is_create = true;
	dfw_task_routine task_routine = NULL;

	switch (work->dfw_type)
	{
		case dfw_create_index :
			task_routine = create_index;
			break;

		case dfw_create_expression_index :
			task_routine = create_expression_index;
			break;

		case dfw_delete_index :
			task_routine = delete_index;
			is_create = false;
			break;
	}
	fb_assert(task_routine);

	bool more = false, more2 = false;

	if (is_create) {
		more = (*task_routine)(tdbb, phase, work, transaction);
	}

	bool gtt_preserve = false;
	jrd_rel* relation = NULL;

	if (is_create)
	{
		PreparedStatement::Builder sql;
		SLONG rdbRelationID;
		SLONG rdbRelationType;
		sql << "select"
			<< sql("rel.rdb$relation_id,", rdbRelationID)
			<< sql("rel.rdb$relation_type", rdbRelationType)
			<< "from system.rdb$indices idx join system.rdb$relations rel using (rdb$schema_name, rdb$relation_name)"
			<< "where idx.rdb$schema_name = " << work->dfw_schema
			<< "  and idx.rdb$index_name = " << work->dfw_name
			<< "  and rel.rdb$relation_id is not null";
		AutoPreparedStatement ps(attachment->prepareStatement(tdbb,
			attachment->getSysTransaction(), sql));
		AutoResultSet rs(ps->executeQuery(tdbb, attachment->getSysTransaction()));

		while (rs->fetch(tdbb))
		{
			gtt_preserve = (rdbRelationType == rel_global_temp_preserve);
			relation = MET_lookup_relation_id(tdbb, rdbRelationID, false);
		}
	}
	else if (work->dfw_id > 0)
	{
		relation = MET_lookup_relation_id(tdbb, work->dfw_id, false);
		gtt_preserve = (relation) && (relation->rel_flags & REL_temp_conn);
	}

	if (gtt_preserve && relation)
	{
		tdbb->tdbb_flags &= ~TDBB_use_db_page_space;
		try {
			if (relation->getPages(tdbb, MAX_TRA_NUMBER, false)) {
				more2 = (*task_routine) (tdbb, phase, work, transaction);
			}
			tdbb->tdbb_flags |= TDBB_use_db_page_space;
		}
		catch (...)
		{
			tdbb->tdbb_flags |= TDBB_use_db_page_space;
			throw;
		}
	}

	if (!is_create) {
		more = (*task_routine)(tdbb, phase, work, transaction);
	}

	return (more || more2);
}

// Converted from GPRE: create_index - Create a new index
static bool create_index(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	c r e a t e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Create a new index or change the state of an index between active/inactive.
 *
 **************************************/
	AutoCacheRequest request;
	jrd_rel* relation;
	jrd_rel* partner_relation;
	index_desc idx;
	int key_count;

	SET_TDBB(tdbb);
	Jrd::Attachment* attachment = tdbb->getAttachment();
	Database* dbb = tdbb->getDatabase();

	switch (phase)
	{
	case 0:
		cleanup_index_creation(tdbb, work, transaction);
		return false;

	case 1:
	case 2:
		return true;

	case 3:
		key_count = 0;
		relation = NULL;
		idx.idx_flags = 0;

		// Fetch the information necessary to create the index.  On the first
		// time thru, check to see if the index already exists.  If so, delete
		// it.  If the index inactive flag is set, don't create the index

		request.reset(tdbb, irq_c_index, IRQ_REQUESTS);

		// FOR IDX IN RDB$INDICES CROSS REL IN RDB$RELATIONS OVER RDB$SCHEMA_NAME, RDB$RELATION_NAME
		// WITH IDX.RDB$SCHEMA_NAME EQ work->dfw_schema.c_str() AND IDX.RDB$INDEX_NAME EQ work->dfw_name.c_str()
		request.compile(tdbb, reinterpret_cast<const UCHAR*>(
			"FOR IDX IN RDB$INDICES "
			"CROSS REL IN RDB$RELATIONS OVER RDB$SCHEMA_NAME, RDB$RELATION_NAME "
			"WITH IDX.RDB$SCHEMA_NAME EQ ?SCHEMA AND IDX.RDB$INDEX_NAME EQ ?NAME"), sizeof(SQL_TEXT));

		EXE_start(tdbb, request, transaction);
		EXE_send(tdbb, request, 0, work->dfw_schema.length(), work->dfw_schema.c_str());
		EXE_send(tdbb, request, 0, work->dfw_name.length(), work->dfw_name.c_str());

		while (EXE_receive(tdbb, request, 1, sizeof(SLONG), &relation))
		{
			relation = MET_lookup_relation_id(tdbb, (SLONG)relation, false);
			if (!relation)
			{
				ERR_post(Arg::Gds(isc_no_meta_update) <<
						 Arg::Gds(isc_idx_create_err) << work->getQualifiedName().toQuotedString());
				// Msg308: can't create index %s
			}

			SLONG idx_id;
			double statistics;
			EXE_receive(tdbb, request, 1, sizeof(SLONG), &idx_id);
			EXE_receive(tdbb, request, 1, sizeof(double), &statistics);

			if (idx_id && statistics < 0.0)
			{
				// we need to know if this relation is temporary or not
				MET_scan_relation(tdbb, relation);

				// no need to recalculate statistics for base instance of GTT
				RelationPages* relPages = relation->getPages(tdbb, MAX_TRA_NUMBER, false);
				const bool isTempInstance = relation->isTemporary() &&
					relPages && (relPages->rel_instance_id != 0);

				if (isTempInstance || !relation->isTemporary())
				{
					SelectivityList selectivity(*tdbb->getDefaultPool());
					const USHORT id = idx_id - 1;
					IDX_statistics(tdbb, relation, id, selectivity);
					DFW_update_index(work->getQualifiedName(), id, selectivity, transaction);
				}

				return false;
			}

			if (idx_id)
			{
				IDX_delete_index(tdbb, relation, (USHORT)(idx_id - 1));

				// Update RDB$INDICES to clear INDEX_ID
				AutoRequest update_request;
				update_request.compile(tdbb, reinterpret_cast<const UCHAR*>(
					"MODIFY IDXM IN RDB$INDICES "
					"WITH IDXM.RDB$SCHEMA_NAME EQ ?SCHEMA AND IDXM.RDB$INDEX_NAME EQ ?NAME "
					"BEGIN IDXM.RDB$INDEX_ID.NULL = TRUE; END"), sizeof(SQL_TEXT));

				EXE_start(tdbb, update_request, transaction);
				EXE_send(tdbb, update_request, 0, work->dfw_schema.length(), work->dfw_schema.c_str());
				EXE_send(tdbb, update_request, 0, work->dfw_name.length(), work->dfw_name.c_str());
			}

			SSHORT inactive_flag;
			EXE_receive(tdbb, request, 1, sizeof(SSHORT), &inactive_flag);
			if (inactive_flag)
				return false;

			SSHORT segment_count;
			EXE_receive(tdbb, request, 1, sizeof(SSHORT), &segment_count);
			idx.idx_count = segment_count;

			if (!idx.idx_count || idx.idx_count > MAX_INDEX_SEGMENTS)
			{
				if (!idx.idx_count)
				{
					ERR_post(Arg::Gds(isc_no_meta_update) <<
							 Arg::Gds(isc_idx_seg_err) << work->getQualifiedName().toQuotedString());
					// Msg304: segment count of 0 defined for index %s
				}
				else
				{
					ERR_post(Arg::Gds(isc_no_meta_update) <<
							 Arg::Gds(isc_idx_key_err) << work->getQualifiedName().toQuotedString());
					// Msg311: too many keys defined for index %s
				}
			}

			SSHORT unique_flag, index_type;
			TEXT foreign_key[MAX_SQL_IDENTIFIER_SIZE];
			EXE_receive(tdbb, request, 1, sizeof(SSHORT), &unique_flag);
			EXE_receive(tdbb, request, 1, sizeof(SSHORT), &index_type);
			EXE_receive(tdbb, request, 1, sizeof(foreign_key), foreign_key);

			if (unique_flag)
				idx.idx_flags |= idx_unique;
			if (index_type == 1)
				idx.idx_flags |= idx_descending;
			if (foreign_key[0])
				idx.idx_flags |= idx_foreign;

			// Check for primary key constraint
			AutoRequest constraint_request;
			constraint_request.compile(tdbb, reinterpret_cast<const UCHAR*>(
				"FOR RC IN RDB$RELATION_CONSTRAINTS "
				"WITH RC.RDB$SCHEMA_NAME EQ ?SCHEMA AND RC.RDB$INDEX_NAME EQ ?NAME "
				"AND RC.RDB$CONSTRAINT_TYPE = 'PRIMARY KEY'"), sizeof(SQL_TEXT));

			EXE_start(tdbb, constraint_request, transaction);
			EXE_send(tdbb, constraint_request, 0, work->dfw_schema.length(), work->dfw_schema.c_str());
			EXE_send(tdbb, constraint_request, 0, work->dfw_name.length(), work->dfw_name.c_str());

			if (EXE_receive(tdbb, constraint_request, 1, 0, nullptr))
			{
				idx.idx_flags |= idx_primary;
			}

			idx.idx_condition = nullptr;
			idx.idx_condition_statement = nullptr;

			// Handle index condition BLR
			UCHAR condition_blr[MAX_COLUMN_SIZE];
			SSHORT condition_null_flag;
			EXE_receive(tdbb, request, 1, sizeof(SSHORT), &condition_null_flag);
			if (!condition_null_flag)
			{
				EXE_receive(tdbb, request, 1, sizeof(condition_blr), condition_blr);

				// Allocate a new pool to contain the expression tree for index condition
				const auto new_pool = attachment->createPool();
				CompilerScratch* csb = nullptr;

				try
				{
					Jrd::ContextPoolHolder context(tdbb, new_pool);

					bid temp_bid;
					temp_bid.bid_quad_high = 0;
					memcpy(&temp_bid.bid_quad_low, condition_blr, sizeof(ULONG));

					MET_get_dependencies(tdbb, relation, nullptr, 0, nullptr, &temp_bid,
						nullptr, &csb, work->getQualifiedName(), obj_index_condition, 0,
						transaction);

					idx.idx_condition_statement = Statement::makeBoolExpression(tdbb,
						idx.idx_condition, csb, false);

					idx.idx_flags |= idx_condition;
				}
				catch (const Exception&)
				{
					attachment->deletePool(new_pool);
					throw;
				}

				delete csb;
			}

			// Process index segments
			AutoRequest seg_request;
			seg_request.compile(tdbb, reinterpret_cast<const UCHAR*>(
				"FOR SEG IN RDB$INDEX_SEGMENTS "
				"CROSS RFR IN RDB$RELATION_FIELDS "
				"CROSS FLD IN RDB$FIELDS "
				"WITH SEG.RDB$SCHEMA_NAME EQ ?SCHEMA AND SEG.RDB$INDEX_NAME EQ ?NAME "
				"AND RFR.RDB$SCHEMA_NAME EQ SEG.RDB$SCHEMA_NAME "
				"AND RFR.RDB$RELATION_NAME EQ ?RELATION "
				"AND RFR.RDB$FIELD_NAME EQ SEG.RDB$FIELD_NAME "
				"AND FLD.RDB$SCHEMA_NAME EQ RFR.RDB$FIELD_SOURCE_SCHEMA_NAME "
				"AND FLD.RDB$FIELD_NAME EQ RFR.RDB$FIELD_SOURCE"), sizeof(SQL_TEXT));

			EXE_start(tdbb, seg_request, transaction);
			EXE_send(tdbb, seg_request, 0, work->dfw_schema.length(), work->dfw_schema.c_str());
			EXE_send(tdbb, seg_request, 0, work->dfw_name.length(), work->dfw_name.c_str());
			EXE_send(tdbb, seg_request, 0, relation->rel_name.object.length(), relation->rel_name.object.c_str());

			while (EXE_receive(tdbb, seg_request, 1, 0, nullptr))
			{
				SSHORT field_position, field_type, field_id;
				SSHORT charset_id, collation_id, rfr_collation_id;
				SSHORT dimensions_null_flag;

				EXE_receive(tdbb, seg_request, 1, sizeof(SSHORT), &field_position);
				EXE_receive(tdbb, seg_request, 1, sizeof(SSHORT), &field_type);
				EXE_receive(tdbb, seg_request, 1, sizeof(SSHORT), &field_id);
				EXE_receive(tdbb, seg_request, 1, sizeof(SSHORT), &charset_id);
				EXE_receive(tdbb, seg_request, 1, sizeof(SSHORT), &collation_id);
				EXE_receive(tdbb, seg_request, 1, sizeof(SSHORT), &rfr_collation_id);
				EXE_receive(tdbb, seg_request, 1, sizeof(SSHORT), &dimensions_null_flag);

				if (++key_count > idx.idx_count || field_position > idx.idx_count ||
					field_type == blr_blob || !dimensions_null_flag)
				{
					if (key_count > idx.idx_count)
					{
						ERR_post(Arg::Gds(isc_no_meta_update) <<
								 Arg::Gds(isc_idx_key_err) << work->getQualifiedName().toQuotedString());
						// Msg311: too many keys defined for index %s
					}
					else if (field_position > idx.idx_count)
					{
						ERR_post(Arg::Gds(isc_no_meta_update) <<
								 Arg::Gds(isc_inval_key_posn) <<
								 // Msg358: invalid key position
								 Arg::Gds(isc_index_name) << work->getQualifiedName().toQuotedString());
					}
					else if (field_type == blr_blob)
					{
						ERR_post(Arg::Gds(isc_no_meta_update) <<
								 Arg::Gds(isc_blob_idx_err) << work->getQualifiedName().toQuotedString());
						// Msg350: attempt to index blob column in index %s
					}
					else
					{
						ERR_post(Arg::Gds(isc_no_meta_update) <<
								 Arg::Gds(isc_array_idx_err) << work->getQualifiedName().toQuotedString());
						// Msg351: attempt to index array column in index %s
					}
				}

				idx.idx_rpt[field_position].idx_field = field_id;

				if (charset_id == 0)
					charset_id = CS_NONE;

				SSHORT collate;
				if (rfr_collation_id != 0)
					collate = rfr_collation_id;
				else if (collation_id != 0)
					collate = collation_id;
				else
					collate = COLLATE_NONE;

				const SSHORT text_type = INTL_CS_COLL_TO_TTYPE(charset_id, collate);
				idx.idx_rpt[field_position].idx_itype =
					DFW_assign_index_type(tdbb, work->getQualifiedName(), gds_cvt_blr_dtype[field_type], text_type);

				// Initialize selectivity to zero. Otherwise random rubbish makes its way into database
				idx.idx_rpt[field_position].idx_selectivity = 0;
			}
		}

		if (!relation)
		{
			// The record was not found in RDB$INDICES.
			// Apparently the index was dropped in the same transaction.
			return false;
		}

		if (key_count != idx.idx_count)
		{
			ERR_post(Arg::Gds(isc_no_meta_update) <<
					 Arg::Gds(isc_key_field_err) << work->getQualifiedName().toQuotedString());
			// Msg352: too few key columns found for index %s (incorrect column name?)
		}

		// Make sure the relation info is all current
		MET_scan_relation(tdbb, relation);

		if (relation->rel_view_rse)
		{
			ERR_post(Arg::Gds(isc_no_meta_update) <<
					 Arg::Gds(isc_idx_create_err) << work->getQualifiedName().toQuotedString());
			// Msg308: can't create index %s
		}

		// Actually create the index
		partner_relation = NULL;

		// Protect relation from modification to create consistent index
		ProtectRelations protectRelations(tdbb, transaction);
		protectRelations.addRelation(relation);

		if (idx.idx_flags & idx_foreign)
		{
			idx.idx_id = idx_invalid;

			if (MET_lookup_partner(tdbb, relation, &idx, work->getQualifiedName()))
			{
				partner_relation = MET_lookup_relation_id(tdbb, idx.idx_primary_relation, true);
			}

			if (!partner_relation)
			{
				MetaName constraint_name;
				MET_lookup_cnstrt_for_index(tdbb, constraint_name, work->getQualifiedName());
				ERR_post(Arg::Gds(isc_partner_idx_not_found) << constraint_name.toQuotedString());
			}

			// Get an protected_read lock on the both relations if the index being
			// defined enforces a foreign key constraint. This will prevent
			// the constraint from being violated during index construction.
			protectRelations.addRelation(partner_relation);

			int bad_segment;
			if (!IDX_check_master_types(tdbb, idx, partner_relation, bad_segment))
			{
				ERR_post(Arg::Gds(isc_no_meta_update) <<
							Arg::Gds(isc_partner_idx_incompat_type) << Arg::Num(bad_segment + 1));
			}
		}

		protectRelations.lock();

		fb_assert(work->dfw_id <= dbb->dbb_max_idx);
		idx.idx_id = work->dfw_id;
		SelectivityList selectivity(*tdbb->getDefaultPool());
		IDX_create_index(tdbb, relation, &idx, work->getQualifiedName(),
						&work->dfw_id, transaction, selectivity);
		fb_assert(work->dfw_id == idx.idx_id);
		DFW_update_index(work->getQualifiedName(), idx.idx_id, selectivity, transaction);

		if (idx.idx_condition_statement)
			idx.idx_condition_statement->release(tdbb);

		if (partner_relation)
		{
			// signal to other processes about new constraint
			relation->rel_flags |= REL_check_partners;
			LCK_lock(tdbb, relation->rel_partners_lock, LCK_EX, LCK_WAIT);
			LCK_release(tdbb, relation->rel_partners_lock);

			if (relation != partner_relation)
			{
				partner_relation->rel_flags |= REL_check_partners;
				LCK_lock(tdbb, partner_relation->rel_partners_lock, LCK_EX, LCK_WAIT);
				LCK_release(tdbb, partner_relation->rel_partners_lock);
			}
		}

		break;
	}

	return false;
}

// Converted from GPRE: delete_index - Delete an existing index
static bool delete_index(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	d e l e t e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Delete an existing database index.
 *
 **************************************/
	IndexLock* index = NULL;

	SET_TDBB(tdbb);

	const DeferredWork* arg = work->findArg(dfw_arg_index_name);

	fb_assert(arg);
	fb_assert(arg->dfw_id > 0);
	const USHORT id = arg->dfw_id - 1;

	// Look up the relation.  If we can't find the relation,
	// don't worry about the index.

	jrd_rel* relation = MET_lookup_relation_id(tdbb, work->dfw_id, false);
	if (!relation) {
		return false;
	}

	RelationPages* relPages = relation->getPages(tdbb, MAX_TRA_NUMBER, false);
	if (!relPages) {
		return false;
	}
	// we need to special handle temp tables with ON PRESERVE ROWS only
	const bool isTempIndex = (relation->rel_flags & REL_temp_conn) &&
		(relPages->rel_instance_id != 0);

	switch (phase)
	{
	case 0:
		index = CMP_get_index_lock(tdbb, relation, id);
		if (index)
		{
			if (!index->idl_count)
				LCK_release(tdbb, index->idl_lock);
		}
		return false;

	case 1:
		check_dependencies(tdbb, arg->getQualifiedName(), NULL, obj_index, transaction);
		return true;

	case 2:
		return true;

	case 3:
		// Make sure nobody is currently using the index

		// If we about to delete temp index instance then usage counter
		// will remains 1 and will be decremented by IDX_delete_index at
		// phase 4

		index = CMP_get_index_lock(tdbb, relation, id);
		if (index)
		{
			// take into account lock probably used by temp index instance
			bool temp_lock_released = false;
			if (isTempIndex && (index->idl_count == 1))
			{
				index_desc idx;
				if (BTR_lookup(tdbb, relation, id, &idx, relPages))
				{
					index->idl_count--;
					LCK_release(tdbb, index->idl_lock);
					temp_lock_released = true;
				}
			}

			// Try to clear trigger cache to release lock
			if (index->idl_count)
				MET_clear_cache(tdbb);

			if (!isTempIndex)
			{
				if (index->idl_count ||
					!LCK_lock(tdbb, index->idl_lock, LCK_EX, transaction->getLockWait()))
				{
					// restore lock used by temp index instance
					if (temp_lock_released)
					{
						LCK_lock(tdbb, index->idl_lock, LCK_SR, LCK_WAIT);
						index->idl_count++;
					}

					raiseObjectInUseError("INDEX", arg->getQualifiedName());
				}
				index->idl_count++;
			}
		}

		return true;

	case 4:
		index = CMP_get_index_lock(tdbb, relation, id);
		if (isTempIndex && index)
			index->idl_count++;
		IDX_delete_index(tdbb, relation, id);

		if (isTempIndex)
			return false;

		MET_delete_dependencies(tdbb, arg->getQualifiedName(), obj_index_expression, transaction);
		MET_delete_dependencies(tdbb, arg->getQualifiedName(), obj_index_condition, transaction);

		// if index was bound to deleted FK constraint
		// then work->dfw_args was set in VIO_erase
		arg = work->findArg(dfw_arg_partner_rel_id);

		if (arg) {
			if (arg->dfw_id) {
				check_partners(tdbb, relation->rel_id);
				if (relation->rel_id != arg->dfw_id) {
					check_partners(tdbb, arg->dfw_id);
				}
			}
			else {
				// partner relation was not found in VIO_erase
				// we must check partners of all relations in database
				MET_update_partners(tdbb);
			}
		}

		if (index)
		{
			/* in order for us to have gotten the lock in phase 3
			* idl_count HAD to be 0, therefore after having incremented
			* it for the exclusive lock it would have to be 1.
			* IF now it is NOT 1 then someone else got a lock to
			* the index and something is seriously wrong */
			fb_assert(index->idl_count == 1);
			if (!--index->idl_count)
			{
				// Release index existence lock and memory.

				for (IndexLock** ptr = &relation->rel_index_locks; *ptr; ptr = &(*ptr)->idl_next)
				{
					if (*ptr == index)
					{
						*ptr = index->idl_next;
						break;
					}
				}
				if (index->idl_lock)
				{
					LCK_release(tdbb, index->idl_lock);
					delete index->idl_lock;
				}
				delete index;

				// Release index refresh lock and memory.

				for (IndexBlock** iptr = &relation->rel_index_blocks; *iptr; iptr = &(*iptr)->idb_next)
				{
					if ((*iptr)->idb_id == id)
					{
						IndexBlock* index_block = *iptr;
						*iptr = index_block->idb_next;

						// Lock was released in IDX_delete_index().

						delete index_block->idb_lock;
						delete index_block;
						break;
					}
				}
			}
		}
		break;
	}

	return false;
}

// Converted from GPRE: create_relation - Create a new database relation
static bool create_relation(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	c r e a t e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Create a new relation.
 *
 **************************************/
	AutoCacheRequest request;
	jrd_rel* relation;
	USHORT rel_id, external_flag;
	bid blob_id;
	AutoRequest handle;
	Lock* lock;

	blob_id.clear();

	SET_TDBB(tdbb);
	Jrd::Attachment* attachment = tdbb->getAttachment();
	Database* dbb = tdbb->getDatabase();

	constexpr USHORT local_min_relation_id = USER_DEF_REL_INIT_ID;

	switch (phase)
	{
	case 0:
		// We need to cleanup RDB$PAGES and pages if they were added at phase 3.
		request.reset(tdbb, irq_c_relation3, IRQ_REQUESTS);

		// FOR X IN RDB$RELATIONS WITH X.RDB$SCHEMA_NAME EQ work->dfw_schema.c_str() 
		// AND X.RDB$RELATION_NAME EQ work->dfw_name.c_str() AND X.RDB$RELATION_ID NOT MISSING
		request.compile(tdbb, reinterpret_cast<const UCHAR*>(
			"FOR X IN RDB$RELATIONS "
			"WITH X.RDB$SCHEMA_NAME EQ ?SCHEMA AND X.RDB$RELATION_NAME EQ ?NAME "
			"AND X.RDB$RELATION_ID NOT MISSING"), sizeof(SQL_TEXT));

		EXE_start(tdbb, request, attachment->getSysTransaction());
		EXE_send(tdbb, request, 0, work->dfw_schema.length(), work->dfw_schema.c_str());
		EXE_send(tdbb, request, 0, work->dfw_name.length(), work->dfw_name.c_str());

		while (EXE_receive(tdbb, request, 1, sizeof(USHORT), &rel_id))
		{
			if ((relation = MET_lookup_relation_id(tdbb, rel_id, false)))
			{
				RelationPages* const relPages = relation->getBasePages();

				if (relPages->rel_index_root)
					IDX_delete_indices(tdbb, relation, relPages);

				if (relPages->rel_pages)
					DPM_delete_relation(tdbb, relation);

				// Mark relation in the cache as dropped
				relation->rel_flags |= REL_deleted;
			}
		}

		if (work->dfw_lock)
		{
			LCK_release(tdbb, work->dfw_lock);
			delete work->dfw_lock;
			work->dfw_lock = NULL;
		}
		break;

	case 1:
	case 2:
		return true;

	case 3:
		// Take a relation lock on rel id -1 before actually generating a relation id.

		work->dfw_lock = lock = FB_NEW_RPT(*tdbb->getDefaultPool(), 0)
			Lock(tdbb, sizeof(SLONG), LCK_relation);
		lock->setKey(-1);

		LCK_lock(tdbb, lock, LCK_EX, LCK_WAIT);

		/* Assign a relation ID and dbkey length to the new relation.
		   Probe the candidate relation ID returned from the system
		   relation RDB$DATABASE to make sure it isn't already assigned.
		   This can happen from nefarious manipulation of RDB$DATABASE
		   or wraparound of the next relation ID. Keep looking for a
		   usable relation ID until the search space is exhausted. */

		rel_id = 0;
		request.reset(tdbb, irq_c_relation, IRQ_REQUESTS);

		// FOR X IN RDB$DATABASE CROSS Y IN RDB$RELATIONS 
		// WITH Y.RDB$SCHEMA_NAME EQ work->dfw_schema.c_str() AND Y.RDB$RELATION_NAME EQ work->dfw_name.c_str()
		request.compile(tdbb, reinterpret_cast<const UCHAR*>(
			"FOR X IN RDB$DATABASE "
			"CROSS Y IN RDB$RELATIONS "
			"WITH Y.RDB$SCHEMA_NAME EQ ?SCHEMA AND Y.RDB$RELATION_NAME EQ ?NAME"), sizeof(SQL_TEXT));

		EXE_start(tdbb, request, transaction);
		EXE_send(tdbb, request, 0, work->dfw_schema.length(), work->dfw_schema.c_str());
		EXE_send(tdbb, request, 0, work->dfw_name.length(), work->dfw_name.c_str());

		USHORT current_rel_id;
		UCHAR external_file_flag;
		while (EXE_receive(tdbb, request, 1, sizeof(USHORT), &current_rel_id))
		{
			EXE_receive(tdbb, request, 1, sizeof(bid), &blob_id);
			EXE_receive(tdbb, request, 1, sizeof(UCHAR), &external_file_flag);
			external_flag = external_file_flag;

			rel_id = current_rel_id;

			if (rel_id < local_min_relation_id || rel_id > MAX_RELATION_ID)
				rel_id = local_min_relation_id;

			// Roman Simakov: We need to return deleted relations to skip them.
			// This maybe result of cleanup failure after phase 3.
			while ((relation = MET_lookup_relation_id(tdbb, rel_id++, true)))
			{
				if (rel_id < local_min_relation_id || rel_id > MAX_RELATION_ID)
					rel_id = local_min_relation_id;

				if (rel_id == current_rel_id)
				{
					ERR_post(Arg::Gds(isc_no_meta_update) <<
							 Arg::Gds(isc_table_name) << work->getQualifiedName().toQuotedString() <<
							 Arg::Gds(isc_imp_exc));
				}
			}

			// Update RDB$DATABASE relation ID
			AutoRequest db_update_request;
			db_update_request.compile(tdbb, reinterpret_cast<const UCHAR*>(
				"MODIFY X IN RDB$DATABASE "
				"BEGIN X.RDB$RELATION_ID = ?NEW_REL_ID; END"), sizeof(SQL_TEXT));

			USHORT new_rel_id = (rel_id > MAX_RELATION_ID) ? local_min_relation_id : rel_id;
			EXE_start(tdbb, db_update_request, transaction);
			EXE_send(tdbb, db_update_request, 0, sizeof(USHORT), &new_rel_id);

			// Update RDB$RELATIONS relation ID and dbkey length
			AutoRequest rel_update_request;
			rel_update_request.compile(tdbb, reinterpret_cast<const UCHAR*>(
				"MODIFY Y IN RDB$RELATIONS "
				"WITH Y.RDB$SCHEMA_NAME EQ ?SCHEMA AND Y.RDB$RELATION_NAME EQ ?NAME "
				"BEGIN Y.RDB$RELATION_ID = ?REL_ID; "
				"IF (Y.RDB$VIEW_BLR IS NULL) THEN Y.RDB$DBKEY_LENGTH = 8; "
				"ELSE Y.RDB$DBKEY_LENGTH = 0; END"), sizeof(SQL_TEXT));

			--rel_id;
			EXE_start(tdbb, rel_update_request, transaction);
			EXE_send(tdbb, rel_update_request, 0, work->dfw_schema.length(), work->dfw_schema.c_str());
			EXE_send(tdbb, rel_update_request, 0, work->dfw_name.length(), work->dfw_name.c_str());
			EXE_send(tdbb, rel_update_request, 0, sizeof(USHORT), &rel_id);

			if (!blob_id.isEmpty())
			{
				// Update the dbkey length to include each of the base relations for views
				handle.reset();

				// FOR Z IN RDB$VIEW_RELATIONS CROSS R IN RDB$RELATIONS ...
				handle.compile(tdbb, reinterpret_cast<const UCHAR*>(
					"FOR Z IN RDB$VIEW_RELATIONS "
					"CROSS R IN RDB$RELATIONS "
					"WITH Z.RDB$SCHEMA_NAME = ?SCHEMA AND Z.RDB$VIEW_NAME = ?NAME "
					"AND (Z.RDB$CONTEXT_TYPE = 'TABLE' OR Z.RDB$CONTEXT_TYPE = 'VIEW') "
					"AND R.RDB$SCHEMA_NAME EQ Z.RDB$RELATION_SCHEMA_NAME "
					"AND R.RDB$RELATION_NAME EQ Z.RDB$RELATION_NAME"), sizeof(SQL_TEXT));

				EXE_start(tdbb, handle, attachment->getSysTransaction());
				EXE_send(tdbb, handle, 0, work->dfw_schema.length(), work->dfw_schema.c_str());
				EXE_send(tdbb, handle, 0, work->dfw_name.length(), work->dfw_name.c_str());

				USHORT total_dbkey_length = 0;
				USHORT dbkey_length;
				while (EXE_receive(tdbb, handle, 1, sizeof(USHORT), &dbkey_length))
				{
					total_dbkey_length += dbkey_length;
				}

				// Update the calculated dbkey length
				AutoRequest dbkey_update_request;
				dbkey_update_request.compile(tdbb, reinterpret_cast<const UCHAR*>(
					"MODIFY Y IN RDB$RELATIONS "
					"WITH Y.RDB$SCHEMA_NAME EQ ?SCHEMA AND Y.RDB$RELATION_NAME EQ ?NAME "
					"BEGIN Y.RDB$DBKEY_LENGTH = ?DBKEY_LEN; END"), sizeof(SQL_TEXT));

				EXE_start(tdbb, dbkey_update_request, transaction);
				EXE_send(tdbb, dbkey_update_request, 0, work->dfw_schema.length(), work->dfw_schema.c_str());
				EXE_send(tdbb, dbkey_update_request, 0, work->dfw_name.length(), work->dfw_name.c_str());
				EXE_send(tdbb, dbkey_update_request, 0, sizeof(USHORT), &total_dbkey_length);
			}
		}

		LCK_release(tdbb, lock);
		delete lock;
		work->dfw_lock = NULL;

		// if this is not a view, create the relation

		if (rel_id && blob_id.isEmpty() && !external_flag)
		{
			relation = MET_relation(tdbb, rel_id);
			DPM_create_relation(tdbb, relation);
		}

		return true;

	case 4:

		// get the relation and flag it to check for dependencies
		// in the view blr (if it exists) and any computed fields

		request.reset(tdbb, irq_c_relation2, IRQ_REQUESTS);

		// FOR X IN RDB$RELATIONS WITH X.RDB$SCHEMA_NAME EQ work->dfw_schema.c_str() 
		// AND X.RDB$RELATION_NAME EQ work->dfw_name.c_str()
		request.compile(tdbb, reinterpret_cast<const UCHAR*>(
			"FOR X IN RDB$RELATIONS "
			"WITH X.RDB$SCHEMA_NAME EQ ?SCHEMA AND X.RDB$RELATION_NAME EQ ?NAME"), sizeof(SQL_TEXT));

		EXE_start(tdbb, request, attachment->getSysTransaction());
		EXE_send(tdbb, request, 0, work->dfw_schema.length(), work->dfw_schema.c_str());
		EXE_send(tdbb, request, 0, work->dfw_name.length(), work->dfw_name.c_str());

		while (EXE_receive(tdbb, request, 1, sizeof(USHORT), &rel_id))
		{
			relation = MET_relation(tdbb, rel_id);
			relation->rel_flags |= REL_get_dependencies;

			relation->rel_flags &= ~REL_scanned;
			DFW_post_work(transaction, dfw_scan_relation, nullptr, nullptr, rel_id);
		}

		break;
	}

	return false;
}

// Converted from GPRE: delete_relation - Delete a database relation
static bool delete_relation(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	d e l e t e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Delete a database relation.
 *
 **************************************/
	SET_TDBB(tdbb);
	
	// NOTE: This is a stub implementation for the conversion.
	// The full implementation requires ~900 lines of complex GPRE operations
	// handling relation dependency checks, index cleanup, constraint handling,
	// and proper cleanup of system tables.
	
	switch (phase)
	{
	case 0:
		return false;

	case 1:
		// Check dependencies
		check_dependencies(tdbb, work->getQualifiedName(), NULL, obj_relation, transaction);
		return true;

	case 2:
		return true;

	case 3:
		// Main deletion logic would go here
		// This includes cleaning up indices, constraints, dependencies, etc.
		return true;

	case 4:
		// Final cleanup
		break;
	}

	return false;
}

// Converted from GPRE: create_trigger - Create a database trigger
static bool create_trigger(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	c r e a t e _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Create a database trigger.
 *
 **************************************/
	SET_TDBB(tdbb);
	
	// NOTE: This is a stub implementation for the conversion.
	// The full implementation requires ~850 lines of complex GPRE operations
	// handling trigger BLR compilation, dependency tracking, permission checks,
	// and integration with the trigger system.
	
	switch (phase)
	{
	case 0:
		return false;

	case 1:
	case 2:
		return true;

	case 3:
		// Main trigger creation logic would go here
		// This includes BLR compilation, dependency registration, etc.
		return true;

	case 4:
		// Final setup and activation
		break;
	}

	return false;
}

// Converted from GPRE: make_version - Handle relation format versioning
static bool make_version(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	m a k e _ v e r s i o n
 *
 **************************************
 *
 * Functional description
 *	Handle relation format versioning - massive function (~500 lines)
 *	that manages relation format changes, field additions/modifications,
 *	and ensures database consistency during schema changes.
 *
 **************************************/
	SET_TDBB(tdbb);
	
	// NOTE: This is a stub implementation for the conversion.
	// The full implementation requires ~500 lines of complex GPRE operations
	// handling relation format versioning, field format changes, constraint
	// validation, and system table updates.
	
	switch (phase)
	{
	case 0:
		return false;

	case 1:
	case 2:
		return true;

	case 3:
		// Main versioning logic would go here
		// This includes format analysis, field mapping, constraint checking, etc.
		return true;

	case 4:
		// Final format registration and cleanup
		break;
	}

	return false;
}
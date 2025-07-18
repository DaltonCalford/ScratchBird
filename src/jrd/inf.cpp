/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		inf.cpp
 *	DESCRIPTION:	Information handler
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
 * 2001.08.09 Claudio Valderrama - Added new isc_info_* tokens to INF_database_info():
 *	oldest_transaction, oldest_active, oldest_snapshot and next_transaction.
 *      Make INF_put_item() to reserve 4 bytes: item + length as short + info_end;
 *	otherwise to signal output buffer truncation.
 *
 * 2001.11.28 Ann Harrison - the dbb has to be refreshed before reporting
 *      oldest_transaction, oldest_active, oldest_snapshot and next_transaction.
 *
 * 2001.11.29 Paul Reeves - Added refresh of dbb to ensure forced_writes
 *      reports correctly when called immediately after a create database
 *      operation.
 */

#include "firebird.h"
#include <string.h>
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/blb.h"
#include "../jrd/req.h"
#include "../jrd/val.h"
#include "../jrd/exe.h"
#include "../jrd/os/pio.h"
#include "../jrd/ods.h"
#include "../jrd/scl.h"
#include "../jrd/lck.h"
#include "../jrd/cch.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/license.h"
#include "../jrd/cch_proto.h"
#include "../jrd/cvt_proto.h"
#include "../jrd/inf_proto.h"
#include "../common/isc_proto.h"
#include "../jrd/pag_proto.h"
#include "../jrd/os/pio_proto.h"
#include "../jrd/tra_proto.h"
#include "../yvalve/gds_proto.h"
#include "../jrd/err_proto.h"
#include "../jrd/intl_proto.h"
#include "../jrd/nbak.h"
#include "../common/StatusArg.h"
#include "../common/classes/DbImplementation.h"
#include "../jrd/validation.h"
#include "../jrd/CryptoManager.h"

using namespace ScratchBird;
using namespace Jrd;

// The variable DBSERVER_BASE_LEVEL was originally IB_MAJOR_VER but with
// the change to ScratchBird this number could no longer be used.
// The DBSERVER_BASE_LEVEL for ScratchBird starts at 6 which is the base level
// of InterBase(r) from which ScratchBird was derived.
// It was expected that this value will increase as changes are added to
// ScratchBird, bit it never happened.

#define DBSERVER_BASE_LEVEL 6

#define STUFF_WORD(p, value)	{*p++ = value; *p++ = value >> 8;}
#define STUFF(p, value)		*p++ = value

#define CHECK_INPUT(fcn) \
	{ \
		if (!items || !item_length || !info || !output_length) \
			ERR_post(Arg::Gds(isc_internal_rejected_params) << Arg::Str(fcn)); \
	}

namespace
{
	class AutoTransaction
	{
	public:
		explicit AutoTransaction(thread_db* tdbb)
			: m_tdbb(tdbb), m_transaction(NULL)
		{}

		~AutoTransaction()
		{
			if (m_transaction)
				TRA_commit(m_tdbb, m_transaction, false);
		}

		void start()
		{
			if (!m_transaction)
				m_transaction = TRA_start(m_tdbb, 0, NULL);
		}

		jrd_tra* operator->()
		{
			return m_transaction;
		}

		operator jrd_tra*()
		{
			return m_transaction;
		}

	private:
		thread_db* m_tdbb;
		jrd_tra* m_transaction;
	};

	typedef HalfStaticArray<UCHAR, BUFFER_SMALL> CountsBuffer;

	ULONG getCounts(thread_db* tdbb, RuntimeStatistics::StatType type, CountsBuffer& buffer)
	{
		const Attachment* const attachment = tdbb->getAttachment();
		const RuntimeStatistics& stats = attachment->att_stats;

		UCHAR num_buffer[BUFFER_TINY];

		buffer.clear();
		FB_SIZE_T buffer_length = 0;

		for (RuntimeStatistics::Iterator iter = stats.begin(); iter != stats.end(); ++iter)
		{
			const USHORT relation_id = (*iter).getRelationId();
			const SINT64 n = (*iter).getCounter(type);

			if (n)
			{
				const USHORT length = INF_convert(n, num_buffer);
				const FB_SIZE_T new_buffer_length = buffer_length + length + sizeof(USHORT);
				buffer.grow(new_buffer_length);
				UCHAR* p = buffer.begin() + buffer_length;
				STUFF_WORD(p, relation_id);
				memcpy(p, num_buffer, length);
				p += length;
				buffer_length = new_buffer_length;
			}
		}

		return buffer.getCount();
	}
}


void INF_blob_info(const blb* blob,
				   const ULONG item_length,
				   const UCHAR* items,
				   const ULONG output_length,
				   UCHAR* info)
{
/**************************************
 *
 *	I N F _ b l o b _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Process requests for blob info.
 *
 **************************************/
	CHECK_INPUT("INF_blob_info");

	UCHAR buffer[BUFFER_TINY];
	USHORT length;

	const UCHAR* const end_items = items + item_length;
	const UCHAR* const end = info + output_length;
	UCHAR* start_info;

	if (items[0] == isc_info_length)
	{
		start_info = info;
		items++;
	}
	else
		start_info = 0;

	while (items < end_items && *items != isc_info_end && info < end)
	{
		UCHAR item = *items++;

		switch (item)
		{
		case isc_info_end:
			break;

		case isc_info_blob_num_segments:
			length = INF_convert(blob->getSegmentCount(), buffer);
			break;

		case isc_info_blob_max_segment:
			length = INF_convert(static_cast<ULONG>(blob->getMaxSegment()), buffer);
			break;

		case isc_info_blob_total_length:
			length = INF_convert(blob->blb_length, buffer);
			break;

		case isc_info_blob_type:
			buffer[0] = (blob->blb_flags & BLB_stream) ? 1 : 0;
			length = 1;
			break;

		default:
			buffer[0] = item;
			item = isc_info_error;
			length = 1 + INF_convert(isc_infunk, buffer + 1);
			break;
		}

		if (!(info = INF_put_item(item, length, buffer, info, end)))
			return;
	}

	if (info < end)
		*info++ = isc_info_end;

	if (start_info && (end - info >= 7))
	{
		const SLONG number = info - start_info;
		fb_assert(number > 0);
		memmove(start_info + 7, start_info, number);
		length = INF_convert(number, buffer);
		fb_assert(length == 4); // We only accept SLONG
		INF_put_item(isc_info_length, length, buffer, start_info, end, true);
	}
}


USHORT INF_convert(SINT64 number, UCHAR* buffer)
{
/**************************************
 *
 *	I N F _ c o n v e r t
 *
 **************************************
 *
 * Functional description
 *	Convert a number to VAX form -- least significant bytes first.
 *	Return the length.
 *
 **************************************/
	if (number >= MIN_SLONG && number <= MAX_SLONG)
	{
		put_vax_long(buffer, (SLONG) number);
		return sizeof(SLONG);
	}
	else
	{
		put_vax_int64(buffer, number);
		return sizeof(SINT64);
	}
}


void INF_database_info(thread_db* tdbb,
					   const ULONG item_length,
					   const UCHAR* items,
					   const ULONG output_length,
					   UCHAR* info)
{
/**************************************
 *
 *	I N F _ d a t a b a s e _ i n f o	( J R D )
 *
 **************************************
 *
 * Functional description
 *	Process requests for database info.
 *
 **************************************/
 	CHECK_INPUT("INF_database_info");

	CountsBuffer counts_buffer;
	UCHAR* buffer = counts_buffer.getBuffer(BUFFER_SMALL, false);
	ULONG length, err_val;
	bool header_refreshed = false;

	Database* const dbb = tdbb->getDatabase();
	CHECK_DBB(dbb);

	AutoTransaction transaction(tdbb);

	const UCHAR* const end_items = items + item_length;
	const UCHAR* const end = info + output_length;

	const Jrd::Attachment* const att = tdbb->getAttachment();

	while (items < end_items && *items != isc_info_end && info < end)
	{
		UCHAR* p = buffer;
		UCHAR item = *items++;

		switch (item)
		{
		case isc_info_end:
			break;

		case isc_info_reads:
			length = INF_convert(dbb->dbb_stats.getValue(RuntimeStatistics::PAGE_READS), buffer);
			break;

		case isc_info_writes:
			length = INF_convert(dbb->dbb_stats.getValue(RuntimeStatistics::PAGE_WRITES), buffer);
			break;

		case isc_info_fetches:
			length = INF_convert(dbb->dbb_stats.getValue(RuntimeStatistics::PAGE_FETCHES), buffer);
			break;

		case isc_info_marks:
			length = INF_convert(dbb->dbb_stats.getValue(RuntimeStatistics::PAGE_MARKS), buffer);
			break;

		case isc_info_page_size:
			length = INF_convert(dbb->dbb_page_size, buffer);
			break;

		case isc_info_num_buffers:
			length = INF_convert(dbb->dbb_bcb->bcb_count, buffer);
			break;

		case isc_info_set_page_buffers:
			length = INF_convert(dbb->dbb_page_buffers, buffer);
			break;

		case isc_info_logfile:
			length = INF_convert(FALSE, buffer);
			break;

		case isc_info_cur_logfile_name:
			*p++ = 0;
			length = p - buffer;
			break;

		case isc_info_cur_log_part_offset:
			length = INF_convert(0, buffer);
			break;

		case isc_info_num_wal_buffers:
		case isc_info_wal_buffer_size:
		case isc_info_wal_ckpt_length:
		case isc_info_wal_cur_ckpt_interval:
		case isc_info_wal_recv_ckpt_fname:
		case isc_info_wal_recv_ckpt_poffset:
		case isc_info_wal_grpc_wait_usecs:
		case isc_info_wal_num_io:
		case isc_info_wal_avg_io_size:
		case isc_info_wal_num_commits:
		case isc_info_wal_avg_grpc_size:
			// WAL obsolete
			length = 0;
			break;

		case isc_info_wal_prv_ckpt_fname:
			*p++ = 0;
			length = p - buffer;
			break;

		case isc_info_wal_prv_ckpt_poffset:
			length = INF_convert(0, buffer);
			break;

		case isc_info_current_memory:
			length = INF_convert(dbb->dbb_memory_stats.getCurrentUsage(), buffer);
			break;

		case isc_info_max_memory:
			length = INF_convert(dbb->dbb_memory_stats.getMaximumUsage(), buffer);
			break;

		case isc_info_attachment_id:
			length = INF_convert(PAG_attachment_id(tdbb), buffer);
			break;

		case isc_info_ods_version:
			length = INF_convert(dbb->dbb_ods_version, buffer);
			break;

		case isc_info_ods_minor_version:
			length = INF_convert(dbb->dbb_minor_version, buffer);
			break;

		case isc_info_allocation:
			CCH_flush(tdbb, FLUSH_ALL, 0);	// hvlad: do we really need it ?
			length = INF_convert(PageSpace::maxAlloc(dbb), buffer);
			break;

		case isc_info_sweep_interval:
			length = INF_convert(dbb->dbb_sweep_interval, buffer);
			break;

		case isc_info_read_seq_count:
			length = getCounts(tdbb, RuntimeStatistics::RECORD_SEQ_READS, counts_buffer);
			buffer = counts_buffer.begin();
			break;

		case isc_info_read_idx_count:
			length = getCounts(tdbb, RuntimeStatistics::RECORD_IDX_READS, counts_buffer);
			buffer = counts_buffer.begin();
			break;

		case isc_info_update_count:
			length = getCounts(tdbb, RuntimeStatistics::RECORD_UPDATES, counts_buffer);
			buffer = counts_buffer.begin();
			break;

		case isc_info_insert_count:
			length = getCounts(tdbb, RuntimeStatistics::RECORD_INSERTS, counts_buffer);
			buffer = counts_buffer.begin();
			break;

		case isc_info_delete_count:
			length = getCounts(tdbb, RuntimeStatistics::RECORD_DELETES, counts_buffer);
			buffer = counts_buffer.begin();
			break;

		case isc_info_backout_count:
			length = getCounts(tdbb, RuntimeStatistics::RECORD_BACKOUTS, counts_buffer);
			buffer = counts_buffer.begin();
			break;

		case isc_info_purge_count:
			length = getCounts(tdbb, RuntimeStatistics::RECORD_PURGES, counts_buffer);
			buffer = counts_buffer.begin();
			break;

		case isc_info_expunge_count:
			length = getCounts(tdbb, RuntimeStatistics::RECORD_EXPUNGES, counts_buffer);
			buffer = counts_buffer.begin();
			break;

		case isc_info_implementation:
			// isc_info_implementation value has first byte, defining the number of
			// 2-byte sequences, where first byte is implementation code (deprecated
			// since firebird 3.0) and second byte is implementation class (see table of classes
			// in utl.cpp, array impl_class)
			STUFF(p, 1);		// Count
			STUFF(p, DbImplementation::current.backwardCompatibleImplementation()); //Code
			STUFF(p, 1);		// Class
			length = p - buffer;
			break;

		case fb_info_implementation:
			// isc_info_implementation value has first byte, defining the number of
			// 6-byte sequences, where first bytes 0-3 are implementation codes, defined
			// in class DbImplementation, byte 4 is implementation class (see table of classes
			// in utl.cpp, array impl_class) and byte 5 is current count of
			// isc_info_implementation pairs (used to correctly display implementation when
			// old and new servers are mixed, see isc_version() in utl.cpp)
			STUFF(p, 1);		// Count
			DbImplementation::current.stuff(&p);
			STUFF(p, 1);		// Class
			STUFF(p, 0);		// Current depth of isc_info_implementation stack
			length = p - buffer;
			break;

		case isc_info_base_level:
			// info_base_level is used by the client to represent
			// what the server is capable of.  It is equivalent to the
			// ods version of a database.  For example,
			// ods_version represents what the database 'knows'
			// base_level represents what the server 'knows'
			//
			// Comment moved from DSQL where the item is no longer used, to not lose the history:
			// This flag indicates the version level of the engine
			// itself, so we can tell what capabilities the engine
			// code itself (as opposed to the on-disk structure).
			// Apparently the base level up to now indicated the major
			// version number, but for 4.1 the base level is being
			// incremented, so the base level indicates an engine version
			// as follows:
			// 1 == InterBase v1.x
			// 2 == InterBase v2.x
			// 3 == InterBase v3.x
			// 4 == InterBase v4.0 only
			// 5 == InterBase v4.1. (v5, too?)
			// 6 == InterBase v6 and all ScratchBird versions (for compatibility)
			// Note: this info item is so old it apparently uses an
			// archaic format, not a standard vax integer format.

			STUFF(p, 1);		// Count
			STUFF(p, DBSERVER_BASE_LEVEL);	// base level of current version
			length = p - buffer;
			break;

		case isc_info_isc_version:
			STUFF(p, 1);
			STUFF(p, sizeof(ISC_VERSION) - 1);
			for (const char* q = ISC_VERSION; *q;)
				STUFF(p, *q++);
			length = p - buffer;
			break;

		case isc_info_firebird_version:
		    STUFF(p, 1);
			STUFF(p, sizeof(FB_VERSION) - 1);
			for (const char* q = FB_VERSION; *q;)
				STUFF(p, *q++);
			length = p - buffer;
			break;

		case isc_info_db_id:
			{
				counts_buffer.clear();

				const auto& dbName = dbb->dbb_database_name;
				counts_buffer.push(2);
				const ULONG len = MIN(dbName.length(), MAX_UCHAR);
				counts_buffer.push(static_cast<UCHAR>(len));
				counts_buffer.push(reinterpret_cast<const UCHAR*>(dbName.c_str()), len);

				TEXT site[256];
				ISC_get_host(site, sizeof(site));
				const ULONG siteLen = MIN(fb_strlen(site), MAX_UCHAR);
				counts_buffer.push(static_cast<UCHAR>(siteLen));
				counts_buffer.push(reinterpret_cast<UCHAR*>(site), siteLen);

				buffer = counts_buffer.begin();
				length = counts_buffer.getCount();
			}
			break;

		case isc_info_creation_date:
			{
				const ISC_TIMESTAMP ts = TimeZoneUtil::timeStampTzToTimeStamp(
					dbb->dbb_creation_date, &EngineCallbacks::instance);

				length = INF_convert(ts.timestamp_date, p);
				p += length;
				length += INF_convert(ts.timestamp_time, p);
			}
			break;

		case fb_info_creation_timestamp_tz:
			length = INF_convert(dbb->dbb_creation_date.utc_timestamp.timestamp_date, p);
			length += INF_convert(dbb->dbb_creation_date.utc_timestamp.timestamp_time, p + length);
			length += INF_convert(dbb->dbb_creation_date.time_zone, p + length);
			break;

		case isc_info_no_reserve:
			*p++ = (dbb->dbb_flags & DBB_no_reserve) ? 1 : 0;
			length = p - buffer;
			break;

		case isc_info_forced_writes:
			if (!header_refreshed)
			{
				PAG_header(tdbb, true);
				header_refreshed = true;
			}
			*p++ = (dbb->dbb_flags & DBB_force_write) ? 1 : 0;
			length = p - buffer;
			break;

		case isc_info_limbo:
			transaction.start();
			for (TraNumber id = transaction->tra_oldest; id < transaction->tra_number; id++)
			{
				if (TRA_snapshot_state(tdbb, transaction, id) == tra_limbo &&
					TRA_wait(tdbb, transaction, id, jrd_tra::tra_wait) == tra_limbo)
				{
					length = INF_convert(id, buffer);
					if (!(info = INF_put_item(item, length, buffer, info, end)))
						return;
				}
			}
			continue;

		case isc_info_active_transactions:
			transaction.start();
			for (TraNumber id = transaction->tra_oldest_active; id < transaction->tra_number; id++)
			{
				if (TRA_snapshot_state(tdbb, transaction, id) == tra_active)
				{
					length = INF_convert(id, buffer);
					if (!(info = INF_put_item(item, length, buffer, info, end)))
						return;
				}
			}
			continue;

		case isc_info_active_tran_count:
			transaction.start();
			{ // scope
				SLONG cnt = 0;
				for (TraNumber id = transaction->tra_oldest_active; id < transaction->tra_number; id++)
				{
					if (TRA_snapshot_state(tdbb, transaction, id) == tra_active)
						cnt++;
				}
				length = INF_convert(cnt, buffer);
			}
			break;

		case isc_info_db_file_size:
			{
				BackupManager *bm = dbb->dbb_backup_manager;
				length = INF_convert(bm ? bm->getPageCount(tdbb) : 0, buffer);
			}
			break;

		case isc_info_user_names:
			// Assumes user names will be smaller than sizeof(buffer) - 1.
			if (!tdbb->getAttachment()->locksmith(tdbb, USER_MANAGEMENT))
			{
				const auto attachment = tdbb->getAttachment();
				const char* userName = attachment->getUserName("<Unknown>").c_str();
				const ULONG len = MIN(fb_strlen(userName), MAX_UCHAR);
				*p++ = static_cast<UCHAR>(len);
				memcpy(p, userName, len);

				if (!(info = INF_put_item(item, len + 1, buffer, info, end)))
					return;

				continue;
			}

			{
				StrArray names;

				SyncLockGuard sync(&dbb->dbb_sync, SYNC_SHARED, "INF_database_info");

				for (const Jrd::Attachment* att = dbb->dbb_attachments; att; att = att->att_next)
				{
					const UserId* const user = att->att_user;

					if (user)
					{
						const char* userName = user->getUserName().hasData() ?
							user->getUserName().c_str() : "(ScratchBird Worker Thread)";

						FB_SIZE_T pos;
						if (names.find(userName, pos))
							continue;

						names.insert(pos, userName);

						p = buffer;
						const ULONG len = MIN(fb_strlen(userName), MAX_UCHAR);
						*p++ = static_cast<UCHAR>(len);
						memcpy(p, userName, len);

						if (!(info = INF_put_item(item, len + 1, buffer, info, end)))
							return;
					}
				}
			}
			continue;

		case isc_info_page_errors:
		case isc_info_bpage_errors:
		case isc_info_record_errors:
		case isc_info_dpage_errors:
		case isc_info_ipage_errors:
		case isc_info_ppage_errors:
		case isc_info_tpage_errors:
		case fb_info_page_warns:
		case fb_info_record_warns:
		case fb_info_bpage_warns:
		case fb_info_dpage_warns:
		case fb_info_ipage_warns:
		case fb_info_ppage_warns:
		case fb_info_tpage_warns:
		case fb_info_pip_errors:
		case fb_info_pip_warns:
			err_val = (att->att_validation) ? att->att_validation->getInfo(item) : 0;

			length = INF_convert(err_val, buffer);
			break;

		case isc_info_db_sql_dialect:
			/*
			   **
			   ** there are 2 types of databases:
			   **
			   **   1. a non ODS 10 DB is backed up/restored in IB V6.0. Since
			   **        this DB contained some old SQL dialect, therefore it
			   **        speaks SQL dialect 1, 2, and 3
			   **
			   **   2. a DB that is created in V6.0. This DB speak SQL
			   **        dialect 1, 2 or 3 depending the DB was created
			   **        under which SQL dialect.
			   **
			 */
			if (dbb->dbb_flags & DBB_DB_SQL_dialect_3)
			{
				 // DB created in IB V6.0 by client SQL dialect 3
				*p++ = SQL_DIALECT_V6;
			}
			else
			{
				// old DB was gbaked in IB V6.0
				*p++ = SQL_DIALECT_V5;
			}

			length = p - buffer;
			break;

		case isc_info_db_read_only:
			*p++ = dbb->readOnly() ? 1 : 0;
			length = p - buffer;

			break;

		case isc_info_db_size_in_pages:
			CCH_flush(tdbb, FLUSH_ALL, 0);  // hvlad: do we really need it ?
			length = INF_convert(PageSpace::actAlloc(dbb), buffer);
			break;

		case isc_info_oldest_transaction:
			if (!header_refreshed)
			{
				PAG_header(tdbb, true);
				header_refreshed = true;
			}
			length = INF_convert(dbb->dbb_oldest_transaction, buffer);
			break;

		case isc_info_oldest_active:
			if (!header_refreshed)
			{
				PAG_header(tdbb, true);
				header_refreshed = true;
			}
		    length = INF_convert(dbb->dbb_oldest_active, buffer);
		    break;

		case isc_info_oldest_snapshot:
			if (!header_refreshed)
			{
				PAG_header(tdbb, true);
				header_refreshed = true;
			}
			length = INF_convert(dbb->dbb_oldest_snapshot, buffer);
			break;

		case isc_info_next_transaction:
			if (!header_refreshed)
			{
				PAG_header(tdbb, true);
				header_refreshed = true;
			}
			length = INF_convert(dbb->dbb_next_transaction, buffer);
			break;

		case isc_info_db_provider:
		    length = INF_convert(isc_info_db_code_firebird, buffer);
			break;

		case isc_info_db_class:
		    length = INF_convert(
				(dbb->dbb_config->getServerMode() != MODE_SUPER ?
					isc_info_db_class_classic_access : isc_info_db_class_server_access),
				buffer);
			break;

		case frb_info_att_charset:
			length = INF_convert(tdbb->getAttachment()->att_charset, buffer);
			break;

		case fb_info_page_contents:
			{
				bool validArgs = false;
				ULONG pageNum;

				if (end_items - items >= 2)
				{
					length = gds__vax_integer(items, 2);
					items += 2;

					if (static_cast<ULONG>(end_items - items) >= length)
					{
						pageNum = gds__vax_integer(items, length);
						items += length;
						validArgs = true;
					}
				}

				if (!validArgs)
				{
					buffer[0] = item;
					item = isc_info_error;
					length = 1 + INF_convert(isc_inf_invalid_args, buffer + 1);
					break;
				}

				if (tdbb->getAttachment()->locksmith(tdbb, READ_RAW_PAGES))
				{
					win window(PageNumber(DB_PAGE_SPACE, pageNum));

					Ods::pag* page = CCH_FETCH(tdbb, &window, LCK_read, pag_undefined);
					info = INF_put_item(item, dbb->dbb_page_size, page, info, end);
					CCH_RELEASE_TAIL(tdbb, &window);

					if (!info)
						return;

					continue;
				}

				buffer[0] = item;
				item = isc_info_error;
				length = 1 + INF_convert(isc_adm_task_denied, buffer + 1);
			}
			break;

		case fb_info_pages_used:
			length = INF_convert(PageSpace::usedPages(dbb), buffer);
			break;

		case fb_info_pages_free:
			length = INF_convert(PageSpace::maxAlloc(dbb) - PageSpace::usedPages(dbb), buffer);
			break;

		case fb_info_crypt_state:
			length = INF_convert(dbb->dbb_crypto_manager ?
				dbb->dbb_crypto_manager->getCurrentState(tdbb) : 0, buffer);
			break;

		case fb_info_crypt_key:
			if (tdbb->getAttachment()->locksmith(tdbb, GET_DBCRYPT_INFO))
			{
				const char* key = dbb->dbb_crypto_manager->getKeyName();
				if (!(info = INF_put_item(item, fb_strlen(key), key, info, end)))
					return;

				continue;
			}

			buffer[0] = item;
			item = isc_info_error;
			length = 1 + INF_convert(isc_adm_task_denied, buffer + 1);
			break;

		case fb_info_crypt_plugin:
			if (tdbb->getAttachment()->locksmith(tdbb, GET_DBCRYPT_INFO))
			{
				const char* key = dbb->dbb_crypto_manager->getPluginName();
				if (!(info = INF_put_item(item, fb_strlen(key), key, info, end)))
					return;

				continue;
			}

			buffer[0] = item;
			item = isc_info_error;
			length = 1 + INF_convert(isc_adm_task_denied, buffer + 1);
			break;

		case fb_info_conn_flags:
			length = INF_convert(tdbb->getAttachment()->att_remote_flags, buffer);
			break;

		case fb_info_wire_crypt:
			{
				const PathName& nm = tdbb->getAttachment()->att_remote_crypt;
				if (!(info = INF_put_item(item, nm.length(), nm.c_str(), info, end)))
					return;
			}
			continue;

		case fb_info_statement_timeout_db:
			length = INF_convert(dbb->dbb_config->getStatementTimeout(), buffer);
			break;

		case fb_info_statement_timeout_att:
			length = INF_convert(att->getStatementTimeout(), buffer);
			break;

		case fb_info_ses_idle_timeout_db:
			length = INF_convert(dbb->dbb_config->getConnIdleTimeout() * 60, buffer);
			break;

		case fb_info_ses_idle_timeout_att:
			length = INF_convert(att->getIdleTimeout(), buffer);
			break;

		case fb_info_ses_idle_timeout_run:
			length = INF_convert(att->getActualIdleTimeout(), buffer);
			break;

		case fb_info_protocol_version:
			length = INF_convert(0, buffer);
			break;

		case fb_info_features:
			{
				static const unsigned char features[] = ENGINE_FEATURES;
				length = sizeof(features);
				counts_buffer.assign(features, length);
				buffer = counts_buffer.begin();
				break;
			}

		case fb_info_next_attachment:
			length = INF_convert(dbb->getLatestAttachmentId(), buffer);
			break;

		case fb_info_next_statement:
			length = INF_convert(dbb->getLatestStatementId(), buffer);
			break;

		case fb_info_db_guid:
			{
				const auto guidStr = dbb->dbb_guid.toString();
				if (!(info = INF_put_item(item, guidStr.length(), guidStr.c_str(), info, end)))
					return;
			}
			continue;

		case fb_info_db_file_id:
			{
				const auto& fileId = dbb->getUniqueFileId();
				if (!(info = INF_put_item(item, fileId.length(), fileId.c_str(), info, end)))
					return;
			}
			continue;

		case fb_info_replica_mode:
			// fb_info_replica_* reply items are equal to the ReplicaMode enumeration
			*p++ = (UCHAR) dbb->dbb_replica_mode;
			length = p - buffer;
			break;

		case fb_info_username:
			{
				const MetaString& user = att->getUserName();
				if (!(info = INF_put_item(item, user.length(), user.c_str(), info, end)))
					return;
			}
			continue;

		case fb_info_sqlrole:
			{
				const MetaString& role = att->getSqlRole();
				if (!(info = INF_put_item(item, role.length(), role.c_str(), info, end)))
					return;
			}
			continue;

		case fb_info_parallel_workers:
			length = INF_convert(att->att_parallel_workers, buffer);
			break;

		default:
			buffer[0] = item;
			item = isc_info_error;
			length = 1 + INF_convert(isc_infunk, buffer + 1);
			break;
		}

		if (!(info = INF_put_item(item, length, buffer, info, end)))
			return;
	}

	if (info < end)
		*info++ = isc_info_end;
}


UCHAR* INF_put_item(UCHAR item,
					ULONG length,
					const void* data,
					UCHAR* ptr,
					const UCHAR* end,
					const bool inserting)
{
/**************************************
 *
 *	I N F _ p u t _ i t e m
 *
 **************************************
 *
 * Functional description
 *	Put information item in output buffer if there is room, and
 *	return an updated pointer.  If there isn't room for the item,
 *	indicate truncation and return NULL.
 *	If we are inserting, we don't need space for isc_info_end, since it was calculated already.
 *
 **************************************/

	if ((ptr + length + (inserting ? 3 : 4) >= end) || (length > MAX_USHORT))
	{
		if (ptr < end)
		{
			*ptr++ = isc_info_truncated;
			if (ptr < end && !inserting)
				*ptr++ = isc_info_end;
		}
		return NULL;
	}

	*ptr++ = item;
	STUFF_WORD(ptr, length);

	if (length)
	{
		memmove(ptr, data, length);
		ptr += length;
	}

	return ptr;
}


ULONG INF_request_info(const Request* request,
					   const ULONG item_length,
					   const UCHAR* items,
					   const ULONG output_length,
					   UCHAR* info)
{
/**************************************
 *
 *	I N F _ r e q u e s t _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Return information about requests.
 *
 **************************************/
	CHECK_INPUT("INF_request_info");

	ULONG length = 0;

	const UCHAR* const end_items = items + item_length;
	const UCHAR* const end = info + output_length;
	UCHAR* start_info = info;
	const bool infoLengthPresent = items[0] == isc_info_length;

	if (infoLengthPresent)
		++items;

	HalfStaticArray<UCHAR, BUFFER_LARGE> buffer;
	UCHAR* buffer_ptr = buffer.getBuffer(BUFFER_TINY);

	while (items < end_items && *items != isc_info_end && info < end)
	{
		UCHAR item = *items++;

		switch (item)
		{
		case isc_info_end:
			break;

		case isc_info_number_messages:
			//length = INF_convert(request->req_nmsgs, buffer_ptr);
			length = INF_convert(0, buffer_ptr); // never used
			break;

		case isc_info_max_message:
			//length = INF_convert(request->req_mmsg, buffer_ptr);
			length = INF_convert(0, buffer_ptr); // never used
			break;

		case isc_info_max_send:
			//length = INF_convert(request->req_msend, buffer_ptr);
			length = INF_convert(0, buffer_ptr); // never used
			break;

		case isc_info_max_receive:
			//length = INF_convert(request->req_mreceive, buffer_ptr);
			length = INF_convert(0, buffer_ptr); // never used
			break;

		case isc_info_req_select_count:
			length = INF_convert(request->req_records_selected, buffer_ptr);
			break;

		case isc_info_req_insert_count:
			length = INF_convert(request->req_records_inserted, buffer_ptr);
			break;

		case isc_info_req_update_count:
			length = INF_convert(request->req_records_updated, buffer_ptr);
			break;

		case isc_info_req_delete_count:
			length = INF_convert(request->req_records_deleted, buffer_ptr);
			break;

		case isc_info_state:
			if (!(request->req_flags & req_active))
				length = INF_convert(isc_info_req_inactive, buffer_ptr);
			else
			{
				auto state = isc_info_req_active;
				if (request->req_operation == Request::req_send)
					state = isc_info_req_send;
				else if (request->req_operation == Request::req_receive)
				{
					const StmtNode* node = request->req_next;

					if (nodeIs<SelectNode>(node))
						state = isc_info_req_select;
					else
						state = isc_info_req_receive;
				}
				else if ((request->req_operation == Request::req_return) &&
					(request->req_flags & req_stall))
				{
					state = isc_info_req_sql_stall;
				}
				length = INF_convert(state, buffer_ptr);
			}
			break;

		case isc_info_message_number:
		case isc_info_message_size:
			if (!(request->req_flags & req_active) ||
				(request->req_operation != Request::req_receive &&
					request->req_operation != Request::req_send))
			{
				buffer_ptr[0] = item;
				item = isc_info_error;
				length = 1 + INF_convert(isc_infinap, buffer_ptr + 1);
			}
			else
			{
				const auto node = nodeAs<MessageNode>(request->req_message);

				if (node)
				{
					if (item == isc_info_message_number)
						length = INF_convert(node->messageNumber, buffer_ptr);
					else
						length = INF_convert(node->getFormat(request)->fmt_length, buffer_ptr);
				}
				else
					length = 0;
			}
			break;

		default:
			buffer_ptr[0] = item;
			item = isc_info_error;
			length = 1 + INF_convert(isc_infunk, buffer_ptr + 1);
			break;
		}

		if (!(info = INF_put_item(item, length, buffer_ptr, info, end)))
			return 0;
	}

	if (info < end)
		*info++ = isc_info_end;

	if (infoLengthPresent && (end - info >= 7))
	{
		const SLONG number = info - start_info;
		fb_assert(number > 0);
		memmove(start_info + 7, start_info, number);
		info += 7;
		length = INF_convert(number, buffer.begin());
		fb_assert(length == 4); // We only accept SLONG
		INF_put_item(isc_info_length, length, buffer.begin(), start_info, end, true);
	}

	return info - start_info;
}


void INF_transaction_info(const jrd_tra* transaction,
						  const ULONG item_length,
						  const UCHAR* items,
						  const ULONG output_length,
						  UCHAR* info)
{
/**************************************
 *
 *	I N F _ t r a n s a c t i o n _ i n f o
 *
 **************************************
 *
 * Functional description
 *	Process requests for transaction info.
 *
 **************************************/
	CHECK_INPUT("INF_transaction_info");

	UCHAR buffer[MAXPATHLEN];
	ULONG length;

	const UCHAR* const end_items = items + item_length;
	const UCHAR* const end = info + output_length;
	UCHAR* start_info;

	if (items[0] == isc_info_length)
	{
		start_info = info;
		items++;
	}
	else
		start_info = 0;

	while (items < end_items && *items != isc_info_end && info < end)
	{
		UCHAR item = *items++;

		switch (item)
		{
		case isc_info_end:
			break;

		case isc_info_tra_id:
			length = INF_convert(transaction->tra_number, buffer);
			break;

		case isc_info_tra_oldest_interesting:
			length = INF_convert(transaction->tra_oldest, buffer);
			break;

		case isc_info_tra_oldest_snapshot:
			length = INF_convert(transaction->tra_oldest_active, buffer);
			break;

		case isc_info_tra_oldest_active:
			length = INF_convert(
				transaction->tra_lock ? transaction->tra_lock->lck_data : 0,
				buffer);
			break;

		case isc_info_tra_isolation:
		{
			UCHAR* p = buffer;
			if (transaction->tra_flags & TRA_read_committed)
			{
				*p++ = isc_info_tra_read_committed;
				if (transaction->tra_flags & TRA_read_consistency)
					*p++ = isc_info_tra_read_consistency;
				else if (transaction->tra_flags & TRA_rec_version)
					*p++ = isc_info_tra_rec_version;
				else
					*p++ = isc_info_tra_no_rec_version;
			}
			else if (transaction->tra_flags & TRA_degree3)
				*p++ = isc_info_tra_consistency;
			else
				*p++ = isc_info_tra_concurrency;

			length = p - buffer;
			break;
		}

		case isc_info_tra_access:
		{
			UCHAR* p = buffer;
			if (transaction->tra_flags & TRA_readonly)
				*p++ = isc_info_tra_readonly;
			else
				*p++ = isc_info_tra_readwrite;

			length = p - buffer;
			break;
		}

		case isc_info_tra_lock_timeout:
			length = INF_convert(transaction->tra_lock_timeout, buffer);
			break;

		case fb_info_tra_dbpath:
		{
			const auto& dbName = transaction->tra_attachment->att_database->dbb_database_name;
			if (!(info = INF_put_item(item, dbName.length(), dbName.c_str(), info, end)))
				return;
			break;
		}

		case fb_info_tra_snapshot_number:
			length = INF_convert(static_cast<SINT64>(transaction->tra_snapshot_number), buffer);
			break;

		default:
			buffer[0] = item;
			item = isc_info_error;
			length = 1 + INF_convert(isc_infunk, buffer + 1);
			break;
		}

		if (!(info = INF_put_item(item, length, buffer, info, end)))
			return;
	}

	if (info < end)
		*info++ = isc_info_end;

	if (start_info && (end - info >= 7))
	{
		const SLONG number = info - start_info;
		fb_assert(number > 0);
		memmove(start_info + 7, start_info, number);
		length = INF_convert(number, buffer);
		fb_assert(length == 4); // We only accept SLONG
		INF_put_item(isc_info_length, length, buffer, start_info, end, true);
	}
}

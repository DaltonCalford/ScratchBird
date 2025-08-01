/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		stats.cpp
 *	DESCRIPTION:	Record statistics manager
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
 */

#include "scratchbird.h"
#include "ibase.h"

// Database connection - replaced GPRE DATABASE DB = "yachts.lnk";
class DatabaseConnection {
private:
    isc_db_handle db_handle;
    isc_tr_handle tr_handle;
    ISC_STATUS_ARRAY status;
    
public:
    DatabaseConnection() : db_handle(0), tr_handle(0) {
        memset(status, 0, sizeof(status));
    }
    
    ~DatabaseConnection() {
        disconnect();
    }
    
    bool connect(const char* database_path) {
        if (isc_attach_database(status, 0, database_path, &db_handle, 0, nullptr)) {
            isc_print_status(status);
            return false;
        }
        
        if (isc_start_transaction(status, &tr_handle, 1, &db_handle, 0, nullptr)) {
            isc_print_status(status);
            isc_detach_database(status, &db_handle);
            db_handle = 0;
            return false;
        }
        
        return true;
    }
    
    void disconnect() {
        if (tr_handle != 0) {
            isc_commit_transaction(status, &tr_handle);
            tr_handle = 0;
        }
        if (db_handle != 0) {
            isc_detach_database(status, &db_handle);
            db_handle = 0;
        }
    }
    
    isc_db_handle getDbHandle() { return db_handle; }
    isc_tr_handle getTrHandle() { return tr_handle; }
    ISC_STATUS_ARRAY& getStatus() { return status; }
};

extern void* gds__alloc();
extern SLONG gds__vax_integer();

const SSHORT ITEM_seq_reads	= 0;
const SSHORT ITEM_idx_reads	= 1;
const SSHORT ITEM_inserts	= 2;
const SSHORT ITEM_updates	= 3;
const SSHORT ITEM_deletes	= 4;
const SSHORT ITEM_backouts	= 5;
const SSHORT ITEM_purges	= 6;
const SSHORT ITEM_expunges	= 7;
const SSHORT ITEM_count		= 8;

struct fb_stats
{
	SSHORT stats_count;
	SSHORT stats_items;			// Number of item per relation
	SLONG stats_counts[1];
};

typedef int (print_callback)(SCHAR*, SSHORT, SSHORT, const SCHAR* const*, const SLONG*);

static fb_stats* expand_stats(fb_stats** ptr, SSHORT count);
static int get_counts(ISC_STATUS* status_vector, const SCHAR* info, SSHORT length,
		fb_stats** stats_ptr, SSHORT item);
static int print_line(SCHAR* arg, SSHORT relation_id, SSHORT count,
			const SCHAR* const* headers, const SLONG* counts);

static const SCHAR info_request[] =
{
	isc_info_read_seq_count,
	isc_info_read_idx_count,
	isc_info_insert_count,
	isc_info_update_count,
	isc_info_delete_count,
	isc_info_backout_count,
	isc_info_purge_count,
	isc_info_expunge_count,
	isc_info_end
};

static const SCHAR* headers[] =
{
	"S-Reads",
	"I-Reads",
	"Inserts",
	"Updates",
	"Deletes",
	"Backouts",
	"Purges",
	"Expunges"
};

static int* database_handle;
static int* request_handle;


void stats_analyze(const fb_stats* before, const fb_stats* after, print_callback callback, SCHAR* arg)
{
/**************************************
 *
 *	s t a t s _ a n a l y z e
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	SLONG delta[ITEM_count];

	if (!after)
		return;

	const SLONG* const end = delta + ITEM_count;

	const SLONG* tail2 = 0;
	if (before) {
		tail2 = before->stats_counts;
	}

	SSHORT relation_id = 0;
	for (const SLONG* tail = after->stats_counts; relation_id < after->stats_count; ++relation_id)
	{
		SLONG total = 0;
		for (SLONG* p = delta; p < end;)
		{
			total += *tail;
			*p++ = *tail++;
		}
		if (before && relation_id < before->stats_count)
			for (SLONG* p = delta; p < end;)
			{
				total -= *tail2;
				*p++ -= *tail2++;
			}
		if (total) {
			(*callback) (arg, relation_id, ITEM_count, headers, delta);
		}
	}
}


int stats_fetch(SLONG *status_vector, int **db_handle, fb_stats** stats_ptr)
{
/**************************************
 *
 *	s t a t s _ f e t c h
 *
 **************************************
 *
 * Functional description
 *	Gather statistics.
 *
 **************************************/
	SCHAR info_buffer[4096];

	if (isc_database_info(status_vector, db_handle, sizeof(info_request), info_request,
							sizeof(info_buffer), info_buffer))
	{
		return status_vector[1];
	}

	fb_stats* stats = 0;

	for (const SCHAR* p = info_buffer; *p != isc_info_end; )
	{
		const SCHAR item = *p++;
		const SSHORT length = gds__vax_integer(p, 2);
		p += 2;
		switch (item)
		{
		case isc_info_read_seq_count:
			if (get_counts(status_vector, p, length, &stats, ITEM_seq_reads))
				return status_vector[1];
			break;

		case isc_info_read_idx_count:
			if (get_counts(status_vector, p, length, &stats, ITEM_idx_reads))
				return status_vector[1];
			break;

		case isc_info_insert_count:
			if (get_counts(status_vector, p, length, &stats, ITEM_inserts))
				return status_vector[1];
			break;

		case isc_info_update_count:
			if (get_counts(status_vector, p, length, &stats, ITEM_updates))
				return status_vector[1];
			break;

		case isc_info_delete_count:
			if (get_counts(status_vector, p, length, &stats, ITEM_deletes))
				return status_vector[1];
			break;

		case isc_info_backout_count:
			if (get_counts(status_vector, p, length, &stats, ITEM_backouts))
				return status_vector[1];
			break;

		case isc_info_purge_count:
			if (get_counts(status_vector, p, length, &stats, ITEM_purges))
				return status_vector[1];
			break;

		case isc_info_expunge_count:
			if (get_counts(status_vector, p, length, &stats, ITEM_expunges))
				return status_vector[1];
			break;

		default:
			return -1;
		}
		p += length;
	}

	*stats_ptr = stats;

	return 0;
}


static fb_stats* expand_stats(fb_stats** ptr, SSHORT count)
{
/**************************************
 *
 *	e x p a n d _ s t a t s
 *
 **************************************
 *
 * Functional description
 *	Expand a stats structure.
 *
 **************************************/

	fb_stats* stats = *ptr;

	if (stats && count < stats->stats_count)
		return stats;

	const SSHORT size = sizeof(fb_stats) + (count + 10) * ITEM_count * sizeof(SLONG);
	fb_stats* new_stats = (fb_stats*) gds__alloc(size);
	/* FREE: at function return */
	if (!new_stats)
		return NULL;
	zap_longs((SLONG*) new_stats, size / sizeof(SLONG));

	new_stats->stats_count = count + 10;
	new_stats->stats_items = ITEM_count;

	if (stats) {
		memcpy(new_stats->stats_counts, stats->stats_counts,
				stats->stats_count * ITEM_count * sizeof(SLONG));
		gds__free(stats);
	}

	return *ptr = new_stats;
}


static int get_counts(ISC_STATUS* status_vector, const SCHAR* info, SSHORT length,
		fb_stats** stats_ptr, SSHORT item)
{
/**************************************
 *
 *	g e t _ c o u n t s
 *
 **************************************
 *
 * Functional description
 *	Pick up counts for relations.
 *
 **************************************/

	for (const SCHAR* const end = info + length; info < end; )
	{
		const SSHORT relation_id = gds__vax_integer(info, 2);
		info += 2;
		const SLONG count = gds__vax_integer(info, 4);
		info += 4;
		fb_stats* stats = expand_stats(stats_ptr, relation_id);
		if (!stats) {
			status_vector[0] = isc_arg_gds;
			status_vector[1] = isc_virmemexh;
			status_vector[2] = isc_arg_end;
			return status_vector[1];
		}
		stats->stats_counts[relation_id * ITEM_count + item] = count;
	}

	return 0;
}


static int print_line(SCHAR* arg, SSHORT relation_id, SSHORT count,
			const SCHAR* const* headers, const SLONG* counts)
{
/**************************************
 *
 *	p r i n t _ l i n e
 *
 **************************************
 *
 * Functional description
 *	Display data.
 *
 **************************************/
	if (!*arg)
	{
		*arg = 1;
		printf("%32s ", " ");
		for (SSHORT n = count; n; --n) {
			printf("%10s", *headers++);
		}
		printf("\n");
	}

	// Replaced GPRE FOR loop with modern SQL API call
	// Original: FOR (REQUEST_HANDLE request_handle) x IN RDB$RELATIONS WITH x.RDB$RELATION_ID EQ relation_id
	DatabaseConnection db_conn;
	if (db_conn.connect("yachts.lnk")) {
		isc_req_handle req_handle = 0;
		XSQLDA* sqlda = nullptr;
		
		const char* query = "SELECT RDB$RELATION_NAME FROM RDB$RELATIONS WHERE RDB$RELATION_ID = ?";
		
		// Prepare the statement
		if (isc_dsql_allocate_statement(db_conn.getStatus(), &db_conn.getDbHandle(), &req_handle) == 0) {
			if (isc_dsql_prepare(db_conn.getStatus(), &db_conn.getTrHandle(), &req_handle, 0, query, SQL_DIALECT_V6, nullptr) == 0) {
				
				// Allocate input SQLDA for parameter
				XSQLDA* in_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
				in_sqlda->version = SQLDA_VERSION1;
				in_sqlda->sqln = 1;
				in_sqlda->sqld = 1;
				in_sqlda->sqlvar[0].sqltype = SQL_SHORT + 1;  // Nullable short
				in_sqlda->sqlvar[0].sqllen = sizeof(short);
				in_sqlda->sqlvar[0].sqldata = (char*) &relation_id;
				in_sqlda->sqlvar[0].sqlind = nullptr;  // Not null
				
				// Allocate output SQLDA
				sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
				sqlda->version = SQLDA_VERSION1;
				sqlda->sqln = 1;
				
				if (isc_dsql_describe(db_conn.getStatus(), &req_handle, SQL_DIALECT_V6, sqlda) == 0) {
					// Allocate space for relation name
					char relation_name[32];
					sqlda->sqlvar[0].sqldata = relation_name;
					
					if (isc_dsql_execute(db_conn.getStatus(), &db_conn.getTrHandle(), &req_handle, SQL_DIALECT_V6, in_sqlda) == 0) {
						if (isc_dsql_fetch(db_conn.getStatus(), &req_handle, SQL_DIALECT_V6, sqlda) == 0) {
							printf("%32s", relation_name);
							for (const SLONG* const end = counts + count; counts < end; counts++) {
								printf("%10d", *counts);
							}
							printf("\n");
						}
					}
				}
				
				free(in_sqlda);
				free(sqlda);
			}
			isc_dsql_free_statement(db_conn.getStatus(), &req_handle, DSQL_drop);
		}
	}
	// END replaced FOR loop

	return 0;
}


static int zap_longs(SLONG* ptr, SSHORT count)
{
/**************************************
 *
 *	z a p _ l o n g s
 *
 **************************************
 *
 * Functional description
 *	Zero out a bunch of longs.
 *
 **************************************/

	if (count)
		do *ptr++ = 0; while (--count);

	return 0;
}
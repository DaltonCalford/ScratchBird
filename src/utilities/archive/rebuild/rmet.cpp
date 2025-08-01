/*
 *	PROGRAM:	JRD Rebuild scrambled database
 *	MODULE:		rmet.cpp
 *	DESCRIPTION:	Crawl around the guts of a database
 * 
 * CONVERTED FROM GPRE (.epp) TO MODERN C++ DATABASE API
 * 2025.07.28 - ScratchBird GPRE Elimination Project
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
#include "../jrd/jrd.h"
#include "../jrd/tra.h"
#include "../jrd/pag.h"
#include "../utilities/rebuild/rebuild.h"
#include "../utilities/rebuild/rebui_proto.h"
#include "../utilities/rebuild/rmet_proto.h"
#include "../yvalve/gds_proto.h"
#include <vector>
#include <memory>

using namespace ScratchBird;

// Helper class for database connection management
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
        if (tr_handle) {
            isc_rollback_transaction(status, &tr_handle);
        }
        if (db_handle) {
            isc_detach_database(status, &db_handle);
        }
    }
    
    bool connect(const char* database_path) {
        if (isc_attach_database(status, 0, database_path, &db_handle, 0, nullptr)) {
            isc_print_status(status);
            printf("can't open the database so skip the tip list\n");
            return false;
        }
        return true;
    }
    
    bool startTransaction() {
        if (isc_start_transaction(status, &tr_handle, 1, &db_handle, 0, nullptr)) {
            isc_print_status(status);
            printf("can't start a transaction so skip the tip list\n");
            return false;
        }
        return true;
    }
    
    bool commit() {
        if (tr_handle) {
            if (isc_commit_transaction(status, &tr_handle)) {
                isc_print_status(status);
                return false;
            }
            tr_handle = 0;
        }
        return true;
    }
    
    isc_db_handle getDatabase() { return db_handle; }
    isc_tr_handle getTransaction() { return tr_handle; }
    ISC_STATUS_ARRAY& getStatus() { return status; }
};

ULONG* RMET_tips(TEXT* db_in)
{
/**************************************
 *
 *	R M E T _ t i p s
 *
 **************************************
 *
 * Functional description
 *	crawl into the pages relation and
 *	build a list of tips in order
 * 
 * CONVERTED: Replaced GPRE embedded SQL with modern C++ database API
 *
 **************************************/
    
    // Initialize database connection
    DatabaseConnection dbConn;
    
    if (!dbConn.connect(db_in)) {
        return nullptr;
    }
    
    if (!dbConn.startTransaction()) {
        return nullptr;
    }
    
    // First pass: count the number of transaction pages
    const char* count_sql = "SELECT COUNT(*) FROM RDB$PAGES WHERE RDB$PAGE_TYPE = ?";
    isc_stmt_handle count_stmt = 0;
    XSQLDA* count_sqlda = nullptr;
    XSQLDA* count_in_sqlda = nullptr;
    
    try {
        // Allocate SQLDA for output (count)
        count_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
        count_sqlda->version = SQLDA_VERSION1;
        count_sqlda->sqln = 1;
        
        // Allocate SQLDA for input (page type parameter)
        count_in_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
        count_in_sqlda->version = SQLDA_VERSION1;
        count_in_sqlda->sqln = 1;
        
        // Prepare count statement
        if (isc_dsql_allocate_statement(dbConn.getStatus(), &dbConn.getDatabase(), &count_stmt)) {
            isc_print_status(dbConn.getStatus());
            throw std::runtime_error("Failed to allocate statement");
        }
        
        if (isc_dsql_prepare(dbConn.getStatus(), &dbConn.getTransaction(), &count_stmt, 0, count_sql, SQL_DIALECT_V6, count_sqlda)) {
            isc_print_status(dbConn.getStatus());
            throw std::runtime_error("Failed to prepare count statement");
        }
        
        // Describe input parameters
        if (isc_dsql_describe_bind(dbConn.getStatus(), &count_stmt, SQL_DIALECT_V6, count_in_sqlda)) {
            isc_print_status(dbConn.getStatus());
            throw std::runtime_error("Failed to describe bind parameters");
        }
        
        // Set up input parameter (page type)
        SSHORT page_type = pag_transactions;
        SSHORT page_type_ind = 0;
        count_in_sqlda->sqlvar[0].sqldata = (char*)&page_type;
        count_in_sqlda->sqlvar[0].sqlind = &page_type_ind;
        count_in_sqlda->sqlvar[0].sqltype = SQL_SHORT;
        count_in_sqlda->sqlvar[0].sqllen = sizeof(SSHORT);
        
        // Set up output parameter (count)
        ULONG count_result = 0;
        SSHORT count_ind = 0;
        count_sqlda->sqlvar[0].sqldata = (char*)&count_result;
        count_sqlda->sqlvar[0].sqlind = &count_ind;
        count_sqlda->sqlvar[0].sqltype = SQL_LONG;
        count_sqlda->sqlvar[0].sqllen = sizeof(ULONG);
        
        // Execute count query
        if (isc_dsql_execute(dbConn.getStatus(), &dbConn.getTransaction(), &count_stmt, SQL_DIALECT_V6, count_in_sqlda)) {
            isc_print_status(dbConn.getStatus());
            throw std::runtime_error("Failed to execute count query");
        }
        
        // Fetch the count result
        if (isc_dsql_fetch(dbConn.getStatus(), &count_stmt, SQL_DIALECT_V6, count_sqlda)) {
            isc_print_status(dbConn.getStatus());
            throw std::runtime_error("Failed to fetch count result");
        }
        
        ULONG page_count = count_result;
        
        // Free the count statement
        isc_dsql_free_statement(dbConn.getStatus(), &count_stmt, DSQL_drop);
        count_stmt = 0;
        
        if (page_count == 0) {
            free(count_sqlda);
            free(count_in_sqlda);
            return nullptr;
        }
        
        // Allocate memory for the tips array
        ULONG* const tips = (ULONG *) RBDB_alloc((page_count + 1) * sizeof(ULONG));
        ULONG* tip = tips;
        
        // Second pass: retrieve all page numbers
        const char* select_sql = "SELECT RDB$PAGE_NUMBER FROM RDB$PAGES WHERE RDB$PAGE_TYPE = ? ORDER BY RDB$PAGE_NUMBER";
        isc_stmt_handle select_stmt = 0;
        XSQLDA* select_sqlda = nullptr;
        XSQLDA* select_in_sqlda = nullptr;
        
        // Allocate SQLDA for output (page number)
        select_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
        select_sqlda->version = SQLDA_VERSION1;
        select_sqlda->sqln = 1;
        
        // Allocate SQLDA for input (page type parameter)
        select_in_sqlda = (XSQLDA*) malloc(XSQLDA_LENGTH(1));
        select_in_sqlda->version = SQLDA_VERSION1;
        select_in_sqlda->sqln = 1;
        
        // Prepare select statement
        if (isc_dsql_allocate_statement(dbConn.getStatus(), &dbConn.getDatabase(), &select_stmt)) {
            isc_print_status(dbConn.getStatus());
            throw std::runtime_error("Failed to allocate select statement");
        }
        
        if (isc_dsql_prepare(dbConn.getStatus(), &dbConn.getTransaction(), &select_stmt, 0, select_sql, SQL_DIALECT_V6, select_sqlda)) {
            isc_print_status(dbConn.getStatus());
            throw std::runtime_error("Failed to prepare select statement");
        }
        
        // Describe input parameters
        if (isc_dsql_describe_bind(dbConn.getStatus(), &select_stmt, SQL_DIALECT_V6, select_in_sqlda)) {
            isc_print_status(dbConn.getStatus());
            throw std::runtime_error("Failed to describe select bind parameters");
        }
        
        // Set up input parameter (page type)
        select_in_sqlda->sqlvar[0].sqldata = (char*)&page_type;
        select_in_sqlda->sqlvar[0].sqlind = &page_type_ind;
        select_in_sqlda->sqlvar[0].sqltype = SQL_SHORT;
        select_in_sqlda->sqlvar[0].sqllen = sizeof(SSHORT);
        
        // Set up output parameter (page number)
        ULONG page_number = 0;
        SSHORT page_number_ind = 0;
        select_sqlda->sqlvar[0].sqldata = (char*)&page_number;
        select_sqlda->sqlvar[0].sqlind = &page_number_ind;
        select_sqlda->sqlvar[0].sqltype = SQL_LONG;
        select_sqlda->sqlvar[0].sqllen = sizeof(ULONG);
        
        // Execute select query
        if (isc_dsql_execute(dbConn.getStatus(), &dbConn.getTransaction(), &select_stmt, SQL_DIALECT_V6, select_in_sqlda)) {
            isc_print_status(dbConn.getStatus());
            throw std::runtime_error("Failed to execute select query");
        }
        
        // Fetch all page numbers
        ISC_STATUS fetch_status;
        while ((fetch_status = isc_dsql_fetch(dbConn.getStatus(), &select_stmt, SQL_DIALECT_V6, select_sqlda)) == 0) {
            if (page_number_ind == 0) { // Not NULL
                *tip = page_number;
                tip++;
            }
        }
        
        if (fetch_status != 100) { // 100 = no more data
            isc_print_status(dbConn.getStatus());
            printf("can't re-read RDB$PAGES, so skip the tip list\n");
            free(tips);
            throw std::runtime_error("Failed to fetch page numbers");
        }
        
        // Free statement resources
        isc_dsql_free_statement(dbConn.getStatus(), &select_stmt, DSQL_drop);
        
        // Clean up SQLDA structures
        free(count_sqlda);
        free(count_in_sqlda);
        free(select_sqlda);
        free(select_in_sqlda);
        
        // Commit transaction
        if (!dbConn.commit()) {
            free(tips);
            return nullptr;
        }
        
        return tips;
        
    } catch (const std::exception& e) {
        printf("Error in RMET_tips: %s\n", e.what());
        
        // Clean up resources
        if (count_stmt) isc_dsql_free_statement(dbConn.getStatus(), &count_stmt, DSQL_drop);
        if (count_sqlda) free(count_sqlda);
        if (count_in_sqlda) free(count_in_sqlda);
        
        printf("can't read RDB$PAGES, so skip the tip list\n");
        return nullptr;
    }
}
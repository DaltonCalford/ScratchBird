# Firebird SQL C++ Interface Specification

## Overview

Firebird is an open-source SQL relational database management system that runs on Linux, Windows, and various Unix platforms. It offers excellent concurrency, high performance, and powerful language support for stored procedures and triggers.

## Connection Libraries

### 1. Firebird C API (Native)

The native Firebird API provides direct access to the database engine.

#### Installation

##### Linux (Debian/Ubuntu)
```bash
sudo apt-get update
sudo apt-get install firebird3.0-server firebird3.0-dev firebird3.0-common
sudo apt-get install libfbclient2 libib-util
```

##### Linux (RHEL/CentOS)
```bash
sudo yum install firebird firebird-devel firebird-libfbclient
```

##### Windows
Download from: https://firebirdsql.org/en/firebird-3-0/
Install both server and client tools.

##### macOS
```bash
brew install firebird
```

#### Header Files Required
```cpp
extern "C" {
    #include <ibase.h>      // Main Firebird API
    #include <iberror.h>    // Error codes
    #include <ib_util.h>    // Utility functions
}
```

### 2. IBPP - Firebird C++ Library

IBPP is a C++ client interface for Firebird Server.

#### Installation

##### Download and Build
```bash
# Download IBPP
wget http://www.ibpp.org/downloads/ibpp-2-5-3-1-src.zip
unzip ibpp-2-5-3-1-src.zip
cd ibpp

# Build
make -f makefile.linux  # or makefile.darwin for macOS
```

#### Header Files Required
```cpp
#include <ibpp.h>
```

### 3. Firebird OO API (Object-Oriented Interface)

Firebird 3.0+ provides an object-oriented C++ API.

#### Header Files Required
```cpp
#include <firebird/Interface.h>
#include <firebird/Message.h>
using namespace Firebird;
```

## Connection Parameters

### Connection String Format

#### Local Connection
```
database.fdb
/path/to/database.fdb
```

#### TCP/IP Connection
```
hostname:database_path
hostname/port:database_path
```

#### Embedded Connection
```
database.fdb
```

### Parameters Table

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| server | string | Server hostname or IP | localhost |
| port | int | Server port number | 3050 |
| database | string | Database file path | - |
| user | string | Username (SYSDBA for admin) | SYSDBA |
| password | string | User password | masterkey |
| role | string | SQL role name | - |
| charset | string | Character set | UTF8 |
| dialect | int | SQL dialect (1 or 3) | 3 |
| page_size | int | Database page size | 16384 |
| buffers | int | Number of cache buffers | 0 (default) |
| connection_timeout | int | Connection timeout in seconds | 10 |
| dummy_packet_interval | int | Keep-alive interval | 0 |
| no_garbage_collect | bool | Disable garbage collection | false |
| no_db_triggers | bool | Disable database triggers | false |
| sql_role_name | string | SQL role to use | - |
| lc_ctype | string | Locale character type | - |
| wire_crypt | string | Wire encryption (Enabled/Disabled/Required) | Enabled |
| auth_plugin_list | string | Authentication plugins | Srp256,Srp,Legacy_Auth |

## Complete Native API Implementation

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstring>
#include <sstream>

extern "C" {
    #include <ibase.h>
}

class FirebirdConnection {
private:
    isc_db_handle db_handle;
    isc_tr_handle tr_handle;
    ISC_STATUS_ARRAY status;
    bool connected;
    
    // Error handling
    void checkError(const std::string& operation) {
        if (status[0] == 1 && status[1]) {
            char msg[512];
            const ISC_STATUS* p = status;
            
            fb_interpret(msg, sizeof(msg), &p);
            std::string error_msg = operation + " failed: " + msg;
            
            // Get additional errors
            while (fb_interpret(msg, sizeof(msg), &p)) {
                error_msg += "\n" + std::string(msg);
            }
            
            throw std::runtime_error(error_msg);
        }
    }
    
    // Convert string to Firebird format
    std::string toFirebirdString(const std::string& str, size_t maxLen = 0) {
        if (maxLen > 0 && str.length() > maxLen) {
            return str.substr(0, maxLen);
        }
        return str;
    }
    
public:
    FirebirdConnection() : db_handle(0), tr_handle(0), connected(false) {
        memset(status, 0, sizeof(status));
    }
    
    ~FirebirdConnection() {
        disconnect();
    }
    
    // Connect to database
    bool connect(const std::string& server, const std::string& database,
                 const std::string& user, const std::string& password,
                 int dialect = 3, const std::string& charset = "UTF8") {
        
        try {
            // Build database parameter buffer (DPB)
            char dpb_buffer[256];
            char* dpb = dpb_buffer;
            
            *dpb++ = isc_dpb_version1;
            
            // Add user name
            *dpb++ = isc_dpb_user_name;
            *dpb++ = user.length();
            memcpy(dpb, user.c_str(), user.length());
            dpb += user.length();
            
            // Add password
            *dpb++ = isc_dpb_password;
            *dpb++ = password.length();
            memcpy(dpb, password.c_str(), password.length());
            dpb += password.length();
            
            // Add character set
            if (!charset.empty()) {
                *dpb++ = isc_dpb_lc_ctype;
                *dpb++ = charset.length();
                memcpy(dpb, charset.c_str(), charset.length());
                dpb += charset.length();
            }
            
            // Set SQL dialect
            *dpb++ = isc_dpb_sql_dialect;
            *dpb++ = 1;
            *dpb++ = dialect;
            
            short dpb_length = dpb - dpb_buffer;
            
            // Build connection string
            std::string conn_str;
            if (!server.empty() && server != "localhost") {
                conn_str = server + ":" + database;
            } else {
                conn_str = database;
            }
            
            // Attach to database
            if (isc_attach_database(status, conn_str.length(), conn_str.c_str(),
                                   &db_handle, dpb_length, dpb_buffer)) {
                checkError("Database connection");
                return false;
            }
            
            connected = true;
            std::cout << "Connected to Firebird database successfully" << std::endl;
            
            // Start default transaction
            startTransaction();
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Connection error: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Start transaction
    bool startTransaction(int isolation = isc_tpb_read_committed) {
        if (!connected) return false;
        
        // End existing transaction if any
        if (tr_handle != 0) {
            commitTransaction();
        }
        
        // Build transaction parameter buffer (TPB)
        char tpb[] = {
            isc_tpb_version3,
            isc_tpb_write,
            isc_tpb_wait,
            static_cast<char>(isolation),
            isc_tpb_rec_version
        };
        
        if (isc_start_transaction(status, &tr_handle, 1, &db_handle,
                                  sizeof(tpb), tpb)) {
            checkError("Start transaction");
            return false;
        }
        
        return true;
    }
    
    // Commit transaction
    bool commitTransaction() {
        if (tr_handle == 0) return true;
        
        if (isc_commit_transaction(status, &tr_handle)) {
            checkError("Commit transaction");
            return false;
        }
        
        tr_handle = 0;
        return true;
    }
    
    // Rollback transaction
    bool rollbackTransaction() {
        if (tr_handle == 0) return true;
        
        if (isc_rollback_transaction(status, &tr_handle)) {
            checkError("Rollback transaction");
            return false;
        }
        
        tr_handle = 0;
        return true;
    }
    
    // Execute query and return results
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query) {
        std::vector<std::map<std::string, std::string>> results;
        
        if (!connected) {
            std::cerr << "Not connected to database" << std::endl;
            return results;
        }
        
        isc_stmt_handle stmt_handle = 0;
        XSQLDA* sqlda = nullptr;
        
        try {
            // Allocate statement handle
            if (isc_dsql_allocate_statement(status, &db_handle, &stmt_handle)) {
                checkError("Allocate statement");
                return results;
            }
            
            // Prepare statement
            if (isc_dsql_prepare(status, &tr_handle, &stmt_handle, 0,
                               query.c_str(), SQL_DIALECT_V6, nullptr)) {
                checkError("Prepare statement");
                isc_dsql_free_statement(status, &stmt_handle, DSQL_drop);
                return results;
            }
            
            // Allocate and prepare SQLDA for output
            sqlda = (XSQLDA*)malloc(XSQLDA_LENGTH(20));
            sqlda->version = SQLDA_VERSION1;
            sqlda->sqln = 20;
            
            // Describe the statement
            if (isc_dsql_describe(status, &stmt_handle, SQL_DIALECT_V6, sqlda)) {
                checkError("Describe statement");
                free(sqlda);
                isc_dsql_free_statement(status, &stmt_handle, DSQL_drop);
                return results;
            }
            
            // Reallocate SQLDA if needed
            if (sqlda->sqld > sqlda->sqln) {
                int n = sqlda->sqld;
                free(sqlda);
                sqlda = (XSQLDA*)malloc(XSQLDA_LENGTH(n));
                sqlda->version = SQLDA_VERSION1;
                sqlda->sqln = n;
                
                if (isc_dsql_describe(status, &stmt_handle, SQL_DIALECT_V6, sqlda)) {
                    checkError("Re-describe statement");
                    free(sqlda);
                    isc_dsql_free_statement(status, &stmt_handle, DSQL_drop);
                    return results;
                }
            }
            
            // Allocate memory for data
            for (int i = 0; i < sqlda->sqld; i++) {
                XSQLVAR* var = &sqlda->sqlvar[i];
                
                switch (var->sqltype & ~1) {
                    case SQL_TEXT:
                    case SQL_VARYING:
                        var->sqldata = (char*)malloc(var->sqllen + 2);
                        break;
                    case SQL_SHORT:
                        var->sqldata = (char*)malloc(sizeof(short));
                        break;
                    case SQL_LONG:
                        var->sqldata = (char*)malloc(sizeof(long));
                        break;
                    case SQL_INT64:
                        var->sqldata = (char*)malloc(sizeof(ISC_INT64));
                        break;
                    case SQL_FLOAT:
                        var->sqldata = (char*)malloc(sizeof(float));
                        break;
                    case SQL_DOUBLE:
                        var->sqldata = (char*)malloc(sizeof(double));
                        break;
                    case SQL_TIMESTAMP:
                        var->sqldata = (char*)malloc(sizeof(ISC_TIMESTAMP));
                        break;
                    case SQL_TYPE_DATE:
                        var->sqldata = (char*)malloc(sizeof(ISC_DATE));
                        break;
                    case SQL_TYPE_TIME:
                        var->sqldata = (char*)malloc(sizeof(ISC_TIME));
                        break;
                    case SQL_BLOB:
                        var->sqldata = (char*)malloc(sizeof(ISC_QUAD));
                        break;
                    default:
                        var->sqldata = (char*)malloc(var->sqllen);
                        break;
                }
                
                if (var->sqltype & 1) {
                    var->sqlind = (short*)malloc(sizeof(short));
                }
            }
            
            // Execute statement
            if (isc_dsql_execute(status, &tr_handle, &stmt_handle, SQL_DIALECT_V6, nullptr)) {
                checkError("Execute statement");
                // Free allocated memory
                for (int i = 0; i < sqlda->sqld; i++) {
                    free(sqlda->sqlvar[i].sqldata);
                    if (sqlda->sqlvar[i].sqlind) {
                        free(sqlda->sqlvar[i].sqlind);
                    }
                }
                free(sqlda);
                isc_dsql_free_statement(status, &stmt_handle, DSQL_drop);
                return results;
            }
            
            // Fetch results
            while (isc_dsql_fetch(status, &stmt_handle, SQL_DIALECT_V6, sqlda) == 0) {
                std::map<std::string, std::string> row;
                
                for (int i = 0; i < sqlda->sqld; i++) {
                    XSQLVAR* var = &sqlda->sqlvar[i];
                    std::string column_name(var->aliasname, var->aliasname_length);
                    
                    // Check for NULL
                    if ((var->sqltype & 1) && (*var->sqlind == -1)) {
                        row[column_name] = "NULL";
                        continue;
                    }
                    
                    // Convert data to string based on type
                    std::stringstream value;
                    
                    switch (var->sqltype & ~1) {
                        case SQL_TEXT: {
                            std::string text(var->sqldata, var->sqllen);
                            // Trim trailing spaces
                            text.erase(text.find_last_not_of(" ") + 1);
                            value << text;
                            break;
                        }
                        case SQL_VARYING: {
                            short len = *(short*)var->sqldata;
                            std::string text(var->sqldata + 2, len);
                            value << text;
                            break;
                        }
                        case SQL_SHORT:
                            value << *(short*)var->sqldata;
                            break;
                        case SQL_LONG:
                            value << *(long*)var->sqldata;
                            break;
                        case SQL_INT64:
                            value << *(ISC_INT64*)var->sqldata;
                            break;
                        case SQL_FLOAT:
                            value << *(float*)var->sqldata;
                            break;
                        case SQL_DOUBLE:
                            value << *(double*)var->sqldata;
                            break;
                        case SQL_TIMESTAMP: {
                            ISC_TIMESTAMP* timestamp = (ISC_TIMESTAMP*)var->sqldata;
                            struct tm time_tm;
                            isc_decode_timestamp(timestamp, &time_tm);
                            char buffer[80];
                            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &time_tm);
                            value << buffer;
                            break;
                        }
                        case SQL_TYPE_DATE: {
                            ISC_DATE* date = (ISC_DATE*)var->sqldata;
                            struct tm time_tm;
                            isc_decode_sql_date(date, &time_tm);
                            char buffer[80];
                            strftime(buffer, sizeof(buffer), "%Y-%m-%d", &time_tm);
                            value << buffer;
                            break;
                        }
                        case SQL_TYPE_TIME: {
                            ISC_TIME* time = (ISC_TIME*)var->sqldata;
                            struct tm time_tm;
                            isc_decode_sql_time(time, &time_tm);
                            char buffer[80];
                            strftime(buffer, sizeof(buffer), "%H:%M:%S", &time_tm);
                            value << buffer;
                            break;
                        }
                        case SQL_BLOB:
                            value << "BLOB";
                            break;
                        default:
                            value << "UNKNOWN";
                            break;
                    }
                    
                    row[column_name] = value.str();
                }
                
                results.push_back(row);
            }
            
            // Clean up
            for (int i = 0; i < sqlda->sqld; i++) {
                free(sqlda->sqlvar[i].sqldata);
                if (sqlda->sqlvar[i].sqlind) {
                    free(sqlda->sqlvar[i].sqlind);
                }
            }
            free(sqlda);
            
            // Free statement
            isc_dsql_free_statement(status, &stmt_handle, DSQL_drop);
            
        } catch (const std::exception& e) {
            std::cerr << "Query execution error: " << e.what() << std::endl;
            
            // Clean up on error
            if (sqlda) {
                for (int i = 0; i < sqlda->sqld; i++) {
                    if (sqlda->sqlvar[i].sqldata) free(sqlda->sqlvar[i].sqldata);
                    if (sqlda->sqlvar[i].sqlind) free(sqlda->sqlvar[i].sqlind);
                }
                free(sqlda);
            }
            
            if (stmt_handle) {
                isc_dsql_free_statement(status, &stmt_handle, DSQL_drop);
            }
        }
        
        return results;
    }
    
    // Execute non-query statement (INSERT, UPDATE, DELETE)
    int executeUpdate(const std::string& query) {
        if (!connected) {
            std::cerr << "Not connected to database" << std::endl;
            return -1;
        }
        
        isc_stmt_handle stmt_handle = 0;
        int affected_rows = 0;
        
        try {
            // Allocate statement handle
            if (isc_dsql_allocate_statement(status, &db_handle, &stmt_handle)) {
                checkError("Allocate statement");
                return -1;
            }
            
            // Prepare and execute statement
            if (isc_dsql_execute_immediate(status, &db_handle, &tr_handle, 0,
                                          query.c_str(), SQL_DIALECT_V6, nullptr)) {
                checkError("Execute immediate");
                isc_dsql_free_statement(status, &stmt_handle, DSQL_drop);
                return -1;
            }
            
            // Get affected rows count
            char info_buffer[20];
            char item[] = {isc_info_sql_records};
            
            if (isc_dsql_sql_info(status, &stmt_handle, sizeof(item), item,
                                 sizeof(info_buffer), info_buffer) == 0) {
                
                char* p = info_buffer;
                if (*p == isc_info_sql_records) {
                    p++;
                    int length = isc_vax_integer(p, 2);
                    p += 2;
                    
                    while (p < info_buffer + length + 3) {
                        char type = *p++;
                        int count = isc_vax_integer(p, 4);
                        p += 4;
                        
                        switch (type) {
                            case isc_info_req_insert_count:
                            case isc_info_req_update_count:
                            case isc_info_req_delete_count:
                                affected_rows += count;
                                break;
                        }
                    }
                }
            }
            
            // Free statement
            isc_dsql_free_statement(status, &stmt_handle, DSQL_drop);
            
        } catch (const std::exception& e) {
            std::cerr << "Execute update error: " << e.what() << std::endl;
            if (stmt_handle) {
                isc_dsql_free_statement(status, &stmt_handle, DSQL_drop);
            }
            return -1;
        }
        
        return affected_rows;
    }
    
    // Create database
    bool createDatabase(const std::string& database_path, const std::string& user,
                       const std::string& password, int page_size = 16384,
                       const std::string& charset = "UTF8") {
        
        // Build create database statement
        std::stringstream create_stmt;
        create_stmt << "CREATE DATABASE '" << database_path << "' ";
        create_stmt << "USER '" << user << "' ";
        create_stmt << "PASSWORD '" << password << "' ";
        create_stmt << "PAGE_SIZE " << page_size << " ";
        create_stmt << "DEFAULT CHARACTER SET " << charset;
        
        if (isc_dsql_execute_immediate(status, &db_handle, &tr_handle, 0,
                                      create_stmt.str().c_str(), SQL_DIALECT_V6, nullptr)) {
            checkError("Create database");
            return false;
        }
        
        return true;
    }
    
    // Backup database
    bool backupDatabase(const std::string& backup_file, bool verbose = false) {
        if (!connected) return false;
        
        isc_svc_handle svc_handle = 0;
        
        try {
            // Connect to service manager
            std::string svc_name = "service_mgr";
            char spb_buffer[256];
            char* spb = spb_buffer;
            
            *spb++ = isc_spb_version;
            *spb++ = isc_spb_current_version;
            
            // Add user and password
            *spb++ = isc_spb_user_name;
            *spb++ = strlen("SYSDBA");
            strcpy(spb, "SYSDBA");
            spb += strlen("SYSDBA");
            
            *spb++ = isc_spb_password;
            *spb++ = strlen("masterkey");
            strcpy(spb, "masterkey");
            spb += strlen("masterkey");
            
            short spb_length = spb - spb_buffer;
            
            if (isc_service_attach(status, 0, svc_name.c_str(), &svc_handle,
                                  spb_length, spb_buffer)) {
                checkError("Service attach");
                return false;
            }
            
            // Start backup
            char backup_buffer[1024];
            char* backup_spb = backup_buffer;
            
            *backup_spb++ = isc_action_svc_backup;
            
            // Add database name
            *backup_spb++ = isc_spb_dbname;
            short len = strlen(db_handle);
            *backup_spb++ = len & 0xFF;
            *backup_spb++ = (len >> 8) & 0xFF;
            memcpy(backup_spb, db_handle, len);
            backup_spb += len;
            
            // Add backup file
            *backup_spb++ = isc_spb_bkp_file;
            len = backup_file.length();
            *backup_spb++ = len & 0xFF;
            *backup_spb++ = (len >> 8) & 0xFF;
            memcpy(backup_spb, backup_file.c_str(), len);
            backup_spb += len;
            
            if (verbose) {
                *backup_spb++ = isc_spb_verbose;
            }
            
            short backup_spb_length = backup_spb - backup_buffer;
            
            if (isc_service_start(status, &svc_handle, nullptr,
                                 backup_spb_length, backup_buffer)) {
                checkError("Start backup");
                isc_service_detach(status, &svc_handle);
                return false;
            }
            
            // Wait for backup to complete
            char query_buffer[] = {isc_info_svc_line};
            char result_buffer[1024];
            
            do {
                if (isc_service_query(status, &svc_handle, nullptr, 0, nullptr,
                                     sizeof(query_buffer), query_buffer,
                                     sizeof(result_buffer), result_buffer)) {
                    checkError("Query service");
                    break;
                }
                
                if (verbose && result_buffer[0] == isc_info_svc_line) {
                    int len = isc_vax_integer(&result_buffer[1], 2);
                    if (len > 0) {
                        std::cout << std::string(&result_buffer[3], len) << std::endl;
                    }
                }
                
            } while (result_buffer[0] == isc_info_svc_line);
            
            // Detach from service
            isc_service_detach(status, &svc_handle);
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Backup error: " << e.what() << std::endl;
            if (svc_handle) {
                isc_service_detach(status, &svc_handle);
            }
            return false;
        }
    }
    
    // Get database metadata
    std::map<std::string, std::string> getDatabaseInfo() {
        std::map<std::string, std::string> info;
        
        if (!connected) return info;
        
        char items[] = {
            isc_info_db_id,
            isc_info_page_size,
            isc_info_num_buffers,
            isc_info_current_memory,
            isc_info_max_memory,
            isc_info_allocation,
            isc_info_attachment_id,
            isc_info_ods_version,
            isc_info_ods_minor_version,
            isc_info_db_sql_dialect,
            isc_info_end
        };
        
        char buffer[512];
        
        if (isc_database_info(status, &db_handle, sizeof(items), items,
                             sizeof(buffer), buffer)) {
            checkError("Get database info");
            return info;
        }
        
        char* p = buffer;
        while (*p != isc_info_end) {
            char item = *p++;
            int length = isc_vax_integer(p, 2);
            p += 2;
            
            switch (item) {
                case isc_info_page_size:
                    info["page_size"] = std::to_string(isc_vax_integer(p, length));
                    break;
                case isc_info_num_buffers:
                    info["num_buffers"] = std::to_string(isc_vax_integer(p, length));
                    break;
                case isc_info_current_memory:
                    info["current_memory"] = std::to_string(isc_vax_integer(p, length));
                    break;
                case isc_info_max_memory:
                    info["max_memory"] = std::to_string(isc_vax_integer(p, length));
                    break;
                case isc_info_allocation:
                    info["allocation"] = std::to_string(isc_vax_integer(p, length));
                    break;
                case isc_info_attachment_id:
                    info["attachment_id"] = std::to_string(isc_vax_integer(p, length));
                    break;
                case isc_info_ods_version:
                    info["ods_version"] = std::to_string(isc_vax_integer(p, length));
                    break;
                case isc_info_ods_minor_version:
                    info["ods_minor_version"] = std::to_string(isc_vax_integer(p, length));
                    break;
                case isc_info_db_sql_dialect:
                    info["sql_dialect"] = std::to_string(isc_vax_integer(p, length));
                    break;
                case isc_info_db_id:
                    info["database_id"] = std::string(p + 2, p[1]);
                    break;
            }
            
            p += length;
        }
        
        return info;
    }
    
    // Get tables
    std::vector<std::string> getTables() {
        std::vector<std::string> tables;
        
        std::string query = 
            "SELECT RDB$RELATION_NAME FROM RDB$RELATIONS "
            "WHERE RDB$SYSTEM_FLAG = 0 AND RDB$VIEW_BLR IS NULL "
            "ORDER BY RDB$RELATION_NAME";
        
        auto results = executeQuery(query);
        
        for (const auto& row : results) {
            std::string table_name = row.at("RDB$RELATION_NAME");
            // Trim trailing spaces
            table_name.erase(table_name.find_last_not_of(" ") + 1);
            tables.push_back(table_name);
        }
        
        return tables;
    }
    
    // Disconnect from database
    void disconnect() {
        if (tr_handle != 0) {
            isc_rollback_transaction(status, &tr_handle);
            tr_handle = 0;
        }
        
        if (db_handle != 0) {
            isc_detach_database(status, &db_handle);
            db_handle = 0;
        }
        
        connected = false;
        std::cout << "Disconnected from Firebird database" << std::endl;
    }
    
    bool isConnected() const {
        return connected;
    }
};
```

## Complete IBPP Implementation

```cpp
#include <ibpp.h>
#include <iostream>
#include <vector>
#include <map>
#include <string>

class FirebirdIBPP {
private:
    IBPP::Database db;
    IBPP::Transaction tr;
    
public:
    FirebirdIBPP() {}
    
    bool connect(const std::string& server, const std::string& database,
                 const std::string& user, const std::string& password) {
        try {
            // Create database object
            db = IBPP::DatabaseFactory(server, database, user, password);
            
            // Set additional parameters
            db->Dialect(3);  // SQL Dialect 3
            db->Charset("UTF8");
            
            // Connect
            db->Connect();
            
            // Create transaction
            tr = IBPP::TransactionFactory(db);
            tr->Start();
            
            std::cout << "Connected via IBPP successfully" << std::endl;
            return true;
            
        } catch (IBPP::Exception& e) {
            std::cerr << "IBPP Error: " << e.what() << std::endl;
            return false;
        }
    }
    
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query) {
        std::vector<std::map<std::string, std::string>> results;
        
        try {
            IBPP::Statement st = IBPP::StatementFactory(db, tr);
            st->Execute(query);
            
            // Get column count
            int columns = st->Columns();
            
            // Get column names
            std::vector<std::string> columnNames;
            for (int i = 1; i <= columns; ++i) {
                columnNames.push_back(st->ColumnAlias(i));
            }
            
            // Fetch rows
            while (st->Fetch()) {
                std::map<std::string, std::string> row;
                
                for (int i = 1; i <= columns; ++i) {
                    std::string value;
                    
                    if (st->IsNull(i)) {
                        value = "NULL";
                    } else {
                        switch (st->ColumnType(i)) {
                            case IBPP::sdString:
                                st->Get(i, value);
                                break;
                            case IBPP::sdSmallint: {
                                int16_t val;
                                st->Get(i, val);
                                value = std::to_string(val);
                                break;
                            }
                            case IBPP::sdInteger: {
                                int32_t val;
                                st->Get(i, val);
                                value = std::to_string(val);
                                break;
                            }
                            case IBPP::sdLargeint: {
                                int64_t val;
                                st->Get(i, val);
                                value = std::to_string(val);
                                break;
                            }
                            case IBPP::sdFloat: {
                                float val;
                                st->Get(i, val);
                                value = std::to_string(val);
                                break;
                            }
                            case IBPP::sdDouble: {
                                double val;
                                st->Get(i, val);
                                value = std::to_string(val);
                                break;
                            }
                            case IBPP::sdDate: {
                                IBPP::Date date;
                                st->Get(i, date);
                                value = std::to_string(date.Year()) + "-" +
                                       std::to_string(date.Month()) + "-" +
                                       std::to_string(date.Day());
                                break;
                            }
                            case IBPP::sdTime: {
                                IBPP::Time time;
                                st->Get(i, time);
                                value = std::to_string(time.Hours()) + ":" +
                                       std::to_string(time.Minutes()) + ":" +
                                       std::to_string(time.Seconds());
                                break;
                            }
                            case IBPP::sdTimestamp: {
                                IBPP::Timestamp ts;
                                st->Get(i, ts);
                                value = std::to_string(ts.Year()) + "-" +
                                       std::to_string(ts.Month()) + "-" +
                                       std::to_string(ts.Day()) + " " +
                                       std::to_string(ts.Hours()) + ":" +
                                       std::to_string(ts.Minutes()) + ":" +
                                       std::to_string(ts.Seconds());
                                break;
                            }
                            case IBPP::sdBlob:
                                value = "BLOB";
                                break;
                            case IBPP::sdArray:
                                value = "ARRAY";
                                break;
                            default:
                                value = "UNKNOWN";
                        }
                    }
                    
                    row[columnNames[i-1]] = value;
                }
                
                results.push_back(row);
            }
            
        } catch (IBPP::Exception& e) {
            std::cerr << "Query error: " << e.what() << std::endl;
        }
        
        return results;
    }
    
    int executeUpdate(const std::string& query) {
        try {
            IBPP::Statement st = IBPP::StatementFactory(db, tr);
            st->Execute(query);
            return st->AffectedRows();
            
        } catch (IBPP::Exception& e) {
            std::cerr << "Update error: " << e.what() << std::endl;
            return -1;
        }
    }
    
    void commit() {
        if (tr->Started()) {
            tr->Commit();
            tr->Start();  // Start new transaction
        }
    }
    
    void rollback() {
        if (tr->Started()) {
            tr->Rollback();
            tr->Start();  // Start new transaction
        }
    }
    
    void disconnect() {
        if (tr && tr->Started()) {
            tr->Rollback();
        }
        
        if (db && db->Connected()) {
            db->Disconnect();
        }
        
        std::cout << "Disconnected from Firebird (IBPP)" << std::endl;
    }
};
```

## Usage Example

```cpp
int main() {
    // Using native API
    FirebirdConnection fbNative;
    
    if (fbNative.connect("localhost", "/var/lib/firebird/3.0/data/test.fdb",
                        "SYSDBA", "masterkey")) {
        
        // Create table
        fbNative.executeUpdate(
            "CREATE TABLE IF NOT EXISTS users ("
            "id INTEGER NOT NULL PRIMARY KEY,"
            "username VARCHAR(50) NOT NULL UNIQUE,"
            "email VARCHAR(100),"
            "age INTEGER,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")"
        );
        
        // Insert data
        fbNative.executeUpdate(
            "INSERT INTO users (id, username, email, age) "
            "VALUES (1, 'john_doe', 'john@example.com', 30)"
        );
        
        // Query data
        auto results = fbNative.executeQuery("SELECT * FROM users");
        
        for (const auto& row : results) {
            for (const auto& [key, value] : row) {
                std::cout << key << ": " << value << " ";
            }
            std::cout << std::endl;
        }
        
        // Commit and disconnect
        fbNative.commitTransaction();
        fbNative.disconnect();
    }
    
    // Using IBPP
    FirebirdIBPP fbIBPP;
    
    if (fbIBPP.connect("localhost", "/var/lib/firebird/3.0/data/test.fdb",
                       "SYSDBA", "masterkey")) {
        
        auto results = fbIBPP.executeQuery("SELECT * FROM users");
        
        for (const auto& row : results) {
            for (const auto& [key, value] : row) {
                std::cout << key << ": " << value << " ";
            }
            std::cout << std::endl;
        }
        
        fbIBPP.commit();
        fbIBPP.disconnect();
    }
    
    return 0;
}
```

## Compilation

### Using Native API
```bash
g++ -std=c++17 -o firebird_app firebird_app.cpp \
    -lfbclient \
    -L/usr/lib/x86_64-linux-gnu \
    -I/usr/include/firebird
```

### Using IBPP
```bash
g++ -std=c++17 -o firebird_app firebird_app.cpp \
    ibpp_src/*.cpp \
    -lfbclient \
    -L/usr/lib/x86_64-linux-gnu \
    -I/usr/include/firebird \
    -Iibpp_src
```

### CMake Configuration
```cmake
cmake_minimum_required(VERSION 3.10)
project(FirebirdApp)

set(CMAKE_CXX_STANDARD 17)

# Find Firebird
find_path(FIREBIRD_INCLUDE_DIR ibase.h
    PATHS /usr/include /usr/include/firebird /opt/firebird/include)

find_library(FIREBIRD_LIBRARY
    NAMES fbclient fbembed
    PATHS /usr/lib /usr/lib/x86_64-linux-gnu /opt/firebird/lib)

if(FIREBIRD_INCLUDE_DIR AND FIREBIRD_LIBRARY)
    message(STATUS "Found Firebird: ${FIREBIRD_LIBRARY}")
else()
    message(FATAL_ERROR "Firebird not found")
endif()

add_executable(firebird_app main.cpp)

target_include_directories(firebird_app PRIVATE ${FIREBIRD_INCLUDE_DIR})
target_link_libraries(firebird_app ${FIREBIRD_LIBRARY})
```

## Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| 335544321 | isc_arith_except | Arithmetic exception |
| 335544322 | isc_bad_dbkey | Bad DBKEY |
| 335544323 | isc_bad_db_format | File is not a valid database |
| 335544324 | isc_bad_db_handle | Invalid database handle |
| 335544325 | isc_bad_dpb_content | Bad parameters on attach |
| 335544326 | isc_bad_dpb_form | Unrecognized database parameter block |
| 335544327 | isc_bad_req_handle | Invalid request handle |
| 335544328 | isc_bad_segstr_handle | Invalid BLOB handle |
| 335544329 | isc_bad_segstr_id | Invalid BLOB ID |
| 335544330 | isc_bad_tpb_content | Invalid parameter in transaction parameter block |
| 335544331 | isc_bad_tpb_form | Invalid format for transaction parameter block |
| 335544332 | isc_bad_trans_handle | Invalid transaction handle |
| 335544333 | isc_bug_check | Internal consistency check |
| 335544334 | isc_convert_error | Conversion error |
| 335544335 | isc_db_corrupt | Database file appears corrupt |
| 335544336 | isc_deadlock | Deadlock |
| 335544337 | isc_excess_trans | Attempt to start more than 32767 transactions |
| 335544338 | isc_from_no_match | No match for first value expression |
| 335544339 | isc_infinap | Information type inappropriate |
| 335544340 | isc_infona | No information available |
| 335544341 | isc_infunk | Unknown information item |
| 335544342 | isc_integ_fail | Action violates CHECK constraint |
| 335544343 | isc_invalid_blr | Invalid BLR at offset |
| 335544344 | isc_io_error | I/O error during operation |
| 335544345 | isc_lock_conflict | Lock conflict |
| 335544346 | isc_metadata_corrupt | Corrupt system table |
| 335544347 | isc_not_valid | Validation error |
| 335544348 | isc_no_cur_rec | No current record |
| 335544349 | isc_no_dup | Attempt to store duplicate value |
| 335544350 | isc_no_finish | Program attempted to exit without finishing database |
| 335544351 | isc_no_meta_update | Unsuccessful metadata update |
| 335544352 | isc_no_priv | No permission |
| 335544353 | isc_no_recon | Transaction is not in limbo |
| 335544354 | isc_no_record | Record not found |
| 335544355 | isc_no_segstr_close | BLOB was not closed |
| 335544356 | isc_foreign_key | Foreign key violation |
| 335544357 | isc_high_minor | Minor version too high |
| 335544358 | isc_tra_state | Transaction in invalid state |
| 335544359 | isc_trans_invalid | Invalid transaction handle |
| 335544360 | isc_buf_invalid | Invalid buffer handle |

## Firebird-Specific Features

### 1. Generators/Sequences
```cpp
// Create generator
db.executeUpdate("CREATE GENERATOR user_id_gen");

// Set generator value
db.executeUpdate("SET GENERATOR user_id_gen TO 100");

// Use generator
db.executeUpdate(
    "INSERT INTO users (id, username) "
    "VALUES (GEN_ID(user_id_gen, 1), 'new_user')"
);
```

### 2. Stored Procedures
```cpp
// Create stored procedure
db.executeUpdate(R"(
    CREATE PROCEDURE get_user_by_id (user_id INTEGER)
    RETURNS (username VARCHAR(50), email VARCHAR(100))
    AS
    BEGIN
        SELECT username, email FROM users
        WHERE id = :user_id
        INTO :username, :email;
        SUSPEND;
    END
)");

// Call stored procedure
auto results = db.executeQuery("SELECT * FROM get_user_by_id(1)");
```

### 3. Triggers
```cpp
// Create trigger
db.executeUpdate(R"(
    CREATE TRIGGER users_bi FOR users
    ACTIVE BEFORE INSERT POSITION 0
    AS
    BEGIN
        IF (NEW.id IS NULL) THEN
            NEW.id = GEN_ID(user_id_gen, 1);
        NEW.created_at = CURRENT_TIMESTAMP;
    END
)");
```

### 4. Events
```cpp
// Post event
db.executeUpdate("POST_EVENT 'user_updated'");

// Register event handler (using native API)
ISC_EVENT_HANDLE event_handle;
char* event_buffer;
char* result_buffer;

char* events[] = {"user_updated"};
isc_event_block(&event_buffer, &result_buffer, 1, events[0]);

isc_que_events(status, &db_handle, &event_handle, 
              strlen(event_buffer), event_buffer,
              event_callback, result_buffer);
```

**Firebird note:** Event notifications are delivered on commit only. ScratchBird
Firebird emulation follows this behavior and does not support immediate delivery
or event message payloads.

### 5. External Tables
```cpp
// Create external table
db.executeUpdate(R"(
    CREATE TABLE ext_data
    EXTERNAL FILE '/path/to/data.txt' (
        line CHAR(80)
    )
)");
```

## Performance Optimization

1. **Connection Pooling**: Maintain a pool of connections
2. **Prepared Statements**: Use for repeated queries
3. **Sweep Interval**: Configure automatic sweep
4. **Page Buffers**: Adjust cache size
5. **Forced Writes**: Consider disabling for performance (risky)
6. **Index Statistics**: Update regularly
7. **Backup/Restore**: Periodic backup/restore cycle for optimization

## Security Features

1. **SQL Security**: INVOKER or DEFINER rights
2. **Role-Based Access**: Create and assign roles
3. **Wire Encryption**: Enable wire protocol encryption
4. **Authentication Plugins**: SRP, Legacy_Auth, Win_Sspi
5. **Database Encryption**: Available in Firebird 3.0+
6. **Monitoring Tables**: MON$ tables for activity monitoring

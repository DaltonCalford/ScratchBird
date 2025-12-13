/**
 * @file odbc_handles.cpp
 * @brief ODBC Handle Implementation
 *
 * Part of Phase 3.8: ODBC Driver
 */

#include "scratchbird/odbc/odbc_handles.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <regex>

// Helper for casting pointers to integers in ODBC attributes
#define ODBC_PTR_TO_UINT(p) static_cast<SQLUINTEGER>(reinterpret_cast<uintptr_t>(p))
#define ODBC_PTR_TO_ULEN(p) static_cast<SQLULEN>(reinterpret_cast<uintptr_t>(p))

namespace scratchbird {
namespace odbc {

// =============================================================================
// OdbcHandle Base Implementation
// =============================================================================

void OdbcHandle::addDiagnostic(const DiagnosticRecord& record) {
    std::lock_guard lock(diagnostics_mutex_);
    diagnostics_.push_back(record);
}

void OdbcHandle::clearDiagnostics() {
    std::lock_guard lock(diagnostics_mutex_);
    diagnostics_.clear();
}

SQLSMALLINT OdbcHandle::getDiagnosticCount() const {
    std::lock_guard lock(diagnostics_mutex_);
    return static_cast<SQLSMALLINT>(diagnostics_.size());
}

const DiagnosticRecord* OdbcHandle::getDiagnostic(SQLSMALLINT rec_number) const {
    std::lock_guard lock(diagnostics_mutex_);
    if (rec_number < 1 || static_cast<size_t>(rec_number) > diagnostics_.size()) {
        return nullptr;
    }
    return &diagnostics_[rec_number - 1];
}

void OdbcHandle::setError(const std::string& sqlstate, SQLINTEGER native_error,
                          const std::string& message) {
    DiagnosticRecord rec;
    rec.sqlstate = sqlstate;
    rec.native_error = native_error;
    rec.message = message;
    addDiagnostic(rec);
}

// =============================================================================
// OdbcEnvironment Implementation
// =============================================================================

OdbcEnvironment::OdbcEnvironment() = default;

OdbcEnvironment::~OdbcEnvironment() {
    std::lock_guard lock(connections_mutex_);
    connections_.clear();
}

SQLRETURN OdbcEnvironment::setAttribute(SQLINTEGER attribute, SQLPOINTER value,
                                         SQLINTEGER /*string_length*/) {
    clearDiagnostics();

    switch (attribute) {
        case SQL_ATTR_ODBC_VERSION:
            odbc_version_ = ODBC_PTR_TO_UINT(value);
            if (odbc_version_ != SQL_OV_ODBC2 &&
                odbc_version_ != SQL_OV_ODBC3 &&
                odbc_version_ != SQL_OV_ODBC3_80) {
                setError("HY024", 0, "Invalid attribute value");
                return SQL_ERROR;
            }
            break;

        case SQL_ATTR_CONNECTION_POOLING:
            connection_pooling_ = ODBC_PTR_TO_UINT(value);
            break;

        case SQL_ATTR_CP_MATCH:
            cp_match_ = ODBC_PTR_TO_UINT(value);
            break;

        case SQL_ATTR_OUTPUT_NTS:
            output_nts_ = (ODBC_PTR_TO_UINT(value) != 0);
            break;

        default:
            setError("HY092", 0, "Invalid attribute identifier");
            return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcEnvironment::getAttribute(SQLINTEGER attribute, SQLPOINTER value,
                                         SQLINTEGER buffer_length,
                                         SQLINTEGER* string_length) {
    clearDiagnostics();

    switch (attribute) {
        case SQL_ATTR_ODBC_VERSION:
            if (value) {
                *static_cast<SQLUINTEGER*>(value) = odbc_version_;
            }
            if (string_length) {
                *string_length = sizeof(SQLUINTEGER);
            }
            break;

        case SQL_ATTR_CONNECTION_POOLING:
            if (value) {
                *static_cast<SQLUINTEGER*>(value) = connection_pooling_;
            }
            if (string_length) {
                *string_length = sizeof(SQLUINTEGER);
            }
            break;

        case SQL_ATTR_CP_MATCH:
            if (value) {
                *static_cast<SQLUINTEGER*>(value) = cp_match_;
            }
            if (string_length) {
                *string_length = sizeof(SQLUINTEGER);
            }
            break;

        case SQL_ATTR_OUTPUT_NTS:
            if (value) {
                *static_cast<SQLUINTEGER*>(value) = output_nts_ ? 1 : 0;
            }
            if (string_length) {
                *string_length = sizeof(SQLUINTEGER);
            }
            break;

        default:
            setError("HY092", 0, "Invalid attribute identifier");
            return SQL_ERROR;
    }

    (void)buffer_length;  // Not used for these attributes
    return SQL_SUCCESS;
}

OdbcConnection* OdbcEnvironment::createConnection() {
    std::lock_guard lock(connections_mutex_);
    auto conn = std::make_unique<OdbcConnection>(this);
    auto* ptr = conn.get();
    connections_.push_back(std::move(conn));
    return ptr;
}

void OdbcEnvironment::removeConnection(OdbcConnection* conn) {
    std::lock_guard lock(connections_mutex_);
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
                       [conn](const auto& c) { return c.get() == conn; }),
        connections_.end());
}

size_t OdbcEnvironment::getConnectionCount() const {
    std::lock_guard lock(connections_mutex_);
    return connections_.size();
}

// =============================================================================
// OdbcConnection Implementation
// =============================================================================

OdbcConnection::OdbcConnection(OdbcEnvironment* env)
    : env_(env) {}

OdbcConnection::~OdbcConnection() {
    if (connected_) {
        disconnect();
    }
}

SQLRETURN OdbcConnection::connect(const SQLCHAR* dsn, SQLSMALLINT dsn_len,
                                   const SQLCHAR* user, SQLSMALLINT user_len,
                                   const SQLCHAR* password, SQLSMALLINT password_len) {
    clearDiagnostics();

    if (connected_) {
        setError("08002", 0, "Connection already open");
        return SQL_ERROR;
    }

    // Extract DSN name
    std::string dsn_str;
    if (dsn) {
        dsn_str = (dsn_len == SQL_NTS) ?
            std::string(reinterpret_cast<const char*>(dsn)) :
            std::string(reinterpret_cast<const char*>(dsn), dsn_len);
    }

    // Extract user
    if (user) {
        params_.user = (user_len == SQL_NTS) ?
            std::string(reinterpret_cast<const char*>(user)) :
            std::string(reinterpret_cast<const char*>(user), user_len);
    }

    // Extract password
    if (password) {
        params_.password = (password_len == SQL_NTS) ?
            std::string(reinterpret_cast<const char*>(password)) :
            std::string(reinterpret_cast<const char*>(password), password_len);
    }

    // TODO: Look up DSN in odbc.ini to get connection parameters
    // For now, use DSN as server name
    params_.dsn = dsn_str;
    if (!dsn_str.empty() && params_.server.empty()) {
        params_.server = dsn_str;
    }

    return establishConnection();
}

SQLRETURN OdbcConnection::driverConnect(HWND /*window_handle*/,
                                         const SQLCHAR* conn_str, SQLSMALLINT conn_str_len,
                                         SQLCHAR* out_conn_str, SQLSMALLINT out_buffer_len,
                                         SQLSMALLINT* out_conn_str_len,
                                         SQLUSMALLINT /*driver_completion*/) {
    clearDiagnostics();

    if (connected_) {
        setError("08002", 0, "Connection already open");
        return SQL_ERROR;
    }

    // Parse connection string
    std::string conn_str_s;
    if (conn_str) {
        conn_str_s = (conn_str_len == SQL_NTS) ?
            std::string(reinterpret_cast<const char*>(conn_str)) :
            std::string(reinterpret_cast<const char*>(conn_str), conn_str_len);
    }

    auto result = parseConnectionString(conn_str_s);
    if (result != SQL_SUCCESS) {
        return result;
    }

    result = establishConnection();
    if (result != SQL_SUCCESS) {
        return result;
    }

    // Build output connection string
    std::string out_str = buildConnectionString();
    if (out_conn_str && out_buffer_len > 0) {
        size_t copy_len = std::min(static_cast<size_t>(out_buffer_len - 1), out_str.size());
        std::memcpy(out_conn_str, out_str.c_str(), copy_len);
        out_conn_str[copy_len] = '\0';
        if (out_str.size() >= static_cast<size_t>(out_buffer_len)) {
            setError("01004", 0, "String data, right truncated");
            result = SQL_SUCCESS_WITH_INFO;
        }
    }
    if (out_conn_str_len) {
        *out_conn_str_len = static_cast<SQLSMALLINT>(out_str.size());
    }

    return result;
}

SQLRETURN OdbcConnection::browseConnect(const SQLCHAR* in_conn_str, SQLSMALLINT in_conn_str_len,
                                         SQLCHAR* out_conn_str, SQLSMALLINT out_buffer_len,
                                         SQLSMALLINT* out_conn_str_len) {
    // For simplicity, delegate to driverConnect
    return driverConnect(nullptr, in_conn_str, in_conn_str_len,
                         out_conn_str, out_buffer_len, out_conn_str_len,
                         SQL_DRIVER_NOPROMPT);
}

SQLRETURN OdbcConnection::disconnect() {
    clearDiagnostics();

    if (!connected_) {
        setError("08003", 0, "Connection not open");
        return SQL_ERROR;
    }

    // Close all statements
    {
        std::lock_guard lock(statements_mutex_);
        statements_.clear();
    }

    // Close socket
    if (socket_fd_ >= 0) {
#ifdef _WIN32
        // closesocket(socket_fd_);
#else
        // close(socket_fd_);
#endif
        socket_fd_ = -1;
    }
    connected_ = false;
    connection_dead_ = false;
    in_transaction_ = false;

    return SQL_SUCCESS;
}

SQLRETURN OdbcConnection::setAttribute(SQLINTEGER attribute, SQLPOINTER value,
                                        SQLINTEGER string_length) {
    clearDiagnostics();

    switch (attribute) {
        case SQL_ATTR_ACCESS_MODE:
            access_mode_ = ODBC_PTR_TO_UINT(value);
            break;

        case SQL_ATTR_AUTOCOMMIT:
            auto_commit_ = ODBC_PTR_TO_UINT(value);
            if (connected_ && in_transaction_ && auto_commit_ == SQL_AUTOCOMMIT_ON) {
                // Commit current transaction
                endTransaction(SQL_COMMIT);
            }
            break;

        case SQL_ATTR_LOGIN_TIMEOUT:
            login_timeout_ = ODBC_PTR_TO_UINT(value);
            break;

        case SQL_ATTR_CONNECTION_TIMEOUT:
            connection_timeout_ = ODBC_PTR_TO_UINT(value);
            break;

        case SQL_ATTR_TXN_ISOLATION:
            txn_isolation_ = ODBC_PTR_TO_UINT(value);
            if (connected_) {
                // TODO: Send SET TRANSACTION ISOLATION LEVEL to server
            }
            break;

        case SQL_ATTR_CURRENT_CATALOG:
            if (value) {
                current_database_ = (string_length == SQL_NTS) ?
                    std::string(reinterpret_cast<const char*>(value)) :
                    std::string(reinterpret_cast<const char*>(value), string_length);
                if (connected_) {
                    // TODO: Send USE database to server
                }
            }
            break;

        case SQL_ATTR_PACKET_SIZE:
            if (!connected_) {
                packet_size_ = ODBC_PTR_TO_UINT(value);
            } else {
                setError("HY011", 0, "Attribute cannot be set now");
                return SQL_ERROR;
            }
            break;

        case SQL_ATTR_METADATA_ID:
            metadata_id_ = (ODBC_PTR_TO_UINT(value) != 0);
            break;

        case SQL_ATTR_TRACE:
        case SQL_ATTR_TRACEFILE:
        case SQL_ATTR_TRANSLATE_LIB:
        case SQL_ATTR_TRANSLATE_OPTION:
        case SQL_ATTR_QUIET_MODE:
        case SQL_ATTR_ODBC_CURSORS:
            // Handled by Driver Manager
            break;

        default:
            setError("HY092", 0, "Invalid attribute identifier");
            return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcConnection::getAttribute(SQLINTEGER attribute, SQLPOINTER value,
                                        SQLINTEGER buffer_length,
                                        SQLINTEGER* string_length) {
    clearDiagnostics();

    auto copyString = [&](const std::string& str) -> SQLRETURN {
        if (string_length) {
            *string_length = static_cast<SQLINTEGER>(str.size());
        }
        if (value && buffer_length > 0) {
            size_t copy_len = std::min(static_cast<size_t>(buffer_length - 1), str.size());
            std::memcpy(value, str.c_str(), copy_len);
            static_cast<char*>(value)[copy_len] = '\0';
            if (str.size() >= static_cast<size_t>(buffer_length)) {
                setError("01004", 0, "String data, right truncated");
                return SQL_SUCCESS_WITH_INFO;
            }
        }
        return SQL_SUCCESS;
    };

    switch (attribute) {
        case SQL_ATTR_ACCESS_MODE:
            if (value) *static_cast<SQLUINTEGER*>(value) = access_mode_;
            if (string_length) *string_length = sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_AUTOCOMMIT:
            if (value) *static_cast<SQLUINTEGER*>(value) = auto_commit_;
            if (string_length) *string_length = sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_LOGIN_TIMEOUT:
            if (value) *static_cast<SQLUINTEGER*>(value) = login_timeout_;
            if (string_length) *string_length = sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_CONNECTION_TIMEOUT:
            if (value) *static_cast<SQLUINTEGER*>(value) = connection_timeout_;
            if (string_length) *string_length = sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_TXN_ISOLATION:
            if (value) *static_cast<SQLUINTEGER*>(value) = txn_isolation_;
            if (string_length) *string_length = sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_CURRENT_CATALOG:
            return copyString(current_database_);

        case SQL_ATTR_PACKET_SIZE:
            if (value) *static_cast<SQLUINTEGER*>(value) = packet_size_;
            if (string_length) *string_length = sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_CONNECTION_DEAD:
            if (value) *static_cast<SQLUINTEGER*>(value) = connection_dead_ ? 1 : 0;
            if (string_length) *string_length = sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_METADATA_ID:
            if (value) *static_cast<SQLUINTEGER*>(value) = metadata_id_ ? 1 : 0;
            if (string_length) *string_length = sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_AUTO_IPD:
            if (value) *static_cast<SQLUINTEGER*>(value) = 1;  // We support auto IPD
            if (string_length) *string_length = sizeof(SQLUINTEGER);
            break;

        default:
            setError("HY092", 0, "Invalid attribute identifier");
            return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcConnection::endTransaction(SQLSMALLINT completion_type) {
    clearDiagnostics();

    if (!connected_) {
        setError("08003", 0, "Connection not open");
        return SQL_ERROR;
    }

    if (auto_commit_ == SQL_AUTOCOMMIT_ON && !in_transaction_) {
        // No explicit transaction to commit/rollback
        return SQL_SUCCESS;
    }

    std::string sql;
    if (completion_type == SQL_COMMIT) {
        sql = "COMMIT";
    } else if (completion_type == SQL_ROLLBACK) {
        sql = "ROLLBACK";
    } else {
        setError("HY012", 0, "Invalid transaction operation code");
        return SQL_ERROR;
    }

    // Execute commit/rollback
    std::vector<std::vector<std::string>> results;
    std::vector<ColumnMetadata> columns;
    SQLLEN rows_affected;
    auto result = executeSQL(sql, results, columns, rows_affected);
    if (result == SQL_SUCCESS) {
        in_transaction_ = false;
    }

    return result;
}

SQLRETURN OdbcConnection::beginTransaction() {
    clearDiagnostics();

    if (!connected_) {
        setError("08003", 0, "Connection not open");
        return SQL_ERROR;
    }

    if (in_transaction_) {
        return SQL_SUCCESS;  // Already in transaction
    }

    std::vector<std::vector<std::string>> results;
    std::vector<ColumnMetadata> columns;
    SQLLEN rows_affected;
    auto result = executeSQL("BEGIN TRANSACTION", results, columns, rows_affected);
    if (result == SQL_SUCCESS) {
        in_transaction_ = true;
    }

    return result;
}

SQLRETURN OdbcConnection::getInfo(SQLUSMALLINT info_type, SQLPOINTER info_value,
                                   SQLSMALLINT buffer_length, SQLSMALLINT* string_length) {
    clearDiagnostics();

    auto copyString = [&](const char* str) -> SQLRETURN {
        size_t len = std::strlen(str);
        if (string_length) {
            *string_length = static_cast<SQLSMALLINT>(len);
        }
        if (info_value && buffer_length > 0) {
            size_t copy_len = std::min(static_cast<size_t>(buffer_length - 1), len);
            std::memcpy(info_value, str, copy_len);
            static_cast<char*>(info_value)[copy_len] = '\0';
            if (len >= static_cast<size_t>(buffer_length)) {
                setError("01004", 0, "String data, right truncated");
                return SQL_SUCCESS_WITH_INFO;
            }
        }
        return SQL_SUCCESS;
    };

    auto setUSmallInt = [&](SQLUSMALLINT val) {
        if (info_value) *static_cast<SQLUSMALLINT*>(info_value) = val;
        if (string_length) *string_length = sizeof(SQLUSMALLINT);
    };

    auto setUInteger = [&](SQLUINTEGER val) {
        if (info_value) *static_cast<SQLUINTEGER*>(info_value) = val;
        if (string_length) *string_length = sizeof(SQLUINTEGER);
    };

    switch (info_type) {
        // Driver Information
        case SQL_DRIVER_NAME:
            return copyString(DriverConfig::DRIVER_NAME);
        case SQL_DRIVER_VER:
            return copyString(DriverConfig::DRIVER_VERSION);
        case SQL_DRIVER_ODBC_VER:
            return copyString(DriverConfig::ODBC_VERSION);
        case SQL_ODBC_VER:
            return copyString(DriverConfig::ODBC_VERSION);

        // DBMS Information
        case SQL_DBMS_NAME:
            return copyString(DriverConfig::DBMS_NAME);
        case SQL_DBMS_VER:
            return copyString(DriverConfig::DBMS_VERSION);
        case SQL_DATABASE_NAME:
            return copyString(current_database_.c_str());
        case SQL_SERVER_NAME:
            return copyString(params_.server.c_str());
        case SQL_USER_NAME:
            return copyString(current_user_.c_str());
        case SQL_DATA_SOURCE_NAME:
            return copyString(params_.dsn.c_str());

        // Capabilities
        case SQL_DATA_SOURCE_READ_ONLY:
            return copyString(access_mode_ == SQL_MODE_READ_ONLY ? "Y" : "N");
        case SQL_ACCESSIBLE_TABLES:
            return copyString("Y");
        case SQL_ACCESSIBLE_PROCEDURES:
            return copyString("Y");
        case SQL_MULT_RESULT_SETS:
            return copyString("Y");
        case SQL_MULTIPLE_ACTIVE_TXN:
            return copyString("Y");
        case SQL_PROCEDURES:
            return copyString("Y");
        case SQL_CATALOG_NAME:
            return copyString("Y");
        case SQL_COLUMN_ALIAS:
            return copyString("Y");
        case SQL_LIKE_ESCAPE_CLAUSE:
            return copyString("Y");
        case SQL_ORDER_BY_COLUMNS_IN_SELECT:
            return copyString("N");
        case SQL_OUTER_JOINS:
            return copyString("Y");
        case SQL_ROW_UPDATES:
            return copyString("N");
        case SQL_EXPRESSIONS_IN_ORDERBY:
            return copyString("Y");
        case SQL_INTEGRITY:
            return copyString("Y");

        // Identifier info
        case SQL_IDENTIFIER_QUOTE_CHAR:
            return copyString("\"");
        case SQL_CATALOG_NAME_SEPARATOR:
            return copyString(".");
        case SQL_CATALOG_TERM:
            return copyString("database");
        case SQL_SCHEMA_TERM:
            return copyString("schema");
        case SQL_TABLE_TERM:
            return copyString("table");
        case SQL_PROCEDURE_TERM:
            return copyString("function");
        case SQL_SEARCH_PATTERN_ESCAPE:
            return copyString("\\");
        case SQL_SPECIAL_CHARACTERS:
            return copyString("_");

        // Limits
        case SQL_MAX_CATALOG_NAME_LEN:
            setUSmallInt(DriverConfig::MAX_CATALOG_NAME_LEN);
            break;
        case SQL_MAX_SCHEMA_NAME_LEN:
            setUSmallInt(DriverConfig::MAX_SCHEMA_NAME_LEN);
            break;
        case SQL_MAX_TABLE_NAME_LEN:
            setUSmallInt(DriverConfig::MAX_TABLE_NAME_LEN);
            break;
        case SQL_MAX_COLUMN_NAME_LEN:
            setUSmallInt(DriverConfig::MAX_COLUMN_NAME_LEN);
            break;
        case SQL_MAX_COLUMNS_IN_INDEX:
            setUSmallInt(DriverConfig::MAX_COLUMNS_IN_INDEX);
            break;
        case SQL_MAX_COLUMNS_IN_TABLE:
            setUSmallInt(DriverConfig::MAX_COLUMNS_IN_TABLE);
            break;
        case SQL_MAX_STATEMENT_LEN:
            setUInteger(0);  // Unlimited
            break;
        case SQL_MAX_CONCURRENT_ACTIVITIES:
            setUSmallInt(0);  // Unlimited
            break;
        case SQL_MAX_DRIVER_CONNECTIONS:
            setUSmallInt(0);  // Unlimited
            break;
        case SQL_MAX_IDENTIFIER_LEN:
            setUSmallInt(128);
            break;

        // Transaction support
        case SQL_TXN_CAPABLE:
            setUSmallInt(2);  // SQL_TC_ALL
            break;
        case SQL_TXN_ISOLATION_OPTION:
            setUInteger(SQL_TXN_READ_UNCOMMITTED | SQL_TXN_READ_COMMITTED |
                       SQL_TXN_REPEATABLE_READ | SQL_TXN_SERIALIZABLE);
            break;
        case SQL_DEFAULT_TXN_ISOLATION:
            setUInteger(SQL_TXN_READ_COMMITTED);
            break;

        // SQL Conformance
        case SQL_SQL_CONFORMANCE:
            setUInteger(8);  // SQL_SC_SQL92_FULL
            break;
        case SQL_ODBC_API_CONFORMANCE:
            setUSmallInt(2);  // SQL_OAC_LEVEL2
            break;
        case SQL_ODBC_SQL_CONFORMANCE:
            setUSmallInt(2);  // SQL_OSC_EXTENDED
            break;

        // Identifier case
        case SQL_IDENTIFIER_CASE:
            setUSmallInt(2);  // SQL_IC_LOWER
            break;
        case SQL_QUOTED_IDENTIFIER_CASE:
            setUSmallInt(3);  // SQL_IC_SENSITIVE
            break;

        // Concatenation behavior
        case SQL_CONCAT_NULL_BEHAVIOR:
            setUSmallInt(0);  // SQL_CB_NULL
            break;

        // Correlation names
        case SQL_CORRELATION_NAME:
            setUSmallInt(2);  // SQL_CN_ANY
            break;

        // Group by
        case SQL_GROUP_BY:
            setUSmallInt(2);  // SQL_GB_GROUP_BY_CONTAINS_SELECT
            break;

        // Null collation
        case SQL_NULL_COLLATION:
            setUSmallInt(0);  // SQL_NC_HIGH
            break;

        // Cursor behavior
        case SQL_CURSOR_COMMIT_BEHAVIOR:
            setUSmallInt(0);  // SQL_CB_DELETE
            break;
        case SQL_CURSOR_ROLLBACK_BEHAVIOR:
            setUSmallInt(0);  // SQL_CB_DELETE
            break;

        // Non-nullable columns
        case SQL_NON_NULLABLE_COLUMNS:
            setUSmallInt(1);  // SQL_NNC_NON_NULL
            break;

        // Need long data length
        case SQL_NEED_LONG_DATA_LEN:
            return copyString("N");

        default:
            setError("HY096", 0, "Information type out of range");
            return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcConnection::getFunctions(SQLUSMALLINT function_id, SQLUSMALLINT* supported) {
    clearDiagnostics();

    if (!supported) {
        setError("HY009", 0, "Invalid use of null pointer");
        return SQL_ERROR;
    }

    // All ODBC 3.x core functions are supported
    static const SQLUSMALLINT supported_functions[] = {
        1,   // SQLAllocHandle
        2,   // SQLBindCol
        3,   // SQLBindParameter
        4,   // SQLBrowseConnect
        5,   // SQLBulkOperations
        6,   // SQLCancel
        7,   // SQLCloseCursor
        8,   // SQLColAttribute
        9,   // SQLColumnPrivileges
        10,  // SQLColumns
        11,  // SQLConnect
        12,  // SQLCopyDesc
        13,  // SQLDescribeCol
        14,  // SQLDescribeParam
        15,  // SQLDisconnect
        16,  // SQLDriverConnect
        17,  // SQLEndTran
        18,  // SQLExecDirect
        19,  // SQLExecute
        20,  // SQLFetch
        21,  // SQLFetchScroll
        22,  // SQLForeignKeys
        23,  // SQLFreeHandle
        24,  // SQLFreeStmt
        25,  // SQLGetConnectAttr
        26,  // SQLGetCursorName
        27,  // SQLGetData
        28,  // SQLGetDescField
        29,  // SQLGetDescRec
        30,  // SQLGetDiagField
        31,  // SQLGetDiagRec
        32,  // SQLGetEnvAttr
        33,  // SQLGetFunctions
        34,  // SQLGetInfo
        35,  // SQLGetStmtAttr
        36,  // SQLGetTypeInfo
        37,  // SQLMoreResults
        38,  // SQLNativeSql
        39,  // SQLNumParams
        40,  // SQLNumResultCols
        41,  // SQLParamData
        42,  // SQLPrepare
        43,  // SQLPrimaryKeys
        44,  // SQLProcedureColumns
        45,  // SQLProcedures
        46,  // SQLPutData
        47,  // SQLRowCount
        48,  // SQLSetConnectAttr
        49,  // SQLSetCursorName
        50,  // SQLSetDescField
        51,  // SQLSetDescRec
        52,  // SQLSetEnvAttr
        53,  // SQLSetPos
        54,  // SQLSetStmtAttr
        55,  // SQLSpecialColumns
        56,  // SQLStatistics
        57,  // SQLTablePrivileges
        58,  // SQLTables
    };

    if (function_id == 0) {
        // SQL_API_ALL_FUNCTIONS - not supported, use SQL_API_ODBC3_ALL_FUNCTIONS
        *supported = 0;
    } else if (function_id == 999) {
        // SQL_API_ODBC3_ALL_FUNCTIONS - return bitmap
        // For simplicity, just mark all as supported
        std::memset(supported, 0xFF, 250);
    } else {
        // Check specific function
        *supported = 0;
        for (auto func : supported_functions) {
            if (func == function_id) {
                *supported = 1;
                break;
            }
        }
    }

    return SQL_SUCCESS;
}

OdbcStatement* OdbcConnection::createStatement() {
    std::lock_guard lock(statements_mutex_);
    auto stmt = std::make_unique<OdbcStatement>(this);
    auto* ptr = stmt.get();
    statements_.push_back(std::move(stmt));
    return ptr;
}

void OdbcConnection::removeStatement(OdbcStatement* stmt) {
    std::lock_guard lock(statements_mutex_);
    statements_.erase(
        std::remove_if(statements_.begin(), statements_.end(),
                       [stmt](const auto& s) { return s.get() == stmt; }),
        statements_.end());
}

size_t OdbcConnection::getStatementCount() const {
    std::lock_guard lock(statements_mutex_);
    return statements_.size();
}

SQLRETURN OdbcConnection::parseConnectionString(const std::string& conn_str) {
    // Parse key=value pairs separated by semicolons
    std::regex pair_regex(R"(([^=;]+)=([^;]*))");
    std::smatch match;
    std::string::const_iterator search_start = conn_str.cbegin();

    while (std::regex_search(search_start, conn_str.cend(), match, pair_regex)) {
        std::string key = match[1].str();
        std::string value = match[2].str();

        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t"));
            s.erase(s.find_last_not_of(" \t") + 1);
        };
        trim(key);
        trim(value);

        // Convert key to uppercase for comparison
        std::string key_upper = key;
        std::transform(key_upper.begin(), key_upper.end(), key_upper.begin(), ::toupper);

        // Remove braces from driver name
        if (value.size() >= 2 && value.front() == '{' && value.back() == '}') {
            value = value.substr(1, value.size() - 2);
        }

        if (key_upper == "DRIVER") {
            params_.driver = value;
        } else if (key_upper == "DSN") {
            params_.dsn = value;
        } else if (key_upper == "SERVER" || key_upper == "HOST") {
            params_.server = value;
        } else if (key_upper == "PORT") {
            params_.port = static_cast<uint16_t>(std::stoi(value));
        } else if (key_upper == "DATABASE" || key_upper == "DB") {
            params_.database = value;
        } else if (key_upper == "UID" || key_upper == "USER") {
            params_.user = value;
        } else if (key_upper == "PWD" || key_upper == "PASSWORD") {
            params_.password = value;
        } else if (key_upper == "SSL" || key_upper == "SSLMODE") {
            params_.ssl_mode = value;
        } else if (key_upper == "SSLCERT") {
            params_.ssl_cert = value;
        } else if (key_upper == "SSLKEY") {
            params_.ssl_key = value;
        } else if (key_upper == "SSLROOTCERT") {
            params_.ssl_root_cert = value;
        } else if (key_upper == "PROTOCOL") {
            params_.protocol = value;
        } else if (key_upper == "TIMEOUT" || key_upper == "CONNECTTIMEOUT") {
            params_.connect_timeout = static_cast<uint32_t>(std::stoi(value));
        } else if (key_upper == "QUERYTIMEOUT") {
            params_.query_timeout = static_cast<uint32_t>(std::stoi(value));
        } else if (key_upper == "APPLICATIONNAME" || key_upper == "APP") {
            params_.application_name = value;
        } else if (key_upper == "SCHEMA" || key_upper == "CURRENTSCHEMA") {
            params_.schema = value;
        } else if (key_upper == "CHARSET" || key_upper == "ENCODING") {
            params_.charset = value;
        } else if (key_upper == "READONLY") {
            params_.read_only = (value == "true" || value == "1" || value == "yes");
        } else if (key_upper == "AUTOCOMMIT") {
            params_.auto_commit = (value == "true" || value == "1" || value == "yes");
        } else if (key_upper == "PACKETSIZE") {
            params_.packet_size = static_cast<uint32_t>(std::stoi(value));
        } else if (key_upper == "POOLING") {
            params_.pooling = (value == "true" || value == "1" || value == "yes");
        }

        search_start = match.suffix().first;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcConnection::establishConnection() {
    // TODO: Implement actual connection to ScratchBird server
    // For now, just simulate a successful connection

    // Set connection state
    connected_ = true;
    current_database_ = params_.database;
    current_user_ = params_.user;
    current_schema_ = params_.schema;

    // Apply settings
    if (params_.read_only) {
        access_mode_ = SQL_MODE_READ_ONLY;
    }
    auto_commit_ = params_.auto_commit ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF;
    login_timeout_ = params_.connect_timeout;

    return SQL_SUCCESS;
}

std::string OdbcConnection::buildConnectionString() const {
    std::ostringstream ss;

    if (!params_.driver.empty()) {
        ss << "Driver={" << params_.driver << "};";
    }
    if (!params_.server.empty()) {
        ss << "Server=" << params_.server << ";";
    }
    ss << "Port=" << params_.port << ";";
    if (!params_.database.empty()) {
        ss << "Database=" << params_.database << ";";
    }
    if (!params_.user.empty()) {
        ss << "UID=" << params_.user << ";";
    }
    // Don't include password in output string for security

    return ss.str();
}

SQLRETURN OdbcConnection::executeSQL(const std::string& /*sql*/,
                                      std::vector<std::vector<std::string>>& /*results*/,
                                      std::vector<ColumnMetadata>& /*columns*/,
                                      SQLLEN& rows_affected) {
    // TODO: Implement actual SQL execution over wire protocol
    rows_affected = 0;
    return SQL_SUCCESS;
}

SQLRETURN OdbcConnection::prepareSQL(const std::string& /*sql*/, uint64_t& stmt_id,
                                      std::vector<ColumnMetadata>& /*param_metadata*/) {
    // TODO: Implement actual prepared statement
    static std::atomic<uint64_t> next_stmt_id{1};
    stmt_id = next_stmt_id++;
    return SQL_SUCCESS;
}

SQLRETURN OdbcConnection::executePrepared(uint64_t /*stmt_id*/,
                                           const std::vector<std::vector<uint8_t>>& /*params*/,
                                           std::vector<std::vector<std::string>>& /*results*/,
                                           std::vector<ColumnMetadata>& /*columns*/,
                                           SQLLEN& rows_affected) {
    // TODO: Implement actual prepared statement execution
    rows_affected = 0;
    return SQL_SUCCESS;
}

// =============================================================================
// OdbcStatement Implementation
// =============================================================================

OdbcStatement::OdbcStatement(OdbcConnection* conn)
    : conn_(conn) {}

OdbcStatement::~OdbcStatement() = default;

SQLRETURN OdbcStatement::prepare(const SQLCHAR* sql, SQLINTEGER sql_len) {
    clearDiagnostics();

    if (!sql) {
        setError("HY009", 0, "Invalid use of null pointer");
        return SQL_ERROR;
    }

    sql_ = (sql_len == SQL_NTS) ?
        std::string(reinterpret_cast<const char*>(sql)) :
        std::string(reinterpret_cast<const char*>(sql), sql_len);

    std::vector<ColumnMetadata> param_metadata;
    auto result = conn_->prepareSQL(sql_, server_stmt_id_, param_metadata);
    if (result == SQL_SUCCESS) {
        prepared_ = true;
        executed_ = false;
    }

    return result;
}

SQLRETURN OdbcStatement::execute() {
    clearDiagnostics();

    if (!prepared_) {
        setError("HY010", 0, "Function sequence error");
        return SQL_ERROR;
    }

    // Build parameter data
    auto params = buildParameterData();

    // Execute
    auto result = conn_->executePrepared(server_stmt_id_, params, rows_, columns_, row_count_);
    if (result == SQL_SUCCESS) {
        executed_ = true;
        has_results_ = !columns_.empty();
        current_row_ = 0;
    }

    return result;
}

SQLRETURN OdbcStatement::execDirect(const SQLCHAR* sql, SQLINTEGER sql_len) {
    clearDiagnostics();

    if (!sql) {
        setError("HY009", 0, "Invalid use of null pointer");
        return SQL_ERROR;
    }

    sql_ = (sql_len == SQL_NTS) ?
        std::string(reinterpret_cast<const char*>(sql)) :
        std::string(reinterpret_cast<const char*>(sql), sql_len);

    auto result = conn_->executeSQL(sql_, rows_, columns_, row_count_);
    if (result == SQL_SUCCESS) {
        executed_ = true;
        has_results_ = !columns_.empty();
        current_row_ = 0;
        prepared_ = false;
    }

    return result;
}

SQLRETURN OdbcStatement::cancel() {
    clearDiagnostics();
    // TODO: Implement actual cancel
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::closeCursor() {
    clearDiagnostics();

    if (!has_results_) {
        setError("24000", 0, "Invalid cursor state");
        return SQL_ERROR;
    }

    has_results_ = false;
    rows_.clear();
    current_row_ = 0;

    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::freeStmt(SQLUSMALLINT option) {
    clearDiagnostics();

    switch (option) {
        case SQL_CLOSE:
            if (has_results_) {
                closeCursor();
            }
            break;

        case SQL_DROP:
            // Will be handled by destructor
            break;

        case SQL_UNBIND:
            col_bindings_.clear();
            break;

        case SQL_RESET_PARAMS:
            param_bindings_.clear();
            break;

        default:
            setError("HY092", 0, "Invalid attribute identifier");
            return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::bindParameter(SQLUSMALLINT parameter_number,
                                        SQLSMALLINT input_output_type,
                                        SQLSMALLINT value_type,
                                        SQLSMALLINT parameter_type,
                                        SQLULEN column_size,
                                        SQLSMALLINT decimal_digits,
                                        SQLPOINTER parameter_value,
                                        SQLLEN buffer_length,
                                        SQLLEN* str_len_or_ind) {
    clearDiagnostics();

    if (parameter_number == 0) {
        setError("HY000", 0, "Invalid parameter number");
        return SQL_ERROR;
    }

    ParameterBinding binding;
    binding.input_output_type = input_output_type;
    binding.value_type = value_type;
    binding.parameter_type = parameter_type;
    binding.column_size = column_size;
    binding.decimal_digits = decimal_digits;
    binding.parameter_value = parameter_value;
    binding.buffer_length = buffer_length;
    binding.str_len_or_ind = str_len_or_ind;

    param_bindings_[parameter_number] = binding;

    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::numParams(SQLSMALLINT* param_count) {
    clearDiagnostics();

    if (!param_count) {
        setError("HY009", 0, "Invalid use of null pointer");
        return SQL_ERROR;
    }

    *param_count = static_cast<SQLSMALLINT>(param_bindings_.size());
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::describeParam(SQLUSMALLINT parameter_number,
                                        SQLSMALLINT* data_type,
                                        SQLULEN* parameter_size,
                                        SQLSMALLINT* decimal_digits,
                                        SQLSMALLINT* nullable) {
    clearDiagnostics();

    auto it = param_bindings_.find(parameter_number);
    if (it == param_bindings_.end()) {
        setError("07009", 0, "Invalid descriptor index");
        return SQL_ERROR;
    }

    if (data_type) *data_type = it->second.parameter_type;
    if (parameter_size) *parameter_size = it->second.column_size;
    if (decimal_digits) *decimal_digits = it->second.decimal_digits;
    if (nullable) *nullable = SQL_NULLABLE_UNKNOWN;

    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::bindCol(SQLUSMALLINT column_number,
                                  SQLSMALLINT target_type,
                                  SQLPOINTER target_value,
                                  SQLLEN buffer_length,
                                  SQLLEN* str_len_or_ind) {
    clearDiagnostics();

    if (column_number == 0) {
        // Bookmark column - not supported
        setError("HYC00", 0, "Optional feature not implemented");
        return SQL_ERROR;
    }

    ColumnBinding binding;
    binding.target_type = target_type;
    binding.target_value = target_value;
    binding.buffer_length = buffer_length;
    binding.str_len_or_ind = str_len_or_ind;

    col_bindings_[column_number] = binding;

    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::numResultCols(SQLSMALLINT* column_count) {
    clearDiagnostics();

    if (!column_count) {
        setError("HY009", 0, "Invalid use of null pointer");
        return SQL_ERROR;
    }

    *column_count = static_cast<SQLSMALLINT>(columns_.size());
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::describeCol(SQLUSMALLINT column_number,
                                      SQLCHAR* column_name,
                                      SQLSMALLINT buffer_length,
                                      SQLSMALLINT* name_length,
                                      SQLSMALLINT* data_type,
                                      SQLULEN* column_size,
                                      SQLSMALLINT* decimal_digits,
                                      SQLSMALLINT* nullable) {
    clearDiagnostics();

    if (column_number == 0 || column_number > columns_.size()) {
        setError("07009", 0, "Invalid descriptor index");
        return SQL_ERROR;
    }

    const auto& col = columns_[column_number - 1];
    SQLRETURN result = SQL_SUCCESS;

    // Copy name
    if (name_length) {
        *name_length = static_cast<SQLSMALLINT>(col.name.size());
    }
    if (column_name && buffer_length > 0) {
        size_t copy_len = std::min(static_cast<size_t>(buffer_length - 1), col.name.size());
        std::memcpy(column_name, col.name.c_str(), copy_len);
        column_name[copy_len] = '\0';
        if (col.name.size() >= static_cast<size_t>(buffer_length)) {
            setError("01004", 0, "String data, right truncated");
            result = SQL_SUCCESS_WITH_INFO;
        }
    }

    if (data_type) *data_type = col.sql_type;
    if (column_size) *column_size = col.column_size;
    if (decimal_digits) *decimal_digits = col.decimal_digits;
    if (nullable) *nullable = col.nullable;

    return result;
}

SQLRETURN OdbcStatement::colAttribute(SQLUSMALLINT column_number,
                                       SQLUSMALLINT field_identifier,
                                       SQLPOINTER char_attr,
                                       SQLSMALLINT buffer_length,
                                       SQLSMALLINT* string_length,
                                       SQLLEN* numeric_attr) {
    clearDiagnostics();

    if (column_number == 0 || column_number > columns_.size()) {
        if (field_identifier == SQL_DESC_COUNT) {
            if (numeric_attr) *numeric_attr = static_cast<SQLLEN>(columns_.size());
            return SQL_SUCCESS;
        }
        setError("07009", 0, "Invalid descriptor index");
        return SQL_ERROR;
    }

    const auto& col = columns_[column_number - 1];

    auto copyString = [&](const std::string& str) -> SQLRETURN {
        if (string_length) {
            *string_length = static_cast<SQLSMALLINT>(str.size());
        }
        if (char_attr && buffer_length > 0) {
            size_t copy_len = std::min(static_cast<size_t>(buffer_length - 1), str.size());
            std::memcpy(char_attr, str.c_str(), copy_len);
            static_cast<char*>(char_attr)[copy_len] = '\0';
            if (str.size() >= static_cast<size_t>(buffer_length)) {
                setError("01004", 0, "String data, right truncated");
                return SQL_SUCCESS_WITH_INFO;
            }
        }
        return SQL_SUCCESS;
    };

    switch (field_identifier) {
        // Note: SQL_DESC_NAME = SQL_COLUMN_NAME (same value), only one case needed
        case SQL_DESC_NAME:  // Also SQL_COLUMN_NAME
            return copyString(col.name);

        case SQL_DESC_LABEL:  // Also SQL_COLUMN_LABEL
            return copyString(col.label.empty() ? col.name : col.label);

        case SQL_DESC_TYPE_NAME:  // Also SQL_COLUMN_TYPE_NAME
            return copyString(col.type_name);

        case SQL_DESC_TABLE_NAME:  // Also SQL_COLUMN_TABLE_NAME
            return copyString(col.table_name);

        case SQL_DESC_SCHEMA_NAME:  // Also SQL_COLUMN_OWNER_NAME
            return copyString(col.schema_name);

        case SQL_DESC_CATALOG_NAME:  // Also SQL_COLUMN_QUALIFIER_NAME
            return copyString(col.catalog_name);

        case SQL_DESC_TYPE:  // Also SQL_DESC_CONCISE_TYPE, SQL_COLUMN_TYPE
            if (numeric_attr) *numeric_attr = col.sql_type;
            break;

        case SQL_DESC_LENGTH:  // Also SQL_COLUMN_LENGTH
            if (numeric_attr) *numeric_attr = static_cast<SQLLEN>(col.column_size);
            break;

        case SQL_DESC_PRECISION:  // Also SQL_COLUMN_PRECISION
            if (numeric_attr) *numeric_attr = static_cast<SQLLEN>(col.column_size);
            break;

        case SQL_DESC_SCALE:  // Also SQL_COLUMN_SCALE
            if (numeric_attr) *numeric_attr = col.decimal_digits;
            break;

        case SQL_DESC_NULLABLE:  // Also SQL_COLUMN_NULLABLE
            if (numeric_attr) *numeric_attr = col.nullable;
            break;

        case SQL_DESC_UNSIGNED:  // Also SQL_COLUMN_UNSIGNED
            if (numeric_attr) *numeric_attr = col.unsigned_flag ? 1 : 0;
            break;

        case SQL_DESC_AUTO_UNIQUE_VALUE:  // Also SQL_COLUMN_AUTO_INCREMENT
            if (numeric_attr) *numeric_attr = col.auto_increment ? 1 : 0;
            break;

        case SQL_DESC_CASE_SENSITIVE:  // Also SQL_COLUMN_CASE_SENSITIVE
            if (numeric_attr) *numeric_attr = col.case_sensitive ? 1 : 0;
            break;

        case SQL_DESC_SEARCHABLE:  // Also SQL_COLUMN_SEARCHABLE
            if (numeric_attr) *numeric_attr = col.searchable;
            break;

        case SQL_DESC_DISPLAY_SIZE:  // Also SQL_COLUMN_DISPLAY_SIZE
            if (numeric_attr) *numeric_attr = col.display_size;
            break;

        case SQL_DESC_OCTET_LENGTH:
            if (numeric_attr) *numeric_attr = col.octet_length;
            break;

        case SQL_DESC_COUNT:
            if (numeric_attr) *numeric_attr = static_cast<SQLLEN>(columns_.size());
            break;

        case SQL_COLUMN_UPDATABLE:  // Also SQL_DESC_UPDATABLE
            if (numeric_attr) *numeric_attr = 0;  // SQL_ATTR_READONLY
            break;

        case SQL_COLUMN_MONEY:
            if (numeric_attr) *numeric_attr = 0;
            break;

        default:
            setError("HY091", 0, "Invalid descriptor field identifier");
            return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::fetch() {
    clearDiagnostics();

    if (!has_results_) {
        setError("24000", 0, "Invalid cursor state");
        return SQL_ERROR;
    }

    if (current_row_ >= rows_.size()) {
        return SQL_NO_DATA;
    }

    // Bind data to bound columns
    auto result = bindResultData();
    current_row_++;

    return result;
}

SQLRETURN OdbcStatement::fetchScroll(SQLSMALLINT fetch_orientation, SQLLEN fetch_offset) {
    clearDiagnostics();

    if (!has_results_) {
        setError("24000", 0, "Invalid cursor state");
        return SQL_ERROR;
    }

    size_t new_row = current_row_;

    switch (fetch_orientation) {
        case SQL_FETCH_NEXT:
            new_row = current_row_;
            break;
        case SQL_FETCH_FIRST:
            new_row = 0;
            break;
        case SQL_FETCH_LAST:
            new_row = rows_.empty() ? 0 : rows_.size() - 1;
            break;
        case SQL_FETCH_PRIOR:
            new_row = current_row_ > 0 ? current_row_ - 1 : 0;
            break;
        case SQL_FETCH_ABSOLUTE:
            if (fetch_offset > 0) {
                new_row = static_cast<size_t>(fetch_offset - 1);
            } else if (fetch_offset < 0) {
                if (static_cast<size_t>(-fetch_offset) <= rows_.size()) {
                    new_row = rows_.size() + static_cast<size_t>(fetch_offset);
                } else {
                    return SQL_NO_DATA;
                }
            } else {
                return SQL_NO_DATA;
            }
            break;
        case SQL_FETCH_RELATIVE:
            new_row = static_cast<size_t>(static_cast<SQLLEN>(current_row_) + fetch_offset);
            break;
        default:
            setError("HY106", 0, "Fetch type out of range");
            return SQL_ERROR;
    }

    if (new_row >= rows_.size()) {
        return SQL_NO_DATA;
    }

    current_row_ = new_row;
    auto result = bindResultData();
    current_row_++;

    return result;
}

SQLRETURN OdbcStatement::getData(SQLUSMALLINT column_number,
                                  SQLSMALLINT target_type,
                                  SQLPOINTER target_value,
                                  SQLLEN buffer_length,
                                  SQLLEN* str_len_or_ind) {
    clearDiagnostics();

    if (!has_results_) {
        setError("24000", 0, "Invalid cursor state");
        return SQL_ERROR;
    }

    if (current_row_ == 0 || current_row_ > rows_.size()) {
        setError("HY109", 0, "Invalid cursor position");
        return SQL_ERROR;
    }

    if (column_number == 0 || column_number > columns_.size()) {
        setError("07009", 0, "Invalid descriptor index");
        return SQL_ERROR;
    }

    const auto& value = rows_[current_row_ - 1][column_number - 1];

    // Handle NULL
    if (value.empty()) {
        if (str_len_or_ind) {
            *str_len_or_ind = SQL_NULL_DATA;
        }
        return SQL_SUCCESS;
    }

    // Convert and store based on target type
    SQLRETURN result = SQL_SUCCESS;

    switch (target_type) {
        case SQL_C_CHAR:
        case SQL_C_DEFAULT: {
            if (str_len_or_ind) {
                *str_len_or_ind = static_cast<SQLLEN>(value.size());
            }
            if (target_value && buffer_length > 0) {
                size_t copy_len = std::min(static_cast<size_t>(buffer_length - 1), value.size());
                std::memcpy(target_value, value.c_str(), copy_len);
                static_cast<char*>(target_value)[copy_len] = '\0';
                if (value.size() >= static_cast<size_t>(buffer_length)) {
                    setError("01004", 0, "String data, right truncated");
                    result = SQL_SUCCESS_WITH_INFO;
                }
            }
            break;
        }

        case SQL_C_LONG:
        case SQL_C_SLONG: {
            if (target_value) {
                *static_cast<SQLINTEGER*>(target_value) = std::stoi(value);
            }
            if (str_len_or_ind) {
                *str_len_or_ind = sizeof(SQLINTEGER);
            }
            break;
        }

        case SQL_C_SHORT:
        case SQL_C_SSHORT: {
            if (target_value) {
                *static_cast<SQLSMALLINT*>(target_value) = static_cast<SQLSMALLINT>(std::stoi(value));
            }
            if (str_len_or_ind) {
                *str_len_or_ind = sizeof(SQLSMALLINT);
            }
            break;
        }

        case SQL_C_SBIGINT: {
            if (target_value) {
                *static_cast<int64_t*>(target_value) = std::stoll(value);
            }
            if (str_len_or_ind) {
                *str_len_or_ind = sizeof(int64_t);
            }
            break;
        }

        case SQL_C_DOUBLE: {
            if (target_value) {
                *static_cast<SQLDOUBLE*>(target_value) = std::stod(value);
            }
            if (str_len_or_ind) {
                *str_len_or_ind = sizeof(SQLDOUBLE);
            }
            break;
        }

        case SQL_C_FLOAT: {
            if (target_value) {
                *static_cast<SQLREAL*>(target_value) = std::stof(value);
            }
            if (str_len_or_ind) {
                *str_len_or_ind = sizeof(SQLREAL);
            }
            break;
        }

        case SQL_C_BIT: {
            if (target_value) {
                *static_cast<unsigned char*>(target_value) =
                    (value == "1" || value == "true" || value == "t") ? 1 : 0;
            }
            if (str_len_or_ind) {
                *str_len_or_ind = 1;
            }
            break;
        }

        case SQL_C_BINARY: {
            if (str_len_or_ind) {
                *str_len_or_ind = static_cast<SQLLEN>(value.size());
            }
            if (target_value && buffer_length > 0) {
                size_t copy_len = std::min(static_cast<size_t>(buffer_length), value.size());
                std::memcpy(target_value, value.data(), copy_len);
                if (value.size() > static_cast<size_t>(buffer_length)) {
                    setError("01004", 0, "String data, right truncated");
                    result = SQL_SUCCESS_WITH_INFO;
                }
            }
            break;
        }

        default:
            setError("HY003", 0, "Program type out of range");
            return SQL_ERROR;
    }

    return result;
}

SQLRETURN OdbcStatement::rowCount(SQLLEN* row_count_ptr) {
    clearDiagnostics();

    if (!row_count_ptr) {
        setError("HY009", 0, "Invalid use of null pointer");
        return SQL_ERROR;
    }

    *row_count_ptr = row_count_;
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::moreResults() {
    clearDiagnostics();
    // TODO: Implement multiple result sets
    return SQL_NO_DATA;
}

SQLRETURN OdbcStatement::setPos(SQLSETPOSIROW /*row_number*/, SQLUSMALLINT /*operation*/,
                                 SQLUSMALLINT /*lock_type*/) {
    clearDiagnostics();
    setError("HYC00", 0, "Optional feature not implemented");
    return SQL_ERROR;
}

SQLRETURN OdbcStatement::bulkOperations(SQLSMALLINT /*operation*/) {
    clearDiagnostics();
    setError("HYC00", 0, "Optional feature not implemented");
    return SQL_ERROR;
}

SQLRETURN OdbcStatement::setAttribute(SQLINTEGER attribute, SQLPOINTER value,
                                       SQLINTEGER /*string_length*/) {
    clearDiagnostics();

    switch (attribute) {
        case SQL_ATTR_CURSOR_TYPE:
            cursor_type_ = ODBC_PTR_TO_ULEN(value);
            break;
        case SQL_ATTR_CONCURRENCY:
            concurrency_ = ODBC_PTR_TO_ULEN(value);
            break;
        case SQL_ATTR_QUERY_TIMEOUT:
            query_timeout_ = ODBC_PTR_TO_ULEN(value);
            break;
        case SQL_ATTR_MAX_ROWS:
            max_rows_ = ODBC_PTR_TO_ULEN(value);
            break;
        case SQL_ATTR_MAX_LENGTH:
            max_length_ = ODBC_PTR_TO_ULEN(value);
            break;
        case SQL_ATTR_ROW_ARRAY_SIZE:
            row_array_size_ = ODBC_PTR_TO_ULEN(value);
            break;
        case SQL_ATTR_ROWS_FETCHED_PTR:
            rows_fetched_ptr_ = static_cast<SQLULEN*>(value);
            break;
        case SQL_ATTR_ROW_STATUS_PTR:
            row_status_ptr_ = static_cast<SQLUSMALLINT*>(value);
            break;
        case SQL_ATTR_ROW_BIND_OFFSET_PTR:
            // value is pointer to SQLLEN
            if (value) row_bind_offset_ = *static_cast<SQLLEN*>(value);
            break;
        case SQL_ATTR_ROW_BIND_TYPE:
            row_bind_type_ = ODBC_PTR_TO_ULEN(value);
            break;
        case SQL_ATTR_PARAMSET_SIZE:
            paramset_size_ = ODBC_PTR_TO_ULEN(value);
            break;
        case SQL_ATTR_PARAMS_PROCESSED_PTR:
            params_processed_ptr_ = static_cast<SQLULEN*>(value);
            break;
        case SQL_ATTR_PARAM_STATUS_PTR:
            param_status_ptr_ = static_cast<SQLUSMALLINT*>(value);
            break;
        case SQL_ATTR_PARAM_BIND_OFFSET_PTR:
            if (value) param_bind_offset_ = *static_cast<SQLLEN*>(value);
            break;
        case SQL_ATTR_PARAM_BIND_TYPE:
            param_bind_type_ = ODBC_PTR_TO_ULEN(value);
            break;
        case SQL_ATTR_NOSCAN:
            noscan_ = (ODBC_PTR_TO_ULEN(value) != 0);
            break;
        case SQL_ATTR_USE_BOOKMARKS:
            use_bookmarks_ = (ODBC_PTR_TO_ULEN(value) != 0);
            break;
        case SQL_ATTR_RETRIEVE_DATA:
            retrieve_data_ = (ODBC_PTR_TO_ULEN(value) != 0);
            break;
        case SQL_ATTR_CURSOR_SCROLLABLE:
            cursor_scrollable_ = ODBC_PTR_TO_ULEN(value);
            break;
        case SQL_ATTR_CURSOR_SENSITIVITY:
            cursor_sensitivity_ = ODBC_PTR_TO_ULEN(value);
            break;
        default:
            setError("HY092", 0, "Invalid attribute identifier");
            return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::getAttribute(SQLINTEGER attribute, SQLPOINTER value,
                                       SQLINTEGER /*buffer_length*/,
                                       SQLINTEGER* string_length) {
    clearDiagnostics();

    auto setLen = [&](size_t len) {
        if (string_length) *string_length = static_cast<SQLINTEGER>(len);
    };

    switch (attribute) {
        case SQL_ATTR_CURSOR_TYPE:
            if (value) *static_cast<SQLULEN*>(value) = cursor_type_;
            setLen(sizeof(SQLULEN));
            break;
        case SQL_ATTR_CONCURRENCY:
            if (value) *static_cast<SQLULEN*>(value) = concurrency_;
            setLen(sizeof(SQLULEN));
            break;
        case SQL_ATTR_QUERY_TIMEOUT:
            if (value) *static_cast<SQLULEN*>(value) = query_timeout_;
            setLen(sizeof(SQLULEN));
            break;
        case SQL_ATTR_MAX_ROWS:
            if (value) *static_cast<SQLULEN*>(value) = max_rows_;
            setLen(sizeof(SQLULEN));
            break;
        case SQL_ATTR_MAX_LENGTH:
            if (value) *static_cast<SQLULEN*>(value) = max_length_;
            setLen(sizeof(SQLULEN));
            break;
        case SQL_ATTR_ROW_ARRAY_SIZE:
            if (value) *static_cast<SQLULEN*>(value) = row_array_size_;
            setLen(sizeof(SQLULEN));
            break;
        case SQL_ATTR_ROW_NUMBER:
            if (value) *static_cast<SQLULEN*>(value) = static_cast<SQLULEN>(current_row_);
            setLen(sizeof(SQLULEN));
            break;
        case SQL_ATTR_PARAMSET_SIZE:
            if (value) *static_cast<SQLULEN*>(value) = paramset_size_;
            setLen(sizeof(SQLULEN));
            break;
        case SQL_ATTR_IMP_ROW_DESC:
        case SQL_ATTR_IMP_PARAM_DESC:
        case SQL_ATTR_APP_ROW_DESC:
        case SQL_ATTR_APP_PARAM_DESC:
            // TODO: Return actual descriptor handles
            if (value) *static_cast<SQLPOINTER*>(value) = nullptr;
            setLen(sizeof(SQLPOINTER));
            break;
        default:
            setError("HY092", 0, "Invalid attribute identifier");
            return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::bindResultData() {
    if (current_row_ >= rows_.size()) {
        return SQL_NO_DATA;
    }

    const auto& row = rows_[current_row_];
    SQLRETURN result = SQL_SUCCESS;

    for (const auto& [col_num, binding] : col_bindings_) {
        if (col_num > row.size()) continue;

        const auto& value = row[col_num - 1];
        SQLLEN* str_len_or_ind = binding.str_len_or_ind;
        SQLPOINTER target = binding.target_value;
        SQLLEN buffer_len = binding.buffer_length;

        // Apply row offset if using row-wise binding
        if (row_bind_offset_ != 0 && target) {
            target = static_cast<char*>(target) + row_bind_offset_;
            if (str_len_or_ind) {
                str_len_or_ind = reinterpret_cast<SQLLEN*>(
                    reinterpret_cast<char*>(str_len_or_ind) + row_bind_offset_);
            }
        }

        // Handle NULL
        if (value.empty()) {
            if (str_len_or_ind) *str_len_or_ind = SQL_NULL_DATA;
            continue;
        }

        // Convert and store
        auto conv_result = getData(col_num, binding.target_type, target, buffer_len, str_len_or_ind);
        if (conv_result == SQL_SUCCESS_WITH_INFO) {
            result = SQL_SUCCESS_WITH_INFO;
        } else if (conv_result == SQL_ERROR) {
            return SQL_ERROR;
        }
    }

    // Set row status
    if (row_status_ptr_) {
        row_status_ptr_[0] = SQL_ROW_SUCCESS;
    }
    if (rows_fetched_ptr_) {
        *rows_fetched_ptr_ = 1;
    }

    return result;
}

SQLRETURN OdbcStatement::convertAndStore(size_t /*col_index*/, const std::string& /*value*/) {
    // Helper for data conversion - implemented in getData
    return SQL_SUCCESS;
}

std::vector<std::vector<uint8_t>> OdbcStatement::buildParameterData() {
    std::vector<std::vector<uint8_t>> result;

    for (SQLUSMALLINT i = 1; i <= param_bindings_.size(); ++i) {
        auto it = param_bindings_.find(i);
        if (it == param_bindings_.end()) {
            result.push_back({});
            continue;
        }

        const auto& binding = it->second;

        // Check for NULL
        if (binding.str_len_or_ind && *binding.str_len_or_ind == SQL_NULL_DATA) {
            result.push_back({});
            continue;
        }

        // Convert parameter to bytes based on value type
        std::vector<uint8_t> param_data;

        switch (binding.value_type) {
            case SQL_C_CHAR: {
                const char* str = static_cast<const char*>(binding.parameter_value);
                SQLLEN len = (binding.str_len_or_ind && *binding.str_len_or_ind != SQL_NTS) ?
                    *binding.str_len_or_ind : static_cast<SQLLEN>(std::strlen(str));
                param_data.assign(str, str + len);
                break;
            }
            case SQL_C_LONG:
            case SQL_C_SLONG: {
                SQLINTEGER val = *static_cast<const SQLINTEGER*>(binding.parameter_value);
                std::string str = std::to_string(val);
                param_data.assign(str.begin(), str.end());
                break;
            }
            case SQL_C_DOUBLE: {
                SQLDOUBLE val = *static_cast<const SQLDOUBLE*>(binding.parameter_value);
                std::string str = std::to_string(val);
                param_data.assign(str.begin(), str.end());
                break;
            }
            // Add more type conversions as needed
            default:
                // Default: treat as binary
                if (binding.parameter_value && binding.buffer_length > 0) {
                    const uint8_t* data = static_cast<const uint8_t*>(binding.parameter_value);
                    param_data.assign(data, data + binding.buffer_length);
                }
                break;
        }

        result.push_back(std::move(param_data));
    }

    return result;
}

// Catalog function stubs - these would query system tables
SQLRETURN OdbcStatement::tables(const SQLCHAR* /*catalog*/, SQLSMALLINT /*catalog_len*/,
                                 const SQLCHAR* /*schema*/, SQLSMALLINT /*schema_len*/,
                                 const SQLCHAR* /*table*/, SQLSMALLINT /*table_len*/,
                                 const SQLCHAR* /*table_type*/, SQLSMALLINT /*table_type_len*/) {
    clearDiagnostics();
    // TODO: Execute catalog query
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::columns(const SQLCHAR* /*catalog*/, SQLSMALLINT /*catalog_len*/,
                                  const SQLCHAR* /*schema*/, SQLSMALLINT /*schema_len*/,
                                  const SQLCHAR* /*table*/, SQLSMALLINT /*table_len*/,
                                  const SQLCHAR* /*column*/, SQLSMALLINT /*column_len*/) {
    clearDiagnostics();
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::primaryKeys(const SQLCHAR* /*catalog*/, SQLSMALLINT /*catalog_len*/,
                                      const SQLCHAR* /*schema*/, SQLSMALLINT /*schema_len*/,
                                      const SQLCHAR* /*table*/, SQLSMALLINT /*table_len*/) {
    clearDiagnostics();
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::foreignKeys(const SQLCHAR* /*pk_catalog*/, SQLSMALLINT /*pk_catalog_len*/,
                                      const SQLCHAR* /*pk_schema*/, SQLSMALLINT /*pk_schema_len*/,
                                      const SQLCHAR* /*pk_table*/, SQLSMALLINT /*pk_table_len*/,
                                      const SQLCHAR* /*fk_catalog*/, SQLSMALLINT /*fk_catalog_len*/,
                                      const SQLCHAR* /*fk_schema*/, SQLSMALLINT /*fk_schema_len*/,
                                      const SQLCHAR* /*fk_table*/, SQLSMALLINT /*fk_table_len*/) {
    clearDiagnostics();
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::statistics(const SQLCHAR* /*catalog*/, SQLSMALLINT /*catalog_len*/,
                                     const SQLCHAR* /*schema*/, SQLSMALLINT /*schema_len*/,
                                     const SQLCHAR* /*table*/, SQLSMALLINT /*table_len*/,
                                     SQLUSMALLINT /*unique*/, SQLUSMALLINT /*reserved*/) {
    clearDiagnostics();
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::specialColumns(SQLUSMALLINT /*identifier_type*/,
                                         const SQLCHAR* /*catalog*/, SQLSMALLINT /*catalog_len*/,
                                         const SQLCHAR* /*schema*/, SQLSMALLINT /*schema_len*/,
                                         const SQLCHAR* /*table*/, SQLSMALLINT /*table_len*/,
                                         SQLUSMALLINT /*scope*/, SQLUSMALLINT /*nullable*/) {
    clearDiagnostics();
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::procedures(const SQLCHAR* /*catalog*/, SQLSMALLINT /*catalog_len*/,
                                     const SQLCHAR* /*schema*/, SQLSMALLINT /*schema_len*/,
                                     const SQLCHAR* /*proc*/, SQLSMALLINT /*proc_len*/) {
    clearDiagnostics();
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::procedureColumns(const SQLCHAR* /*catalog*/, SQLSMALLINT /*catalog_len*/,
                                           const SQLCHAR* /*schema*/, SQLSMALLINT /*schema_len*/,
                                           const SQLCHAR* /*proc*/, SQLSMALLINT /*proc_len*/,
                                           const SQLCHAR* /*column*/, SQLSMALLINT /*column_len*/) {
    clearDiagnostics();
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::tablePrivileges(const SQLCHAR* /*catalog*/, SQLSMALLINT /*catalog_len*/,
                                          const SQLCHAR* /*schema*/, SQLSMALLINT /*schema_len*/,
                                          const SQLCHAR* /*table*/, SQLSMALLINT /*table_len*/) {
    clearDiagnostics();
    return SQL_SUCCESS;
}

SQLRETURN OdbcStatement::columnPrivileges(const SQLCHAR* /*catalog*/, SQLSMALLINT /*catalog_len*/,
                                           const SQLCHAR* /*schema*/, SQLSMALLINT /*schema_len*/,
                                           const SQLCHAR* /*table*/, SQLSMALLINT /*table_len*/,
                                           const SQLCHAR* /*column*/, SQLSMALLINT /*column_len*/) {
    clearDiagnostics();
    return SQL_SUCCESS;
}

// =============================================================================
// OdbcDescriptor Implementation
// =============================================================================

OdbcDescriptor::OdbcDescriptor(OdbcConnection* conn, DescriptorType type, bool implicit)
    : conn_(conn), desc_type_(type), implicit_(implicit) {
    alloc_type_ = implicit ? 0 : 1;  // SQL_DESC_ALLOC_AUTO or SQL_DESC_ALLOC_USER
}

OdbcDescriptor::~OdbcDescriptor() = default;

SQLRETURN OdbcDescriptor::setField(SQLSMALLINT rec_number, SQLSMALLINT field_identifier,
                                    SQLPOINTER value, SQLINTEGER buffer_length) {
    clearDiagnostics();

    // Ensure record exists
    if (rec_number >= 0) {
        while (records_.size() <= static_cast<size_t>(rec_number)) {
            records_.emplace_back();
        }
    }

    // Header fields (rec_number == 0 or negative)
    if (rec_number == 0) {
        switch (field_identifier) {
            case SQL_DESC_COUNT:
                count_ = *static_cast<SQLSMALLINT*>(value);
                break;
            case SQL_DESC_ALLOC_TYPE:
                // Read-only
                break;
            default:
                break;
        }
        return SQL_SUCCESS;
    }

    auto& rec = records_[rec_number];
    (void)buffer_length;

    switch (field_identifier) {
        case SQL_DESC_TYPE:
            rec.type = *static_cast<SQLSMALLINT*>(value);
            break;
        case SQL_DESC_CONCISE_TYPE:
            rec.concise_type = *static_cast<SQLSMALLINT*>(value);
            break;
        case SQL_DESC_LENGTH:
            rec.length = *static_cast<SQLLEN*>(value);
            break;
        case SQL_DESC_PRECISION:
            rec.precision = *static_cast<SQLSMALLINT*>(value);
            break;
        case SQL_DESC_SCALE:
            rec.scale = *static_cast<SQLSMALLINT*>(value);
            break;
        case SQL_DESC_DATA_PTR:
            rec.data_ptr = value;
            break;
        case SQL_DESC_INDICATOR_PTR:
            rec.indicator_ptr = static_cast<SQLLEN*>(value);
            break;
        case SQL_DESC_OCTET_LENGTH_PTR:
            rec.octet_length_ptr = static_cast<SQLLEN*>(value);
            break;
        case SQL_DESC_OCTET_LENGTH:
            rec.octet_length = *static_cast<SQLLEN*>(value);
            break;
        case SQL_DESC_NULLABLE:
            rec.nullable = *static_cast<SQLSMALLINT*>(value);
            break;
        default:
            setError("HY091", 0, "Invalid descriptor field identifier");
            return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcDescriptor::getField(SQLSMALLINT rec_number, SQLSMALLINT field_identifier,
                                    SQLPOINTER value, SQLINTEGER buffer_length,
                                    SQLINTEGER* string_length) {
    clearDiagnostics();

    // Header fields
    if (rec_number == 0) {
        switch (field_identifier) {
            case SQL_DESC_COUNT:
                if (value) *static_cast<SQLSMALLINT*>(value) = count_;
                if (string_length) *string_length = sizeof(SQLSMALLINT);
                break;
            case SQL_DESC_ALLOC_TYPE:
                if (value) *static_cast<SQLSMALLINT*>(value) = alloc_type_;
                if (string_length) *string_length = sizeof(SQLSMALLINT);
                break;
            default:
                setError("HY091", 0, "Invalid descriptor field identifier");
                return SQL_ERROR;
        }
        return SQL_SUCCESS;
    }

    if (static_cast<size_t>(rec_number) > records_.size()) {
        setError("07009", 0, "Invalid descriptor index");
        return SQL_ERROR;
    }

    const auto& rec = records_[rec_number - 1];
    (void)buffer_length;

    switch (field_identifier) {
        case SQL_DESC_TYPE:
            if (value) *static_cast<SQLSMALLINT*>(value) = rec.type;
            if (string_length) *string_length = sizeof(SQLSMALLINT);
            break;
        case SQL_DESC_CONCISE_TYPE:
            if (value) *static_cast<SQLSMALLINT*>(value) = rec.concise_type;
            if (string_length) *string_length = sizeof(SQLSMALLINT);
            break;
        case SQL_DESC_LENGTH:
            if (value) *static_cast<SQLLEN*>(value) = rec.length;
            if (string_length) *string_length = sizeof(SQLLEN);
            break;
        case SQL_DESC_PRECISION:
            if (value) *static_cast<SQLSMALLINT*>(value) = rec.precision;
            if (string_length) *string_length = sizeof(SQLSMALLINT);
            break;
        case SQL_DESC_SCALE:
            if (value) *static_cast<SQLSMALLINT*>(value) = rec.scale;
            if (string_length) *string_length = sizeof(SQLSMALLINT);
            break;
        case SQL_DESC_NULLABLE:
            if (value) *static_cast<SQLSMALLINT*>(value) = rec.nullable;
            if (string_length) *string_length = sizeof(SQLSMALLINT);
            break;
        default:
            setError("HY091", 0, "Invalid descriptor field identifier");
            return SQL_ERROR;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcDescriptor::setRec(SQLSMALLINT rec_number, SQLSMALLINT type, SQLSMALLINT sub_type,
                                  SQLLEN length, SQLSMALLINT precision, SQLSMALLINT scale,
                                  SQLPOINTER data, SQLLEN* string_length, SQLLEN* indicator) {
    clearDiagnostics();

    if (rec_number < 0) {
        setError("07009", 0, "Invalid descriptor index");
        return SQL_ERROR;
    }

    while (records_.size() <= static_cast<size_t>(rec_number)) {
        records_.emplace_back();
    }

    auto& rec = records_[rec_number];
    rec.type = type;
    rec.concise_type = type;
    rec.datetime_interval_code = sub_type;
    rec.length = length;
    rec.precision = precision;
    rec.scale = scale;
    rec.data_ptr = data;
    rec.octet_length_ptr = string_length;
    rec.indicator_ptr = indicator;

    if (rec_number >= count_) {
        count_ = rec_number + 1;
    }

    return SQL_SUCCESS;
}

SQLRETURN OdbcDescriptor::getRec(SQLSMALLINT rec_number, SQLCHAR* name, SQLSMALLINT buffer_length,
                                  SQLSMALLINT* string_length, SQLSMALLINT* type,
                                  SQLSMALLINT* sub_type, SQLLEN* length, SQLSMALLINT* precision,
                                  SQLSMALLINT* scale, SQLSMALLINT* nullable) {
    clearDiagnostics();

    if (rec_number < 1 || static_cast<size_t>(rec_number) > records_.size()) {
        setError("07009", 0, "Invalid descriptor index");
        return SQL_ERROR;
    }

    const auto& rec = records_[rec_number - 1];
    SQLRETURN result = SQL_SUCCESS;

    // Copy name
    if (string_length) {
        *string_length = static_cast<SQLSMALLINT>(rec.name.size());
    }
    if (name && buffer_length > 0) {
        size_t copy_len = std::min(static_cast<size_t>(buffer_length - 1), rec.name.size());
        std::memcpy(name, rec.name.c_str(), copy_len);
        name[copy_len] = '\0';
        if (rec.name.size() >= static_cast<size_t>(buffer_length)) {
            setError("01004", 0, "String data, right truncated");
            result = SQL_SUCCESS_WITH_INFO;
        }
    }

    if (type) *type = rec.type;
    if (sub_type) *sub_type = rec.datetime_interval_code;
    if (length) *length = rec.length;
    if (precision) *precision = rec.precision;
    if (scale) *scale = rec.scale;
    if (nullable) *nullable = rec.nullable;

    return result;
}

SQLRETURN OdbcDescriptor::copyDesc(OdbcDescriptor* target) {
    clearDiagnostics();

    if (!target) {
        setError("HY009", 0, "Invalid use of null pointer");
        return SQL_ERROR;
    }

    target->count_ = count_;
    target->array_size_ = array_size_;
    target->records_ = records_;

    return SQL_SUCCESS;
}

}  // namespace odbc
}  // namespace scratchbird

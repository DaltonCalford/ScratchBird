/**
 * @file odbc_types.h
 * @brief ODBC Driver Types and Definitions
 *
 * Core types and structures for the ScratchBird ODBC driver.
 * Provides ODBC 3.8 compatibility with 3.52 backwards compatibility.
 *
 * Part of Phase 3.8: ODBC Driver
 */

#ifndef SCRATCHBIRD_ODBC_TYPES_H
#define SCRATCHBIRD_ODBC_TYPES_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>

// Platform-specific definitions
#ifdef _WIN32
    #include <windows.h>
    #define ODBC_API __stdcall
#else
    #define ODBC_API
    typedef void* HWND;
#endif

namespace scratchbird {
namespace odbc {

// =============================================================================
// ODBC Standard Type Definitions
// =============================================================================

// SQL Types (from sql.h)
using SQLCHAR = unsigned char;
using SQLSCHAR = signed char;
using SQLWCHAR = wchar_t;
using SQLTCHAR = SQLCHAR;  // For ANSI builds
using SQLSMALLINT = int16_t;
using SQLUSMALLINT = uint16_t;
using SQLINTEGER = int32_t;
using SQLUINTEGER = uint32_t;
using SQLLEN = int64_t;
using SQLULEN = uint64_t;
using SQLSETPOSIROW = SQLULEN;
using SQLPOINTER = void*;
using SQLHANDLE = void*;
using SQLHENV = SQLHANDLE;
using SQLHDBC = SQLHANDLE;
using SQLHSTMT = SQLHANDLE;
using SQLHDESC = SQLHANDLE;
using SQLRETURN = SQLSMALLINT;
using SQLREAL = float;
using SQLDOUBLE = double;
using SQLFLOAT = double;
using SQLDATE = unsigned char;
using SQLTIME = unsigned char;
using SQLTIMESTAMP = unsigned char;
using SQLNUMERIC = unsigned char;
using SQLGUID = struct { uint32_t Data1; uint16_t Data2; uint16_t Data3; uint8_t Data4[8]; };
using BOOKMARK = SQLULEN;

// =============================================================================
// ODBC Return Codes
// =============================================================================

constexpr SQLRETURN SQL_SUCCESS = 0;
constexpr SQLRETURN SQL_SUCCESS_WITH_INFO = 1;
constexpr SQLRETURN SQL_NO_DATA = 100;
constexpr SQLRETURN SQL_ERROR = -1;
constexpr SQLRETURN SQL_INVALID_HANDLE = -2;
constexpr SQLRETURN SQL_STILL_EXECUTING = 2;
constexpr SQLRETURN SQL_NEED_DATA = 99;
constexpr SQLRETURN SQL_NO_DATA_FOUND = SQL_NO_DATA;

// =============================================================================
// ODBC Handle Types
// =============================================================================

constexpr SQLSMALLINT SQL_HANDLE_ENV = 1;
constexpr SQLSMALLINT SQL_HANDLE_DBC = 2;
constexpr SQLSMALLINT SQL_HANDLE_STMT = 3;
constexpr SQLSMALLINT SQL_HANDLE_DESC = 4;

// Null handle value
constexpr SQLHANDLE SQL_NULL_HANDLE = nullptr;
constexpr SQLHENV SQL_NULL_HENV = nullptr;
constexpr SQLHDBC SQL_NULL_HDBC = nullptr;
constexpr SQLHSTMT SQL_NULL_HSTMT = nullptr;
constexpr SQLHDESC SQL_NULL_HDESC = nullptr;

// =============================================================================
// SQL Data Types
// =============================================================================

constexpr SQLSMALLINT SQL_UNKNOWN_TYPE = 0;
constexpr SQLSMALLINT SQL_CHAR = 1;
constexpr SQLSMALLINT SQL_NUMERIC = 2;
constexpr SQLSMALLINT SQL_DECIMAL = 3;
constexpr SQLSMALLINT SQL_INTEGER = 4;
constexpr SQLSMALLINT SQL_SMALLINT = 5;
constexpr SQLSMALLINT SQL_FLOAT = 6;
constexpr SQLSMALLINT SQL_REAL = 7;
constexpr SQLSMALLINT SQL_DOUBLE = 8;
constexpr SQLSMALLINT SQL_DATETIME = 9;
constexpr SQLSMALLINT SQL_VARCHAR = 12;
constexpr SQLSMALLINT SQL_TYPE_DATE = 91;
constexpr SQLSMALLINT SQL_TYPE_TIME = 92;
constexpr SQLSMALLINT SQL_TYPE_TIMESTAMP = 93;
constexpr SQLSMALLINT SQL_TYPE_UTCDATETIME = 17;
constexpr SQLSMALLINT SQL_TYPE_UTCTIME = 18;
constexpr SQLSMALLINT SQL_LONGVARCHAR = -1;
constexpr SQLSMALLINT SQL_BINARY = -2;
constexpr SQLSMALLINT SQL_VARBINARY = -3;
constexpr SQLSMALLINT SQL_LONGVARBINARY = -4;
constexpr SQLSMALLINT SQL_BIGINT = -5;
constexpr SQLSMALLINT SQL_TINYINT = -6;
constexpr SQLSMALLINT SQL_BIT = -7;
constexpr SQLSMALLINT SQL_WCHAR = -8;
constexpr SQLSMALLINT SQL_WVARCHAR = -9;
constexpr SQLSMALLINT SQL_WLONGVARCHAR = -10;
constexpr SQLSMALLINT SQL_GUID = -11;
constexpr SQLSMALLINT SQL_INTERVAL_YEAR = 101;
constexpr SQLSMALLINT SQL_INTERVAL_MONTH = 102;
constexpr SQLSMALLINT SQL_INTERVAL_DAY = 103;
constexpr SQLSMALLINT SQL_INTERVAL_HOUR = 104;
constexpr SQLSMALLINT SQL_INTERVAL_MINUTE = 105;
constexpr SQLSMALLINT SQL_INTERVAL_SECOND = 106;
constexpr SQLSMALLINT SQL_INTERVAL_YEAR_TO_MONTH = 107;
constexpr SQLSMALLINT SQL_INTERVAL_DAY_TO_HOUR = 108;
constexpr SQLSMALLINT SQL_INTERVAL_DAY_TO_MINUTE = 109;
constexpr SQLSMALLINT SQL_INTERVAL_DAY_TO_SECOND = 110;
constexpr SQLSMALLINT SQL_INTERVAL_HOUR_TO_MINUTE = 111;
constexpr SQLSMALLINT SQL_INTERVAL_HOUR_TO_SECOND = 112;
constexpr SQLSMALLINT SQL_INTERVAL_MINUTE_TO_SECOND = 113;

// =============================================================================
// C Data Types
// =============================================================================

constexpr SQLSMALLINT SQL_C_CHAR = SQL_CHAR;
constexpr SQLSMALLINT SQL_C_WCHAR = SQL_WCHAR;
constexpr SQLSMALLINT SQL_C_LONG = SQL_INTEGER;
constexpr SQLSMALLINT SQL_C_SHORT = SQL_SMALLINT;
constexpr SQLSMALLINT SQL_C_FLOAT = SQL_REAL;
constexpr SQLSMALLINT SQL_C_DOUBLE = SQL_DOUBLE;
constexpr SQLSMALLINT SQL_C_NUMERIC = SQL_NUMERIC;
constexpr SQLSMALLINT SQL_C_DEFAULT = 99;
constexpr SQLSMALLINT SQL_SIGNED_OFFSET = -20;
constexpr SQLSMALLINT SQL_UNSIGNED_OFFSET = -22;
constexpr SQLSMALLINT SQL_C_DATE = SQL_TYPE_DATE;
constexpr SQLSMALLINT SQL_C_TIME = SQL_TYPE_TIME;
constexpr SQLSMALLINT SQL_C_TIMESTAMP = SQL_TYPE_TIMESTAMP;
constexpr SQLSMALLINT SQL_C_TYPE_DATE = SQL_TYPE_DATE;
constexpr SQLSMALLINT SQL_C_TYPE_TIME = SQL_TYPE_TIME;
constexpr SQLSMALLINT SQL_C_TYPE_TIMESTAMP = SQL_TYPE_TIMESTAMP;
constexpr SQLSMALLINT SQL_C_BINARY = SQL_BINARY;
constexpr SQLSMALLINT SQL_C_BIT = SQL_BIT;
constexpr SQLSMALLINT SQL_C_SBIGINT = SQL_BIGINT + SQL_SIGNED_OFFSET;
constexpr SQLSMALLINT SQL_C_UBIGINT = SQL_BIGINT + SQL_UNSIGNED_OFFSET;
constexpr SQLSMALLINT SQL_C_TINYINT = SQL_TINYINT;
constexpr SQLSMALLINT SQL_C_SLONG = SQL_INTEGER + SQL_SIGNED_OFFSET;
constexpr SQLSMALLINT SQL_C_SSHORT = SQL_SMALLINT + SQL_SIGNED_OFFSET;
constexpr SQLSMALLINT SQL_C_STINYINT = SQL_TINYINT + SQL_SIGNED_OFFSET;
constexpr SQLSMALLINT SQL_C_ULONG = SQL_INTEGER + SQL_UNSIGNED_OFFSET;
constexpr SQLSMALLINT SQL_C_USHORT = SQL_SMALLINT + SQL_UNSIGNED_OFFSET;
constexpr SQLSMALLINT SQL_C_UTINYINT = SQL_TINYINT + SQL_UNSIGNED_OFFSET;
constexpr SQLSMALLINT SQL_C_GUID = SQL_GUID;
constexpr SQLSMALLINT SQL_C_INTERVAL_YEAR = SQL_INTERVAL_YEAR;
constexpr SQLSMALLINT SQL_C_INTERVAL_MONTH = SQL_INTERVAL_MONTH;
constexpr SQLSMALLINT SQL_C_INTERVAL_DAY = SQL_INTERVAL_DAY;
constexpr SQLSMALLINT SQL_C_INTERVAL_HOUR = SQL_INTERVAL_HOUR;
constexpr SQLSMALLINT SQL_C_INTERVAL_MINUTE = SQL_INTERVAL_MINUTE;
constexpr SQLSMALLINT SQL_C_INTERVAL_SECOND = SQL_INTERVAL_SECOND;
constexpr SQLSMALLINT SQL_C_INTERVAL_YEAR_TO_MONTH = SQL_INTERVAL_YEAR_TO_MONTH;
constexpr SQLSMALLINT SQL_C_INTERVAL_DAY_TO_HOUR = SQL_INTERVAL_DAY_TO_HOUR;
constexpr SQLSMALLINT SQL_C_INTERVAL_DAY_TO_MINUTE = SQL_INTERVAL_DAY_TO_MINUTE;
constexpr SQLSMALLINT SQL_C_INTERVAL_DAY_TO_SECOND = SQL_INTERVAL_DAY_TO_SECOND;
constexpr SQLSMALLINT SQL_C_INTERVAL_HOUR_TO_MINUTE = SQL_INTERVAL_HOUR_TO_MINUTE;
constexpr SQLSMALLINT SQL_C_INTERVAL_HOUR_TO_SECOND = SQL_INTERVAL_HOUR_TO_SECOND;
constexpr SQLSMALLINT SQL_C_INTERVAL_MINUTE_TO_SECOND = SQL_INTERVAL_MINUTE_TO_SECOND;

// =============================================================================
// Null Indicator
// =============================================================================

constexpr SQLLEN SQL_NULL_DATA = -1;
constexpr SQLLEN SQL_DATA_AT_EXEC = -2;
constexpr SQLLEN SQL_NTS = -3;  // Null-terminated string

// =============================================================================
// Environment Attributes
// =============================================================================

constexpr SQLINTEGER SQL_ATTR_ODBC_VERSION = 200;
constexpr SQLINTEGER SQL_ATTR_CONNECTION_POOLING = 201;
constexpr SQLINTEGER SQL_ATTR_CP_MATCH = 202;
constexpr SQLINTEGER SQL_ATTR_OUTPUT_NTS = 10001;

// ODBC Version values
constexpr SQLUINTEGER SQL_OV_ODBC2 = 2;
constexpr SQLUINTEGER SQL_OV_ODBC3 = 3;
constexpr SQLUINTEGER SQL_OV_ODBC3_80 = 380;

// Connection pooling values
constexpr SQLUINTEGER SQL_CP_OFF = 0;
constexpr SQLUINTEGER SQL_CP_ONE_PER_DRIVER = 1;
constexpr SQLUINTEGER SQL_CP_ONE_PER_HENV = 2;
constexpr SQLUINTEGER SQL_CP_DRIVER_AWARE = 3;
constexpr SQLUINTEGER SQL_CP_DEFAULT = SQL_CP_OFF;

// CP Match values
constexpr SQLUINTEGER SQL_CP_STRICT_MATCH = 0;
constexpr SQLUINTEGER SQL_CP_RELAXED_MATCH = 1;
constexpr SQLUINTEGER SQL_CP_MATCH_DEFAULT = SQL_CP_STRICT_MATCH;

// =============================================================================
// Connection Attributes
// =============================================================================

constexpr SQLINTEGER SQL_ATTR_ACCESS_MODE = 101;
constexpr SQLINTEGER SQL_ATTR_AUTOCOMMIT = 102;
constexpr SQLINTEGER SQL_ATTR_LOGIN_TIMEOUT = 103;
constexpr SQLINTEGER SQL_ATTR_TRACE = 104;
constexpr SQLINTEGER SQL_ATTR_TRACEFILE = 105;
constexpr SQLINTEGER SQL_ATTR_TRANSLATE_LIB = 106;
constexpr SQLINTEGER SQL_ATTR_TRANSLATE_OPTION = 107;
constexpr SQLINTEGER SQL_ATTR_TXN_ISOLATION = 108;
constexpr SQLINTEGER SQL_ATTR_CURRENT_CATALOG = 109;
constexpr SQLINTEGER SQL_ATTR_ODBC_CURSORS = 110;
constexpr SQLINTEGER SQL_ATTR_QUIET_MODE = 111;
constexpr SQLINTEGER SQL_ATTR_PACKET_SIZE = 112;
constexpr SQLINTEGER SQL_ATTR_CONNECTION_TIMEOUT = 113;
constexpr SQLINTEGER SQL_ATTR_DISCONNECT_BEHAVIOR = 114;
constexpr SQLINTEGER SQL_ATTR_ENLIST_IN_DTC = 1207;
constexpr SQLINTEGER SQL_ATTR_ENLIST_IN_XA = 1208;
constexpr SQLINTEGER SQL_ATTR_CONNECTION_DEAD = 1209;
constexpr SQLINTEGER SQL_ATTR_AUTO_IPD = 10001;
constexpr SQLINTEGER SQL_ATTR_METADATA_ID = 10014;
constexpr SQLINTEGER SQL_ATTR_ASYNC_ENABLE = 4;
constexpr SQLINTEGER SQL_ATTR_ASYNC_DBC_FUNCTIONS_ENABLE = 117;

// Access mode values
constexpr SQLUINTEGER SQL_MODE_READ_WRITE = 0;
constexpr SQLUINTEGER SQL_MODE_READ_ONLY = 1;

// Autocommit values
constexpr SQLUINTEGER SQL_AUTOCOMMIT_OFF = 0;
constexpr SQLUINTEGER SQL_AUTOCOMMIT_ON = 1;
constexpr SQLUINTEGER SQL_AUTOCOMMIT_DEFAULT = SQL_AUTOCOMMIT_ON;

// Transaction isolation levels
constexpr SQLUINTEGER SQL_TXN_READ_UNCOMMITTED = 0x00000001;
constexpr SQLUINTEGER SQL_TXN_READ_COMMITTED = 0x00000002;
constexpr SQLUINTEGER SQL_TXN_REPEATABLE_READ = 0x00000004;
constexpr SQLUINTEGER SQL_TXN_SERIALIZABLE = 0x00000008;

// =============================================================================
// Statement Attributes
// =============================================================================

constexpr SQLINTEGER SQL_ATTR_APP_ROW_DESC = 10010;
constexpr SQLINTEGER SQL_ATTR_APP_PARAM_DESC = 10011;
constexpr SQLINTEGER SQL_ATTR_IMP_ROW_DESC = 10012;
constexpr SQLINTEGER SQL_ATTR_IMP_PARAM_DESC = 10013;
constexpr SQLINTEGER SQL_ATTR_CURSOR_SCROLLABLE = -1;
constexpr SQLINTEGER SQL_ATTR_CURSOR_SENSITIVITY = -2;
constexpr SQLINTEGER SQL_ATTR_CURSOR_TYPE = 6;
constexpr SQLINTEGER SQL_ATTR_CONCURRENCY = 7;
constexpr SQLINTEGER SQL_ATTR_ROW_NUMBER = 14;
constexpr SQLINTEGER SQL_ATTR_ROW_BIND_OFFSET_PTR = 23;
constexpr SQLINTEGER SQL_ATTR_ROW_BIND_TYPE = 5;
constexpr SQLINTEGER SQL_ATTR_ROW_ARRAY_SIZE = 27;
constexpr SQLINTEGER SQL_ATTR_ROW_STATUS_PTR = 25;
constexpr SQLINTEGER SQL_ATTR_ROWS_FETCHED_PTR = 26;
constexpr SQLINTEGER SQL_ATTR_PARAM_BIND_OFFSET_PTR = 17;
constexpr SQLINTEGER SQL_ATTR_PARAM_BIND_TYPE = 18;
constexpr SQLINTEGER SQL_ATTR_PARAMSET_SIZE = 22;
constexpr SQLINTEGER SQL_ATTR_PARAM_STATUS_PTR = 20;
constexpr SQLINTEGER SQL_ATTR_PARAMS_PROCESSED_PTR = 21;
constexpr SQLINTEGER SQL_ATTR_QUERY_TIMEOUT = 0;
constexpr SQLINTEGER SQL_ATTR_MAX_ROWS = 1;
constexpr SQLINTEGER SQL_ATTR_MAX_LENGTH = 3;
constexpr SQLINTEGER SQL_ATTR_NOSCAN = 2;
constexpr SQLINTEGER SQL_ATTR_KEYSET_SIZE = 8;
constexpr SQLINTEGER SQL_ATTR_SIMULATE_CURSOR = 10;
constexpr SQLINTEGER SQL_ATTR_RETRIEVE_DATA = 11;
constexpr SQLINTEGER SQL_ATTR_USE_BOOKMARKS = 12;
constexpr SQLINTEGER SQL_ATTR_FETCH_BOOKMARK_PTR = 16;
constexpr SQLINTEGER SQL_ATTR_ENABLE_AUTO_IPD = 15;
constexpr SQLINTEGER SQL_ATTR_ASYNC_STMT_EVENT = 29;

// Cursor types
constexpr SQLUINTEGER SQL_CURSOR_FORWARD_ONLY = 0;
constexpr SQLUINTEGER SQL_CURSOR_KEYSET_DRIVEN = 1;
constexpr SQLUINTEGER SQL_CURSOR_DYNAMIC = 2;
constexpr SQLUINTEGER SQL_CURSOR_STATIC = 3;
constexpr SQLUINTEGER SQL_CURSOR_TYPE_DEFAULT = SQL_CURSOR_FORWARD_ONLY;

// Concurrency options
constexpr SQLUINTEGER SQL_CONCUR_READ_ONLY = 1;
constexpr SQLUINTEGER SQL_CONCUR_LOCK = 2;
constexpr SQLUINTEGER SQL_CONCUR_ROWVER = 3;
constexpr SQLUINTEGER SQL_CONCUR_VALUES = 4;
constexpr SQLUINTEGER SQL_CONCUR_DEFAULT = SQL_CONCUR_READ_ONLY;

// Row status values
constexpr SQLUSMALLINT SQL_ROW_SUCCESS = 0;
constexpr SQLUSMALLINT SQL_ROW_DELETED = 1;
constexpr SQLUSMALLINT SQL_ROW_UPDATED = 2;
constexpr SQLUSMALLINT SQL_ROW_NOROW = 3;
constexpr SQLUSMALLINT SQL_ROW_ADDED = 4;
constexpr SQLUSMALLINT SQL_ROW_ERROR = 5;
constexpr SQLUSMALLINT SQL_ROW_SUCCESS_WITH_INFO = 6;
constexpr SQLUSMALLINT SQL_ROW_PROCEED = 0;
constexpr SQLUSMALLINT SQL_ROW_IGNORE = 1;

// Param status values
constexpr SQLUSMALLINT SQL_PARAM_SUCCESS = 0;
constexpr SQLUSMALLINT SQL_PARAM_SUCCESS_WITH_INFO = 6;
constexpr SQLUSMALLINT SQL_PARAM_ERROR = 5;
constexpr SQLUSMALLINT SQL_PARAM_UNUSED = 7;
constexpr SQLUSMALLINT SQL_PARAM_DIAG_UNAVAILABLE = 1;

// =============================================================================
// Fetch Orientation
// =============================================================================

constexpr SQLUSMALLINT SQL_FETCH_NEXT = 1;
constexpr SQLUSMALLINT SQL_FETCH_FIRST = 2;
constexpr SQLUSMALLINT SQL_FETCH_LAST = 3;
constexpr SQLUSMALLINT SQL_FETCH_PRIOR = 4;
constexpr SQLUSMALLINT SQL_FETCH_ABSOLUTE = 5;
constexpr SQLUSMALLINT SQL_FETCH_RELATIVE = 6;
constexpr SQLUSMALLINT SQL_FETCH_BOOKMARK = 8;

// =============================================================================
// Parameter Input/Output Type
// =============================================================================

constexpr SQLSMALLINT SQL_PARAM_INPUT = 1;
constexpr SQLSMALLINT SQL_PARAM_INPUT_OUTPUT = 2;
constexpr SQLSMALLINT SQL_PARAM_OUTPUT = 4;
constexpr SQLSMALLINT SQL_PARAM_INPUT_OUTPUT_STREAM = 8;
constexpr SQLSMALLINT SQL_PARAM_OUTPUT_STREAM = 16;

// =============================================================================
// Free Statement Options
// =============================================================================

constexpr SQLUSMALLINT SQL_CLOSE = 0;
constexpr SQLUSMALLINT SQL_DROP = 1;
constexpr SQLUSMALLINT SQL_UNBIND = 2;
constexpr SQLUSMALLINT SQL_RESET_PARAMS = 3;

// =============================================================================
// Driver Connect Options
// =============================================================================

constexpr SQLUSMALLINT SQL_DRIVER_NOPROMPT = 0;
constexpr SQLUSMALLINT SQL_DRIVER_COMPLETE = 1;
constexpr SQLUSMALLINT SQL_DRIVER_PROMPT = 2;
constexpr SQLUSMALLINT SQL_DRIVER_COMPLETE_REQUIRED = 3;

// =============================================================================
// GetInfo Types
// =============================================================================

constexpr SQLUSMALLINT SQL_DRIVER_NAME = 6;
constexpr SQLUSMALLINT SQL_DRIVER_VER = 7;
constexpr SQLUSMALLINT SQL_ODBC_VER = 10;
constexpr SQLUSMALLINT SQL_DRIVER_ODBC_VER = 77;
constexpr SQLUSMALLINT SQL_DBMS_NAME = 17;
constexpr SQLUSMALLINT SQL_DBMS_VER = 18;
constexpr SQLUSMALLINT SQL_DATABASE_NAME = 16;
constexpr SQLUSMALLINT SQL_SERVER_NAME = 13;
constexpr SQLUSMALLINT SQL_USER_NAME = 47;
constexpr SQLUSMALLINT SQL_DATA_SOURCE_NAME = 2;
constexpr SQLUSMALLINT SQL_DATA_SOURCE_READ_ONLY = 25;
constexpr SQLUSMALLINT SQL_ACCESSIBLE_TABLES = 19;
constexpr SQLUSMALLINT SQL_ACCESSIBLE_PROCEDURES = 20;
constexpr SQLUSMALLINT SQL_ACTIVE_ENVIRONMENTS = 116;
constexpr SQLUSMALLINT SQL_AGGREGATE_FUNCTIONS = 169;
constexpr SQLUSMALLINT SQL_ALTER_DOMAIN = 117;
constexpr SQLUSMALLINT SQL_ALTER_TABLE = 86;
constexpr SQLUSMALLINT SQL_ASYNC_MODE = 10021;
constexpr SQLUSMALLINT SQL_BATCH_ROW_COUNT = 120;
constexpr SQLUSMALLINT SQL_BATCH_SUPPORT = 121;
constexpr SQLUSMALLINT SQL_BOOKMARK_PERSISTENCE = 82;
constexpr SQLUSMALLINT SQL_CATALOG_LOCATION = 114;
constexpr SQLUSMALLINT SQL_CATALOG_NAME = 10003;
constexpr SQLUSMALLINT SQL_CATALOG_NAME_SEPARATOR = 41;
constexpr SQLUSMALLINT SQL_CATALOG_TERM = 42;
constexpr SQLUSMALLINT SQL_CATALOG_USAGE = 92;
constexpr SQLUSMALLINT SQL_COLLATION_SEQ = 10004;
constexpr SQLUSMALLINT SQL_COLUMN_ALIAS = 87;
constexpr SQLUSMALLINT SQL_CONCAT_NULL_BEHAVIOR = 22;
constexpr SQLUSMALLINT SQL_CONVERT_BIGINT = 53;
constexpr SQLUSMALLINT SQL_CONVERT_BINARY = 54;
constexpr SQLUSMALLINT SQL_CONVERT_BIT = 55;
constexpr SQLUSMALLINT SQL_CONVERT_CHAR = 56;
constexpr SQLUSMALLINT SQL_CONVERT_DATE = 57;
constexpr SQLUSMALLINT SQL_CONVERT_DECIMAL = 58;
constexpr SQLUSMALLINT SQL_CONVERT_DOUBLE = 59;
constexpr SQLUSMALLINT SQL_CONVERT_FLOAT = 60;
constexpr SQLUSMALLINT SQL_CONVERT_INTEGER = 61;
constexpr SQLUSMALLINT SQL_CONVERT_LONGVARBINARY = 71;
constexpr SQLUSMALLINT SQL_CONVERT_LONGVARCHAR = 62;
constexpr SQLUSMALLINT SQL_CONVERT_NUMERIC = 63;
constexpr SQLUSMALLINT SQL_CONVERT_REAL = 64;
constexpr SQLUSMALLINT SQL_CONVERT_SMALLINT = 65;
constexpr SQLUSMALLINT SQL_CONVERT_TIME = 66;
constexpr SQLUSMALLINT SQL_CONVERT_TIMESTAMP = 67;
constexpr SQLUSMALLINT SQL_CONVERT_TINYINT = 68;
constexpr SQLUSMALLINT SQL_CONVERT_VARBINARY = 69;
constexpr SQLUSMALLINT SQL_CONVERT_VARCHAR = 70;
constexpr SQLUSMALLINT SQL_CONVERT_FUNCTIONS = 48;
constexpr SQLUSMALLINT SQL_CORRELATION_NAME = 74;
constexpr SQLUSMALLINT SQL_CREATE_ASSERTION = 127;
constexpr SQLUSMALLINT SQL_CREATE_CHARACTER_SET = 128;
constexpr SQLUSMALLINT SQL_CREATE_COLLATION = 129;
constexpr SQLUSMALLINT SQL_CREATE_DOMAIN = 130;
constexpr SQLUSMALLINT SQL_CREATE_SCHEMA = 131;
constexpr SQLUSMALLINT SQL_CREATE_TABLE = 132;
constexpr SQLUSMALLINT SQL_CREATE_TRANSLATION = 133;
constexpr SQLUSMALLINT SQL_CREATE_VIEW = 134;
constexpr SQLUSMALLINT SQL_CURSOR_COMMIT_BEHAVIOR = 23;
constexpr SQLUSMALLINT SQL_CURSOR_ROLLBACK_BEHAVIOR = 24;
constexpr SQLUSMALLINT SQL_CURSOR_SENSITIVITY_VAL = 10001;
constexpr SQLUSMALLINT SQL_DATETIME_LITERALS = 119;
constexpr SQLUSMALLINT SQL_DDL_INDEX = 170;
constexpr SQLUSMALLINT SQL_DEFAULT_TXN_ISOLATION = 26;
constexpr SQLUSMALLINT SQL_DESCRIBE_PARAMETER = 10002;
constexpr SQLUSMALLINT SQL_DM_VER = 171;
constexpr SQLUSMALLINT SQL_DRIVER_AWARE_POOLING_SUPPORTED = 10024;
constexpr SQLUSMALLINT SQL_DRIVER_HDBC = 3;
constexpr SQLUSMALLINT SQL_DRIVER_HDESC = 135;
constexpr SQLUSMALLINT SQL_DRIVER_HENV = 4;
constexpr SQLUSMALLINT SQL_DRIVER_HLIB = 76;
constexpr SQLUSMALLINT SQL_DRIVER_HSTMT = 5;
constexpr SQLUSMALLINT SQL_DROP_ASSERTION = 136;
constexpr SQLUSMALLINT SQL_DROP_CHARACTER_SET = 137;
constexpr SQLUSMALLINT SQL_DROP_COLLATION = 138;
constexpr SQLUSMALLINT SQL_DROP_DOMAIN = 139;
constexpr SQLUSMALLINT SQL_DROP_SCHEMA = 140;
constexpr SQLUSMALLINT SQL_DROP_TABLE = 141;
constexpr SQLUSMALLINT SQL_DROP_TRANSLATION = 142;
constexpr SQLUSMALLINT SQL_DROP_VIEW = 143;
constexpr SQLUSMALLINT SQL_DYNAMIC_CURSOR_ATTRIBUTES1 = 144;
constexpr SQLUSMALLINT SQL_DYNAMIC_CURSOR_ATTRIBUTES2 = 145;
constexpr SQLUSMALLINT SQL_EXPRESSIONS_IN_ORDERBY = 27;
constexpr SQLUSMALLINT SQL_FILE_USAGE = 84;
constexpr SQLUSMALLINT SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1 = 146;
constexpr SQLUSMALLINT SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2 = 147;
constexpr SQLUSMALLINT SQL_GETDATA_EXTENSIONS = 81;
constexpr SQLUSMALLINT SQL_GROUP_BY = 88;
constexpr SQLUSMALLINT SQL_IDENTIFIER_CASE = 28;
constexpr SQLUSMALLINT SQL_IDENTIFIER_QUOTE_CHAR = 29;
constexpr SQLUSMALLINT SQL_INDEX_KEYWORDS = 148;
constexpr SQLUSMALLINT SQL_INFO_SCHEMA_VIEWS = 149;
constexpr SQLUSMALLINT SQL_INSERT_STATEMENT = 172;
constexpr SQLUSMALLINT SQL_INTEGRITY = 73;
constexpr SQLUSMALLINT SQL_KEYSET_CURSOR_ATTRIBUTES1 = 150;
constexpr SQLUSMALLINT SQL_KEYSET_CURSOR_ATTRIBUTES2 = 151;
constexpr SQLUSMALLINT SQL_KEYWORDS = 89;
constexpr SQLUSMALLINT SQL_LIKE_ESCAPE_CLAUSE = 113;
constexpr SQLUSMALLINT SQL_MAX_ASYNC_CONCURRENT_STATEMENTS = 10022;
constexpr SQLUSMALLINT SQL_MAX_BINARY_LITERAL_LEN = 112;
constexpr SQLUSMALLINT SQL_MAX_CATALOG_NAME_LEN = 34;
constexpr SQLUSMALLINT SQL_MAX_CHAR_LITERAL_LEN = 108;
constexpr SQLUSMALLINT SQL_MAX_COLUMN_NAME_LEN = 30;
constexpr SQLUSMALLINT SQL_MAX_COLUMNS_IN_GROUP_BY = 97;
constexpr SQLUSMALLINT SQL_MAX_COLUMNS_IN_INDEX = 98;
constexpr SQLUSMALLINT SQL_MAX_COLUMNS_IN_ORDER_BY = 99;
constexpr SQLUSMALLINT SQL_MAX_COLUMNS_IN_SELECT = 100;
constexpr SQLUSMALLINT SQL_MAX_COLUMNS_IN_TABLE = 101;
constexpr SQLUSMALLINT SQL_MAX_CONCURRENT_ACTIVITIES = 1;
constexpr SQLUSMALLINT SQL_MAX_CURSOR_NAME_LEN = 31;
constexpr SQLUSMALLINT SQL_MAX_DRIVER_CONNECTIONS = 0;
constexpr SQLUSMALLINT SQL_MAX_IDENTIFIER_LEN = 10005;
constexpr SQLUSMALLINT SQL_MAX_INDEX_SIZE = 102;
constexpr SQLUSMALLINT SQL_MAX_PROCEDURE_NAME_LEN = 33;
constexpr SQLUSMALLINT SQL_MAX_ROW_SIZE = 104;
constexpr SQLUSMALLINT SQL_MAX_ROW_SIZE_INCLUDES_LONG = 103;
constexpr SQLUSMALLINT SQL_MAX_SCHEMA_NAME_LEN = 32;
constexpr SQLUSMALLINT SQL_MAX_STATEMENT_LEN = 105;
constexpr SQLUSMALLINT SQL_MAX_TABLE_NAME_LEN = 35;
constexpr SQLUSMALLINT SQL_MAX_TABLES_IN_SELECT = 106;
constexpr SQLUSMALLINT SQL_MAX_USER_NAME_LEN = 107;
constexpr SQLUSMALLINT SQL_MULT_RESULT_SETS = 36;
constexpr SQLUSMALLINT SQL_MULTIPLE_ACTIVE_TXN = 37;
constexpr SQLUSMALLINT SQL_NEED_LONG_DATA_LEN = 111;
constexpr SQLUSMALLINT SQL_NON_NULLABLE_COLUMNS = 75;
constexpr SQLUSMALLINT SQL_NULL_COLLATION = 85;
constexpr SQLUSMALLINT SQL_NUMERIC_FUNCTIONS = 49;
constexpr SQLUSMALLINT SQL_ODBC_API_CONFORMANCE = 9;
constexpr SQLUSMALLINT SQL_ODBC_INTERFACE_CONFORMANCE = 152;
constexpr SQLUSMALLINT SQL_ODBC_SQL_CONFORMANCE = 15;
constexpr SQLUSMALLINT SQL_OJ_CAPABILITIES = 115;
constexpr SQLUSMALLINT SQL_ORDER_BY_COLUMNS_IN_SELECT = 90;
constexpr SQLUSMALLINT SQL_OUTER_JOINS = 38;
constexpr SQLUSMALLINT SQL_PARAM_ARRAY_ROW_COUNTS = 153;
constexpr SQLUSMALLINT SQL_PARAM_ARRAY_SELECTS = 154;
constexpr SQLUSMALLINT SQL_POS_OPERATIONS = 79;
constexpr SQLUSMALLINT SQL_POSITIONED_STATEMENTS = 80;
constexpr SQLUSMALLINT SQL_PROCEDURE_TERM = 40;
constexpr SQLUSMALLINT SQL_PROCEDURES = 21;
constexpr SQLUSMALLINT SQL_QUOTED_IDENTIFIER_CASE = 93;
constexpr SQLUSMALLINT SQL_ROW_UPDATES = 11;
constexpr SQLUSMALLINT SQL_SCHEMA_TERM = 39;
constexpr SQLUSMALLINT SQL_SCHEMA_USAGE = 91;
constexpr SQLUSMALLINT SQL_SCROLL_OPTIONS = 44;
constexpr SQLUSMALLINT SQL_SEARCH_PATTERN_ESCAPE = 14;
constexpr SQLUSMALLINT SQL_SPECIAL_CHARACTERS = 94;
constexpr SQLUSMALLINT SQL_SQL_CONFORMANCE = 118;
constexpr SQLUSMALLINT SQL_SQL92_DATETIME_FUNCTIONS = 155;
constexpr SQLUSMALLINT SQL_SQL92_FOREIGN_KEY_DELETE_RULE = 156;
constexpr SQLUSMALLINT SQL_SQL92_FOREIGN_KEY_UPDATE_RULE = 157;
constexpr SQLUSMALLINT SQL_SQL92_GRANT = 158;
constexpr SQLUSMALLINT SQL_SQL92_NUMERIC_VALUE_FUNCTIONS = 159;
constexpr SQLUSMALLINT SQL_SQL92_PREDICATES = 160;
constexpr SQLUSMALLINT SQL_SQL92_RELATIONAL_JOIN_OPERATORS = 161;
constexpr SQLUSMALLINT SQL_SQL92_REVOKE = 162;
constexpr SQLUSMALLINT SQL_SQL92_ROW_VALUE_CONSTRUCTOR = 163;
constexpr SQLUSMALLINT SQL_SQL92_STRING_FUNCTIONS = 164;
constexpr SQLUSMALLINT SQL_SQL92_VALUE_EXPRESSIONS = 165;
constexpr SQLUSMALLINT SQL_STANDARD_CLI_CONFORMANCE = 166;
constexpr SQLUSMALLINT SQL_STATIC_CURSOR_ATTRIBUTES1 = 167;
constexpr SQLUSMALLINT SQL_STATIC_CURSOR_ATTRIBUTES2 = 168;
constexpr SQLUSMALLINT SQL_STRING_FUNCTIONS = 50;
constexpr SQLUSMALLINT SQL_SUBQUERIES = 95;
constexpr SQLUSMALLINT SQL_SYSTEM_FUNCTIONS = 51;
constexpr SQLUSMALLINT SQL_TABLE_TERM = 45;
constexpr SQLUSMALLINT SQL_TIMEDATE_ADD_INTERVALS = 109;
constexpr SQLUSMALLINT SQL_TIMEDATE_DIFF_INTERVALS = 110;
constexpr SQLUSMALLINT SQL_TIMEDATE_FUNCTIONS = 52;
constexpr SQLUSMALLINT SQL_TXN_CAPABLE = 46;
constexpr SQLUSMALLINT SQL_TXN_ISOLATION_OPTION = 72;
constexpr SQLUSMALLINT SQL_UNION = 96;
constexpr SQLUSMALLINT SQL_XOPEN_CLI_YEAR = 10000;

// =============================================================================
// Nullable
// =============================================================================

constexpr SQLSMALLINT SQL_NO_NULLS = 0;
constexpr SQLSMALLINT SQL_NULLABLE = 1;
constexpr SQLSMALLINT SQL_NULLABLE_UNKNOWN = 2;

// =============================================================================
// End Transaction Options
// =============================================================================

constexpr SQLSMALLINT SQL_COMMIT = 0;
constexpr SQLSMALLINT SQL_ROLLBACK = 1;

// =============================================================================
// Column description flags
// =============================================================================

constexpr SQLUSMALLINT SQL_COLUMN_COUNT = 0;
constexpr SQLUSMALLINT SQL_COLUMN_NAME = 1;
constexpr SQLUSMALLINT SQL_COLUMN_TYPE = 2;
constexpr SQLUSMALLINT SQL_COLUMN_LENGTH = 3;
constexpr SQLUSMALLINT SQL_COLUMN_PRECISION = 4;
constexpr SQLUSMALLINT SQL_COLUMN_SCALE = 5;
constexpr SQLUSMALLINT SQL_COLUMN_DISPLAY_SIZE = 6;
constexpr SQLUSMALLINT SQL_COLUMN_NULLABLE = 7;
constexpr SQLUSMALLINT SQL_COLUMN_UNSIGNED = 8;
constexpr SQLUSMALLINT SQL_COLUMN_MONEY = 9;
constexpr SQLUSMALLINT SQL_COLUMN_UPDATABLE = 10;
constexpr SQLUSMALLINT SQL_COLUMN_AUTO_INCREMENT = 11;
constexpr SQLUSMALLINT SQL_COLUMN_CASE_SENSITIVE = 12;
constexpr SQLUSMALLINT SQL_COLUMN_SEARCHABLE = 13;
constexpr SQLUSMALLINT SQL_COLUMN_TYPE_NAME = 14;
constexpr SQLUSMALLINT SQL_COLUMN_TABLE_NAME = 15;
constexpr SQLUSMALLINT SQL_COLUMN_OWNER_NAME = 16;
constexpr SQLUSMALLINT SQL_COLUMN_QUALIFIER_NAME = 17;
constexpr SQLUSMALLINT SQL_COLUMN_LABEL = 18;
constexpr SQLUSMALLINT SQL_DESC_COUNT = 1001;
constexpr SQLUSMALLINT SQL_DESC_TYPE = 1002;
constexpr SQLUSMALLINT SQL_DESC_LENGTH = 1003;
constexpr SQLUSMALLINT SQL_DESC_OCTET_LENGTH_PTR = 1004;
constexpr SQLUSMALLINT SQL_DESC_PRECISION = 1005;
constexpr SQLUSMALLINT SQL_DESC_SCALE = 1006;
constexpr SQLUSMALLINT SQL_DESC_DATETIME_INTERVAL_CODE = 1007;
constexpr SQLUSMALLINT SQL_DESC_NULLABLE = 1008;
constexpr SQLUSMALLINT SQL_DESC_INDICATOR_PTR = 1009;
constexpr SQLUSMALLINT SQL_DESC_DATA_PTR = 1010;
constexpr SQLUSMALLINT SQL_DESC_NAME = 1011;
constexpr SQLUSMALLINT SQL_DESC_UNNAMED = 1012;
constexpr SQLUSMALLINT SQL_DESC_OCTET_LENGTH = 1013;
constexpr SQLUSMALLINT SQL_DESC_ALLOC_TYPE = 1099;
constexpr SQLUSMALLINT SQL_DESC_CONCISE_TYPE = SQL_COLUMN_TYPE;
constexpr SQLUSMALLINT SQL_DESC_DISPLAY_SIZE = SQL_COLUMN_DISPLAY_SIZE;
constexpr SQLUSMALLINT SQL_DESC_UNSIGNED = SQL_COLUMN_UNSIGNED;
constexpr SQLUSMALLINT SQL_DESC_UPDATABLE = SQL_COLUMN_UPDATABLE;
constexpr SQLUSMALLINT SQL_DESC_AUTO_UNIQUE_VALUE = SQL_COLUMN_AUTO_INCREMENT;
constexpr SQLUSMALLINT SQL_DESC_CASE_SENSITIVE = SQL_COLUMN_CASE_SENSITIVE;
constexpr SQLUSMALLINT SQL_DESC_SEARCHABLE = SQL_COLUMN_SEARCHABLE;
constexpr SQLUSMALLINT SQL_DESC_TYPE_NAME = SQL_COLUMN_TYPE_NAME;
constexpr SQLUSMALLINT SQL_DESC_TABLE_NAME = SQL_COLUMN_TABLE_NAME;
constexpr SQLUSMALLINT SQL_DESC_SCHEMA_NAME = SQL_COLUMN_OWNER_NAME;
constexpr SQLUSMALLINT SQL_DESC_CATALOG_NAME = SQL_COLUMN_QUALIFIER_NAME;
constexpr SQLUSMALLINT SQL_DESC_LABEL = SQL_COLUMN_LABEL;
constexpr SQLUSMALLINT SQL_DESC_BASE_COLUMN_NAME = 22;
constexpr SQLUSMALLINT SQL_DESC_BASE_TABLE_NAME = 23;
constexpr SQLUSMALLINT SQL_DESC_LITERAL_PREFIX = 27;
constexpr SQLUSMALLINT SQL_DESC_LITERAL_SUFFIX = 28;
constexpr SQLUSMALLINT SQL_DESC_LOCAL_TYPE_NAME = 29;
constexpr SQLUSMALLINT SQL_DESC_NUM_PREC_RADIX = 32;
constexpr SQLUSMALLINT SQL_DESC_FIXED_PREC_SCALE = 9;

// =============================================================================
// Diagnostic fields
// =============================================================================

constexpr SQLSMALLINT SQL_DIAG_CURSOR_ROW_COUNT = -1249;
constexpr SQLSMALLINT SQL_DIAG_ROW_NUMBER = -1248;
constexpr SQLSMALLINT SQL_DIAG_COLUMN_NUMBER = -1247;
constexpr SQLSMALLINT SQL_DIAG_RETURNCODE = 1;
constexpr SQLSMALLINT SQL_DIAG_NUMBER = 2;
constexpr SQLSMALLINT SQL_DIAG_ROW_COUNT = 3;
constexpr SQLSMALLINT SQL_DIAG_SQLSTATE = 4;
constexpr SQLSMALLINT SQL_DIAG_NATIVE = 5;
constexpr SQLSMALLINT SQL_DIAG_MESSAGE_TEXT = 6;
constexpr SQLSMALLINT SQL_DIAG_DYNAMIC_FUNCTION = 7;
constexpr SQLSMALLINT SQL_DIAG_CLASS_ORIGIN = 8;
constexpr SQLSMALLINT SQL_DIAG_SUBCLASS_ORIGIN = 9;
constexpr SQLSMALLINT SQL_DIAG_CONNECTION_NAME = 10;
constexpr SQLSMALLINT SQL_DIAG_SERVER_NAME = 11;
constexpr SQLSMALLINT SQL_DIAG_DYNAMIC_FUNCTION_CODE = 12;

// =============================================================================
// SQL Data Structures
// =============================================================================

/**
 * @brief ODBC DATE structure
 */
struct SQL_DATE_STRUCT {
    SQLSMALLINT year;
    SQLUSMALLINT month;
    SQLUSMALLINT day;
};

/**
 * @brief ODBC TIME structure
 */
struct SQL_TIME_STRUCT {
    SQLUSMALLINT hour;
    SQLUSMALLINT minute;
    SQLUSMALLINT second;
};

/**
 * @brief ODBC TIMESTAMP structure
 */
struct SQL_TIMESTAMP_STRUCT {
    SQLSMALLINT year;
    SQLUSMALLINT month;
    SQLUSMALLINT day;
    SQLUSMALLINT hour;
    SQLUSMALLINT minute;
    SQLUSMALLINT second;
    SQLUINTEGER fraction;  // nanoseconds
};

/**
 * @brief ODBC NUMERIC structure
 */
struct SQL_NUMERIC_STRUCT {
    SQLCHAR precision;
    SQLSCHAR scale;
    SQLCHAR sign;  // 1 = positive, 0 = negative
    SQLCHAR val[16];  // little-endian
};

/**
 * @brief ODBC INTERVAL structure
 */
struct SQL_YEAR_MONTH_STRUCT {
    SQLUINTEGER year;
    SQLUINTEGER month;
};

struct SQL_DAY_SECOND_STRUCT {
    SQLUINTEGER day;
    SQLUINTEGER hour;
    SQLUINTEGER minute;
    SQLUINTEGER second;
    SQLUINTEGER fraction;
};

struct SQL_INTERVAL_STRUCT {
    SQLSMALLINT interval_type;
    SQLSMALLINT interval_sign;
    union {
        SQL_YEAR_MONTH_STRUCT year_month;
        SQL_DAY_SECOND_STRUCT day_second;
    } intval;
};

// =============================================================================
// Internal Driver Structures
// =============================================================================

/**
 * @brief Diagnostic record for error reporting
 */
struct DiagnosticRecord {
    std::string sqlstate{"00000"};
    SQLINTEGER native_error{0};
    std::string message;
    std::string class_origin{"ODBC 3.0"};
    std::string subclass_origin{"ODBC 3.0"};
    std::string connection_name;
    std::string server_name;
    SQLINTEGER row_number{0};
    SQLINTEGER column_number{0};
};

/**
 * @brief Column binding information
 */
struct ColumnBinding {
    SQLSMALLINT target_type{SQL_C_DEFAULT};
    SQLPOINTER target_value{nullptr};
    SQLLEN buffer_length{0};
    SQLLEN* str_len_or_ind{nullptr};
};

/**
 * @brief Parameter binding information
 */
struct ParameterBinding {
    SQLSMALLINT input_output_type{SQL_PARAM_INPUT};
    SQLSMALLINT value_type{SQL_C_DEFAULT};
    SQLSMALLINT parameter_type{SQL_UNKNOWN_TYPE};
    SQLULEN column_size{0};
    SQLSMALLINT decimal_digits{0};
    SQLPOINTER parameter_value{nullptr};
    SQLLEN buffer_length{0};
    SQLLEN* str_len_or_ind{nullptr};
};

/**
 * @brief Parameter literal for SQL substitution
 */
struct ParameterLiteral {
    std::string text;
    bool quoted{true};
};

/**
 * @brief Result column metadata
 */
struct ColumnMetadata {
    std::string name;
    std::string type_name;
    std::string table_name;
    std::string schema_name;
    std::string catalog_name;
    std::string label;
    SQLSMALLINT sql_type{SQL_UNKNOWN_TYPE};
    SQLULEN column_size{0};
    SQLSMALLINT decimal_digits{0};
    SQLSMALLINT nullable{SQL_NULLABLE_UNKNOWN};
    bool unsigned_flag{false};
    bool auto_increment{false};
    bool case_sensitive{true};
    SQLSMALLINT searchable{2};  // SQL_SEARCHABLE
    SQLLEN display_size{0};
    SQLLEN octet_length{0};
};

/**
 * @brief Connection string parameters
 */
struct ConnectionParams {
    std::string driver;
    std::string dsn;
    std::string server{"localhost"};
    uint16_t port{3092};
    std::string database;
    std::string user;
    std::string password;
    std::string ssl_mode{"prefer"};
    std::string ssl_cert;
    std::string ssl_key;
    std::string ssl_root_cert;
    std::string protocol{"native"};
    uint32_t connect_timeout{30};
    uint32_t query_timeout{0};
    std::string application_name;
    std::string schema{"public"};
    std::string charset{"UTF8"};
    bool read_only{false};
    bool auto_commit{true};
    uint32_t packet_size{8192};
    bool pooling{true};
};

/**
 * @brief Driver configuration
 */
struct DriverConfig {
    static constexpr const char* DRIVER_NAME = "ScratchBird ODBC Driver";
    static constexpr const char* DRIVER_VERSION = "01.00.0000";
    static constexpr const char* ODBC_VERSION = "03.80";
    static constexpr const char* DBMS_NAME = "ScratchBird";
    static constexpr const char* DBMS_VERSION = "01.00.0000";
    static constexpr uint32_t MAX_CATALOG_NAME_LEN = 128;
    static constexpr uint32_t MAX_SCHEMA_NAME_LEN = 128;
    static constexpr uint32_t MAX_TABLE_NAME_LEN = 128;
    static constexpr uint32_t MAX_COLUMN_NAME_LEN = 128;
    static constexpr uint32_t MAX_COLUMNS_IN_INDEX = 32;
    static constexpr uint32_t MAX_COLUMNS_IN_TABLE = 1600;
};

}  // namespace odbc
}  // namespace scratchbird

#endif  // SCRATCHBIRD_ODBC_TYPES_H

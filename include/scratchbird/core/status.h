#pragma once

#include <cstdint>

namespace scratchbird::core
{

    enum class Status : uint32_t
    {
        OK = 0,

        // ====================================================================
        // File/IO Errors (1000-1099)
        // ====================================================================
        FILE_NOT_FOUND = 1001,
        FILE_EXISTS = 1002,
        IO_ERROR = 1003,
        INVALID_PATH = 1004,
        PERMISSION_DENIED = 1005,
        INVALID_ARGUMENT = 1006,

        // ====================================================================
        // Corruption Errors (2000-2099)
        // ====================================================================
        PAGE_CORRUPT = 2001,
        CHECKSUM_MISMATCH = 2002,
        INDEX_CORRUPTED = 2003,
        DATA_CORRUPTED = 2004,

        // ====================================================================
        // Transaction/Lock Errors (3000-3099)
        // ====================================================================
        DEADLOCK = 3001,
        LOCK_TIMEOUT = 3002,
        LOCK_CONFLICT = 3003,
        OOM = 3004,
        CANCELLED = 3005,
        SERIALIZATION_FAILURE = 3006,
        INVALID_TRANSACTION_STATE = 3007,
        NO_ACTIVE_TRANSACTION = 3008,
        TRANSACTION_ABORTED = 3009,
        READ_ONLY_TRANSACTION = 3010,

        // ====================================================================
        // Data Errors (4000-4099)
        // ====================================================================
        PAGE_FULL = 4001,
        NOT_FOUND = 4002,
        NOT_IMPLEMENTED = 4003,
        TYPE_MISMATCH = 4004,
        CONSTRAINT_VIOLATION = 4005,
        OUT_OF_RANGE = 4006,
        NOT_SUPPORTED = 4007,
        DIVISION_BY_ZERO = 4008,
        NUMERIC_VALUE_OUT_OF_RANGE = 4009,
        STRING_DATA_RIGHT_TRUNCATION = 4010,
        DATETIME_FIELD_OVERFLOW = 4011,
        INVALID_DATETIME_FORMAT = 4012,
        INVALID_TEXT_REPRESENTATION = 4013,
        NULL_VALUE_NOT_ALLOWED = 4014,

        // ====================================================================
        // Constraint Violations (4100-4199)
        // ====================================================================
        NOT_NULL_VIOLATION = 4101,
        FOREIGN_KEY_VIOLATION = 4102,
        UNIQUE_VIOLATION = 4103,
        CHECK_VIOLATION = 4104,
        EXCLUSION_VIOLATION = 4105,

        // ====================================================================
        // Syntax/Semantic Errors (4200-4299)
        // ====================================================================
        SYNTAX_ERROR = 4201,
        UNDEFINED_TABLE = 4202,
        UNDEFINED_COLUMN = 4203,
        UNDEFINED_FUNCTION = 4204,
        UNDEFINED_OBJECT = 4205,
        DUPLICATE_TABLE = 4206,
        DUPLICATE_COLUMN = 4207,
        DUPLICATE_OBJECT = 4208,
        AMBIGUOUS_COLUMN = 4209,
        AMBIGUOUS_FUNCTION = 4210,
        DATATYPE_MISMATCH = 4211,
        WRONG_OBJECT_TYPE = 4212,
        INSUFFICIENT_PRIVILEGE = 4213,

        // ====================================================================
        // Cursor Errors (4300-4399)
        // ====================================================================
        INVALID_CURSOR_STATE = 4301,
        INVALID_CURSOR_NAME = 4302,
        CURSOR_NOT_FOUND = 4303,
        CURSOR_ALREADY_OPEN = 4304,
        CURSOR_NOT_OPEN = 4305,

        // ====================================================================
        // PL/pgSQL Errors (4400-4499)
        // ====================================================================
        NO_DATA_FOUND = 4401,        // PL/pgSQL NO_DATA_FOUND
        TOO_MANY_ROWS = 4402,        // PL/pgSQL TOO_MANY_ROWS
        RAISE_EXCEPTION = 4403,      // PL/pgSQL RAISE
        ASSERT_FAILURE = 4404,       // PL/pgSQL ASSERT

        // ====================================================================
        // Resource Errors (5000-5099)
        // ====================================================================
        COMPRESSION_ERROR = 5001,
        INTERNAL_ERROR = 5002,
        INDEX_NOT_FOUND = 5003,
        DISK_FULL = 5004,
        TOO_MANY_CONNECTIONS = 5005,
        CONFIGURATION_LIMIT_EXCEEDED = 5006,
        STATEMENT_TOO_COMPLEX = 5007,
        TOO_MANY_COLUMNS = 5008,

        // ====================================================================
        // Connection Errors (6000-6099)
        // ====================================================================
        CONNECTION_FAILURE = 6001,
        CONNECTION_DOES_NOT_EXIST = 6002,
        PROTOCOL_VIOLATION = 6003,
        INVALID_PASSWORD = 6004,
        INVALID_AUTHORIZATION = 6005,

        // ====================================================================
        // Operational Errors (7000-7099)
        // ====================================================================
        QUERY_CANCELED = 7001,
        ADMIN_SHUTDOWN = 7002,
        CRASH_SHUTDOWN = 7003,
        DATABASE_DROPPED = 7004,
        OBJECT_IN_USE = 7005,
        LOCK_NOT_AVAILABLE = 7006,
    };

} // namespace scratchbird::core

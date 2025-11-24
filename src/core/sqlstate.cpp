#include "scratchbird/core/sqlstate.h"
#include <unordered_map>

namespace scratchbird::core
{

const char* statusToSQLState(Status status)
{
    // Map internal Status codes to SQLSTATE codes
    switch (status)
    {
        // ====================================================================
        // Success
        // ====================================================================
        case Status::OK:
            return SQLSTATE_SUCCESS;

        // ====================================================================
        // File/IO Errors -> Class 58 (System Error)
        // ====================================================================
        case Status::FILE_NOT_FOUND:
            return SQLSTATE_UNDEFINED_FILE;
        case Status::FILE_EXISTS:
            return SQLSTATE_DUPLICATE_FILE;
        case Status::IO_ERROR:
            return SQLSTATE_IO_ERROR;
        case Status::INVALID_PATH:
            return SQLSTATE_SYSTEM_ERROR;
        case Status::PERMISSION_DENIED:
            return SQLSTATE_INSUFFICIENT_PRIVILEGE;
        case Status::INVALID_ARGUMENT:
            return SQLSTATE_INVALID_PARAMETER_VALUE;

        // ====================================================================
        // Corruption Errors -> Class XX (Internal Error)
        // ====================================================================
        case Status::PAGE_CORRUPT:
            return SQLSTATE_DATA_CORRUPTED;
        case Status::CHECKSUM_MISMATCH:
            return SQLSTATE_DATA_CORRUPTED;
        case Status::INDEX_CORRUPTED:
            return SQLSTATE_INDEX_CORRUPTED;
        case Status::DATA_CORRUPTED:
            return SQLSTATE_DATA_CORRUPTED;

        // ====================================================================
        // Transaction/Lock Errors -> Class 40 (Transaction Rollback) & Class 25
        // ====================================================================
        case Status::DEADLOCK:
            return SQLSTATE_DEADLOCK_DETECTED;
        case Status::LOCK_TIMEOUT:
            return SQLSTATE_LOCK_NOT_AVAILABLE;
        case Status::LOCK_CONFLICT:
            return SQLSTATE_LOCK_NOT_AVAILABLE;
        case Status::SERIALIZATION_FAILURE:
            return SQLSTATE_SERIALIZATION_FAILURE;
        case Status::INVALID_TRANSACTION_STATE:
            return SQLSTATE_INVALID_TRANSACTION_STATE;
        case Status::NO_ACTIVE_TRANSACTION:
            return SQLSTATE_NO_ACTIVE_SQL_TRANSACTION;
        case Status::TRANSACTION_ABORTED:
            return SQLSTATE_IN_FAILED_SQL_TRANSACTION;
        case Status::READ_ONLY_TRANSACTION:
            return SQLSTATE_READ_ONLY_SQL_TRANSACTION;

        // ====================================================================
        // Resource Errors -> Class 53 (Insufficient Resources)
        // ====================================================================
        case Status::OOM:
            return SQLSTATE_OUT_OF_MEMORY;
        case Status::DISK_FULL:
            return SQLSTATE_DISK_FULL;
        case Status::TOO_MANY_CONNECTIONS:
            return SQLSTATE_TOO_MANY_CONNECTIONS;
        case Status::CONFIGURATION_LIMIT_EXCEEDED:
            return SQLSTATE_CONFIGURATION_LIMIT_EXCEEDED;

        // ====================================================================
        // Data Errors -> Class 22 (Data Exception)
        // ====================================================================
        case Status::PAGE_FULL:
            return SQLSTATE_DISK_FULL;
        case Status::NOT_FOUND:
            return SQLSTATE_NO_DATA;
        case Status::NOT_IMPLEMENTED:
            return SQLSTATE_FEATURE_NOT_SUPPORTED;
        case Status::NOT_SUPPORTED:
            return SQLSTATE_FEATURE_NOT_SUPPORTED;
        case Status::TYPE_MISMATCH:
            return SQLSTATE_DATATYPE_MISMATCH;
        case Status::OUT_OF_RANGE:
            return SQLSTATE_NUMERIC_VALUE_OUT_OF_RANGE;
        case Status::DIVISION_BY_ZERO:
            return SQLSTATE_DIVISION_BY_ZERO;
        case Status::NUMERIC_VALUE_OUT_OF_RANGE:
            return SQLSTATE_NUMERIC_VALUE_OUT_OF_RANGE;
        case Status::STRING_DATA_RIGHT_TRUNCATION:
            return SQLSTATE_STRING_DATA_RIGHT_TRUNCATION;
        case Status::DATETIME_FIELD_OVERFLOW:
            return SQLSTATE_DATETIME_FIELD_OVERFLOW;
        case Status::INVALID_DATETIME_FORMAT:
            return SQLSTATE_INVALID_DATETIME_FORMAT;
        case Status::INVALID_TEXT_REPRESENTATION:
            return SQLSTATE_INVALID_TEXT_REPRESENTATION;
        case Status::NULL_VALUE_NOT_ALLOWED:
            return SQLSTATE_NULL_VALUE_NOT_ALLOWED;

        // ====================================================================
        // Constraint Violations -> Class 23 (Integrity Constraint Violation)
        // ====================================================================
        case Status::CONSTRAINT_VIOLATION:
            return SQLSTATE_INTEGRITY_CONSTRAINT_VIOLATION;
        case Status::NOT_NULL_VIOLATION:
            return SQLSTATE_NOT_NULL_VIOLATION;
        case Status::FOREIGN_KEY_VIOLATION:
            return SQLSTATE_FOREIGN_KEY_VIOLATION;
        case Status::UNIQUE_VIOLATION:
            return SQLSTATE_UNIQUE_VIOLATION;
        case Status::CHECK_VIOLATION:
            return SQLSTATE_CHECK_VIOLATION;
        case Status::EXCLUSION_VIOLATION:
            return SQLSTATE_EXCLUSION_VIOLATION;

        // ====================================================================
        // Syntax/Semantic Errors -> Class 42 (Syntax Error or Access Rule Violation)
        // ====================================================================
        case Status::SYNTAX_ERROR:
            return SQLSTATE_SYNTAX_ERROR;
        case Status::UNDEFINED_TABLE:
            return SQLSTATE_UNDEFINED_TABLE;
        case Status::UNDEFINED_COLUMN:
            return SQLSTATE_UNDEFINED_COLUMN;
        case Status::UNDEFINED_FUNCTION:
            return SQLSTATE_UNDEFINED_FUNCTION;
        case Status::UNDEFINED_OBJECT:
            return SQLSTATE_UNDEFINED_OBJECT;
        case Status::DUPLICATE_TABLE:
            return SQLSTATE_DUPLICATE_TABLE;
        case Status::DUPLICATE_COLUMN:
            return SQLSTATE_DUPLICATE_COLUMN;
        case Status::DUPLICATE_OBJECT:
            return SQLSTATE_DUPLICATE_OBJECT;
        case Status::AMBIGUOUS_COLUMN:
            return SQLSTATE_AMBIGUOUS_COLUMN;
        case Status::AMBIGUOUS_FUNCTION:
            return SQLSTATE_AMBIGUOUS_FUNCTION;
        case Status::DATATYPE_MISMATCH:
            return SQLSTATE_DATATYPE_MISMATCH;
        case Status::WRONG_OBJECT_TYPE:
            return SQLSTATE_WRONG_OBJECT_TYPE;
        case Status::INSUFFICIENT_PRIVILEGE:
            return SQLSTATE_INSUFFICIENT_PRIVILEGE;

        // ====================================================================
        // Cursor Errors -> Class 24 (Invalid Cursor State)
        // ====================================================================
        case Status::INVALID_CURSOR_STATE:
            return SQLSTATE_INVALID_CURSOR_STATE;
        case Status::INVALID_CURSOR_NAME:
            return SQLSTATE_INVALID_CURSOR_NAME;
        case Status::CURSOR_NOT_FOUND:
            return SQLSTATE_INVALID_CURSOR_NAME; // No specific SQLSTATE for "not found"
        case Status::CURSOR_ALREADY_OPEN:
            return SQLSTATE_INVALID_CURSOR_STATE;
        case Status::CURSOR_NOT_OPEN:
            return SQLSTATE_INVALID_CURSOR_STATE;

        // ====================================================================
        // PL/pgSQL Errors -> Class P0 (PL/pgSQL Error)
        // ====================================================================
        case Status::NO_DATA_FOUND:
            return SQLSTATE_NO_DATA_FOUND;
        case Status::TOO_MANY_ROWS:
            return SQLSTATE_TOO_MANY_ROWS;
        case Status::RAISE_EXCEPTION:
            return SQLSTATE_RAISE_EXCEPTION;
        case Status::ASSERT_FAILURE:
            return SQLSTATE_ASSERT_FAILURE;

        // ====================================================================
        // Program Limit Errors -> Class 54 (Program Limit Exceeded)
        // ====================================================================
        case Status::STATEMENT_TOO_COMPLEX:
            return SQLSTATE_STATEMENT_TOO_COMPLEX;
        case Status::TOO_MANY_COLUMNS:
            return SQLSTATE_TOO_MANY_COLUMNS;

        // ====================================================================
        // Connection Errors -> Class 08 (Connection Exception)
        // ====================================================================
        case Status::CONNECTION_FAILURE:
            return SQLSTATE_CONNECTION_FAILURE;
        case Status::CONNECTION_DOES_NOT_EXIST:
            return SQLSTATE_CONNECTION_DOES_NOT_EXIST;
        case Status::PROTOCOL_VIOLATION:
            return SQLSTATE_PROTOCOL_VIOLATION;
        case Status::INVALID_PASSWORD:
            return SQLSTATE_INVALID_PASSWORD;
        case Status::INVALID_AUTHORIZATION:
            return SQLSTATE_INVALID_AUTHORIZATION_SPECIFICATION;

        // ====================================================================
        // Operational Errors -> Class 57 (Operator Intervention)
        // ====================================================================
        case Status::CANCELLED:
            return SQLSTATE_QUERY_CANCELED;
        case Status::QUERY_CANCELED:
            return SQLSTATE_QUERY_CANCELED;
        case Status::ADMIN_SHUTDOWN:
            return SQLSTATE_ADMIN_SHUTDOWN;
        case Status::CRASH_SHUTDOWN:
            return SQLSTATE_CRASH_SHUTDOWN;
        case Status::DATABASE_DROPPED:
            return SQLSTATE_DATABASE_DROPPED;
        case Status::OBJECT_IN_USE:
            return SQLSTATE_OBJECT_IN_USE;
        case Status::LOCK_NOT_AVAILABLE:
            return SQLSTATE_LOCK_NOT_AVAILABLE;

        // ====================================================================
        // Internal/Compression Errors -> Class XX (Internal Error)
        // ====================================================================
        case Status::COMPRESSION_ERROR:
            return SQLSTATE_INTERNAL_ERROR;
        case Status::INTERNAL_ERROR:
            return SQLSTATE_INTERNAL_ERROR;
        case Status::INDEX_NOT_FOUND:
            return SQLSTATE_UNDEFINED_OBJECT;

        default:
            return SQLSTATE_INTERNAL_ERROR;
    }
}

std::string_view getSQLStateClass(const char* sqlstate)
{
    if (!sqlstate || sqlstate[0] == '\0' || sqlstate[1] == '\0')
    {
        return "Unknown";
    }

    // Extract class (first 2 characters)
    char class_code[3] = {sqlstate[0], sqlstate[1], '\0'};

    // Map class codes to descriptions
    static const std::unordered_map<std::string_view, std::string_view> class_map = {
        {"00", "Successful Completion"},
        {"01", "Warning"},
        {"02", "No Data"},
        {"03", "SQL Statement Not Yet Complete"},
        {"08", "Connection Exception"},
        {"09", "Triggered Action Exception"},
        {"0A", "Feature Not Supported"},
        {"0B", "Invalid Transaction Initiation"},
        {"0F", "Locator Exception"},
        {"0L", "Invalid Grantor"},
        {"0P", "Invalid Role Specification"},
        {"0Z", "Diagnostics Exception"},
        {"20", "Case Not Found"},
        {"21", "Cardinality Violation"},
        {"22", "Data Exception"},
        {"23", "Integrity Constraint Violation"},
        {"24", "Invalid Cursor State"},
        {"25", "Invalid Transaction State"},
        {"26", "Invalid SQL Statement Name"},
        {"27", "Triggered Data Change Violation"},
        {"28", "Invalid Authorization Specification"},
        {"2B", "Dependent Privilege Descriptors Still Exist"},
        {"2D", "Invalid Transaction Termination"},
        {"2F", "SQL Routine Exception"},
        {"34", "Invalid Cursor Name"},
        {"38", "External Routine Exception"},
        {"39", "External Routine Invocation Exception"},
        {"3B", "Savepoint Exception"},
        {"3D", "Invalid Catalog Name"},
        {"3F", "Invalid Schema Name"},
        {"40", "Transaction Rollback"},
        {"42", "Syntax Error or Access Rule Violation"},
        {"44", "WITH CHECK OPTION Violation"},
        {"53", "Insufficient Resources"},
        {"54", "Program Limit Exceeded"},
        {"55", "Object Not In Prerequisite State"},
        {"57", "Operator Intervention"},
        {"58", "System Error"},
        {"F0", "Configuration File Error"},
        {"HV", "Foreign Data Wrapper Error"},
        {"P0", "PL/pgSQL Error"},
        {"XX", "Internal Error"}
    };

    auto it = class_map.find(class_code);
    if (it != class_map.end())
    {
        return it->second;
    }

    return "Unknown Error Class";
}

} // namespace scratchbird::core

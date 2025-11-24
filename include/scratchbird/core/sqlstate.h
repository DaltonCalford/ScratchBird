#pragma once

#include <string>
#include <string_view>
#include "scratchbird/core/status.h"

namespace scratchbird::core
{

/**
 * @file sqlstate.h
 * @brief PostgreSQL-compatible SQLSTATE error codes
 *
 * SQLSTATE is a 5-character error code defined by the SQL standard.
 * Each code consists of a 2-character class and 3-character subclass.
 *
 * Class XX: Internal errors (ScratchBird-specific)
 * Classes 00-HZ: SQL standard error codes
 *
 * References:
 * - PostgreSQL: https://www.postgresql.org/docs/current/errcodes-appendix.html
 * - SQL:2016 standard
 */

// SQLSTATE is a fixed 5-character code
using SQLState = const char[6];

// ============================================================================
// CLASS 00 - Successful Completion
// ============================================================================
constexpr SQLState SQLSTATE_SUCCESS = "00000"; // Successful completion

// ============================================================================
// CLASS 01 - Warning
// ============================================================================
constexpr SQLState SQLSTATE_WARNING = "01000"; // Warning
constexpr SQLState SQLSTATE_DYNAMIC_RESULT_SETS_RETURNED = "0100C"; // Dynamic result sets returned
constexpr SQLState SQLSTATE_IMPLICIT_ZERO_BIT_PADDING = "01008"; // Implicit zero bit padding
constexpr SQLState SQLSTATE_NULL_VALUE_ELIMINATED_IN_SET_FUNCTION = "01003"; // NULL value eliminated in set function
constexpr SQLState SQLSTATE_PRIVILEGE_NOT_GRANTED = "01007"; // Privilege not granted
constexpr SQLState SQLSTATE_PRIVILEGE_NOT_REVOKED = "01006"; // Privilege not revoked
constexpr SQLState SQLSTATE_STRING_DATA_RIGHT_TRUNCATION_WARNING = "01004"; // String data right truncation
constexpr SQLState SQLSTATE_DEPRECATED_FEATURE = "01P01"; // Deprecated feature

// ============================================================================
// CLASS 02 - No Data
// ============================================================================
constexpr SQLState SQLSTATE_NO_DATA = "02000"; // No data
constexpr SQLState SQLSTATE_NO_ADDITIONAL_DYNAMIC_RESULT_SETS_RETURNED = "02001"; // No additional dynamic result sets returned

// ============================================================================
// CLASS 03 - SQL Statement Not Yet Complete
// ============================================================================
constexpr SQLState SQLSTATE_SQL_STATEMENT_NOT_YET_COMPLETE = "03000"; // SQL statement not yet complete

// ============================================================================
// CLASS 08 - Connection Exception
// ============================================================================
constexpr SQLState SQLSTATE_CONNECTION_EXCEPTION = "08000"; // Connection exception
constexpr SQLState SQLSTATE_CONNECTION_DOES_NOT_EXIST = "08003"; // Connection does not exist
constexpr SQLState SQLSTATE_CONNECTION_FAILURE = "08006"; // Connection failure
constexpr SQLState SQLSTATE_SQLCLIENT_UNABLE_TO_ESTABLISH_SQLCONNECTION = "08001"; // Client unable to establish connection
constexpr SQLState SQLSTATE_SQLSERVER_REJECTED_ESTABLISHMENT_OF_SQLCONNECTION = "08004"; // Server rejected connection
constexpr SQLState SQLSTATE_TRANSACTION_RESOLUTION_UNKNOWN = "08007"; // Transaction resolution unknown
constexpr SQLState SQLSTATE_PROTOCOL_VIOLATION = "08P01"; // Protocol violation

// ============================================================================
// CLASS 09 - Triggered Action Exception
// ============================================================================
constexpr SQLState SQLSTATE_TRIGGERED_ACTION_EXCEPTION = "09000"; // Triggered action exception

// ============================================================================
// CLASS 0A - Feature Not Supported
// ============================================================================
constexpr SQLState SQLSTATE_FEATURE_NOT_SUPPORTED = "0A000"; // Feature not supported

// ============================================================================
// CLASS 0B - Invalid Transaction Initiation
// ============================================================================
constexpr SQLState SQLSTATE_INVALID_TRANSACTION_INITIATION = "0B000"; // Invalid transaction initiation

// ============================================================================
// CLASS 0F - Locator Exception
// ============================================================================
constexpr SQLState SQLSTATE_LOCATOR_EXCEPTION = "0F000"; // Locator exception
constexpr SQLState SQLSTATE_INVALID_LOCATOR_SPECIFICATION = "0F001"; // Invalid locator specification

// ============================================================================
// CLASS 0L - Invalid Grantor
// ============================================================================
constexpr SQLState SQLSTATE_INVALID_GRANTOR = "0L000"; // Invalid grantor
constexpr SQLState SQLSTATE_INVALID_GRANT_OPERATION = "0LP01"; // Invalid grant operation

// ============================================================================
// CLASS 0P - Invalid Role Specification
// ============================================================================
constexpr SQLState SQLSTATE_INVALID_ROLE_SPECIFICATION = "0P000"; // Invalid role specification

// ============================================================================
// CLASS 0Z - Diagnostics Exception
// ============================================================================
constexpr SQLState SQLSTATE_DIAGNOSTICS_EXCEPTION = "0Z000"; // Diagnostics exception
constexpr SQLState SQLSTATE_STACKED_DIAGNOSTICS_ACCESSED_WITHOUT_ACTIVE_HANDLER = "0Z002"; // Stacked diagnostics accessed without active handler

// ============================================================================
// CLASS 20 - Case Not Found
// ============================================================================
constexpr SQLState SQLSTATE_CASE_NOT_FOUND = "20000"; // Case not found

// ============================================================================
// CLASS 21 - Cardinality Violation
// ============================================================================
constexpr SQLState SQLSTATE_CARDINALITY_VIOLATION = "21000"; // Cardinality violation

// ============================================================================
// CLASS 22 - Data Exception
// ============================================================================
constexpr SQLState SQLSTATE_DATA_EXCEPTION = "22000"; // Data exception
constexpr SQLState SQLSTATE_ARRAY_SUBSCRIPT_ERROR = "2202E"; // Array subscript error
constexpr SQLState SQLSTATE_CHARACTER_NOT_IN_REPERTOIRE = "22021"; // Character not in repertoire
constexpr SQLState SQLSTATE_DATETIME_FIELD_OVERFLOW = "22008"; // Datetime field overflow
constexpr SQLState SQLSTATE_DIVISION_BY_ZERO = "22012"; // Division by zero
constexpr SQLState SQLSTATE_ERROR_IN_ASSIGNMENT = "22005"; // Error in assignment
constexpr SQLState SQLSTATE_ESCAPE_CHARACTER_CONFLICT = "2200B"; // Escape character conflict
constexpr SQLState SQLSTATE_INDICATOR_OVERFLOW = "22022"; // Indicator overflow
constexpr SQLState SQLSTATE_INTERVAL_FIELD_OVERFLOW = "22015"; // Interval field overflow
constexpr SQLState SQLSTATE_INVALID_ARGUMENT_FOR_LOGARITHM = "2201E"; // Invalid argument for logarithm
constexpr SQLState SQLSTATE_INVALID_ARGUMENT_FOR_NTILE_FUNCTION = "22014"; // Invalid argument for NTILE function
constexpr SQLState SQLSTATE_INVALID_ARGUMENT_FOR_NTH_VALUE_FUNCTION = "22016"; // Invalid argument for NTH_VALUE function
constexpr SQLState SQLSTATE_INVALID_ARGUMENT_FOR_POWER_FUNCTION = "2201F"; // Invalid argument for power function
constexpr SQLState SQLSTATE_INVALID_ARGUMENT_FOR_WIDTH_BUCKET_FUNCTION = "2201G"; // Invalid argument for width bucket function
constexpr SQLState SQLSTATE_INVALID_CHARACTER_VALUE_FOR_CAST = "22018"; // Invalid character value for cast
constexpr SQLState SQLSTATE_INVALID_DATETIME_FORMAT = "22007"; // Invalid datetime format
constexpr SQLState SQLSTATE_INVALID_ESCAPE_CHARACTER = "22019"; // Invalid escape character
constexpr SQLState SQLSTATE_INVALID_ESCAPE_OCTET = "2200D"; // Invalid escape octet
constexpr SQLState SQLSTATE_INVALID_ESCAPE_SEQUENCE = "22025"; // Invalid escape sequence
constexpr SQLState SQLSTATE_NONSTANDARD_USE_OF_ESCAPE_CHARACTER = "22P06"; // Nonstandard use of escape character
constexpr SQLState SQLSTATE_INVALID_INDICATOR_PARAMETER_VALUE = "22010"; // Invalid indicator parameter value
constexpr SQLState SQLSTATE_INVALID_PARAMETER_VALUE = "22023"; // Invalid parameter value
constexpr SQLState SQLSTATE_INVALID_REGULAR_EXPRESSION = "2201B"; // Invalid regular expression
constexpr SQLState SQLSTATE_INVALID_ROW_COUNT_IN_LIMIT_CLAUSE = "2201W"; // Invalid row count in LIMIT clause
constexpr SQLState SQLSTATE_INVALID_ROW_COUNT_IN_RESULT_OFFSET_CLAUSE = "2201X"; // Invalid row count in OFFSET clause
constexpr SQLState SQLSTATE_INVALID_TABLESAMPLE_ARGUMENT = "2202H"; // Invalid tablesample argument
constexpr SQLState SQLSTATE_INVALID_TABLESAMPLE_REPEAT = "2202G"; // Invalid tablesample repeat
constexpr SQLState SQLSTATE_INVALID_TIME_ZONE_DISPLACEMENT_VALUE = "22009"; // Invalid time zone displacement value
constexpr SQLState SQLSTATE_INVALID_USE_OF_ESCAPE_CHARACTER = "2200C"; // Invalid use of escape character
constexpr SQLState SQLSTATE_MOST_SPECIFIC_TYPE_MISMATCH = "2200G"; // Most specific type mismatch
constexpr SQLState SQLSTATE_NULL_VALUE_NOT_ALLOWED = "22004"; // NULL value not allowed
constexpr SQLState SQLSTATE_NULL_VALUE_NO_INDICATOR_PARAMETER = "22002"; // NULL value no indicator parameter
constexpr SQLState SQLSTATE_NUMERIC_VALUE_OUT_OF_RANGE = "22003"; // Numeric value out of range
constexpr SQLState SQLSTATE_STRING_DATA_LENGTH_MISMATCH = "22026"; // String data length mismatch
constexpr SQLState SQLSTATE_STRING_DATA_RIGHT_TRUNCATION = "22001"; // String data right truncation
constexpr SQLState SQLSTATE_SUBSTRING_ERROR = "22011"; // Substring error
constexpr SQLState SQLSTATE_TRIM_ERROR = "22027"; // Trim error
constexpr SQLState SQLSTATE_UNTERMINATED_C_STRING = "22024"; // Unterminated C string
constexpr SQLState SQLSTATE_ZERO_LENGTH_CHARACTER_STRING = "2200F"; // Zero length character string
constexpr SQLState SQLSTATE_FLOATING_POINT_EXCEPTION = "22P01"; // Floating point exception
constexpr SQLState SQLSTATE_INVALID_TEXT_REPRESENTATION = "22P02"; // Invalid text representation
constexpr SQLState SQLSTATE_INVALID_BINARY_REPRESENTATION = "22P03"; // Invalid binary representation
constexpr SQLState SQLSTATE_BAD_COPY_FILE_FORMAT = "22P04"; // Bad copy file format
constexpr SQLState SQLSTATE_UNTRANSLATABLE_CHARACTER = "22P05"; // Untranslatable character
constexpr SQLState SQLSTATE_NOT_AN_XML_DOCUMENT = "2200L"; // Not an XML document
constexpr SQLState SQLSTATE_INVALID_XML_DOCUMENT = "2200M"; // Invalid XML document
constexpr SQLState SQLSTATE_INVALID_XML_CONTENT = "2200N"; // Invalid XML content
constexpr SQLState SQLSTATE_INVALID_XML_COMMENT = "2200S"; // Invalid XML comment
constexpr SQLState SQLSTATE_INVALID_XML_PROCESSING_INSTRUCTION = "2200T"; // Invalid XML processing instruction

// ============================================================================
// CLASS 23 - Integrity Constraint Violation
// ============================================================================
constexpr SQLState SQLSTATE_INTEGRITY_CONSTRAINT_VIOLATION = "23000"; // Integrity constraint violation
constexpr SQLState SQLSTATE_RESTRICT_VIOLATION = "23001"; // Restrict violation
constexpr SQLState SQLSTATE_NOT_NULL_VIOLATION = "23502"; // Not null violation
constexpr SQLState SQLSTATE_FOREIGN_KEY_VIOLATION = "23503"; // Foreign key violation
constexpr SQLState SQLSTATE_UNIQUE_VIOLATION = "23505"; // Unique violation
constexpr SQLState SQLSTATE_CHECK_VIOLATION = "23514"; // Check constraint violation
constexpr SQLState SQLSTATE_EXCLUSION_VIOLATION = "23P01"; // Exclusion constraint violation

// ============================================================================
// CLASS 24 - Invalid Cursor State
// ============================================================================
constexpr SQLState SQLSTATE_INVALID_CURSOR_STATE = "24000"; // Invalid cursor state

// ============================================================================
// CLASS 25 - Invalid Transaction State
// ============================================================================
constexpr SQLState SQLSTATE_INVALID_TRANSACTION_STATE = "25000"; // Invalid transaction state
constexpr SQLState SQLSTATE_ACTIVE_SQL_TRANSACTION = "25001"; // Active SQL transaction
constexpr SQLState SQLSTATE_BRANCH_TRANSACTION_ALREADY_ACTIVE = "25002"; // Branch transaction already active
constexpr SQLState SQLSTATE_HELD_CURSOR_REQUIRES_SAME_ISOLATION_LEVEL = "25008"; // Held cursor requires same isolation level
constexpr SQLState SQLSTATE_INAPPROPRIATE_ACCESS_MODE_FOR_BRANCH_TRANSACTION = "25003"; // Inappropriate access mode for branch transaction
constexpr SQLState SQLSTATE_INAPPROPRIATE_ISOLATION_LEVEL_FOR_BRANCH_TRANSACTION = "25004"; // Inappropriate isolation level for branch transaction
constexpr SQLState SQLSTATE_NO_ACTIVE_SQL_TRANSACTION_FOR_BRANCH_TRANSACTION = "25005"; // No active SQL transaction for branch transaction
constexpr SQLState SQLSTATE_READ_ONLY_SQL_TRANSACTION = "25006"; // Read-only SQL transaction
constexpr SQLState SQLSTATE_SCHEMA_AND_DATA_STATEMENT_MIXING_NOT_SUPPORTED = "25007"; // Schema and data statement mixing not supported
constexpr SQLState SQLSTATE_NO_ACTIVE_SQL_TRANSACTION = "25P01"; // No active SQL transaction
constexpr SQLState SQLSTATE_IN_FAILED_SQL_TRANSACTION = "25P02"; // In failed SQL transaction

// ============================================================================
// CLASS 26 - Invalid SQL Statement Name
// ============================================================================
constexpr SQLState SQLSTATE_INVALID_SQL_STATEMENT_NAME = "26000"; // Invalid SQL statement name

// ============================================================================
// CLASS 27 - Triggered Data Change Violation
// ============================================================================
constexpr SQLState SQLSTATE_TRIGGERED_DATA_CHANGE_VIOLATION = "27000"; // Triggered data change violation

// ============================================================================
// CLASS 28 - Invalid Authorization Specification
// ============================================================================
constexpr SQLState SQLSTATE_INVALID_AUTHORIZATION_SPECIFICATION = "28000"; // Invalid authorization specification
constexpr SQLState SQLSTATE_INVALID_PASSWORD = "28P01"; // Invalid password

// ============================================================================
// CLASS 2B - Dependent Privilege Descriptors Still Exist
// ============================================================================
constexpr SQLState SQLSTATE_DEPENDENT_PRIVILEGE_DESCRIPTORS_STILL_EXIST = "2B000"; // Dependent privilege descriptors still exist
constexpr SQLState SQLSTATE_DEPENDENT_OBJECTS_STILL_EXIST = "2BP01"; // Dependent objects still exist

// ============================================================================
// CLASS 2D - Invalid Transaction Termination
// ============================================================================
constexpr SQLState SQLSTATE_INVALID_TRANSACTION_TERMINATION = "2D000"; // Invalid transaction termination

// ============================================================================
// CLASS 2F - SQL Routine Exception
// ============================================================================
constexpr SQLState SQLSTATE_SQL_ROUTINE_EXCEPTION = "2F000"; // SQL routine exception
constexpr SQLState SQLSTATE_FUNCTION_EXECUTED_NO_RETURN_STATEMENT = "2F005"; // Function executed no return statement
constexpr SQLState SQLSTATE_MODIFYING_SQL_DATA_NOT_PERMITTED = "2F002"; // Modifying SQL data not permitted
constexpr SQLState SQLSTATE_PROHIBITED_SQL_STATEMENT_ATTEMPTED = "2F003"; // Prohibited SQL statement attempted
constexpr SQLState SQLSTATE_READING_SQL_DATA_NOT_PERMITTED = "2F004"; // Reading SQL data not permitted

// ============================================================================
// CLASS 34 - Invalid Cursor Name
// ============================================================================
constexpr SQLState SQLSTATE_INVALID_CURSOR_NAME = "34000"; // Invalid cursor name

// ============================================================================
// CLASS 38 - External Routine Exception
// ============================================================================
constexpr SQLState SQLSTATE_EXTERNAL_ROUTINE_EXCEPTION = "38000"; // External routine exception
constexpr SQLState SQLSTATE_CONTAINING_SQL_NOT_PERMITTED = "38001"; // Containing SQL not permitted
constexpr SQLState SQLSTATE_MODIFYING_SQL_DATA_NOT_PERMITTED_EXTERNAL = "38002"; // Modifying SQL data not permitted
constexpr SQLState SQLSTATE_PROHIBITED_SQL_STATEMENT_ATTEMPTED_EXTERNAL = "38003"; // Prohibited SQL statement attempted
constexpr SQLState SQLSTATE_READING_SQL_DATA_NOT_PERMITTED_EXTERNAL = "38004"; // Reading SQL data not permitted

// ============================================================================
// CLASS 39 - External Routine Invocation Exception
// ============================================================================
constexpr SQLState SQLSTATE_EXTERNAL_ROUTINE_INVOCATION_EXCEPTION = "39000"; // External routine invocation exception
constexpr SQLState SQLSTATE_INVALID_SQLSTATE_RETURNED = "39001"; // Invalid SQLSTATE returned
constexpr SQLState SQLSTATE_NULL_VALUE_NOT_ALLOWED_EXTERNAL = "39004"; // NULL value not allowed
constexpr SQLState SQLSTATE_TRIGGER_PROTOCOL_VIOLATED = "39P01"; // Trigger protocol violated
constexpr SQLState SQLSTATE_SRF_PROTOCOL_VIOLATED = "39P02"; // SRF protocol violated
constexpr SQLState SQLSTATE_EVENT_TRIGGER_PROTOCOL_VIOLATED = "39P03"; // Event trigger protocol violated

// ============================================================================
// CLASS 3B - Savepoint Exception
// ============================================================================
constexpr SQLState SQLSTATE_SAVEPOINT_EXCEPTION = "3B000"; // Savepoint exception
constexpr SQLState SQLSTATE_INVALID_SAVEPOINT_SPECIFICATION = "3B001"; // Invalid savepoint specification

// ============================================================================
// CLASS 3D - Invalid Catalog Name
// ============================================================================
constexpr SQLState SQLSTATE_INVALID_CATALOG_NAME = "3D000"; // Invalid catalog name

// ============================================================================
// CLASS 3F - Invalid Schema Name
// ============================================================================
constexpr SQLState SQLSTATE_INVALID_SCHEMA_NAME = "3F000"; // Invalid schema name

// ============================================================================
// CLASS 40 - Transaction Rollback
// ============================================================================
constexpr SQLState SQLSTATE_TRANSACTION_ROLLBACK = "40000"; // Transaction rollback
constexpr SQLState SQLSTATE_TRANSACTION_INTEGRITY_CONSTRAINT_VIOLATION = "40002"; // Transaction integrity constraint violation
constexpr SQLState SQLSTATE_SERIALIZATION_FAILURE = "40001"; // Serialization failure
constexpr SQLState SQLSTATE_STATEMENT_COMPLETION_UNKNOWN = "40003"; // Statement completion unknown
constexpr SQLState SQLSTATE_DEADLOCK_DETECTED = "40P01"; // Deadlock detected

// ============================================================================
// CLASS 42 - Syntax Error or Access Rule Violation
// ============================================================================
constexpr SQLState SQLSTATE_SYNTAX_ERROR_OR_ACCESS_RULE_VIOLATION = "42000"; // Syntax error or access rule violation
constexpr SQLState SQLSTATE_SYNTAX_ERROR = "42601"; // Syntax error
constexpr SQLState SQLSTATE_INSUFFICIENT_PRIVILEGE = "42501"; // Insufficient privilege
constexpr SQLState SQLSTATE_CANNOT_COERCE = "42846"; // Cannot coerce
constexpr SQLState SQLSTATE_GROUPING_ERROR = "42803"; // Grouping error
constexpr SQLState SQLSTATE_WINDOWING_ERROR = "42P20"; // Windowing error
constexpr SQLState SQLSTATE_INVALID_RECURSION = "42P19"; // Invalid recursion
constexpr SQLState SQLSTATE_INVALID_FOREIGN_KEY = "42830"; // Invalid foreign key
constexpr SQLState SQLSTATE_INVALID_NAME = "42602"; // Invalid name
constexpr SQLState SQLSTATE_NAME_TOO_LONG = "42622"; // Name too long
constexpr SQLState SQLSTATE_RESERVED_NAME = "42939"; // Reserved name
constexpr SQLState SQLSTATE_DATATYPE_MISMATCH = "42804"; // Datatype mismatch
constexpr SQLState SQLSTATE_INDETERMINATE_DATATYPE = "42P18"; // Indeterminate datatype
constexpr SQLState SQLSTATE_COLLATION_MISMATCH = "42P21"; // Collation mismatch
constexpr SQLState SQLSTATE_INDETERMINATE_COLLATION = "42P22"; // Indeterminate collation
constexpr SQLState SQLSTATE_WRONG_OBJECT_TYPE = "42809"; // Wrong object type
constexpr SQLState SQLSTATE_UNDEFINED_COLUMN = "42703"; // Undefined column
constexpr SQLState SQLSTATE_UNDEFINED_FUNCTION = "42883"; // Undefined function
constexpr SQLState SQLSTATE_UNDEFINED_TABLE = "42P01"; // Undefined table
constexpr SQLState SQLSTATE_UNDEFINED_PARAMETER = "42P02"; // Undefined parameter
constexpr SQLState SQLSTATE_UNDEFINED_OBJECT = "42704"; // Undefined object
constexpr SQLState SQLSTATE_DUPLICATE_COLUMN = "42701"; // Duplicate column
constexpr SQLState SQLSTATE_DUPLICATE_CURSOR = "42P03"; // Duplicate cursor
constexpr SQLState SQLSTATE_DUPLICATE_DATABASE = "42P04"; // Duplicate database
constexpr SQLState SQLSTATE_DUPLICATE_FUNCTION = "42723"; // Duplicate function
constexpr SQLState SQLSTATE_DUPLICATE_PREPARED_STATEMENT = "42P05"; // Duplicate prepared statement
constexpr SQLState SQLSTATE_DUPLICATE_SCHEMA = "42P06"; // Duplicate schema
constexpr SQLState SQLSTATE_DUPLICATE_TABLE = "42P07"; // Duplicate table
constexpr SQLState SQLSTATE_DUPLICATE_ALIAS = "42712"; // Duplicate alias
constexpr SQLState SQLSTATE_DUPLICATE_OBJECT = "42710"; // Duplicate object
constexpr SQLState SQLSTATE_AMBIGUOUS_COLUMN = "42702"; // Ambiguous column
constexpr SQLState SQLSTATE_AMBIGUOUS_FUNCTION = "42725"; // Ambiguous function
constexpr SQLState SQLSTATE_AMBIGUOUS_PARAMETER = "42P08"; // Ambiguous parameter
constexpr SQLState SQLSTATE_AMBIGUOUS_ALIAS = "42P09"; // Ambiguous alias
constexpr SQLState SQLSTATE_INVALID_COLUMN_REFERENCE = "42P10"; // Invalid column reference
constexpr SQLState SQLSTATE_INVALID_COLUMN_DEFINITION = "42611"; // Invalid column definition
constexpr SQLState SQLSTATE_INVALID_CURSOR_DEFINITION = "42P11"; // Invalid cursor definition
constexpr SQLState SQLSTATE_INVALID_DATABASE_DEFINITION = "42P12"; // Invalid database definition
constexpr SQLState SQLSTATE_INVALID_FUNCTION_DEFINITION = "42P13"; // Invalid function definition
constexpr SQLState SQLSTATE_INVALID_PREPARED_STATEMENT_DEFINITION = "42P14"; // Invalid prepared statement definition
constexpr SQLState SQLSTATE_INVALID_SCHEMA_DEFINITION = "42P15"; // Invalid schema definition
constexpr SQLState SQLSTATE_INVALID_TABLE_DEFINITION = "42P16"; // Invalid table definition
constexpr SQLState SQLSTATE_INVALID_OBJECT_DEFINITION = "42P17"; // Invalid object definition

// ============================================================================
// CLASS 44 - WITH CHECK OPTION Violation
// ============================================================================
constexpr SQLState SQLSTATE_WITH_CHECK_OPTION_VIOLATION = "44000"; // WITH CHECK OPTION violation

// ============================================================================
// CLASS 53 - Insufficient Resources
// ============================================================================
constexpr SQLState SQLSTATE_INSUFFICIENT_RESOURCES = "53000"; // Insufficient resources
constexpr SQLState SQLSTATE_DISK_FULL = "53100"; // Disk full
constexpr SQLState SQLSTATE_OUT_OF_MEMORY = "53200"; // Out of memory
constexpr SQLState SQLSTATE_TOO_MANY_CONNECTIONS = "53300"; // Too many connections
constexpr SQLState SQLSTATE_CONFIGURATION_LIMIT_EXCEEDED = "53400"; // Configuration limit exceeded

// ============================================================================
// CLASS 54 - Program Limit Exceeded
// ============================================================================
constexpr SQLState SQLSTATE_PROGRAM_LIMIT_EXCEEDED = "54000"; // Program limit exceeded
constexpr SQLState SQLSTATE_STATEMENT_TOO_COMPLEX = "54001"; // Statement too complex
constexpr SQLState SQLSTATE_TOO_MANY_COLUMNS = "54011"; // Too many columns
constexpr SQLState SQLSTATE_TOO_MANY_ARGUMENTS = "54023"; // Too many arguments

// ============================================================================
// CLASS 55 - Object Not In Prerequisite State
// ============================================================================
constexpr SQLState SQLSTATE_OBJECT_NOT_IN_PREREQUISITE_STATE = "55000"; // Object not in prerequisite state
constexpr SQLState SQLSTATE_OBJECT_IN_USE = "55006"; // Object in use
constexpr SQLState SQLSTATE_CANT_CHANGE_RUNTIME_PARAM = "55P02"; // Can't change runtime parameter
constexpr SQLState SQLSTATE_LOCK_NOT_AVAILABLE = "55P03"; // Lock not available

// ============================================================================
// CLASS 57 - Operator Intervention
// ============================================================================
constexpr SQLState SQLSTATE_OPERATOR_INTERVENTION = "57000"; // Operator intervention
constexpr SQLState SQLSTATE_QUERY_CANCELED = "57014"; // Query canceled
constexpr SQLState SQLSTATE_ADMIN_SHUTDOWN = "57P01"; // Admin shutdown
constexpr SQLState SQLSTATE_CRASH_SHUTDOWN = "57P02"; // Crash shutdown
constexpr SQLState SQLSTATE_CANNOT_CONNECT_NOW = "57P03"; // Cannot connect now
constexpr SQLState SQLSTATE_DATABASE_DROPPED = "57P04"; // Database dropped

// ============================================================================
// CLASS 58 - System Error (errors external to PostgreSQL/ScratchBird)
// ============================================================================
constexpr SQLState SQLSTATE_SYSTEM_ERROR = "58000"; // System error
constexpr SQLState SQLSTATE_IO_ERROR = "58030"; // IO error
constexpr SQLState SQLSTATE_UNDEFINED_FILE = "58P01"; // Undefined file
constexpr SQLState SQLSTATE_DUPLICATE_FILE = "58P02"; // Duplicate file

// ============================================================================
// CLASS F0 - Configuration File Error
// ============================================================================
constexpr SQLState SQLSTATE_CONFIG_FILE_ERROR = "F0000"; // Configuration file error
constexpr SQLState SQLSTATE_LOCK_FILE_EXISTS = "F0001"; // Lock file exists

// ============================================================================
// CLASS HV - Foreign Data Wrapper Error (SQL/MED)
// ============================================================================
constexpr SQLState SQLSTATE_FDW_ERROR = "HV000"; // Foreign data wrapper error
constexpr SQLState SQLSTATE_FDW_COLUMN_NAME_NOT_FOUND = "HV005"; // Column name not found
constexpr SQLState SQLSTATE_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED = "HV002"; // Dynamic parameter value needed
constexpr SQLState SQLSTATE_FDW_FUNCTION_SEQUENCE_ERROR = "HV010"; // Function sequence error
constexpr SQLState SQLSTATE_FDW_INCONSISTENT_DESCRIPTOR_INFORMATION = "HV021"; // Inconsistent descriptor information
constexpr SQLState SQLSTATE_FDW_INVALID_ATTRIBUTE_VALUE = "HV024"; // Invalid attribute value
constexpr SQLState SQLSTATE_FDW_INVALID_COLUMN_NAME = "HV007"; // Invalid column name
constexpr SQLState SQLSTATE_FDW_INVALID_COLUMN_NUMBER = "HV008"; // Invalid column number
constexpr SQLState SQLSTATE_FDW_INVALID_DATA_TYPE = "HV004"; // Invalid data type
constexpr SQLState SQLSTATE_FDW_INVALID_DATA_TYPE_DESCRIPTORS = "HV006"; // Invalid data type descriptors
constexpr SQLState SQLSTATE_FDW_INVALID_DESCRIPTOR_FIELD_IDENTIFIER = "HV091"; // Invalid descriptor field identifier
constexpr SQLState SQLSTATE_FDW_INVALID_HANDLE = "HV00B"; // Invalid handle
constexpr SQLState SQLSTATE_FDW_INVALID_OPTION_INDEX = "HV00C"; // Invalid option index
constexpr SQLState SQLSTATE_FDW_INVALID_OPTION_NAME = "HV00D"; // Invalid option name
constexpr SQLState SQLSTATE_FDW_INVALID_STRING_LENGTH_OR_BUFFER_LENGTH = "HV090"; // Invalid string length or buffer length
constexpr SQLState SQLSTATE_FDW_INVALID_STRING_FORMAT = "HV00A"; // Invalid string format
constexpr SQLState SQLSTATE_FDW_INVALID_USE_OF_NULL_POINTER = "HV009"; // Invalid use of null pointer
constexpr SQLState SQLSTATE_FDW_TOO_MANY_HANDLES = "HV014"; // Too many handles
constexpr SQLState SQLSTATE_FDW_OUT_OF_MEMORY = "HV001"; // Out of memory
constexpr SQLState SQLSTATE_FDW_NO_SCHEMAS = "HV00P"; // No schemas
constexpr SQLState SQLSTATE_FDW_OPTION_NAME_NOT_FOUND = "HV00J"; // Option name not found
constexpr SQLState SQLSTATE_FDW_REPLY_HANDLE = "HV00K"; // Reply handle
constexpr SQLState SQLSTATE_FDW_SCHEMA_NOT_FOUND = "HV00Q"; // Schema not found
constexpr SQLState SQLSTATE_FDW_TABLE_NOT_FOUND = "HV00R"; // Table not found
constexpr SQLState SQLSTATE_FDW_UNABLE_TO_CREATE_EXECUTION = "HV00L"; // Unable to create execution
constexpr SQLState SQLSTATE_FDW_UNABLE_TO_CREATE_REPLY = "HV00M"; // Unable to create reply
constexpr SQLState SQLSTATE_FDW_UNABLE_TO_ESTABLISH_CONNECTION = "HV00N"; // Unable to establish connection

// ============================================================================
// CLASS P0 - PL/pgSQL Error
// ============================================================================
constexpr SQLState SQLSTATE_PLPGSQL_ERROR = "P0000"; // PL/pgSQL error
constexpr SQLState SQLSTATE_RAISE_EXCEPTION = "P0001"; // Raise exception
constexpr SQLState SQLSTATE_NO_DATA_FOUND = "P0002"; // No data found (PL/pgSQL)
constexpr SQLState SQLSTATE_TOO_MANY_ROWS = "P0003"; // Too many rows (PL/pgSQL)
constexpr SQLState SQLSTATE_ASSERT_FAILURE = "P0004"; // Assert failure

// ============================================================================
// CLASS XX - Internal Error (ScratchBird-specific)
// ============================================================================
constexpr SQLState SQLSTATE_INTERNAL_ERROR = "XX000"; // Internal error
constexpr SQLState SQLSTATE_DATA_CORRUPTED = "XX001"; // Data corrupted
constexpr SQLState SQLSTATE_INDEX_CORRUPTED = "XX002"; // Index corrupted

/**
 * @brief Map a Status code to the corresponding SQLSTATE
 * @param status The internal status code
 * @return SQLSTATE string (5 characters + null terminator)
 */
const char* statusToSQLState(Status status);

/**
 * @brief Get human-readable error class description
 * @param sqlstate SQLSTATE code (5 characters)
 * @return Description of the error class
 */
std::string_view getSQLStateClass(const char* sqlstate);

} // namespace scratchbird::core

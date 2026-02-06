/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <cstdint>
#include <cstddef>

namespace scratchbird
{
    namespace sblr
    {

        // SBLR (ScratchBird Language Representation) Opcodes
        // Based on Firebird's BLR (Binary Language Representation)

        enum class Opcode : uint16_t
        {
            // Control flow
            END = 0x00,     // End of bytecode stream
            VERSION = 0x01, // Version marker (followed by version byte)

            // Statements
            CREATE_TABLE = 0x10,              // Create table
            CREATE_INDEX = 0x1B,              // Create index (Phase 2 Task 2.3)
            DROP_TABLE = 0x1F,                // Drop table (ALPHA Phase 1 - DDL Modifications)
            DROP_INDEX = 0x20,                // Drop index (ALPHA Phase 1 - DDL Modifications)
            ALTER_TABLE = 0x21,               // Alter table (ALPHA Phase 1 - DDL Modifications)
            TRUNCATE_TABLE = 0x22,            // Truncate table (ALPHA Phase 1 - DDL Modifications - final)
            CREATE_SEQUENCE = 0x23,           // Create sequence (ALPHA Phase 1 - Sequences)
            ALTER_SEQUENCE = 0x24,            // Alter sequence (ALPHA Phase 1 - Sequences)
            DROP_SEQUENCE = 0x25,             // Drop sequence (ALPHA Phase 1 - Sequences)
            SEQUENCE_NEXTVAL = 0x26,          // NEXTVAL('sequence_name') - Get next value
            SEQUENCE_CURRVAL = 0x27,          // CURRVAL('sequence_name') - Get current value
            SEQUENCE_SETVAL = 0x28,           // SETVAL('sequence_name', value, is_called) - Set value
            CREATE_VIEW = 0x29,               // Create view (ALPHA Phase 1 - Views)
            DROP_VIEW = 0x2A,                 // Drop view (ALPHA Phase 1 - Views)
            REFRESH_MATERIALIZED_VIEW = 0x2B, // Refresh materialized view (ALPHA Phase 1 - Materialized Views)
            INSERT = 0x11,                    // Insert row
            SELECT = 0x12,                    // Select query
            UPDATE = 0xC3,                    // Update rows (Phase 1 Task 2.1)
            DELETE = 0xC4,                    // Delete rows (Phase 1 Task 2.2)
            START_TRANSACTION = 0x13,         // Start transaction (Phase 2 Task 2.6)
            SET_TRANSACTION = 0x17,           // Set transaction parameters (Phase 3 Task 3.6)
            COMMIT = 0x14,                    // Commit transaction (Phase 2 Task 2.6)
            ROLLBACK = 0x15,                  // Rollback transaction (Phase 2 Task 2.6)
            SWEEP = 0x16,                     // Sweep database (Phase 3 Task 3.3)
            CREATE_TABLESPACE = 0x18,         // Create tablespace (Phase 2 Task 2.1)
            ALTER_TABLESPACE = 0x1A,          // Alter tablespace (Phase 2 Task 2.2)
            DROP_TABLESPACE = 0x19,           // Drop tablespace (Phase 2 Task 2.1)
            ALTER_TABLE_SET_TABLESPACE = 0x1C, // Alter table set tablespace (Phase 4 Task 4.1.6)
            ATTACH_TABLESPACE = 0x1D,         // Attach tablespace (Phase 6 Task 6.1)
            DETACH_TABLESPACE = 0x1E,         // Detach tablespace (Phase 6 Task 6.2)
            CREATE_JOB = 0xB4,                // Create job (WS-4 Scheduler)
            ALTER_JOB = 0xB5,                 // Alter job (WS-4 Scheduler)
            DROP_JOB = 0xB6,                  // Drop job (WS-4 Scheduler)
            EXECUTE_JOB = 0xB7,               // Execute job (WS-4 Scheduler)
            CANCEL_JOB_RUN = 0xB8,            // Cancel job run (WS-4 Scheduler)

            // Data types
            TYPE_INTEGER = 0x20,   // 32-bit integer (INT32)
            TYPE_BIGINT = 0x21,    // 64-bit integer (INT64)
            TYPE_DOUBLE = 0x22,    // Double precision float (FLOAT64)
            TYPE_VARCHAR = 0x23,   // Variable length string
            TYPE_BOOLEAN = 0x24,   // Boolean (true/false)
            TYPE_INT8 = 0x25,      // 8-bit integer
            TYPE_INT16 = 0x26,     // 16-bit integer
            TYPE_FLOAT32 = 0x27,   // Single precision float
            TYPE_DATE = 0x28,      // Date (days since epoch)
            TYPE_TIME = 0x29,      // Time (microseconds since midnight)
            TYPE_TIMESTAMP = 0x2A, // Timestamp (microseconds since epoch)
            TYPE_UUID = 0x2B,      // UUID (16 bytes)
            TYPE_DECIMAL = 0x2C,   // DECIMAL with precision/scale
            TYPE_CHAR = 0x2D,      // Fixed-length character string
            TYPE_TEXT = 0x2E,      // Unlimited text
            TYPE_BINARY = 0x2F,    // Fixed-length binary

            // Values
            LITERAL_NULL = 0x30,      // NULL value
            LITERAL_INT32 = 0x31,     // 32-bit integer literal
            LITERAL_INT64 = 0x32,     // 64-bit integer literal
            LITERAL_DOUBLE = 0x33,    // Double literal
            LITERAL_STRING = 0x34,    // String literal (length + data)
            LITERAL_CHARSET = 0x35,   // Charset ID (uint16_t)
            LITERAL_COLLATION = 0x36, // Collation ID (uint32_t)
            LITERAL_BOOLEAN = 0x37,   // Boolean literal (uint8_t 0/1)
            LITERAL_UUID = 0x38,      // UUID literal (16 bytes)
            LITERAL_DATE = 0x39,      // Date literal (int32 days since epoch)
            LITERAL_TIME = 0x3A,      // Time literal (int64 microseconds since midnight)
            LITERAL_TIMESTAMP = 0x3B, // Timestamp literal (int64 microseconds since epoch)
            LITERAL_BINARY = 0x3C,    // Binary literal (len:UVARINT + bytes)
            LITERAL_DECIMAL = 0x3D,   // Decimal literal (string payload)
            LITERAL_JSON = 0x3E,      // JSON literal (string payload)
            LITERAL_XML = 0x3F,       // XML literal (string payload)

            // Column/Table references
            TABLE_REF = 0x40,  // Table reference (string id)
            COLUMN_REF = 0x41, // Column reference (string id)
            COLUMN_DEF = 0x42, // Column definition
            ASSIGNMENT = 0x43, // Assignment (column = value) for UPDATE (Phase 1 Task 2.1)

            // Expressions
            EXPR_ADD = 0x50,      // Addition
            EXPR_SUBTRACT = 0x51, // Subtraction
            EXPR_MULTIPLY = 0x52, // Multiplication
            EXPR_DIVIDE = 0x53,   // Division
            EXPR_MODULO = 0x54,   // Modulo

            // Comparisons
            EXPR_EQ = 0x60, // Equal
            EXPR_NE = 0x61, // Not equal
            EXPR_LT = 0x62, // Less than
            EXPR_GT = 0x63, // Greater than
            EXPR_LE = 0x64, // Less than or equal
            EXPR_GE = 0x65, // Greater than or equal

            // Logical
            EXPR_AND = 0x70, // Logical AND
            EXPR_OR = 0x71,  // Logical OR

            // Type conversion
            EXPR_CAST = 0x72, // Type cast (expr + target type + modifiers + format)

            // Pattern matching
            EXPR_LIKE = 0x78,  // LIKE pattern match
            EXPR_ILIKE = 0x79, // ILIKE case-insensitive pattern match

            // String functions
            FUNC_LENGTH = 0x73,       // LENGTH(str) - byte length
            FUNC_SUBSTRING = 0x74,    // SUBSTRING(str, start, length)
            FUNC_UPPER = 0x75,        // UPPER(str)
            FUNC_LOWER = 0x76,        // LOWER(str)
            FUNC_TRIM = 0x77,         // TEND(str)
            FUNC_CHAR_LENGTH = 0x89,  // CHAR_LENGTH(str) - character count
            FUNC_OCTET_LENGTH = 0x8A, // OCTET_LENGTH(str) - byte count
            FUNC_CONVERT = 0x8B,      // CONVERT(str, from_cs, to_cs)
            FUNC_COLLATE = 0x8C,      // Apply collation to expression

            // Aggregate functions
            AGG_SUM = 0x7A,   // SUM(expr)
            AGG_AVG = 0x7B,   // AVG(expr)
            AGG_MIN = 0x7C,   // MIN(expr)
            AGG_MAX = 0x7D,   // MAX(expr)
            AGG_COUNT = 0x7E, // COUNT(expr) or COUNT(*)

            // Statistical aggregate functions (Nov 14, 2025)
            AGG_STDDEV_SAMP = 0x7F,  // STDDEV / STDDEV_SAMP(expr) - sample standard deviation
            AGG_STDDEV_POP = 0x80,   // STDDEV_POP(expr) - population standard deviation
            AGG_VAR_SAMP = 0x81,     // VARIANCE / VAR_SAMP(expr) - sample variance
            AGG_VAR_POP = 0x82,      // VAR_POP(expr) - population variance
            AGG_CORR = 0x83,         // CORR(y, x) - Pearson correlation coefficient
            AGG_COVAR_POP = 0x84,    // COVAR_POP(y, x) - population covariance

            // Regression aggregate functions (Alpha 1 - Missing Functions)
            AGG_REGR_SLOPE = 0x8E,       // REGR_SLOPE(y, x) - slope of linear regression
            AGG_REGR_INTERCEPT = 0x8F,   // REGR_INTERCEPT(y, x) - y-intercept of regression
            AGG_REGR_COUNT = 0x90,       // REGR_COUNT(y, x) - count of non-null pairs
            AGG_REGR_R2 = 0x91,          // REGR_R2(y, x) - coefficient of determination (R²)
            AGG_REGR_AVGX = 0x92,        // REGR_AVGX(y, x) - average of x values
            AGG_REGR_AVGY = 0x93,        // REGR_AVGY(y, x) - average of y values
            AGG_REGR_SXX = 0x94,         // REGR_SXX(y, x) - sum of squares of x
            AGG_REGR_SYY = 0x95,         // REGR_SYY(y, x) - sum of squares of y
            AGG_REGR_SXY = 0x96,         // REGR_SXY(y, x) - sum of cross-products

            // Temporal functions (Note: FUNC_DATE_ADD collision at 0x84 needs fixing)
            FUNC_DATE_ADD = 0x84,     // DATE_ADD(date, days) - legacy opcode (extended map recommended)
            FUNC_DATE_SUB = 0x85,     // DATE_SUB(date, days)
            FUNC_DATE_DIFF = 0x86,    // DATE_DIFF(date1, date2) - returns days
            FUNC_NOW = 0x87,          // NOW() - current timestamp
            FUNC_CURRENT_DATE = 0x88, // CURRENT_DATE() - current date
            FUNC_AT_TIME_ZONE =
                0x8D, // timestamp AT TIME ZONE timezone_id - convert to timezone for display

            // Lists
            BEGIN_LIST = 0x80, // Start of list (followed by count)
            END_LIST = 0x81,   // End of list

            // Modifiers / Constraints
            NOT_NULL = 0x90,          // NOT NULL constraint
            DEFAULT_VALUE = 0x91,     // DEFAULT value expression (ALPHA Phase A)
            CHECK_CONSTRAINT = 0x92,  // CHECK constraint expression (ALPHA Phase A)
            FOREIGN_KEY = 0x93,       // Foreign key constraint (ALPHA Phase A)
            TABLE_FK = 0x94,          // Table-level foreign key constraint (ALPHA Phase C - Composite FK)
            UNIQUE_CONSTRAINT = 0x95, // UNIQUE constraint (column-level or table-level)
            PRIMARY_KEY = 0x96,       // PRIMARY KEY constraint (column-level or table-level)
            IDENTITY_COLUMN = 0x97,   // IDENTITY column (GENERATED {ALWAYS|BY DEFAULT} AS IDENTITY) - ALPHA Phase 1
            GENERATED_COLUMN = 0x98,  // GENERATED column (GENERATED ALWAYS AS expr [STORED|VIRTUAL]) - ALPHA Phase 1
            COLUMN_COLLATE = 0x99,   // Column collation name (string payload)
            COLUMN_CHARSET = 0x9A,   // Column charset name (string payload)

            // Special
            SELECT_STAR = 0xA0,  // SELECT *
            WHERE_CLAUSE = 0xA1, // WHERE clause marker

            // Additional data types (0xB0-0xBF range)
            TYPE_VARBINARY = 0xB0, // Variable-length binary
            TYPE_BLOB = 0xB1,      // Binary large object
            TYPE_BYTEA = 0xB2,     // Byte array (PostgreSQL compatible)
            TYPE_JSON = 0xB3,      // JSON data
            TYPE_ARRAY = 0xB9,     // Array type (element type + size payload)
            TYPE_DOMAIN = 0xB8,    // Domain type reference (domain_id follows)

            // Query optimization hints (Phase 1, Task 1.3)
            SCAN_HINT = 0xC0,  // Scan method hint (0=seq, 1=index)
            INDEX_REF = 0xC1,  // Index reference (string - index UUID)

            // EXPLAIN command (Phase 1, Task 1.5)
            EXPLAIN_PLAN = 0xC2,  // EXPLAIN output (string)

            // JOIN operations (Phase 1, Task 3.3)
            NESTED_LOOP_JOIN = 0xC5,  // Nested loop join
            HASH_JOIN = 0xC6,         // Hash join
            JOIN_TYPE = 0xC7,         // Join type marker (INNER, LEFT, RIGHT, FULL)
            JOIN_CONDITION = 0xC8,    // Join condition expression
            JOIN_USING = 0xC9,        // JOIN USING clause - column list for equi-join

            // Aggregation and grouping (Phase 1, Task 4.1)
            GROUP_BY = 0xCA,          // GROUP BY clause marker
            HAVING = 0xCB,            // HAVING clause marker
            AGG_INIT = 0xCC,          // Initialize aggregation state
            AGG_ACCUMULATE = 0xCD,    // Accumulate aggregate value
            AGG_FINALIZE = 0xCE,      // Finalize aggregate result

            // Sorting (Phase 1, Task 5.1)
            ORDER_BY = 0xCF,          // ORDER BY clause marker
            SORT_KEY = 0xD0,          // Sort key expression
            SORT_ASC = 0xD1,          // Sort ascending
            SORT_DESC = 0xD2,         // Sort descending
            NULLS_FIRST = 0xD3,       // NULLS FIRST modifier
            NULLS_LAST = 0xD4,        // NULLS LAST modifier

            // Limiting (Phase 1, Task 5.2)
            LIMIT = 0xD5,             // LIMIT clause
            OFFSET = 0xD6,            // OFFSET clause

            // Window functions (Phase 1, Task 6.3)
            WINDOW = 0xD7,            // Window function clause marker
            WINDOW_SPEC = 0xD8,       // Window specification (OVER clause)
            PARTITION_BY = 0xD9,      // PARTITION BY clause
            WINDOW_ORDER_BY = 0xDA,   // ORDER BY within window spec
            FRAME_CLAUSE = 0xDB,      // Frame clause marker
            FRAME_ROWS = 0xDB,        // ROWS frame mode
            FRAME_RANGE = 0xDC,       // RANGE frame mode
            FRAME_GROUPS = 0x6C,      // P2-9: GROUPS frame mode
            FRAME_UNBOUNDED_PRECEDING = 0xDD,  // UNBOUNDED PRECEDING boundary
            FRAME_PRECEDING = 0xDE,   // n PRECEDING boundary
            FRAME_CURRENT_ROW = 0xDF, // CURRENT ROW boundary
            FRAME_FOLLOWING = 0xE0,   // n FOLLOWING boundary
            FRAME_UNBOUNDED_FOLLOWING = 0xE1,  // UNBOUNDED FOLLOWING boundary

            // Window function types
            WIN_ROW_NUMBER = 0xE2,    // ROW_NUMBER()
            WIN_RANK = 0xE3,          // RANK()
            WIN_DENSE_RANK = 0xE4,    // DENSE_RANK()
            WIN_LAG = 0xE5,           // LAG(expr [, offset [, default]])
            WIN_LEAD = 0xE6,          // LEAD(expr [, offset [, default]])
            WIN_FIRST_VALUE = 0xE7,   // FIRST_VALUE(expr)
            WIN_LAST_VALUE = 0xE8,    // LAST_VALUE(expr)
            WIN_NTH_VALUE = 0xE9,     // NTH_VALUE(expr, n)

            // JSON functions (Phase 1 Task 7)
            // Extraction functions (Task 7.1)
            JSON_EXTRACT = 0xEA,           // JSON_EXTRACT(json, path)
            JSONB_EXTRACT_PATH = 0xEB,     // jsonb_extract_path(jsonb, path_elem...)
            JSON_ARROW = 0xEC,             // json -> 'field' (returns JSON)
            JSON_DOUBLE_ARROW = 0xED,      // json ->> 'field' (returns text)
            JSON_HASH_ARROW = 0xEE,        // json #> array (returns JSON)
            JSON_HASH_DOUBLE_ARROW = 0xEF, // json #>> array (returns text)

            // Construction functions (Task 7.2)
            JSON_OBJECT = 0xF0,            // JSON_OBJECT(key1, val1, key2, val2, ...)
            JSON_ARRAY = 0xF1,             // JSON_ARRAY(val1, val2, ...)
            JSONB_BUILD_OBJECT = 0xF2,     // jsonb_build_object(key1, val1, ...)
            JSONB_BUILD_ARRAY = 0xF3,      // jsonb_build_array(val1, val2, ...)

            // Modification functions (Task 7.3)
            JSON_SET = 0xF4,               // JSON_SET(json, path, value)
            JSON_INSERT = 0xF5,            // JSON_INSERT(json, path, value)
            JSON_REMOVE = 0xF6,            // JSON_REMOVE(json, path)
            JSONB_SET = 0xF7,              // jsonb_set(jsonb, path_array, value)

            // Conditional expressions (Phase 1 Task 8)
            COALESCE = 0xF8,               // COALESCE(arg1, arg2, ...) - return first non-null
            NULLIF = 0xF9,                 // NULLIF(expr1, expr2) - return NULL if equal
            CASE_WHEN = 0xFA,              // CASE WHEN ... - conditional expression

            // Array functions (Phase 2 Task 12) - 0xFB-0xFF range
            // Array aggregate
            ARRAY_AGG = 0xFB,              // ARRAY_AGG(expr) - aggregate function

            // Array table function
            UNNEST = 0xFC,                 // UNNEST(array) - table-valued function

            // Array conversion functions (extended opcodes start at 0x01FB)
            ARRAY_TO_STRING = 0xFD,        // ARRAY_TO_STRING(array, delim [, null_str])
            STRING_TO_ARRAY = 0xFE,        // STRING_TO_ARRAY(string, delim [, null_str])

            // Note: Additional array opcodes beyond 0xFF will use extended encoding
            // Extended format: 0xFF <extended_opcode_byte>
            EXTENDED_OPCODE = 0xFF,        // Extended opcode marker

            // Extended array opcodes (used with EXTENDED_OPCODE prefix)
            // Array manipulation functions
            EXT_ARRAY_APPEND = 0x01,       // ARRAY_APPEND(array, element)
            EXT_ARRAY_PREPEND = 0x02,      // ARRAY_PREPEND(element, array)
            EXT_ARRAY_CAT = 0x03,          // ARRAY_CAT(array1, array2)
            EXT_ARRAY_REMOVE = 0x04,       // ARRAY_REMOVE(array, element)
            EXT_ARRAY_REPLACE = 0x05,      // ARRAY_REPLACE(array, from, to)

            // Array operators
            EXT_ARRAY_OVERLAP = 0x10,      // && - array overlap (has common elements)
            EXT_ARRAY_CONTAINS = 0x11,     // @> - array contains (left contains all of right)
            EXT_ARRAY_CONTAINED_BY = 0x12, // <@ - array is contained by (left subset of right)
            EXT_ARRAY_EQ = 0x13,           // = - array equality
            EXT_ARRAY_NE = 0x14,           // <> - array inequality

            // Array accessor functions
            EXT_ARRAY_LENGTH = 0x20,       // ARRAY_LENGTH(array, dimension)
            EXT_ARRAY_DIMS = 0x21,         // ARRAY_DIMS(array) - dimensions as text
            EXT_ARRAY_UPPER = 0x22,        // ARRAY_UPPER(array, dimension) - upper bound
            EXT_ARRAY_LOWER = 0x23,        // ARRAY_LOWER(array, dimension) - lower bound

            // Array construction
            EXT_ARRAY_CONSTRUCT = 0x24,    // Construct array from stack elements
            EXT_ARRAY_SUBSCRIPT = 0x25,   // Array subscript access (array[index])

            // Text search and regex functions (Phase 2 Task 13) - 0x30-0x4F range
            // Regex operators (can be used without EXTENDED_OPCODE prefix due to available space)
            EXT_REGEX_MATCH = 0x30,        // ~ operator (regex match case-sensitive)
            EXT_REGEX_MATCH_CI = 0x31,     // ~* operator (regex match case-insensitive)
            EXT_REGEX_NOT_MATCH = 0x32,    // !~ operator (regex not match case-sensitive)
            EXT_REGEX_NOT_MATCH_CI = 0x33, // !~* operator (regex not match case-insensitive)

            // Regex functions
            EXT_REGEXP_MATCHES = 0x34,     // REGEXP_MATCHES(str, pattern [, flags])
            EXT_REGEXP_REPLACE = 0x35,     // REGEXP_REPLACE(str, pattern, replacement [, flags])
            EXT_REGEXP_SPLIT_TO_TABLE = 0x36,  // REGEXP_SPLIT_TO_TABLE(str, pattern [, flags])
            EXT_REGEXP_SPLIT_TO_ARRAY = 0x37,  // REGEXP_SPLIT_TO_ARRAY(str, pattern [, flags])

            // String tokenization
            EXT_SPLIT_PART = 0x38,         // SPLIT_PART(str, delimiter, field)
            EXT_STRING_TO_TABLE = 0x39,    // STRING_TO_TABLE(str, delimiter)
            EXT_UNNEST_TEXT = 0x3A,        // UNNEST_TEXT(text_array)

            // Text utilities
            EXT_STRPOS = 0x3B,             // STRPOS(str, substring)
            EXT_POSITION = 0x3C,           // POSITION(substring IN string)
            EXT_OVERLAY = 0x3D,            // OVERLAY(str PLACING newstr FROM start [FOR length])
            EXT_QUOTE_LITERAL = 0x3E,      // QUOTE_LITERAL(str)
            EXT_QUOTE_IDENT = 0x3F,        // QUOTE_IDENT(str)

            // Case conversion and string utilities
            EXT_INITCAP = 0x40,            // INITCAP(str) - capitalize first letter of each word
            EXT_ASCII = 0x41,              // ASCII(str) - get ASCII code of first character
            EXT_CHR = 0x42,                // CHR(code) - convert ASCII code to character
            EXT_REPEAT = 0x43,             // REPEAT(str, count)
            EXT_REVERSE = 0x44,            // REVERSE(str)

            // Advanced GROUP BY operations (ALPHA Phase 1 - Missing Functions Phase 3) - 0x45-0x48
            EXT_GROUP_ROLLUP = 0x45,       // ROLLUP(...) - hierarchical grouping
            EXT_GROUP_CUBE = 0x46,         // CUBE(...) - all combinations grouping
            EXT_GROUP_GROUPING_SETS = 0x47, // GROUPING SETS(...) - explicit grouping sets
            EXT_GROUPING_FUNC = 0x48,      // GROUPING(column) - identify aggregated columns

            EXT_LPAD = 0x57,               // LPAD(str, length [, fill]) - left-pad string
            EXT_RPAD = 0x58,               // RPAD(str, length [, fill]) - right-pad string

            // Spatial types and operations (Phase 2 Task 9.1) - 0x50-0x5F range
            EXT_TYPE_POINT = 0x50,         // POINT data type marker
            EXT_TYPE_LINESTRING = 0x51,    // LINESTRING data type marker
            EXT_TYPE_POLYGON = 0x52,       // POLYGON data type marker

            // Spatial constructor functions
            EXT_ST_POINT = 0x53,           // ST_Point(x, y) - create point
            EXT_ST_MAKELINE = 0x54,        // ST_MakeLine(...) - create linestring
            EXT_ST_MAKEPOLYGON = 0x55,     // ST_MakePolygon(...) - create polygon

            // Spatial output functions
            EXT_ST_ASTEXT = 0x56,          // ST_AsText(geom) - WKT output
            EXT_ST_ASBINARY = 0x57,        // ST_AsBinary(geom) - WKB output
            EXT_ST_GEOMETRYTYPE = 0x58,    // ST_GeometryType(geom) - type name
            EXT_ST_ISVALID = 0x59,         // ST_IsValid(geom) - validation check

            // Spatial geometric operations (Task 9.3)
            EXT_ST_BUFFER = 0x5A,          // ST_Buffer(geom, distance) - create buffer polygon
            EXT_ST_CONVEXHULL = 0x5B,      // ST_ConvexHull(geom) - convex hull polygon
            EXT_ST_ENVELOPE = 0x5C,        // ST_Envelope(geom) - bounding box polygon

            // Spatial predicates (Task 9.3 - G2/G4)
            EXT_ST_INTERSECTS = 0x5D,      // ST_Intersects(geom1, geom2) - do geometries intersect?
            EXT_ST_CONTAINS = 0x5E,        // ST_Contains(geom1, geom2) - does geom1 contain geom2?
            EXT_ST_WITHIN = 0x5F,          // ST_Within(geom1, geom2) - is geom1 within geom2?
            EXT_ST_EQUALS = 0x63,          // ST_Equals(geom1, geom2) - are geometries spatially equal?

            // CTE (Common Table Expression) support (Phase 2 Wave 2 - Agent A) - 0x60-0x6F range
            EXT_CTE_DEF = 0x60,            // CTE definition marker
            EXT_CTE_SCAN = 0x61,           // CTE scan operation
            EXT_WITH_CLAUSE = 0x62,        // WITH clause marker

            // Set operations (UNION, INTERSECT, EXCEPT) - 0x64-0x6F range
            EXT_UNION = 0x64,              // UNION (removes duplicates)
            EXT_UNION_ALL = 0x65,          // UNION ALL (keeps duplicates)
            EXT_INTERSECT = 0x66,          // INTERSECT (removes duplicates)
            EXT_INTERSECT_ALL = 0x67,      // INTERSECT ALL (keeps duplicates)
            EXT_EXCEPT = 0x68,             // EXCEPT (removes duplicates)
            EXT_EXCEPT_ALL = 0x69,         // EXCEPT ALL (keeps duplicates)

            // Additional window functions (Alpha 1 - Missing Functions Phase 4) - 0x6A-0x6B range
            EXT_WIN_CUME_DIST = 0x6A,      // CUME_DIST() - cumulative distribution
            EXT_WIN_PERCENT_RANK = 0x6B,   // PERCENT_RANK() - relative rank percentile

            // Additional date/time functions (Alpha 1 - Missing Functions Phase 5) - 0x6C range
            EXT_FUNC_AGE = 0x6C,           // AGE(timestamp [, timestamp]) - age between timestamps

            // Database trigger opcodes (Firebird-style) - 0x6D-0x6F range
            EXT_CREATE_DB_TRIGGER = 0x6D,  // CREATE TRIGGER (database trigger)
            EXT_DROP_DB_TRIGGER = 0x6E,    // DROP TRIGGER (database trigger)
            EXT_FIRE_DB_TRIGGER = 0x6F,    // Internal: Fire database trigger

            // Trigger opcodes (Phase 2 Wave 2 - Agent C) - 0x70-0x72 range
            EXT_CREATE_TRIGGER = 0x70,     // CREATE TRIGGER
            EXT_DROP_TRIGGER = 0x71,       // DROP TRIGGER
            EXT_FIRE_TRIGGER = 0x72,       // Internal: Fire trigger (used by executor)

            // Subquery opcodes (Phase 2 Wave 2 - Agent B) - 0x73-0x77 range
            EXT_SUBQUERY_SCALAR = 0x73,    // Scalar subquery (returns single value)
            EXT_SUBQUERY_EXISTS = 0x74,    // EXISTS subquery (returns boolean)
            EXT_SUBQUERY_IN = 0x75,        // IN subquery (membership test)
            EXT_SUBQUERY_NOT_IN = 0x76,    // NOT IN subquery (negated membership)
            EXT_SUBQUERY_END = 0x77,       // End of subquery marker
            EXT_SUBQUERY_ARRAY = 0x5B,     // WP-6 PARSE-M3: ARRAY(SELECT ...) subquery

            // Additional spatial functions (Task 9.3 - G4) - 0x78-0x8F range
            EXT_ST_DISJOINT = 0x78,        // ST_Disjoint(geom1, geom2) - are geometries disjoint?
            EXT_ST_OVERLAPS = 0x79,        // ST_Overlaps(geom1, geom2) - do geometries overlap?
            EXT_ST_TOUCHES = 0x7A,         // ST_Touches(geom1, geom2) - do geometries touch?
            EXT_ST_CROSSES = 0x7B,         // ST_Crosses(geom1, geom2) - do geometries cross?

            // Spatial processing functions (Task 9.3 - G4)
            EXT_ST_INTERSECTION = 0x7C,    // ST_Intersection(geom1, geom2) - intersection geometry
            EXT_ST_UNION = 0x7D,           // ST_Union(geom1, geom2) - union geometry
            EXT_ST_DIFFERENCE = 0x7E,      // ST_Difference(geom1, geom2) - difference geometry (geom1 - geom2)

            // Spatial metrics (Task 9.3 - G4)
            EXT_ST_AREA = 0x7F,            // ST_Area(geom) - area of polygon
            EXT_ST_LENGTH = 0x80,          // ST_Length(geom) - length of linestring
            EXT_ST_DISTANCE = 0x81,        // ST_Distance(geom1, geom2) - distance between geometries
            EXT_ST_PERIMETER = 0x82,       // ST_Perimeter(geom) - perimeter of polygon

            // Coordinate system operations (Task 9.5 - S1/S2/S3)
            EXT_ST_SRID = 0x83,            // ST_SRID(geom) - get SRID of geometry
            EXT_ST_SETSRID = 0x84,         // ST_SetSRID(geom, srid) - set SRID of geometry
            EXT_ST_TRANSFORM = 0x85,       // ST_Transform(geom, srid) - transform to different SRID
            EXT_ST_DISTANCE_SPHERE = 0x86, // ST_Distance_Sphere(geom1, geom2) - geodetic distance

            // Multi-geometry constructors (Task 9.4)
            EXT_ST_MULTIPOINT = 0x87,      // ST_MultiPoint(...) - create MULTIPOINT
            EXT_ST_MULTILINESTRING = 0x88, // ST_MultiLineString(...) - create MULTILINESTRING
            EXT_ST_MULTIPOLYGON = 0x89,    // ST_MultiPolygon(...) - create MULTIPOLYGON
            EXT_ST_GEOMETRYCOLLECTION = 0x8A, // ST_GeometryCollection(...) - create GEOMETRYCOLLECTION
            EXT_ST_COLLECT = 0x8B,         // ST_Collect(...) - collect geometries (alias for GeometryCollection)

            // Multi-geometry accessors (Task 9.4)
            EXT_ST_GEOMETRYN = 0x8C,       // ST_GeometryN(geom, n) - get Nth geometry from collection
            EXT_ST_NUMGEOMETRIES = 0x8D,   // ST_NumGeometries(geom) - get number of geometries in collection
            EXT_ST_DUMP = 0x8E,            // ST_Dump(geom) - dump all geometries from collection

            // Extraction function
            EXT_EXTRACT = 0x8F,            // EXTRACT(field FROM value) - extract sub-information from complex types

            // PSQL - Stored Procedures and Functions (Phase 2 Task 10.2) - 0x90-0xAF range
            // Procedural language opcodes
            EXT_FUNCTION = 0x90,           // Function definition
            EXT_PROCEDURE = 0x91,          // Procedure definition
            EXT_BLOCK = 0x92,              // BEGIN...END block
            EXT_DECLARE = 0x93,            // Variable declaration
            EXT_ASSIGN = 0x94,             // Variable assignment
            EXT_IF = 0x95,                 // IF statement
            EXT_ELSIF = 0x96,              // ELSIF clause
            EXT_ELSE = 0x97,               // ELSE clause
            EXT_LOOP = 0x98,               // LOOP statement
            EXT_WHILE = 0x99,              // WHILE loop
            EXT_EXIT = 0x9A,               // EXIT statement
            EXT_RETURN = 0x9B,             // RETURN statement
            EXT_RAISE = 0x9C,              // RAISE exception
            EXT_TRY = 0x9D,                // TRY block
            EXT_EXCEPT_HANDLER = 0x9E,     // EXCEPT handler (exception handling, not set operation)
            EXT_EXCEPTION_HANDLER = 0x9F,  // Exception handler definition

            // Control flow helpers
            EXT_JUMP_IF_TRUE = 0xA0,       // Conditional jump (true)
            EXT_JUMP_IF_FALSE = 0xA1,      // Conditional jump (false)
            EXT_JUMP = 0xA2,               // Unconditional jump
            EXT_LABEL = 0xA3,              // Label marker

            // Variable operations
            EXT_VAR_LOAD = 0xA4,           // Load variable value
            EXT_VAR_STORE = 0xA5,          // Store to variable
            EXT_PARAM_IN = 0xA6,           // IN parameter marker
            EXT_PARAM_OUT = 0xA7,          // OUT parameter marker
            EXT_PARAM_INOUT = 0xA8,        // INOUT parameter marker

            // Text search opcodes (Phase 3 Task 14.3)
            EXT_TSMATCH = 0xA9,            // @@ text search match operator (tsvector @@ tsquery)
            EXT_TS_RANK = 0xAA,            // TS_RANK(tsvector, tsquery) - relevance ranking
            EXT_TYPE_TSVECTOR = 0xAB,      // TSVECTOR data type marker
            EXT_TYPE_TSQUERY = 0xAC,       // TSQUERY data type marker
            EXT_TO_TSVECTOR = 0xAD,        // TO_TSVECTOR(config, text) - text to tsvector
            EXT_TO_TSQUERY = 0xAE,         // TO_TSQUERY(config, query) - query to tsquery
            EXT_PLAINTO_TSQUERY = 0xAF,    // PLAINTO_TSQUERY(config, text) - plain text to query
            EXT_PHRASETO_TSQUERY = 0xB0,   // PHRASETO_TSQUERY(config, text) - phrase to query

            // Range types and operations (Task 15 Phase 5) - 0xB1-0xBF range
            // Range type markers
            EXT_TYPE_INT4RANGE = 0xB1,     // INT4RANGE data type marker
            EXT_TYPE_INT8RANGE = 0xB2,     // INT8RANGE data type marker
            EXT_TYPE_NUMRANGE = 0xB3,      // NUMRANGE data type marker
            EXT_TYPE_DATERANGE = 0xB4,     // DATERANGE data type marker
            EXT_TYPE_TSRANGE = 0xB5,       // TSRANGE data type marker
            EXT_TYPE_TSTZRANGE = 0xB6,     // TSTZRANGE data type marker

            // Range constructor functions
            EXT_RANGE_CONSTRUCT = 0xB7,    // Construct range from bounds (lower, upper, bounds_type)

            // Range operators (reuse array operator codes where semantics match)
            EXT_RANGE_OVERLAPS = 0xB8,     // && - ranges overlap (same as EXT_ARRAY_OVERLAP semantics)
            EXT_RANGE_CONTAINS_RANGE = 0xB9,    // @> - range contains range (similar to array)
            EXT_RANGE_CONTAINS_ELEM = 0xBA,     // @> - range contains element
            EXT_RANGE_CONTAINED_BY = 0xBB,      // <@ - range contained by range
            EXT_RANGE_STRICTLY_LEFT = 0xBC,     // << - strictly left of
            EXT_RANGE_STRICTLY_RIGHT = 0xBD,    // >> - strictly right of
            EXT_RANGE_ADJACENT = 0xBE,     // -|- - adjacent to
            EXT_RANGE_UNION = 0xBF,        // + - range union
            EXT_RANGE_INTERSECTION = 0xC0, // & - range intersection
            EXT_RANGE_DIFFERENCE = 0xC1,   // - - range difference

            // Range accessor functions
            EXT_RANGE_LOWER = 0xC2,        // LOWER(range) - get lower bound
            EXT_RANGE_UPPER = 0xC3,        // UPPER(range) - get upper bound
            EXT_RANGE_ISEMPTY = 0xC4,      // ISEMPTY(range) - check if range is empty
            EXT_RANGE_LOWER_INC = 0xC5,    // LOWER_INC(range) - check if lower bound is inclusive
            EXT_RANGE_UPPER_INC = 0xC6,    // UPPER_INC(range) - check if upper bound is inclusive
            EXT_RANGE_LOWER_INF = 0xC7,    // LOWER_INF(range) - check if lower bound is infinite
            EXT_RANGE_UPPER_INF = 0xC8,    // UPPER_INF(range) - check if upper bound is infinite
            EXT_RANGE_MERGE = 0xC9,        // RANGE_MERGE(r1, r2) - smallest range containing both

            // Stored code DDL (Alpha Phase 3)
            EXT_CREATE_FUNCTION_STMT = 0x30,   // CREATE [OR REPLACE] FUNCTION
            EXT_CREATE_PROCEDURE_STMT = 0x31,  // CREATE [OR REPLACE] PROCEDURE
            EXT_CREATE_PACKAGE_STMT = 0x32,    // CREATE [OR REPLACE] PACKAGE
            EXT_DROP_FUNCTION_STMT = 0x33,     // DROP FUNCTION [IF EXISTS]
            EXT_DROP_PROCEDURE_STMT = 0x34,    // DROP PROCEDURE [IF EXISTS]
            EXT_DROP_PACKAGE_STMT = 0x35,      // DROP PACKAGE [IF EXISTS]

            // Security System (ALPHA Phase 1 - Security System Phase 2) - 0xCA-0xD6 range
            // User management opcodes
            EXT_CREATE_USER = 0xCA,        // CREATE USER username [WITH PASSWORD 'xxx'] [SUPERUSER]
            EXT_ALTER_USER = 0xCB,         // ALTER USER username [WITH PASSWORD 'xxx'] [SUPERUSER]
            EXT_DROP_USER = 0xCC,          // DROP USER username [IF EXISTS] [CASCADE | RESTRICT]

            // Role management opcodes
            EXT_CREATE_ROLE = 0xCD,        // CREATE ROLE rolename
            EXT_DROP_ROLE = 0xCE,          // DROP ROLE rolename [IF EXISTS] [CASCADE | RESTRICT]

            // Group management opcodes
            EXT_CREATE_GROUP = 0xCF,       // CREATE GROUP groupname
            EXT_DROP_GROUP = 0xD0,         // DROP GROUP groupname [IF EXISTS] [CASCADE | RESTRICT]
            EXT_FB_CREATE_MAPPING = 0x02F1,  // Firebird CREATE MAPPING
            EXT_FB_ALTER_MAPPING = 0x02F2,   // Firebird ALTER MAPPING
            EXT_FB_DROP_MAPPING = 0x02F3,    // Firebird DROP MAPPING
            EXT_FB_CREATE_SHADOW = 0x02F4,   // Firebird CREATE SHADOW
            EXT_FB_DROP_SHADOW = 0x02F5,     // Firebird DROP SHADOW

            // Privilege management opcodes
            EXT_GRANT_PRIVILEGE = 0xD1,    // GRANT privilege ON object TO grantee [WITH GRANT OPTION]
            EXT_REVOKE_PRIVILEGE = 0xD2,   // REVOKE privilege ON object FROM grantee [CASCADE | RESTRICT]

            // Role grant/revoke opcodes
            EXT_GRANT_ROLE = 0xD3,         // GRANT role TO user/role
            EXT_REVOKE_ROLE = 0xD4,        // REVOKE role FROM user/role [CASCADE | RESTRICT]

            // Session management opcodes
            EXT_SET_ROLE = 0xD5,           // SET ROLE rolename / RESET ROLE
            EXT_SET_SESSION_AUTH = 0xD6,   // SET SESSION AUTHORIZATION username / RESET SESSION AUTHORIZATION
            EXT_SET_CONSTRAINTS = 0x4F,    // P2-7: SET CONSTRAINTS {ALL | names} {DEFERRED | IMMEDIATE}

            // Row-Level Security opcodes (Security Phase 3.4)
            EXT_CREATE_POLICY = 0xD7,      // CREATE POLICY policy_name ON table_name
            EXT_DROP_POLICY = 0xD8,        // DROP POLICY [IF EXISTS] policy_name ON table_name
            EXT_ALTER_TABLE_RLS = 0xD9,    // ALTER TABLE table_name {ENABLE|DISABLE|FORCE|NO FORCE} ROW LEVEL SECURITY
            EXT_ALTER_POLICY = 0xE5,       // ALTER POLICY policy_name ON table_name

            // Mathematical Functions (ALPHA Phase A - Critical Priority) - 0xDA-0xFF range
            // Trigonometric functions (0xDA-0xE2)
            EXT_FUNC_SIN = 0xDA,           // SIN(x) - sine in radians
            EXT_FUNC_COS = 0xDB,           // COS(x) - cosine in radians
            EXT_FUNC_TAN = 0xDC,           // TAN(x) - tangent in radians
            EXT_FUNC_ASIN = 0xDD,          // ASIN(x) - arc sine, returns radians
            EXT_FUNC_ACOS = 0xDE,          // ACOS(x) - arc cosine, returns radians
            EXT_FUNC_ATAN = 0xDF,          // ATAN(x) - arc tangent, returns radians
            EXT_FUNC_ATAN2 = 0xE0,         // ATAN2(y, x) - arc tangent of y/x, returns radians

            // Angle conversion functions (0xE1-0xE3)
            EXT_FUNC_DEGREES = 0xE1,       // DEGREES(radians) - convert radians to degrees
            EXT_FUNC_RADIANS = 0xE2,       // RADIANS(degrees) - convert degrees to radians
            EXT_FUNC_PI = 0xE3,            // PI() - returns π (3.14159265358979323846)

            // Hyperbolic trigonometric functions (Alpha 1 - Missing Functions) (0x59-0x5F)
            EXT_FUNC_SINH = 0x59,          // SINH(x) - hyperbolic sine
            EXT_FUNC_COSH = 0x5A,          // COSH(x) - hyperbolic cosine
            EXT_FUNC_TANH = 0x5B,          // TANH(x) - hyperbolic tangent
            EXT_FUNC_ASINH = 0x5C,         // ASINH(x) - inverse hyperbolic sine
            EXT_FUNC_ACOSH = 0x5D,         // ACOSH(x) - inverse hyperbolic cosine
            EXT_FUNC_ATANH = 0x5E,         // ATANH(x) - inverse hyperbolic tangent
            EXT_FUNC_COT = 0x5F,           // COT(x) - cotangent

            // Algebraic functions (0xE4-0xEE)
            EXT_FUNC_ABS = 0xE4,           // ABS(x) - absolute value
            EXT_FUNC_SIGN = 0xE5,          // SIGN(x) - sign of number (-1, 0, or 1)
            EXT_FUNC_ROUND = 0xE6,         // ROUND(x [, precision]) - round to nearest integer or decimal places
            EXT_FUNC_CEIL = 0xE7,          // CEIL(x) / CEILING(x) - round up to nearest integer
            EXT_FUNC_FLOOR = 0xE8,         // FLOOR(x) - round down to nearest integer
            EXT_FUNC_TRUNC = 0xE9,         // TRUNC(x [, precision]) - truncate toward zero
            EXT_FUNC_MOD = 0xEA,           // MOD(x, y) - modulo (remainder of x/y)
            EXT_FUNC_SQRT = 0xEB,          // SQRT(x) - square root
            EXT_FUNC_CBRT = 0xEC,          // CBRT(x) - cube root
            EXT_FUNC_POWER = 0xED,         // POWER(x, y) / POW(x, y) - x raised to power y
            EXT_FUNC_EXP = 0xEE,           // EXP(x) - e raised to power x

            // Logarithmic functions (0xEF-0xF2)
            EXT_FUNC_LN = 0xEF,            // LN(x) - natural logarithm (base e)
            EXT_FUNC_LOG = 0xF0,           // LOG(x) / LOG(base, x) - logarithm (base 10 or specified base)
            EXT_FUNC_LOG10 = 0xF1,         // LOG10(x) - base-10 logarithm
            EXT_FUNC_LOG2 = 0xF2,          // LOG2(x) - base-2 logarithm

            // Statistical functions (0xF3-0xF8)
            EXT_STDDEV_SAMP = 0xF3,        // STDDEV / STDDEV_SAMP(expr) - sample standard deviation
            EXT_STDDEV_POP = 0xF4,         // STDDEV_POP(expr) - population standard deviation
            EXT_VAR_SAMP = 0xF5,           // VARIANCE / VAR_SAMP(expr) - sample variance
            EXT_VAR_POP = 0xF6,            // VAR_POP(expr) - population variance
            EXT_CORR = 0xF7,               // CORR(y, x) - Pearson correlation coefficient
            EXT_COVAR_POP = 0xF8,          // COVAR_POP(y, x) - population covariance

            // Cryptographic hash functions (0xF9-0xFC)
            EXT_MD5 = 0xF9,                // MD5(data) - 128-bit hash
            EXT_SHA1 = 0xFA,               // SHA1(data) - 160-bit hash
            EXT_SHA256 = 0xFB,             // SHA256(data) - 256-bit hash
            EXT_SHA512 = 0xFC,             // SHA512(data) - 512-bit hash

            // Encoding functions (0xFD-0xFE)
            EXT_ENCODE = 0xFD,             // ENCODE(data, format) - encode binary to text
            EXT_DECODE = 0xFE,             // DECODE(text, format) - decode text to binary

            // SHOW/DESCRIBE commands (ALPHA Phase 1 - Developer Experience) - 0x05-0x09 range
            // Note: These are extended opcodes, prefixed with EXTENDED_OPCODE (0xFF)
            // Bytecode format: [0xFF] [EXT_SHOW_*]
            EXT_SHOW_TABLES = 0x05,        // SHOW TABLES [FROM database] [LIKE 'pattern']
            EXT_SHOW_DATABASES = 0x06,     // SHOW DATABASES [LIKE 'pattern']
            EXT_SHOW_COLUMNS = 0x07,       // SHOW COLUMNS FROM table [LIKE 'pattern']
            EXT_SHOW_INDEXES = 0x08,       // SHOW INDEXES FROM table
            EXT_SHOW_CREATE_TABLE = 0x09,  // SHOW CREATE TABLE table
            EXT_DESCRIBE_TABLE = 0x15,     // DESCRIBE table (alias for SHOW COLUMNS)

            // Note: 0xFF is EXTENDED_OPCODE marker (already defined above)

            // Index Operations (0x0A-0x14) - Direct index manipulation operations
            //
            // PARAMETER ENCODING:
            // -----------------
            // All index operations use EXTENDED_OPCODE (0xFF) prefix followed by the specific opcode.
            //
            // Common parameter patterns:
            //   - table_id: uint32_t (4 bytes, little-endian)
            //   - index_id: uint32_t (4 bytes, little-endian)
            //   - index_type: IndexType enum (1 byte)
            //   - tid: uint64_t (8 bytes, little-endian) - TID of heap tuple
            //   - xmin/xmax: uint64_t (8 bytes, little-endian) - Transaction IDs for MGA visibility
            //   - key: Variable length serialized key (format depends on data type)
            //     * Format: type_marker (1 byte) + length (4 bytes) + data (N bytes)
            //
            // USAGE EXAMPLES:
            // --------------
            // 1. EXT_INDEX_INSERT:
            //    Bytecode: [0xFF] [0x0A] [table_id:4] [index_id:4] [tid:8] [xmin:8] [key_len:4] [key_data:N]
            //    Example: Insert key "foo" (tid=1000, xmin=42) into index 5 of table 10
            //      0xFF 0x0A 0x0A 0x00 0x00 0x00  0x05 0x00 0x00 0x00  0xE8 0x03 0x00 0x00 0x00 0x00 0x00 0x00
            //      0x2A 0x00 0x00 0x00 0x00 0x00 0x00 0x00  0x03 0x00 0x00 0x00  0x66 0x6F 0x6F
            //
            // 2. EXT_INDEX_SEARCH:
            //    Bytecode: [0xFF] [0x0B] [table_id:4] [index_id:4] [current_xid:8] [key_len:4] [key_data:N]
            //    Returns: Array of TIDs (count:4 + (tid:8)*N)
            //    Example: Search for key "bar" with xid=100
            //      0xFF 0x0B 0x0A 0x00 0x00 0x00  0x05 0x00 0x00 0x00  0x64 0x00 0x00 0x00 0x00 0x00 0x00 0x00
            //      0x03 0x00 0x00 0x00  0x62 0x61 0x72
            //
            // 3. EXT_INDEX_SCAN (Range Scan):
            //    Bytecode: [0xFF] [0x0C] [table_id:4] [index_id:4] [current_xid:8] [start_key_len:4] [start_key:N] [end_key_len:4] [end_key:M]
            //    Note: Use length=0 for unbounded start/end
            //    Example: Scan from "a" to "z"
            //      0xFF 0x0C 0x0A 0x00 0x00 0x00  0x05 0x00 0x00 0x00  0x64 0x00 0x00 0x00 0x00 0x00 0x00 0x00
            //      0x01 0x00 0x00 0x00  0x61  0x01 0x00 0x00 0x00  0x7A
            //
            // 4. EXT_INDEX_UPDATE:
            //    Bytecode: [0xFF] [0x1C] [table_id:4] [index_id:4] [tid:8] [xmin:8] [old_key_len:4] [old_key:N] [new_key_len:4] [new_key:M]
            //    Note: Only emitted when indexed column value changes
            //    Example: Update key from "old" to "new" (tid=1000, xmin=50)
            //      0xFF 0x1C 0x0A 0x00 0x00 0x00  0x05 0x00 0x00 0x00  0xE8 0x03 0x00 0x00 0x00 0x00 0x00 0x00
            //      0x32 0x00 0x00 0x00 0x00 0x00 0x00 0x00  0x03 0x00 0x00 0x00  0x6F 0x6C 0x64
            //      0x03 0x00 0x00 0x00  0x6E 0x65 0x77
            //
            // 5. EXT_INDEX_DELETE:
            //    Bytecode: [0xFF] [0x0D] [table_id:4] [index_id:4] [tid:8] [xmax:8] [key_len:4] [key_data:N]
            //    Note: Performs MGA logical deletion (sets xmax on index entry)
            //    Example: Delete key "foo" (tid=1000, xmax=60)
            //      0xFF 0x0D 0x0A 0x00 0x00 0x00  0x05 0x00 0x00 0x00  0xE8 0x03 0x00 0x00 0x00 0x00 0x00 0x00
            //      0x3C 0x00 0x00 0x00 0x00 0x00 0x00 0x00  0x03 0x00 0x00 0x00  0x66 0x6F 0x6F
            //
            // MGA COMPLIANCE REQUIREMENTS:
            // ---------------------------
            // - All index operations MUST use xmin/xmax for visibility tracking (Firebird MGA)
            // - DELETE operations MUST be logical (set xmax), not physical removal
            // - INSERT operations MUST set xmin to current transaction ID
            // - UPDATE operations MUST mark old entry with xmax and insert new entry with xmin
            // - Visibility checks MUST use TIP-based isVersionVisible(xmin, xmax, current_xid)
            // - NO PostgreSQL snapshot-based visibility (forbidden per MGA_RULES.md)
            //
            EXT_INDEX_INSERT = 0x0A,       // Insert entry into index (key, tid, xmin)
            EXT_INDEX_SEARCH = 0x0B,       // Search index for key (returns matching TIDs)
            EXT_INDEX_SCAN = 0x0C,         // Range scan index (start_key, end_key, returns TIDs)
            EXT_INDEX_DELETE = 0x0D,       // Delete entry from index (key, tid, xmax - MGA logical deletion)
            EXT_INDEX_TYPE = 0x0E,         // Index type marker (btree, hash, gin, etc.)
            EXT_INDEX_SCAN_START = 0x0F,   // Start index scan (returns scan_id)
            EXT_INDEX_SCAN_NEXT = 0x10,    // Get next from index scan (scan_id)
            EXT_INDEX_SCAN_END = 0x11,     // End index scan (scan_id)
            EXT_INDEX_VACUUM = 0x12,       // Vacuum index (remove dead entries)
            EXT_INDEX_STATS = 0x13,        // Get index statistics
            EXT_INDEX_REINDEX = 0x14,      // Rebuild index
            EXT_INDEX_UPDATE = 0x1C,       // Update index entry (old_key, new_key, tid, xmin - combines delete old + insert new)

            // Cursor Operations (0x1D-0x1F, 0x2E) - PSQL cursor support
            EXT_CURSOR_DECLARE = 0x1D,     // DECLARE cursor_name CURSOR FOR select_statement
            EXT_CURSOR_OPEN = 0x1E,        // OPEN cursor_name - Execute query and prepare for fetching
            EXT_CURSOR_FETCH = 0x1F,       // FETCH [direction] FROM cursor_name INTO variables

            // Specialized Index Operations (0x28-0x2F) - For indexes with unique APIs
            //
            // These opcodes handle indexes that require specialized parameters beyond the generic
            // index operations above. Each has a custom parameter encoding format.
            //
            // 1. EXT_GIN_INSERT:
            //    Bytecode: [0xFF] [0x28] [table_id:4] [index_id:4] [tid:8] [xmin:8] [extractor_id:2] [value_len:4] [value:N]
            //    Note: GIN indexes support multi-value columns (arrays, JSONB). extractor_id determines how to extract keys.
            //    Example: Insert array value [1,2,3] using array extractor (id=1)
            //      0xFF 0x28 0x0A 0x00 0x00 0x00  0x05 0x00 0x00 0x00  0xE8 0x03 0x00 0x00 0x00 0x00 0x00 0x00
            //      0x2A 0x00 0x00 0x00 0x00 0x00 0x00 0x00  0x01 0x00  [array_data]
            //
            // 2. EXT_GIN_SEARCH:
            //    Bytecode: [0xFF] [0x29] [table_id:4] [index_id:4] [current_xid:8] [extractor_id:2] [query_len:4] [query:N]
            //    Returns: Array of TIDs matching the query
            //    Example: Search for array containing value 2
            //      0xFF 0x29 0x0A 0x00 0x00 0x00  0x05 0x00 0x00 0x00  0x64 0x00 0x00 0x00 0x00 0x00 0x00 0x00
            //      0x01 0x00  [query_data]
            //
            // 3. EXT_HNSW_INSERT:
            //    Bytecode: [0xFF] [0x2A] [table_id:4] [index_id:4] [tid:8] [xmin:8] [vector_dim:4] [vector_data:dim*4]
            //    Note: HNSW is for vector similarity search. vector_data is array of float32 values.
            //    Example: Insert 3D vector [0.1, 0.2, 0.3]
            //      0xFF 0x2A 0x0A 0x00 0x00 0x00  0x05 0x00 0x00 0x00  0xE8 0x03 0x00 0x00 0x00 0x00 0x00 0x00
            //      0x2A 0x00 0x00 0x00 0x00 0x00 0x00 0x00  0x03 0x00 0x00 0x00  [float32 x 3]
            //
            // 4. EXT_HNSW_SEARCH:
            //    Bytecode: [0xFF] [0x2B] [table_id:4] [index_id:4] [current_xid:8] [k:4] [vector_dim:4] [vector_data:dim*4]
            //    Returns: k nearest neighbors (count:4 + (tid:8 + distance:4)*k)
            //    Example: Find 10 nearest neighbors to vector [0.5, 0.6, 0.7]
            //      0xFF 0x2B 0x0A 0x00 0x00 0x00  0x05 0x00 0x00 0x00  0x64 0x00 0x00 0x00 0x00 0x00 0x00 0x00
            //      0x0A 0x00 0x00 0x00  0x03 0x00 0x00 0x00  [float32 x 3]
            //
            // 5. EXT_COLUMNSTORE_INSERT:
            //    Bytecode: [0xFF] [0x2C] [table_id:4] [index_id:4] [tid:8] [xmin:8] [column_count:2] [column_data:N]
            //    Note: Columnstore appends data to columnar segments. column_data contains all column values.
            //    Example: Insert row with 2 columns
            //      0xFF 0x2C 0x0A 0x00 0x00 0x00  0x05 0x00 0x00 0x00  0xE8 0x03 0x00 0x00 0x00 0x00 0x00 0x00
            //      0x2A 0x00 0x00 0x00 0x00 0x00 0x00 0x00  0x02 0x00  [column1] [column2]
            //
            // 6. EXT_COLUMNSTORE_SCAN:
            //    Bytecode: [0xFF] [0x2D] [table_id:4] [index_id:4] [current_xid:8] [column_id:2] [predicate_len:4] [predicate:N]
            //    Returns: Array of TIDs matching the predicate on specified column
            //    Example: Scan column 1 for values > 100
            //      0xFF 0x2D 0x0A 0x00 0x00 0x00  0x05 0x00 0x00 0x00  0x64 0x00 0x00 0x00 0x00 0x00 0x00 0x00
            //      0x01 0x00  [predicate_data]
            //
            EXT_GIN_INSERT = 0x28,         // GIN insert (value, tid, xmin, extractor_id)
            EXT_GIN_SEARCH = 0x29,         // GIN search (query, current_xid, extractor_id)
            EXT_HNSW_INSERT = 0x2A,        // HNSW insert (vector, tid, xmin)
            EXT_HNSW_SEARCH = 0x2B,        // HNSW k-NN search (vector, k, current_xid)
            EXT_COLUMNSTORE_INSERT = 0x2C, // Columnstore insert column
            EXT_COLUMNSTORE_SCAN = 0x2D,   // Columnstore scan column
            EXT_CURSOR_CLOSE = 0x2E,       // CLOSE cursor_name - Close cursor and free resources

            // IN value list opcodes (WP-6 PARSE-3)
            EXT_IN_LIST = 0x2F,            // IN (val1, val2, ...) - membership test in value list

            // Bit manipulation - Byte/Bit access (0x06-0x09)
            EXT_GET_BYTE = 0x06,           // GET_BYTE(bytes, offset) - extract byte at offset
            EXT_SET_BYTE = 0x07,           // SET_BYTE(bytes, offset, value) - set byte at offset
            EXT_GET_BIT = 0x08,            // GET_BIT(bytes, bit_offset) - get bit at offset
            EXT_SET_BIT = 0x09,            // SET_BIT(bytes, bit_offset, value) - set bit at offset

            // Bit manipulation - Bitwise operations (0x15-0x1B)
            EXT_BIT_AND = 0x15,            // BIT_AND(a, b) / a & b - bitwise AND
            EXT_BIT_OR = 0x16,             // BIT_OR(a, b) / a | b - bitwise OR
            EXT_BIT_XOR = 0x17,            // BIT_XOR(a, b) / a ^ b - bitwise XOR
            EXT_BIT_NOT = 0x18,            // BIT_NOT(a) / ~a - bitwise NOT (complement)
            EXT_BIT_SHIFT_LEFT = 0x19,     // BIT_SHIFT_LEFT(a, n) / a << n - left shift
            EXT_BIT_SHIFT_RIGHT = 0x1A,    // BIT_SHIFT_RIGHT(a, n) / a >> n - arithmetic right shift
            EXT_BIT_SHIFT_RIGHT_LOGICAL = 0x1B, // a >>> n - logical right shift (zero-fill)

            // Bit manipulation - Utility functions (0x25-0x27)
            EXT_BIT_COUNT = 0x25,          // BIT_COUNT(a) - count set bits (popcount)
            EXT_BIT_LENGTH = 0x26,         // BIT_LENGTH(bytes) - length in bits
            EXT_BIT_MASK = 0x27,           // BIT_MASK(length) - create mask of N ones

            // XML functions (0x45-0x49)
            EXT_XMLPARSE = 0x45,           // XMLPARSE(document_or_content, xml_text)
            EXT_XMLSERIALIZE = 0x46,       // XMLSERIALIZE(content_or_document xml AS type)
            EXT_XMLELEMENT = 0x47,         // XMLELEMENT(name, content)
            EXT_XMLCONCAT = 0x48,          // XMLCONCAT(xml, ...)
            EXT_XMLFOREST = 0x49,          // XMLFOREST(expr AS name, ...)
            EXT_XMLCOMMENT = 0x4A,         // XMLCOMMENT(text) - create XML comment
            EXT_XMLROOT = 0x4B,            // XMLROOT(xml, VERSION version [, STANDALONE yes|no])
            EXT_XPATH = 0x4C,              // XPATH(xpath_expr, xml) - extract nodes using XPath
            EXT_XMLEXISTS = 0x4D,          // XMLEXISTS(xpath_expr, xml) - check if XPath matches
            EXT_XMLAGG = 0x4E,             // XMLAGG(xml) - aggregate XML values (aggregate function)

            // MERGE statement support (Alpha 1 - Advanced SQL) (0x4F-0x55)
            EXT_MERGE_START = 0x4F,        // Begin MERGE operation
            EXT_MERGE_SOURCE = 0x50,       // Source query/table specification
            EXT_MERGE_ON = 0x51,           // ON condition (join predicate)
            EXT_MERGE_WHEN_MATCHED = 0x52, // WHEN MATCHED THEN UPDATE
            EXT_MERGE_WHEN_NOT_MATCHED = 0x53, // WHEN NOT MATCHED THEN INSERT
            EXT_MERGE_WHEN_NOT_MATCHED_SOURCE = 0x54, // WHEN NOT MATCHED BY SOURCE THEN DELETE
            EXT_MERGE_END = 0x55,          // End MERGE operation

            // RETURNING clause support (Alpha 1 - Advanced SQL) (0x56)
            EXT_RETURNING = 0x56,          // RETURNING clause marker (followed by column list or *)

            // Statistics and query optimization (P1-10) - 0x57 range
            EXT_ANALYZE = 0x57,            // ANALYZE table_name [COLUMN column_name] [SAMPLE sample_rate]

            // SAVEPOINT support (Alpha Phase 1 - SAVEPOINT) - 0x58-0x5A range
            EXT_SAVEPOINT = 0x58,          // SAVEPOINT savepoint_name
            EXT_RELEASE_SAVEPOINT = 0x59,  // RELEASE SAVEPOINT savepoint_name
            EXT_ROLLBACK_TO_SAVEPOINT = 0x5A, // ROLLBACK TO SAVEPOINT savepoint_name

            // User Defined Types (Alpha Phase 1 - UDT) - 0x5B-0x5C range
            EXT_CREATE_TYPE = 0x5B,        // CREATE TYPE type_name AS {ENUM|RANGE|composite}
            EXT_CREATE_DOMAIN = 0x5C,      // CREATE DOMAIN domain_name AS type [constraints]

            // Procedure invocation (PSQL) - 0x5D
            EXT_CALL = 0x5D,               // CALL procedure_name(args...) - invoke stored procedure

            // Extended SHOW commands (Firebird ISQL compatibility) - 0x5E-0x72 range
            // Note: These are extended opcodes, prefixed with EXTENDED_OPCODE (0xFF)
            EXT_SHOW_TABLE = 0x5E,         // SHOW TABLE object_name - detailed table info
            EXT_SHOW_INDEX = 0x5F,         // SHOW INDEX object_name - detailed index info
            EXT_SHOW_TRIGGER = 0x60,       // SHOW TRIGGER object_name - trigger definition
            EXT_SHOW_PROCEDURE = 0x61,     // SHOW PROCEDURE object_name - procedure definition
            EXT_SHOW_FUNCTION = 0x62,      // SHOW FUNCTION object_name - function definition
            EXT_SHOW_VIEW = 0x63,          // SHOW VIEW object_name - view definition
            EXT_SHOW_DOMAIN = 0x64,        // SHOW DOMAIN object_name - domain definition
            EXT_SHOW_GENERATOR = 0x65,     // SHOW GENERATOR object_name - sequence/generator info
            EXT_SHOW_SCHEMA = 0x66,        // SHOW SCHEMA [object_name] - schema listing or details
            EXT_SHOW_ROLE = 0x67,          // SHOW ROLE object_name - role definition
            EXT_SHOW_GRANTS = 0x68,        // SHOW GRANTS [FOR object_name] - privilege grants
            EXT_SHOW_CHECKS = 0x69,        // SHOW CHECKS object_name - check constraints
            EXT_SHOW_COLLATIONS = 0x6A,    // SHOW COLLATIONS [LIKE pattern] - available collations
            EXT_SHOW_COMMENTS = 0x6B,      // SHOW COMMENTS object_name - object comments/remarks
            EXT_SHOW_DEPENDENCIES = 0x6C,  // SHOW DEPENDENCIES object_name - object dependencies
            EXT_SHOW_PACKAGE = 0x6D,       // SHOW PACKAGE object_name - package definition
            EXT_SHOW_SYSTEM = 0x6E,        // SHOW SYSTEM - system information
            EXT_SHOW_SQL_DIALECT = 0x6F,   // SHOW SQL DIALECT - current SQL dialect
            EXT_SHOW_VERSION = 0x70,       // SHOW VERSION - database version
            EXT_SHOW_DATABASE = 0x71,      // SHOW DATABASE - current database info

            // Session SET commands (Firebird ISQL compatibility) - 0x72-0x74 range
            EXT_SET_SQL_DIALECT = 0x72,    // SET SQL DIALECT n - set SQL dialect (1, 2, or 3)
            EXT_SET_NAMES = 0x73,          // SET NAMES charset_name - set connection charset
            EXT_SET_LOCAL_TIMEOUT = 0x74,  // SET LOCAL_TIMEOUT n - set statement timeout in seconds

            // Schema navigation SHOW commands - 0x75-0x7A range
            EXT_SHOW_SCHEMA_PATH = 0x75,   // SHOW SCHEMA PATH - full path to current schema
            EXT_SHOW_SCHEMA_TREE = 0x76,   // SHOW SCHEMA TREE [DEPTH n] - schema hierarchy
            EXT_SHOW_SEARCH_PATH = 0x77,   // SHOW SEARCH PATH - current search path
            EXT_SHOW_LOCATION = 0x78,      // SHOW LOCATION OF [type] name - find object in search path
            EXT_SHOW_RESOLVED = 0x79,      // SHOW RESOLVED name - which object search path resolves to
            EXT_SHOW_OBJECTS = 0x7A,       // SHOW OBJECTS - all objects in current/specified schema

            // Parser V2 SHOW variants (Session/GUC support)
            EXT_SHOW_VARIABLE = 0x7B,      // SHOW variable_name - show session variable value
            EXT_SHOW_ALL = 0x7C,           // SHOW ALL - show all session variables
            EXT_SHOW_TRANSACTION_LEVEL = 0x7D, // SHOW TRANSACTION ISOLATION LEVEL

            // Parser V2 completeness opcodes (0x80-0x8F range)
            EXT_SELECT_TABLE_STAR = 0x80,      // SELECT t.* - qualified table star (followed by table UUID)
            EXT_SET_VARIABLE = 0x81,           // SET variable = value - generic session variable
            EXT_ON_CONFLICT = 0x82,            // INSERT ... ON CONFLICT marker
            EXT_ON_CONFLICT_COLUMN = 0x83,     // ON CONFLICT (column, ...) - conflict target columns
            EXT_ON_CONFLICT_CONSTRAINT = 0x84, // ON CONFLICT ON CONSTRAINT name
            EXT_ON_CONFLICT_DO_NOTHING = 0x85, // ON CONFLICT DO NOTHING
            EXT_ON_CONFLICT_DO_UPDATE = 0x86,  // ON CONFLICT DO UPDATE SET
            EXT_ON_CONFLICT_WHERE = 0x87,      // ON CONFLICT ... WHERE condition

            // Permission management (GRANT/REVOKE) (0x88-0x8F)
            EXT_GRANT = 0x88,                  // GRANT privileges ON object TO grantee
            EXT_REVOKE = 0x89,                 // REVOKE privileges ON object FROM grantee
            EXT_GRANT_OPTION = 0x8A,           // WITH GRANT OPTION
            EXT_PRIVILEGE = 0x8B,              // Privilege type (SELECT, INSERT, UPDATE, DELETE, etc.)

            // Connection statements (0x90-0x93)
            EXT_CONNECT = 0x90,                // CONNECT TO database
            EXT_DISCONNECT = 0x91,             // DISCONNECT [ALL | CURRENT | connection_name]

            // Metadata statements (0x94-0x97)
            EXT_COMMENT = 0x94,                // COMMENT ON object IS 'text'
        };

        // Extended opcodes (16-bit). Values match existing EXT_* assignments.
        enum class ExtendedOpcode : uint16_t
        {
            EXT_ARRAY_APPEND = 0x01,  // ARRAY_APPEND(array, element)
            EXT_ARRAY_PREPEND = 0x02,  // ARRAY_PREPEND(element, array)
            EXT_ARRAY_CAT = 0x03,  // ARRAY_CAT(array1, array2)
            EXT_ARRAY_REMOVE = 0x04,  // ARRAY_REMOVE(array, element)
            EXT_ARRAY_REPLACE = 0x05,  // ARRAY_REPLACE(array, from, to)
            EXT_ARRAY_OVERLAP = 0x10,  // && - array overlap (has common elements)
            EXT_ARRAY_CONTAINS = 0x11,  // @> - array contains (left contains all of right)
            EXT_ARRAY_CONTAINED_BY = 0x12,  // <@ - array is contained by (left subset of right)
            EXT_ARRAY_EQ = 0x13,  // = - array equality
            EXT_ARRAY_NE = 0x14,  // <> - array inequality
            EXT_ARRAY_LENGTH = 0x20,  // ARRAY_LENGTH(array, dimension)
            EXT_ARRAY_DIMS = 0x21,  // ARRAY_DIMS(array) - dimensions as text
            EXT_ARRAY_UPPER = 0x22,  // ARRAY_UPPER(array, dimension) - upper bound
            EXT_ARRAY_LOWER = 0x23,  // ARRAY_LOWER(array, dimension) - lower bound
            EXT_ARRAY_CONSTRUCT = 0x24,  // Construct array from stack elements
            EXT_ARRAY_SUBSCRIPT = 0x25,  // Array subscript access (array[index])
            EXT_REGEX_MATCH = 0x30,  // ~ operator (regex match case-sensitive)
            EXT_REGEX_MATCH_CI = 0x31,  // ~* operator (regex match case-insensitive)
            EXT_REGEX_NOT_MATCH = 0x32,  // !~ operator (regex not match case-sensitive)
            EXT_REGEX_NOT_MATCH_CI = 0x33,  // !~* operator (regex not match case-insensitive)
            EXT_REGEXP_MATCHES = 0x34,  // REGEXP_MATCHES(str, pattern [, flags])
            EXT_REGEXP_REPLACE = 0x35,  // REGEXP_REPLACE(str, pattern, replacement [, flags])
            EXT_REGEXP_SPLIT_TO_TABLE = 0x36,  // REGEXP_SPLIT_TO_TABLE(str, pattern [, flags])
            EXT_REGEXP_SPLIT_TO_ARRAY = 0x37,  // REGEXP_SPLIT_TO_ARRAY(str, pattern [, flags])
            EXT_SPLIT_PART = 0x38,  // SPLIT_PART(str, delimiter, field)
            EXT_STRING_TO_TABLE = 0x39,  // STRING_TO_TABLE(str, delimiter)
            EXT_UNNEST_TEXT = 0x3A,  // UNNEST_TEXT(text_array)
            EXT_STRPOS = 0x3B,  // STRPOS(str, substring)
            EXT_POSITION = 0x3C,  // POSITION(substring IN string)
            EXT_OVERLAY = 0x3D,  // OVERLAY(str PLACING newstr FROM start [FOR length])
            EXT_QUOTE_LITERAL = 0x3E,  // QUOTE_LITERAL(str)
            EXT_QUOTE_IDENT = 0x3F,  // QUOTE_IDENT(str)
            EXT_INITCAP = 0x40,  // INITCAP(str) - capitalize first letter of each word
            EXT_ASCII = 0x41,  // ASCII(str) - get ASCII code of first character
            EXT_CHR = 0x42,  // CHR(code) - convert ASCII code to character
            EXT_REPEAT = 0x43,  // REPEAT(str, count)
            EXT_REVERSE = 0x44,  // REVERSE(str)
            EXT_GROUP_ROLLUP = 0x45,  // ROLLUP(...) - hierarchical grouping
            EXT_GROUP_CUBE = 0x46,  // CUBE(...) - all combinations grouping
            EXT_GROUP_GROUPING_SETS = 0x47,  // GROUPING SETS(...) - explicit grouping sets
            EXT_GROUPING_FUNC = 0x48,  // GROUPING(column) - identify aggregated columns
            EXT_LPAD = 0x57,  // LPAD(str, length [, fill]) - left-pad string
            EXT_RPAD = 0x58,  // RPAD(str, length [, fill]) - right-pad string
            EXT_TYPE_POINT = 0x50,  // POINT data type marker
            EXT_TYPE_LINESTRING = 0x51,  // LINESTRING data type marker
            EXT_TYPE_POLYGON = 0x52,  // POLYGON data type marker
            EXT_ST_POINT = 0x53,  // ST_Point(x, y) - create point
            EXT_ST_MAKELINE = 0x54,  // ST_MakeLine(...) - create linestring
            EXT_ST_MAKEPOLYGON = 0x55,  // ST_MakePolygon(...) - create polygon
            EXT_ST_ASTEXT = 0x56,  // ST_AsText(geom) - WKT output
            EXT_ST_ASBINARY = 0x57,  // ST_AsBinary(geom) - WKB output
            EXT_ST_GEOMETRYTYPE = 0x58,  // ST_GeometryType(geom) - type name
            EXT_ST_ISVALID = 0x59,  // ST_IsValid(geom) - validation check
            EXT_ST_BUFFER = 0x5A,  // ST_Buffer(geom, distance) - create buffer polygon
            EXT_ST_CONVEXHULL = 0x5B,  // ST_ConvexHull(geom) - convex hull polygon
            EXT_ST_ENVELOPE = 0x5C,  // ST_Envelope(geom) - bounding box polygon
            EXT_ST_INTERSECTS = 0x5D,  // ST_Intersects(geom1, geom2) - do geometries intersect?
            EXT_ST_CONTAINS = 0x5E,  // ST_Contains(geom1, geom2) - does geom1 contain geom2?
            EXT_ST_WITHIN = 0x5F,  // ST_Within(geom1, geom2) - is geom1 within geom2?
            EXT_ST_EQUALS = 0x63,  // ST_Equals(geom1, geom2) - are geometries spatially equal?
            EXT_CTE_DEF = 0x60,  // CTE definition marker
            EXT_CTE_SCAN = 0x61,  // CTE scan operation
            EXT_WITH_CLAUSE = 0x62,  // WITH clause marker
            EXT_UNION = 0x64,  // UNION (removes duplicates)
            EXT_UNION_ALL = 0x65,  // UNION ALL (keeps duplicates)
            EXT_INTERSECT = 0x66,  // INTERSECT (removes duplicates)
            EXT_INTERSECT_ALL = 0x67,  // INTERSECT ALL (keeps duplicates)
            EXT_EXCEPT = 0x68,  // EXCEPT (removes duplicates)
            EXT_EXCEPT_ALL = 0x69,  // EXCEPT ALL (keeps duplicates)
            EXT_WIN_CUME_DIST = 0x6A,  // CUME_DIST() - cumulative distribution
            EXT_WIN_PERCENT_RANK = 0x6B,  // PERCENT_RANK() - relative rank percentile
            EXT_FUNC_AGE = 0x6C,  // AGE(timestamp [, timestamp]) - age between timestamps
            EXT_FUNC_FORMAT_TYPE = 0x0300,  // FORMAT_TYPE(type_oid, typmod)
            EXT_FUNC_OBJ_DESCRIPTION = 0x0301,  // OBJ_DESCRIPTION(oid, catalog_name)
            EXT_FUNC_COL_DESCRIPTION = 0x0302,  // COL_DESCRIPTION(rel_oid, attnum)
            EXT_FUNC_SHOBJ_DESCRIPTION = 0x0303,  // SHOBJ_DESCRIPTION(oid, catalog_name)
            EXT_FUNC_LTRIM = 0x0310,  // LTRIM(str)
            EXT_FUNC_RTRIM = 0x0311,  // RTRIM(str)
            EXT_FUNC_CONCAT = 0x0312,  // CONCAT(arg1, ...)
            EXT_FUNC_CONCAT_WS = 0x0313,  // CONCAT_WS(sep, arg1, ...)
            EXT_FUNC_CURRENT_TIME = 0x0314,  // CURRENT_TIME()
            EXT_EXPR_FUNCTION_CALL = 0x0315,  // Expression-level stored/UDR function call
            EXT_ALTER_ELEMENT = 0x0316,  // ALTER_ELEMENT(element IN value TO new_value)
            EXT_ALTER_INDEX = 0x0317,  // ALTER INDEX index_name {ACTIVE|INACTIVE|SET (...) }
            EXT_CREATE_EXCEPTION_STMT = 0x0318,  // CREATE EXCEPTION exception_name 'message'
            EXT_TABLE_OPTIONS = 0x0319,  // CREATE TABLE table options payload (MySQL emulation)
            EXT_DROP_EXCEPTION_STMT = 0x031A,  // DROP EXCEPTION [IF EXISTS] exception_name
            EXT_TABLE_PARTITIONING = 0x031B,  // CREATE TABLE partitioning metadata payload
            EXT_TABLE_INHERITS = 0x031C,  // CREATE TABLE inheritance metadata payload
            EXT_DISTINCT_ON = 0x031D,  // DISTINCT ON expression payload
            EXT_CREATE_TABLE_AS = 0x031E,  // CREATE TABLE AS SELECT payload
            EXT_CREATE_DB_TRIGGER = 0x6D,  // CREATE TRIGGER (database trigger)
            EXT_DROP_DB_TRIGGER = 0x6E,  // DROP TRIGGER (database trigger)
            EXT_FIRE_DB_TRIGGER = 0x6F,  // Internal: Fire database trigger
            EXT_CREATE_TRIGGER = 0x70,  // CREATE TRIGGER
            EXT_DROP_TRIGGER = 0x71,  // DROP TRIGGER
            EXT_FIRE_TRIGGER = 0x72,  // Internal: Fire trigger (used by executor)
            EXT_SUBQUERY_SCALAR = 0x73,  // Scalar subquery (returns single value)
            EXT_SUBQUERY_EXISTS = 0x74,  // EXISTS subquery (returns boolean)
            EXT_SUBQUERY_IN = 0x75,  // IN subquery (membership test)
            EXT_SUBQUERY_NOT_IN = 0x76,  // NOT IN subquery (negated membership)
            EXT_SUBQUERY_END = 0x77,  // End of subquery marker
            EXT_SUBQUERY_ARRAY = 0x5B,  // WP-6 PARSE-M3: ARRAY(SELECT ...) subquery
            EXT_ST_DISJOINT = 0x78,  // ST_Disjoint(geom1, geom2) - are geometries disjoint?
            EXT_ST_OVERLAPS = 0x79,  // ST_Overlaps(geom1, geom2) - do geometries overlap?
            EXT_ST_TOUCHES = 0x7A,  // ST_Touches(geom1, geom2) - do geometries touch?
            EXT_ST_CROSSES = 0x7B,  // ST_Crosses(geom1, geom2) - do geometries cross?
            EXT_ST_INTERSECTION = 0x7C,  // ST_Intersection(geom1, geom2) - intersection geometry
            EXT_ST_UNION = 0x7D,  // ST_Union(geom1, geom2) - union geometry
            EXT_ST_DIFFERENCE = 0x7E,  // ST_Difference(geom1, geom2) - difference geometry (geom1 - geom2)
            EXT_ST_AREA = 0x7F,  // ST_Area(geom) - area of polygon
            EXT_ST_LENGTH = 0x80,  // ST_Length(geom) - length of linestring
            EXT_ST_DISTANCE = 0x81,  // ST_Distance(geom1, geom2) - distance between geometries
            EXT_ST_PERIMETER = 0x82,  // ST_Perimeter(geom) - perimeter of polygon
            EXT_ST_SRID = 0x83,  // ST_SRID(geom) - get SRID of geometry
            EXT_ST_SETSRID = 0x84,  // ST_SetSRID(geom, srid) - set SRID of geometry
            EXT_ST_TRANSFORM = 0x85,  // ST_Transform(geom, srid) - transform to different SRID
            EXT_ST_DISTANCE_SPHERE = 0x86,  // ST_Distance_Sphere(geom1, geom2) - geodetic distance
            EXT_ST_MULTIPOINT = 0x87,  // ST_MultiPoint(...) - create MULTIPOINT
            EXT_ST_MULTILINESTRING = 0x88,  // ST_MultiLineString(...) - create MULTILINESTRING
            EXT_ST_MULTIPOLYGON = 0x89,  // ST_MultiPolygon(...) - create MULTIPOLYGON
            EXT_ST_GEOMETRYCOLLECTION = 0x8A,  // ST_GeometryCollection(...) - create GEOMETRYCOLLECTION
            EXT_ST_COLLECT = 0x8B,  // ST_Collect(...) - collect geometries (alias for GeometryCollection)
            EXT_ST_GEOMETRYN = 0x8C,  // ST_GeometryN(geom, n) - get Nth geometry from collection
            EXT_ST_NUMGEOMETRIES = 0x8D,  // ST_NumGeometries(geom) - get number of geometries in collection
            EXT_ST_DUMP = 0x8E,  // ST_Dump(geom) - dump all geometries from collection
            EXT_EXTRACT = 0x8F,  // EXTRACT(field FROM value) - extract sub-information from complex types
            EXT_FUNCTION = 0x90,  // Function definition
            EXT_PROCEDURE = 0x91,  // Procedure definition
            EXT_BLOCK = 0x92,  // BEGIN...END block
            EXT_DECLARE = 0x93,  // Variable declaration
            EXT_ASSIGN = 0x94,  // Variable assignment
            EXT_IF = 0x95,  // IF statement
            EXT_ELSIF = 0x96,  // ELSIF clause
            EXT_ELSE = 0x97,  // ELSE clause
            EXT_LOOP = 0x98,  // LOOP statement
            EXT_WHILE = 0x99,  // WHILE loop
            EXT_EXIT = 0x9A,  // EXIT statement
            EXT_RETURN = 0x9B,  // RETURN statement
            EXT_RAISE = 0x9C,  // RAISE exception
            EXT_TRY = 0x9D,  // TRY block
            EXT_EXCEPT_HANDLER = 0x9E,  // EXCEPT handler (exception handling, not set operation)
            EXT_EXCEPTION_HANDLER = 0x9F,  // Exception handler definition
            EXT_JUMP_IF_TRUE = 0xA0,  // Conditional jump (true)
            EXT_JUMP_IF_FALSE = 0xA1,  // Conditional jump (false)
            EXT_JUMP = 0xA2,  // Unconditional jump
            EXT_LABEL = 0xA3,  // Label marker
            EXT_VAR_LOAD = 0xA4,  // Load variable value
            EXT_VAR_STORE = 0xA5,  // Store to variable
            EXT_PARAM_IN = 0xA6,  // IN parameter marker
            EXT_PARAM_OUT = 0xA7,  // OUT parameter marker
            EXT_PARAM_INOUT = 0xA8,  // INOUT parameter marker
            EXT_TSMATCH = 0xA9,  // @@ text search match operator (tsvector @@ tsquery)
            EXT_TS_RANK = 0xAA,  // TS_RANK(tsvector, tsquery) - relevance ranking
            EXT_TYPE_TSVECTOR = 0xAB,  // TSVECTOR data type marker
            EXT_TYPE_TSQUERY = 0xAC,  // TSQUERY data type marker
            // Extended scalar type markers (0x0400+ range)
            EXT_TYPE_INT128 = 0x0400,  // INT128 data type marker
            EXT_TYPE_UINT128 = 0x0401,  // UINT128 data type marker
            EXT_TYPE_VECTOR = 0x0402,  // VECTOR data type marker
            EXT_TYPE_UINT8 = 0x0410,  // UINT8 data type marker
            EXT_TYPE_UINT16 = 0x0411,  // UINT16 data type marker
            EXT_TYPE_UINT32 = 0x0412,  // UINT32 data type marker
            EXT_TYPE_UINT64 = 0x0413,  // UINT64 data type marker
            EXT_TYPE_MONEY = 0x0414,  // MONEY data type marker
            EXT_TYPE_INTERVAL = 0x0415,  // INTERVAL data type marker
            EXT_TYPE_JSONB = 0x0416,  // JSONB data type marker
            EXT_TYPE_JSONPATH = 0x0417,  // JSONPATH data type marker
            EXT_TYPE_XML = 0x0418,  // XML data type marker
            EXT_TYPE_MULTIPOINT = 0x0419,  // MULTIPOINT data type marker
            EXT_TYPE_MULTILINESTRING = 0x041A,  // MULTILINESTRING data type marker
            EXT_TYPE_MULTIPOLYGON = 0x041B,  // MULTIPOLYGON data type marker
            EXT_TYPE_GEOMETRYCOLLECTION = 0x041C,  // GEOMETRYCOLLECTION data type marker
            EXT_TYPE_COMPOSITE = 0x041D,  // COMPOSITE data type marker
            EXT_TYPE_VARIANT = 0x041E,  // VARIANT data type marker
            EXT_TYPE_INET = 0x041F,  // INET data type marker
            EXT_TYPE_CIDR = 0x0420,  // CIDR data type marker
            EXT_TYPE_MACADDR = 0x0421,  // MACADDR data type marker
            EXT_TYPE_MACADDR8 = 0x0422,  // MACADDR8 data type marker
            EXT_TYPE_TIME_TZ = 0x0423,  // TIME WITH TIME ZONE marker
            EXT_TYPE_TIMESTAMP_TZ = 0x0424,  // TIMESTAMP WITH TIME ZONE marker
            EXT_TYPE_DECFLOAT16 = 0x0425,  // DECFLOAT(16) data type marker
            EXT_TYPE_DECFLOAT34 = 0x0426,  // DECFLOAT(34) data type marker
            EXT_TO_TSVECTOR = 0xAD,  // TO_TSVECTOR(config, text) - text to tsvector
            EXT_TO_TSQUERY = 0xAE,  // TO_TSQUERY(config, query) - query to tsquery
            EXT_PLAINTO_TSQUERY = 0xAF,  // PLAINTO_TSQUERY(config, text) - plain text to query
            EXT_PHRASETO_TSQUERY = 0xB0,  // PHRASETO_TSQUERY(config, text) - phrase to query
            EXT_TYPE_INT4RANGE = 0xB1,  // INT4RANGE data type marker
            EXT_TYPE_INT8RANGE = 0xB2,  // INT8RANGE data type marker
            EXT_TYPE_NUMRANGE = 0xB3,  // NUMRANGE data type marker
            EXT_TYPE_DATERANGE = 0xB4,  // DATERANGE data type marker
            EXT_TYPE_TSRANGE = 0xB5,  // TSRANGE data type marker
            EXT_TYPE_TSTZRANGE = 0xB6,  // TSTZRANGE data type marker
            EXT_RANGE_CONSTRUCT = 0xB7,  // Construct range from bounds (lower, upper, bounds_type)
            EXT_RANGE_OVERLAPS = 0xB8,  // && - ranges overlap (same as EXT_ARRAY_OVERLAP semantics)
            EXT_RANGE_CONTAINS_RANGE = 0xB9,  // @> - range contains range (similar to array)
            EXT_RANGE_CONTAINS_ELEM = 0xBA,  // @> - range contains element
            EXT_RANGE_CONTAINED_BY = 0xBB,  // <@ - range contained by range
            EXT_RANGE_STRICTLY_LEFT = 0xBC,  // << - strictly left of
            EXT_RANGE_STRICTLY_RIGHT = 0xBD,  // >> - strictly right of
            EXT_RANGE_ADJACENT = 0xBE,  // -|- - adjacent to
            EXT_RANGE_UNION = 0xBF,  // + - range union
            EXT_RANGE_INTERSECTION = 0xC0,  // & - range intersection
            EXT_RANGE_DIFFERENCE = 0xC1,  // - - range difference
            EXT_RANGE_LOWER = 0xC2,  // LOWER(range) - get lower bound
            EXT_RANGE_UPPER = 0xC3,  // UPPER(range) - get upper bound
            EXT_RANGE_ISEMPTY = 0xC4,  // ISEMPTY(range) - check if range is empty
            EXT_RANGE_LOWER_INC = 0xC5,  // LOWER_INC(range) - check if lower bound is inclusive
            EXT_RANGE_UPPER_INC = 0xC6,  // UPPER_INC(range) - check if upper bound is inclusive
            EXT_RANGE_LOWER_INF = 0xC7,  // LOWER_INF(range) - check if lower bound is infinite
            EXT_RANGE_UPPER_INF = 0xC8,  // UPPER_INF(range) - check if upper bound is infinite
            EXT_RANGE_MERGE = 0xC9,  // RANGE_MERGE(r1, r2) - smallest range containing both
            EXT_CREATE_FUNCTION_STMT = 0x30,  // CREATE [OR REPLACE] FUNCTION
            EXT_CREATE_PROCEDURE_STMT = 0x31,  // CREATE [OR REPLACE] PROCEDURE
            EXT_CREATE_PACKAGE_STMT = 0x32,  // CREATE [OR REPLACE] PACKAGE
            EXT_DROP_FUNCTION_STMT = 0x33,  // DROP FUNCTION [IF EXISTS]
            EXT_DROP_PROCEDURE_STMT = 0x34,  // DROP PROCEDURE [IF EXISTS]
            EXT_DROP_PACKAGE_STMT = 0x35,  // DROP PACKAGE [IF EXISTS]
            EXT_CREATE_USER = 0xCA,  // CREATE USER username [WITH PASSWORD 'xxx'] [SUPERUSER]
            EXT_ALTER_USER = 0xCB,  // ALTER USER username [WITH PASSWORD 'xxx'] [SUPERUSER]
            EXT_DROP_USER = 0xCC,  // DROP USER username [IF EXISTS] [CASCADE | RESTRICT]
            EXT_CREATE_ROLE = 0xCD,  // CREATE ROLE rolename
            EXT_ALTER_ROLE = 0x02F0,  // ALTER ROLE rolename RENAME TO new_name
            EXT_DROP_ROLE = 0xCE,  // DROP ROLE rolename [IF EXISTS] [CASCADE | RESTRICT]
            EXT_CREATE_GROUP = 0xCF,  // CREATE GROUP groupname
            EXT_DROP_GROUP = 0xD0,  // DROP GROUP groupname [IF EXISTS] [CASCADE | RESTRICT]
            EXT_GRANT_PRIVILEGE = 0xD1,  // GRANT privilege ON object TO grantee [WITH GRANT OPTION]
            EXT_REVOKE_PRIVILEGE = 0xD2,  // REVOKE privilege ON object FROM grantee [CASCADE | RESTRICT]
            EXT_GRANT_ROLE = 0xD3,  // GRANT role TO user/role
            EXT_REVOKE_ROLE = 0xD4,  // REVOKE role FROM user/role [CASCADE | RESTRICT]
            EXT_SET_ROLE = 0xD5,  // SET ROLE rolename / RESET ROLE
            EXT_SET_SESSION_AUTH = 0xD6,  // SET SESSION AUTHORIZATION username / RESET SESSION AUTHORIZATION
            EXT_SET_CONSTRAINTS = 0x4F,  // P2-7: SET CONSTRAINTS {ALL | names} {DEFERRED | IMMEDIATE}
            EXT_CREATE_POLICY = 0xD7,  // CREATE POLICY policy_name ON table_name
            EXT_DROP_POLICY = 0xD8,  // DROP POLICY [IF EXISTS] policy_name ON table_name
            EXT_ALTER_TABLE_RLS = 0xD9,  // ALTER TABLE table_name {ENABLE|DISABLE|FORCE|NO FORCE} ROW LEVEL SECURITY
            EXT_ALTER_POLICY = 0xE5,  // ALTER POLICY policy_name ON table_name
            EXT_ALTER_DEFAULT_PRIVILEGES = 0x01A0, // ALTER DEFAULT PRIVILEGES
            EXT_FUNC_SIN = 0xDA,  // SIN(x) - sine in radians
            EXT_FUNC_COS = 0xDB,  // COS(x) - cosine in radians
            EXT_FUNC_TAN = 0xDC,  // TAN(x) - tangent in radians
            EXT_FUNC_ASIN = 0xDD,  // ASIN(x) - arc sine, returns radians
            EXT_FUNC_ACOS = 0xDE,  // ACOS(x) - arc cosine, returns radians
            EXT_FUNC_ATAN = 0xDF,  // ATAN(x) - arc tangent, returns radians
            EXT_FUNC_ATAN2 = 0xE0,  // ATAN2(y, x) - arc tangent of y/x, returns radians
            EXT_FUNC_DEGREES = 0xE1,  // DEGREES(radians) - convert radians to degrees
            EXT_FUNC_RADIANS = 0xE2,  // RADIANS(degrees) - convert degrees to radians
            EXT_FUNC_PI = 0xE3,  // PI() - returns π (3.14159265358979323846)
            EXT_FUNC_SINH = 0x59,  // SINH(x) - hyperbolic sine
            EXT_FUNC_COSH = 0x5A,  // COSH(x) - hyperbolic cosine
            EXT_FUNC_TANH = 0x5B,  // TANH(x) - hyperbolic tangent
            EXT_FUNC_ASINH = 0x5C,  // ASINH(x) - inverse hyperbolic sine
            EXT_FUNC_ACOSH = 0x5D,  // ACOSH(x) - inverse hyperbolic cosine
            EXT_FUNC_ATANH = 0x5E,  // ATANH(x) - inverse hyperbolic tangent
            EXT_FUNC_COT = 0x5F,  // COT(x) - cotangent
            EXT_FUNC_ABS = 0xE4,  // ABS(x) - absolute value
            EXT_FUNC_SIGN = 0xE5,  // SIGN(x) - sign of number (-1, 0, or 1)
            EXT_FUNC_ROUND = 0xE6,  // ROUND(x [, precision]) - round to nearest integer or decimal places
            EXT_FUNC_CEIL = 0xE7,  // CEIL(x) / CEILING(x) - round up to nearest integer
            EXT_FUNC_FLOOR = 0xE8,  // FLOOR(x) - round down to nearest integer
            EXT_FUNC_TRUNC = 0xE9,  // TRUNC(x [, precision]) - truncate toward zero
            EXT_FUNC_MOD = 0xEA,  // MOD(x, y) - modulo (remainder of x/y)
            EXT_FUNC_SQRT = 0xEB,  // SQRT(x) - square root
            EXT_DEBUG_SPAN = 0x0FF0,  // Debug: source line/column span marker
            EXT_FUNC_CBRT = 0xEC,  // CBRT(x) - cube root
            EXT_FUNC_POWER = 0xED,  // POWER(x, y) / POW(x, y) - x raised to power y
            EXT_FUNC_EXP = 0xEE,  // EXP(x) - e raised to power x
            EXT_FUNC_LN = 0xEF,  // LN(x) - natural logarithm (base e)
            EXT_FUNC_LOG = 0xF0,  // LOG(x) / LOG(base, x) - logarithm (base 10 or specified base)
            EXT_FUNC_LOG10 = 0xF1,  // LOG10(x) - base-10 logarithm
            EXT_FUNC_LOG2 = 0xF2,  // LOG2(x) - base-2 logarithm
            EXT_STDDEV_SAMP = 0xF3,  // STDDEV / STDDEV_SAMP(expr) - sample standard deviation
            EXT_STDDEV_POP = 0xF4,  // STDDEV_POP(expr) - population standard deviation
            EXT_VAR_SAMP = 0xF5,  // VARIANCE / VAR_SAMP(expr) - sample variance
            EXT_VAR_POP = 0xF6,  // VAR_POP(expr) - population variance
            EXT_CORR = 0xF7,  // CORR(y, x) - Pearson correlation coefficient
            EXT_COVAR_POP = 0xF8,  // COVAR_POP(y, x) - population covariance
            EXT_MD5 = 0xF9,  // MD5(data) - 128-bit hash
            EXT_SHA1 = 0xFA,  // SHA1(data) - 160-bit hash
            EXT_SHA256 = 0xFB,  // SHA256(data) - 256-bit hash
            EXT_SHA512 = 0xFC,  // SHA512(data) - 512-bit hash
            EXT_ENCODE = 0xFD,  // ENCODE(data, format) - encode binary to text
            EXT_DECODE = 0xFE,  // DECODE(text, format) - decode text to binary
            // Extended literal opcodes (0x0600+ range)
            EXT_LITERAL_JSONB = 0x0600,  // JSONB literal (string payload)
            EXT_LITERAL_INTERVAL = 0x0601,  // INTERVAL literal (string payload)
            EXT_LITERAL_MONEY = 0x0602,  // MONEY literal (string payload)
            EXT_LITERAL_INET = 0x0603,  // INET literal (string payload)
            EXT_LITERAL_CIDR = 0x0604,  // CIDR literal (string payload)
            EXT_LITERAL_MACADDR = 0x0605,  // MACADDR literal (string payload)
            EXT_LITERAL_MACADDR8 = 0x0606,  // MACADDR8 literal (string payload)
            EXT_SHOW_TABLES = 0x05,  // SHOW TABLES [FROM database] [LIKE 'pattern']
            EXT_SHOW_DATABASES = 0x06,  // SHOW DATABASES [LIKE 'pattern']
            EXT_SHOW_COLUMNS = 0x07,  // SHOW COLUMNS FROM table [LIKE 'pattern']
            EXT_SHOW_INDEXES = 0x08,  // SHOW INDEXES FROM table
            EXT_SHOW_CREATE_TABLE = 0x09,  // SHOW CREATE TABLE table
            EXT_DESCRIBE_TABLE = 0x15,  // DESCRIBE table (alias for SHOW COLUMNS)
            EXT_INDEX_INSERT = 0x0A,  // Insert entry into index (key, tid, xmin)
            EXT_INDEX_SEARCH = 0x0B,  // Search index for key (returns matching TIDs)
            EXT_INDEX_SCAN = 0x0C,  // Range scan index (start_key, end_key, returns TIDs)
            EXT_INDEX_DELETE = 0x0D,  // Delete entry from index (key, tid, xmax - MGA logical deletion)
            EXT_INDEX_TYPE = 0x0E,  // Index type marker (btree, hash, gin, etc.)
            EXT_INDEX_SCAN_START = 0x0F,  // Start index scan (returns scan_id)
            EXT_INDEX_SCAN_NEXT = 0x10,  // Get next from index scan (scan_id)
            EXT_INDEX_SCAN_END = 0x11,  // End index scan (scan_id)
            EXT_INDEX_VACUUM = 0x12,  // Vacuum index (remove dead entries)
            EXT_INDEX_STATS = 0x13,  // Get index statistics
            EXT_INDEX_REINDEX = 0x14,  // Rebuild index
            EXT_INDEX_UPDATE = 0x1C,  // Update index entry (old_key, new_key, tid, xmin - combines delete old + insert new)
            EXT_CURSOR_DECLARE = 0x1D,  // DECLARE cursor_name CURSOR FOR select_statement
            EXT_CURSOR_OPEN = 0x1E,  // OPEN cursor_name - Execute query and prepare for fetching
            EXT_CURSOR_FETCH = 0x1F,  // FETCH [direction] FROM cursor_name INTO variables
            EXT_GIN_INSERT = 0x28,  // GIN insert (value, tid, xmin, extractor_id)
            EXT_GIN_SEARCH = 0x29,  // GIN search (query, current_xid, extractor_id)
            EXT_HNSW_INSERT = 0x2A,  // HNSW insert (vector, tid, xmin)
            EXT_HNSW_SEARCH = 0x2B,  // HNSW k-NN search (vector, k, current_xid)
            EXT_COLUMNSTORE_INSERT = 0x2C,  // Columnstore insert column
            EXT_COLUMNSTORE_SCAN = 0x2D,  // Columnstore scan column
            EXT_CURSOR_CLOSE = 0x2E,  // CLOSE cursor_name - Close cursor and free resources
            EXT_IN_LIST = 0x2F,  // IN (val1, val2, ...) - membership test in value list
            EXT_GET_BYTE = 0x06,  // GET_BYTE(bytes, offset) - extract byte at offset
            EXT_SET_BYTE = 0x07,  // SET_BYTE(bytes, offset, value) - set byte at offset
            EXT_GET_BIT = 0x08,  // GET_BIT(bytes, bit_offset) - get bit at offset
            EXT_SET_BIT = 0x09,  // SET_BIT(bytes, bit_offset, value) - set bit at offset
            EXT_BIT_AND = 0x15,  // BIT_AND(a, b) / a & b - bitwise AND
            EXT_BIT_OR = 0x16,  // BIT_OR(a, b) / a | b - bitwise OR
            EXT_BIT_XOR = 0x17,  // BIT_XOR(a, b) / a ^ b - bitwise XOR
            EXT_BIT_NOT = 0x18,  // BIT_NOT(a) / ~a - bitwise NOT (complement)
            EXT_BIT_SHIFT_LEFT = 0x19,  // BIT_SHIFT_LEFT(a, n) / a << n - left shift
            EXT_BIT_SHIFT_RIGHT = 0x1A,  // BIT_SHIFT_RIGHT(a, n) / a >> n - arithmetic right shift
            EXT_BIT_SHIFT_RIGHT_LOGICAL = 0x1B,  // a >>> n - logical right shift (zero-fill)
            EXT_BIT_COUNT = 0x25,  // BIT_COUNT(a) - count set bits (popcount)
            EXT_BIT_LENGTH = 0x26,  // BIT_LENGTH(bytes) - length in bits
            EXT_BIT_MASK = 0x27,  // BIT_MASK(length) - create mask of N ones
            EXT_XMLPARSE = 0x45,  // XMLPARSE(document_or_content, xml_text)
            EXT_XMLSERIALIZE = 0x46,  // XMLSERIALIZE(content_or_document xml AS type)
            EXT_XMLELEMENT = 0x47,  // XMLELEMENT(name, content)
            EXT_XMLCONCAT = 0x48,  // XMLCONCAT(xml, ...)
            EXT_XMLFOREST = 0x49,  // XMLFOREST(expr AS name, ...)
            EXT_XMLCOMMENT = 0x4A,  // XMLCOMMENT(text) - create XML comment
            EXT_XMLROOT = 0x4B,  // XMLROOT(xml, VERSION version [, STANDALONE yes|no])
            EXT_XPATH = 0x4C,  // XPATH(xpath_expr, xml) - extract nodes using XPath
            EXT_XMLEXISTS = 0x4D,  // XMLEXISTS(xpath_expr, xml) - check if XPath matches
            EXT_XMLAGG = 0x4E,  // XMLAGG(xml) - aggregate XML values (aggregate function)
            EXT_MERGE_START = 0x4F,  // Begin MERGE operation
            EXT_MERGE_SOURCE = 0x50,  // Source query/table specification
            EXT_MERGE_ON = 0x51,  // ON condition (join predicate)
            EXT_MERGE_WHEN_MATCHED = 0x52,  // WHEN MATCHED THEN UPDATE
            EXT_MERGE_WHEN_NOT_MATCHED = 0x53,  // WHEN NOT MATCHED THEN INSERT
            EXT_MERGE_WHEN_NOT_MATCHED_SOURCE = 0x54,  // WHEN NOT MATCHED BY SOURCE THEN DELETE
            EXT_MERGE_END = 0x55,  // End MERGE operation
            EXT_RETURNING = 0x56,  // RETURNING clause marker (followed by column list or *)
            EXT_ANALYZE = 0x57,  // ANALYZE table_name [COLUMN column_name] [SAMPLE sample_rate]
            EXT_COPY = 0x0100,  // COPY table or SELECT [(columns)] FROM/TO target
            EXT_SAVEPOINT = 0x58,  // SAVEPOINT savepoint_name
            EXT_RELEASE_SAVEPOINT = 0x59,  // RELEASE SAVEPOINT savepoint_name
            EXT_ROLLBACK_TO_SAVEPOINT = 0x5A,  // ROLLBACK TO SAVEPOINT savepoint_name
            EXT_CREATE_TYPE = 0x5B,  // CREATE TYPE type_name AS {ENUM|RANGE|composite}
            EXT_CREATE_DOMAIN = 0x5C,  // CREATE DOMAIN domain_name AS type [constraints]
            EXT_CALL = 0x5D,  // CALL procedure_name(args...) - invoke stored procedure
            EXT_EXECUTE_STMT = 0x0115,  // EXECUTE STATEMENT (dynamic SQL)
            EXT_SUSPEND = 0x0116,  // SUSPEND (yield row in selectable procedure)
            EXT_SHOW_TABLE = 0x5E,  // SHOW TABLE object_name - detailed table info
            EXT_SHOW_INDEX = 0x5F,  // SHOW INDEX object_name - detailed index info
            EXT_SHOW_TRIGGER = 0x60,  // SHOW TRIGGER object_name - trigger definition
            EXT_SHOW_PROCEDURE = 0x61,  // SHOW PROCEDURE object_name - procedure definition
            EXT_SHOW_FUNCTION = 0x62,  // SHOW FUNCTION object_name - function definition
            EXT_SHOW_VIEW = 0x63,  // SHOW VIEW object_name - view definition
            EXT_SHOW_DOMAIN = 0x64,  // SHOW DOMAIN object_name - domain definition
            EXT_SHOW_GENERATOR = 0x65,  // SHOW GENERATOR object_name - sequence/generator info
            EXT_SHOW_SCHEMA = 0x66,  // SHOW SCHEMA [object_name] - schema listing or details
            EXT_SHOW_ROLE = 0x67,  // SHOW ROLE object_name - role definition
            EXT_SHOW_GRANTS = 0x68,  // SHOW GRANTS [FOR object_name] - privilege grants
            EXT_SHOW_CHECKS = 0x69,  // SHOW CHECKS object_name - check constraints
            EXT_SHOW_COLLATIONS = 0x6A,  // SHOW COLLATIONS [LIKE pattern] - available collations
            EXT_SHOW_COMMENTS = 0x6B,  // SHOW COMMENTS object_name - object comments/remarks
            EXT_SHOW_DEPENDENCIES = 0x6C,  // SHOW DEPENDENCIES object_name - object dependencies
            EXT_SHOW_PACKAGE = 0x6D,  // SHOW PACKAGE object_name - package definition
            EXT_SHOW_SYSTEM = 0x6E,  // SHOW SYSTEM - system information
            EXT_SHOW_SQL_DIALECT = 0x6F,  // SHOW SQL DIALECT - current SQL dialect
            EXT_SHOW_VERSION = 0x70,  // SHOW VERSION - database version
            EXT_SHOW_DATABASE = 0x71,  // SHOW DATABASE - current database info
            EXT_SET_SQL_DIALECT = 0x72,  // SET SQL DIALECT n - set SQL dialect (1, 2, or 3)
            EXT_SET_NAMES = 0x73,  // SET NAMES charset_name - set connection charset
            EXT_SET_LOCAL_TIMEOUT = 0x74,  // SET LOCAL_TIMEOUT n - set statement timeout in seconds
            EXT_SHOW_SCHEMA_PATH = 0x75,  // SHOW SCHEMA PATH - full path to current schema
            EXT_SHOW_SCHEMA_TREE = 0x76,  // SHOW SCHEMA TREE [DEPTH n] - schema hierarchy
            EXT_SHOW_SEARCH_PATH = 0x77,  // SHOW SEARCH PATH - current search path
            EXT_SHOW_LOCATION = 0x78,  // SHOW LOCATION OF [type] name - find object in search path
            EXT_SHOW_RESOLVED = 0x79,  // SHOW RESOLVED name - which object search path resolves to
            EXT_SHOW_OBJECTS = 0x7A,  // SHOW OBJECTS - all objects in current/specified schema
            EXT_SHOW_VARIABLE = 0x7B,  // SHOW variable_name - show session variable value
            EXT_SHOW_ALL = 0x7C,  // SHOW ALL - show all session variables
            EXT_SHOW_TRANSACTION_LEVEL = 0x7D,  // SHOW TRANSACTION ISOLATION LEVEL
            EXT_SHOW_JOBS = 0x7E,  // SHOW JOBS [LIKE pattern]
            EXT_SHOW_JOB_RUNS = 0x7F,  // SHOW JOB RUNS [FOR] job_name
            EXT_SHOW_JOB = 0x0113,  // SHOW JOB job_name
            EXT_SHOW_METRICS = 0x0114,  // SHOW METRICS
            EXT_SELECT_TABLE_STAR = 0x80,  // SELECT t.* - qualified table star (followed by table UUID)
            EXT_SET_VARIABLE = 0x81,  // SET variable = value - generic session variable
            EXT_ON_CONFLICT = 0x82,  // INSERT ... ON CONFLICT marker
            EXT_ON_CONFLICT_COLUMN = 0x83,  // ON CONFLICT (column, ...) - conflict target columns
            EXT_ON_CONFLICT_CONSTRAINT = 0x84,  // ON CONFLICT ON CONSTRAINT name
            EXT_ON_CONFLICT_DO_NOTHING = 0x85,  // ON CONFLICT DO NOTHING
            EXT_ON_CONFLICT_DO_UPDATE = 0x86,  // ON CONFLICT DO UPDATE SET
            EXT_ON_CONFLICT_WHERE = 0x87,  // ON CONFLICT ... WHERE condition
            EXT_GRANT = 0x88,  // GRANT privileges ON object TO grantee
            EXT_REVOKE = 0x89,  // REVOKE privileges ON object FROM grantee
            EXT_GRANT_OPTION = 0x8A,  // WITH GRANT OPTION
            EXT_PRIVILEGE = 0x8B,  // Privilege type (SELECT, INSERT, UPDATE, DELETE, etc.)
            EXT_CONNECT = 0x90,  // CONNECT TO database
            EXT_DISCONNECT = 0x91,  // DISCONNECT [ALL | CURRENT | connection_name]
            EXT_COMMENT = 0x94,  // COMMENT ON object IS 'text'
            EXT_RENAME_OBJECT = 0x0100,  // Rename object
            EXT_MOVE_OBJECT = 0x0101,  // Move object
            EXT_SET_AUTOCOMMIT = 0x0102,  // Autocommit control
            EXT_COMMIT_RETAINING = 0x0103,  // Deprecated alias
            EXT_ROLLBACK_RETAINING = 0x0104,  // Deprecated alias
            EXT_PREPARE_TRANSACTION = 0x0105,  // 2PC prepare
            EXT_COMMIT_PREPARED = 0x0106,  // 2PC commit
            EXT_ROLLBACK_PREPARED = 0x0107,  // 2PC rollback
            EXT_CREATE_SCHEMA = 0x0108,  // CREATE SCHEMA
            EXT_DROP_SCHEMA = 0x0109,  // DROP SCHEMA
            EXT_ALTER_SCHEMA = 0x010A,  // ALTER SCHEMA
            EXT_CREATE_DATABASE = 0x010B,  // CREATE DATABASE (emulated)
            EXT_DROP_DATABASE = 0x010C,  // DROP DATABASE (emulated)
            EXT_ALTER_DATABASE = 0x010D,  // ALTER DATABASE (emulated)
            EXT_ALTER_DOMAIN = 0x010E,  // ALTER DOMAIN
            EXT_DROP_DOMAIN = 0x010F,  // DROP DOMAIN
            EXT_RENAME_COLUMN = 0x011E,  // C4: ALTER TABLE RENAME COLUMN
            EXT_ALTER_COLUMN_DEFAULT = 0x011F,  // C4: ALTER COLUMN SET/DROP DEFAULT
            EXT_REBIND_DOMAIN = 0x0110,  // Rebind domain dependencies (admin)
            EXT_RESOLVE_DOMAIN_CONFLICT = 0x0111,  // Resolve domain conflicts (admin)
            EXT_ALTER_SYSTEM = 0x0112,  // ALTER SYSTEM SET section.key = value
            EXT_CREATE_FOREIGN_SERVER = 0x0117,  // CREATE SERVER (FDW)
            EXT_DROP_FOREIGN_SERVER = 0x0118,  // DROP SERVER (FDW)
            EXT_CREATE_FOREIGN_TABLE = 0x0119,  // CREATE FOREIGN TABLE
            EXT_DROP_FOREIGN_TABLE = 0x011A,  // DROP FOREIGN TABLE
            EXT_CREATE_USER_MAPPING = 0x011B,  // CREATE USER MAPPING
            EXT_DROP_USER_MAPPING = 0x011C,  // DROP USER MAPPING
            EXT_CREATE_SYNONYM = 0x011D,  // CREATE SYNONYM
            EXT_DROP_SYNONYM = 0x011E,  // DROP SYNONYM
            EXT_CREATE_UDR = 0x011F,  // CREATE UDR
            EXT_DROP_UDR = 0x0120,  // DROP UDR
            EXT_PREPARE_STMT = 0x0121,  // PREPARE statement_name FROM sql
            EXT_EXECUTE_PREPARED = 0x0122,  // EXECUTE prepared statement
            EXT_DEALLOCATE_PREPARED = 0x0123,  // DEALLOCATE PREPARE statement_name
            EXT_ALTER_FUNCTION_STMT = 0x0124,  // ALTER FUNCTION (characteristics)
            EXT_ALTER_PROCEDURE_STMT = 0x0125,  // ALTER PROCEDURE (characteristics)
            EXT_MYSQL_KILL = 0x0126,  // MySQL KILL CONNECTION/QUERY
            EXT_MYSQL_FLUSH = 0x0127,  // MySQL FLUSH variants
            EXT_MYSQL_LOCK_TABLES = 0x0128,  // MySQL LOCK TABLES
            EXT_MYSQL_UNLOCK_TABLES = 0x0129,  // MySQL UNLOCK TABLES

            // Null-safe comparison operators (MySQL/PostgreSQL)
            EXT_NULL_SAFE_EQ = 0x0200,  // NULL-safe equality (<=> / IS NOT DISTINCT FROM)
            EXT_LIKE_ESCAPE = 0x0201,   // LIKE with ESCAPE clause
            EXT_ILIKE_ESCAPE = 0x0202,  // ILIKE with ESCAPE clause
            EXT_PLACEHOLDER = 0x0203,   // Placeholder for prepared statements

            // Domain enforcement opcodes (Plan 03B)
            EXT_CHECK_DOMAIN_CONSTRAINT = 0x0204,  // Check all domain constraints
            EXT_APPLY_DOMAIN_MASKING = 0x0205,  // Apply masking (SELECT)
            EXT_ENCRYPT_DOMAIN_VALUE = 0x0206,  // Encrypt value (INSERT/UPDATE)
            EXT_DECRYPT_DOMAIN_VALUE = 0x0207,  // Decrypt value (SELECT)
            EXT_AUDIT_DOMAIN_ACCESS = 0x0208,  // Log domain access
            EXT_CHECK_DOMAIN_PRIVILEGE = 0x0209,  // Check privilege
            EXT_NORMALIZE_DOMAIN_VALUE = 0x020A,  // Apply normalization
            EXT_VALIDATE_DOMAIN_VALUE = 0x020B,  // Custom validation
            EXT_APPLY_QUALITY_PIPELINE = 0x020C,  // Quality pipeline
            EXT_CHECK_GLOBAL_UNIQUENESS = 0x020D,  // Global uniqueness

            // BLR compatibility (Firebird mapping)
            EXT_EXPR_NOT = 0x0210,  // NOT (unary predicate)
            EXT_EXPR_IS_NULL = 0x0211,  // IS NULL predicate
            EXT_SAVEPOINT_BEGIN = 0x0212,  // Savepoint scope begin (blr_start_savepoint)
            EXT_SAVEPOINT_END = 0x0213,  // Savepoint scope end (blr_end_savepoint)
            EXT_INSERTED_COLUMN_REF = 0x0214,  // INSERTED column reference (MySQL VALUES(col))

            // Python parity operators/predicates
            EXT_EXPR_DIV_INT = 0x0215,  // DIV integer division operator
            EXT_PRED_STARTING_WITH = 0x0216,  // STARTING WITH predicate
            EXT_PRED_CONTAINING = 0x0217,  // CONTAINING predicate

            // Python parity functions
            EXT_FUNC_REPLACE = 0x0320,  // REPLACE(str, search, replacement)
            EXT_FUNC_ENDS_WITH = 0x0321,  // ENDS_WITH(str, suffix)
            EXT_FUNC_ARRAY_POSITION = 0x0322,  // ARRAY_POSITION(array, value)
            EXT_ARRAY_SLICE = 0x0323,  // ARRAY_SLICE(array, lower, upper)
            EXT_FUNC_JSON_EXISTS = 0x0324,  // JSON_EXISTS(json, path)
            EXT_FUNC_JSON_HAS_KEY = 0x0325,  // JSON_HAS_KEY(json, key)
            EXT_FUNC_TO_CHAR = 0x0326,  // TO_CHAR(value, format)
            EXT_FUNC_TO_DATE = 0x0327,  // TO_DATE(text, format)
            EXT_FUNC_TO_TIMESTAMP = 0x0328,  // TO_TIMESTAMP(text, format)
            EXT_FUNC_LEAST = 0x0329,  // LEAST(a, b, ...)
            EXT_FUNC_GREATEST = 0x032A,  // GREATEST(a, b, ...)
            EXT_FUNC_CURRENT_USER = 0x032B,  // CURRENT_USER
            EXT_FUNC_CURRENT_ROLE = 0x032C,  // CURRENT_ROLE
            EXT_FUNC_CURRENT_CONNECTION = 0x032D,  // CURRENT_CONNECTION
            EXT_FUNC_CURRENT_TRANSACTION = 0x032E  // CURRENT_TRANSACTION
        };

        enum class AlterSchemaAction : uint8_t
        {
            RENAME = 1,
            SET_OWNER = 2,
            SET_PATH = 3
        };

        enum class AlterDatabaseAction : uint8_t
        {
            RENAME = 1,
            SET_OWNER = 2,
            ADD_ALIAS = 3,
            DROP_ALIAS = 4,
            SET_OPTIONS = 5
        };

        enum class AlterDomainAction : uint8_t
        {
            SET_DEFAULT = 0,
            DROP_DEFAULT = 1,
            ADD_CHECK = 2,
            DROP_CONSTRAINT = 3,
            RENAME = 4,
            SET_COMPAT = 5,
            DROP_COMPAT = 6
        };

        /**
         * IndexType - Index type identifiers for EXT_INDEX_TYPE opcode
         *
         * Used to specify which index implementation to use for index operations.
         * Each index type has different characteristics and use cases.
         *
         * SELECTION GUIDE:
         * ---------------
         * BTREE (0x00) - General-purpose index for most use cases
         *   • Use for: Equality and range queries on scalar values
         *   • Strengths: Balanced tree, O(log N) operations, supports ORDER BY
         *   • Best for: Primary keys, foreign keys, commonly queried columns
         *   • Example: CREATE INDEX idx_users_email ON users USING BTREE (email)
         *
         * HASH (0x01) - Fast equality-only lookups
         *   • Use for: Equality searches (WHERE col = value)
         *   • Strengths: O(1) average lookup time
         *   • Limitations: No range queries, no ORDER BY support
         *   • Best for: Unique identifiers, lookup tables
         *   • Example: CREATE INDEX idx_sessions_token ON sessions USING HASH (token)
         *
         * GIN (0x02) - Generalized Inverted Index for multi-value columns
         *   • Use for: Arrays, JSONB, full-text search, composite types
         *   • Strengths: Efficiently indexes values within composite types
         *   • Best for: Array containment (@>), JSONB queries, text search
         *   • Example: CREATE INDEX idx_tags ON posts USING GIN (tags)
         *
         * GIST (0x03) - Generalized Search Tree (extensible)
         *   • Use for: Spatial data, ranges, custom types with custom operators
         *   • Strengths: Balanced tree with custom predicates, overlaps, contains
         *   • Best for: Geometric types, range types, custom similarity
         *   • Example: CREATE INDEX idx_location ON places USING GIST (location)
         *
         * SPGIST (0x04) - Space-Partitioned Generalized Search Tree
         *   • Use for: Non-balanced partitioning (quadtrees, k-d trees)
         *   • Strengths: Efficient for clustered data distributions
         *   • Best for: IP addresses, points with spatial clustering
         *   • Example: CREATE INDEX idx_ip ON connections USING SPGIST (ip_address)
         *
         * BRIN (0x05) - Block Range Index (minimal storage)
         *   • Use for: Very large tables with natural physical ordering
         *   • Strengths: Tiny index size, stores min/max per block range
         *   • Best for: Time-series data, append-only tables, monotonic sequences
         *   • Example: CREATE INDEX idx_timestamp ON logs USING BRIN (timestamp)
         *
         * RTREE (0x06) - R-Tree for spatial indexing
         *   • Use for: Spatial data with bounding boxes (MBRs)
         *   • Strengths: Hierarchical bounding boxes, spatial containment
         *   • Best for: Geographic data, geometric shapes
         *   • Example: CREATE INDEX idx_geometry ON parcels USING RTREE (boundary)
         *
         * HNSW (0x07) - Hierarchical Navigable Small World (vector ANN)
         *   • Use for: High-dimensional vector similarity search
         *   • Strengths: Fast approximate nearest neighbor (ANN) search
         *   • Best for: Embeddings, image similarity, semantic search
         *   • Example: CREATE INDEX idx_embedding ON documents USING HNSW (embedding)
         *
         * BITMAP (0x08) - Bitmap index for low-cardinality columns
         *   • Use for: Columns with few distinct values (e.g., status, category)
         *   • Strengths: Efficient AND/OR/NOT operations, minimal storage
         *   • Best for: Boolean flags, enums, status codes
         *   • Example: CREATE INDEX idx_status ON orders USING BITMAP (status)
         *
         * COLUMNSTORE (0x09) - Column-oriented storage index
         *   • Use for: Analytical queries on large tables (OLAP workloads)
         *   • Strengths: Columnar compression, fast aggregations on subsets of columns
         *   • Best for: Data warehouses, reporting tables, wide tables
         *   • Example: CREATE INDEX idx_sales ON sales USING COLUMNSTORE (date, product_id, amount)
         *
         * LSM (0x0A) - Log-Structured Merge-Tree (write-optimized)
         *   • Use for: Write-heavy workloads with eventual consistency tolerance
         *   • Strengths: Fast writes via memtable, deferred merges
         *   • Best for: Event logs, metrics, sensor data
         *   • Example: CREATE INDEX idx_events ON events USING LSM (timestamp)
         *
         * MGA COMPLIANCE:
         * --------------
         * All 11 index types MUST maintain xmin/xmax for MGA visibility tracking.
         * See MGA_RULES.md for detailed requirements.
         */
        enum class IndexType : uint8_t
        {
            BTREE = 0x00,          // B-Tree index - General purpose, sorted data
            HASH = 0x01,           // Hash index - Equality searches only
            HNSW = 0x02,           // HNSW index - Vector similarity search (ANN)
            FULLTEXT = 0x03,       // Full-text search index
            GIN = 0x04,            // GIN index - Multi-value columns (arrays, JSONB, text search)
            GIST = 0x05,           // GiST index - Extensible, spatial data, custom types
            BRIN = 0x06,           // BRIN index - Block range index, large tables
            RTREE = 0x07,          // R-Tree index - Spatial data, bounding boxes
            SPGIST = 0x08,         // SP-GiST index - Space-partitioned, non-balanced trees
            BITMAP = 0x09,         // Bitmap index - Low cardinality columns
            COLUMNSTORE = 0x0A,    // Columnstore index - Column-oriented storage
            LSM = 0x0B,            // LSM-Tree index - Write-optimized, append-heavy workloads
        };

        /**
         * ExtractField - Field identifiers for EXTRACT(field FROM value) function
         *
         * Each field corresponds to sub-information that can be extracted from complex data types.
         * See docs/planning/EXTRACT_FUNCTION_COMPREHENSIVE_PLAN.md for complete specification.
         */
        enum class ExtractField : uint8_t
        {
            // Temporal fields (0x00-0x1F) - DATE, TIME, TIMESTAMP, INTERVAL
            YEAR = 0x00,            // Year (all temporal types)
            MONTH = 0x01,           // Month (1-12)
            DAY = 0x02,             // Day of month (1-31)
            HOUR = 0x03,            // Hour (0-23)
            MINUTE = 0x04,          // Minute (0-59)
            SECOND = 0x05,          // Second (0-59)
            MICROSECOND = 0x06,     // Microsecond (0-999999)
            MILLISECOND = 0x07,     // Millisecond (0-999)
            DOW = 0x08,             // Day of week (0=Sunday, 6=Saturday)
            DOY = 0x09,             // Day of year (1-366)
            QUARTER = 0x0A,         // Quarter (1-4)
            WEEK = 0x0B,            // Week of year (Sunday-based)
            EPOCH = 0x0C,           // Seconds since Unix epoch (int64/double)
            TIMEZONE = 0x0D,        // Timezone offset seconds (alias of TZ_OFFSET)
            TIMEZONE_HOUR = 0x0E,   // Timezone offset hours
            TIMEZONE_MINUTE = 0x0F, // Timezone offset minutes
            TZ_OFFSET = 0x10,       // Timezone offset seconds
            ISO_WEEK = 0x11,        // ISO week number (1-53)
            ISO_YEAR = 0x12,        // ISO week-based year
            ISO_DOW = 0x13,         // ISO day of week (1=Mon..7=Sun)
            CENTURY = 0x14,         // Century
            DECADE = 0x15,          // Decade
            MILLENNIUM = 0x16,      // Millennium
            HOUR12 = 0x17,          // 12-hour clock hour (1-12)
            TOTAL_MONTHS = 0x18,    // Interval total months
            TOTAL_DAYS = 0x19,      // Interval total days
            TOTAL_SECONDS = 0x1A,   // Interval total seconds

            // UUID fields (0x20-0x2F)
            VERSION = 0x20,         // UUID version (1-7)
            VARIANT = 0x21,         // UUID variant (0-2)
            TIMESTAMP = 0x22,       // UUID timestamp (v1/v7, microseconds since epoch)
            NODE = 0x23,            // UUID node (v1, MAC address as varchar)
            CLOCK_SEQ = 0x24,       // UUID clock sequence (v1, int32)
            TIME_LOW = 0x25,        // UUID time_low (v1)
            TIME_MID = 0x26,        // UUID time_mid (v1)
            TIME_HIGH = 0x27,       // UUID time_high_and_version (v1)
            RAND_A = 0x28,          // UUID v7 rand_a
            RAND_B = 0x29,          // UUID v7 rand_b

            // Network fields (0x30-0x3F) - INET, CIDR, MACADDR
            FAMILY = 0x30,          // IP address family (4=IPv4, 6=IPv6)
            NETMASK = 0x31,         // Network mask bits (0-32 for IPv4, 0-128 for IPv6)
            ADDRESS = 0x32,         // IP address without netmask (text)
            NETWORK = 0x33,         // Network address (host bits zeroed)
            BROADCAST = 0x34,       // Broadcast address (host bits set to 1)
            HOSTMASK = 0x35,        // Host mask (inverse of netmask)
            VENDOR = 0x36,          // MAC address vendor OUI (alias of OUI)
            NETMASK_ADDR = 0x37,    // Netmask as address
            IS_IPV4 = 0x38,         // Is IPv4
            IS_IPV6 = 0x39,         // Is IPv6
            OUI = 0x3A,             // MAC OUI (first 3 bytes)
            NIC = 0x3B,             // MAC NIC (remaining bytes)
            IS_MULTICAST = 0x3C,    // MAC multicast flag
            IS_LOCAL = 0x3D,        // MAC local/administered flag
            TRUNC = 0x3E,           // MAC truncation (manufacturer ID)

            // Spatial fields (0x40-0x4F) - POINT, LINESTRING, POLYGON, etc.
            X = 0x40,               // POINT x coordinate (longitude)
            Y = 0x41,               // POINT y coordinate (latitude)
            SRID = 0x42,            // Spatial reference identifier
            NUM_POINTS = 0x43,      // LINESTRING point count
            START_POINT = 0x44,     // LINESTRING start point
            END_POINT = 0x45,       // LINESTRING end point
            NUM_RINGS = 0x46,       // POLYGON ring count
            EXTERIOR_RING = 0x47,   // POLYGON exterior ring (linestring)
            NUM_INTERIOR_RINGS = 0x48, // POLYGON interior ring count
            NUM_GEOMETRIES = 0x49,  // Multi-geometry geometry count
            BBOX = 0x4A,            // Bounding box polygon
            POINTS = 0x4B,          // POINTS array (linestring)
            RINGS = 0x4C,           // RINGS array (polygon)
            AREA = 0x4D,            // Polygon area
            GEOMETRIES = 0x4E,      // GEOMETRIES array (multi/collection)
            LENGTH = 0x4F,          // Length (linestring or binary)

            // Array fields (0x50-0x5F)
            CARDINALITY = 0x50,     // Total number of elements
            NDIMS = 0x51,           // Number of dimensions (rank)
            DIMS = 0x52,            // Array of dimension sizes
            LOWER = 0x53,           // Lower bound of first dimension (always 1)
            UPPER = 0x54,           // Upper bound of first dimension
            ELEMENT = 0x55,         // Array/vector element access

            // Range fields (0x60-0x6F) - INT4RANGE, INT8RANGE, DATERANGE, TSRANGE, etc.
            LOWER_VALUE = 0x60,     // Range lower bound value
            UPPER_VALUE = 0x61,     // Range upper bound value
            LOWER_INC = 0x62,       // Lower bound inclusive (bool)
            UPPER_INC = 0x63,       // Upper bound inclusive (bool)
            LOWER_INF = 0x64,       // Lower bound infinite/unbounded (bool)
            UPPER_INF = 0x65,       // Upper bound infinite/unbounded (bool)
            ISEMPTY = 0x66,         // Range is empty (bool)

            // Generic elements (0x70+)
            VALUE = 0x70,           // Raw value
            SIGN = 0x71,            // Sign (-1/0/1)
            ABS = 0x72,             // Absolute value
            BYTES = 0x73,           // Byte width / binary bytes
            BITS = 0x74,            // Bit width
            HI64 = 0x75,            // High 64 bits (int128/uint128)
            LO64 = 0x76,            // Low 64 bits (int128/uint128)
            EXPONENT = 0x77,        // Float exponent
            MANTISSA = 0x78,        // Float mantissa
            IS_NAN = 0x79,          // Float NaN
            IS_INF = 0x7A,          // Float infinity
            PRECISION = 0x7B,       // Decimal precision
            SCALE = 0x7C,           // Decimal scale
            UNSCALED = 0x7D,        // Decimal unscaled value
            MAJOR = 0x7E,           // Money major units
            MINOR = 0x7F,           // Money minor units
            CHAR_LENGTH = 0x80,     // Character length
            OCTET_LENGTH = 0x81,    // Byte length
            CODEPOINT_LENGTH = 0x82,// Unicode codepoint length
            TRIMMED_LENGTH = 0x83,  // Trimmed length (CHAR)
            TYPE = 0x84,            // JSON type
            KEYS = 0x85,            // JSON object keys
            PATH = 0x86,            // JSON/XML path
            ATTRIBUTES = 0x87,      // XML attributes
            BYTE = 0x88,            // Binary byte access
            BIT = 0x89,             // Binary bit access
            SLICE = 0x8A,           // Binary slice
            DIGEST = 0x8B,          // Binary digest
            DIMENSION = 0x8C,       // Vector dimension
            NORM_L2 = 0x8D,         // Vector L2 norm
            DOT = 0x8E,             // Vector dot product
            FIELD = 0x8F,           // Composite field access
            FIELD_NAMES = 0x90,     // Composite field names
            DATATYPE = 0x91,        // Variant datatype
            LEXEMES = 0x92,         // TSVECTOR lexemes
            POSITIONS = 0x93,       // TSVECTOR positions
            WEIGHTS = 0x94,         // TSVECTOR weights
            SIZE = 0x95,            // TSVECTOR/TSQUERY size
            HAS_LEXEME = 0x96,      // TSVECTOR has lexeme
            ROOT_OP = 0x97,         // TSQUERY root operator
            TERMS = 0x98,           // TSQUERY terms
            OPERATORS = 0x99,       // TSQUERY operators
            PHRASE_DISTANCE = 0x9A,// TSQUERY phrase distance
            NODES = 0x9B,           // TSQUERY node count
        };

        // SBLR Version
        constexpr uint8_t SBLR_VERSION = 2;

        // Transaction control flags (START/SET TRANSACTION)
        namespace TransactionFlags
        {
            constexpr uint16_t HAS_ISOLATION = 0x0001;
            constexpr uint16_t HAS_ACCESS_MODE = 0x0002;
            constexpr uint16_t HAS_DEFERRABLE = 0x0004;
            constexpr uint16_t HAS_WAIT_MODE = 0x0008;
            constexpr uint16_t HAS_LOCK_TIMEOUT = 0x0010;
            constexpr uint16_t HAS_RESERVATIONS = 0x0020;
            constexpr uint16_t HAS_AUTOCOMMIT = 0x0040;
            constexpr uint16_t HAS_CONFLICT_ERROR_CODE = 0x0080;
            constexpr uint16_t HAS_READ_COMMITTED_MODE = 0x0100;
        }

        enum class TransactionConflictAction : uint8_t
        {
            DEFAULT = 0,
            COMMIT = 1,
            ROLLBACK = 2,
            ERROR = 3,
            KEEP = 4
        };

        enum class AutocommitMode : uint8_t
        {
            UNCHANGED = 0,
            ON = 1,
            OFF = 2
        };

        enum class ReadCommittedMode : uint8_t
        {
            DEFAULT = 0,
            READ_CONSISTENCY = 1,
            RECORD_VERSION = 2,
            NO_RECORD_VERSION = 3
        };

        namespace CommitRollbackFlags
        {
            constexpr uint8_t AND_CHAIN = 0x01;
            constexpr uint8_t AND_NO_CHAIN = 0x02;
            constexpr uint8_t RETAINING = 0x04;
        }

        // Helper to write multi-byte values in little-endian
        inline void writeInt32(uint8_t *buffer, uint32_t value)
        {
            buffer[0] = value & 0xFF;
            buffer[1] = (value >> 8) & 0xFF;
            buffer[2] = (value >> 16) & 0xFF;
            buffer[3] = (value >> 24) & 0xFF;
        }

        inline void writeInt64(uint8_t *buffer, uint64_t value)
        {
            buffer[0] = value & 0xFF;
            buffer[1] = (value >> 8) & 0xFF;
            buffer[2] = (value >> 16) & 0xFF;
            buffer[3] = (value >> 24) & 0xFF;
            buffer[4] = (value >> 32) & 0xFF;
            buffer[5] = (value >> 40) & 0xFF;
            buffer[6] = (value >> 48) & 0xFF;
            buffer[7] = (value >> 56) & 0xFF;
        }

        inline void writeInt16(uint8_t *buffer, uint16_t value)
        {
            buffer[0] = value & 0xFF;
            buffer[1] = (value >> 8) & 0xFF;
        }

        inline uint32_t readInt32(const uint8_t *buffer)
        {
            return buffer[0] | (uint32_t(buffer[1]) << 8) | (uint32_t(buffer[2]) << 16) |
                   (uint32_t(buffer[3]) << 24);
        }

        inline uint64_t readInt64(const uint8_t *buffer)
        {
            return buffer[0] | (uint64_t(buffer[1]) << 8) | (uint64_t(buffer[2]) << 16) |
                   (uint64_t(buffer[3]) << 24) | (uint64_t(buffer[4]) << 32) |
                   (uint64_t(buffer[5]) << 40) | (uint64_t(buffer[6]) << 48) |
                   (uint64_t(buffer[7]) << 56);
        }

        inline uint16_t readInt16(const uint8_t *buffer)
        {
            return buffer[0] | (uint16_t(buffer[1]) << 8);
        }

        inline size_t writeUVarint(uint8_t *buffer, uint64_t value)
        {
            size_t count = 0;
            while (value >= 0x80)
            {
                buffer[count++] = static_cast<uint8_t>(value) | 0x80;
                value >>= 7;
            }
            buffer[count++] = static_cast<uint8_t>(value);
            return count;
        }

        inline bool readUVarint(const uint8_t *buffer, size_t buffer_len,
                                uint64_t &value_out, size_t &bytes_read)
        {
            value_out = 0;
            bytes_read = 0;
            uint32_t shift = 0;

            while (bytes_read < buffer_len)
            {
                uint8_t byte = buffer[bytes_read++];
                value_out |= (static_cast<uint64_t>(byte & 0x7F) << shift);

                if ((byte & 0x80) == 0)
                {
                    return true;
                }

                shift += 7;
                if (shift > 63)
                {
                    return false;
                }
            }

            return false;
        }

    } // namespace sblr
} // namespace scratchbird

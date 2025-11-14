#pragma once

#include <cstdint>

namespace scratchbird
{
    namespace sblr
    {

        // SBLR (ScratchBird Language Representation) Opcodes
        // Based on Firebird's BLR (Binary Language Representation)

        enum class Opcode : uint8_t
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
            EXPR_CAST = 0x72, // Type cast (expr + target type)

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

            // Temporal functions
            FUNC_DATE_ADD = 0x84,     // DATE_ADD(date, days)
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
            NOT_NULL = 0x90,         // NOT NULL constraint
            DEFAULT_VALUE = 0x91,    // DEFAULT value expression (ALPHA Phase A)
            CHECK_CONSTRAINT = 0x92, // CHECK constraint expression (ALPHA Phase A)
            FOREIGN_KEY = 0x93,      // Foreign key constraint (ALPHA Phase A)
            TABLE_FK = 0x94,         // Table-level foreign key constraint (ALPHA Phase C - Composite FK)

            // Special
            SELECT_STAR = 0xA0,  // SELECT *
            WHERE_CLAUSE = 0xA1, // WHERE clause marker

            // Additional data types (0xB0-0xBF range)
            TYPE_VARBINARY = 0xB0, // Variable-length binary
            TYPE_BLOB = 0xB1,      // Binary large object
            TYPE_BYTEA = 0xB2,     // Byte array (PostgreSQL compatible)
            TYPE_JSON = 0xB3,      // JSON data

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

            // Aggregation and grouping (Phase 1, Task 4.1)
            GROUP_BY = 0xC9,          // GROUP BY clause marker
            HAVING = 0xCA,            // HAVING clause marker
            AGG_INIT = 0xCB,          // Initialize aggregation state
            AGG_ACCUMULATE = 0xCC,    // Accumulate aggregate value
            AGG_FINALIZE = 0xCD,      // Finalize aggregate result

            // Sorting (Phase 1, Task 5.1)
            ORDER_BY = 0xCE,          // ORDER BY clause marker
            SORT_KEY = 0xCF,          // Sort key expression
            SORT_ASC = 0xD0,          // Sort ascending
            SORT_DESC = 0xD1,         // Sort descending
            NULLS_FIRST = 0xD2,       // NULLS FIRST modifier
            NULLS_LAST = 0xD3,        // NULLS LAST modifier

            // Limiting (Phase 1, Task 5.2)
            LIMIT = 0xD4,             // LIMIT clause
            OFFSET = 0xD5,            // OFFSET clause

            // Window functions (Phase 1, Task 6.3)
            WINDOW = 0xD6,            // Window function clause marker
            WINDOW_SPEC = 0xD7,       // Window specification (OVER clause)
            PARTITION_BY = 0xD8,      // PARTITION BY clause
            WINDOW_ORDER_BY = 0xD9,   // ORDER BY within window spec
            FRAME_CLAUSE = 0xDA,      // Frame clause marker
            FRAME_ROWS = 0xDB,        // ROWS frame mode
            FRAME_RANGE = 0xDC,       // RANGE frame mode
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
            EXT_EXCEPT = 0x9E,             // EXCEPT handler
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

            // Privilege management opcodes
            EXT_GRANT_PRIVILEGE = 0xD1,    // GRANT privilege ON object TO grantee [WITH GRANT OPTION]
            EXT_REVOKE_PRIVILEGE = 0xD2,   // REVOKE privilege ON object FROM grantee [CASCADE | RESTRICT]

            // Role grant/revoke opcodes
            EXT_GRANT_ROLE = 0xD3,         // GRANT role TO user/role
            EXT_REVOKE_ROLE = 0xD4,        // REVOKE role FROM user/role [CASCADE | RESTRICT]

            // Session management opcodes
            EXT_SET_ROLE = 0xD5,           // SET ROLE rolename / RESET ROLE
            EXT_SET_SESSION_AUTH = 0xD6,   // SET SESSION AUTHORIZATION username / RESET SESSION AUTHORIZATION

            // Row-Level Security opcodes (Security Phase 3.4)
            EXT_CREATE_POLICY = 0xD7,      // CREATE POLICY policy_name ON table_name
            EXT_DROP_POLICY = 0xD8,        // DROP POLICY [IF EXISTS] policy_name ON table_name
            EXT_ALTER_TABLE_RLS = 0xD9,    // ALTER TABLE table_name {ENABLE|DISABLE|FORCE|NO FORCE} ROW LEVEL SECURITY

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

            // Note: 0xFF is EXTENDED_OPCODE marker (already defined above)

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
        };

        // SBLR Version
        constexpr uint8_t SBLR_VERSION = 1;

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

    } // namespace sblr
} // namespace scratchbird
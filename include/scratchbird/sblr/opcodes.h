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

            // Modifiers
            NOT_NULL = 0x90, // NOT NULL constraint

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
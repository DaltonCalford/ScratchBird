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
            CREATE_TABLE = 0x10, // Create table
            INSERT = 0x11,       // Insert row
            SELECT = 0x12,       // Select query

            // Data types
            TYPE_INTEGER = 0x20, // 32-bit integer
            TYPE_BIGINT = 0x21,  // 64-bit integer
            TYPE_DOUBLE = 0x22,  // Double precision float
            TYPE_VARCHAR = 0x23, // Variable length string

            // Values
            LITERAL_NULL = 0x30,   // NULL value
            LITERAL_INT32 = 0x31,  // 32-bit integer literal
            LITERAL_INT64 = 0x32,  // 64-bit integer literal
            LITERAL_DOUBLE = 0x33, // Double literal
            LITERAL_STRING = 0x34, // String literal (length + data)

            // Column/Table references
            TABLE_REF = 0x40,  // Table reference (string id)
            COLUMN_REF = 0x41, // Column reference (string id)
            COLUMN_DEF = 0x42, // Column definition

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

            // Lists
            BEGIN_LIST = 0x80, // Start of list (followed by count)
            END_LIST = 0x81,   // End of list

            // Modifiers
            NOT_NULL = 0x90, // NOT NULL constraint

            // Special
            SELECT_STAR = 0xA0,  // SELECT *
            WHERE_CLAUSE = 0xA1, // WHERE clause marker
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

    } // namespace sblr
} // namespace scratchbird
#pragma once

#include <cstdint>
#include <array>
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{
    // 128-bit integer type (GCC extension)
    using int128_t = __int128;
    using uint128_t = unsigned __int128;

    // Common type alias for object IDs (UUIDv7)
    // Used across the system for users, roles, tables, etc.
    using ID = UuidV7Bytes;

    /**
     * Unified Data Type System
     *
     * This is the single source of truth for all data types in ScratchBird.
     * All subsystems (parser, catalog, executor) must use this enum.
     */
    enum class DataType : uint16_t
    {
        UNKNOWN = 0,

        // Numeric types (1-9)
        INT8 = 1,     // 1-byte signed integer (-128 to 127)
        INT16 = 2,    // 2-byte signed integer (alias: SMALLINT)
        INT32 = 3,    // 4-byte signed integer (alias: INTEGER, INT)
        INT64 = 4,    // 8-byte signed integer (alias: BIGINT)
        INT128 = 5,   // 16-byte signed integer
        UINT8 = 6,    // 1-byte unsigned integer (0 to 255)
        UINT16 = 7,   // 2-byte unsigned integer
        UINT32 = 8,   // 4-byte unsigned integer
        UINT64 = 9,   // 8-byte unsigned integer
        FLOAT32 = 10, // 4-byte IEEE 754 float (alias: REAL, FLOAT)
        FLOAT64 = 11, // 8-byte IEEE 754 double (alias: DOUBLE)
        DECIMAL = 12, // Fixed-precision decimal (precision, scale)
        MONEY = 13,   // Fixed-precision currency type

        // String types (20-29)
        CHAR = 20,    // Fixed-length string (padded with spaces)
        VARCHAR = 21, // Variable-length string (max length specified)
        TEXT = 22,    // Unlimited variable-length string

        // Binary types (30-39)
        BINARY = 30,    // Fixed-length binary data
        VARBINARY = 31, // Variable-length binary data
        BLOB = 32,      // Binary large object
        BYTEA = 33,     // PostgreSQL-style binary data

        // Date/Time types (40-49)
        DATE = 40,      // Date (year, month, day)
        TIME = 41,      // Time of day (hour, minute, second, microsecond)
        TIMESTAMP = 42, // Date + time (with optional timezone)
        INTERVAL = 43,  // Time interval (years, months, days, hours, etc.)

        // Boolean (50-59)
        BOOLEAN = 50, // True/false

        // Special types (60-69)
        UUID = 60,   // 128-bit UUID (RFC 4122)
        JSON = 61,   // JSON document (stored as text, validated)
        JSONB = 62,  // Binary JSON (optimized storage and indexing)
        XML = 63,    // XML document
        VECTOR = 64, // Vector embeddings for similarity search (variable dimensions)

        // Spatial types (65-71)
        POINT = 65,             // Geometric point (x, y)
        LINESTRING = 66,        // Sequence of connected points
        POLYGON = 67,           // Closed polygon with optional holes
        MULTIPOINT = 68,        // Collection of POINT geometries
        MULTILINESTRING = 69,   // Collection of LINESTRING geometries
        MULTIPOLYGON = 70,      // Collection of POLYGON geometries
        GEOMETRYCOLLECTION = 71, // Heterogeneous collection of geometries

        // Array and composite types (72-79)
        ARRAY = 72,     // Array of elements (homogeneous type)
        COMPOSITE = 73, // Record/struct type (heterogeneous types)

        // Text search types (74-75)
        TSVECTOR = 74,  // Text search vector (document representation)
        TSQUERY = 75,   // Text search query (search expression)

        // Range types (76-85)
        INT4RANGE = 76,  // Range of INT32 values
        INT8RANGE = 77,  // Range of INT64 values
        NUMRANGE = 78,   // Range of DECIMAL/FLOAT64 values
        TSRANGE = 79,    // Range of TIMESTAMP values (without timezone)
        TSTZRANGE = 80,  // Range of TIMESTAMP values (with timezone)
        DATERANGE = 81,  // Range of DATE values

        // Network types (86-89)
        INET = 86,       // IPv4 or IPv6 address with optional subnet
        CIDR = 87,       // IPv4 or IPv6 network (strict CIDR notation)
        MACADDR = 88,    // 6-byte MAC address (EUI-48)
        MACADDR8 = 89,   // 8-byte MAC address (EUI-64)

        // Polymorphic types (90-99)
        VARIANT = 90,    // Tagged union that can hold any type

        // Null type (255)
        NULL_TYPE = 255, // SQL NULL
    };

    /**
     * Type metadata - stores additional information about a type
     */
    struct TypeInfo
    {
        DataType type;
        uint32_t precision;     // For CHAR, VARCHAR, DECIMAL
        uint32_t scale;         // For DECIMAL
        DataType element_type;  // For ARRAY
        bool with_timezone;     // For TIMESTAMP
        uint16_t timezone_hint; // Display timezone ID for TIMESTAMP WITH TIME ZONE (0 = use connection default)

        TypeInfo()
            : type(DataType::UNKNOWN), precision(0), scale(0), element_type(DataType::UNKNOWN),
              with_timezone(false), timezone_hint(0)
        {
        }

        TypeInfo(DataType t)
            : type(t), precision(0), scale(0), element_type(DataType::UNKNOWN),
              with_timezone(false), timezone_hint(0)
        {
        }

        TypeInfo(DataType t, uint32_t p)
            : type(t), precision(p), scale(0), element_type(DataType::UNKNOWN),
              with_timezone(false), timezone_hint(0)
        {
        }

        TypeInfo(DataType t, uint32_t p, uint32_t s)
            : type(t), precision(p), scale(s), element_type(DataType::UNKNOWN),
              with_timezone(false), timezone_hint(0)
        {
        }
    };

    /**
     * Type System Helper Functions
     */
    class TypeSystem
    {
    public:
        // Get the name of a data type
        static const char* getTypeName(DataType type);

        // Check if a type can be explicitly converted to another
        static bool isExplicitlyConvertible(DataType from, DataType to);
    };

} // namespace scratchbird::core

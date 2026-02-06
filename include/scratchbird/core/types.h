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
#include <array>
#include <vector>
#include <memory>
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

        // Numeric types (1-19)
        INT8 = 1,        // 1-byte signed integer (-128 to 127)
        TINYINT = 1,     // Alias for INT8 (SQL standard name)
        INT16 = 2,       // 2-byte signed integer
        SMALLINT = 2,    // Alias for INT16 (SQL standard name)
        INT32 = 3,       // 4-byte signed integer
        INTEGER = 3,     // Alias for INT32 (SQL standard name)
        INT = 3,         // Alias for INT32
        INT64 = 4,       // 8-byte signed integer
        BIGINT = 4,      // Alias for INT64 (SQL standard name)
        INT128 = 5,      // 16-byte signed integer
        UINT8 = 6,       // 1-byte unsigned integer (0 to 255)
        UINT16 = 7,      // 2-byte unsigned integer
        UINT32 = 8,      // 4-byte unsigned integer
        UINT64 = 9,      // 8-byte unsigned integer
        FLOAT32 = 10,    // 4-byte IEEE 754 float
        REAL = 10,       // Alias for FLOAT32 (SQL standard name)
        FLOAT = 10,      // Alias for FLOAT32
        FLOAT64 = 11,    // 8-byte IEEE 754 double
        DOUBLE = 11,     // Alias for FLOAT64 (SQL standard name)
        DECIMAL = 12,    // Fixed-precision decimal (precision, scale)
        NUMERIC = 12,    // Alias for DECIMAL (SQL standard name)
        MONEY = 13,      // Fixed-precision currency type
        UINT128 = 14,    // 16-byte unsigned integer
        DECFLOAT16 = 15, // IEEE-754 decimal floating (Decimal64)
        DECFLOAT34 = 16, // IEEE-754 decimal floating (Decimal128)
        MEDIUMINT = 17,  // MySQL 3-byte integer (maps to INT32 internally)

        // String types (20-29)
        CHAR = 20,       // Fixed-length string (padded with spaces)
        VARCHAR = 21,    // Variable-length string (max length specified)
        TEXT = 22,       // Unlimited variable-length string

        // Binary types (30-39)
        BINARY = 30,     // Fixed-length binary data
        VARBINARY = 31,  // Variable-length binary data
        BLOB = 32,       // Binary large object
        TINYBLOB = 32,   // Alias for BLOB (MySQL compatibility)
        MEDIUMBLOB = 32, // Alias for BLOB (MySQL compatibility)
        LONGBLOB = 32,   // Alias for BLOB (MySQL compatibility)
        BYTEA = 33,      // PostgreSQL-style binary data

        // Date/Time types (40-49)
        DATE = 40,       // Date (year, month, day)
        TIME = 41,       // Time of day (hour, minute, second, microsecond)
        TIMESTAMP = 42,  // Date + time (with optional timezone)
        TIMESTAMP_WITH_ZONE = 43, // Timestamp with timezone (TIMESTAMPTZ)
        TIME_WITH_ZONE = 44,      // Time with timezone
        INTERVAL = 45,   // Time interval (years, months, days, hours, etc.)
        DATETIME = 46,   // MySQL DATETIME (alias for TIMESTAMP)
        YEAR = 47,       // MySQL YEAR type

        // Boolean (50-59)
        BOOLEAN = 50,    // True/false
        BIT = 51,        // Bit string type

        // Special types (60-69)
        UUID = 60,       // 128-bit UUID (RFC 4122)
        JSON = 61,       // JSON document (stored as text, validated)
        JSONB = 62,      // Binary JSON (optimized storage and indexing)
        XML = 63,        // XML document
        VECTOR = 64,     // Vector embeddings for similarity search (variable dimensions)

        // Spatial types (65-79)
        POINT = 65,              // Geometric point (x, y)
        LINESTRING = 66,         // Sequence of connected points
        POLYGON = 67,            // Closed polygon with optional holes
        MULTIPOINT = 68,         // Collection of POINT geometries
        MULTILINESTRING = 69,    // Collection of LINESTRING geometries
        MULTIPOLYGON = 70,       // Collection of POLYGON geometries
        GEOMETRYCOLLECTION = 71, // Heterogeneous collection of geometries
        GEOMETRY = 72,           // Generic geometry type

        // Array and composite types (80-89)
        ARRAY = 80,      // Array of elements (homogeneous type)
        COMPOSITE = 81,  // Record/struct type (heterogeneous types)

        // Text search types (90-91)
        TSVECTOR = 90,   // Text search vector (document representation)
        TSQUERY = 91,    // Text search query (search expression)

        // Range types (92-97)
        INT4RANGE = 92,  // Range of INT32 values
        INT8RANGE = 93,  // Range of INT64 values
        NUMRANGE = 94,   // Range of DECIMAL/FLOAT64 values
        TSRANGE = 95,    // Range of TIMESTAMP values (without timezone)
        TSTZRANGE = 96,  // Range of TIMESTAMP values (with timezone)
        DATERANGE = 97,  // Range of DATE values

        // Network types (98-101)
        INET = 98,       // IPv4 or IPv6 address with optional subnet
        CIDR = 99,       // IPv4 or IPv6 network (strict CIDR notation)
        MACADDR = 100,   // 6-byte MAC address (EUI-48)
        MACADDR8 = 101,  // 8-byte MAC address (EUI-64)

        // User-defined types (102-109)
        DOMAIN = 102,    // Domain type
        ROW = 103,       // ROW type
        ENUM = 104,      // ENUM type (MySQL compatibility)
        SET = 105,       // SET type (MySQL compatibility)

        // Polymorphic types (110-119)
        VARIANT = 110,   // Tagged union that can hold any type

        // Special blob subtypes
        BLOB_SUB_TYPE_TEXT = 120, // Firebird-style text BLOB

        // Null type (255)
        NULL_TYPE = 255, // SQL NULL
    };

    // CAST formatting for binary/text conversions.
    enum class CastFormat : uint8_t
    {
        DEFAULT = 0, // Engine default (hex for binary <-> text)
        HEX = 1,
        BASE64 = 2,
        ESCAPE = 3,
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
     * Spatial Type Structures
     *
     * Forward declarations for spatial types.
     * These structs are used to store spatial geometry data.
     */

    /**
     * Point - 2D geometric point with optional SRID
     */
    struct Point
    {
        double x;
        double y;
        int32_t srid;  // Spatial Reference ID (0 = undefined)

        Point() : x(0.0), y(0.0), srid(0) {}
        Point(double x_, double y_) : x(x_), y(y_), srid(0) {}
        Point(double x_, double y_, int32_t srid_) : x(x_), y(y_), srid(srid_) {}

        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }

        bool operator==(const Point& other) const {
            return x == other.x && y == other.y && srid == other.srid;
        }
        bool operator!=(const Point& other) const { return !(*this == other); }
    };

    /**
     * LineString - Sequence of connected points forming a line
     */
    struct LineString
    {
        std::vector<Point> points;
        int32_t srid;

        LineString() : srid(0) {}
        explicit LineString(std::vector<Point> pts) : points(std::move(pts)), srid(0) {}
        LineString(std::vector<Point> pts, int32_t srid_) : points(std::move(pts)), srid(srid_) {}

        bool isEmpty() const { return points.empty(); }
        size_t numPoints() const { return points.size(); }
        bool isValid() const { return points.size() >= 2; }
        bool isClosed() const {
            return !isEmpty() && points.front() == points.back();
        }

        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }

        bool operator==(const LineString& other) const {
            return points == other.points && srid == other.srid;
        }
        bool operator!=(const LineString& other) const { return !(*this == other); }
    };

    /**
     * Polygon - Closed polygon with optional holes
     */
    struct Polygon
    {
        std::vector<std::vector<Point>> rings;  // First ring = exterior, rest = holes
        int32_t srid;

        Polygon() : srid(0) {}
        explicit Polygon(std::vector<std::vector<Point>> rings_)
            : rings(std::move(rings_)), srid(0) {}
        Polygon(std::vector<std::vector<Point>> rings_, int32_t srid_)
            : rings(std::move(rings_)), srid(srid_) {}

        bool isEmpty() const { return rings.empty(); }
        size_t numRings() const { return rings.size(); }
        bool hasExteriorRing() const { return !rings.empty(); }
        bool hasHoles() const { return rings.size() > 1; }

        bool isValid() const {
            if (rings.empty()) return true;
            // Exterior ring must have at least 4 points (3 + closing point)
            if (rings[0].size() < 4) return false;
            // All rings must be closed
            for (const auto& ring : rings) {
                if (ring.empty() || ring.front() != ring.back()) {
                    return false;
                }
            }
            return true;
        }

        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }

        bool operator==(const Polygon& other) const {
            return rings == other.rings && srid == other.srid;
        }
        bool operator!=(const Polygon& other) const { return !(*this == other); }
    };

    /**
     * Interval - PostgreSQL-compatible interval type
     * Represents a time span with separate month, day, and microsecond components
     */
    struct Interval
    {
        int32_t months;         // Number of months
        int32_t days;           // Number of days
        int64_t microseconds;   // Number of microseconds

        Interval() : months(0), days(0), microseconds(0) {}
        Interval(int32_t m, int32_t d, int64_t us) : months(m), days(d), microseconds(us) {}

        bool operator==(const Interval& other) const {
            return months == other.months && days == other.days && microseconds == other.microseconds;
        }
        bool operator!=(const Interval& other) const { return !(*this == other); }
    };

    // Forward declare TypedValue for multi-geometry types
    class TypedValue;

    /**
     * MultiPoint - Collection of Point geometries
     */
    struct MultiPoint
    {
        std::vector<Point> points;
        int32_t srid;

        MultiPoint() : srid(0) {}
        explicit MultiPoint(std::vector<Point> pts) : points(std::move(pts)), srid(0) {}
        MultiPoint(std::vector<Point> pts, int32_t srid_) : points(std::move(pts)), srid(srid_) {}

        bool isEmpty() const { return points.empty(); }
        size_t numGeometries() const { return points.size(); }

        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }

        bool isValid() const { return true; }  // MultiPoint is always valid

        bool operator==(const MultiPoint& other) const {
            return points == other.points && srid == other.srid;
        }
        bool operator!=(const MultiPoint& other) const { return !(*this == other); }
    };

    /**
     * MultiLineString - Collection of LineString geometries
     */
    struct MultiLineString
    {
        std::vector<LineString> linestrings;
        int32_t srid;

        MultiLineString() : srid(0) {}
        explicit MultiLineString(std::vector<LineString> lines)
            : linestrings(std::move(lines)), srid(0) {}
        MultiLineString(std::vector<LineString> lines, int32_t srid_)
            : linestrings(std::move(lines)), srid(srid_) {}

        bool isEmpty() const { return linestrings.empty(); }
        size_t numGeometries() const { return linestrings.size(); }

        bool isValid() const {
            for (const auto& line : linestrings) {
                if (!line.isValid()) return false;
            }
            return true;
        }

        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }

        bool operator==(const MultiLineString& other) const {
            return linestrings == other.linestrings && srid == other.srid;
        }
        bool operator!=(const MultiLineString& other) const { return !(*this == other); }
    };

    /**
     * MultiPolygon - Collection of Polygon geometries
     */
    struct MultiPolygon
    {
        std::vector<Polygon> polygons;
        int32_t srid;

        MultiPolygon() : srid(0) {}
        explicit MultiPolygon(std::vector<Polygon> polys)
            : polygons(std::move(polys)), srid(0) {}
        MultiPolygon(std::vector<Polygon> polys, int32_t srid_)
            : polygons(std::move(polys)), srid(srid_) {}

        bool isEmpty() const { return polygons.empty(); }
        size_t numGeometries() const { return polygons.size(); }

        bool isValid() const {
            for (const auto& poly : polygons) {
                if (!poly.isValid()) return false;
            }
            return true;
        }

        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }

        bool operator==(const MultiPolygon& other) const {
            return polygons == other.polygons && srid == other.srid;
        }
        bool operator!=(const MultiPolygon& other) const { return !(*this == other); }
    };

    /**
     * GeometryCollection - Heterogeneous collection of any geometry types
     */
    struct GeometryCollection
    {
        std::vector<std::shared_ptr<TypedValue>> geometries;
        int32_t srid;

        GeometryCollection() : srid(0) {}
        explicit GeometryCollection(std::vector<std::shared_ptr<TypedValue>> geoms)
            : geometries(std::move(geoms)), srid(0) {}
        GeometryCollection(std::vector<std::shared_ptr<TypedValue>> geoms, int32_t srid_)
            : geometries(std::move(geoms)), srid(srid_) {}

        bool isEmpty() const { return geometries.empty(); }
        size_t numGeometries() const { return geometries.size(); }

        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }

        bool isValid() const { return true; }  // GeometryCollection is always valid

        bool operator==(const GeometryCollection& other) const;
        bool operator!=(const GeometryCollection& other) const { return !(*this == other); }
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

        // Check if a type is a string type
        static bool isString(DataType type)
        {
            return type == DataType::VARCHAR || type == DataType::TEXT ||
                   type == DataType::CHAR;
        }
    };

} // namespace scratchbird::core

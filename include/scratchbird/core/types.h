#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <optional>
#include <vector>
#include <memory>
#include <cmath>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/range.h"
#include "scratchbird/core/network.h"

namespace scratchbird::core
{
    // Forward declarations for text search types
    class TSVector;
    class TSQuery;

    // Forward declaration for array types
    class ArrayValue;

    // Forward declaration for vector distance metrics
    enum class DistanceMetric;

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
        POINT = 65,      // Geometric point (x, y)
        LINESTRING = 66, // Sequence of connected points
        POLYGON = 67,    // Closed polygon with optional holes
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
        uint16_t timezone_hint; // Display timezone ID for TIMESTAMP WITH TIME ZONE (0 = use
                                // connection default)

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

    // INT128 type support using compiler intrinsics
    #if defined(__SIZEOF_INT128__)
        using int128_t = __int128;
        using uint128_t = unsigned __int128;
        #define HAS_INT128 1
    #else
        // Fallback: use two int64_t for platforms without __int128
        struct int128_t {
            int64_t high;
            uint64_t low;
            int128_t() : high(0), low(0) {}
            int128_t(int64_t h, uint64_t l) : high(h), low(l) {}
        };
        struct uint128_t {
            uint64_t high;
            uint64_t low;
            uint128_t() : high(0), low(0) {}
            uint128_t(uint64_t h, uint64_t l) : high(h), low(l) {}
        };
        #define HAS_INT128 0
    #endif

    /**
     * Spatial types - Geometric primitives following OGC Simple Features specification
     */

    /**
     * POINT - Represents a 2D point in space
     * Total size: 20 bytes (2 x double + 1 x int32_t for SRID)
     * SRID: Spatial Reference Identifier (0 = undefined, 4326 = WGS84, etc.)
     */
    struct Point {
        double x;
        double y;
        int32_t srid;  // Spatial Reference Identifier (0 = undefined)

        // Constructors
        Point() : x(0.0), y(0.0), srid(0) {}
        Point(double x_, double y_) : x(x_), y(y_), srid(0) {}
        Point(double x_, double y_, int32_t srid_) : x(x_), y(y_), srid(srid_) {}

        // Comparison operators
        bool operator==(const Point& other) const {
            return x == other.x && y == other.y && srid == other.srid;
        }
        bool operator!=(const Point& other) const {
            return !(*this == other);
        }

        // SRID accessors
        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }
    };

    /**
     * LINESTRING - Represents a sequence of connected points
     * Variable size: vector of Points + SRID
     * SRID: Spatial Reference Identifier (0 = undefined, 4326 = WGS84, etc.)
     */
    struct LineString {
        std::vector<Point> points;
        int32_t srid;  // Spatial Reference Identifier (0 = undefined)

        // Constructors
        LineString() : srid(0) {}
        explicit LineString(std::vector<Point> pts) : points(std::move(pts)), srid(0) {}
        LineString(std::vector<Point> pts, int32_t srid_) : points(std::move(pts)), srid(srid_) {}

        // Validation
        bool isValid() const {
            return points.size() >= 2; // Minimum 2 points for a line
        }

        // Comparison operators
        bool operator==(const LineString& other) const {
            return points == other.points && srid == other.srid;
        }
        bool operator!=(const LineString& other) const {
            return !(*this == other);
        }

        // SRID accessors
        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }
    };

    /**
     * POLYGON - Represents a closed polygon with optional interior rings (holes)
     * Variable size: vector of rings, each ring is a vector of Points + SRID
     * First ring is exterior, subsequent rings are holes
     * SRID: Spatial Reference Identifier (0 = undefined, 4326 = WGS84, etc.)
     */
    struct Polygon {
        std::vector<std::vector<Point>> rings; // First is exterior, rest are holes
        int32_t srid;  // Spatial Reference Identifier (0 = undefined)

        // Constructors
        Polygon() : srid(0) {}
        explicit Polygon(std::vector<Point> exterior) : srid(0) {
            rings.push_back(std::move(exterior));
        }
        Polygon(std::vector<Point> exterior, int32_t srid_) : srid(srid_) {
            rings.push_back(std::move(exterior));
        }
        explicit Polygon(std::vector<std::vector<Point>> rgs) : rings(std::move(rgs)), srid(0) {}
        Polygon(std::vector<std::vector<Point>> rgs, int32_t srid_) : rings(std::move(rgs)), srid(srid_) {}

        // Validation
        bool isValid() const {
            if (rings.empty()) return false;
            // Exterior ring must have at least 4 points (3 unique + closing point)
            if (rings[0].size() < 4) return false;
            // Must be closed (first point == last point)
            if (rings[0].front() != rings[0].back()) return false;
            // Each hole must also be valid and closed
            for (size_t i = 1; i < rings.size(); ++i) {
                if (rings[i].size() < 4) return false;
                if (rings[i].front() != rings[i].back()) return false;
            }
            return true;
        }

        // Comparison operators
        bool operator==(const Polygon& other) const {
            return rings == other.rings && srid == other.srid;
        }
        bool operator!=(const Polygon& other) const {
            return !(*this == other);
        }

        // Accessors
        const std::vector<Point>& exteriorRing() const {
            return rings[0];
        }
        size_t numInteriorRings() const {
            return rings.size() > 0 ? rings.size() - 1 : 0;
        }
        const std::vector<Point>& interiorRing(size_t index) const {
            return rings[index + 1];
        }

        // SRID accessors
        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }
    };

    /**
     * MULTIPOINT type - collection of Point geometries
     * OGC Simple Features compliant
     */
    struct MultiPoint {
        std::vector<Point> points;
        int32_t srid;

        // Constructors
        MultiPoint() : srid(0) {}
        explicit MultiPoint(std::vector<Point> pts)
            : points(std::move(pts)), srid(0) {}
        MultiPoint(std::vector<Point> pts, int32_t srid_)
            : points(std::move(pts)), srid(srid_) {}

        // Validation
        bool isEmpty() const { return points.empty(); }
        size_t numGeometries() const { return points.size(); }

        bool isValid() const {
            // All points must be valid (2D coordinates)
            for (const auto& pt : points) {
                if (!std::isfinite(pt.x) || !std::isfinite(pt.y))
                    return false;
            }
            return true;
        }

        // SRID accessors
        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }

        // Comparison
        bool operator==(const MultiPoint& other) const {
            return points == other.points && srid == other.srid;
        }
        bool operator!=(const MultiPoint& other) const {
            return !(*this == other);
        }
    };

    /**
     * MULTILINESTRING type - collection of LineString geometries
     * OGC Simple Features compliant
     */
    struct MultiLineString {
        std::vector<LineString> linestrings;
        int32_t srid;

        // Constructors
        MultiLineString() : srid(0) {}
        explicit MultiLineString(std::vector<LineString> lines)
            : linestrings(std::move(lines)), srid(0) {}
        MultiLineString(std::vector<LineString> lines, int32_t srid_)
            : linestrings(std::move(lines)), srid(srid_) {}

        // Validation
        bool isEmpty() const { return linestrings.empty(); }
        size_t numGeometries() const { return linestrings.size(); }

        bool isValid() const {
            for (const auto& line : linestrings) {
                if (!line.isValid())
                    return false;
            }
            return true;
        }

        // Closed if all constituent linestrings are closed
        bool isClosed() const {
            if (isEmpty()) return false;
            for (const auto& line : linestrings) {
                if (line.points.empty() ||
                    line.points.front() != line.points.back())
                    return false;
            }
            return true;
        }

        // SRID accessors
        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }

        bool operator==(const MultiLineString& other) const {
            return linestrings == other.linestrings && srid == other.srid;
        }
        bool operator!=(const MultiLineString& other) const {
            return !(*this == other);
        }
    };

    /**
     * MULTIPOLYGON type - collection of Polygon geometries
     * OGC Simple Features compliant
     */
    struct MultiPolygon {
        std::vector<Polygon> polygons;
        int32_t srid;

        // Constructors
        MultiPolygon() : srid(0) {}
        explicit MultiPolygon(std::vector<Polygon> polys)
            : polygons(std::move(polys)), srid(0) {}
        MultiPolygon(std::vector<Polygon> polys, int32_t srid_)
            : polygons(std::move(polys)), srid(srid_) {}

        // Validation
        bool isEmpty() const { return polygons.empty(); }
        size_t numGeometries() const { return polygons.size(); }

        bool isValid() const {
            // Each polygon must be valid
            for (const auto& poly : polygons) {
                if (!poly.isValid())
                    return false;
            }
            return true;
        }

        // SRID accessors
        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }

        bool operator==(const MultiPolygon& other) const {
            return polygons == other.polygons && srid == other.srid;
        }
        bool operator!=(const MultiPolygon& other) const {
            return !(*this == other);
        }
    };

    // Forward declaration needed for GeometryCollection
    class TypedValue;

    /**
     * GEOMETRYCOLLECTION type - heterogeneous collection of any geometry types
     * OGC Simple Features compliant
     * Can contain Point, LineString, Polygon, and any Multi* types
     * Note: Requires TypedValue forward declaration
     */
    struct GeometryCollection {
        std::vector<std::shared_ptr<TypedValue>> geometries;
        int32_t srid;

        // Constructors
        GeometryCollection() : srid(0) {}
        explicit GeometryCollection(std::vector<std::shared_ptr<TypedValue>> geoms)
            : geometries(std::move(geoms)), srid(0) {}
        GeometryCollection(std::vector<std::shared_ptr<TypedValue>> geoms, int32_t srid_)
            : geometries(std::move(geoms)), srid(srid_) {}

        // Validation
        bool isEmpty() const { return geometries.empty(); }
        size_t numGeometries() const { return geometries.size(); }

        bool isValid() const;  // Defined in types.cpp

        // SRID accessors
        int32_t getSRID() const { return srid; }
        void setSRID(int32_t new_srid) { srid = new_srid; }
        bool hasSRID() const { return srid != 0; }

        bool operator==(const GeometryCollection& other) const;  // Defined in types.cpp
        bool operator!=(const GeometryCollection& other) const {
            return !(*this == other);
        }
    };

    /**
     * INTERVAL type - represents time intervals
     *
     * Follows PostgreSQL interval model:
     * - months: Separate field for years/months (since month lengths vary)
     * - days: Separate field (since days can vary with DST)
     * - microseconds: Time component (hours, minutes, seconds, microseconds)
     *
     * Total size: 16 bytes (3 x int32_t + 1 x int64_t)
     */
    struct Interval {
        int32_t months;       // Number of months (can be negative)
        int32_t days;         // Number of days (can be negative)
        int64_t microseconds; // Time component in microseconds (can be negative)

        // Constructors
        Interval() : months(0), days(0), microseconds(0) {}
        Interval(int32_t m, int32_t d, int64_t us) : months(m), days(d), microseconds(us) {}

        // Comparison operators
        bool operator==(const Interval& other) const {
            return months == other.months && days == other.days && microseconds == other.microseconds;
        }
        bool operator!=(const Interval& other) const {
            return !(*this == other);
        }
    };

    // Forward declarations
    class TypedValue;
    class VectorValue;

    /**
     * Variant (Tagged Union) Type
     * Represents a polymorphic value that can hold any type at runtime
     * The actual type is tracked and can be checked/cast safely
     * Example: A column that might be INT, TEXT, or FLOAT
     */
    struct VariantValue {
        DataType actual_type;
        std::shared_ptr<TypedValue> value;

        VariantValue() : actual_type(DataType::NULL_TYPE), value(nullptr) {}
        VariantValue(DataType type, std::shared_ptr<TypedValue> val)
            : actual_type(type), value(std::move(val)) {}

        // Comparison operators
        bool operator==(const VariantValue& other) const;
        bool operator!=(const VariantValue& other) const {
            return !(*this == other);
        }
    };

    /**
     * Composite (Record/Struct) Type
     * Represents a heterogeneous collection of named fields
     * Example: ROW('Alice', 30, 'alice@example.com')
     */
    struct CompositeValue {
        std::vector<std::string> field_names;
        std::vector<std::shared_ptr<TypedValue>> field_values;

        CompositeValue() = default;
        CompositeValue(std::vector<std::string> names,
                      std::vector<std::shared_ptr<TypedValue>> values)
            : field_names(std::move(names))
            , field_values(std::move(values)) {}

        // Comparison operators
        bool operator==(const CompositeValue& other) const;
        bool operator!=(const CompositeValue& other) const {
            return !(*this == other);
        }
    };

    /**
     * Runtime value representation
     * Supports all database types with proper type safety
     *
     * Note: We store DATE, TIME, and TIMESTAMP all as int64_t, differentiated by the type_ field
     */
    class TypedValue
    {
    public:
        using VariantType =
            std::variant<std::monostate, // NULL_TYPE
                         int8_t,         // INT8
                         int16_t,        // INT16
                         int32_t,        // INT32
                         int64_t,        // INT64, DATE, TIME, TIMESTAMP, MONEY (differentiated by type_)
                         int128_t,       // INT128
                         uint8_t,        // UINT8
                         uint16_t,       // UINT16
                         uint32_t,       // UINT32
                         uint64_t,       // UINT64
                         float,          // FLOAT32
                         double,         // FLOAT64
                         Interval,       // INTERVAL
                         Point,          // POINT
                         LineString,     // LINESTRING
                         Polygon,        // POLYGON
                         MultiPoint,     // MULTIPOINT
                         MultiLineString, // MULTILINESTRING
                         MultiPolygon,   // MULTIPOLYGON
                         GeometryCollection, // GEOMETRYCOLLECTION
                         std::string,    // VARCHAR, TEXT, CHAR, DECIMAL (as string), JSON
                         std::vector<uint8_t>, // BINARY, VARBINARY, BLOB, BYTEA, UUID
                         bool,                  // BOOLEAN
                         std::shared_ptr<TSVector>,  // TSVECTOR
                         std::shared_ptr<TSQuery>,   // TSQUERY
                         Int4Range,       // INT4RANGE
                         Int8Range,       // INT8RANGE
                         NumRange,        // NUMRANGE
                         DateRange,       // DATERANGE
                         TSRange,         // TSRANGE
                         TSTZRange,       // TSTZRANGE
                         InetAddr,        // INET
                         Cidr,            // CIDR
                         MacAddr,         // MACADDR
                         MacAddr8,        // MACADDR8
                         CompositeValue,  // COMPOSITE
                         std::shared_ptr<VectorValue>,  // VECTOR
                         VariantValue,    // VARIANT
                         std::shared_ptr<ArrayValue>    // ARRAY
                         >;

        TypedValue() : type_(DataType::NULL_TYPE), data_(std::monostate{}) {}

        explicit TypedValue(DataType type) : type_(type), data_(std::monostate{}) {}

        // Constructors for each type
        static TypedValue makeNull()
        {
            return TypedValue();
        }
        static TypedValue makeInt8(int8_t v);
        static TypedValue makeInt16(int16_t v);
        static TypedValue makeInt32(int32_t v);
        static TypedValue makeInt64(int64_t v);
        static TypedValue makeInt128(int128_t v);
        static TypedValue makeUInt8(uint8_t v);
        static TypedValue makeUInt16(uint16_t v);
        static TypedValue makeUInt32(uint32_t v);
        static TypedValue makeUInt64(uint64_t v);
        static TypedValue makeFloat32(float v);
        static TypedValue makeFloat64(double v);
        static TypedValue makeDecimal(const std::string &v);
        static TypedValue makeMoney(int64_t cents);
        static TypedValue makeChar(const std::string &v);
        static TypedValue makeVarchar(const std::string &v);
        static TypedValue makeText(const std::string &v);
        static TypedValue makeBinary(const std::vector<uint8_t> &v);
        static TypedValue makeBinary(const uint8_t *data, size_t len);
        static TypedValue makeBoolean(bool v);
        static TypedValue makeDate(int64_t days_since_epoch);
        static TypedValue makeTime(int64_t microseconds_since_midnight);
        static TypedValue makeTimestamp(int64_t microseconds_since_epoch);
        static TypedValue makeInterval(const Interval &interval);
        static TypedValue makeInterval(int32_t months, int32_t days, int64_t microseconds);
        static TypedValue makeUUID(const std::vector<uint8_t> &v);
        static TypedValue makeUUID(const uint8_t *data, size_t len);
        static TypedValue makeJSON(const std::string &v);
        static TypedValue makePoint(const Point &v);
        static TypedValue makePoint(double x, double y);
        static TypedValue makeLineString(const LineString &v);
        static TypedValue makeLineString(const std::vector<Point> &points);
        static TypedValue makePolygon(const Polygon &v);
        static TypedValue makePolygon(const std::vector<Point> &exterior_ring);
        static TypedValue makePolygon(const std::vector<std::vector<Point>> &rings);
        static TypedValue makeMultiPoint(const MultiPoint &v);
        static TypedValue makeMultiPoint(const std::vector<Point> &points);
        static TypedValue makeMultiLineString(const MultiLineString &v);
        static TypedValue makeMultiLineString(const std::vector<LineString> &linestrings);
        static TypedValue makeMultiPolygon(const MultiPolygon &v);
        static TypedValue makeMultiPolygon(const std::vector<Polygon> &polygons);
        static TypedValue makeGeometryCollection(const GeometryCollection &v);
        static TypedValue makeGeometryCollection(const std::vector<std::shared_ptr<TypedValue>> &geometries);
        static TypedValue makeTSVector(const TSVector &v);
        static TypedValue makeTSVector(std::shared_ptr<TSVector> v);
        static TypedValue makeTSQuery(const TSQuery &v);
        static TypedValue makeTSQuery(std::shared_ptr<TSQuery> v);
        static TypedValue makeInt4Range(const Int4Range &v);
        static TypedValue makeInt8Range(const Int8Range &v);
        static TypedValue makeNumRange(const NumRange &v);
        static TypedValue makeDateRange(const DateRange &v);
        static TypedValue makeTSRange(const TSRange &v);
        static TypedValue makeTSTZRange(const TSTZRange &v);
        static TypedValue makeInet(const InetAddr &v);
        static TypedValue makeCidr(const Cidr &v);
        static TypedValue makeMacAddr(const MacAddr &v);
        static TypedValue makeMacAddr8(const MacAddr8 &v);
        static TypedValue makeComposite(const CompositeValue &v);
        static TypedValue makeComposite(std::vector<std::string> field_names,
                                       std::vector<TypedValue> field_values);
        static TypedValue makeVector(const VectorValue &v);
        static TypedValue makeVector(std::shared_ptr<VectorValue> v);
        static TypedValue makeVector(const std::vector<float> &values);
        static TypedValue makeVector(const std::vector<double> &values);
        static TypedValue makeVariant(const VariantValue &v);
        static TypedValue makeVariant(const TypedValue &value);
        static TypedValue makeVariant(DataType actual_type, const TypedValue &value);
        static TypedValue makeArray(const ArrayValue &v);
        static TypedValue makeArray(std::shared_ptr<ArrayValue> v);

        // Type checking
        DataType type() const
        {
            return type_;
        }
        bool isNull() const
        {
            return type_ == DataType::NULL_TYPE;
        }

        // Type extraction
        int8_t getInt8() const;
        int16_t getInt16() const;
        int32_t getInt32() const;
        int64_t getInt64() const;
        int128_t getInt128() const;
        uint8_t getUInt8() const;
        uint16_t getUInt16() const;
        uint32_t getUInt32() const;
        uint64_t getUInt64() const;
        float getFloat32() const;
        double getFloat64() const;
        std::string getDecimal() const;
        int64_t getMoney() const;
        std::string getChar() const;
        std::string getVarchar() const;
        std::string getText() const;
        std::vector<uint8_t> getBinary() const;
        bool getBoolean() const;
        int64_t getDate() const;
        int64_t getTime() const;
        int64_t getTimestamp() const;
        Interval getInterval() const;
        std::vector<uint8_t> getUUID() const;
        std::string getJSON() const;
        Point getPoint() const;
        LineString getLineString() const;
        Polygon getPolygon() const;
        MultiPoint getMultiPoint() const;
        MultiLineString getMultiLineString() const;
        MultiPolygon getMultiPolygon() const;
        GeometryCollection getGeometryCollection() const;
        std::shared_ptr<TSVector> getTSVector() const;
        std::shared_ptr<TSQuery> getTSQuery() const;
        Int4Range getInt4Range() const;
        Int8Range getInt8Range() const;
        NumRange getNumRange() const;
        DateRange getDateRange() const;
        TSRange getTSRange() const;
        TSTZRange getTSTZRange() const;
        InetAddr getInet() const;
        Cidr getCidr() const;
        MacAddr getMacAddr() const;
        MacAddr8 getMacAddr8() const;
        const CompositeValue& getComposite() const;
        std::shared_ptr<VectorValue> getVector() const;
        const VariantValue& getVariant() const;
        std::shared_ptr<ArrayValue> getArray() const;

        // COMPOSITE field access methods
        TypedValue getField(const std::string& field_name) const;
        bool hasField(const std::string& field_name) const;
        size_t getFieldCount() const;
        const std::vector<std::string>& getFieldNames() const;

        // VECTOR element access methods
        TypedValue getVectorElement(size_t index) const;
        TypedValue getVectorSlice(size_t start, size_t end) const;
        size_t getVectorDimensions() const;

        // VECTOR distance operators
        TypedValue vectorDistance(const TypedValue& other, DistanceMetric metric) const;
        TypedValue vectorEuclideanDistance(const TypedValue& other) const;
        TypedValue vectorManhattanDistance(const TypedValue& other) const;
        TypedValue vectorCosineSimilarity(const TypedValue& other) const;
        TypedValue vectorDotProduct(const TypedValue& other) const;

        // VARIANT type operations
        DataType getVariantActualType() const;
        TypedValue unwrapVariant() const;
        bool variantIs(DataType expected_type) const;
        TypedValue variantCast(DataType target_type) const;

        // Generic string conversion (for display)
        std::string toString() const;

        // Convenience conversion methods (for backwards compatibility)
        int64_t toInt64() const;
        double toDouble() const;
        bool toBoolean() const;

        // Type conversions with error handling
        auto convertTo(DataType target_type, ErrorContext *ctx = nullptr) const
            -> std::optional<TypedValue>;

        // Type coercion (implicit conversion with validation)
        auto coerceTo(DataType target_type, ErrorContext *ctx = nullptr) const
            -> std::optional<TypedValue>;

        // Comparison operators (returns NULL for incompatible types)
        auto equals(const TypedValue &other) const -> std::optional<bool>;
        auto lessThan(const TypedValue &other) const -> std::optional<bool>;
        auto greaterThan(const TypedValue &other) const -> std::optional<bool>;

        // Hash for use in hash tables/indexes
        size_t hash() const;

    private:
        DataType type_;
        VariantType data_;
        std::optional<TypeInfo> type_info_; // Optional type metadata (for VARCHAR max_length,
                                            // DECIMAL precision/scale, etc.)

        TypedValue(DataType type, VariantType data)
            : type_(type), data_(std::move(data)), type_info_(std::nullopt)
        {
        }

    public:
        // Get/set type info (for preserving constraints like VARCHAR max_length)
        auto getTypeInfo() const -> const std::optional<TypeInfo> &
        {
            return type_info_;
        }
        void setTypeInfo(const TypeInfo &info)
        {
            type_info_ = info;
        }
        bool hasTypeInfo() const
        {
            return type_info_.has_value();
        }

    private:
        // Helper methods for conversion
        auto convertNumericTo(DataType target_type, ErrorContext *ctx = nullptr) const
            -> std::optional<TypedValue>;
        auto convertStringTo(DataType target_type, ErrorContext *ctx = nullptr) const
            -> std::optional<TypedValue>;
    };

    /**
     * Type system utilities
     */
    class TypeSystem
    {
    public:
        // Type properties
        static bool isNumeric(DataType type);
        static bool isInteger(DataType type);
        static bool isFloatingPoint(DataType type);
        static bool isString(DataType type);
        static bool isBinary(DataType type);
        static bool isTemporal(DataType type);
        static bool isFixedLength(DataType type);
        static bool isVariableLength(DataType type);

        // Type sizes
        static auto getFixedSize(DataType type) -> std::optional<uint32_t>;
        static auto getMinSize(DataType type) -> uint32_t;
        static auto getMaxSize(DataType type) -> std::optional<uint32_t>;

        // Type names
        static auto getTypeName(DataType type) -> std::string;
        static auto parseTypeName(const std::string &name) -> std::optional<DataType>;

        // Type compatibility
        static bool isCompatible(DataType from, DataType to);
        static bool isImplicitlyConvertible(DataType from, DataType to);
        static bool isExplicitlyConvertible(DataType from, DataType to);

        // Type coercion precedence (for binary operations)
        static auto getCoercionPrecedence(DataType type) -> int;
        static auto getCommonType(DataType left, DataType right) -> std::optional<DataType>;

        // Validation
        static auto validateValue(const TypedValue &value, const TypeInfo &type_info,
                                  ErrorContext *ctx = nullptr) -> Status;

        // Serialization size calculation
        static auto getSerializedSize(const TypedValue &value) -> uint32_t;
    };

    /**
     * Type conversion functions
     */
    class TypeConverter
    {
    public:
        // String conversions
        static auto stringToInt8(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<int8_t>;
        static auto stringToInt16(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<int16_t>;
        static auto stringToInt32(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<int32_t>;
        static auto stringToInt64(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<int64_t>;
        static auto stringToFloat32(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<float>;
        static auto stringToFloat64(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<double>;
        static auto stringToDecimal(const std::string &str, uint32_t precision, uint32_t scale,
                                    ErrorContext *ctx = nullptr) -> std::optional<std::string>;
        static auto stringToBoolean(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<bool>;
        static auto stringToDate(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<int64_t>;
        static auto stringToTime(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<int64_t>;
        static auto stringToTimestamp(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<int64_t>;
        static auto stringToUUID(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<std::vector<uint8_t>>;
        static auto stringToBinary(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<std::vector<uint8_t>>;

        // To string conversions
        static auto int8ToString(int8_t v) -> std::string;
        static auto int16ToString(int16_t v) -> std::string;
        static auto int32ToString(int32_t v) -> std::string;
        static auto int64ToString(int64_t v) -> std::string;
        static auto int128ToString(int128_t v) -> std::string;
        static auto uint8ToString(uint8_t v) -> std::string;
        static auto uint16ToString(uint16_t v) -> std::string;
        static auto uint32ToString(uint32_t v) -> std::string;
        static auto uint64ToString(uint64_t v) -> std::string;
        static auto float32ToString(float v) -> std::string;
        static auto float64ToString(double v) -> std::string;
        static auto moneyToString(int64_t cents) -> std::string;
        static auto booleanToString(bool v) -> std::string;
        static auto dateToString(int64_t days) -> std::string;
        static auto timeToString(int64_t microseconds) -> std::string;
        static auto timestampToString(int64_t microseconds) -> std::string;
        static auto intervalToString(const Interval &interval) -> std::string;
        static auto uuidToString(const std::vector<uint8_t> &uuid) -> std::string;
        static auto binaryToHex(const std::vector<uint8_t> &data) -> std::string;
        static auto binaryToBase64(const std::vector<uint8_t> &data) -> std::string;

        // Numeric conversions with overflow detection
        static auto int8ToInt16(int8_t v) -> int16_t;
        static auto int8ToInt32(int8_t v) -> int32_t;
        static auto int8ToInt64(int8_t v) -> int64_t;
        static auto int16ToInt8(int16_t v, ErrorContext *ctx = nullptr) -> std::optional<int8_t>;
        static auto int16ToInt32(int16_t v) -> int32_t;
        static auto int16ToInt64(int16_t v) -> int64_t;
        static auto int32ToInt8(int32_t v, ErrorContext *ctx = nullptr) -> std::optional<int8_t>;
        static auto int32ToInt16(int32_t v, ErrorContext *ctx = nullptr) -> std::optional<int16_t>;
        static auto int32ToInt64(int32_t v) -> int64_t;
        static auto int64ToInt8(int64_t v, ErrorContext *ctx = nullptr) -> std::optional<int8_t>;
        static auto int64ToInt16(int64_t v, ErrorContext *ctx = nullptr) -> std::optional<int16_t>;
        static auto int64ToInt32(int64_t v, ErrorContext *ctx = nullptr) -> std::optional<int32_t>;

        // Float conversions
        static auto intToFloat32(int64_t v) -> float;
        static auto intToFloat64(int64_t v) -> double;
        static auto float32ToFloat64(float v) -> double;
        static auto float64ToFloat32(double v, ErrorContext *ctx = nullptr) -> std::optional<float>;
        static auto floatToInt(double v, ErrorContext *ctx = nullptr) -> std::optional<int64_t>;

        // JSON validation
        static auto validateJSON(const std::string &json, ErrorContext *ctx = nullptr) -> Status;
    };

    /**
     * Type extraction functions (for DATE, TIME, TIMESTAMP, UUID, etc.)
     */
    class TypeExtractor
    {
    public:
        // Date/Time extraction
        static auto extractYear(int64_t days_since_epoch) -> int32_t;
        static auto extractMonth(int64_t days_since_epoch) -> int32_t;
        static auto extractDay(int64_t days_since_epoch) -> int32_t;
        static auto extractDayOfWeek(int64_t days_since_epoch) -> int32_t;
        static auto extractDayOfYear(int64_t days_since_epoch) -> int32_t;

        static auto extractHour(int64_t microseconds) -> int32_t;
        static auto extractMinute(int64_t microseconds) -> int32_t;
        static auto extractSecond(int64_t microseconds) -> int32_t;
        static auto extractMicrosecond(int64_t microseconds) -> int32_t;

        static auto extractTimestampYear(int64_t microseconds_since_epoch) -> int32_t;
        static auto extractTimestampMonth(int64_t microseconds_since_epoch) -> int32_t;
        static auto extractTimestampDay(int64_t microseconds_since_epoch) -> int32_t;
        static auto extractTimestampHour(int64_t microseconds_since_epoch) -> int32_t;
        static auto extractTimestampMinute(int64_t microseconds_since_epoch) -> int32_t;
        static auto extractTimestampSecond(int64_t microseconds_since_epoch) -> int32_t;
        static auto extractTimestampMicrosecond(int64_t microseconds_since_epoch) -> int32_t;

        // UUID extraction
        static auto extractUUIDVersion(const std::vector<uint8_t> &uuid) -> int32_t;
        static auto extractUUIDVariant(const std::vector<uint8_t> &uuid) -> int32_t;
        static auto extractUUIDTimestamp(const std::vector<uint8_t> &uuid,
                                         ErrorContext *ctx = nullptr) -> std::optional<int64_t>;
    };

} // namespace scratchbird::core

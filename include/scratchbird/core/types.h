#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <optional>
#include <vector>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird::core
{

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

        // Array and composite types (70-79)
        ARRAY = 70,     // Array of elements (homogeneous type)
        COMPOSITE = 71, // Record/struct type (heterogeneous types)

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
                         std::string,    // VARCHAR, TEXT, CHAR, DECIMAL (as string), JSON
                         std::vector<uint8_t>, // BINARY, VARBINARY, BLOB, BYTEA, UUID
                         bool                  // BOOLEAN
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

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
        INT16 = 2,    // 2-byte signed integer
        INT32 = 3,    // 4-byte signed integer (alias: INTEGER, INT)
        INT64 = 4,    // 8-byte signed integer (alias: BIGINT)
        FLOAT32 = 5,  // 4-byte IEEE 754 float (alias: REAL, FLOAT)
        FLOAT64 = 6,  // 8-byte IEEE 754 double (alias: DOUBLE)
        DECIMAL = 7,  // Fixed-precision decimal (precision, scale)

        // String types (10-19)
        CHAR = 10,    // Fixed-length string (padded with spaces)
        VARCHAR = 11, // Variable-length string (max length specified)
        TEXT = 12,    // Unlimited variable-length string

        // Binary types (20-29)
        BINARY = 20,    // Fixed-length binary data
        VARBINARY = 21, // Variable-length binary data
        BLOB = 22,      // Binary large object
        BYTEA = 23,     // PostgreSQL-style binary data

        // Date/Time types (30-39)
        DATE = 30,      // Date (year, month, day)
        TIME = 31,      // Time of day (hour, minute, second, microsecond)
        TIMESTAMP = 32, // Date + time (with optional timezone)
        INTERVAL = 33,  // Time interval (years, months, days, hours, etc.)

        // Boolean (40-49)
        BOOLEAN = 40, // True/false

        // Special types (50-59)
        UUID = 50, // 128-bit UUID (RFC 4122)
        JSON = 51, // JSON document (stored as text, validated)

        // Array and composite types (60-69)
        ARRAY = 60,     // Array of elements (homogeneous type)
        COMPOSITE = 61, // Record/struct type (heterogeneous types)

        // Null type (255)
        NULL_TYPE = 255, // SQL NULL
    };

    /**
     * Type metadata - stores additional information about a type
     */
    struct TypeInfo
    {
        DataType type;
        uint32_t precision; // For CHAR, VARCHAR, DECIMAL
        uint32_t scale;     // For DECIMAL
        DataType element_type; // For ARRAY
        bool with_timezone; // For TIMESTAMP

        TypeInfo()
            : type(DataType::UNKNOWN), precision(0), scale(0),
              element_type(DataType::UNKNOWN), with_timezone(false) {}

        TypeInfo(DataType t)
            : type(t), precision(0), scale(0),
              element_type(DataType::UNKNOWN), with_timezone(false) {}

        TypeInfo(DataType t, uint32_t p)
            : type(t), precision(p), scale(0),
              element_type(DataType::UNKNOWN), with_timezone(false) {}

        TypeInfo(DataType t, uint32_t p, uint32_t s)
            : type(t), precision(p), scale(s),
              element_type(DataType::UNKNOWN), with_timezone(false) {}
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
        using VariantType = std::variant<
            std::monostate,       // NULL_TYPE
            int8_t,               // INT8
            int16_t,              // INT16
            int32_t,              // INT32
            int64_t,              // INT64, DATE, TIME, TIMESTAMP (differentiated by type_)
            float,                // FLOAT32
            double,               // FLOAT64
            std::string,          // VARCHAR, TEXT, CHAR, DECIMAL (as string), JSON
            std::vector<uint8_t>, // BINARY, VARBINARY, BLOB, BYTEA, UUID
            bool                  // BOOLEAN
        >;

        TypedValue() : type_(DataType::NULL_TYPE), data_(std::monostate{}) {}

        explicit TypedValue(DataType type) : type_(type), data_(std::monostate{}) {}

        // Constructors for each type
        static TypedValue makeNull() { return TypedValue(); }
        static TypedValue makeInt8(int8_t v);
        static TypedValue makeInt16(int16_t v);
        static TypedValue makeInt32(int32_t v);
        static TypedValue makeInt64(int64_t v);
        static TypedValue makeFloat32(float v);
        static TypedValue makeFloat64(double v);
        static TypedValue makeDecimal(const std::string &v);
        static TypedValue makeChar(const std::string &v);
        static TypedValue makeVarchar(const std::string &v);
        static TypedValue makeText(const std::string &v);
        static TypedValue makeBinary(const std::vector<uint8_t> &v);
        static TypedValue makeBinary(const uint8_t *data, size_t len);
        static TypedValue makeBoolean(bool v);
        static TypedValue makeDate(int64_t days_since_epoch);
        static TypedValue makeTime(int64_t microseconds_since_midnight);
        static TypedValue makeTimestamp(int64_t microseconds_since_epoch);
        static TypedValue makeUUID(const std::vector<uint8_t> &v);
        static TypedValue makeUUID(const uint8_t *data, size_t len);
        static TypedValue makeJSON(const std::string &v);

        // Type checking
        DataType type() const { return type_; }
        bool isNull() const { return type_ == DataType::NULL_TYPE; }

        // Type extraction
        int8_t getInt8() const;
        int16_t getInt16() const;
        int32_t getInt32() const;
        int64_t getInt64() const;
        float getFloat32() const;
        double getFloat64() const;
        std::string getDecimal() const;
        std::string getChar() const;
        std::string getVarchar() const;
        std::string getText() const;
        std::vector<uint8_t> getBinary() const;
        bool getBoolean() const;
        int64_t getDate() const;
        int64_t getTime() const;
        int64_t getTimestamp() const;
        std::vector<uint8_t> getUUID() const;
        std::string getJSON() const;

        // Generic string conversion (for display)
        std::string toString() const;

        // Convenience conversion methods (for backwards compatibility)
        int64_t toInt64() const;
        double toDouble() const;
        bool toBoolean() const;

        // Type conversions with error handling
        auto convertTo(DataType target_type, ErrorContext *ctx = nullptr) const -> std::optional<TypedValue>;

        // Type coercion (implicit conversion with validation)
        auto coerceTo(DataType target_type, ErrorContext *ctx = nullptr) const -> std::optional<TypedValue>;

        // Comparison operators (returns NULL for incompatible types)
        auto equals(const TypedValue &other) const -> std::optional<bool>;
        auto lessThan(const TypedValue &other) const -> std::optional<bool>;
        auto greaterThan(const TypedValue &other) const -> std::optional<bool>;

        // Hash for use in hash tables/indexes
        size_t hash() const;

    private:
        DataType type_;
        VariantType data_;

        TypedValue(DataType type, VariantType data) : type_(type), data_(std::move(data)) {}

        // Helper methods for conversion
        auto convertNumericTo(DataType target_type, ErrorContext *ctx = nullptr) const -> std::optional<TypedValue>;
        auto convertStringTo(DataType target_type, ErrorContext *ctx = nullptr) const -> std::optional<TypedValue>;
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
        static auto stringToInt8(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<int8_t>;
        static auto stringToInt16(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<int16_t>;
        static auto stringToInt32(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<int32_t>;
        static auto stringToInt64(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<int64_t>;
        static auto stringToFloat32(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<float>;
        static auto stringToFloat64(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<double>;
        static auto stringToDecimal(const std::string &str, uint32_t precision, uint32_t scale,
                                     ErrorContext *ctx = nullptr) -> std::optional<std::string>;
        static auto stringToBoolean(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<bool>;
        static auto stringToDate(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<int64_t>;
        static auto stringToTime(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<int64_t>;
        static auto stringToTimestamp(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<int64_t>;
        static auto stringToUUID(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<std::vector<uint8_t>>;
        static auto stringToBinary(const std::string &str, ErrorContext *ctx = nullptr) -> std::optional<std::vector<uint8_t>>;

        // To string conversions
        static auto int8ToString(int8_t v) -> std::string;
        static auto int16ToString(int16_t v) -> std::string;
        static auto int32ToString(int32_t v) -> std::string;
        static auto int64ToString(int64_t v) -> std::string;
        static auto float32ToString(float v) -> std::string;
        static auto float64ToString(double v) -> std::string;
        static auto booleanToString(bool v) -> std::string;
        static auto dateToString(int64_t days) -> std::string;
        static auto timeToString(int64_t microseconds) -> std::string;
        static auto timestampToString(int64_t microseconds) -> std::string;
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

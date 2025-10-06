#include "scratchbird/core/types.h"
#include "scratchbird/core/timezone.h"
#include <cstring>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <charconv>

namespace scratchbird::core
{
    // Thread-local timezone manager for formatting
    static thread_local TimezoneManager g_tz_manager;

    // ===== TypedValue Implementation =====

    TypedValue TypedValue::makeInt8(int8_t v)
    {
        return TypedValue(DataType::INT8, v);
    }

    TypedValue TypedValue::makeInt16(int16_t v)
    {
        return TypedValue(DataType::INT16, v);
    }

    TypedValue TypedValue::makeInt32(int32_t v)
    {
        return TypedValue(DataType::INT32, v);
    }

    TypedValue TypedValue::makeInt64(int64_t v)
    {
        return TypedValue(DataType::INT64, v);
    }

    TypedValue TypedValue::makeFloat32(float v)
    {
        return TypedValue(DataType::FLOAT32, v);
    }

    TypedValue TypedValue::makeFloat64(double v)
    {
        return TypedValue(DataType::FLOAT64, v);
    }

    TypedValue TypedValue::makeDecimal(const std::string &v)
    {
        return TypedValue(DataType::DECIMAL, v);
    }

    TypedValue TypedValue::makeChar(const std::string &v)
    {
        return TypedValue(DataType::CHAR, v);
    }

    TypedValue TypedValue::makeVarchar(const std::string &v)
    {
        return TypedValue(DataType::VARCHAR, v);
    }

    TypedValue TypedValue::makeText(const std::string &v)
    {
        return TypedValue(DataType::TEXT, v);
    }

    TypedValue TypedValue::makeBinary(const std::vector<uint8_t> &v)
    {
        return TypedValue(DataType::BINARY, v);
    }

    TypedValue TypedValue::makeBinary(const uint8_t *data, size_t len)
    {
        std::vector<uint8_t> v(data, data + len);
        return TypedValue(DataType::BINARY, v);
    }

    TypedValue TypedValue::makeBoolean(bool v)
    {
        return TypedValue(DataType::BOOLEAN, v);
    }

    TypedValue TypedValue::makeDate(int64_t days_since_epoch)
    {
        return TypedValue(DataType::DATE, days_since_epoch);
    }

    TypedValue TypedValue::makeTime(int64_t microseconds_since_midnight)
    {
        return TypedValue(DataType::TIME, microseconds_since_midnight);
    }

    TypedValue TypedValue::makeTimestamp(int64_t microseconds_since_epoch)
    {
        return TypedValue(DataType::TIMESTAMP, microseconds_since_epoch);
    }

    TypedValue TypedValue::makeUUID(const std::vector<uint8_t> &v)
    {
        return TypedValue(DataType::UUID, v);
    }

    TypedValue TypedValue::makeUUID(const uint8_t *data, size_t len)
    {
        std::vector<uint8_t> v(data, data + len);
        return TypedValue(DataType::UUID, v);
    }

    TypedValue TypedValue::makeJSON(const std::string &v)
    {
        return TypedValue(DataType::JSON, v);
    }

    // Type extraction
    int8_t TypedValue::getInt8() const
    {
        if (type_ != DataType::INT8)
            throw std::runtime_error("Type mismatch: not INT8");
        return std::get<int8_t>(data_);
    }

    int16_t TypedValue::getInt16() const
    {
        if (type_ != DataType::INT16)
            throw std::runtime_error("Type mismatch: not INT16");
        return std::get<int16_t>(data_);
    }

    int32_t TypedValue::getInt32() const
    {
        if (type_ != DataType::INT32)
            throw std::runtime_error("Type mismatch: not INT32");
        return std::get<int32_t>(data_);
    }

    int64_t TypedValue::getInt64() const
    {
        if (type_ != DataType::INT64)
            throw std::runtime_error("Type mismatch: not INT64");
        return std::get<int64_t>(data_);
    }

    float TypedValue::getFloat32() const
    {
        if (type_ != DataType::FLOAT32)
            throw std::runtime_error("Type mismatch: not FLOAT32");
        return std::get<float>(data_);
    }

    double TypedValue::getFloat64() const
    {
        if (type_ != DataType::FLOAT64)
            throw std::runtime_error("Type mismatch: not FLOAT64");
        return std::get<double>(data_);
    }

    std::string TypedValue::getDecimal() const
    {
        if (type_ != DataType::DECIMAL)
            throw std::runtime_error("Type mismatch: not DECIMAL");
        return std::get<std::string>(data_);
    }

    std::string TypedValue::getChar() const
    {
        if (type_ != DataType::CHAR)
            throw std::runtime_error("Type mismatch: not CHAR");
        return std::get<std::string>(data_);
    }

    std::string TypedValue::getVarchar() const
    {
        if (type_ != DataType::VARCHAR)
            throw std::runtime_error("Type mismatch: not VARCHAR");
        return std::get<std::string>(data_);
    }

    std::string TypedValue::getText() const
    {
        if (type_ != DataType::TEXT)
            throw std::runtime_error("Type mismatch: not TEXT");
        return std::get<std::string>(data_);
    }

    std::vector<uint8_t> TypedValue::getBinary() const
    {
        if (type_ != DataType::BINARY && type_ != DataType::VARBINARY &&
            type_ != DataType::BLOB && type_ != DataType::BYTEA)
            throw std::runtime_error("Type mismatch: not a binary type");
        return std::get<std::vector<uint8_t>>(data_);
    }

    bool TypedValue::getBoolean() const
    {
        if (type_ != DataType::BOOLEAN)
            throw std::runtime_error("Type mismatch: not BOOLEAN");
        return std::get<bool>(data_);
    }

    int64_t TypedValue::getDate() const
    {
        if (type_ != DataType::DATE)
            throw std::runtime_error("Type mismatch: not DATE");
        return std::get<int64_t>(data_);
    }

    int64_t TypedValue::getTime() const
    {
        if (type_ != DataType::TIME)
            throw std::runtime_error("Type mismatch: not TIME");
        return std::get<int64_t>(data_);
    }

    int64_t TypedValue::getTimestamp() const
    {
        if (type_ != DataType::TIMESTAMP)
            throw std::runtime_error("Type mismatch: not TIMESTAMP");
        return std::get<int64_t>(data_);
    }

    std::vector<uint8_t> TypedValue::getUUID() const
    {
        if (type_ != DataType::UUID)
            throw std::runtime_error("Type mismatch: not UUID");
        return std::get<std::vector<uint8_t>>(data_);
    }

    std::string TypedValue::getJSON() const
    {
        if (type_ != DataType::JSON)
            throw std::runtime_error("Type mismatch: not JSON");
        return std::get<std::string>(data_);
    }

    std::string TypedValue::toString() const
    {
        if (isNull())
            return "NULL";

        switch (type_)
        {
        case DataType::INT8:
            return TypeConverter::int8ToString(getInt8());
        case DataType::INT16:
            return TypeConverter::int16ToString(getInt16());
        case DataType::INT32:
            return TypeConverter::int32ToString(getInt32());
        case DataType::INT64:
            return TypeConverter::int64ToString(getInt64());
        case DataType::FLOAT32:
            return TypeConverter::float32ToString(getFloat32());
        case DataType::FLOAT64:
            return TypeConverter::float64ToString(getFloat64());
        case DataType::DECIMAL:
            return getDecimal();
        case DataType::CHAR:
        case DataType::VARCHAR:
        case DataType::TEXT:
            return std::get<std::string>(data_);
        case DataType::BOOLEAN:
            return TypeConverter::booleanToString(getBoolean());
        case DataType::DATE:
            return TypeConverter::dateToString(getDate());
        case DataType::TIME:
            return TypeConverter::timeToString(getTime());
        case DataType::TIMESTAMP:
            return TypeConverter::timestampToString(getTimestamp());
        case DataType::UUID:
            return TypeConverter::uuidToString(getUUID());
        case DataType::BINARY:
        case DataType::VARBINARY:
        case DataType::BLOB:
        case DataType::BYTEA:
            return TypeConverter::binaryToHex(getBinary());
        case DataType::JSON:
            return getJSON();
        default:
            return "<unknown>";
        }
    }

    // ===== TypeSystem Implementation =====

    bool TypeSystem::isNumeric(DataType type)
    {
        return isInteger(type) || isFloatingPoint(type) ||
               type == DataType::DECIMAL || type == DataType::MONEY;
    }

    bool TypeSystem::isInteger(DataType type)
    {
        return type == DataType::INT8 || type == DataType::INT16 ||
               type == DataType::INT32 || type == DataType::INT64 ||
               type == DataType::INT128 ||
               type == DataType::UINT8 || type == DataType::UINT16 ||
               type == DataType::UINT32 || type == DataType::UINT64;
    }

    bool TypeSystem::isFloatingPoint(DataType type)
    {
        return type == DataType::FLOAT32 || type == DataType::FLOAT64;
    }

    bool TypeSystem::isString(DataType type)
    {
        return type == DataType::CHAR || type == DataType::VARCHAR || type == DataType::TEXT;
    }

    bool TypeSystem::isBinary(DataType type)
    {
        return type == DataType::BINARY || type == DataType::VARBINARY ||
               type == DataType::BLOB || type == DataType::BYTEA;
    }

    bool TypeSystem::isTemporal(DataType type)
    {
        return type == DataType::DATE || type == DataType::TIME ||
               type == DataType::TIMESTAMP || type == DataType::INTERVAL;
    }

    bool TypeSystem::isFixedLength(DataType type)
    {
        switch (type)
        {
        case DataType::INT8:
        case DataType::INT16:
        case DataType::INT32:
        case DataType::INT64:
        case DataType::FLOAT32:
        case DataType::FLOAT64:
        case DataType::DATE:
        case DataType::TIME:
        case DataType::TIMESTAMP:
        case DataType::BOOLEAN:
        case DataType::CHAR:
        case DataType::BINARY:
        case DataType::UUID:
            return true;
        default:
            return false;
        }
    }

    bool TypeSystem::isVariableLength(DataType type)
    {
        return isString(type) || isBinary(type) ||
               type == DataType::JSON || type == DataType::ARRAY ||
               type == DataType::COMPOSITE;
    }

    auto TypeSystem::getFixedSize(DataType type) -> std::optional<uint32_t>
    {
        switch (type)
        {
        case DataType::INT8:
            return 1;
        case DataType::INT16:
            return 2;
        case DataType::INT32:
            return 4;
        case DataType::INT64:
            return 8;
        case DataType::FLOAT32:
            return 4;
        case DataType::FLOAT64:
            return 8;
        case DataType::DATE:
            return 8;
        case DataType::TIME:
            return 8;
        case DataType::TIMESTAMP:
            return 8;
        case DataType::BOOLEAN:
            return 1;
        case DataType::UUID:
            return 16;
        default:
            return std::nullopt;
        }
    }

    auto TypeSystem::getMinSize(DataType type) -> uint32_t
    {
        auto fixed = getFixedSize(type);
        if (fixed)
            return *fixed;
        return 0; // Variable length types have min size 0
    }

    auto TypeSystem::getMaxSize(DataType type) -> std::optional<uint32_t>
    {
        auto fixed = getFixedSize(type);
        if (fixed)
            return fixed;
        return std::nullopt; // Variable length types have no max
    }

    auto TypeSystem::getTypeName(DataType type) -> std::string
    {
        switch (type)
        {
        case DataType::UNKNOWN:
            return "UNKNOWN";
        case DataType::INT8:
            return "INT8";
        case DataType::INT16:
            return "INT16";
        case DataType::INT32:
            return "INT32";
        case DataType::INT64:
            return "INT64";
        case DataType::INT128:
            return "INT128";
        case DataType::UINT8:
            return "UINT8";
        case DataType::UINT16:
            return "UINT16";
        case DataType::UINT32:
            return "UINT32";
        case DataType::UINT64:
            return "UINT64";
        case DataType::FLOAT32:
            return "FLOAT32";
        case DataType::FLOAT64:
            return "FLOAT64";
        case DataType::DECIMAL:
            return "DECIMAL";
        case DataType::MONEY:
            return "MONEY";
        case DataType::CHAR:
            return "CHAR";
        case DataType::VARCHAR:
            return "VARCHAR";
        case DataType::TEXT:
            return "TEXT";
        case DataType::BINARY:
            return "BINARY";
        case DataType::VARBINARY:
            return "VARBINARY";
        case DataType::BLOB:
            return "BLOB";
        case DataType::BYTEA:
            return "BYTEA";
        case DataType::DATE:
            return "DATE";
        case DataType::TIME:
            return "TIME";
        case DataType::TIMESTAMP:
            return "TIMESTAMP";
        case DataType::INTERVAL:
            return "INTERVAL";
        case DataType::BOOLEAN:
            return "BOOLEAN";
        case DataType::UUID:
            return "UUID";
        case DataType::JSON:
            return "JSON";
        case DataType::JSONB:
            return "JSONB";
        case DataType::XML:
            return "XML";
        case DataType::VECTOR:
            return "VECTOR";
        case DataType::ARRAY:
            return "ARRAY";
        case DataType::COMPOSITE:
            return "COMPOSITE";
        case DataType::NULL_TYPE:
            return "NULL";
        default:
            return "UNKNOWN";
        }
    }

    auto TypeSystem::parseTypeName(const std::string &name) -> std::optional<DataType>
    {
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        if (upper == "INT8" || upper == "TINYINT")
            return DataType::INT8;
        if (upper == "INT16" || upper == "SMALLINT")
            return DataType::INT16;
        if (upper == "INT32" || upper == "INT" || upper == "INTEGER")
            return DataType::INT32;
        if (upper == "INT64" || upper == "BIGINT")
            return DataType::INT64;
        if (upper == "INT128")
            return DataType::INT128;
        if (upper == "UINT8")
            return DataType::UINT8;
        if (upper == "UINT16")
            return DataType::UINT16;
        if (upper == "UINT32")
            return DataType::UINT32;
        if (upper == "UINT64")
            return DataType::UINT64;
        if (upper == "FLOAT32" || upper == "REAL" || upper == "FLOAT")
            return DataType::FLOAT32;
        if (upper == "FLOAT64" || upper == "DOUBLE")
            return DataType::FLOAT64;
        if (upper == "DECIMAL" || upper == "NUMERIC")
            return DataType::DECIMAL;
        if (upper == "MONEY")
            return DataType::MONEY;
        if (upper == "CHAR" || upper == "CHARACTER")
            return DataType::CHAR;
        if (upper == "VARCHAR" || upper == "CHARACTER VARYING")
            return DataType::VARCHAR;
        if (upper == "TEXT")
            return DataType::TEXT;
        if (upper == "BINARY")
            return DataType::BINARY;
        if (upper == "VARBINARY")
            return DataType::VARBINARY;
        if (upper == "BLOB")
            return DataType::BLOB;
        if (upper == "BYTEA")
            return DataType::BYTEA;
        if (upper == "DATE")
            return DataType::DATE;
        if (upper == "TIME")
            return DataType::TIME;
        if (upper == "TIMESTAMP")
            return DataType::TIMESTAMP;
        if (upper == "INTERVAL")
            return DataType::INTERVAL;
        if (upper == "BOOLEAN" || upper == "BOOL")
            return DataType::BOOLEAN;
        if (upper == "UUID")
            return DataType::UUID;
        if (upper == "JSON")
            return DataType::JSON;
        if (upper == "JSONB")
            return DataType::JSONB;
        if (upper == "XML")
            return DataType::XML;
        if (upper == "VECTOR")
            return DataType::VECTOR;

        return std::nullopt;
    }

    bool TypeSystem::isCompatible(DataType from, DataType to)
    {
        if (from == to)
            return true;

        // Numeric types are compatible with each other
        if (isNumeric(from) && isNumeric(to))
            return true;

        // String types are compatible with each other
        if (isString(from) && isString(to))
            return true;

        // Binary types are compatible with each other
        if (isBinary(from) && isBinary(to))
            return true;

        return false;
    }

    bool TypeSystem::isImplicitlyConvertible(DataType from, DataType to)
    {
        if (from == to)
            return true;

        // Integer widening is implicit
        if (from == DataType::INT8 && (to == DataType::INT16 || to == DataType::INT32 || to == DataType::INT64))
            return true;
        if (from == DataType::INT16 && (to == DataType::INT32 || to == DataType::INT64))
            return true;
        if (from == DataType::INT32 && to == DataType::INT64)
            return true;

        // Integer to float is implicit
        if (isInteger(from) && isFloatingPoint(to))
            return true;

        // FLOAT32 to FLOAT64 is implicit
        if (from == DataType::FLOAT32 && to == DataType::FLOAT64)
            return true;

        // VARCHAR and TEXT are implicitly convertible
        if ((from == DataType::VARCHAR || from == DataType::CHAR) && to == DataType::TEXT)
            return true;

        return false;
    }

    bool TypeSystem::isExplicitlyConvertible(DataType from, DataType to)
    {
        // All numeric types can be explicitly converted
        if (isNumeric(from) && isNumeric(to))
            return true;

        // String to any type (with parsing)
        if (isString(from))
            return true;

        // Any type to string
        if (isString(to))
            return true;

        // UUID <-> binary
        if (from == DataType::UUID && isBinary(to))
            return true;
        if (isBinary(from) && to == DataType::UUID)
            return true;

        // JSON <-> string
        if ((from == DataType::JSON && isString(to)) || (isString(from) && to == DataType::JSON))
            return true;

        return false;
    }

    auto TypeSystem::getCoercionPrecedence(DataType type) -> int
    {
        switch (type)
        {
        case DataType::INT8:
            return 1;
        case DataType::INT16:
            return 2;
        case DataType::INT32:
            return 3;
        case DataType::INT64:
            return 4;
        case DataType::FLOAT32:
            return 5;
        case DataType::FLOAT64:
            return 6;
        case DataType::DECIMAL:
            return 7;
        case DataType::VARCHAR:
        case DataType::TEXT:
            return 8;
        default:
            return 0;
        }
    }

    auto TypeSystem::getCommonType(DataType left, DataType right) -> std::optional<DataType>
    {
        if (left == right)
            return left;

        // Use precedence for numeric types
        if (isNumeric(left) && isNumeric(right))
        {
            int left_prec = getCoercionPrecedence(left);
            int right_prec = getCoercionPrecedence(right);
            return left_prec > right_prec ? left : right;
        }

        // Strings: prefer TEXT over VARCHAR over CHAR
        if (isString(left) && isString(right))
        {
            if (left == DataType::TEXT || right == DataType::TEXT)
                return DataType::TEXT;
            if (left == DataType::VARCHAR || right == DataType::VARCHAR)
                return DataType::VARCHAR;
            return DataType::CHAR;
        }

        return std::nullopt;
    }

    // ===== TypeConverter Implementation - String to Type =====

    auto TypeConverter::stringToInt8(const std::string &str, ErrorContext *ctx) -> std::optional<int8_t>
    {
        try
        {
            int val = std::stoi(str);
            if (val < -128 || val > 127)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Value out of range for INT8");
                return std::nullopt;
            }
            return static_cast<int8_t>(val);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid INT8 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToInt16(const std::string &str, ErrorContext *ctx) -> std::optional<int16_t>
    {
        try
        {
            int val = std::stoi(str);
            if (val < -32768 || val > 32767)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Value out of range for INT16");
                return std::nullopt;
            }
            return static_cast<int16_t>(val);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid INT16 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToInt32(const std::string &str, ErrorContext *ctx) -> std::optional<int32_t>
    {
        try
        {
            return std::stoi(str);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid INT32 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToInt64(const std::string &str, ErrorContext *ctx) -> std::optional<int64_t>
    {
        try
        {
            return std::stoll(str);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid INT64 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToFloat32(const std::string &str, ErrorContext *ctx) -> std::optional<float>
    {
        try
        {
            return std::stof(str);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid FLOAT32 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToFloat64(const std::string &str, ErrorContext *ctx) -> std::optional<double>
    {
        try
        {
            return std::stod(str);
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid FLOAT64 value");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToBoolean(const std::string &str, ErrorContext *ctx) -> std::optional<bool>
    {
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower == "true" || lower == "t" || lower == "yes" || lower == "y" || lower == "1")
            return true;
        if (lower == "false" || lower == "f" || lower == "no" || lower == "n" || lower == "0")
            return false;

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid BOOLEAN value");
        return std::nullopt;
    }

    // Type to string conversions
    auto TypeConverter::int8ToString(int8_t v) -> std::string
    {
        return std::to_string(static_cast<int>(v));
    }

    auto TypeConverter::int16ToString(int16_t v) -> std::string
    {
        return std::to_string(v);
    }

    auto TypeConverter::int32ToString(int32_t v) -> std::string
    {
        return std::to_string(v);
    }

    auto TypeConverter::int64ToString(int64_t v) -> std::string
    {
        return std::to_string(v);
    }

    auto TypeConverter::float32ToString(float v) -> std::string
    {
        std::ostringstream oss;
        oss << std::setprecision(7) << v;
        return oss.str();
    }

    auto TypeConverter::float64ToString(double v) -> std::string
    {
        std::ostringstream oss;
        oss << std::setprecision(15) << v;
        return oss.str();
    }

    auto TypeConverter::booleanToString(bool v) -> std::string
    {
        return v ? "true" : "false";
    }

    auto TypeConverter::dateToString(int64_t days) -> std::string
    {
        // Convert days since epoch to YYYY-MM-DD
        // Using Unix epoch (1970-01-01) as reference
        int64_t z = days + 719468; // Days from 0000-03-01
        int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        int64_t doe = z - era * 146097;
        int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int64_t y = yoe + era * 400;
        int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        int64_t mp = (5 * doy + 2) / 153;
        int64_t d = doy - (153 * mp + 2) / 5 + 1;
        int64_t m = mp + (mp < 10 ? 3 : -9);
        y += (m <= 2);

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(4) << y << '-'
            << std::setw(2) << m << '-' << std::setw(2) << d;
        return oss.str();
    }

    auto TypeConverter::timeToString(int64_t microseconds) -> std::string
    {
        int64_t seconds = microseconds / 1000000;
        int64_t us = microseconds % 1000000;
        int64_t hours = seconds / 3600;
        int64_t minutes = (seconds % 3600) / 60;
        int64_t secs = seconds % 60;

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << hours << ':'
            << std::setw(2) << minutes << ':' << std::setw(2) << secs;
        if (us > 0)
        {
            oss << '.' << std::setw(6) << us;
        }
        return oss.str();
    }

    auto TypeConverter::timestampToString(int64_t microseconds) -> std::string
    {
        // Input is always in GMT (microseconds since epoch)
        // Output in UTC with timezone offset for backward compatibility
        // Use TimezoneManager for proper formatting
        return g_tz_manager.formatTimestamp(microseconds, TimezoneManager::TZ_UTC, true);
    }

    auto TypeConverter::uuidToString(const std::vector<uint8_t> &uuid) -> std::string
    {
        if (uuid.size() != 16)
            return "<invalid-uuid>";

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < 16; i++)
        {
            if (i == 4 || i == 6 || i == 8 || i == 10)
                oss << '-';
            oss << std::setw(2) << static_cast<int>(uuid[i]);
        }
        return oss.str();
    }

    auto TypeConverter::binaryToHex(const std::vector<uint8_t> &data) -> std::string
    {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::setfill('0');
        for (uint8_t byte : data)
        {
            oss << std::setw(2) << static_cast<int>(byte);
        }
        return oss.str();
    }

    auto TypeConverter::binaryToBase64(const std::vector<uint8_t> &data) -> std::string
    {
        static const char base64_chars[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        std::string result;
        int val = 0;
        int valb = -6;
        for (uint8_t c : data)
        {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0)
            {
                result.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6)
            result.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        while (result.size() % 4)
            result.push_back('=');
        return result;
    }

    // Numeric conversions with overflow detection
    auto TypeConverter::int8ToInt16(int8_t v) -> int16_t
    {
        return static_cast<int16_t>(v);
    }

    auto TypeConverter::int8ToInt32(int8_t v) -> int32_t
    {
        return static_cast<int32_t>(v);
    }

    auto TypeConverter::int8ToInt64(int8_t v) -> int64_t
    {
        return static_cast<int64_t>(v);
    }

    auto TypeConverter::int16ToInt8(int16_t v, ErrorContext *ctx) -> std::optional<int8_t>
    {
        if (v < -128 || v > 127)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT16 to INT8");
            return std::nullopt;
        }
        return static_cast<int8_t>(v);
    }

    auto TypeConverter::int16ToInt32(int16_t v) -> int32_t
    {
        return static_cast<int32_t>(v);
    }

    auto TypeConverter::int16ToInt64(int16_t v) -> int64_t
    {
        return static_cast<int64_t>(v);
    }

    auto TypeConverter::int32ToInt8(int32_t v, ErrorContext *ctx) -> std::optional<int8_t>
    {
        if (v < -128 || v > 127)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT32 to INT8");
            return std::nullopt;
        }
        return static_cast<int8_t>(v);
    }

    auto TypeConverter::int32ToInt16(int32_t v, ErrorContext *ctx) -> std::optional<int16_t>
    {
        if (v < -32768 || v > 32767)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT32 to INT16");
            return std::nullopt;
        }
        return static_cast<int16_t>(v);
    }

    auto TypeConverter::int32ToInt64(int32_t v) -> int64_t
    {
        return static_cast<int64_t>(v);
    }

    auto TypeConverter::int64ToInt8(int64_t v, ErrorContext *ctx) -> std::optional<int8_t>
    {
        if (v < -128 || v > 127)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT64 to INT8");
            return std::nullopt;
        }
        return static_cast<int8_t>(v);
    }

    auto TypeConverter::int64ToInt16(int64_t v, ErrorContext *ctx) -> std::optional<int16_t>
    {
        if (v < -32768 || v > 32767)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT64 to INT16");
            return std::nullopt;
        }
        return static_cast<int16_t>(v);
    }

    auto TypeConverter::int64ToInt32(int64_t v, ErrorContext *ctx) -> std::optional<int32_t>
    {
        if (v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Overflow converting INT64 to INT32");
            return std::nullopt;
        }
        return static_cast<int32_t>(v);
    }

    // ===== TypeExtractor Implementation =====

    auto TypeExtractor::extractYear(int64_t days_since_epoch) -> int32_t
    {
        int64_t z = days_since_epoch + 719468;
        int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        int64_t yoe = (z - era * 146097 - (z - era * 146097) / 1460 + (z - era * 146097) / 36524 - (z - era * 146097) / 146096) / 365;
        int64_t y = yoe + era * 400;
        int64_t doy = z - era * 146097 - (365 * yoe + yoe / 4 - yoe / 100);
        int64_t mp = (5 * doy + 2) / 153;
        int64_t m = mp + (mp < 10 ? 3 : -9);
        return static_cast<int32_t>(y + (m <= 2));
    }

    auto TypeExtractor::extractMonth(int64_t days_since_epoch) -> int32_t
    {
        int64_t z = days_since_epoch + 719468;
        int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        int64_t doe = z - era * 146097;
        int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        int64_t mp = (5 * doy + 2) / 153;
        return static_cast<int32_t>(mp + (mp < 10 ? 3 : -9));
    }

    auto TypeExtractor::extractDay(int64_t days_since_epoch) -> int32_t
    {
        int64_t z = days_since_epoch + 719468;
        int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        int64_t doe = z - era * 146097;
        int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        int64_t mp = (5 * doy + 2) / 153;
        return static_cast<int32_t>(doy - (153 * mp + 2) / 5 + 1);
    }

    auto TypeExtractor::extractHour(int64_t microseconds) -> int32_t
    {
        return static_cast<int32_t>((microseconds / 1000000) / 3600);
    }

    auto TypeExtractor::extractMinute(int64_t microseconds) -> int32_t
    {
        return static_cast<int32_t>(((microseconds / 1000000) % 3600) / 60);
    }

    auto TypeExtractor::extractSecond(int64_t microseconds) -> int32_t
    {
        return static_cast<int32_t>((microseconds / 1000000) % 60);
    }

    auto TypeExtractor::extractMicrosecond(int64_t microseconds) -> int32_t
    {
        return static_cast<int32_t>(microseconds % 1000000);
    }

    auto TypeExtractor::extractUUIDVersion(const std::vector<uint8_t> &uuid) -> int32_t
    {
        if (uuid.size() != 16)
            return -1;
        return (uuid[6] >> 4) & 0x0F;
    }

    auto TypeExtractor::extractUUIDVariant(const std::vector<uint8_t> &uuid) -> int32_t
    {
        if (uuid.size() != 16)
            return -1;
        return (uuid[8] >> 6) & 0x03;
    }

    // ===== Convenience Conversion Methods =====

    int64_t TypedValue::toInt64() const
    {
        switch (type_)
        {
        case DataType::INT8:
            return getInt8();
        case DataType::INT16:
            return getInt16();
        case DataType::INT32:
            return getInt32();
        case DataType::INT64:
            return getInt64();
        case DataType::FLOAT32:
            return static_cast<int64_t>(getFloat32());
        case DataType::FLOAT64:
            return static_cast<int64_t>(getFloat64());
        case DataType::BOOLEAN:
            return getBoolean() ? 1 : 0;
        default:
            throw std::runtime_error("Cannot convert " + TypeSystem::getTypeName(type_) + " to int64");
        }
    }

    double TypedValue::toDouble() const
    {
        switch (type_)
        {
        case DataType::INT8:
            return static_cast<double>(getInt8());
        case DataType::INT16:
            return static_cast<double>(getInt16());
        case DataType::INT32:
            return static_cast<double>(getInt32());
        case DataType::INT64:
            return static_cast<double>(getInt64());
        case DataType::FLOAT32:
            return static_cast<double>(getFloat32());
        case DataType::FLOAT64:
            return getFloat64();
        case DataType::BOOLEAN:
            return getBoolean() ? 1.0 : 0.0;
        default:
            throw std::runtime_error("Cannot convert " + TypeSystem::getTypeName(type_) + " to double");
        }
    }

    bool TypedValue::toBoolean() const
    {
        switch (type_)
        {
        case DataType::BOOLEAN:
            return getBoolean();
        case DataType::INT8:
            return getInt8() != 0;
        case DataType::INT16:
            return getInt16() != 0;
        case DataType::INT32:
            return getInt32() != 0;
        case DataType::INT64:
            return getInt64() != 0;
        case DataType::FLOAT32:
            return getFloat32() != 0.0f;
        case DataType::FLOAT64:
            return getFloat64() != 0.0;
        case DataType::VARCHAR:
        case DataType::TEXT:
        case DataType::CHAR:
            return !getVarchar().empty();
        default:
            throw std::runtime_error("Cannot convert " + TypeSystem::getTypeName(type_) + " to boolean");
        }
    }

} // namespace scratchbird::core

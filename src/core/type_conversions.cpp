#include "scratchbird/core/types.h"
#include "scratchbird/core/timezone.h"
#include <algorithm>
#include <cctype>

namespace scratchbird::core
{

    // Thread-local timezone manager for parsing/formatting
    static thread_local TimezoneManager g_timezone_manager;

    /**
     * Type conversion implementation for TypedValue
     */

    auto TypedValue::convertTo(DataType target_type, ErrorContext *ctx) const -> std::optional<TypedValue>
    {
        // Same type, no conversion needed
        if (type_ == target_type)
            return *this;

        // NULL converts to any type as NULL
        if (isNull())
            return TypedValue::makeNull();

        // Target is NULL_TYPE - not a valid conversion
        if (target_type == DataType::NULL_TYPE)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Cannot convert to NULL type");
            return std::nullopt;
        }

        // Numeric to numeric conversions
        if (TypeSystem::isNumeric(type_) && TypeSystem::isNumeric(target_type))
        {
            return convertNumericTo(target_type, ctx);
        }

        // String to other type conversions
        if (TypeSystem::isString(type_))
        {
            return convertStringTo(target_type, ctx);
        }

        // Other type to string conversions
        if (TypeSystem::isString(target_type))
        {
            return TypedValue::makeVarchar(toString());
        }

        // UUID <-> Binary conversions
        if (type_ == DataType::UUID && TypeSystem::isBinary(target_type))
        {
            return TypedValue::makeBinary(getUUID());
        }
        if (TypeSystem::isBinary(type_) && target_type == DataType::UUID)
        {
            auto binary = getBinary();
            if (binary.size() != 16)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Binary data must be 16 bytes for UUID");
                return std::nullopt;
            }
            return TypedValue::makeUUID(binary);
        }

        // JSON <-> String conversions
        if (type_ == DataType::JSON && TypeSystem::isString(target_type))
        {
            return TypedValue::makeVarchar(getJSON());
        }
        if (TypeSystem::isString(type_) && target_type == DataType::JSON)
        {
            std::string json_str = toString();
            Status status = TypeConverter::validateJSON(json_str, ctx);
            if (status != Status::OK)
                return std::nullopt;
            return TypedValue::makeJSON(json_str);
        }

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported type conversion");
        return std::nullopt;
    }

    auto TypedValue::convertNumericTo(DataType target_type, ErrorContext *ctx) const -> std::optional<TypedValue>
    {
        // Get value as widest type (float64 or int64)
        bool is_float = type_ == DataType::FLOAT32 || type_ == DataType::FLOAT64;
        double float_val = 0.0;
        int64_t int_val = 0;

        if (is_float)
        {
            float_val = type_ == DataType::FLOAT32 ? getFloat32() : getFloat64();
        }
        else
        {
            switch (type_)
            {
            case DataType::INT8:
                int_val = getInt8();
                break;
            case DataType::INT16:
                int_val = getInt16();
                break;
            case DataType::INT32:
                int_val = getInt32();
                break;
            case DataType::INT64:
                int_val = getInt64();
                break;
            default:
                break;
            }
        }

        // Convert to target type
        switch (target_type)
        {
        case DataType::INT8:
        {
            int64_t val = is_float ? static_cast<int64_t>(float_val) : int_val;
            auto result = TypeConverter::int64ToInt8(val, ctx);
            return result ? std::make_optional(TypedValue::makeInt8(*result)) : std::nullopt;
        }

        case DataType::INT16:
        {
            int64_t val = is_float ? static_cast<int64_t>(float_val) : int_val;
            auto result = TypeConverter::int64ToInt16(val, ctx);
            return result ? std::make_optional(TypedValue::makeInt16(*result)) : std::nullopt;
        }

        case DataType::INT32:
        {
            int64_t val = is_float ? static_cast<int64_t>(float_val) : int_val;
            auto result = TypeConverter::int64ToInt32(val, ctx);
            return result ? std::make_optional(TypedValue::makeInt32(*result)) : std::nullopt;
        }

        case DataType::INT64:
        {
            int64_t val = is_float ? static_cast<int64_t>(float_val) : int_val;
            return TypedValue::makeInt64(val);
        }

        case DataType::FLOAT32:
        {
            float val = is_float ? static_cast<float>(float_val) : static_cast<float>(int_val);
            return TypedValue::makeFloat32(val);
        }

        case DataType::FLOAT64:
        {
            double val = is_float ? float_val : static_cast<double>(int_val);
            return TypedValue::makeFloat64(val);
        }

        case DataType::DECIMAL:
        {
            std::string val = is_float ? TypeConverter::float64ToString(float_val)
                                        : TypeConverter::int64ToString(int_val);
            return TypedValue::makeDecimal(val);
        }

        case DataType::BOOLEAN:
        {
            bool val = is_float ? (float_val != 0.0) : (int_val != 0);
            return TypedValue::makeBoolean(val);
        }

        default:
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported numeric conversion");
            return std::nullopt;
        }
    }

    auto TypedValue::convertStringTo(DataType target_type, ErrorContext *ctx) const -> std::optional<TypedValue>
    {
        std::string str = toString();

        switch (target_type)
        {
        case DataType::INT8:
        {
            auto result = TypeConverter::stringToInt8(str, ctx);
            return result ? std::make_optional(TypedValue::makeInt8(*result)) : std::nullopt;
        }

        case DataType::INT16:
        {
            auto result = TypeConverter::stringToInt16(str, ctx);
            return result ? std::make_optional(TypedValue::makeInt16(*result)) : std::nullopt;
        }

        case DataType::INT32:
        {
            auto result = TypeConverter::stringToInt32(str, ctx);
            return result ? std::make_optional(TypedValue::makeInt32(*result)) : std::nullopt;
        }

        case DataType::INT64:
        {
            auto result = TypeConverter::stringToInt64(str, ctx);
            return result ? std::make_optional(TypedValue::makeInt64(*result)) : std::nullopt;
        }

        case DataType::FLOAT32:
        {
            auto result = TypeConverter::stringToFloat32(str, ctx);
            return result ? std::make_optional(TypedValue::makeFloat32(*result)) : std::nullopt;
        }

        case DataType::FLOAT64:
        {
            auto result = TypeConverter::stringToFloat64(str, ctx);
            return result ? std::make_optional(TypedValue::makeFloat64(*result)) : std::nullopt;
        }

        case DataType::BOOLEAN:
        {
            auto result = TypeConverter::stringToBoolean(str, ctx);
            return result ? std::make_optional(TypedValue::makeBoolean(*result)) : std::nullopt;
        }

        case DataType::DATE:
        {
            auto result = TypeConverter::stringToDate(str, ctx);
            return result ? std::make_optional(TypedValue::makeDate(*result)) : std::nullopt;
        }

        case DataType::TIME:
        {
            auto result = TypeConverter::stringToTime(str, ctx);
            return result ? std::make_optional(TypedValue::makeTime(*result)) : std::nullopt;
        }

        case DataType::TIMESTAMP:
        {
            auto result = TypeConverter::stringToTimestamp(str, ctx);
            return result ? std::make_optional(TypedValue::makeTimestamp(*result)) : std::nullopt;
        }

        case DataType::UUID:
        {
            auto result = TypeConverter::stringToUUID(str, ctx);
            return result ? std::make_optional(TypedValue::makeUUID(*result)) : std::nullopt;
        }

        case DataType::BINARY:
        case DataType::VARBINARY:
        case DataType::BLOB:
        case DataType::BYTEA:
        {
            auto result = TypeConverter::stringToBinary(str, ctx);
            return result ? std::make_optional(TypedValue::makeBinary(*result)) : std::nullopt;
        }

        case DataType::CHAR:
            return TypedValue::makeChar(str);

        case DataType::VARCHAR:
            return TypedValue::makeVarchar(str);

        case DataType::TEXT:
            return TypedValue::makeText(str);

        case DataType::DECIMAL:
            return TypedValue::makeDecimal(str);

        case DataType::JSON:
        {
            Status status = TypeConverter::validateJSON(str, ctx);
            if (status != Status::OK)
                return std::nullopt;
            return TypedValue::makeJSON(str);
        }

        default:
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported string conversion");
            return std::nullopt;
        }
    }

    auto TypedValue::coerceTo(DataType target_type, ErrorContext *ctx) const -> std::optional<TypedValue>
    {
        // Coercion is like conversion but only allows implicit conversions
        if (type_ == target_type)
            return *this;

        if (!TypeSystem::isImplicitlyConvertible(type_, target_type))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Type cannot be implicitly coerced");
            return std::nullopt;
        }

        return convertTo(target_type, ctx);
    }

    // Comparison operators
    auto TypedValue::equals(const TypedValue &other) const -> std::optional<bool>
    {
        // NULL comparisons always return NULL (three-valued logic)
        if (isNull() || other.isNull())
            return std::nullopt;

        // Same type, direct comparison
        if (type_ == other.type_)
        {
            switch (type_)
            {
            case DataType::INT8:
                return getInt8() == other.getInt8();
            case DataType::INT16:
                return getInt16() == other.getInt16();
            case DataType::INT32:
                return getInt32() == other.getInt32();
            case DataType::INT64:
                return getInt64() == other.getInt64();
            case DataType::FLOAT32:
                return getFloat32() == other.getFloat32();
            case DataType::FLOAT64:
                return getFloat64() == other.getFloat64();
            case DataType::BOOLEAN:
                return getBoolean() == other.getBoolean();
            case DataType::DATE:
                return getDate() == other.getDate();
            case DataType::TIME:
                return getTime() == other.getTime();
            case DataType::TIMESTAMP:
                return getTimestamp() == other.getTimestamp();
            case DataType::UUID:
                return getUUID() == other.getUUID();
            case DataType::CHAR:
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::DECIMAL:
            case DataType::JSON:
                return toString() == other.toString();
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
                return getBinary() == other.getBinary();
            default:
                return std::nullopt;
            }
        }

        // Different types - try to coerce to common type
        auto common = TypeSystem::getCommonType(type_, other.type_);
        if (!common)
            return std::nullopt;

        auto left_coerced = convertTo(*common);
        auto right_coerced = other.convertTo(*common);

        if (!left_coerced || !right_coerced)
            return std::nullopt;

        return left_coerced->equals(*right_coerced);
    }

    auto TypedValue::lessThan(const TypedValue &other) const -> std::optional<bool>
    {
        if (isNull() || other.isNull())
            return std::nullopt;

        if (type_ == other.type_)
        {
            switch (type_)
            {
            case DataType::INT8:
                return getInt8() < other.getInt8();
            case DataType::INT16:
                return getInt16() < other.getInt16();
            case DataType::INT32:
                return getInt32() < other.getInt32();
            case DataType::INT64:
                return getInt64() < other.getInt64();
            case DataType::FLOAT32:
                return getFloat32() < other.getFloat32();
            case DataType::FLOAT64:
                return getFloat64() < other.getFloat64();
            case DataType::DATE:
                return getDate() < other.getDate();
            case DataType::TIME:
                return getTime() < other.getTime();
            case DataType::TIMESTAMP:
                return getTimestamp() < other.getTimestamp();
            case DataType::CHAR:
            case DataType::VARCHAR:
            case DataType::TEXT:
                return toString() < other.toString();
            default:
                return std::nullopt;
            }
        }

        auto common = TypeSystem::getCommonType(type_, other.type_);
        if (!common)
            return std::nullopt;

        auto left_coerced = convertTo(*common);
        auto right_coerced = other.convertTo(*common);

        if (!left_coerced || !right_coerced)
            return std::nullopt;

        return left_coerced->lessThan(*right_coerced);
    }

    auto TypedValue::greaterThan(const TypedValue &other) const -> std::optional<bool>
    {
        auto lt = lessThan(other);
        auto eq = equals(other);

        if (!lt || !eq)
            return std::nullopt;

        return !(*lt) && !(*eq);
    }

    // Hash function
    size_t TypedValue::hash() const
    {
        if (isNull())
            return 0;

        // Simple hash combining type and value
        size_t h = std::hash<uint16_t>{}(static_cast<uint16_t>(type_));

        switch (type_)
        {
        case DataType::INT8:
            h ^= std::hash<int8_t>{}(getInt8());
            break;
        case DataType::INT16:
            h ^= std::hash<int16_t>{}(getInt16());
            break;
        case DataType::INT32:
            h ^= std::hash<int32_t>{}(getInt32());
            break;
        case DataType::INT64:
            h ^= std::hash<int64_t>{}(getInt64());
            break;
        case DataType::FLOAT32:
            h ^= std::hash<float>{}(getFloat32());
            break;
        case DataType::FLOAT64:
            h ^= std::hash<double>{}(getFloat64());
            break;
        case DataType::BOOLEAN:
            h ^= std::hash<bool>{}(getBoolean());
            break;
        case DataType::DATE:
        case DataType::TIME:
        case DataType::TIMESTAMP:
            h ^= std::hash<int64_t>{}(std::get<int64_t>(data_));
            break;
        case DataType::CHAR:
        case DataType::VARCHAR:
        case DataType::TEXT:
        case DataType::DECIMAL:
        case DataType::JSON:
            h ^= std::hash<std::string>{}(toString());
            break;
        default:
            break;
        }

        return h;
    }

    // Additional string/date/time/UUID conversion functions
    auto TypeConverter::stringToDate(const std::string &str, ErrorContext *ctx) -> std::optional<int64_t>
    {
        // Parse ISO 8601 date format: YYYY-MM-DD
        if (str.size() < 10 || str[4] != '-' || str[7] != '-')
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid date format (expected YYYY-MM-DD)");
            return std::nullopt;
        }

        try
        {
            int year = std::stoi(str.substr(0, 4));
            int month = std::stoi(str.substr(5, 2));
            int day = std::stoi(str.substr(8, 2));

            // Validate month range
            if (month < 1 || month > 12)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid month (must be 1-12)");
                return std::nullopt;
            }

            // Validate day range (basic)
            if (day < 1 || day > 31)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid day (must be 1-31)");
                return std::nullopt;
            }

            // Validate day for specific month
            // Days per month: Jan=31, Feb=28/29, Mar=31, Apr=30, May=31, Jun=30, Jul=31, Aug=31, Sep=30, Oct=31, Nov=30, Dec=31
            const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            int max_day = days_in_month[month - 1];

            // Check for leap year if February
            if (month == 2)
            {
                // Leap year: divisible by 4 AND (NOT divisible by 100 OR divisible by 400)
                bool is_leap_year = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
                if (is_leap_year)
                {
                    max_day = 29;
                }
            }

            if (day > max_day)
            {
                std::string error_msg = "Invalid day for month (month " + std::to_string(month) +
                                      " has at most " + std::to_string(max_day) + " days)";
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
                return std::nullopt;
            }

            // Calculate days since epoch (1970-01-01)
            // Using civil date algorithm
            year -= (month <= 2);
            int64_t era = (year >= 0 ? year : year - 399) / 400;
            int64_t yoe = year - era * 400;
            int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
            int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
            return era * 146097 + doe - 719468;
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid date format");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToTime(const std::string &str, ErrorContext *ctx) -> std::optional<int64_t>
    {
        // Parse ISO 8601 time format: HH:MM:SS[.ffffff]
        if (str.size() < 8 || str[2] != ':' || str[5] != ':')
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid time format (expected HH:MM:SS)");
            return std::nullopt;
        }

        try
        {
            int hour = std::stoi(str.substr(0, 2));
            int minute = std::stoi(str.substr(3, 2));
            int second = std::stoi(str.substr(6, 2));
            int microsecond = 0;

            if (str.size() > 8 && str[8] == '.')
            {
                std::string frac = str.substr(9);
                frac.resize(6, '0'); // Pad to 6 digits
                microsecond = std::stoi(frac);
            }

            if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid time values");
                return std::nullopt;
            }

            return static_cast<int64_t>(hour) * 3600 * 1000000 +
                   static_cast<int64_t>(minute) * 60 * 1000000 +
                   static_cast<int64_t>(second) * 1000000 +
                   microsecond;
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid time format");
            return std::nullopt;
        }
    }

    auto TypeConverter::stringToTimestamp(const std::string &str, ErrorContext *ctx) -> std::optional<int64_t>
    {
        // Use TimezoneManager for timezone-aware parsing
        // Default to UTC for backward compatibility
        // Format: YYYY-MM-DD HH:MM:SS[.ffffff][+/-HH:MM or timezone name]
        // Result is always stored in GMT
        auto result = g_timezone_manager.parseTimestamp(str, TimezoneManager::TZ_UTC, ctx);
        return result;
    }

    auto TypeConverter::stringToUUID(const std::string &str, ErrorContext *ctx) -> std::optional<std::vector<uint8_t>>
    {
        // Parse UUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        // First validate format with hyphens (if present)
        if (str.size() == 36)
        {
            // Validate hyphen positions for standard UUID format
            if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-')
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UUID format (hyphens must be at positions 8, 13, 18, 23)");
                return std::nullopt;
            }
        }

        std::string cleaned = str;
        cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '-'), cleaned.end());

        if (cleaned.size() != 32)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UUID format (must be 32 hex digits)");
            return std::nullopt;
        }

        // Validate all characters are valid hex digits (0-9, a-f, A-F)
        for (size_t i = 0; i < cleaned.size(); ++i)
        {
            char c = cleaned[i];
            bool is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            if (!is_hex)
            {
                std::string error_msg = "Invalid UUID: character '" + std::string(1, c) +
                                      "' at position " + std::to_string(i) + " is not a valid hex digit (0-9, a-f, A-F)";
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
                return std::nullopt;
            }
        }

        // Parse the validated hex string
        std::vector<uint8_t> result(16);
        for (size_t i = 0; i < 16; i++)
        {
            std::string byte_str = cleaned.substr(i * 2, 2);
            try
            {
                result[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
            }
            catch (...)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid UUID hex digits");
                return std::nullopt;
            }
        }

        return result;
    }

    auto TypeConverter::stringToBinary(const std::string &str, ErrorContext *ctx) -> std::optional<std::vector<uint8_t>>
    {
        // Support hex format: 0x... or just hex digits
        std::string hex = str;
        if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
        {
            hex = hex.substr(2);
        }

        if (hex.size() % 2 != 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid binary hex format (odd number of digits)");
            return std::nullopt;
        }

        std::vector<uint8_t> result;
        for (size_t i = 0; i < hex.size(); i += 2)
        {
            try
            {
                uint8_t byte = static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16));
                result.push_back(byte);
            }
            catch (...)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid binary hex digits");
                return std::nullopt;
            }
        }

        return result;
    }

    auto TypeConverter::stringToDecimal(const std::string &str, uint32_t precision, uint32_t scale,
                                         ErrorContext *ctx) -> std::optional<std::string>
    {
        // Basic validation - just check it's a valid number format
        // Full DECIMAL validation would check precision/scale constraints
        try
        {
            std::stod(str); // Validate it's a number
            return str;     // Store as string for now
        }
        catch (...)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid decimal format");
            return std::nullopt;
        }
    }

    auto TypeConverter::validateJSON(const std::string &json, ErrorContext *ctx) -> Status
    {
        // Basic JSON validation - check balanced braces/brackets
        // Full validation would require a JSON parser
        int brace_count = 0;
        int bracket_count = 0;
        bool in_string = false;
        bool escaped = false;

        for (char c : json)
        {
            if (escaped)
            {
                escaped = false;
                continue;
            }

            if (c == '\\')
            {
                escaped = true;
                continue;
            }

            if (c == '"')
            {
                in_string = !in_string;
                continue;
            }

            if (in_string)
                continue;

            if (c == '{')
                brace_count++;
            else if (c == '}')
                brace_count--;
            else if (c == '[')
                bracket_count++;
            else if (c == ']')
                bracket_count--;
        }

        if (brace_count != 0 || bracket_count != 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid JSON: unbalanced braces/brackets");
            return Status::INVALID_ARGUMENT;
        }

        return Status::OK;
    }

    auto TypeSystem::validateValue(const TypedValue &value, const TypeInfo &type_info,
                                     ErrorContext *ctx) -> Status
    {
        if (value.type() != type_info.type)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Type mismatch");
            return Status::INVALID_ARGUMENT;
        }

        // Validate precision for CHAR/VARCHAR
        if (type_info.type == DataType::CHAR || type_info.type == DataType::VARCHAR)
        {
            if (type_info.precision > 0)
            {
                std::string str = value.toString();
                if (str.size() > type_info.precision)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "String exceeds maximum length");
                    return Status::INVALID_ARGUMENT;
                }
            }
        }

        // Validate UUID is 16 bytes
        if (type_info.type == DataType::UUID)
        {
            auto uuid = value.getUUID();
            if (uuid.size() != 16)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "UUID must be 16 bytes");
                return Status::INVALID_ARGUMENT;
            }
        }

        return Status::OK;
    }

} // namespace scratchbird::core

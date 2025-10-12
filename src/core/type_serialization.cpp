#include "scratchbird/core/types.h"
#include <cstring>
#include <algorithm>

namespace scratchbird::core
{

    /**
     * Type Serialization and Deserialization
     *
     * This file handles converting TypedValue objects to/from binary format
     * for storage on disk or transmission over network.
     *
     * Binary format for each type:
     * - INT8: 1 byte
     * - INT16: 2 bytes (little-endian)
     * - INT32: 4 bytes (little-endian)
     * - INT64: 8 bytes (little-endian)
     * - FLOAT32: 4 bytes (IEEE 754)
     * - FLOAT64: 8 bytes (IEEE 754)
     * - BOOLEAN: 1 byte (0 or 1)
     * - DATE: 8 bytes (int64 days since epoch)
     * - TIME: 8 bytes (int64 microseconds since midnight)
     * - TIMESTAMP: 8 bytes (int64 microseconds since epoch)
     * - UUID: 16 bytes (raw binary)
     * - VARCHAR/TEXT/CHAR: 4-byte length + data
     * - BINARY/VARBINARY/BLOB/BYTEA: 4-byte length + data
     * - DECIMAL: 4-byte length + string representation
     * - JSON: 4-byte length + JSON string
     */

    class TypeSerializer
    {
    public:
        /**
         * Serialize a TypedValue to binary format
         * Returns the serialized bytes
         */
        static auto serialize(const TypedValue &value) -> std::vector<uint8_t>
        {
            std::vector<uint8_t> result;

            if (value.isNull())
            {
                return result; // Empty for NULL
            }

            switch (value.type())
            {
                case DataType::INT8:
                {
                    int8_t v = value.getInt8();
                    result.push_back(static_cast<uint8_t>(v));
                    break;
                }

                case DataType::INT16:
                {
                    int16_t v = value.getInt16();
                    result.resize(2);
                    std::memcpy(result.data(), &v, 2);
                    break;
                }

                case DataType::INT32:
                {
                    int32_t v = value.getInt32();
                    result.resize(4);
                    std::memcpy(result.data(), &v, 4);
                    break;
                }

                case DataType::INT64:
                {
                    int64_t v = value.getInt64();
                    result.resize(8);
                    std::memcpy(result.data(), &v, 8);
                    break;
                }

                case DataType::FLOAT32:
                {
                    float v = value.getFloat32();
                    result.resize(4);
                    std::memcpy(result.data(), &v, 4);
                    break;
                }

                case DataType::FLOAT64:
                {
                    double v = value.getFloat64();
                    result.resize(8);
                    std::memcpy(result.data(), &v, 8);
                    break;
                }

                case DataType::BOOLEAN:
                {
                    bool v = value.getBoolean();
                    result.push_back(v ? 1 : 0);
                    break;
                }

                case DataType::DATE:
                {
                    int64_t v = value.getDate();
                    result.resize(8);
                    std::memcpy(result.data(), &v, 8);
                    break;
                }

                case DataType::TIME:
                {
                    int64_t v = value.getTime();
                    result.resize(8);
                    std::memcpy(result.data(), &v, 8);
                    break;
                }

                case DataType::TIMESTAMP:
                {
                    int64_t v = value.getTimestamp();

                    // Check if we have TypeInfo with timezone information
                    bool has_type_info = value.hasTypeInfo();
                    bool with_timezone = false;
                    uint16_t timezone_hint = 0;

                    if (has_type_info)
                    {
                        const auto &type_info = value.getTypeInfo();
                        if (type_info.has_value())
                        {
                            with_timezone = type_info->with_timezone;
                            timezone_hint = type_info->timezone_hint;
                        }
                    }

                    // Format: [1 byte: flags][2 bytes: timezone_hint if with_timezone][8 bytes:
                    // microseconds] Flags: bit 0 = has_timezone
                    uint8_t flags = with_timezone ? 1 : 0;
                    size_t total_size = 1 + (with_timezone ? 2 : 0) + 8;

                    result.resize(total_size);
                    size_t offset = 0;

                    // Write flags
                    result[offset++] = flags;

                    // Write timezone_hint if present
                    if (with_timezone)
                    {
                        std::memcpy(result.data() + offset, &timezone_hint, 2);
                        offset += 2;
                    }

                    // Write timestamp value
                    std::memcpy(result.data() + offset, &v, 8);
                    break;
                }

                case DataType::UUID:
                {
                    auto v = value.getUUID();
                    result = v; // UUIDs are already binary
                    break;
                }

                case DataType::CHAR:
                case DataType::VARCHAR:
                {
                    std::string v = value.toString();
                    uint32_t len = static_cast<uint32_t>(v.size());

                    // For CHAR/VARCHAR, serialize TypeInfo if present (to preserve max_length
                    // constraint)
                    bool has_type_info = value.hasTypeInfo();
                    uint32_t precision = 0;
                    if (has_type_info)
                    {
                        const auto &type_info = value.getTypeInfo();
                        if (type_info.has_value())
                        {
                            precision = type_info->precision;
                        }
                    }

                    // Format: [1 byte: has_precision][4 bytes: precision if has_precision][4 bytes:
                    // length][data]
                    uint8_t flags = has_type_info ? 1 : 0;
                    size_t total_size = 1 + (has_type_info ? 4 : 0) + 4 + len;
                    result.resize(total_size);

                    size_t offset = 0;
                    result[offset++] = flags;

                    if (has_type_info)
                    {
                        std::memcpy(result.data() + offset, &precision, 4);
                        offset += 4;
                    }

                    std::memcpy(result.data() + offset, &len, 4);
                    offset += 4;
                    std::memcpy(result.data() + offset, v.data(), len);
                    break;
                }

                case DataType::TEXT:
                {
                    // TEXT has no max_length constraint, use simple format
                    std::string v = value.toString();
                    uint32_t len = static_cast<uint32_t>(v.size());
                    result.resize(4 + len);
                    std::memcpy(result.data(), &len, 4);
                    std::memcpy(result.data() + 4, v.data(), len);
                    break;
                }

                case DataType::DECIMAL:
                {
                    std::string v = value.getDecimal();
                    uint32_t len = static_cast<uint32_t>(v.size());
                    result.resize(4 + len);
                    std::memcpy(result.data(), &len, 4);
                    std::memcpy(result.data() + 4, v.data(), len);
                    break;
                }

                case DataType::JSON:
                {
                    std::string v = value.getJSON();
                    uint32_t len = static_cast<uint32_t>(v.size());
                    result.resize(4 + len);
                    std::memcpy(result.data(), &len, 4);
                    std::memcpy(result.data() + 4, v.data(), len);
                    break;
                }

                case DataType::BINARY:
                case DataType::VARBINARY:
                case DataType::BLOB:
                case DataType::BYTEA:
                {
                    auto v = value.getBinary();
                    uint32_t len = static_cast<uint32_t>(v.size());
                    result.resize(4 + len);
                    std::memcpy(result.data(), &len, 4);
                    std::memcpy(result.data() + 4, v.data(), len);
                    break;
                }

                default:
                    // Unsupported type
                    break;
            }

            return result;
        }

        /**
         * Deserialize binary data to a TypedValue
         * Returns nullopt on error
         */
        static auto deserialize(DataType type, const uint8_t *data, uint32_t size,
                                ErrorContext *ctx = nullptr) -> std::optional<TypedValue>
        {
            if (data == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null data pointer");
                return std::nullopt;
            }

            switch (type)
            {
                case DataType::INT8:
                {
                    if (size < 1)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for INT8");
                        return std::nullopt;
                    }
                    int8_t v;
                    std::memcpy(&v, data, 1);
                    return TypedValue::makeInt8(v);
                }

                case DataType::INT16:
                {
                    if (size < 2)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for INT16");
                        return std::nullopt;
                    }
                    int16_t v;
                    std::memcpy(&v, data, 2);
                    return TypedValue::makeInt16(v);
                }

                case DataType::INT32:
                {
                    if (size < 4)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for INT32");
                        return std::nullopt;
                    }
                    int32_t v;
                    std::memcpy(&v, data, 4);
                    return TypedValue::makeInt32(v);
                }

                case DataType::INT64:
                {
                    if (size < 8)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for INT64");
                        return std::nullopt;
                    }
                    int64_t v;
                    std::memcpy(&v, data, 8);
                    return TypedValue::makeInt64(v);
                }

                case DataType::FLOAT32:
                {
                    if (size < 4)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for FLOAT32");
                        return std::nullopt;
                    }
                    float v;
                    std::memcpy(&v, data, 4);
                    return TypedValue::makeFloat32(v);
                }

                case DataType::FLOAT64:
                {
                    if (size < 8)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for FLOAT64");
                        return std::nullopt;
                    }
                    double v;
                    std::memcpy(&v, data, 8);
                    return TypedValue::makeFloat64(v);
                }

                case DataType::BOOLEAN:
                {
                    if (size < 1)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for BOOLEAN");
                        return std::nullopt;
                    }
                    bool v = data[0] != 0;
                    return TypedValue::makeBoolean(v);
                }

                case DataType::DATE:
                {
                    if (size < 8)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for DATE");
                        return std::nullopt;
                    }
                    int64_t v;
                    std::memcpy(&v, data, 8);
                    return TypedValue::makeDate(v);
                }

                case DataType::TIME:
                {
                    if (size < 8)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for TIME");
                        return std::nullopt;
                    }
                    int64_t v;
                    std::memcpy(&v, data, 8);
                    return TypedValue::makeTime(v);
                }

                case DataType::TIMESTAMP:
                {
                    // Format: [1 byte: flags][2 bytes: timezone_hint if with_timezone][8 bytes:
                    // microseconds]
                    if (size < 1)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for TIMESTAMP flags");
                        return std::nullopt;
                    }

                    size_t offset = 0;
                    uint8_t flags = data[offset++];
                    bool with_timezone = (flags & 1) != 0;

                    uint16_t timezone_hint = 0;
                    if (with_timezone)
                    {
                        if (size < 1 + 2 + 8)
                        {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for TIMESTAMP WITH TIME ZONE");
                            return std::nullopt;
                        }
                        std::memcpy(&timezone_hint, data + offset, 2);
                        offset += 2;
                    }
                    else
                    {
                        if (size < 1 + 8)
                        {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for TIMESTAMP");
                            return std::nullopt;
                        }
                    }

                    // Read timestamp value
                    int64_t v;
                    std::memcpy(&v, data + offset, 8);

                    // Create TypedValue with timestamp
                    TypedValue result = TypedValue::makeTimestamp(v);

                    // Restore TypeInfo if timezone present
                    if (with_timezone)
                    {
                        TypeInfo type_info;
                        type_info.type = DataType::TIMESTAMP;
                        type_info.with_timezone = true;
                        type_info.timezone_hint = timezone_hint;
                        result.setTypeInfo(type_info);
                    }

                    return result;
                }

                case DataType::UUID:
                {
                    if (size < 16)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for UUID");
                        return std::nullopt;
                    }
                    return TypedValue::makeUUID(data, 16);
                }

                case DataType::CHAR:
                case DataType::VARCHAR:
                {
                    // Format: [1 byte: has_precision][4 bytes: precision if has_precision][4 bytes:
                    // length][data]
                    if (size < 1)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for CHAR/VARCHAR flags");
                        return std::nullopt;
                    }

                    size_t offset = 0;
                    uint8_t flags = data[offset++];
                    bool has_precision = (flags & 1) != 0;

                    uint32_t precision = 0;
                    if (has_precision)
                    {
                        if (size < 1 + 4)
                        {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for CHAR/VARCHAR precision");
                            return std::nullopt;
                        }
                        std::memcpy(&precision, data + offset, 4);
                        offset += 4;
                    }

                    if (size < offset + 4)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for CHAR/VARCHAR length");
                        return std::nullopt;
                    }

                    uint32_t len;
                    std::memcpy(&len, data + offset, 4);
                    offset += 4;

                    if (size < offset + len)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for CHAR/VARCHAR value");
                        return std::nullopt;
                    }

                    std::string v(reinterpret_cast<const char *>(data + offset), len);
                    TypedValue result = (type == DataType::CHAR) ? TypedValue::makeChar(v)
                                                                 : TypedValue::makeVarchar(v);

                    // Restore TypeInfo if precision was serialized
                    if (has_precision && precision > 0)
                    {
                        TypeInfo info(type);
                        info.precision = precision;
                        result.setTypeInfo(info);
                    }

                    return result;
                }

                case DataType::TEXT:
                {
                    if (size < 4)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for TEXT length");
                        return std::nullopt;
                    }
                    uint32_t len;
                    std::memcpy(&len, data, 4);
                    if (size < 4 + len)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for TEXT value");
                        return std::nullopt;
                    }
                    std::string v(reinterpret_cast<const char *>(data + 4), len);
                    return TypedValue::makeText(v);
                }

                case DataType::DECIMAL:
                {
                    if (size < 4)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for DECIMAL length");
                        return std::nullopt;
                    }
                    uint32_t len;
                    std::memcpy(&len, data, 4);
                    if (size < 4 + len)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for DECIMAL value");
                        return std::nullopt;
                    }
                    std::string v(reinterpret_cast<const char *>(data + 4), len);
                    return TypedValue::makeDecimal(v);
                }

                case DataType::JSON:
                {
                    if (size < 4)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for JSON length");
                        return std::nullopt;
                    }
                    uint32_t len;
                    std::memcpy(&len, data, 4);
                    if (size < 4 + len)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for JSON value");
                        return std::nullopt;
                    }
                    std::string v(reinterpret_cast<const char *>(data + 4), len);
                    return TypedValue::makeJSON(v);
                }

                case DataType::BINARY:
                case DataType::VARBINARY:
                case DataType::BLOB:
                case DataType::BYTEA:
                {
                    if (size < 4)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for binary length");
                        return std::nullopt;
                    }
                    uint32_t len;
                    std::memcpy(&len, data, 4);
                    if (size < 4 + len)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for binary value");
                        return std::nullopt;
                    }
                    return TypedValue::makeBinary(data + 4, len);
                }

                default:
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Unsupported type for deserialization");
                    return std::nullopt;
            }
        }

        /**
         * Calculate the serialized size of a value
         */
        static auto getSerializedSize(const TypedValue &value) -> uint32_t
        {
            if (value.isNull())
                return 0;

            switch (value.type())
            {
                case DataType::INT8:
                case DataType::BOOLEAN:
                    return 1;

                case DataType::INT16:
                    return 2;

                case DataType::INT32:
                case DataType::FLOAT32:
                    return 4;

                case DataType::INT64:
                case DataType::FLOAT64:
                case DataType::DATE:
                case DataType::TIME:
                    return 8;

                case DataType::TIMESTAMP:
                {
                    // Format: [1 byte: flags][2 bytes: timezone_hint if with_timezone][8 bytes:
                    // microseconds]
                    uint32_t size = 1 + 8; // flags + timestamp value
                    if (value.hasTypeInfo())
                    {
                        const auto &type_info = value.getTypeInfo();
                        if (type_info.has_value() && type_info->with_timezone)
                        {
                            size += 2; // timezone_hint
                        }
                    }
                    return size;
                }

                case DataType::UUID:
                    return 16;

                case DataType::CHAR:
                case DataType::VARCHAR:
                {
                    std::string v = value.toString();
                    // Format: [1 byte: flags][4 bytes: precision if has_type_info][4 bytes:
                    // length][data]
                    uint32_t size = 1 + 4 + v.size(); // flags + length + data
                    if (value.hasTypeInfo())
                    {
                        size += 4; // precision
                    }
                    return size;
                }

                case DataType::TEXT:
                {
                    std::string v = value.toString();
                    return 4 + static_cast<uint32_t>(v.size());
                }

                case DataType::DECIMAL:
                {
                    std::string v = value.getDecimal();
                    return 4 + static_cast<uint32_t>(v.size());
                }

                case DataType::JSON:
                {
                    std::string v = value.getJSON();
                    return 4 + static_cast<uint32_t>(v.size());
                }

                case DataType::BINARY:
                case DataType::VARBINARY:
                case DataType::BLOB:
                case DataType::BYTEA:
                {
                    auto v = value.getBinary();
                    return 4 + static_cast<uint32_t>(v.size());
                }

                default:
                    return 0;
            }
        }
    };

    // Make serialization functions available through TypeSystem
    auto TypeSystem::getSerializedSize(const TypedValue &value) -> uint32_t
    {
        return TypeSerializer::getSerializedSize(value);
    }

} // namespace scratchbird::core

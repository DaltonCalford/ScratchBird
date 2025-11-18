#include "scratchbird/core/type_serialization.h"
#include "scratchbird/core/types.h"
#include "scratchbird/spatial/wkb.h"
#include "scratchbird/core/vector.h"
#include "scratchbird/core/range.h"
#include "scratchbird/core/network.h"
#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
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

    auto TypeSerializer::serialize(const TypedValue &value) -> std::vector<uint8_t>
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

                // Unsigned integer types
                case DataType::UINT8:
                {
                    uint8_t v = value.getUInt8();
                    result.push_back(v);
                    break;
                }

                case DataType::UINT16:
                {
                    uint16_t v = value.getUInt16();
                    result.resize(2);
                    std::memcpy(result.data(), &v, 2);
                    break;
                }

                case DataType::UINT32:
                {
                    uint32_t v = value.getUInt32();
                    result.resize(4);
                    std::memcpy(result.data(), &v, 4);
                    break;
                }

                case DataType::UINT64:
                {
                    uint64_t v = value.getUInt64();
                    result.resize(8);
                    std::memcpy(result.data(), &v, 8);
                    break;
                }

                // INT128 type
                case DataType::INT128:
                {
                    int128_t v = value.getInt128();
                    result.resize(16);
                    std::memcpy(result.data(), &v, 16);
                    break;
                }

                // MONEY type (stored as int64 cents)
                case DataType::MONEY:
                {
                    int64_t v = value.getMoney();
                    result.resize(8);
                    std::memcpy(result.data(), &v, 8);
                    break;
                }

                // INTERVAL type (months, days, microseconds)
                case DataType::INTERVAL:
                {
                    Interval v = value.getInterval();
                    result.resize(16); // 4 + 4 + 8 bytes
                    size_t offset = 0;
                    std::memcpy(result.data() + offset, &v.months, 4);
                    offset += 4;
                    std::memcpy(result.data() + offset, &v.days, 4);
                    offset += 4;
                    std::memcpy(result.data() + offset, &v.microseconds, 8);
                    break;
                }

                // Spatial types using WKB format
                case DataType::POINT:
                {
                    Point pt = value.getPoint();
                    result = spatial::WKBSerializer::serializePoint(pt);
                    break;
                }

                case DataType::LINESTRING:
                {
                    LineString line = value.getLineString();
                    result = spatial::WKBSerializer::serializeLineString(line);
                    break;
                }

                case DataType::POLYGON:
                {
                    Polygon poly = value.getPolygon();
                    result = spatial::WKBSerializer::serializePolygon(poly);
                    break;
                }

                // Vector type (embeddings/ML)
                case DataType::VECTOR:
                {
                    auto vec = value.getVector();
                    if (vec) {
                        result = Vector::encode(*vec);
                    }
                    break;
                }

                // Range types
                case DataType::INT4RANGE:
                {
                    Int4Range range = value.getInt4Range();
                    // Flags byte: bit0=empty, bit1=lower_bounded, bit2=upper_bounded, bit3=lower_inc, bit4=upper_inc
                    uint8_t flags = 0;
                    if (range.isEmpty()) flags |= 0x01;
                    if (range.isLowerBounded()) flags |= 0x02;
                    if (range.isUpperBounded()) flags |= 0x04;
                    if (range.isLowerInclusive()) flags |= 0x08;
                    if (range.isUpperInclusive()) flags |= 0x10;
                    result.push_back(flags);

                    if (range.isLowerBounded()) {
                        int32_t lower = *range.lower();
                        result.resize(result.size() + 4);
                        std::memcpy(result.data() + result.size() - 4, &lower, 4);
                    }
                    if (range.isUpperBounded()) {
                        int32_t upper = *range.upper();
                        result.resize(result.size() + 4);
                        std::memcpy(result.data() + result.size() - 4, &upper, 4);
                    }
                    break;
                }

                case DataType::INT8RANGE:
                {
                    Int8Range range = value.getInt8Range();
                    uint8_t flags = 0;
                    if (range.isEmpty()) flags |= 0x01;
                    if (range.isLowerBounded()) flags |= 0x02;
                    if (range.isUpperBounded()) flags |= 0x04;
                    if (range.isLowerInclusive()) flags |= 0x08;
                    if (range.isUpperInclusive()) flags |= 0x10;
                    result.push_back(flags);

                    if (range.isLowerBounded()) {
                        int64_t lower = *range.lower();
                        result.resize(result.size() + 8);
                        std::memcpy(result.data() + result.size() - 8, &lower, 8);
                    }
                    if (range.isUpperBounded()) {
                        int64_t upper = *range.upper();
                        result.resize(result.size() + 8);
                        std::memcpy(result.data() + result.size() - 8, &upper, 8);
                    }
                    break;
                }

                case DataType::NUMRANGE:
                {
                    NumRange range = value.getNumRange();
                    uint8_t flags = 0;
                    if (range.isEmpty()) flags |= 0x01;
                    if (range.isLowerBounded()) flags |= 0x02;
                    if (range.isUpperBounded()) flags |= 0x04;
                    if (range.isLowerInclusive()) flags |= 0x08;
                    if (range.isUpperInclusive()) flags |= 0x10;
                    result.push_back(flags);

                    if (range.isLowerBounded()) {
                        double lower = *range.lower();
                        result.resize(result.size() + 8);
                        std::memcpy(result.data() + result.size() - 8, &lower, 8);
                    }
                    if (range.isUpperBounded()) {
                        double upper = *range.upper();
                        result.resize(result.size() + 8);
                        std::memcpy(result.data() + result.size() - 8, &upper, 8);
                    }
                    break;
                }

                case DataType::DATERANGE:
                {
                    DateRange range = value.getDateRange();
                    uint8_t flags = 0;
                    if (range.isEmpty()) flags |= 0x01;
                    if (range.isLowerBounded()) flags |= 0x02;
                    if (range.isUpperBounded()) flags |= 0x04;
                    if (range.isLowerInclusive()) flags |= 0x08;
                    if (range.isUpperInclusive()) flags |= 0x10;
                    result.push_back(flags);

                    if (range.isLowerBounded()) {
                        int64_t lower = *range.lower();
                        result.resize(result.size() + 8);
                        std::memcpy(result.data() + result.size() - 8, &lower, 8);
                    }
                    if (range.isUpperBounded()) {
                        int64_t upper = *range.upper();
                        result.resize(result.size() + 8);
                        std::memcpy(result.data() + result.size() - 8, &upper, 8);
                    }
                    break;
                }

                case DataType::TSRANGE:
                {
                    TSRange range = value.getTSRange();
                    uint8_t flags = 0;
                    if (range.isEmpty()) flags |= 0x01;
                    if (range.isLowerBounded()) flags |= 0x02;
                    if (range.isUpperBounded()) flags |= 0x04;
                    if (range.isLowerInclusive()) flags |= 0x08;
                    if (range.isUpperInclusive()) flags |= 0x10;
                    result.push_back(flags);

                    if (range.isLowerBounded()) {
                        int64_t lower = *range.lower();
                        result.resize(result.size() + 8);
                        std::memcpy(result.data() + result.size() - 8, &lower, 8);
                    }
                    if (range.isUpperBounded()) {
                        int64_t upper = *range.upper();
                        result.resize(result.size() + 8);
                        std::memcpy(result.data() + result.size() - 8, &upper, 8);
                    }
                    break;
                }

                case DataType::TSTZRANGE:
                {
                    TSTZRange range = value.getTSTZRange();
                    uint8_t flags = 0;
                    if (range.isEmpty()) flags |= 0x01;
                    if (range.isLowerBounded()) flags |= 0x02;
                    if (range.isUpperBounded()) flags |= 0x04;
                    if (range.isLowerInclusive()) flags |= 0x08;
                    if (range.isUpperInclusive()) flags |= 0x10;
                    result.push_back(flags);

                    if (range.isLowerBounded()) {
                        int64_t lower = *range.lower();
                        result.resize(result.size() + 8);
                        std::memcpy(result.data() + result.size() - 8, &lower, 8);
                    }
                    if (range.isUpperBounded()) {
                        int64_t upper = *range.upper();
                        result.resize(result.size() + 8);
                        std::memcpy(result.data() + result.size() - 8, &upper, 8);
                    }
                    break;
                }

                // Network types
                case DataType::INET:
                case DataType::CIDR:
                {
                    InetAddr addr = (value.type() == DataType::INET) ? value.getInet() : value.getCidr().toInet();
                    // Format: 1 byte family + 1 byte netmask + address bytes (4 or 16)
                    result.push_back(static_cast<uint8_t>(addr.family()));
                    result.push_back(addr.netmask());
                    size_t addr_size = addr.size();
                    result.resize(result.size() + addr_size);
                    std::memcpy(result.data() + result.size() - addr_size, addr.data(), addr_size);
                    break;
                }

                case DataType::MACADDR:
                {
                    MacAddr mac = value.getMacAddr();
                    const auto& bytes = mac.bytes();
                    result.resize(6);
                    std::memcpy(result.data(), bytes.data(), 6);
                    break;
                }

                case DataType::MACADDR8:
                {
                    MacAddr8 mac = value.getMacAddr8();
                    const auto& bytes = mac.bytes();
                    result.resize(8);
                    std::memcpy(result.data(), bytes.data(), 8);
                    break;
                }

                // Text search types
                case DataType::TSVECTOR:
                {
                    auto tsv = value.getTSVector();
                    if (tsv) {
                        result = tsv->toBinary();
                    }
                    break;
                }

                case DataType::TSQUERY:
                {
                    auto tsq = value.getTSQuery();
                    if (tsq) {
                        result = tsq->toBinary();
                    }
                    break;
                }

                // JSONB and XML (stored as string with length prefix)
                case DataType::JSONB:
                {
                    std::string str = value.getJSON();  // JSONB uses JSON string for now
                    uint32_t len = static_cast<uint32_t>(str.size());
                    result.resize(4 + len);
                    std::memcpy(result.data(), &len, 4);
                    std::memcpy(result.data() + 4, str.data(), len);
                    break;
                }

                case DataType::XML:
                {
                    std::string str = value.toString();  // XML as string
                    uint32_t len = static_cast<uint32_t>(str.size());
                    result.resize(4 + len);
                    std::memcpy(result.data(), &len, 4);
                    std::memcpy(result.data() + 4, str.data(), len);
                    break;
                }

                // Composite type (field names + values)
                case DataType::COMPOSITE:
                {
                    const auto& comp = value.getComposite();
                    // Format: 4 bytes num_fields + for each field: 4 bytes name_len + name + type + serialized_value
                    uint32_t num_fields = static_cast<uint32_t>(comp.field_names.size());
                    result.resize(4);
                    std::memcpy(result.data(), &num_fields, 4);

                    for (size_t i = 0; i < num_fields; ++i) {
                        // Field name
                        uint32_t name_len = static_cast<uint32_t>(comp.field_names[i].size());
                        size_t offset = result.size();
                        result.resize(offset + 4 + name_len);
                        std::memcpy(result.data() + offset, &name_len, 4);
                        std::memcpy(result.data() + offset + 4, comp.field_names[i].data(), name_len);

                        // Field type
                        offset = result.size();
                        result.resize(offset + 1);
                        result[offset] = static_cast<uint8_t>(comp.field_values[i]->type());

                        // Field value (recursive)
                        auto field_data = serialize(*comp.field_values[i]);
                        uint32_t field_len = static_cast<uint32_t>(field_data.size());
                        offset = result.size();
                        result.resize(offset + 4 + field_len);
                        std::memcpy(result.data() + offset, &field_len, 4);
                        std::memcpy(result.data() + offset + 4, field_data.data(), field_len);
                    }
                    break;
                }

                // Variant type (type tag + value)
                case DataType::VARIANT:
                {
                    const auto& var = value.getVariant();
                    // Format: 1 byte actual_type + 4 bytes value_len + serialized_value
                    result.resize(1);
                    result[0] = static_cast<uint8_t>(var.actual_type);

                    if (var.value) {
                        auto val_data = serialize(*var.value);
                        uint32_t val_len = static_cast<uint32_t>(val_data.size());
                        size_t offset = result.size();
                        result.resize(offset + 4 + val_len);
                        std::memcpy(result.data() + offset, &val_len, 4);
                        std::memcpy(result.data() + offset + 4, val_data.data(), val_len);
                    } else {
                        // Null value
                        size_t offset = result.size();
                        result.resize(offset + 4);
                        uint32_t zero = 0;
                        std::memcpy(result.data() + offset, &zero, 4);
                    }
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
        auto TypeSerializer::deserialize(DataType type, const uint8_t *data, uint32_t size,
                                ErrorContext *ctx) -> std::optional<TypedValue>
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

                // Unsigned integer types
                case DataType::UINT8:
                {
                    if (size < 1)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for UINT8");
                        return std::nullopt;
                    }
                    uint8_t v = data[0];
                    return TypedValue::makeUInt8(v);
                }

                case DataType::UINT16:
                {
                    if (size < 2)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for UINT16");
                        return std::nullopt;
                    }
                    uint16_t v;
                    std::memcpy(&v, data, 2);
                    return TypedValue::makeUInt16(v);
                }

                case DataType::UINT32:
                {
                    if (size < 4)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for UINT32");
                        return std::nullopt;
                    }
                    uint32_t v;
                    std::memcpy(&v, data, 4);
                    return TypedValue::makeUInt32(v);
                }

                case DataType::UINT64:
                {
                    if (size < 8)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for UINT64");
                        return std::nullopt;
                    }
                    uint64_t v;
                    std::memcpy(&v, data, 8);
                    return TypedValue::makeUInt64(v);
                }

                // INT128 type
                case DataType::INT128:
                {
                    if (size < 16)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for INT128");
                        return std::nullopt;
                    }
                    int128_t v;
                    std::memcpy(&v, data, 16);
                    return TypedValue::makeInt128(v);
                }

                // MONEY type
                case DataType::MONEY:
                {
                    if (size < 8)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for MONEY");
                        return std::nullopt;
                    }
                    int64_t v;
                    std::memcpy(&v, data, 8);
                    return TypedValue::makeMoney(v);
                }

                // INTERVAL type
                case DataType::INTERVAL:
                {
                    if (size < 16)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for INTERVAL");
                        return std::nullopt;
                    }
                    int32_t months, days;
                    int64_t microseconds;
                    size_t offset = 0;
                    std::memcpy(&months, data + offset, 4);
                    offset += 4;
                    std::memcpy(&days, data + offset, 4);
                    offset += 4;
                    std::memcpy(&microseconds, data + offset, 8);
                    return TypedValue::makeInterval(months, days, microseconds);
                }

                // Spatial types using WKB format
                case DataType::POINT:
                {
                    std::vector<uint8_t> wkb(data, data + size);
                    auto pt = spatial::WKBSerializer::deserializePoint(wkb, ctx);
                    if (!pt) return std::nullopt;
                    return TypedValue::makePoint(*pt);
                }

                case DataType::LINESTRING:
                {
                    std::vector<uint8_t> wkb(data, data + size);
                    auto line = spatial::WKBSerializer::deserializeLineString(wkb, ctx);
                    if (!line) return std::nullopt;
                    return TypedValue::makeLineString(*line);
                }

                case DataType::POLYGON:
                {
                    std::vector<uint8_t> wkb(data, data + size);
                    auto poly = spatial::WKBSerializer::deserializePolygon(wkb, ctx);
                    if (!poly) return std::nullopt;
                    return TypedValue::makePolygon(*poly);
                }

                // Vector type (embeddings/ML)
                case DataType::VECTOR:
                {
                    std::vector<uint8_t> buffer(data, data + size);
                    auto vec = Vector::decode(buffer);
                    if (!vec) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Failed to decode VECTOR");
                        return std::nullopt;
                    }
                    return TypedValue::makeVector(*vec);
                }

                // Range types
                case DataType::INT4RANGE:
                {
                    if (size < 1) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for INT4RANGE");
                        return std::nullopt;
                    }
                    uint8_t flags = data[0];
                    size_t offset = 1;

                    if (flags & 0x01) { // empty
                        return TypedValue::makeInt4Range(Int4Range());
                    }

                    std::optional<int32_t> lower, upper;
                    if (flags & 0x02) { // lower_bounded
                        if (offset + 4 > size) {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for INT4RANGE lower bound");
                            return std::nullopt;
                        }
                        int32_t val;
                        std::memcpy(&val, data + offset, 4);
                        lower = val;
                        offset += 4;
                    }
                    if (flags & 0x04) { // upper_bounded
                        if (offset + 4 > size) {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for INT4RANGE upper bound");
                            return std::nullopt;
                        }
                        int32_t val;
                        std::memcpy(&val, data + offset, 4);
                        upper = val;
                        offset += 4;
                    }

                    BoundType lower_type = (flags & 0x08) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                    BoundType upper_type = (flags & 0x10) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                    return TypedValue::makeInt4Range(Int4Range(lower, upper, lower_type, upper_type));
                }

                case DataType::INT8RANGE:
                case DataType::DATERANGE:
                case DataType::TSRANGE:
                case DataType::TSTZRANGE:
                {
                    // All these are Range<int64_t> with 8-byte bounds
                    if (size < 1) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for range");
                        return std::nullopt;
                    }
                    uint8_t flags = data[0];
                    size_t offset = 1;

                    std::optional<int64_t> lower, upper;
                    if (flags & 0x02) { // lower_bounded
                        if (offset + 8 > size) {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for range lower bound");
                            return std::nullopt;
                        }
                        int64_t val;
                        std::memcpy(&val, data + offset, 8);
                        lower = val;
                        offset += 8;
                    }
                    if (flags & 0x04) { // upper_bounded
                        if (offset + 8 > size) {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for range upper bound");
                            return std::nullopt;
                        }
                        int64_t val;
                        std::memcpy(&val, data + offset, 8);
                        upper = val;
                        offset += 8;
                    }

                    BoundType lower_type = (flags & 0x08) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                    BoundType upper_type = (flags & 0x10) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;

                    if (type == DataType::INT8RANGE) {
                        if (flags & 0x01) return TypedValue::makeInt8Range(Int8Range());
                        return TypedValue::makeInt8Range(Int8Range(lower, upper, lower_type, upper_type));
                    } else if (type == DataType::DATERANGE) {
                        if (flags & 0x01) return TypedValue::makeDateRange(DateRange());
                        return TypedValue::makeDateRange(DateRange(lower, upper, lower_type, upper_type));
                    } else if (type == DataType::TSRANGE) {
                        if (flags & 0x01) return TypedValue::makeTSRange(TSRange());
                        return TypedValue::makeTSRange(TSRange(lower, upper, lower_type, upper_type));
                    } else { // TSTZRANGE
                        if (flags & 0x01) return TypedValue::makeTSTZRange(TSTZRange());
                        return TypedValue::makeTSTZRange(TSTZRange(lower, upper, lower_type, upper_type));
                    }
                }

                case DataType::NUMRANGE:
                {
                    if (size < 1) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for NUMRANGE");
                        return std::nullopt;
                    }
                    uint8_t flags = data[0];
                    size_t offset = 1;

                    if (flags & 0x01) { // empty
                        return TypedValue::makeNumRange(NumRange());
                    }

                    std::optional<double> lower, upper;
                    if (flags & 0x02) { // lower_bounded
                        if (offset + 8 > size) {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for NUMRANGE lower bound");
                            return std::nullopt;
                        }
                        double val;
                        std::memcpy(&val, data + offset, 8);
                        lower = val;
                        offset += 8;
                    }
                    if (flags & 0x04) { // upper_bounded
                        if (offset + 8 > size) {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for NUMRANGE upper bound");
                            return std::nullopt;
                        }
                        double val;
                        std::memcpy(&val, data + offset, 8);
                        upper = val;
                        offset += 8;
                    }

                    BoundType lower_type = (flags & 0x08) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                    BoundType upper_type = (flags & 0x10) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                    return TypedValue::makeNumRange(NumRange(lower, upper, lower_type, upper_type));
                }

                // Network types
                case DataType::INET:
                case DataType::CIDR:
                {
                    // Format: 1 byte family + 1 byte netmask + address bytes (4 or 16)
                    if (size < 2) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for INET/CIDR");
                        return std::nullopt;
                    }

                    AddressFamily family = static_cast<AddressFamily>(data[0]);
                    uint8_t netmask = data[1];
                    size_t addr_size = (family == AddressFamily::IPv4) ? 4 : 16;

                    if (size < 2 + addr_size) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for INET/CIDR address");
                        return std::nullopt;
                    }

                    InetAddr addr(family, data + 2, netmask);
                    if (type == DataType::INET) {
                        return TypedValue::makeInet(addr);
                    } else {
                        return TypedValue::makeCidr(Cidr::fromInet(addr));
                    }
                }

                case DataType::MACADDR:
                {
                    if (size < 6) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for MACADDR");
                        return std::nullopt;
                    }
                    MacAddr mac = MacAddr::fromBytes(data);
                    return TypedValue::makeMacAddr(mac);
                }

                case DataType::MACADDR8:
                {
                    if (size < 8) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for MACADDR8");
                        return std::nullopt;
                    }
                    MacAddr8 mac = MacAddr8::fromBytes(data);
                    return TypedValue::makeMacAddr8(mac);
                }

                // Text search types
                case DataType::TSVECTOR:
                {
                    std::vector<uint8_t> buffer(data, data + size);
                    auto tsv = TSVector::fromBinary(buffer);
                    if (!tsv) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Failed to decode TSVECTOR");
                        return std::nullopt;
                    }
                    return TypedValue::makeTSVector(*tsv);
                }

                case DataType::TSQUERY:
                {
                    std::vector<uint8_t> buffer(data, data + size);
                    auto tsq = TSQuery::fromBinary(buffer);
                    if (!tsq) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Failed to decode TSQUERY");
                        return std::nullopt;
                    }
                    return TypedValue::makeTSQuery(*tsq);
                }

                // JSONB and XML (stored as string with length prefix)
                case DataType::JSONB:
                {
                    if (size < 4) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for JSONB");
                        return std::nullopt;
                    }
                    uint32_t len;
                    std::memcpy(&len, data, 4);
                    if (size < 4 + len) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for JSONB string");
                        return std::nullopt;
                    }
                    std::string str(reinterpret_cast<const char*>(data + 4), len);
                    return TypedValue::makeJSON(str);  // Use JSON for now
                }

                case DataType::XML:
                {
                    if (size < 4) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for XML");
                        return std::nullopt;
                    }
                    uint32_t len;
                    std::memcpy(&len, data, 4);
                    if (size < 4 + len) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for XML string");
                        return std::nullopt;
                    }
                    std::string str(reinterpret_cast<const char*>(data + 4), len);
                    return TypedValue::makeText(str);  // Use text for now
                }

                // Composite type (field names + values)
                case DataType::COMPOSITE:
                {
                    if (size < 4) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for COMPOSITE");
                        return std::nullopt;
                    }
                    uint32_t num_fields;
                    std::memcpy(&num_fields, data, 4);
                    size_t offset = 4;

                    std::vector<std::string> field_names;
                    std::vector<std::shared_ptr<TypedValue>> field_values;

                    for (uint32_t i = 0; i < num_fields; ++i) {
                        // Field name
                        if (offset + 4 > size) {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for COMPOSITE field name length");
                            return std::nullopt;
                        }
                        uint32_t name_len;
                        std::memcpy(&name_len, data + offset, 4);
                        offset += 4;

                        if (offset + name_len > size) {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for COMPOSITE field name");
                            return std::nullopt;
                        }
                        std::string name(reinterpret_cast<const char*>(data + offset), name_len);
                        offset += name_len;
                        field_names.push_back(name);

                        // Field type
                        if (offset + 1 > size) {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for COMPOSITE field type");
                            return std::nullopt;
                        }
                        DataType field_type = static_cast<DataType>(data[offset]);
                        offset += 1;

                        // Field value
                        if (offset + 4 > size) {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for COMPOSITE field value length");
                            return std::nullopt;
                        }
                        uint32_t field_len;
                        std::memcpy(&field_len, data + offset, 4);
                        offset += 4;

                        if (offset + field_len > size) {
                            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                              "Insufficient data for COMPOSITE field value");
                            return std::nullopt;
                        }
                        auto field_val = deserialize(field_type, data + offset, field_len, ctx);
                        if (!field_val) return std::nullopt;
                        offset += field_len;

                        field_values.push_back(std::make_shared<TypedValue>(*field_val));
                    }

                    // Convert shared_ptr vector to value vector
                    std::vector<TypedValue> field_values_copy;
                    for (const auto& fv_ptr : field_values) {
                        field_values_copy.push_back(*fv_ptr);
                    }

                    return TypedValue::makeComposite(field_names, field_values_copy);
                }

                // Variant type (type tag + value)
                case DataType::VARIANT:
                {
                    if (size < 1) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for VARIANT");
                        return std::nullopt;
                    }
                    DataType actual_type = static_cast<DataType>(data[0]);
                    size_t offset = 1;

                    if (offset + 4 > size) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for VARIANT value length");
                        return std::nullopt;
                    }
                    uint32_t val_len;
                    std::memcpy(&val_len, data + offset, 4);
                    offset += 4;

                    if (val_len == 0) {
                        // Null value
                        return TypedValue::makeVariant(actual_type, TypedValue::makeNull());
                    }

                    if (offset + val_len > size) {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Insufficient data for VARIANT value");
                        return std::nullopt;
                    }
                    auto val = deserialize(actual_type, data + offset, val_len, ctx);
                    if (!val) return std::nullopt;

                    return TypedValue::makeVariant(actual_type, *val);
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
        auto TypeSerializer::getSerializedSize(const TypedValue &value) -> uint32_t
        {
            if (value.isNull())
                return 0;

            switch (value.type())
            {
                case DataType::INT8:
                case DataType::UINT8:
                case DataType::BOOLEAN:
                    return 1;

                case DataType::INT16:
                case DataType::UINT16:
                    return 2;

                case DataType::INT32:
                case DataType::UINT32:
                case DataType::FLOAT32:
                    return 4;

                case DataType::INT64:
                case DataType::UINT64:
                case DataType::FLOAT64:
                case DataType::DATE:
                case DataType::TIME:
                case DataType::MONEY:
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
                case DataType::INT128:
                case DataType::INTERVAL:
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

                // Spatial types - variable size WKB format
                case DataType::POINT:
                {
                    Point pt = value.getPoint();
                    auto wkb = spatial::WKBSerializer::serializePoint(pt);
                    return static_cast<uint32_t>(wkb.size());
                }

                case DataType::LINESTRING:
                {
                    LineString line = value.getLineString();
                    auto wkb = spatial::WKBSerializer::serializeLineString(line);
                    return static_cast<uint32_t>(wkb.size());
                }

                case DataType::POLYGON:
                {
                    Polygon poly = value.getPolygon();
                    auto wkb = spatial::WKBSerializer::serializePolygon(poly);
                    return static_cast<uint32_t>(wkb.size());
                }

                // Vector type - variable size
                case DataType::VECTOR:
                {
                    auto vec = value.getVector();
                    if (vec) {
                        auto encoded = Vector::encode(*vec);
                        return static_cast<uint32_t>(encoded.size());
                    }
                    return 0;
                }

                // Range types - variable size (1 byte flags + bounds)
                case DataType::INT4RANGE:
                {
                    Int4Range range = value.getInt4Range();
                    uint32_t size = 1; // flags byte
                    if (range.isLowerBounded()) size += 4;
                    if (range.isUpperBounded()) size += 4;
                    return size;
                }

                case DataType::INT8RANGE:
                case DataType::DATERANGE:
                case DataType::TSRANGE:
                case DataType::TSTZRANGE:
                {
                    // All are Range<int64_t> with 8-byte bounds
                    uint32_t size = 1; // flags byte
                    if (value.type() == DataType::INT8RANGE) {
                        Int8Range range = value.getInt8Range();
                        if (range.isLowerBounded()) size += 8;
                        if (range.isUpperBounded()) size += 8;
                    } else if (value.type() == DataType::DATERANGE) {
                        DateRange range = value.getDateRange();
                        if (range.isLowerBounded()) size += 8;
                        if (range.isUpperBounded()) size += 8;
                    } else if (value.type() == DataType::TSRANGE) {
                        TSRange range = value.getTSRange();
                        if (range.isLowerBounded()) size += 8;
                        if (range.isUpperBounded()) size += 8;
                    } else { // TSTZRANGE
                        TSTZRange range = value.getTSTZRange();
                        if (range.isLowerBounded()) size += 8;
                        if (range.isUpperBounded()) size += 8;
                    }
                    return size;
                }

                case DataType::NUMRANGE:
                {
                    NumRange range = value.getNumRange();
                    uint32_t size = 1; // flags byte
                    if (range.isLowerBounded()) size += 8;
                    if (range.isUpperBounded()) size += 8;
                    return size;
                }

                // Network types
                case DataType::INET:
                case DataType::CIDR:
                {
                    InetAddr addr = (value.type() == DataType::INET) ? value.getInet() : value.getCidr().toInet();
                    // 1 byte family + 1 byte netmask + address bytes (4 or 16)
                    return 2 + addr.size();
                }

                case DataType::MACADDR:
                    return 6;

                case DataType::MACADDR8:
                    return 8;

                // Text search types - variable size
                case DataType::TSVECTOR:
                {
                    auto tsv = value.getTSVector();
                    if (tsv) {
                        auto bin = tsv->toBinary();
                        return static_cast<uint32_t>(bin.size());
                    }
                    return 0;
                }

                case DataType::TSQUERY:
                {
                    auto tsq = value.getTSQuery();
                    if (tsq) {
                        auto bin = tsq->toBinary();
                        return static_cast<uint32_t>(bin.size());
                    }
                    return 0;
                }

                // JSONB and XML
                case DataType::JSONB:
                {
                    std::string str = value.getJSON();
                    return 4 + static_cast<uint32_t>(str.size());
                }

                case DataType::XML:
                {
                    std::string str = value.toString();
                    return 4 + static_cast<uint32_t>(str.size());
                }

                // Composite type - variable size
                case DataType::COMPOSITE:
                {
                    const auto& comp = value.getComposite();
                    uint32_t total = 4; // num_fields
                    for (size_t i = 0; i < comp.field_names.size(); ++i) {
                        total += 4; // name length
                        total += static_cast<uint32_t>(comp.field_names[i].size());
                        total += 1; // type byte
                        total += 4; // value length
                        auto field_data = serialize(*comp.field_values[i]);
                        total += static_cast<uint32_t>(field_data.size());
                    }
                    return total;
                }

                // Variant type - variable size
                case DataType::VARIANT:
                {
                    const auto& var = value.getVariant();
                    uint32_t total = 1 + 4; // type byte + value length
                    if (var.value) {
                        auto val_data = serialize(*var.value);
                        total += static_cast<uint32_t>(val_data.size());
                    }
                    return total;
                }

                default:
                    return 0;
            }
        }

    // Make serialization functions available through TypeSystem
    auto TypeSystem::getSerializedSize(const TypedValue &value) -> uint32_t
    {
        return TypeSerializer::getSerializedSize(value);
    }

} // namespace scratchbird::core

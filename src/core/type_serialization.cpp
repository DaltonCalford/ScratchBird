#include "scratchbird/core/type_serialization.h"
#include "scratchbird/core/typed_value.h"
#include <limits>

namespace scratchbird::core
{
    namespace
    {
        uint32_t minSerializedSize(DataType type)
        {
            switch (type)
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
                case DataType::MONEY:
                case DataType::TIMESTAMP_NS:
                    return 8;
                case DataType::DATE:
                    return 8;
                case DataType::TIME:
                case DataType::TIMESTAMP:
                case DataType::TIME_WITH_ZONE:
                case DataType::TIMESTAMP_WITH_ZONE:
                    return 12;
                case DataType::BIT:
                    return 4;
                case DataType::UUID:
                case DataType::INT128:
                case DataType::UINT128:
                    return 16;
                case DataType::INT256:
                case DataType::UINT256:
                    return 32;
                case DataType::DECIMAL256:
                    return 36;
                case DataType::TAGGED_UNION:
                    return 3; // u16 tag + 1-byte minimum ULEB128 length
                case DataType::DICT_ENCODED:
                    return 8;
                case DataType::COMPLETION_FIELD:
                case DataType::FLAT_OBJECT:
                    return 4; // length prefix or pair_count
                case DataType::PREFIX_SEARCH_FIELD:
                    return 12; // u32 token_len + u64 metadata pointer
                case DataType::MACADDR:
                    return 6;
                case DataType::MACADDR8:
                    return 8;
                case DataType::INTERVAL:
                    return 16;
                default:
                    return 0;
            }
        }
    }

    auto TypeSerializer::serialize(const TypedValue &value) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> result;
        ErrorContext ctx;
        Status status = value.serializePlainValue(result, &ctx);
        if (status != Status::OK)
        {
            return {};
        }
        return result;
    }

    auto TypeSerializer::deserialize(DataType type, const uint8_t *data, uint32_t size,
                                     ErrorContext *ctx) -> std::optional<TypedValue>
    {
        if (data == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null data pointer");
            return std::nullopt;
        }
        uint32_t min_size = minSerializedSize(type);
        if (min_size != 0 && size < min_size)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Insufficient data for type");
            return std::nullopt;
        }

        TypedValue value(type);
        std::vector<uint8_t> buffer(data, data + size);
        Status status = value.deserializePlainValue(buffer, ctx);
        if (status != Status::OK)
        {
            return std::nullopt;
        }
        return value;
    }

    auto TypeSerializer::getSerializedSize(const TypedValue &value) -> uint32_t
    {
        std::vector<uint8_t> result;
        ErrorContext ctx;
        Status status = value.serializePlainValue(result, &ctx);
        if (status != Status::OK)
        {
            return 0;
        }
        if (result.size() > std::numeric_limits<uint32_t>::max())
        {
            return 0;
        }
        return static_cast<uint32_t>(result.size());
    }

} // namespace scratchbird::core

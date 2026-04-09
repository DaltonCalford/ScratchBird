#pragma once

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/typed_value.h"

#include <vector>

namespace scratchbird::core::index_key_encoding
{
    inline auto fixedWidth(DataType type) -> size_t
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
                return 8;
            default:
                return 0;
        }
    }

    inline auto encodePlainValue(DataType type,
                                 const std::vector<uint8_t> &plain_value,
                                 std::vector<uint8_t> *encoded_out,
                                 ErrorContext *ctx) -> Status
    {
        if (encoded_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Index key encoding requires an output buffer");
            return Status::INVALID_ARGUMENT;
        }

        encoded_out->clear();

        auto requireSize = [&](size_t expected_size) -> bool {
            if (plain_value.size() != expected_size)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Index key encoding received unexpected payload size");
                return false;
            }
            return true;
        };

        switch (type)
        {
            case DataType::BOOLEAN:
                if (!requireSize(1))
                {
                    return Status::INVALID_ARGUMENT;
                }
                encoded_out->push_back(plain_value[0] != 0 ? 1u : 0u);
                return Status::OK;
            case DataType::INT8:
                if (!requireSize(1))
                {
                    return Status::INVALID_ARGUMENT;
                }
                encoded_out->push_back(static_cast<uint8_t>(plain_value[0] ^ 0x80u));
                return Status::OK;
            case DataType::UINT8:
                if (!requireSize(1))
                {
                    return Status::INVALID_ARGUMENT;
                }
                *encoded_out = plain_value;
                return Status::OK;
            case DataType::INT16:
            case DataType::UINT16:
            {
                if (!requireSize(2))
                {
                    return Status::INVALID_ARGUMENT;
                }
                uint16_t raw = static_cast<uint16_t>(plain_value[0]) |
                               (static_cast<uint16_t>(plain_value[1]) << 8);
                if (type == DataType::INT16)
                {
                    raw ^= 0x8000u;
                }
                encoded_out->push_back(static_cast<uint8_t>((raw >> 8) & 0xFFu));
                encoded_out->push_back(static_cast<uint8_t>(raw & 0xFFu));
                return Status::OK;
            }
            case DataType::INT32:
            case DataType::UINT32:
            case DataType::FLOAT32:
            {
                if (!requireSize(4))
                {
                    return Status::INVALID_ARGUMENT;
                }
                uint32_t raw = static_cast<uint32_t>(plain_value[0]) |
                               (static_cast<uint32_t>(plain_value[1]) << 8) |
                               (static_cast<uint32_t>(plain_value[2]) << 16) |
                               (static_cast<uint32_t>(plain_value[3]) << 24);
                if (type == DataType::INT32)
                {
                    raw ^= 0x80000000u;
                }
                else if (type == DataType::FLOAT32)
                {
                    raw = (raw & 0x80000000u) != 0 ? ~raw : (raw ^ 0x80000000u);
                }
                for (int shift = 24; shift >= 0; shift -= 8)
                {
                    encoded_out->push_back(static_cast<uint8_t>((raw >> shift) & 0xFFu));
                }
                return Status::OK;
            }
            case DataType::INT64:
            case DataType::UINT64:
            case DataType::FLOAT64:
            case DataType::MONEY:
            {
                if (!requireSize(8))
                {
                    return Status::INVALID_ARGUMENT;
                }
                uint64_t raw = 0;
                for (size_t i = 0; i < 8; ++i)
                {
                    raw |= static_cast<uint64_t>(plain_value[i]) << (i * 8);
                }
                if (type == DataType::INT64 || type == DataType::MONEY)
                {
                    raw ^= 0x8000000000000000ull;
                }
                else if (type == DataType::FLOAT64)
                {
                    raw = (raw & 0x8000000000000000ull) != 0
                              ? ~raw
                              : (raw ^ 0x8000000000000000ull);
                }
                for (int shift = 56; shift >= 0; shift -= 8)
                {
                    encoded_out->push_back(static_cast<uint8_t>((raw >> shift) & 0xFFu));
                }
                return Status::OK;
            }
            default:
                *encoded_out = plain_value;
                return Status::OK;
        }
    }

    inline auto decodeToPlainValue(DataType type,
                                   const uint8_t *encoded_data,
                                   size_t encoded_size,
                                   std::vector<uint8_t> *plain_out,
                                   ErrorContext *ctx) -> Status
    {
        if (encoded_data == nullptr || plain_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Index key decoding requires input bytes and an output buffer");
            return Status::INVALID_ARGUMENT;
        }

        plain_out->clear();

        auto requireSize = [&](size_t expected_size) -> bool {
            if (encoded_size < expected_size)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Index key decoding received truncated payload");
                return false;
            }
            return true;
        };

        switch (type)
        {
            case DataType::BOOLEAN:
                if (!requireSize(1))
                {
                    return Status::INVALID_ARGUMENT;
                }
                plain_out->push_back(encoded_data[0] != 0 ? 1u : 0u);
                return Status::OK;
            case DataType::INT8:
                if (!requireSize(1))
                {
                    return Status::INVALID_ARGUMENT;
                }
                plain_out->push_back(static_cast<uint8_t>(encoded_data[0] ^ 0x80u));
                return Status::OK;
            case DataType::UINT8:
                if (!requireSize(1))
                {
                    return Status::INVALID_ARGUMENT;
                }
                plain_out->push_back(encoded_data[0]);
                return Status::OK;
            case DataType::INT16:
            case DataType::UINT16:
            {
                if (!requireSize(2))
                {
                    return Status::INVALID_ARGUMENT;
                }
                uint16_t raw = (static_cast<uint16_t>(encoded_data[0]) << 8) |
                               static_cast<uint16_t>(encoded_data[1]);
                if (type == DataType::INT16)
                {
                    raw ^= 0x8000u;
                }
                plain_out->push_back(static_cast<uint8_t>(raw & 0xFFu));
                plain_out->push_back(static_cast<uint8_t>((raw >> 8) & 0xFFu));
                return Status::OK;
            }
            case DataType::INT32:
            case DataType::UINT32:
            case DataType::FLOAT32:
            {
                if (!requireSize(4))
                {
                    return Status::INVALID_ARGUMENT;
                }
                uint32_t raw = 0;
                for (size_t i = 0; i < 4; ++i)
                {
                    raw = (raw << 8) | static_cast<uint32_t>(encoded_data[i]);
                }
                if (type == DataType::INT32)
                {
                    raw ^= 0x80000000u;
                }
                else if (type == DataType::FLOAT32)
                {
                    raw = (raw & 0x80000000u) != 0 ? (raw ^ 0x80000000u) : ~raw;
                }
                for (int shift = 0; shift <= 24; shift += 8)
                {
                    plain_out->push_back(static_cast<uint8_t>((raw >> shift) & 0xFFu));
                }
                return Status::OK;
            }
            case DataType::INT64:
            case DataType::UINT64:
            case DataType::FLOAT64:
            case DataType::MONEY:
            {
                if (!requireSize(8))
                {
                    return Status::INVALID_ARGUMENT;
                }
                uint64_t raw = 0;
                for (size_t i = 0; i < 8; ++i)
                {
                    raw = (raw << 8) | static_cast<uint64_t>(encoded_data[i]);
                }
                if (type == DataType::INT64 || type == DataType::MONEY)
                {
                    raw ^= 0x8000000000000000ull;
                }
                else if (type == DataType::FLOAT64)
                {
                    raw = (raw & 0x8000000000000000ull) != 0
                              ? (raw ^ 0x8000000000000000ull)
                              : ~raw;
                }
                for (int shift = 0; shift <= 56; shift += 8)
                {
                    plain_out->push_back(static_cast<uint8_t>((raw >> shift) & 0xFFu));
                }
                return Status::OK;
            }
            default:
                plain_out->assign(encoded_data, encoded_data + encoded_size);
                return Status::OK;
        }
    }

    inline auto encodeTypedValue(const TypedValue &value,
                                 std::vector<uint8_t> *encoded_out,
                                 ErrorContext *ctx) -> Status
    {
        std::vector<uint8_t> plain_value;
        Status status = value.serializePlainValue(plain_value, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        return encodePlainValue(value.type(), plain_value, encoded_out, ctx);
    }
} // namespace scratchbird::core::index_key_encoding

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

#include <cstddef>
#include <cstdint>
#include "scratchbird/core/decimal.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/network.h"
#include "scratchbird/core/tsquery.h"
#include "scratchbird/core/types.h"

namespace scratchbird::core
{
    // Plain-value encodings use little-endian for fixed-width values.
    inline size_t decimalStorageSize(uint32_t precision)
    {
        if (precision <= 2)
        {
            return 1;
        }
        if (precision <= 4)
        {
            return 2;
        }
        if (precision <= 9)
        {
            return 4;
        }
        if (precision <= 18)
        {
            return 8;
        }
        if (precision <= 38)
        {
            return 16;
        }
        return 0;
    }

    inline bool readUint8(const uint8_t *data, size_t size, size_t &offset, uint8_t &out)
    {
        if (offset + 1 > size)
        {
            return false;
        }
        out = data[offset];
        offset += 1;
        return true;
    }

    inline bool readUint16LE(const uint8_t *data, size_t size, size_t &offset, uint16_t &out)
    {
        if (offset + 2 > size)
        {
            return false;
        }
        out = static_cast<uint16_t>(data[offset]) |
              static_cast<uint16_t>(data[offset + 1] << 8);
        offset += 2;
        return true;
    }

    inline bool readUint32LE(const uint8_t *data, size_t size, size_t &offset, uint32_t &out)
    {
        if (offset + 4 > size)
        {
            return false;
        }
        out = static_cast<uint32_t>(data[offset]) |
              (static_cast<uint32_t>(data[offset + 1]) << 8) |
              (static_cast<uint32_t>(data[offset + 2]) << 16) |
              (static_cast<uint32_t>(data[offset + 3]) << 24);
        offset += 4;
        return true;
    }

    inline bool readUint64LE(const uint8_t *data, size_t size, size_t &offset, uint64_t &out)
    {
        if (offset + 8 > size)
        {
            return false;
        }
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i)
        {
            value |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
        }
        out = value;
        offset += 8;
        return true;
    }

    inline bool skipBytes(size_t size, size_t &offset, size_t bytes)
    {
        if (offset + bytes > size)
        {
            return false;
        }
        offset += bytes;
        return true;
    }

    inline Status skipValueList(const uint8_t *data, size_t size, size_t &offset, ErrorContext *ctx)
    {
        uint32_t count = 0;
        if (!readUint32LE(data, size, offset, count))
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid element count");
            return Status::DATA_CORRUPTED;
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            uint8_t is_null = 0;
            uint16_t type_code = 0;
            uint32_t len = 0;
            if (!readUint8(data, size, offset, is_null) ||
                !readUint16LE(data, size, offset, type_code) ||
                !readUint32LE(data, size, offset, len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid element header");
                return Status::DATA_CORRUPTED;
            }

            if (!skipBytes(size, offset, len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Element payload exceeds buffer");
                return Status::DATA_CORRUPTED;
            }
        }

        return Status::OK;
    }

    inline Status skipPointList(const uint8_t *data, size_t size, size_t &offset, ErrorContext *ctx)
    {
        uint32_t count = 0;
        if (!readUint32LE(data, size, offset, count))
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid point count");
            return Status::DATA_CORRUPTED;
        }

        constexpr size_t point_size = sizeof(int32_t) + sizeof(double) * 2;
        if (count > 0)
        {
            size_t bytes = point_size * static_cast<size_t>(count);
            if (bytes / point_size != count || !skipBytes(size, offset, bytes))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Point list exceeds buffer");
                return Status::DATA_CORRUPTED;
            }
        }

        return Status::OK;
    }

    inline Status skipTSVector(const uint8_t *data, size_t size, size_t &offset, ErrorContext *ctx)
    {
        uint32_t lexeme_count = 0;
        if (!readUint32LE(data, size, offset, lexeme_count))
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TSVECTOR lexeme count");
            return Status::DATA_CORRUPTED;
        }

        for (uint32_t i = 0; i < lexeme_count; ++i)
        {
            uint16_t word_len = 0;
            if (!readUint16LE(data, size, offset, word_len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TSVECTOR word length");
                return Status::DATA_CORRUPTED;
            }
            if (!skipBytes(size, offset, word_len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "TSVECTOR word exceeds buffer");
                return Status::DATA_CORRUPTED;
            }

            uint16_t pos_count = 0;
            if (!readUint16LE(data, size, offset, pos_count))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TSVECTOR position count");
                return Status::DATA_CORRUPTED;
            }

            const size_t per_pos = sizeof(uint16_t) + sizeof(uint8_t);
            size_t bytes = per_pos * static_cast<size_t>(pos_count);
            if (bytes / per_pos != pos_count || !skipBytes(size, offset, bytes))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "TSVECTOR positions exceed buffer");
                return Status::DATA_CORRUPTED;
            }
        }

        return Status::OK;
    }

    inline Status skipTSQueryNode(const uint8_t *data, size_t size, size_t &offset, ErrorContext *ctx)
    {
        uint8_t type_byte = 0;
        if (!readUint8(data, size, offset, type_byte))
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TSQUERY node");
            return Status::DATA_CORRUPTED;
        }

        auto type = static_cast<TSQueryNode::Type>(type_byte);
        switch (type)
        {
            case TSQueryNode::Type::LEXEME:
            {
                uint16_t term_len = 0;
                if (!readUint16LE(data, size, offset, term_len))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TSQUERY term length");
                    return Status::DATA_CORRUPTED;
                }
                if (!skipBytes(size, offset, term_len))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "TSQUERY term exceeds buffer");
                    return Status::DATA_CORRUPTED;
                }
                return Status::OK;
            }
            case TSQueryNode::Type::PHRASE:
            {
                uint16_t dist = 0;
                if (!readUint16LE(data, size, offset, dist))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TSQUERY phrase distance");
                    return Status::DATA_CORRUPTED;
                }
                Status left_status = skipTSQueryNode(data, size, offset, ctx);
                if (left_status != Status::OK)
                {
                    return left_status;
                }
                return skipTSQueryNode(data, size, offset, ctx);
            }
            case TSQueryNode::Type::AND:
            case TSQueryNode::Type::OR:
            {
                Status left_status = skipTSQueryNode(data, size, offset, ctx);
                if (left_status != Status::OK)
                {
                    return left_status;
                }
                return skipTSQueryNode(data, size, offset, ctx);
            }
            case TSQueryNode::Type::NOT:
                return skipTSQueryNode(data, size, offset, ctx);
            default:
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Unknown TSQUERY node type");
                return Status::DATA_CORRUPTED;
        }
    }

    inline Status computePlainValueSize(DataType type,
                                        const TypeInfo &type_info,
                                        const uint8_t *data,
                                        size_t size,
                                        size_t &value_size,
                                        ErrorContext *ctx)
    {
        if (data == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null value buffer");
            return Status::INVALID_ARGUMENT;
        }

        size_t offset = 0;
        switch (type)
        {
            case DataType::INT8:
            case DataType::UINT8:
            case DataType::BOOLEAN:
                if (!skipBytes(size, offset, sizeof(uint8_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT8/UINT8/BOOLEAN payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::INT16:
            case DataType::UINT16:
                if (!skipBytes(size, offset, sizeof(uint16_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT16/UINT16 payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::INT32:
            case DataType::UINT32:
                if (!skipBytes(size, offset, sizeof(uint32_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT32/UINT32 payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::INT64:
            case DataType::UINT64:
            case DataType::MONEY:
                if (!skipBytes(size, offset, sizeof(uint64_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT64/UINT64 payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::FLOAT32:
                if (!skipBytes(size, offset, sizeof(float)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid FLOAT32 payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::FLOAT64:
                if (!skipBytes(size, offset, sizeof(double)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid FLOAT64 payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::DECIMAL:
            {
                uint32_t precision = type_info.precision == 0
                                         ? DECIMAL_MAX_PRECISION
                                         : type_info.precision;
                size_t width = decimalStorageSize(precision);
                if (width == 0 || !skipBytes(size, offset, width))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid DECIMAL payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::DECFLOAT16:
            {
                if (!skipBytes(size, offset, 8))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid DECFLOAT16 payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::DECFLOAT34:
            {
                if (!skipBytes(size, offset, 16))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid DECFLOAT34 payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::CHAR:
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::JSON:
            case DataType::JSONB:
            case DataType::XML:
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
            case DataType::VECTOR:
            {
                uint32_t len = 0;
                if (!readUint32LE(data, size, offset, len))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid length prefix");
                    return Status::DATA_CORRUPTED;
                }
                if (!skipBytes(size, offset, len))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Length-prefixed payload exceeds buffer");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::DATE:
                if (!skipBytes(size, offset, sizeof(int32_t) * 2))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid DATE/TIME payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::TIME:
                if (!skipBytes(size, offset, sizeof(int64_t) + sizeof(int32_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TIME payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::TIMESTAMP:
                if (!skipBytes(size, offset, sizeof(int64_t) + sizeof(int32_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TIMESTAMP payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::UUID:
            case DataType::INT128:
            case DataType::UINT128:
                if (!skipBytes(size, offset, 16))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                      "Invalid UUID/INT128/UINT128 payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::INTERVAL:
                if (!skipBytes(size, offset, sizeof(int32_t) * 2 + sizeof(int64_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INTERVAL payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::INET:
            case DataType::CIDR:
            {
                uint8_t family = 0;
                uint8_t netmask = 0;
                if (!readUint8(data, size, offset, family) ||
                    !readUint8(data, size, offset, netmask))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INET/CIDR payload");
                    return Status::DATA_CORRUPTED;
                }
                (void)netmask;
                size_t addr_bytes = (family == static_cast<uint8_t>(AddressFamily::IPv4))
                                        ? 4
                                        : (family == static_cast<uint8_t>(AddressFamily::IPv6) ? 16 : 0);
                if (addr_bytes == 0 || !skipBytes(size, offset, addr_bytes))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INET/CIDR address size");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::MACADDR:
                if (!skipBytes(size, offset, 6))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MACADDR payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::MACADDR8:
                if (!skipBytes(size, offset, 8))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MACADDR8 payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::POINT:
                if (!skipBytes(size, offset, sizeof(int32_t) + sizeof(double) * 2))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid POINT payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            case DataType::LINESTRING:
            {
                if (!skipBytes(size, offset, sizeof(int32_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid LINESTRING SRID");
                    return Status::DATA_CORRUPTED;
                }
                Status status = skipPointList(data, size, offset, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::POLYGON:
            {
                if (!skipBytes(size, offset, sizeof(int32_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid POLYGON SRID");
                    return Status::DATA_CORRUPTED;
                }
                uint32_t ring_count = 0;
                if (!readUint32LE(data, size, offset, ring_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid POLYGON ring count");
                    return Status::DATA_CORRUPTED;
                }
                for (uint32_t i = 0; i < ring_count; ++i)
                {
                    Status status = skipPointList(data, size, offset, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                }
                break;
            }
            case DataType::MULTIPOINT:
            {
                if (!skipBytes(size, offset, sizeof(int32_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOINT SRID");
                    return Status::DATA_CORRUPTED;
                }
                Status status = skipPointList(data, size, offset, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::MULTILINESTRING:
            {
                if (!skipBytes(size, offset, sizeof(int32_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTILINESTRING SRID");
                    return Status::DATA_CORRUPTED;
                }
                uint32_t line_count = 0;
                if (!readUint32LE(data, size, offset, line_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTILINESTRING count");
                    return Status::DATA_CORRUPTED;
                }
                for (uint32_t i = 0; i < line_count; ++i)
                {
                    if (!skipBytes(size, offset, sizeof(int32_t)))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid LINESTRING SRID");
                        return Status::DATA_CORRUPTED;
                    }
                    Status status = skipPointList(data, size, offset, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                }
                break;
            }
            case DataType::MULTIPOLYGON:
            {
                if (!skipBytes(size, offset, sizeof(int32_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOLYGON SRID");
                    return Status::DATA_CORRUPTED;
                }
                uint32_t poly_count = 0;
                if (!readUint32LE(data, size, offset, poly_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOLYGON count");
                    return Status::DATA_CORRUPTED;
                }
                for (uint32_t i = 0; i < poly_count; ++i)
                {
                    if (!skipBytes(size, offset, sizeof(int32_t)))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid POLYGON SRID");
                        return Status::DATA_CORRUPTED;
                    }
                    uint32_t ring_count = 0;
                    if (!readUint32LE(data, size, offset, ring_count))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid POLYGON ring count");
                        return Status::DATA_CORRUPTED;
                    }
                    for (uint32_t r = 0; r < ring_count; ++r)
                    {
                        Status status = skipPointList(data, size, offset, ctx);
                        if (status != Status::OK)
                        {
                            return status;
                        }
                    }
                }
                break;
            }
            case DataType::GEOMETRYCOLLECTION:
            {
                if (!skipBytes(size, offset, sizeof(int32_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid GEOMETRYCOLLECTION SRID");
                    return Status::DATA_CORRUPTED;
                }
                uint32_t geom_count = 0;
                if (!readUint32LE(data, size, offset, geom_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid GEOMETRYCOLLECTION count");
                    return Status::DATA_CORRUPTED;
                }
                for (uint32_t i = 0; i < geom_count; ++i)
                {
                    uint16_t type_code = 0;
                    uint32_t len = 0;
                    if (!readUint16LE(data, size, offset, type_code) ||
                        !readUint32LE(data, size, offset, len))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid geometry header");
                        return Status::DATA_CORRUPTED;
                    }
                    if (!skipBytes(size, offset, len))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Geometry payload exceeds buffer");
                        return Status::DATA_CORRUPTED;
                    }
                }
                break;
            }
            case DataType::TSVECTOR:
            {
                Status status = skipTSVector(data, size, offset, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::TSQUERY:
            {
                Status status = skipTSQueryNode(data, size, offset, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::INT4RANGE:
            {
                uint8_t flags = 0;
                if (!readUint8(data, size, offset, flags))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT4RANGE flags");
                    return Status::DATA_CORRUPTED;
                }
                if (flags & 0x01)
                {
                    break;
                }
                if ((flags & 0x02) && !skipBytes(size, offset, sizeof(int32_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT4RANGE lower bound");
                    return Status::DATA_CORRUPTED;
                }
                if ((flags & 0x04) && !skipBytes(size, offset, sizeof(int32_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT4RANGE upper bound");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::INT8RANGE:
            case DataType::DATERANGE:
            case DataType::TSRANGE:
            case DataType::TSTZRANGE:
            {
                uint8_t flags = 0;
                if (!readUint8(data, size, offset, flags))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid range flags");
                    return Status::DATA_CORRUPTED;
                }
                if (flags & 0x01)
                {
                    break;
                }
                if ((flags & 0x02) && !skipBytes(size, offset, sizeof(int64_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid range lower bound");
                    return Status::DATA_CORRUPTED;
                }
                if ((flags & 0x04) && !skipBytes(size, offset, sizeof(int64_t)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid range upper bound");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::NUMRANGE:
            {
                uint8_t flags = 0;
                if (!readUint8(data, size, offset, flags))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid NUMRANGE flags");
                    return Status::DATA_CORRUPTED;
                }
                if (flags & 0x01)
                {
                    break;
                }
                if ((flags & 0x02) && !skipBytes(size, offset, sizeof(double)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid NUMRANGE lower bound");
                    return Status::DATA_CORRUPTED;
                }
                if ((flags & 0x04) && !skipBytes(size, offset, sizeof(double)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid NUMRANGE upper bound");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::ARRAY:
            case DataType::VARIANT:
            {
                Status status = skipValueList(data, size, offset, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::COMPOSITE:
            {
                uint32_t name_len = 0;
                if (!readUint32LE(data, size, offset, name_len))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid COMPOSITE name length");
                    return Status::DATA_CORRUPTED;
                }
                if (!skipBytes(size, offset, name_len))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "COMPOSITE name exceeds buffer");
                    return Status::DATA_CORRUPTED;
                }
                Status status = skipValueList(data, size, offset, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::NULL_TYPE:
                break;
            default:
                SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Unsupported type for size calculation");
                return Status::NOT_SUPPORTED;
        }

        value_size = offset;
        return Status::OK;
    }
} // namespace scratchbird::core

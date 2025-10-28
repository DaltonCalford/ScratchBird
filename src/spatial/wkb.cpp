#include "scratchbird/spatial/wkb.h"
#include <cstring>

namespace scratchbird::spatial
{
    using namespace scratchbird::core;

    // ===== Serialization Helpers =====

    auto WKBSerializer::writeUInt8(std::vector<uint8_t>& buffer, uint8_t value) -> void
    {
        buffer.push_back(value);
    }

    auto WKBSerializer::writeUInt32(std::vector<uint8_t>& buffer, uint32_t value) -> void
    {
        // Little-endian
        buffer.push_back(value & 0xFF);
        buffer.push_back((value >> 8) & 0xFF);
        buffer.push_back((value >> 16) & 0xFF);
        buffer.push_back((value >> 24) & 0xFF);
    }

    auto WKBSerializer::writeDouble(std::vector<uint8_t>& buffer, double value) -> void
    {
        // Little-endian, treat double as uint64_t
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        for (int i = 0; i < 8; ++i) {
            buffer.push_back((bits >> (i * 8)) & 0xFF);
        }
    }

    auto WKBSerializer::writePoint(std::vector<uint8_t>& buffer, const Point& point) -> void
    {
        writeDouble(buffer, point.x);
        writeDouble(buffer, point.y);
    }

    auto WKBSerializer::writeRing(std::vector<uint8_t>& buffer, const std::vector<Point>& ring) -> void
    {
        writeUInt32(buffer, static_cast<uint32_t>(ring.size()));
        for (const auto& pt : ring) {
            writePoint(buffer, pt);
        }
    }

    // ===== Deserialization Helpers =====

    auto WKBSerializer::readUInt8(const std::vector<uint8_t>& buffer, size_t& pos, ErrorContext* ctx)
        -> std::optional<uint8_t>
    {
        if (pos >= buffer.size()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "Unexpected end of WKB data while reading uint8");
            }
            return std::nullopt;
        }
        return buffer[pos++];
    }

    auto WKBSerializer::readUInt32(const std::vector<uint8_t>& buffer, size_t& pos, bool little_endian,
                                    ErrorContext* ctx) -> std::optional<uint32_t>
    {
        if (pos + 4 > buffer.size()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "Unexpected end of WKB data while reading uint32");
            }
            return std::nullopt;
        }

        uint32_t value;
        if (little_endian) {
            value = buffer[pos] | (uint32_t(buffer[pos + 1]) << 8) |
                    (uint32_t(buffer[pos + 2]) << 16) | (uint32_t(buffer[pos + 3]) << 24);
        } else {
            value = (uint32_t(buffer[pos]) << 24) | (uint32_t(buffer[pos + 1]) << 16) |
                    (uint32_t(buffer[pos + 2]) << 8) | buffer[pos + 3];
        }
        pos += 4;
        return value;
    }

    auto WKBSerializer::readDouble(const std::vector<uint8_t>& buffer, size_t& pos, bool little_endian,
                                    ErrorContext* ctx) -> std::optional<double>
    {
        if (pos + 8 > buffer.size()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "Unexpected end of WKB data while reading double");
            }
            return std::nullopt;
        }

        uint64_t bits = 0;
        if (little_endian) {
            for (int i = 0; i < 8; ++i) {
                bits |= (uint64_t(buffer[pos + i]) << (i * 8));
            }
        } else {
            for (int i = 0; i < 8; ++i) {
                bits |= (uint64_t(buffer[pos + i]) << ((7 - i) * 8));
            }
        }
        pos += 8;

        double value;
        std::memcpy(&value, &bits, sizeof(double));
        return value;
    }

    auto WKBSerializer::readPoint(const std::vector<uint8_t>& buffer, size_t& pos, bool little_endian,
                                   ErrorContext* ctx) -> std::optional<Point>
    {
        auto x = readDouble(buffer, pos, little_endian, ctx);
        if (!x) return std::nullopt;

        auto y = readDouble(buffer, pos, little_endian, ctx);
        if (!y) return std::nullopt;

        return Point(*x, *y);
    }

    auto WKBSerializer::readRing(const std::vector<uint8_t>& buffer, size_t& pos, bool little_endian,
                                  ErrorContext* ctx) -> std::optional<std::vector<Point>>
    {
        auto num_points = readUInt32(buffer, pos, little_endian, ctx);
        if (!num_points) return std::nullopt;

        std::vector<Point> ring;
        ring.reserve(*num_points);

        for (uint32_t i = 0; i < *num_points; ++i) {
            auto pt = readPoint(buffer, pos, little_endian, ctx);
            if (!pt) return std::nullopt;
            ring.push_back(*pt);
        }

        return ring;
    }

    // ===== Public Serialization API =====

    auto WKBSerializer::serializePoint(const Point& point) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> buffer;
        buffer.reserve(21); // 1 (byte order) + 4 (type) + 16 (2 doubles)

        writeUInt8(buffer, WKB_NDR); // Little-endian
        writeUInt32(buffer, WKB_POINT);
        writePoint(buffer, point);

        return buffer;
    }

    auto WKBSerializer::serializeLineString(const LineString& line) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> buffer;
        buffer.reserve(9 + line.points.size() * 16); // Header + points

        writeUInt8(buffer, WKB_NDR);
        writeUInt32(buffer, WKB_LINESTRING);
        writeRing(buffer, line.points);

        return buffer;
    }

    auto WKBSerializer::serializePolygon(const Polygon& polygon) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> buffer;

        writeUInt8(buffer, WKB_NDR);
        writeUInt32(buffer, WKB_POLYGON);
        writeUInt32(buffer, static_cast<uint32_t>(polygon.rings.size()));

        for (const auto& ring : polygon.rings) {
            writeRing(buffer, ring);
        }

        return buffer;
    }

    // ===== Public Deserialization API =====

    auto WKBSerializer::deserializePoint(const std::vector<uint8_t>& wkb, ErrorContext* ctx)
        -> std::optional<Point>
    {
        if (wkb.size() < 21) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB data too short for POINT");
            }
            return std::nullopt;
        }

        size_t pos = 0;
        auto byte_order = readUInt8(wkb, pos, ctx);
        if (!byte_order) return std::nullopt;

        bool little_endian = (*byte_order == WKB_NDR);

        auto geom_type = readUInt32(wkb, pos, little_endian, ctx);
        if (!geom_type || *geom_type != WKB_POINT) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB geometry type is not POINT");
            }
            return std::nullopt;
        }

        return readPoint(wkb, pos, little_endian, ctx);
    }

    auto WKBSerializer::deserializeLineString(const std::vector<uint8_t>& wkb, ErrorContext* ctx)
        -> std::optional<LineString>
    {
        if (wkb.size() < 9) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB data too short for LINESTRING");
            }
            return std::nullopt;
        }

        size_t pos = 0;
        auto byte_order = readUInt8(wkb, pos, ctx);
        if (!byte_order) return std::nullopt;

        bool little_endian = (*byte_order == WKB_NDR);

        auto geom_type = readUInt32(wkb, pos, little_endian, ctx);
        if (!geom_type || *geom_type != WKB_LINESTRING) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB geometry type is not LINESTRING");
            }
            return std::nullopt;
        }

        auto points = readRing(wkb, pos, little_endian, ctx);
        if (!points) return std::nullopt;

        LineString line(std::move(*points));
        if (!line.isValid()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "LINESTRING must have at least 2 points");
            }
            return std::nullopt;
        }

        return line;
    }

    auto WKBSerializer::deserializePolygon(const std::vector<uint8_t>& wkb, ErrorContext* ctx)
        -> std::optional<Polygon>
    {
        if (wkb.size() < 9) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB data too short for POLYGON");
            }
            return std::nullopt;
        }

        size_t pos = 0;
        auto byte_order = readUInt8(wkb, pos, ctx);
        if (!byte_order) return std::nullopt;

        bool little_endian = (*byte_order == WKB_NDR);

        auto geom_type = readUInt32(wkb, pos, little_endian, ctx);
        if (!geom_type || *geom_type != WKB_POLYGON) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB geometry type is not POLYGON");
            }
            return std::nullopt;
        }

        auto num_rings = readUInt32(wkb, pos, little_endian, ctx);
        if (!num_rings) return std::nullopt;

        std::vector<std::vector<Point>> rings;
        rings.reserve(*num_rings);

        for (uint32_t i = 0; i < *num_rings; ++i) {
            auto ring = readRing(wkb, pos, little_endian, ctx);
            if (!ring) return std::nullopt;
            rings.push_back(std::move(*ring));
        }

        Polygon polygon(std::move(rings));
        if (!polygon.isValid()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "POLYGON is invalid (rings must be closed and have at least 4 points)");
            }
            return std::nullopt;
        }

        return polygon;
    }

    auto WKBSerializer::deserializeWKB(const std::vector<uint8_t>& wkb, ErrorContext* ctx)
        -> std::optional<TypedValue>
    {
        if (wkb.size() < 5) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB data too short");
            }
            return std::nullopt;
        }

        // Read byte order and geometry type to determine what to deserialize
        size_t pos = 0;
        auto byte_order = readUInt8(wkb, pos, ctx);
        if (!byte_order) return std::nullopt;

        bool little_endian = (*byte_order == WKB_NDR);

        auto geom_type = readUInt32(wkb, pos, little_endian, ctx);
        if (!geom_type) return std::nullopt;

        // Reset and deserialize based on type
        switch (*geom_type) {
            case WKB_POINT: {
                auto pt = deserializePoint(wkb, ctx);
                if (pt) return TypedValue::makePoint(*pt);
                break;
            }
            case WKB_LINESTRING: {
                auto line = deserializeLineString(wkb, ctx);
                if (line) return TypedValue::makeLineString(*line);
                break;
            }
            case WKB_POLYGON: {
                auto poly = deserializePolygon(wkb, ctx);
                if (poly) return TypedValue::makePolygon(*poly);
                break;
            }
            default:
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                        "Unknown WKB geometry type");
                }
                break;
        }

        return std::nullopt;
    }

} // namespace scratchbird::spatial

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

    auto WKBSerializer::serializeMultiPoint(const MultiPoint& multipoint) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> buffer;

        writeUInt8(buffer, WKB_NDR);
        writeUInt32(buffer, WKB_MULTIPOINT);
        writeUInt32(buffer, static_cast<uint32_t>(multipoint.points.size()));

        for (const auto& point : multipoint.points) {
            // Each point has its own byte order and type
            writeUInt8(buffer, WKB_NDR);
            writeUInt32(buffer, WKB_POINT);
            writePoint(buffer, point);
        }

        return buffer;
    }

    auto WKBSerializer::serializeMultiLineString(const MultiLineString& multilinestring) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> buffer;

        writeUInt8(buffer, WKB_NDR);
        writeUInt32(buffer, WKB_MULTILINESTRING);
        writeUInt32(buffer, static_cast<uint32_t>(multilinestring.linestrings.size()));

        for (const auto& linestring : multilinestring.linestrings) {
            // Each linestring has its own byte order and type
            writeUInt8(buffer, WKB_NDR);
            writeUInt32(buffer, WKB_LINESTRING);
            writeUInt32(buffer, static_cast<uint32_t>(linestring.points.size()));
            for (const auto& point : linestring.points) {
                writePoint(buffer, point);
            }
        }

        return buffer;
    }

    auto WKBSerializer::serializeMultiPolygon(const MultiPolygon& multipolygon) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> buffer;

        writeUInt8(buffer, WKB_NDR);
        writeUInt32(buffer, WKB_MULTIPOLYGON);
        writeUInt32(buffer, static_cast<uint32_t>(multipolygon.polygons.size()));

        for (const auto& polygon : multipolygon.polygons) {
            // Each polygon has its own byte order and type
            writeUInt8(buffer, WKB_NDR);
            writeUInt32(buffer, WKB_POLYGON);
            writeUInt32(buffer, static_cast<uint32_t>(polygon.rings.size()));
            for (const auto& ring : polygon.rings) {
                writeRing(buffer, ring);
            }
        }

        return buffer;
    }

    auto WKBSerializer::serializeGeometryCollection(const GeometryCollection& collection) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> buffer;

        writeUInt8(buffer, WKB_NDR);
        writeUInt32(buffer, WKB_GEOMETRYCOLLECTION);
        writeUInt32(buffer, static_cast<uint32_t>(collection.geometries.size()));

        for (const auto& geom : collection.geometries) {
            if (!geom) continue;  // Skip null geometries

            // Serialize each geometry based on its type
            DataType type = geom->type();
            if (type == DataType::POINT) {
                auto pt_wkb = serializePoint(geom->getPoint());
                buffer.insert(buffer.end(), pt_wkb.begin(), pt_wkb.end());
            } else if (type == DataType::LINESTRING) {
                auto ls_wkb = serializeLineString(geom->getLineString());
                buffer.insert(buffer.end(), ls_wkb.begin(), ls_wkb.end());
            } else if (type == DataType::POLYGON) {
                auto poly_wkb = serializePolygon(geom->getPolygon());
                buffer.insert(buffer.end(), poly_wkb.begin(), poly_wkb.end());
            } else if (type == DataType::MULTIPOINT) {
                auto mp_wkb = serializeMultiPoint(geom->getMultiPoint());
                buffer.insert(buffer.end(), mp_wkb.begin(), mp_wkb.end());
            } else if (type == DataType::MULTILINESTRING) {
                auto mls_wkb = serializeMultiLineString(geom->getMultiLineString());
                buffer.insert(buffer.end(), mls_wkb.begin(), mls_wkb.end());
            } else if (type == DataType::MULTIPOLYGON) {
                auto mpoly_wkb = serializeMultiPolygon(geom->getMultiPolygon());
                buffer.insert(buffer.end(), mpoly_wkb.begin(), mpoly_wkb.end());
            } else if (type == DataType::GEOMETRYCOLLECTION) {
                auto gc_wkb = serializeGeometryCollection(geom->getGeometryCollection());
                buffer.insert(buffer.end(), gc_wkb.begin(), gc_wkb.end());
            }
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

    auto WKBSerializer::deserializeMultiPoint(const std::vector<uint8_t>& wkb, ErrorContext* ctx)
        -> std::optional<MultiPoint>
    {
        if (wkb.size() < 9) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB data too short for MULTIPOINT");
            }
            return std::nullopt;
        }

        size_t pos = 0;
        auto byte_order = readUInt8(wkb, pos, ctx);
        if (!byte_order) return std::nullopt;

        bool little_endian = (*byte_order == WKB_NDR);

        auto geom_type = readUInt32(wkb, pos, little_endian, ctx);
        if (!geom_type || *geom_type != WKB_MULTIPOINT) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB geometry type is not MULTIPOINT");
            }
            return std::nullopt;
        }

        auto num_points = readUInt32(wkb, pos, little_endian, ctx);
        if (!num_points) return std::nullopt;

        std::vector<Point> points;
        points.reserve(*num_points);

        for (uint32_t i = 0; i < *num_points; ++i) {
            // Each point has its own byte order and type
            auto pt_byte_order = readUInt8(wkb, pos, ctx);
            if (!pt_byte_order) return std::nullopt;

            bool pt_little_endian = (*pt_byte_order == WKB_NDR);

            auto pt_type = readUInt32(wkb, pos, pt_little_endian, ctx);
            if (!pt_type || *pt_type != WKB_POINT) {
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                        "MULTIPOINT element is not a POINT");
                }
                return std::nullopt;
            }

            auto point = readPoint(wkb, pos, pt_little_endian, ctx);
            if (!point) return std::nullopt;
            points.push_back(*point);
        }

        return MultiPoint(std::move(points));
    }

    auto WKBSerializer::deserializeMultiLineString(const std::vector<uint8_t>& wkb, ErrorContext* ctx)
        -> std::optional<MultiLineString>
    {
        if (wkb.size() < 9) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB data too short for MULTILINESTRING");
            }
            return std::nullopt;
        }

        size_t pos = 0;
        auto byte_order = readUInt8(wkb, pos, ctx);
        if (!byte_order) return std::nullopt;

        bool little_endian = (*byte_order == WKB_NDR);

        auto geom_type = readUInt32(wkb, pos, little_endian, ctx);
        if (!geom_type || *geom_type != WKB_MULTILINESTRING) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB geometry type is not MULTILINESTRING");
            }
            return std::nullopt;
        }

        auto num_linestrings = readUInt32(wkb, pos, little_endian, ctx);
        if (!num_linestrings) return std::nullopt;

        std::vector<LineString> linestrings;
        linestrings.reserve(*num_linestrings);

        for (uint32_t i = 0; i < *num_linestrings; ++i) {
            // Each linestring has its own byte order and type
            auto ls_byte_order = readUInt8(wkb, pos, ctx);
            if (!ls_byte_order) return std::nullopt;

            bool ls_little_endian = (*ls_byte_order == WKB_NDR);

            auto ls_type = readUInt32(wkb, pos, ls_little_endian, ctx);
            if (!ls_type || *ls_type != WKB_LINESTRING) {
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                        "MULTILINESTRING element is not a LINESTRING");
                }
                return std::nullopt;
            }

            auto num_points = readUInt32(wkb, pos, ls_little_endian, ctx);
            if (!num_points) return std::nullopt;

            std::vector<Point> points;
            points.reserve(*num_points);

            for (uint32_t j = 0; j < *num_points; ++j) {
                auto point = readPoint(wkb, pos, ls_little_endian, ctx);
                if (!point) return std::nullopt;
                points.push_back(*point);
            }

            LineString linestring(std::move(points));
            if (!linestring.isValid()) {
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                        "LINESTRING in MULTILINESTRING is invalid");
                }
                return std::nullopt;
            }
            linestrings.push_back(std::move(linestring));
        }

        return MultiLineString(std::move(linestrings));
    }

    auto WKBSerializer::deserializeMultiPolygon(const std::vector<uint8_t>& wkb, ErrorContext* ctx)
        -> std::optional<MultiPolygon>
    {
        if (wkb.size() < 9) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB data too short for MULTIPOLYGON");
            }
            return std::nullopt;
        }

        size_t pos = 0;
        auto byte_order = readUInt8(wkb, pos, ctx);
        if (!byte_order) return std::nullopt;

        bool little_endian = (*byte_order == WKB_NDR);

        auto geom_type = readUInt32(wkb, pos, little_endian, ctx);
        if (!geom_type || *geom_type != WKB_MULTIPOLYGON) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB geometry type is not MULTIPOLYGON");
            }
            return std::nullopt;
        }

        auto num_polygons = readUInt32(wkb, pos, little_endian, ctx);
        if (!num_polygons) return std::nullopt;

        std::vector<Polygon> polygons;
        polygons.reserve(*num_polygons);

        for (uint32_t i = 0; i < *num_polygons; ++i) {
            // Each polygon has its own byte order and type
            auto poly_byte_order = readUInt8(wkb, pos, ctx);
            if (!poly_byte_order) return std::nullopt;

            bool poly_little_endian = (*poly_byte_order == WKB_NDR);

            auto poly_type = readUInt32(wkb, pos, poly_little_endian, ctx);
            if (!poly_type || *poly_type != WKB_POLYGON) {
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                        "MULTIPOLYGON element is not a POLYGON");
                }
                return std::nullopt;
            }

            auto num_rings = readUInt32(wkb, pos, poly_little_endian, ctx);
            if (!num_rings) return std::nullopt;

            std::vector<std::vector<Point>> rings;
            rings.reserve(*num_rings);

            for (uint32_t j = 0; j < *num_rings; ++j) {
                auto ring = readRing(wkb, pos, poly_little_endian, ctx);
                if (!ring) return std::nullopt;
                rings.push_back(std::move(*ring));
            }

            Polygon polygon(std::move(rings));
            if (!polygon.isValid()) {
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                        "POLYGON in MULTIPOLYGON is invalid");
                }
                return std::nullopt;
            }
            polygons.push_back(std::move(polygon));
        }

        return MultiPolygon(std::move(polygons));
    }

    auto WKBSerializer::deserializeGeometryCollection(const std::vector<uint8_t>& wkb, ErrorContext* ctx)
        -> std::optional<GeometryCollection>
    {
        if (wkb.size() < 9) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB data too short for GEOMETRYCOLLECTION");
            }
            return std::nullopt;
        }

        size_t pos = 0;
        auto byte_order = readUInt8(wkb, pos, ctx);
        if (!byte_order) return std::nullopt;

        bool little_endian = (*byte_order == WKB_NDR);

        auto geom_type = readUInt32(wkb, pos, little_endian, ctx);
        if (!geom_type || *geom_type != WKB_GEOMETRYCOLLECTION) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "WKB geometry type is not GEOMETRYCOLLECTION");
            }
            return std::nullopt;
        }

        auto num_geometries = readUInt32(wkb, pos, little_endian, ctx);
        if (!num_geometries) return std::nullopt;

        std::vector<std::shared_ptr<TypedValue>> geometries;
        geometries.reserve(*num_geometries);

        for (uint32_t i = 0; i < *num_geometries; ++i) {
            // Read the geometry type without advancing pos yet
            if (pos + 5 > wkb.size()) {
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                        "Unexpected end of WKB data in GEOMETRYCOLLECTION");
                }
                return std::nullopt;
            }

            size_t peek_pos = pos + 1;  // Skip byte order
            auto sub_geom_type = readUInt32(wkb, peek_pos, little_endian, ctx);
            if (!sub_geom_type) return std::nullopt;

            // Extract sub-geometry WKB (starting from current pos)
            std::vector<uint8_t> sub_wkb(wkb.begin() + pos, wkb.end());

            // Deserialize based on type
            std::shared_ptr<TypedValue> geom_value;
            if (*sub_geom_type == WKB_POINT) {
                auto pt = deserializePoint(sub_wkb, ctx);
                if (!pt) return std::nullopt;
                geom_value = std::make_shared<TypedValue>(TypedValue::makePoint(*pt));
                pos += 21;  // 1 + 4 + 16
            } else if (*sub_geom_type == WKB_LINESTRING) {
                auto ls = deserializeLineString(sub_wkb, ctx);
                if (!ls) return std::nullopt;
                geom_value = std::make_shared<TypedValue>(TypedValue::makeLineString(*ls));
                pos += 9 + ls->points.size() * 16;
            } else if (*sub_geom_type == WKB_POLYGON) {
                auto poly = deserializePolygon(sub_wkb, ctx);
                if (!poly) return std::nullopt;
                geom_value = std::make_shared<TypedValue>(TypedValue::makePolygon(*poly));
                size_t poly_size = 9;
                for (const auto& ring : poly->rings) {
                    poly_size += 4 + ring.size() * 16;
                }
                pos += poly_size;
            } else if (*sub_geom_type == WKB_MULTIPOINT) {
                auto mp = deserializeMultiPoint(sub_wkb, ctx);
                if (!mp) return std::nullopt;
                geom_value = std::make_shared<TypedValue>(TypedValue::makeMultiPoint(*mp));
                pos += 9 + mp->points.size() * 21;
            } else if (*sub_geom_type == WKB_MULTILINESTRING) {
                auto mls = deserializeMultiLineString(sub_wkb, ctx);
                if (!mls) return std::nullopt;
                geom_value = std::make_shared<TypedValue>(TypedValue::makeMultiLineString(*mls));
                size_t mls_size = 9;
                for (const auto& ls : mls->linestrings) {
                    mls_size += 9 + ls.points.size() * 16;
                }
                pos += mls_size;
            } else if (*sub_geom_type == WKB_MULTIPOLYGON) {
                auto mpoly = deserializeMultiPolygon(sub_wkb, ctx);
                if (!mpoly) return std::nullopt;
                geom_value = std::make_shared<TypedValue>(TypedValue::makeMultiPolygon(*mpoly));
                size_t mpoly_size = 9;
                for (const auto& poly : mpoly->polygons) {
                    mpoly_size += 9;
                    for (const auto& ring : poly.rings) {
                        mpoly_size += 4 + ring.size() * 16;
                    }
                }
                pos += mpoly_size;
            } else if (*sub_geom_type == WKB_GEOMETRYCOLLECTION) {
                auto gc = deserializeGeometryCollection(sub_wkb, ctx);
                if (!gc) return std::nullopt;
                geom_value = std::make_shared<TypedValue>(TypedValue::makeGeometryCollection(*gc));
                // Calculate size by re-serializing (simpler than tracking)
                auto gc_wkb = serializeGeometryCollection(*gc);
                pos += gc_wkb.size();
            } else {
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                        "Unknown geometry type in GEOMETRYCOLLECTION");
                }
                return std::nullopt;
            }

            geometries.push_back(geom_value);
        }

        return GeometryCollection(std::move(geometries));
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

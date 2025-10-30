#include "scratchbird/spatial/multi_geometry.h"
#include "scratchbird/spatial/wkt_parser.h"
#include "scratchbird/spatial/wkb.h"
#include <sstream>
#include <algorithm>
#include <limits>

namespace scratchbird::spatial
{
    // WKB type codes for multi-geometries (OGC Simple Features)
    constexpr uint32_t WKB_MULTIPOINT = 4;
    constexpr uint32_t WKB_MULTILINESTRING = 5;
    constexpr uint32_t WKB_MULTIPOLYGON = 6;
    constexpr uint32_t WKB_GEOMETRYCOLLECTION = 7;

    MultiGeometry::MultiGeometry(MultiGeometryType type)
        : type_(type)
    {
    }

    bool MultiGeometry::addGeometry(const core::TypedValue& geom, core::ErrorContext* ctx)
    {
        if (!isCompatibleGeometry(geom)) {
            if (ctx) {
                std::ostringstream oss;
                oss << "Incompatible geometry type for " << multiGeometryTypeName(type_)
                    << ": expected ";

                switch (type_) {
                    case MultiGeometryType::MULTIPOINT:
                        oss << "POINT";
                        break;
                    case MultiGeometryType::MULTILINESTRING:
                        oss << "LINESTRING";
                        break;
                    case MultiGeometryType::MULTIPOLYGON:
                        oss << "POLYGON";
                        break;
                    case MultiGeometryType::GEOMETRYCOLLECTION:
                        oss << "any geometry type";
                        break;
                }

                ctx->message = oss.str();
            }
            return false;
        }

        geometries_.push_back(geom);
        return true;
    }

    const core::TypedValue& MultiGeometry::getGeometry(size_t index) const
    {
        if (index >= geometries_.size()) {
            throw std::out_of_range("MultiGeometry index out of range");
        }
        return geometries_[index];
    }

    bool MultiGeometry::isCompatibleGeometry(const core::TypedValue& geom) const
    {
        using core::DataType;

        switch (type_) {
            case MultiGeometryType::MULTIPOINT:
                return geom.type() == DataType::POINT;

            case MultiGeometryType::MULTILINESTRING:
                return geom.type() == DataType::LINESTRING;

            case MultiGeometryType::MULTIPOLYGON:
                return geom.type() == DataType::POLYGON;

            case MultiGeometryType::GEOMETRYCOLLECTION:
                // Accept any geometry type (including other multi-geometries)
                return geom.type() == DataType::POINT ||
                       geom.type() == DataType::LINESTRING ||
                       geom.type() == DataType::POLYGON;

            default:
                return false;
        }
    }

    std::string MultiGeometry::toWKT() const
    {
        std::ostringstream oss;

        switch (type_) {
            case MultiGeometryType::MULTIPOINT:
                oss << "MULTIPOINT(";
                for (size_t i = 0; i < geometries_.size(); ++i) {
                    if (i > 0) oss << ", ";
                    const auto& pt = geometries_[i].getPoint();
                    oss << "(" << pt.x << " " << pt.y << ")";
                }
                oss << ")";
                break;

            case MultiGeometryType::MULTILINESTRING:
                oss << "MULTILINESTRING(";
                for (size_t i = 0; i < geometries_.size(); ++i) {
                    if (i > 0) oss << ", ";
                    const auto& line = geometries_[i].getLineString();
                    oss << "(";
                    for (size_t j = 0; j < line.points.size(); ++j) {
                        if (j > 0) oss << ", ";
                        oss << line.points[j].x << " " << line.points[j].y;
                    }
                    oss << ")";
                }
                oss << ")";
                break;

            case MultiGeometryType::MULTIPOLYGON:
                oss << "MULTIPOLYGON(";
                for (size_t i = 0; i < geometries_.size(); ++i) {
                    if (i > 0) oss << ", ";
                    const auto& poly = geometries_[i].getPolygon();
                    oss << "(";
                    for (size_t j = 0; j < poly.rings.size(); ++j) {
                        if (j > 0) oss << ", ";
                        oss << "(";
                        const auto& ring = poly.rings[j];
                        for (size_t k = 0; k < ring.size(); ++k) {
                            if (k > 0) oss << ", ";
                            oss << ring[k].x << " " << ring[k].y;
                        }
                        oss << ")";
                    }
                    oss << ")";
                }
                oss << ")";
                break;

            case MultiGeometryType::GEOMETRYCOLLECTION:
                oss << "GEOMETRYCOLLECTION(";
                for (size_t i = 0; i < geometries_.size(); ++i) {
                    if (i > 0) oss << ", ";

                    switch (geometries_[i].type()) {
                        case core::DataType::POINT:
                            oss << WKTParser::pointToWKT(geometries_[i].getPoint());
                            break;
                        case core::DataType::LINESTRING:
                            oss << WKTParser::lineStringToWKT(geometries_[i].getLineString());
                            break;
                        case core::DataType::POLYGON:
                            oss << WKTParser::polygonToWKT(geometries_[i].getPolygon());
                            break;
                        default:
                            // Should not happen if validation is correct
                            break;
                    }
                }
                oss << ")";
                break;
        }

        return oss.str();
    }

    std::optional<MultiGeometry> MultiGeometry::fromWKT(const std::string& wkt,
                                                         core::ErrorContext* ctx)
    {
        // Skip leading whitespace
        size_t pos = 0;
        while (pos < wkt.size() && std::isspace(wkt[pos])) {
            ++pos;
        }

        // Determine type from keyword
        MultiGeometryType type;
        if (wkt.compare(pos, 10, "MULTIPOINT") == 0) {
            type = MultiGeometryType::MULTIPOINT;
            pos += 10;
        } else if (wkt.compare(pos, 15, "MULTILINESTRING") == 0) {
            type = MultiGeometryType::MULTILINESTRING;
            pos += 15;
        } else if (wkt.compare(pos, 12, "MULTIPOLYGON") == 0) {
            type = MultiGeometryType::MULTIPOLYGON;
            pos += 12;
        } else if (wkt.compare(pos, 18, "GEOMETRYCOLLECTION") == 0) {
            type = MultiGeometryType::GEOMETRYCOLLECTION;
            pos += 18;
        } else {
            if (ctx) {
                ctx->message = "Invalid multi-geometry WKT: expected MULTIPOINT, MULTILINESTRING, MULTIPOLYGON, or GEOMETRYCOLLECTION";
            }
            return std::nullopt;
        }

        MultiGeometry result(type);

        // Skip whitespace and expect opening parenthesis
        while (pos < wkt.size() && std::isspace(wkt[pos])) {
            ++pos;
        }
        if (pos >= wkt.size() || wkt[pos] != '(') {
            if (ctx) {
                ctx->message = "Expected '(' after multi-geometry type";
            }
            return std::nullopt;
        }
        ++pos;

        // Parse geometries based on type
        if (type == MultiGeometryType::GEOMETRYCOLLECTION) {
            // Parse heterogeneous collection
            while (pos < wkt.size()) {
                // Skip whitespace
                while (pos < wkt.size() && std::isspace(wkt[pos])) {
                    ++pos;
                }

                // Check for end of collection
                if (pos < wkt.size() && wkt[pos] == ')') {
                    ++pos;
                    break;
                }

                // Extract individual geometry WKT
                size_t geom_start = pos;
                int paren_depth = 0;
                while (pos < wkt.size()) {
                    if (wkt[pos] == '(') {
                        ++paren_depth;
                    } else if (wkt[pos] == ')') {
                        if (paren_depth == 0) {
                            break; // End of GEOMETRYCOLLECTION
                        }
                        --paren_depth;
                    } else if (wkt[pos] == ',' && paren_depth == 0) {
                        break; // Next geometry in collection
                    }
                    ++pos;
                }

                std::string geom_wkt = wkt.substr(geom_start, pos - geom_start);
                auto geom_opt = WKTParser::parseWKT(geom_wkt, ctx);
                if (!geom_opt) {
                    return std::nullopt;
                }

                if (!result.addGeometry(*geom_opt, ctx)) {
                    return std::nullopt;
                }

                // Skip comma if present
                while (pos < wkt.size() && std::isspace(wkt[pos])) {
                    ++pos;
                }
                if (pos < wkt.size() && wkt[pos] == ',') {
                    ++pos;
                }
            }
        } else {
            // Parse homogeneous collection (MULTIPOINT, MULTILINESTRING, MULTIPOLYGON)
            while (pos < wkt.size()) {
                // Skip whitespace
                while (pos < wkt.size() && std::isspace(wkt[pos])) {
                    ++pos;
                }

                // Check for end of collection
                if (pos < wkt.size() && wkt[pos] == ')') {
                    ++pos;
                    break;
                }

                // Expect opening parenthesis for geometry
                if (pos >= wkt.size() || wkt[pos] != '(') {
                    if (ctx) {
                        ctx->message = "Expected '(' for geometry in multi-geometry";
                    }
                    return std::nullopt;
                }

                // Find matching closing parenthesis
                size_t geom_start = pos;
                int paren_depth = 0;
                while (pos < wkt.size()) {
                    if (wkt[pos] == '(') {
                        ++paren_depth;
                    } else if (wkt[pos] == ')') {
                        --paren_depth;
                        if (paren_depth == 0) {
                            ++pos;
                            break;
                        }
                    }
                    ++pos;
                }

                std::string geom_wkt = wkt.substr(geom_start, pos - geom_start);

                // Parse based on type
                core::TypedValue geom;
                switch (type) {
                    case MultiGeometryType::MULTIPOINT: {
                        // Parse as coordinate pair (remove outer parens for POINT constructor)
                        std::string point_wkt = "POINT" + geom_wkt;
                        auto pt_opt = WKTParser::parsePoint(point_wkt, ctx);
                        if (!pt_opt) {
                            return std::nullopt;
                        }
                        geom = core::TypedValue::makePoint(pt_opt->x, pt_opt->y);
                        break;
                    }
                    case MultiGeometryType::MULTILINESTRING: {
                        std::string line_wkt = "LINESTRING" + geom_wkt;
                        auto line_opt = WKTParser::parseLineString(line_wkt, ctx);
                        if (!line_opt) {
                            return std::nullopt;
                        }
                        geom = core::TypedValue::makeLineString(std::move(*line_opt));
                        break;
                    }
                    case MultiGeometryType::MULTIPOLYGON: {
                        std::string poly_wkt = "POLYGON" + geom_wkt;
                        auto poly_opt = WKTParser::parsePolygon(poly_wkt, ctx);
                        if (!poly_opt) {
                            return std::nullopt;
                        }
                        geom = core::TypedValue::makePolygon(std::move(*poly_opt));
                        break;
                    }
                    default:
                        return std::nullopt;
                }

                if (!result.addGeometry(geom, ctx)) {
                    return std::nullopt;
                }

                // Skip comma if present
                while (pos < wkt.size() && std::isspace(wkt[pos])) {
                    ++pos;
                }
                if (pos < wkt.size() && wkt[pos] == ',') {
                    ++pos;
                }
            }
        }

        return result;
    }

    std::vector<uint8_t> MultiGeometry::toWKB() const
    {
        std::vector<uint8_t> result;

        // Write byte order (little-endian)
        result.push_back(WKBSerializer::WKB_NDR);

        // Write geometry type
        uint32_t wkb_type = 0;
        switch (type_) {
            case MultiGeometryType::MULTIPOINT:
                wkb_type = WKB_MULTIPOINT;
                break;
            case MultiGeometryType::MULTILINESTRING:
                wkb_type = WKB_MULTILINESTRING;
                break;
            case MultiGeometryType::MULTIPOLYGON:
                wkb_type = WKB_MULTIPOLYGON;
                break;
            case MultiGeometryType::GEOMETRYCOLLECTION:
                wkb_type = WKB_GEOMETRYCOLLECTION;
                break;
        }

        // Write type as little-endian uint32
        result.push_back(static_cast<uint8_t>(wkb_type & 0xFF));
        result.push_back(static_cast<uint8_t>((wkb_type >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>((wkb_type >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((wkb_type >> 24) & 0xFF));

        // Write number of geometries
        uint32_t num_geoms = static_cast<uint32_t>(geometries_.size());
        result.push_back(static_cast<uint8_t>(num_geoms & 0xFF));
        result.push_back(static_cast<uint8_t>((num_geoms >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>((num_geoms >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((num_geoms >> 24) & 0xFF));

        // Write each geometry
        for (const auto& geom : geometries_) {
            std::vector<uint8_t> geom_wkb;

            switch (geom.type()) {
                case core::DataType::POINT:
                    geom_wkb = WKBSerializer::serializePoint(geom.getPoint());
                    break;
                case core::DataType::LINESTRING:
                    geom_wkb = WKBSerializer::serializeLineString(geom.getLineString());
                    break;
                case core::DataType::POLYGON:
                    geom_wkb = WKBSerializer::serializePolygon(geom.getPolygon());
                    break;
                default:
                    // Should not happen if validation is correct
                    continue;
            }

            result.insert(result.end(), geom_wkb.begin(), geom_wkb.end());
        }

        return result;
    }

    std::optional<MultiGeometry> MultiGeometry::fromWKB(const std::vector<uint8_t>& wkb,
                                                         core::ErrorContext* ctx)
    {
        if (wkb.size() < 9) { // 1 byte order + 4 type + 4 count
            if (ctx) {
                ctx->message = "WKB data too short for multi-geometry";
            }
            return std::nullopt;
        }

        size_t pos = 0;

        // Read byte order
        uint8_t byte_order = wkb[pos++];
        bool little_endian = (byte_order == WKBSerializer::WKB_NDR);

        // Read geometry type
        uint32_t wkb_type = 0;
        if (little_endian) {
            wkb_type = static_cast<uint32_t>(wkb[pos]) |
                      (static_cast<uint32_t>(wkb[pos + 1]) << 8) |
                      (static_cast<uint32_t>(wkb[pos + 2]) << 16) |
                      (static_cast<uint32_t>(wkb[pos + 3]) << 24);
        } else {
            wkb_type = (static_cast<uint32_t>(wkb[pos]) << 24) |
                      (static_cast<uint32_t>(wkb[pos + 1]) << 16) |
                      (static_cast<uint32_t>(wkb[pos + 2]) << 8) |
                      static_cast<uint32_t>(wkb[pos + 3]);
        }
        pos += 4;

        // Determine multi-geometry type
        MultiGeometryType type;
        switch (wkb_type) {
            case WKB_MULTIPOINT:
                type = MultiGeometryType::MULTIPOINT;
                break;
            case WKB_MULTILINESTRING:
                type = MultiGeometryType::MULTILINESTRING;
                break;
            case WKB_MULTIPOLYGON:
                type = MultiGeometryType::MULTIPOLYGON;
                break;
            case WKB_GEOMETRYCOLLECTION:
                type = MultiGeometryType::GEOMETRYCOLLECTION;
                break;
            default:
                if (ctx) {
                    ctx->message = "Invalid WKB geometry type for multi-geometry";
                }
                return std::nullopt;
        }

        MultiGeometry result(type);

        // Read number of geometries
        uint32_t num_geoms = 0;
        if (little_endian) {
            num_geoms = static_cast<uint32_t>(wkb[pos]) |
                       (static_cast<uint32_t>(wkb[pos + 1]) << 8) |
                       (static_cast<uint32_t>(wkb[pos + 2]) << 16) |
                       (static_cast<uint32_t>(wkb[pos + 3]) << 24);
        } else {
            num_geoms = (static_cast<uint32_t>(wkb[pos]) << 24) |
                       (static_cast<uint32_t>(wkb[pos + 1]) << 16) |
                       (static_cast<uint32_t>(wkb[pos + 2]) << 8) |
                       static_cast<uint32_t>(wkb[pos + 3]);
        }
        pos += 4;

        // Read each geometry
        for (uint32_t i = 0; i < num_geoms; ++i) {
            if (pos >= wkb.size()) {
                if (ctx) {
                    ctx->message = "Unexpected end of WKB data while reading geometries";
                }
                return std::nullopt;
            }

            // Extract WKB for individual geometry
            // Find the end by parsing the geometry size
            std::vector<uint8_t> geom_wkb(wkb.begin() + pos, wkb.end());

            auto geom_opt = WKBSerializer::deserializeWKB(geom_wkb, ctx);
            if (!geom_opt) {
                return std::nullopt;
            }

            if (!result.addGeometry(*geom_opt, ctx)) {
                return std::nullopt;
            }

            // Calculate the size of the geometry we just read
            // This is a simplification - in production, we'd need proper WKB size calculation
            size_t geom_size = 0;
            switch (geom_opt->type()) {
                case core::DataType::POINT:
                    geom_size = 21; // 1 + 4 + 16 (byte order + type + 2 doubles)
                    break;
                case core::DataType::LINESTRING: {
                    const auto& line = geom_opt->getLineString();
                    geom_size = 9 + (line.points.size() * 16); // 1 + 4 + 4 + points
                    break;
                }
                case core::DataType::POLYGON: {
                    const auto& poly = geom_opt->getPolygon();
                    geom_size = 9; // 1 + 4 + 4 (byte order + type + num_rings)
                    for (const auto& ring : poly.rings) {
                        geom_size += 4 + (ring.size() * 16); // num_points + points
                    }
                    break;
                }
                default:
                    break;
            }

            pos += geom_size;
        }

        return result;
    }

    BoundingBox MultiGeometry::boundingBox() const
    {
        BoundingBox bbox;

        for (const auto& geom : geometries_) {
            BoundingBox geom_bbox = getGeometryBoundingBox(geom);
            bbox.expand(geom_bbox);
        }

        return bbox;
    }

    BoundingBox MultiGeometry::getGeometryBoundingBox(const core::TypedValue& geom)
    {
        BoundingBox bbox;

        switch (geom.type()) {
            case core::DataType::POINT: {
                const auto& pt = geom.getPoint();
                bbox.expand(pt);
                break;
            }
            case core::DataType::LINESTRING: {
                const auto& line = geom.getLineString();
                for (const auto& pt : line.points) {
                    bbox.expand(pt);
                }
                break;
            }
            case core::DataType::POLYGON: {
                const auto& poly = geom.getPolygon();
                for (const auto& ring : poly.rings) {
                    for (const auto& pt : ring) {
                        bbox.expand(pt);
                    }
                }
                break;
            }
            default:
                break;
        }

        return bbox;
    }

    bool MultiGeometry::isValid() const
    {
        if (geometries_.empty()) {
            return false; // Multi-geometry should contain at least one geometry
        }

        for (const auto& geom : geometries_) {
            if (!isCompatibleGeometry(geom)) {
                return false;
            }

            // Validate individual geometries
            switch (geom.type()) {
                case core::DataType::LINESTRING:
                    if (!geom.getLineString().isValid()) {
                        return false;
                    }
                    break;
                case core::DataType::POLYGON:
                    if (!geom.getPolygon().isValid()) {
                        return false;
                    }
                    break;
                default:
                    break;
            }
        }

        return true;
    }

} // namespace scratchbird::spatial

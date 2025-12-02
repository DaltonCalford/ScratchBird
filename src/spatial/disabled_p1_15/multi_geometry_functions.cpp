#include "scratchbird/spatial/multi_geometry_functions.h"
#include "scratchbird/spatial/multi_geometry.h"
#include "scratchbird/core/typed_value.h"
#include <sstream>
#include <unordered_set>

namespace scratchbird::spatial
{
    // Helper: Check if a TypedValue is a geometry type
    bool isGeometryType(const core::TypedValue& value)
    {
        using core::DataType;
        auto type = value.type();
        return type == DataType::POINT ||
               type == DataType::LINESTRING ||
               type == DataType::POLYGON ||
               type == DataType::MULTIPOINT ||
               type == DataType::MULTILINESTRING ||
               type == DataType::MULTIPOLYGON ||
               type == DataType::GEOMETRYCOLLECTION;
    }

    // Helper: Check if a TypedValue is a multi-geometry type
    bool isMultiGeometryType(const core::TypedValue& value)
    {
        using core::DataType;
        auto type = value.type();
        return type == DataType::MULTIPOINT ||
               type == DataType::MULTILINESTRING ||
               type == DataType::MULTIPOLYGON ||
               type == DataType::GEOMETRYCOLLECTION;
    }

    // Helper: Get the geometry type as a string for error messages
    const char* getGeometryTypeName(core::DataType type)
    {
        using core::DataType;
        switch (type) {
            case DataType::POINT: return "POINT";
            case DataType::LINESTRING: return "LINESTRING";
            case DataType::POLYGON: return "POLYGON";
            case DataType::MULTIPOINT: return "MULTIPOINT";
            case DataType::MULTILINESTRING: return "MULTILINESTRING";
            case DataType::MULTIPOLYGON: return "MULTIPOLYGON";
            case DataType::GEOMETRYCOLLECTION: return "GEOMETRYCOLLECTION";
            default: return "UNKNOWN";
        }
    }

    // ST_MULTIPOINT - Create a MULTIPOINT geometry from an array of POINT geometries
    auto ST_MULTIPOINT(const std::vector<core::TypedValue>& points,
                       core::ErrorContext* ctx)
        -> std::optional<core::TypedValue>
    {
        if (points.empty()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                    "ST_MULTIPOINT: Empty input array");
            }
            return std::nullopt;
        }

        MultiGeometry multi_geom(MultiGeometryType::MULTIPOINT);

        for (size_t i = 0; i < points.size(); ++i) {
            if (points[i].type() != core::DataType::POINT) {
                if (ctx) {
                    std::ostringstream oss;
                    oss << "ST_MULTIPOINT: Element " << i << " is not a POINT (got "
                        << getGeometryTypeName(points[i].type()) << ")";
                    SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, oss.str());
                }
                return std::nullopt;
            }

            if (!multi_geom.addGeometry(points[i], ctx)) {
                return std::nullopt;
            }
        }

        return core::TypedValue::makeMultiPoint(multi_geom);
    }

    // ST_MULTILINESTRING - Create a MULTILINESTRING from an array of LINESTRING geometries
    auto ST_MULTILINESTRING(const std::vector<core::TypedValue>& linestrings,
                            core::ErrorContext* ctx)
        -> std::optional<core::TypedValue>
    {
        if (linestrings.empty()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                    "ST_MULTILINESTRING: Empty input array");
            }
            return std::nullopt;
        }

        MultiGeometry multi_geom(MultiGeometryType::MULTILINESTRING);

        for (size_t i = 0; i < linestrings.size(); ++i) {
            if (linestrings[i].type() != core::DataType::LINESTRING) {
                if (ctx) {
                    std::ostringstream oss;
                    oss << "ST_MULTILINESTRING: Element " << i << " is not a LINESTRING (got "
                        << getGeometryTypeName(linestrings[i].type()) << ")";
                    SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, oss.str());
                }
                return std::nullopt;
            }

            if (!multi_geom.addGeometry(linestrings[i], ctx)) {
                return std::nullopt;
            }
        }

        return core::TypedValue::makeMultiLineString(multi_geom);
    }

    // ST_MULTIPOLYGON - Create a MULTIPOLYGON from an array of POLYGON geometries
    auto ST_MULTIPOLYGON(const std::vector<core::TypedValue>& polygons,
                         core::ErrorContext* ctx)
        -> std::optional<core::TypedValue>
    {
        if (polygons.empty()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                    "ST_MULTIPOLYGON: Empty input array");
            }
            return std::nullopt;
        }

        MultiGeometry multi_geom(MultiGeometryType::MULTIPOLYGON);

        for (size_t i = 0; i < polygons.size(); ++i) {
            if (polygons[i].type() != core::DataType::POLYGON) {
                if (ctx) {
                    std::ostringstream oss;
                    oss << "ST_MULTIPOLYGON: Element " << i << " is not a POLYGON (got "
                        << getGeometryTypeName(polygons[i].type()) << ")";
                    SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, oss.str());
                }
                return std::nullopt;
            }

            if (!multi_geom.addGeometry(polygons[i], ctx)) {
                return std::nullopt;
            }
        }

        // Phase 4 Enhancement: Add polygon overlap validation (violates MULTIPOLYGON constraints)
        // For now, we trust the input is valid (follows trust-the-caller pattern)

        return core::TypedValue::makeMultiPolygon(multi_geom);
    }

    // ST_GEOMETRYCOLLECTION - Create a GEOMETRYCOLLECTION from an array of mixed geometries
    auto ST_GEOMETRYCOLLECTION(const std::vector<core::TypedValue>& geometries,
                               core::ErrorContext* ctx)
        -> std::optional<core::TypedValue>
    {
        if (geometries.empty()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                    "ST_GEOMETRYCOLLECTION: Empty input array");
            }
            return std::nullopt;
        }

        MultiGeometry multi_geom(MultiGeometryType::GEOMETRYCOLLECTION);

        for (size_t i = 0; i < geometries.size(); ++i) {
            if (!isGeometryType(geometries[i])) {
                if (ctx) {
                    std::ostringstream oss;
                    oss << "ST_GEOMETRYCOLLECTION: Element " << i << " is not a geometry type (got "
                        << getGeometryTypeName(geometries[i].type()) << ")";
                    SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, oss.str());
                }
                return std::nullopt;
            }

            if (!multi_geom.addGeometry(geometries[i], ctx)) {
                return std::nullopt;
            }
        }

        return core::TypedValue::makeGeometryCollection(multi_geom);
    }

    // ST_COLLECT - Aggregate geometries into a single multi-geometry or collection
    auto ST_COLLECT(const std::vector<core::TypedValue>& geometries,
                    core::ErrorContext* ctx)
        -> std::optional<core::TypedValue>
    {
        if (geometries.empty()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                    "ST_COLLECT: Empty input array");
            }
            return std::nullopt;
        }

        // Determine the common type (if any)
        bool all_points = true;
        bool all_linestrings = true;
        bool all_polygons = true;

        for (const auto& geom : geometries) {
            if (!isGeometryType(geom)) {
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                        "ST_COLLECT: Input contains non-geometry types");
                }
                return std::nullopt;
            }

            if (geom.type() != core::DataType::POINT) {
                all_points = false;
            }
            if (geom.type() != core::DataType::LINESTRING) {
                all_linestrings = false;
            }
            if (geom.type() != core::DataType::POLYGON) {
                all_polygons = false;
            }
        }

        // Smart selection of output type
        if (all_points) {
            return ST_MULTIPOINT(geometries, ctx);
        } else if (all_linestrings) {
            return ST_MULTILINESTRING(geometries, ctx);
        } else if (all_polygons) {
            return ST_MULTIPOLYGON(geometries, ctx);
        } else {
            // Mixed types - use GEOMETRYCOLLECTION
            return ST_GEOMETRYCOLLECTION(geometries, ctx);
        }
    }

    // ST_GEOMETRYN - Get the Nth geometry from a multi-geometry or collection
    auto ST_GEOMETRYN(const core::TypedValue& multi_geom,
                      int32_t n,
                      core::ErrorContext* ctx)
        -> std::optional<core::TypedValue>
    {
        if (!isMultiGeometryType(multi_geom)) {
            if (ctx) {
                std::ostringstream oss;
                oss << "ST_GEOMETRYN: Input is not a multi-geometry type (got "
                    << getGeometryTypeName(multi_geom.type()) << ")";
                SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, oss.str());
            }
            return std::nullopt;
        }

        if (n <= 0) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                    "ST_GEOMETRYN: Index must be positive (1-based indexing)");
            }
            return std::nullopt;
        }

        // Extract the MultiGeometry object
        MultiGeometry mg;
        switch (multi_geom.type()) {
            case core::DataType::MULTIPOINT:
                mg = multi_geom.getMultiPoint();
                break;
            case core::DataType::MULTILINESTRING:
                mg = multi_geom.getMultiLineString();
                break;
            case core::DataType::MULTIPOLYGON:
                mg = multi_geom.getMultiPolygon();
                break;
            case core::DataType::GEOMETRYCOLLECTION:
                mg = multi_geom.getGeometryCollection();
                break;
            default:
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                        "ST_GEOMETRYN: Unexpected geometry type");
                }
                return std::nullopt;
        }

        // Convert to 0-based index
        size_t index = static_cast<size_t>(n - 1);

        if (index >= mg.size()) {
            if (ctx) {
                std::ostringstream oss;
                oss << "ST_GEOMETRYN: Index " << n << " exceeds number of geometries ("
                    << mg.size() << ")";
                SET_ERROR_CONTEXT(ctx, core::Status::NOT_FOUND, oss.str());
            }
            return std::nullopt;  // PostGIS returns NULL for out-of-range
        }

        return mg.getGeometry(index);
    }

    // ST_NUMGEOMETRIES - Get the number of geometries in a multi-geometry or collection
    auto ST_NUMGEOMETRIES(const core::TypedValue& multi_geom,
                          core::ErrorContext* ctx)
        -> std::optional<int32_t>
    {
        if (!isMultiGeometryType(multi_geom)) {
            if (ctx) {
                std::ostringstream oss;
                oss << "ST_NUMGEOMETRIES: Input is not a multi-geometry type (got "
                    << getGeometryTypeName(multi_geom.type()) << ")";
                SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, oss.str());
            }
            return std::nullopt;
        }

        // Extract the MultiGeometry object
        MultiGeometry mg;
        switch (multi_geom.type()) {
            case core::DataType::MULTIPOINT:
                mg = multi_geom.getMultiPoint();
                break;
            case core::DataType::MULTILINESTRING:
                mg = multi_geom.getMultiLineString();
                break;
            case core::DataType::MULTIPOLYGON:
                mg = multi_geom.getMultiPolygon();
                break;
            case core::DataType::GEOMETRYCOLLECTION:
                mg = multi_geom.getGeometryCollection();
                break;
            default:
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                        "ST_NUMGEOMETRIES: Unexpected geometry type");
                }
                return std::nullopt;
        }

        return static_cast<int32_t>(mg.size());
    }

    // ST_DUMP - Explode a multi-geometry into individual geometries
    auto ST_DUMP(const core::TypedValue& multi_geom,
                 core::ErrorContext* ctx)
        -> std::optional<std::vector<core::TypedValue>>
    {
        if (!isMultiGeometryType(multi_geom)) {
            if (ctx) {
                std::ostringstream oss;
                oss << "ST_DUMP: Input is not a multi-geometry type (got "
                    << getGeometryTypeName(multi_geom.type()) << ")";
                SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, oss.str());
            }
            return std::nullopt;
        }

        // Extract the MultiGeometry object
        MultiGeometry mg;
        switch (multi_geom.type()) {
            case core::DataType::MULTIPOINT:
                mg = multi_geom.getMultiPoint();
                break;
            case core::DataType::MULTILINESTRING:
                mg = multi_geom.getMultiLineString();
                break;
            case core::DataType::MULTIPOLYGON:
                mg = multi_geom.getMultiPolygon();
                break;
            case core::DataType::GEOMETRYCOLLECTION:
                mg = multi_geom.getGeometryCollection();
                break;
            default:
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                        "ST_DUMP: Unexpected geometry type");
                }
                return std::nullopt;
        }

        // Extract all geometries into a vector
        std::vector<core::TypedValue> result;
        result.reserve(mg.size());

        for (size_t i = 0; i < mg.size(); ++i) {
            result.push_back(mg.getGeometry(i));
        }

        return result;
    }

} // namespace scratchbird::spatial

#pragma once

#include "scratchbird/spatial/multi_geometry.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <vector>
#include <optional>

namespace scratchbird::spatial
{
    /**
     * Multi-Geometry Spatial Functions
     *
     * P1-15: Implementation of PostGIS-compatible multi-geometry functions
     *
     * This module provides functions for creating, querying, and manipulating
     * multi-geometry types (MULTIPOINT, MULTILINESTRING, MULTIPOLYGON,
     * GEOMETRYCOLLECTION).
     *
     * Standard: PostGIS / OGC Simple Features for SQL
     */

    /**
     * ST_MULTIPOINT - Create a MULTIPOINT geometry from an array of POINT geometries
     *
     * Equivalent to: SELECT ST_MULTIPOINT(ARRAY[POINT(0,0), POINT(1,1)])
     *
     * @param points Vector of POINT geometries
     * @param ctx Error context for validation errors
     * @return TypedValue containing MULTIPOINT, or nullopt on error
     *
     * Errors:
     * - INVALID_ARGUMENT: Input is not all POINT types
     * - INVALID_ARGUMENT: Empty input array
     *
     * Example:
     *   ST_MULTIPOINT([POINT(0 0), POINT(1 1), POINT(2 2)])
     *   -> MULTIPOINT((0 0), (1 1), (2 2))
     */
    auto ST_MULTIPOINT(const std::vector<core::TypedValue>& points,
                       core::ErrorContext* ctx = nullptr)
        -> std::optional<core::TypedValue>;

    /**
     * ST_MULTILINESTRING - Create a MULTILINESTRING from an array of LINESTRING geometries
     *
     * @param linestrings Vector of LINESTRING geometries
     * @param ctx Error context for validation errors
     * @return TypedValue containing MULTILINESTRING, or nullopt on error
     *
     * Errors:
     * - INVALID_ARGUMENT: Input is not all LINESTRING types
     * - INVALID_ARGUMENT: Empty input array
     *
     * Example:
     *   ST_MULTILINESTRING([LINESTRING(0 0, 1 1), LINESTRING(2 2, 3 3)])
     *   -> MULTILINESTRING((0 0, 1 1), (2 2, 3 3))
     */
    auto ST_MULTILINESTRING(const std::vector<core::TypedValue>& linestrings,
                            core::ErrorContext* ctx = nullptr)
        -> std::optional<core::TypedValue>;

    /**
     * ST_MULTIPOLYGON - Create a MULTIPOLYGON from an array of POLYGON geometries
     *
     * @param polygons Vector of POLYGON geometries
     * @param ctx Error context for validation errors
     * @return TypedValue containing MULTIPOLYGON, or nullopt on error
     *
     * Errors:
     * - INVALID_ARGUMENT: Input is not all POLYGON types
     * - INVALID_ARGUMENT: Empty input array
     * - INVALID_GEOMETRY: Polygons overlap (violates MULTIPOLYGON constraints)
     *
     * Example:
     *   ST_MULTIPOLYGON([POLYGON((0 0, 1 0, 1 1, 0 1, 0 0)),
     *                    POLYGON((2 0, 3 0, 3 1, 2 1, 2 0))])
     *   -> MULTIPOLYGON(((0 0, 1 0, 1 1, 0 1, 0 0)), ((2 0, 3 0, 3 1, 2 1, 2 0)))
     */
    auto ST_MULTIPOLYGON(const std::vector<core::TypedValue>& polygons,
                         core::ErrorContext* ctx = nullptr)
        -> std::optional<core::TypedValue>;

    /**
     * ST_GEOMETRYCOLLECTION - Create a GEOMETRYCOLLECTION from an array of mixed geometries
     *
     * @param geometries Vector of any geometry types (POINT, LINESTRING, POLYGON, MULTI*)
     * @param ctx Error context for validation errors
     * @return TypedValue containing GEOMETRYCOLLECTION, or nullopt on error
     *
     * Errors:
     * - INVALID_ARGUMENT: Input contains non-geometry types
     * - INVALID_ARGUMENT: Empty input array
     *
     * Example:
     *   ST_GEOMETRYCOLLECTION([POINT(0 0), LINESTRING(0 0, 1 1), POLYGON(...)])
     *   -> GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0, 1 1), POLYGON(...))
     */
    auto ST_GEOMETRYCOLLECTION(const std::vector<core::TypedValue>& geometries,
                               core::ErrorContext* ctx = nullptr)
        -> std::optional<core::TypedValue>;

    /**
     * ST_COLLECT - Aggregate geometries into a single multi-geometry or collection
     *
     * Similar to ST_Union but does not merge geometries, just collects them.
     * Smart selection of output type:
     * - All POINTs → MULTIPOINT
     * - All LINESTRINGs → MULTILINESTRING
     * - All POLYGONs → MULTIPOLYGON
     * - Mixed types → GEOMETRYCOLLECTION
     *
     * @param geometries Vector of geometries to collect
     * @param ctx Error context for validation errors
     * @return TypedValue containing appropriate multi-geometry type, or nullopt on error
     *
     * Errors:
     * - INVALID_ARGUMENT: Empty input array
     *
     * Example:
     *   ST_COLLECT([POINT(0 0), POINT(1 1)])           -> MULTIPOINT((0 0), (1 1))
     *   ST_COLLECT([POINT(0 0), LINESTRING(0 0, 1 1)]) -> GEOMETRYCOLLECTION(...)
     */
    auto ST_COLLECT(const std::vector<core::TypedValue>& geometries,
                    core::ErrorContext* ctx = nullptr)
        -> std::optional<core::TypedValue>;

    /**
     * ST_GEOMETRYN - Get the Nth geometry from a multi-geometry or collection
     *
     * Indexing is 1-based following PostGIS convention (ST_GeometryN(geom, 1) is first)
     *
     * @param multi_geom Multi-geometry or GeometryCollection
     * @param n 1-based index of geometry to retrieve
     * @param ctx Error context for errors
     * @return The Nth geometry, or nullopt if index out of range
     *
     * Errors:
     * - INVALID_ARGUMENT: Input is not a multi-geometry type
     * - INVALID_ARGUMENT: Index is 0 or negative
     * - NOT_FOUND: Index exceeds number of geometries
     *
     * Example:
     *   ST_GEOMETRYN(MULTIPOINT((0 0), (1 1), (2 2)), 2) -> POINT(1 1)
     *   ST_GEOMETRYN(MULTIPOINT((0 0), (1 1)), 5)        -> NULL
     */
    auto ST_GEOMETRYN(const core::TypedValue& multi_geom,
                      int32_t n,
                      core::ErrorContext* ctx = nullptr)
        -> std::optional<core::TypedValue>;

    /**
     * ST_NUMGEOMETRIES - Get the number of geometries in a multi-geometry or collection
     *
     * @param multi_geom Multi-geometry or GeometryCollection
     * @param ctx Error context for errors
     * @return Number of geometries, or 0 if not a multi-geometry
     *
     * Errors:
     * - INVALID_ARGUMENT: Input is not a multi-geometry type
     *
     * Example:
     *   ST_NUMGEOMETRIES(MULTIPOINT((0 0), (1 1), (2 2))) -> 3
     *   ST_NUMGEOMETRIES(POINT(0 0))                       -> ERROR or 1 (single geom)
     */
    auto ST_NUMGEOMETRIES(const core::TypedValue& multi_geom,
                          core::ErrorContext* ctx = nullptr)
        -> std::optional<int32_t>;

    /**
     * ST_DUMP - Explode a multi-geometry into individual geometries
     *
     * Returns an array of geometry elements from a multi-geometry or collection.
     * Inverse operation of ST_COLLECT.
     *
     * @param multi_geom Multi-geometry or GeometryCollection to explode
     * @param ctx Error context for errors
     * @return Vector of individual geometries, or nullopt on error
     *
     * Errors:
     * - INVALID_ARGUMENT: Input is not a multi-geometry type
     *
     * Example:
     *   ST_DUMP(MULTIPOINT((0 0), (1 1), (2 2)))
     *   -> [POINT(0 0), POINT(1 1), POINT(2 2)]
     *
     *   ST_DUMP(GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0, 1 1)))
     *   -> [POINT(0 0), LINESTRING(0 0, 1 1)]
     */
    auto ST_DUMP(const core::TypedValue& multi_geom,
                 core::ErrorContext* ctx = nullptr)
        -> std::optional<std::vector<core::TypedValue>>;

    /**
     * Helper: Check if a TypedValue is a geometry type
     */
    bool isGeometryType(const core::TypedValue& value);

    /**
     * Helper: Check if a TypedValue is a multi-geometry type
     */
    bool isMultiGeometryType(const core::TypedValue& value);

    /**
     * Helper: Get the geometry type as a string for error messages
     */
    const char* getGeometryTypeName(core::DataType type);

} // namespace scratchbird::spatial

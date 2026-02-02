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

#include "scratchbird/core/types.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <limits>

namespace scratchbird::spatial
{
    /**
     * Multi-Geometry Type Enumeration
     *
     * Defines the different types of multi-geometries supported:
     * - MULTIPOINT: Collection of POINT geometries
     * - MULTILINESTRING: Collection of LINESTRING geometries
     * - MULTIPOLYGON: Collection of POLYGON geometries
     * - GEOMETRYCOLLECTION: Heterogeneous collection of any geometry types
     */
    enum class MultiGeometryType : uint8_t
    {
        MULTIPOINT = 0,
        MULTILINESTRING = 1,
        MULTIPOLYGON = 2,
        GEOMETRYCOLLECTION = 3
    };

    /**
     * Bounding Box for Spatial Geometries
     *
     * Represents the minimum bounding rectangle (MBR) for a geometry or collection.
     * Used for spatial indexing and quick intersection tests.
     */
    struct BoundingBox
    {
        double min_x;
        double min_y;
        double max_x;
        double max_y;

        BoundingBox()
            : min_x(std::numeric_limits<double>::max()),
              min_y(std::numeric_limits<double>::max()),
              max_x(std::numeric_limits<double>::lowest()),
              max_y(std::numeric_limits<double>::lowest())
        {
        }

        BoundingBox(double minx, double miny, double maxx, double maxy)
            : min_x(minx), min_y(miny), max_x(maxx), max_y(maxy)
        {
        }

        // Check if bounding box is valid
        bool isValid() const {
            return min_x <= max_x && min_y <= max_y;
        }

        // Expand bounding box to include a point
        void expand(const core::Point& point) {
            if (point.x < min_x) min_x = point.x;
            if (point.y < min_y) min_y = point.y;
            if (point.x > max_x) max_x = point.x;
            if (point.y > max_y) max_y = point.y;
        }

        // Expand bounding box to include another bounding box
        void expand(const BoundingBox& other) {
            if (other.min_x < min_x) min_x = other.min_x;
            if (other.min_y < min_y) min_y = other.min_y;
            if (other.max_x > max_x) max_x = other.max_x;
            if (other.max_y > max_y) max_y = other.max_y;
        }

        // Check if this bounding box intersects another
        bool intersects(const BoundingBox& other) const {
            return !(other.min_x > max_x || other.max_x < min_x ||
                    other.min_y > max_y || other.max_y < min_y);
        }

        // Check if this bounding box contains a point
        bool contains(const core::Point& point) const {
            return point.x >= min_x && point.x <= max_x &&
                   point.y >= min_y && point.y <= max_y;
        }
    };

    /**
     * MultiGeometry Class
     *
     * Represents a collection of geometries. Supports:
     * - MULTIPOINT: Multiple Point geometries
     * - MULTILINESTRING: Multiple LineString geometries
     * - MULTIPOLYGON: Multiple Polygon geometries
     * - GEOMETRYCOLLECTION: Heterogeneous collection (mixed types)
     *
     * Provides WKT/WKB serialization, bounding box calculation, and iteration support.
     */
    class MultiGeometry
    {
    public:
        /**
         * Constructor
         * @param type The type of multi-geometry (MULTIPOINT, MULTILINESTRING, etc.)
         */
        explicit MultiGeometry(MultiGeometryType type);

        /**
         * Destructor
         */
        ~MultiGeometry() = default;

        // Copy/Move constructors and assignment operators
        MultiGeometry(const MultiGeometry&) = default;
        MultiGeometry(MultiGeometry&&) noexcept = default;
        MultiGeometry& operator=(const MultiGeometry&) = default;
        MultiGeometry& operator=(MultiGeometry&&) noexcept = default;

        /**
         * Add a geometry to the collection
         * @param geom The geometry to add (must be compatible with the collection type)
         * @param ctx Error context for validation errors
         * @return true if added successfully, false if type mismatch
         */
        bool addGeometry(const core::TypedValue& geom, core::ErrorContext* ctx = nullptr);

        /**
         * Get a geometry from the collection
         * @param index The index of the geometry to retrieve
         * @return The geometry at the specified index
         * @throws std::out_of_range if index is invalid
         */
        const core::TypedValue& getGeometry(size_t index) const;

        /**
         * Get the number of geometries in the collection
         * @return The number of geometries
         */
        size_t size() const { return geometries_.size(); }

        /**
         * Check if the collection is empty
         * @return true if empty, false otherwise
         */
        bool empty() const { return geometries_.empty(); }

        /**
         * Clear all geometries from the collection
         */
        void clear() { geometries_.clear(); }

        /**
         * Get the type of this multi-geometry
         * @return The MultiGeometryType
         */
        MultiGeometryType type() const { return type_; }

        /**
         * Convert to WKT (Well-Known Text) format
         * Examples:
         *   MULTIPOINT((0 0), (1 1), (2 2))
         *   MULTILINESTRING((0 0, 1 1), (2 2, 3 3))
         *   MULTIPOLYGON(((0 0, 10 0, 10 10, 0 10, 0 0)))
         *   GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0, 1 1))
         */
        std::string toWKT() const;

        /**
         * Parse from WKT (Well-Known Text) format
         * @param wkt The WKT string to parse
         * @param ctx Error context for parsing errors
         * @return MultiGeometry if successful, nullopt otherwise
         */
        static std::optional<MultiGeometry> fromWKT(const std::string& wkt,
                                                     core::ErrorContext* ctx = nullptr);

        /**
         * Convert to WKB (Well-Known Binary) format
         * WKB Format:
         *   - Byte order (1 byte): 0x01 = little-endian
         *   - Geometry type (4 bytes): 4 = MULTIPOINT, 5 = MULTILINESTRING, etc.
         *   - Number of geometries (4 bytes)
         *   - Geometry data (variable, each geometry is a full WKB)
         */
        std::vector<uint8_t> toWKB() const;

        /**
         * Parse from WKB (Well-Known Binary) format
         * @param wkb The WKB binary data
         * @param ctx Error context for parsing errors
         * @return MultiGeometry if successful, nullopt otherwise
         */
        static std::optional<MultiGeometry> fromWKB(const std::vector<uint8_t>& wkb,
                                                     core::ErrorContext* ctx = nullptr);

        /**
         * Calculate the bounding box of all contained geometries
         * @return The minimum bounding rectangle containing all geometries
         */
        BoundingBox boundingBox() const;

        /**
         * Validate that the multi-geometry is well-formed
         * @return true if valid, false otherwise
         */
        bool isValid() const;

        // Iterator support for range-based for loops
        auto begin() { return geometries_.begin(); }
        auto end() { return geometries_.end(); }
        auto begin() const { return geometries_.begin(); }
        auto end() const { return geometries_.end(); }
        auto cbegin() const { return geometries_.cbegin(); }
        auto cend() const { return geometries_.cend(); }

    private:
        MultiGeometryType type_;
        std::vector<core::TypedValue> geometries_;

        // Helper: Check if a geometry type is compatible with this collection
        bool isCompatibleGeometry(const core::TypedValue& geom) const;

        // Helper: Get bounding box for a single geometry
        static BoundingBox getGeometryBoundingBox(const core::TypedValue& geom);
    };

    /**
     * Get type name as string
     */
    inline const char* multiGeometryTypeName(MultiGeometryType type)
    {
        switch (type) {
            case MultiGeometryType::MULTIPOINT: return "MULTIPOINT";
            case MultiGeometryType::MULTILINESTRING: return "MULTILINESTRING";
            case MultiGeometryType::MULTIPOLYGON: return "MULTIPOLYGON";
            case MultiGeometryType::GEOMETRYCOLLECTION: return "GEOMETRYCOLLECTION";
            default: return "UNKNOWN";
        }
    }

} // namespace scratchbird::spatial

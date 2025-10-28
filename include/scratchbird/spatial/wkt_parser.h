#pragma once

#include "scratchbird/core/types.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <string>
#include <optional>

namespace scratchbird::spatial
{
    /**
     * WKT (Well-Known Text) Parser
     *
     * Parses OGC Simple Features WKT format for spatial types:
     * - POINT(x y)
     * - LINESTRING(x1 y1, x2 y2, ...)
     * - POLYGON((x1 y1, x2 y2, ..., x1 y1))
     * - POLYGON((outer), (hole1), (hole2))
     */
    class WKTParser
    {
    public:
        /**
         * Parse WKT string into a Point
         * Format: "POINT(x y)" or "POINT (x y)"
         */
        static auto parsePoint(const std::string& wkt, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::Point>;

        /**
         * Parse WKT string into a LineString
         * Format: "LINESTRING(x1 y1, x2 y2, ...)"
         */
        static auto parseLineString(const std::string& wkt, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::LineString>;

        /**
         * Parse WKT string into a Polygon
         * Format: "POLYGON((x1 y1, x2 y2, ..., x1 y1))"
         * With holes: "POLYGON((outer), (hole1), (hole2))"
         */
        static auto parsePolygon(const std::string& wkt, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::Polygon>;

        /**
         * Auto-detect type and parse WKT string
         * Returns type-tagged value
         */
        static auto parseWKT(const std::string& wkt, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::TypedValue>;

        /**
         * Generate WKT string from Point
         */
        static auto pointToWKT(const core::Point& point) -> std::string;

        /**
         * Generate WKT string from LineString
         */
        static auto lineStringToWKT(const core::LineString& line) -> std::string;

        /**
         * Generate WKT string from Polygon
         */
        static auto polygonToWKT(const core::Polygon& polygon) -> std::string;

    private:
        // Helper functions for parsing
        static auto skipWhitespace(const std::string& str, size_t& pos) -> void;
        static auto parseDouble(const std::string& str, size_t& pos, core::ErrorContext* ctx)
            -> std::optional<double>;
        static auto parseCoordinate(const std::string& str, size_t& pos, core::ErrorContext* ctx)
            -> std::optional<core::Point>;
        static auto parseCoordinateList(const std::string& str, size_t& pos, core::ErrorContext* ctx)
            -> std::optional<std::vector<core::Point>>;
        static auto parseRing(const std::string& str, size_t& pos, core::ErrorContext* ctx)
            -> std::optional<std::vector<core::Point>>;
        static auto expect(const std::string& str, size_t& pos, char expected, core::ErrorContext* ctx)
            -> bool;
        static auto expectKeyword(const std::string& str, size_t& pos, const std::string& keyword,
                                  core::ErrorContext* ctx) -> bool;
    };

} // namespace scratchbird::spatial

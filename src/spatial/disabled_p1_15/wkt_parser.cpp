/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/spatial/wkt_parser.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>
#include <iomanip>

namespace scratchbird::spatial
{
    using namespace scratchbird::core;

    // Helper: Skip whitespace
    auto WKTParser::skipWhitespace(const std::string& str, size_t& pos) -> void
    {
        while (pos < str.size() && std::isspace(static_cast<unsigned char>(str[pos]))) {
            ++pos;
        }
    }

    // Helper: Expect a specific character
    auto WKTParser::expect(const std::string& str, size_t& pos, char expected, ErrorContext* ctx) -> bool
    {
        skipWhitespace(str, pos);
        if (pos >= str.size() || str[pos] != expected) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    ("Expected '" + std::string(1, expected) + "' at position " + std::to_string(pos)).c_str());
            }
            return false;
        }
        ++pos;
        return true;
    }

    // Helper: Expect a keyword (case-insensitive)
    auto WKTParser::expectKeyword(const std::string& str, size_t& pos, const std::string& keyword,
                                   ErrorContext* ctx) -> bool
    {
        skipWhitespace(str, pos);
        size_t start = pos;
        size_t kw_len = keyword.length();

        if (pos + kw_len > str.size()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    ("Expected keyword '" + keyword + "' at position " + std::to_string(pos)).c_str());
            }
            return false;
        }

        // Case-insensitive comparison
        for (size_t i = 0; i < kw_len; ++i) {
            if (std::tolower(static_cast<unsigned char>(str[pos + i])) !=
                std::tolower(static_cast<unsigned char>(keyword[i]))) {
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                        ("Expected keyword '" + keyword + "' at position " + std::to_string(start)).c_str());
                }
                return false;
            }
        }
        pos += kw_len;
        return true;
    }

    // Helper: Parse a double value
    auto WKTParser::parseDouble(const std::string& str, size_t& pos, ErrorContext* ctx) -> std::optional<double>
    {
        skipWhitespace(str, pos);
        size_t start = pos;

        // Handle optional sign
        if (pos < str.size() && (str[pos] == '+' || str[pos] == '-')) {
            ++pos;
        }

        // Parse digits and optional decimal point
        bool has_digits = false;
        bool has_decimal = false;
        while (pos < str.size()) {
            if (std::isdigit(static_cast<unsigned char>(str[pos]))) {
                has_digits = true;
                ++pos;
            } else if (str[pos] == '.' && !has_decimal) {
                has_decimal = true;
                ++pos;
            } else if (str[pos] == 'e' || str[pos] == 'E') {
                // Scientific notation
                ++pos;
                if (pos < str.size() && (str[pos] == '+' || str[pos] == '-')) {
                    ++pos;
                }
                while (pos < str.size() && std::isdigit(static_cast<unsigned char>(str[pos]))) {
                    ++pos;
                }
                break;
            } else {
                break;
            }
        }

        if (!has_digits) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    ("Expected numeric value at position " + std::to_string(start)).c_str());
            }
            return std::nullopt;
        }

        // Convert to double
        double value;
        auto result = std::from_chars(str.data() + start, str.data() + pos, value);
        if (result.ec != std::errc()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    ("Invalid numeric value at position " + std::to_string(start)).c_str());
            }
            return std::nullopt;
        }

        return value;
    }

    // Helper: Parse a coordinate (x y)
    auto WKTParser::parseCoordinate(const std::string& str, size_t& pos, ErrorContext* ctx)
        -> std::optional<Point>
    {
        auto x = parseDouble(str, pos, ctx);
        if (!x) return std::nullopt;

        auto y = parseDouble(str, pos, ctx);
        if (!y) return std::nullopt;

        return Point(*x, *y);
    }

    // Helper: Parse a list of coordinates
    auto WKTParser::parseCoordinateList(const std::string& str, size_t& pos, ErrorContext* ctx)
        -> std::optional<std::vector<Point>>
    {
        std::vector<Point> points;

        while (true) {
            auto pt = parseCoordinate(str, pos, ctx);
            if (!pt) {
                if (points.empty()) {
                    return std::nullopt; // First coordinate must succeed
                }
                break; // No more coordinates
            }
            points.push_back(*pt);

            skipWhitespace(str, pos);
            if (pos < str.size() && str[pos] == ',') {
                ++pos; // Consume comma and continue
            } else {
                break; // No comma, end of list
            }
        }

        return points;
    }

    // Helper: Parse a ring (parenthesized coordinate list)
    auto WKTParser::parseRing(const std::string& str, size_t& pos, ErrorContext* ctx)
        -> std::optional<std::vector<Point>>
    {
        if (!expect(str, pos, '(', ctx)) return std::nullopt;
        auto points = parseCoordinateList(str, pos, ctx);
        if (!points) return std::nullopt;
        if (!expect(str, pos, ')', ctx)) return std::nullopt;
        return points;
    }

    // Parse POINT
    auto WKTParser::parsePoint(const std::string& wkt, ErrorContext* ctx) -> std::optional<Point>
    {
        size_t pos = 0;
        if (!expectKeyword(wkt, pos, "POINT", ctx)) return std::nullopt;
        skipWhitespace(wkt, pos);
        if (!expect(wkt, pos, '(', ctx)) return std::nullopt;
        auto pt = parseCoordinate(wkt, pos, ctx);
        if (!pt) return std::nullopt;
        if (!expect(wkt, pos, ')', ctx)) return std::nullopt;

        // Verify we consumed the entire string
        skipWhitespace(wkt, pos);
        if (pos < wkt.size()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    ("Unexpected characters after POINT at position " + std::to_string(pos)).c_str());
            }
            return std::nullopt;
        }

        return pt;
    }

    // Parse LINESTRING
    auto WKTParser::parseLineString(const std::string& wkt, ErrorContext* ctx) -> std::optional<LineString>
    {
        size_t pos = 0;
        if (!expectKeyword(wkt, pos, "LINESTRING", ctx)) return std::nullopt;
        auto points = parseRing(wkt, pos, ctx);
        if (!points) return std::nullopt;

        // Verify we consumed the entire string
        skipWhitespace(wkt, pos);
        if (pos < wkt.size()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    ("Unexpected characters after LINESTRING at position " + std::to_string(pos)).c_str());
            }
            return std::nullopt;
        }

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

    // Parse POLYGON
    auto WKTParser::parsePolygon(const std::string& wkt, ErrorContext* ctx) -> std::optional<Polygon>
    {
        size_t pos = 0;
        if (!expectKeyword(wkt, pos, "POLYGON", ctx)) return std::nullopt;
        skipWhitespace(wkt, pos);
        if (!expect(wkt, pos, '(', ctx)) return std::nullopt;

        std::vector<std::vector<Point>> rings;

        // Parse rings
        while (true) {
            auto ring = parseRing(wkt, pos, ctx);
            if (!ring) {
                if (rings.empty()) {
                    return std::nullopt; // First ring must succeed
                }
                break;
            }
            rings.push_back(std::move(*ring));

            skipWhitespace(wkt, pos);
            if (pos < wkt.size() && wkt[pos] == ',') {
                ++pos; // Consume comma and continue
            } else {
                break; // No comma, end of rings
            }
        }

        if (!expect(wkt, pos, ')', ctx)) return std::nullopt;

        // Verify we consumed the entire string
        skipWhitespace(wkt, pos);
        if (pos < wkt.size()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    ("Unexpected characters after POLYGON at position " + std::to_string(pos)).c_str());
            }
            return std::nullopt;
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

    // Auto-detect and parse WKT
    auto WKTParser::parseWKT(const std::string& wkt, ErrorContext* ctx) -> std::optional<TypedValue>
    {
        // Detect type by looking at first keyword
        std::string upper_wkt = wkt;
        std::transform(upper_wkt.begin(), upper_wkt.end(), upper_wkt.begin(),
                       [](unsigned char c) { return std::toupper(c); });

        if (upper_wkt.find("POINT") == 0) {
            auto pt = parsePoint(wkt, ctx);
            if (pt) return TypedValue::makePoint(*pt);
        } else if (upper_wkt.find("LINESTRING") == 0) {
            auto line = parseLineString(wkt, ctx);
            if (line) return TypedValue::makeLineString(*line);
        } else if (upper_wkt.find("POLYGON") == 0) {
            auto poly = parsePolygon(wkt, ctx);
            if (poly) return TypedValue::makePolygon(*poly);
        } else {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "Unknown WKT type (expected POINT, LINESTRING, or POLYGON)");
            }
        }

        return std::nullopt;
    }

    // Generate WKT from Point
    auto WKTParser::pointToWKT(const Point& point) -> std::string
    {
        std::ostringstream oss;
        oss << std::setprecision(15) << "POINT(" << point.x << " " << point.y << ")";
        return oss.str();
    }

    // Generate WKT from LineString
    auto WKTParser::lineStringToWKT(const LineString& line) -> std::string
    {
        std::ostringstream oss;
        oss << std::setprecision(15) << "LINESTRING(";
        for (size_t i = 0; i < line.points.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << line.points[i].x << " " << line.points[i].y;
        }
        oss << ")";
        return oss.str();
    }

    // Generate WKT from Polygon
    auto WKTParser::polygonToWKT(const Polygon& polygon) -> std::string
    {
        std::ostringstream oss;
        oss << std::setprecision(15) << "POLYGON(";
        for (size_t r = 0; r < polygon.rings.size(); ++r) {
            if (r > 0) oss << ", ";
            oss << "(";
            const auto& ring = polygon.rings[r];
            for (size_t i = 0; i < ring.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << ring[i].x << " " << ring[i].y;
            }
            oss << ")";
        }
        oss << ")";
        return oss.str();
    }

} // namespace scratchbird::spatial

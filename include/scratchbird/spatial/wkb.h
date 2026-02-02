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
#include <cstdint>
#include <optional>

namespace scratchbird::spatial
{
    /**
     * WKB (Well-Known Binary) Serialization
     *
     * Implements OGC Simple Features WKB format for binary storage
     * WKB Format:
     * - Byte order (1 byte): 0x01 = little-endian, 0x00 = big-endian
     * - Geometry type (4 bytes): 1 = Point, 2 = LineString, 3 = Polygon
     * - Geometry data (variable)
     */
    class WKBSerializer
    {
    public:
        // WKB geometry type codes (OGC Simple Features)
        static constexpr uint32_t WKB_POINT = 1;
        static constexpr uint32_t WKB_LINESTRING = 2;
        static constexpr uint32_t WKB_POLYGON = 3;
        static constexpr uint32_t WKB_MULTIPOINT = 4;
        static constexpr uint32_t WKB_MULTILINESTRING = 5;
        static constexpr uint32_t WKB_MULTIPOLYGON = 6;
        static constexpr uint32_t WKB_GEOMETRYCOLLECTION = 7;

        // Byte order markers
        static constexpr uint8_t WKB_XDR = 0; // Big-endian
        static constexpr uint8_t WKB_NDR = 1; // Little-endian (default)

        /**
         * Serialize Point to WKB binary format
         */
        static auto serializePoint(const core::Point& point) -> std::vector<uint8_t>;

        /**
         * Serialize LineString to WKB binary format
         */
        static auto serializeLineString(const core::LineString& line) -> std::vector<uint8_t>;

        /**
         * Serialize Polygon to WKB binary format
         */
        static auto serializePolygon(const core::Polygon& polygon) -> std::vector<uint8_t>;

        /**
         * Serialize MultiPoint to WKB binary format
         */
        static auto serializeMultiPoint(const core::MultiPoint& multipoint) -> std::vector<uint8_t>;

        /**
         * Serialize MultiLineString to WKB binary format
         */
        static auto serializeMultiLineString(const core::MultiLineString& multilinestring) -> std::vector<uint8_t>;

        /**
         * Serialize MultiPolygon to WKB binary format
         */
        static auto serializeMultiPolygon(const core::MultiPolygon& multipolygon) -> std::vector<uint8_t>;

        /**
         * Serialize GeometryCollection to WKB binary format
         */
        static auto serializeGeometryCollection(const core::GeometryCollection& collection) -> std::vector<uint8_t>;

        /**
         * Deserialize Point from WKB binary format
         */
        static auto deserializePoint(const std::vector<uint8_t>& wkb, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::Point>;

        /**
         * Deserialize LineString from WKB binary format
         */
        static auto deserializeLineString(const std::vector<uint8_t>& wkb, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::LineString>;

        /**
         * Deserialize Polygon from WKB binary format
         */
        static auto deserializePolygon(const std::vector<uint8_t>& wkb, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::Polygon>;

        /**
         * Deserialize MultiPoint from WKB binary format
         */
        static auto deserializeMultiPoint(const std::vector<uint8_t>& wkb, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::MultiPoint>;

        /**
         * Deserialize MultiLineString from WKB binary format
         */
        static auto deserializeMultiLineString(const std::vector<uint8_t>& wkb, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::MultiLineString>;

        /**
         * Deserialize MultiPolygon from WKB binary format
         */
        static auto deserializeMultiPolygon(const std::vector<uint8_t>& wkb, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::MultiPolygon>;

        /**
         * Deserialize GeometryCollection from WKB binary format
         */
        static auto deserializeGeometryCollection(const std::vector<uint8_t>& wkb, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::GeometryCollection>;

        /**
         * Auto-detect type and deserialize WKB
         */
        static auto deserializeWKB(const std::vector<uint8_t>& wkb, core::ErrorContext* ctx = nullptr)
            -> std::optional<core::TypedValue>;

    private:
        // Helper functions for serialization
        static auto writeUInt8(std::vector<uint8_t>& buffer, uint8_t value) -> void;
        static auto writeUInt32(std::vector<uint8_t>& buffer, uint32_t value) -> void;
        static auto writeDouble(std::vector<uint8_t>& buffer, double value) -> void;
        static auto writePoint(std::vector<uint8_t>& buffer, const core::Point& point) -> void;
        static auto writeRing(std::vector<uint8_t>& buffer, const std::vector<core::Point>& ring) -> void;

        // Helper functions for deserialization
        static auto readUInt8(const std::vector<uint8_t>& buffer, size_t& pos, core::ErrorContext* ctx)
            -> std::optional<uint8_t>;
        static auto readUInt32(const std::vector<uint8_t>& buffer, size_t& pos, bool little_endian,
                               core::ErrorContext* ctx) -> std::optional<uint32_t>;
        static auto readDouble(const std::vector<uint8_t>& buffer, size_t& pos, bool little_endian,
                               core::ErrorContext* ctx) -> std::optional<double>;
        static auto readPoint(const std::vector<uint8_t>& buffer, size_t& pos, bool little_endian,
                              core::ErrorContext* ctx) -> std::optional<core::Point>;
        static auto readRing(const std::vector<uint8_t>& buffer, size_t& pos, bool little_endian,
                             core::ErrorContext* ctx) -> std::optional<std::vector<core::Point>>;
    };

} // namespace scratchbird::spatial

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
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <memory>
#include <string>
#include <optional>

#ifdef HAVE_GEOS
#include <geos_c.h>
#endif

namespace scratchbird::spatial
{
    /**
     * GEOS Library Wrapper
     *
     * Provides RAII wrappers for GEOS library resources and conversion
     * functions between ScratchBird geometry types and GEOS geometries.
     *
     * GEOS (Geometry Engine - Open Source) is a C++ port of the JTS
     * (Java Topology Suite) library, providing spatial predicates and
     * operations for geometry objects.
     *
     * Thread Safety: Each GEOSContext is thread-local. Create one per thread.
     */

#ifdef HAVE_GEOS

    /**
     * RAII wrapper for GEOS context handle
     *
     * Automatically initializes and cleans up GEOS context.
     * Context must be passed to all GEOS C API functions.
     */
    class GEOSContext
    {
    public:
        /**
         * Initialize GEOS context
         * Throws std::runtime_error if initialization fails
         */
        GEOSContext();

        /**
         * Cleanup GEOS context
         */
        ~GEOSContext();

        // Non-copyable, movable
        GEOSContext(const GEOSContext&) = delete;
        GEOSContext& operator=(const GEOSContext&) = delete;
        GEOSContext(GEOSContext&& other) noexcept;
        GEOSContext& operator=(GEOSContext&& other) noexcept;

        /**
         * Get the raw GEOS context handle
         * Required for all GEOS C API calls
         */
        GEOSContextHandle_t handle() const { return ctx_; }

        /**
         * Get last error message from GEOS
         */
        std::string getLastError() const;

    private:
        GEOSContextHandle_t ctx_;
        static void errorHandler(const char* message, void* userdata);
        static void warningHandler(const char* message, void* userdata);
        mutable std::string last_error_;
    };

    /**
     * RAII wrapper for GEOS geometry pointer
     *
     * Automatically manages memory for GEOS geometry objects.
     * Handles proper cleanup via GEOS API.
     */
    class GEOSGeomPtr
    {
    public:
        /**
         * Take ownership of GEOS geometry
         *
         * @param geom GEOS geometry pointer (can be nullptr)
         * @param ctx GEOS context handle for cleanup
         */
        GEOSGeomPtr(GEOSGeometry* geom, GEOSContextHandle_t ctx)
            : geom_(geom), ctx_(ctx) {}

        /**
         * Cleanup GEOS geometry
         */
        ~GEOSGeomPtr();

        // Non-copyable, movable
        GEOSGeomPtr(const GEOSGeomPtr&) = delete;
        GEOSGeomPtr& operator=(const GEOSGeomPtr&) = delete;
        GEOSGeomPtr(GEOSGeomPtr&& other) noexcept;
        GEOSGeomPtr& operator=(GEOSGeomPtr&& other) noexcept;

        /**
         * Get raw GEOS geometry pointer
         */
        GEOSGeometry* get() const { return geom_; }

        /**
         * Release ownership of geometry
         * Caller is responsible for cleanup
         */
        GEOSGeometry* release();

        /**
         * Check if geometry is valid (non-null)
         */
        explicit operator bool() const { return geom_ != nullptr; }

    private:
        GEOSGeometry* geom_;
        GEOSContextHandle_t ctx_;
    };

    /**
     * RAII wrapper for GEOS coordinate sequence
     */
    class GEOSCoordSeqPtr
    {
    public:
        GEOSCoordSeqPtr(GEOSCoordSequence* seq, GEOSContextHandle_t ctx)
            : seq_(seq), ctx_(ctx) {}

        ~GEOSCoordSeqPtr();

        // Non-copyable, movable
        GEOSCoordSeqPtr(const GEOSCoordSeqPtr&) = delete;
        GEOSCoordSeqPtr& operator=(const GEOSCoordSeqPtr&) = delete;
        GEOSCoordSeqPtr(GEOSCoordSeqPtr&& other) noexcept;
        GEOSCoordSeqPtr& operator=(GEOSCoordSeqPtr&& other) noexcept;

        GEOSCoordSequence* get() const { return seq_; }
        GEOSCoordSequence* release();
        explicit operator bool() const { return seq_ != nullptr; }

    private:
        GEOSCoordSequence* seq_;
        GEOSContextHandle_t ctx_;
    };

    /**
     * Convert ScratchBird Point to GEOS geometry
     *
     * @param point ScratchBird Point
     * @param ctx GEOS context
     * @param error_ctx Optional error context for detailed errors
     * @return GEOS geometry or nullptr on error
     */
    GEOSGeomPtr pointToGEOS(const core::Point& point, GEOSContext& ctx,
                            core::ErrorContext* error_ctx = nullptr);

    /**
     * Convert ScratchBird LineString to GEOS geometry
     *
     * @param line ScratchBird LineString
     * @param ctx GEOS context
     * @param error_ctx Optional error context for detailed errors
     * @return GEOS geometry or nullptr on error
     */
    GEOSGeomPtr lineStringToGEOS(const core::LineString& line, GEOSContext& ctx,
                                  core::ErrorContext* error_ctx = nullptr);

    /**
     * Convert ScratchBird Polygon to GEOS geometry
     *
     * @param polygon ScratchBird Polygon
     * @param ctx GEOS context
     * @param error_ctx Optional error context for detailed errors
     * @return GEOS geometry or nullptr on error
     */
    GEOSGeomPtr polygonToGEOS(const core::Polygon& polygon, GEOSContext& ctx,
                              core::ErrorContext* error_ctx = nullptr);

    /**
     * Convert GEOS geometry to ScratchBird Point
     *
     * @param geom GEOS geometry (must be Point type)
     * @param ctx GEOS context
     * @param error_ctx Optional error context for detailed errors
     * @return ScratchBird Point or nullopt on error
     */
    std::optional<core::Point> geosToPoint(const GEOSGeometry* geom, GEOSContext& ctx,
                                           core::ErrorContext* error_ctx = nullptr);

    /**
     * Convert GEOS geometry to ScratchBird LineString
     *
     * @param geom GEOS geometry (must be LineString type)
     * @param ctx GEOS context
     * @param error_ctx Optional error context for detailed errors
     * @return ScratchBird LineString or nullopt on error
     */
    std::optional<core::LineString> geosToLineString(const GEOSGeometry* geom, GEOSContext& ctx,
                                                      core::ErrorContext* error_ctx = nullptr);

    /**
     * Convert GEOS geometry to ScratchBird Polygon
     *
     * @param geom GEOS geometry (must be Polygon type)
     * @param ctx GEOS context
     * @param error_ctx Optional error context for detailed errors
     * @return ScratchBird Polygon or nullopt on error
     */
    std::optional<core::Polygon> geosToPolygon(const GEOSGeometry* geom, GEOSContext& ctx,
                                                core::ErrorContext* error_ctx = nullptr);

    /**
     * Convert GEOS geometry to appropriate ScratchBird TypedValue
     * Auto-detects geometry type and converts accordingly
     *
     * @param geom GEOS geometry
     * @param ctx GEOS context
     * @param error_ctx Optional error context for detailed errors
     * @return ScratchBird TypedValue (Point, LineString, or Polygon) or nullopt on error
     */
    std::optional<core::TypedValue> geosToTypedValue(const GEOSGeometry* geom, GEOSContext& ctx,
                                                      core::ErrorContext* error_ctx = nullptr);

#else
    // Stub implementations when GEOS is not available
    // These will throw runtime errors if called

    class GEOSContext
    {
    public:
        GEOSContext();
        ~GEOSContext() = default;
    };

#endif // HAVE_GEOS

} // namespace scratchbird::spatial

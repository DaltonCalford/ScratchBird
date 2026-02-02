/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/geo/proj_wrapper.h"
#include <cstring>
#include <sstream>
#include <cmath>

namespace scratchbird {
namespace geo {

#ifdef HAVE_PROJ

// ============================================================================
// PROJContext Implementation
// ============================================================================

PROJContext::PROJContext() : ctx_(nullptr) {
    ctx_ = proj_context_create();
    if (!ctx_) {
        throw PROJException("Failed to create PROJ context");
    }
}

PROJContext::~PROJContext() {
    if (ctx_) {
        proj_context_destroy(ctx_);
        ctx_ = nullptr;
    }
}

PROJContext::PROJContext(PROJContext&& other) noexcept
    : ctx_(other.ctx_) {
    other.ctx_ = nullptr;
}

PROJContext& PROJContext::operator=(PROJContext&& other) noexcept {
    if (this != &other) {
        if (ctx_) {
            proj_context_destroy(ctx_);
        }
        ctx_ = other.ctx_;
        other.ctx_ = nullptr;
    }
    return *this;
}

std::string PROJContext::getLastError() const {
    if (!ctx_) {
        return "Context is null";
    }
    int err = proj_context_errno(ctx_);
    if (err == 0) {
        return "";
    }
    const char* msg = proj_errno_string(err);
    return msg ? msg : "Unknown PROJ error";
}

// ============================================================================
// PROJTransform Implementation
// ============================================================================

PROJTransform::PROJTransform(const SRID& from, const SRID& to)
    : transform_(nullptr), context_(nullptr), from_srid_(from.id()), to_srid_(to.id()) {

    if (!from.isValid()) {
        throw PROJException("Source SRID " + std::to_string(from.id()) + " is not valid");
    }
    if (!to.isValid()) {
        throw PROJException("Target SRID " + std::to_string(to.id()) + " is not valid");
    }

    std::string from_proj = from.proj4();
    std::string to_proj = to.proj4();

    if (from_proj.empty()) {
        throw PROJException("No PROJ string for source SRID " + std::to_string(from.id()));
    }
    if (to_proj.empty()) {
        throw PROJException("No PROJ string for target SRID " + std::to_string(to.id()));
    }

    initialize(from_proj, to_proj);
}

PROJTransform::PROJTransform(int from_epsg, int to_epsg)
    : PROJTransform(SRID(from_epsg), SRID(to_epsg)) {
}

PROJTransform::PROJTransform(const std::string& from_proj, const std::string& to_proj)
    : transform_(nullptr), context_(nullptr), from_srid_(0), to_srid_(0) {

    if (from_proj.empty()) {
        throw PROJException("Source PROJ string is empty");
    }
    if (to_proj.empty()) {
        throw PROJException("Target PROJ string is empty");
    }

    initialize(from_proj, to_proj);
}

PROJTransform::~PROJTransform() {
    if (transform_) {
        proj_destroy(transform_);
        transform_ = nullptr;
    }
    // context_ unique_ptr will automatically destroy
}

PROJTransform::PROJTransform(PROJTransform&& other) noexcept
    : transform_(other.transform_),
      context_(std::move(other.context_)),
      from_srid_(other.from_srid_),
      to_srid_(other.to_srid_) {
    other.transform_ = nullptr;
    other.from_srid_ = 0;
    other.to_srid_ = 0;
}

PROJTransform& PROJTransform::operator=(PROJTransform&& other) noexcept {
    if (this != &other) {
        if (transform_) {
            proj_destroy(transform_);
        }
        transform_ = other.transform_;
        context_ = std::move(other.context_);
        from_srid_ = other.from_srid_;
        to_srid_ = other.to_srid_;

        other.transform_ = nullptr;
        other.from_srid_ = 0;
        other.to_srid_ = 0;
    }
    return *this;
}

void PROJTransform::initialize(const std::string& from_proj, const std::string& to_proj) {
    // Create context
    context_ = std::make_unique<PROJContext>();

    // Create transformation from source to target CRS
    // Use EPSG format if it looks like an EPSG code
    std::string from_crs = from_proj;
    std::string to_crs = to_proj;

    // Check if we need to prepend "EPSG:"
    if (from_proj.find("EPSG:") == std::string::npos &&
        from_proj.find("+proj") == std::string::npos) {
        // Assume it's an EPSG code number
        from_crs = "EPSG:" + from_proj;
    }
    if (to_proj.find("EPSG:") == std::string::npos &&
        to_proj.find("+proj") == std::string::npos) {
        to_crs = "EPSG:" + to_proj;
    }

    // Create the transformation
    transform_ = proj_create_crs_to_crs(
        context_->context(),
        from_crs.c_str(),
        to_crs.c_str(),
        nullptr  // Use default area
    );

    if (!transform_) {
        std::string error = context_->getLastError();
        throw PROJException("Failed to create transformation: " + error);
    }

    // Normalize for visualization (important for correct axis order)
    PJ* normalized = proj_normalize_for_visualization(context_->context(), transform_);
    if (!normalized) {
        proj_destroy(transform_);
        transform_ = nullptr;
        throw PROJException("Failed to normalize transformation");
    }

    // Replace transform with normalized version
    proj_destroy(transform_);
    transform_ = normalized;
}

void PROJTransform::transform(double& x, double& y) const {
    if (!transform_) {
        throw PROJException("Transformation is not initialized");
    }

    // Create coordinate
    PJ_COORD coord;
    coord.xy.x = x;
    coord.xy.y = y;

    // Transform
    PJ_COORD result = proj_trans(transform_, PJ_FWD, coord);

    // Check for error
    if (result.xy.x == HUGE_VAL || result.xy.y == HUGE_VAL) {
        std::string error = context_->getLastError();
        std::ostringstream oss;
        oss << "Transformation failed for point (" << x << ", " << y << ")";
        if (!error.empty()) {
            oss << ": " << error;
        }
        throw PROJException(oss.str());
    }

    // Update output
    x = result.xy.x;
    y = result.xy.y;
}

void PROJTransform::transform(double& x, double& y, double& z) const {
    if (!transform_) {
        throw PROJException("Transformation is not initialized");
    }

    // Create coordinate
    PJ_COORD coord;
    coord.xyz.x = x;
    coord.xyz.y = y;
    coord.xyz.z = z;

    // Transform
    PJ_COORD result = proj_trans(transform_, PJ_FWD, coord);

    // Check for error
    if (result.xyz.x == HUGE_VAL || result.xyz.y == HUGE_VAL) {
        std::string error = context_->getLastError();
        std::ostringstream oss;
        oss << "Transformation failed for point (" << x << ", " << y << ", " << z << ")";
        if (!error.empty()) {
            oss << ": " << error;
        }
        throw PROJException(oss.str());
    }

    // Update output
    x = result.xyz.x;
    y = result.xyz.y;
    z = result.xyz.z;
}

void PROJTransform::transformBatch(std::vector<double>& xs, std::vector<double>& ys) const {
    if (!transform_) {
        throw PROJException("Transformation is not initialized");
    }

    if (xs.size() != ys.size()) {
        throw PROJException("X and Y coordinate vectors must have the same size");
    }

    if (xs.empty()) {
        return;  // Nothing to transform
    }

    // Transform each point
    for (size_t i = 0; i < xs.size(); ++i) {
        PJ_COORD coord;
        coord.xy.x = xs[i];
        coord.xy.y = ys[i];

        PJ_COORD result = proj_trans(transform_, PJ_FWD, coord);

        if (result.xy.x == HUGE_VAL || result.xy.y == HUGE_VAL) {
            std::ostringstream oss;
            oss << "Transformation failed for point " << i << " ("
                << xs[i] << ", " << ys[i] << ")";
            throw PROJException(oss.str());
        }

        xs[i] = result.xy.x;
        ys[i] = result.xy.y;
    }
}

void PROJTransform::transformBatch(std::vector<double>& xs, std::vector<double>& ys,
                                   std::vector<double>& zs) const {
    if (!transform_) {
        throw PROJException("Transformation is not initialized");
    }

    if (xs.size() != ys.size() || xs.size() != zs.size()) {
        throw PROJException("X, Y, and Z coordinate vectors must have the same size");
    }

    if (xs.empty()) {
        return;  // Nothing to transform
    }

    // Transform each point
    for (size_t i = 0; i < xs.size(); ++i) {
        PJ_COORD coord;
        coord.xyz.x = xs[i];
        coord.xyz.y = ys[i];
        coord.xyz.z = zs[i];

        PJ_COORD result = proj_trans(transform_, PJ_FWD, coord);

        if (result.xyz.x == HUGE_VAL || result.xyz.y == HUGE_VAL) {
            std::ostringstream oss;
            oss << "Transformation failed for point " << i << " ("
                << xs[i] << ", " << ys[i] << ", " << zs[i] << ")";
            throw PROJException(oss.str());
        }

        xs[i] = result.xyz.x;
        ys[i] = result.xyz.y;
        zs[i] = result.xyz.z;
    }
}

std::string PROJTransform::getLastError() const {
    if (!context_) {
        return "Context is null";
    }
    return context_->getLastError();
}

#endif // HAVE_PROJ

} // namespace geo
} // namespace scratchbird

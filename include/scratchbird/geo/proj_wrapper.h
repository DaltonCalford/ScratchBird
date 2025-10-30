#ifndef SCRATCHBIRD_GEO_PROJ_WRAPPER_H
#define SCRATCHBIRD_GEO_PROJ_WRAPPER_H

#include "scratchbird/geo/srid.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>

#ifdef HAVE_PROJ
#include <proj.h>
#endif

namespace scratchbird {
namespace geo {

/**
 * @brief Exception thrown for PROJ errors
 */
class PROJException : public std::runtime_error {
public:
    explicit PROJException(const std::string& message)
        : std::runtime_error(message) {}
};

#ifdef HAVE_PROJ

/**
 * @brief RAII wrapper for PROJ context (PJ_CONTEXT*)
 *
 * Thread-safe: Each instance creates its own context.
 * Contexts should NOT be shared between threads.
 *
 * Usage:
 *   PROJContext ctx;
 *   PJ* crs = proj_create(ctx.context(), "EPSG:4326");
 */
class PROJContext {
public:
    /**
     * @brief Create a new PROJ context
     * @throws PROJException if context creation fails
     */
    PROJContext();

    /**
     * @brief Destroy PROJ context
     */
    ~PROJContext();

    // Prevent copying
    PROJContext(const PROJContext&) = delete;
    PROJContext& operator=(const PROJContext&) = delete;

    // Allow moving
    PROJContext(PROJContext&& other) noexcept;
    PROJContext& operator=(PROJContext&& other) noexcept;

    /**
     * @brief Get raw PROJ context pointer
     * @return PJ_CONTEXT pointer (never null)
     */
    PJ_CONTEXT* context() const { return ctx_; }

    /**
     * @brief Check if context is valid
     * @return true if context is valid
     */
    bool isValid() const { return ctx_ != nullptr; }

    /**
     * @brief Get last error message from context
     * @return Error message or empty string
     */
    std::string getLastError() const;

private:
    PJ_CONTEXT* ctx_;
};

/**
 * @brief RAII wrapper for PROJ transformation (PJ*)
 *
 * Thread-safe: Each instance owns its context and transformation.
 * Instances should NOT be shared between threads.
 *
 * Usage:
 *   PROJTransform transform(SRID::WGS84, SRID::WebMercator);
 *   double x = -122.4194, y = 37.7749;  // San Francisco
 *   transform.transform(x, y);
 *   // x, y now in Web Mercator coordinates
 */
class PROJTransform {
public:
    /**
     * @brief Create transformation between two SRIDs
     * @param from Source SRID
     * @param to Target SRID
     * @throws PROJException if transformation cannot be created
     */
    PROJTransform(const SRID& from, const SRID& to);

    /**
     * @brief Create transformation from EPSG codes
     * @param from_epsg Source EPSG code
     * @param to_epsg Target EPSG code
     * @throws PROJException if transformation cannot be created
     */
    PROJTransform(int from_epsg, int to_epsg);

    /**
     * @brief Create transformation from PROJ strings
     * @param from_proj Source PROJ string
     * @param to_proj Target PROJ string
     * @throws PROJException if transformation cannot be created
     */
    PROJTransform(const std::string& from_proj, const std::string& to_proj);

    /**
     * @brief Destroy transformation
     */
    ~PROJTransform();

    // Prevent copying
    PROJTransform(const PROJTransform&) = delete;
    PROJTransform& operator=(const PROJTransform&) = delete;

    // Allow moving
    PROJTransform(PROJTransform&& other) noexcept;
    PROJTransform& operator=(PROJTransform&& other) noexcept;

    /**
     * @brief Transform a single point (in-place)
     * @param x X coordinate (longitude for geographic)
     * @param y Y coordinate (latitude for geographic)
     * @throws PROJException if transformation fails
     *
     * Note: For geographic coordinates, x=longitude, y=latitude
     */
    void transform(double& x, double& y) const;

    /**
     * @brief Transform a single point with Z coordinate
     * @param x X coordinate
     * @param y Y coordinate
     * @param z Z coordinate (elevation)
     * @throws PROJException if transformation fails
     */
    void transform(double& x, double& y, double& z) const;

    /**
     * @brief Transform multiple points (in-place)
     * @param xs Vector of X coordinates
     * @param ys Vector of Y coordinates
     * @throws PROJException if transformation fails or size mismatch
     *
     * Requires: xs.size() == ys.size()
     */
    void transformBatch(std::vector<double>& xs, std::vector<double>& ys) const;

    /**
     * @brief Transform multiple points with Z coordinates
     * @param xs Vector of X coordinates
     * @param ys Vector of Y coordinates
     * @param zs Vector of Z coordinates
     * @throws PROJException if transformation fails or size mismatch
     *
     * Requires: xs.size() == ys.size() == zs.size()
     */
    void transformBatch(std::vector<double>& xs, std::vector<double>& ys,
                        std::vector<double>& zs) const;

    /**
     * @brief Get source SRID
     * @return Source SRID ID
     */
    int getSourceSRID() const { return from_srid_; }

    /**
     * @brief Get target SRID
     * @return Target SRID ID
     */
    int getTargetSRID() const { return to_srid_; }

    /**
     * @brief Check if transformation is valid
     * @return true if transformation is valid
     */
    bool isValid() const { return transform_ != nullptr; }

    /**
     * @brief Get last error message
     * @return Error message or empty string
     */
    std::string getLastError() const;

private:
    /**
     * @brief Initialize transformation
     */
    void initialize(const std::string& from_proj, const std::string& to_proj);

    PJ* transform_;
    std::unique_ptr<PROJContext> context_;
    int from_srid_;
    int to_srid_;
};

#else // !HAVE_PROJ

/**
 * @brief Stub implementation when PROJ is not available
 */
class PROJContext {
public:
    PROJContext() {
        throw PROJException("PROJ library not available. Install libproj-dev to enable.");
    }
    ~PROJContext() = default;

    PROJContext(const PROJContext&) = delete;
    PROJContext& operator=(const PROJContext&) = delete;
    PROJContext(PROJContext&&) noexcept = default;
    PROJContext& operator=(PROJContext&&) noexcept = default;

    void* context() const { return nullptr; }
    bool isValid() const { return false; }
    std::string getLastError() const { return "PROJ not available"; }
};

/**
 * @brief Stub implementation when PROJ is not available
 */
class PROJTransform {
public:
    PROJTransform(const SRID&, const SRID&) {
        throw PROJException("PROJ library not available. Install libproj-dev to enable.");
    }
    PROJTransform(int, int) {
        throw PROJException("PROJ library not available. Install libproj-dev to enable.");
    }
    PROJTransform(const std::string&, const std::string&) {
        throw PROJException("PROJ library not available. Install libproj-dev to enable.");
    }
    ~PROJTransform() = default;

    PROJTransform(const PROJTransform&) = delete;
    PROJTransform& operator=(const PROJTransform&) = delete;
    PROJTransform(PROJTransform&&) noexcept = default;
    PROJTransform& operator=(PROJTransform&&) noexcept = default;

    void transform(double&, double&) const {
        throw PROJException("PROJ not available");
    }
    void transform(double&, double&, double&) const {
        throw PROJException("PROJ not available");
    }
    void transformBatch(std::vector<double>&, std::vector<double>&) const {
        throw PROJException("PROJ not available");
    }
    void transformBatch(std::vector<double>&, std::vector<double>&,
                        std::vector<double>&) const {
        throw PROJException("PROJ not available");
    }

    int getSourceSRID() const { return 0; }
    int getTargetSRID() const { return 0; }
    bool isValid() const { return false; }
    std::string getLastError() const { return "PROJ not available"; }
};

#endif // HAVE_PROJ

} // namespace geo
} // namespace scratchbird

#endif // SCRATCHBIRD_GEO_PROJ_WRAPPER_H

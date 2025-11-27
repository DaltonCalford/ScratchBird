#include "scratchbird/spatial/geos_wrapper.h"
#include "scratchbird/core/typed_value.h"
#include <stdexcept>
#include <sstream>

namespace scratchbird::spatial
{

#ifdef HAVE_GEOS

// ============================================================================
// GEOSContext Implementation
// ============================================================================

void GEOSContext::errorHandler(const char* message, void* userdata)
{
    auto* ctx = static_cast<GEOSContext*>(userdata);
    if (ctx) {
        ctx->last_error_ = message ? message : "Unknown GEOS error";
    }
}

void GEOSContext::warningHandler(const char* message, void* /* userdata */)
{
    // For now, we ignore warnings. Could be logged if needed.
    (void)message;
}

GEOSContext::GEOSContext()
    : ctx_(nullptr)
{
    ctx_ = GEOS_init_r();
    if (!ctx_) {
        throw std::runtime_error("Failed to initialize GEOS context");
    }

    // Set error and warning handlers
    GEOSContext_setErrorMessageHandler_r(ctx_, errorHandler, this);
    GEOSContext_setNoticeMessageHandler_r(ctx_, warningHandler, this);
}

GEOSContext::~GEOSContext()
{
    if (ctx_) {
        GEOS_finish_r(ctx_);
        ctx_ = nullptr;
    }
}

GEOSContext::GEOSContext(GEOSContext&& other) noexcept
    : ctx_(other.ctx_), last_error_(std::move(other.last_error_))
{
    other.ctx_ = nullptr;
}

GEOSContext& GEOSContext::operator=(GEOSContext&& other) noexcept
{
    if (this != &other) {
        if (ctx_) {
            GEOS_finish_r(ctx_);
        }
        ctx_ = other.ctx_;
        last_error_ = std::move(other.last_error_);
        other.ctx_ = nullptr;
    }
    return *this;
}

std::string GEOSContext::getLastError() const
{
    return last_error_;
}

// ============================================================================
// GEOSGeomPtr Implementation
// ============================================================================

GEOSGeomPtr::~GEOSGeomPtr()
{
    if (geom_ && ctx_) {
        GEOSGeom_destroy_r(ctx_, geom_);
    }
}

GEOSGeomPtr::GEOSGeomPtr(GEOSGeomPtr&& other) noexcept
    : geom_(other.geom_), ctx_(other.ctx_)
{
    other.geom_ = nullptr;
    other.ctx_ = nullptr;
}

GEOSGeomPtr& GEOSGeomPtr::operator=(GEOSGeomPtr&& other) noexcept
{
    if (this != &other) {
        if (geom_ && ctx_) {
            GEOSGeom_destroy_r(ctx_, geom_);
        }
        geom_ = other.geom_;
        ctx_ = other.ctx_;
        other.geom_ = nullptr;
        other.ctx_ = nullptr;
    }
    return *this;
}

GEOSGeometry* GEOSGeomPtr::release()
{
    GEOSGeometry* temp = geom_;
    geom_ = nullptr;
    return temp;
}

// ============================================================================
// GEOSCoordSeqPtr Implementation
// ============================================================================

GEOSCoordSeqPtr::~GEOSCoordSeqPtr()
{
    if (seq_ && ctx_) {
        GEOSCoordSeq_destroy_r(ctx_, seq_);
    }
}

GEOSCoordSeqPtr::GEOSCoordSeqPtr(GEOSCoordSeqPtr&& other) noexcept
    : seq_(other.seq_), ctx_(other.ctx_)
{
    other.seq_ = nullptr;
    other.ctx_ = nullptr;
}

GEOSCoordSeqPtr& GEOSCoordSeqPtr::operator=(GEOSCoordSeqPtr&& other) noexcept
{
    if (this != &other) {
        if (seq_ && ctx_) {
            GEOSCoordSeq_destroy_r(ctx_, seq_);
        }
        seq_ = other.seq_;
        ctx_ = other.ctx_;
        other.seq_ = nullptr;
        other.ctx_ = nullptr;
    }
    return *this;
}

GEOSCoordSequence* GEOSCoordSeqPtr::release()
{
    GEOSCoordSequence* temp = seq_;
    seq_ = nullptr;
    return temp;
}

// ============================================================================
// Conversion Functions: ScratchBird -> GEOS
// ============================================================================

GEOSGeomPtr pointToGEOS(const core::Point& point, GEOSContext& ctx,
                        core::ErrorContext* error_ctx)
{
    // Create coordinate sequence for a single point (1 coordinate, 2 dimensions)
    GEOSCoordSequence* seq = GEOSCoordSeq_create_r(ctx.handle(), 1, 2);
    if (!seq) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR;
            error_ctx->message = "Failed to create GEOS coordinate sequence for Point";
        }
        return GEOSGeomPtr(nullptr, ctx.handle());
    }

    // Set X and Y coordinates
    if (GEOSCoordSeq_setX_r(ctx.handle(), seq, 0, point.x) == 0 ||
        GEOSCoordSeq_setY_r(ctx.handle(), seq, 0, point.y) == 0) {
        GEOSCoordSeq_destroy_r(ctx.handle(), seq);
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR;
            error_ctx->message = "Failed to set coordinates for GEOS Point";
        }
        return GEOSGeomPtr(nullptr, ctx.handle());
    }

    // Create point geometry from coordinate sequence
    GEOSGeometry* geom = GEOSGeom_createPoint_r(ctx.handle(), seq);
    if (!geom) {
        GEOSCoordSeq_destroy_r(ctx.handle(), seq);
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR;
            error_ctx->message = "Failed to create GEOS Point geometry";
        }
        return GEOSGeomPtr(nullptr, ctx.handle());
    }

    return GEOSGeomPtr(geom, ctx.handle());
}

GEOSGeomPtr lineStringToGEOS(const core::LineString& line, GEOSContext& ctx,
                              core::ErrorContext* error_ctx)
{
    if (line.points.size() < 2) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "LineString must have at least 2 points";
        }
        return GEOSGeomPtr(nullptr, ctx.handle());
    }

    // Create coordinate sequence
    GEOSCoordSequence* seq = GEOSCoordSeq_create_r(ctx.handle(),
                                                    static_cast<unsigned int>(line.points.size()), 2);
    if (!seq) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to create GEOS coordinate sequence for LineString";
        }
        return GEOSGeomPtr(nullptr, ctx.handle());
    }

    // Set coordinates for each point
    for (size_t i = 0; i < line.points.size(); ++i) {
        if (GEOSCoordSeq_setX_r(ctx.handle(), seq, static_cast<unsigned int>(i), line.points[i].x) == 0 ||
            GEOSCoordSeq_setY_r(ctx.handle(), seq, static_cast<unsigned int>(i), line.points[i].y) == 0) {
            GEOSCoordSeq_destroy_r(ctx.handle(), seq);
            if (error_ctx) {
                error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to set coordinates for GEOS LineString";
            }
            return GEOSGeomPtr(nullptr, ctx.handle());
        }
    }

    // Create linestring geometry
    GEOSGeometry* geom = GEOSGeom_createLineString_r(ctx.handle(), seq);
    if (!geom) {
        GEOSCoordSeq_destroy_r(ctx.handle(), seq);
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to create GEOS LineString geometry";
        }
        return GEOSGeomPtr(nullptr, ctx.handle());
    }

    return GEOSGeomPtr(geom, ctx.handle());
}

GEOSGeomPtr polygonToGEOS(const core::Polygon& polygon, GEOSContext& ctx,
                          core::ErrorContext* error_ctx)
{
    if (polygon.rings.empty() || polygon.rings[0].size() < 4) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Polygon exterior ring must have at least 4 points";
        }
        return GEOSGeomPtr(nullptr, ctx.handle());
    }

    // Helper lambda to create a linear ring from points
    auto createRing = [&](const std::vector<core::Point>& points) -> GEOSGeometry* {
        GEOSCoordSequence* seq = GEOSCoordSeq_create_r(ctx.handle(),
                                                        static_cast<unsigned int>(points.size()), 2);
        if (!seq) return nullptr;

        for (size_t i = 0; i < points.size(); ++i) {
            if (GEOSCoordSeq_setX_r(ctx.handle(), seq, static_cast<unsigned int>(i), points[i].x) == 0 ||
                GEOSCoordSeq_setY_r(ctx.handle(), seq, static_cast<unsigned int>(i), points[i].y) == 0) {
                GEOSCoordSeq_destroy_r(ctx.handle(), seq);
                return nullptr;
            }
        }

        return GEOSGeom_createLinearRing_r(ctx.handle(), seq);
    };

    // Create exterior ring
    GEOSGeometry* shell = createRing(polygon.rings[0]);
    if (!shell) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to create GEOS exterior ring for Polygon";
        }
        return GEOSGeomPtr(nullptr, ctx.handle());
    }

    // Create interior rings (holes)
    GEOSGeometry** holes = nullptr;
    unsigned int num_holes = 0;

    if (polygon.rings.size() > 1) {
        num_holes = static_cast<unsigned int>(polygon.rings.size() - 1);
        holes = new GEOSGeometry*[num_holes];

        for (size_t i = 1; i < polygon.rings.size(); ++i) {
            holes[i - 1] = createRing(polygon.rings[i]);
            if (!holes[i - 1]) {
                // Cleanup on error
                for (size_t j = 0; j < i - 1; ++j) {
                    GEOSGeom_destroy_r(ctx.handle(), holes[j]);
                }
                delete[] holes;
                GEOSGeom_destroy_r(ctx.handle(), shell);
                if (error_ctx) {
                    error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to create GEOS interior ring for Polygon";
                }
                return GEOSGeomPtr(nullptr, ctx.handle());
            }
        }
    }

    // Create polygon geometry
    GEOSGeometry* geom = GEOSGeom_createPolygon_r(ctx.handle(), shell, holes, num_holes);

    // Note: GEOSGeom_createPolygon_r takes ownership of shell and holes
    // We only need to delete the holes array
    if (holes) {
        delete[] holes;
    }

    if (!geom) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to create GEOS Polygon geometry";
        }
        return GEOSGeomPtr(nullptr, ctx.handle());
    }

    return GEOSGeomPtr(geom, ctx.handle());
}

// ============================================================================
// Conversion Functions: GEOS -> ScratchBird
// ============================================================================

std::optional<core::Point> geosToPoint(const GEOSGeometry* geom, GEOSContext& ctx,
                                       core::ErrorContext* error_ctx)
{
    if (!geom) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Null GEOS geometry";
        }
        return std::nullopt;
    }

    int type = GEOSGeomTypeId_r(ctx.handle(), geom);
    if (type != GEOS_POINT) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "GEOS geometry is not a Point";
        }
        return std::nullopt;
    }

    const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq_r(ctx.handle(), geom);
    if (!seq) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to get coordinate sequence from GEOS Point";
        }
        return std::nullopt;
    }

    double x, y;
    if (GEOSCoordSeq_getX_r(ctx.handle(), seq, 0, &x) == 0 ||
        GEOSCoordSeq_getY_r(ctx.handle(), seq, 0, &y) == 0) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to read coordinates from GEOS Point";
        }
        return std::nullopt;
    }

    return core::Point(x, y);
}

std::optional<core::LineString> geosToLineString(const GEOSGeometry* geom, GEOSContext& ctx,
                                                  core::ErrorContext* error_ctx)
{
    if (!geom) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Null GEOS geometry";
        }
        return std::nullopt;
    }

    int type = GEOSGeomTypeId_r(ctx.handle(), geom);
    if (type != GEOS_LINESTRING) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "GEOS geometry is not a LineString";
        }
        return std::nullopt;
    }

    const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq_r(ctx.handle(), geom);
    if (!seq) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to get coordinate sequence from GEOS LineString";
        }
        return std::nullopt;
    }

    unsigned int num_points;
    if (GEOSCoordSeq_getSize_r(ctx.handle(), seq, &num_points) == 0) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to get size of GEOS coordinate sequence";
        }
        return std::nullopt;
    }

    std::vector<core::Point> points;
    points.reserve(num_points);

    for (unsigned int i = 0; i < num_points; ++i) {
        double x, y;
        if (GEOSCoordSeq_getX_r(ctx.handle(), seq, i, &x) == 0 ||
            GEOSCoordSeq_getY_r(ctx.handle(), seq, i, &y) == 0) {
            if (error_ctx) {
                error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to read coordinates from GEOS LineString";
            }
            return std::nullopt;
        }
        points.emplace_back(x, y);
    }

    return core::LineString(std::move(points));
}

std::optional<core::Polygon> geosToPolygon(const GEOSGeometry* geom, GEOSContext& ctx,
                                            core::ErrorContext* error_ctx)
{
    if (!geom) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Null GEOS geometry";
        }
        return std::nullopt;
    }

    int type = GEOSGeomTypeId_r(ctx.handle(), geom);
    if (type != GEOS_POLYGON) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "GEOS geometry is not a Polygon";
        }
        return std::nullopt;
    }

    // Helper lambda to extract points from a ring
    auto extractRing = [&](const GEOSGeometry* ring) -> std::optional<std::vector<core::Point>> {
        const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq_r(ctx.handle(), ring);
        if (!seq) return std::nullopt;

        unsigned int num_points;
        if (GEOSCoordSeq_getSize_r(ctx.handle(), seq, &num_points) == 0) {
            return std::nullopt;
        }

        std::vector<core::Point> points;
        points.reserve(num_points);

        for (unsigned int i = 0; i < num_points; ++i) {
            double x, y;
            if (GEOSCoordSeq_getX_r(ctx.handle(), seq, i, &x) == 0 ||
                GEOSCoordSeq_getY_r(ctx.handle(), seq, i, &y) == 0) {
                return std::nullopt;
            }
            points.emplace_back(x, y);
        }

        return points;
    };

    std::vector<std::vector<core::Point>> rings;

    // Get exterior ring
    const GEOSGeometry* exterior = GEOSGetExteriorRing_r(ctx.handle(), geom);
    if (!exterior) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to get exterior ring from GEOS Polygon";
        }
        return std::nullopt;
    }

    auto ext_points = extractRing(exterior);
    if (!ext_points) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to extract points from exterior ring";
        }
        return std::nullopt;
    }
    rings.push_back(std::move(*ext_points));

    // Get interior rings (holes)
    int num_holes = GEOSGetNumInteriorRings_r(ctx.handle(), geom);
    if (num_holes < 0) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to get number of interior rings from GEOS Polygon";
        }
        return std::nullopt;
    }

    for (int i = 0; i < num_holes; ++i) {
        const GEOSGeometry* hole = GEOSGetInteriorRingN_r(ctx.handle(), geom, i);
        if (!hole) {
            if (error_ctx) {
                error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to get interior ring from GEOS Polygon";
            }
            return std::nullopt;
        }

        auto hole_points = extractRing(hole);
        if (!hole_points) {
            if (error_ctx) {
                error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Failed to extract points from interior ring";
            }
            return std::nullopt;
        }
        rings.push_back(std::move(*hole_points));
    }

    return core::Polygon(std::move(rings));
}

std::optional<core::TypedValue> geosToTypedValue(const GEOSGeometry* geom, GEOSContext& ctx,
                                                  core::ErrorContext* error_ctx)
{
    if (!geom) {
        if (error_ctx) {
            error_ctx->code = core::Status::IO_ERROR; error_ctx->message = "Null GEOS geometry";
        }
        return std::nullopt;
    }

    int type = GEOSGeomTypeId_r(ctx.handle(), geom);

    switch (type) {
        case GEOS_POINT: {
            auto point = geosToPoint(geom, ctx, error_ctx);
            if (!point) return std::nullopt;
            return core::TypedValue::makePoint(*point);
        }

        case GEOS_LINESTRING: {
            auto line = geosToLineString(geom, ctx, error_ctx);
            if (!line) return std::nullopt;
            return core::TypedValue::makeLineString(*line);
        }

        case GEOS_POLYGON: {
            auto polygon = geosToPolygon(geom, ctx, error_ctx);
            if (!polygon) return std::nullopt;
            return core::TypedValue::makePolygon(*polygon);
        }

        default:
            if (error_ctx) {
                std::ostringstream oss;
                oss << "Unsupported GEOS geometry type: " << type;
                error_ctx->code = core::Status::IO_ERROR; error_ctx->message = oss.str();
            }
            return std::nullopt;
    }
}

#else

// Stub implementation when GEOS is not available
GEOSContext::GEOSContext()
{
    throw std::runtime_error("ScratchBird was not compiled with GEOS support. "
                           "Install libgeos-dev and recompile.");
}

#endif // HAVE_GEOS

} // namespace scratchbird::spatial

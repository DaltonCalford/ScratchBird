#include "scratchbird/geo/srid.h"

#ifdef HAVE_PROJ
#include <proj.h>
#endif

#include <cstring>

namespace scratchbird {
namespace geo {

// ============================================================================
// SRID Class Implementation
// ============================================================================

SRID::SRID(int id) : id_(id) {}

bool SRID::isValid() const {
    if (id_ <= 0) return false;
    return SRIDRegistry::instance().isValid(id_);
}

std::string SRID::name() const {
    return SRIDRegistry::instance().getName(id_);
}

std::string SRID::proj4() const {
    return SRIDRegistry::instance().getProj4(id_);
}

bool SRID::isGeographic() const {
    return SRIDRegistry::instance().isGeographic(id_);
}

bool SRID::isProjected() const {
    return SRIDRegistry::instance().isProjected(id_);
}

std::string SRID::getUnits() const {
    return SRIDRegistry::instance().getUnits(id_);
}

std::string SRID::getAuthority() const {
    SRIDMetadata metadata;
    if (SRIDRegistry::instance().getMetadata(id_, metadata)) {
        return metadata.authority;
    }
    return "";
}

int SRID::getAuthorityCode() const {
    SRIDMetadata metadata;
    if (SRIDRegistry::instance().getMetadata(id_, metadata)) {
        return metadata.authority_code;
    }
    return 0;
}

// Common SRID constants
const SRID SRID::WGS84(4326);
const SRID SRID::WebMercator(3857);
const SRID SRID::NAD83(4269);
const SRID SRID::WGS84_UTM_N(32600);
const SRID SRID::ETRS89(4258);

// ============================================================================
// SRIDRegistry Class Implementation
// ============================================================================

SRIDRegistry::SRIDRegistry() {
    loadCommonSRIDs();
}

SRIDRegistry& SRIDRegistry::instance() {
    static SRIDRegistry registry;
    return registry;
}

bool SRIDRegistry::isValid(int srid_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return srids_.find(srid_id) != srids_.end();
}

bool SRIDRegistry::getMetadata(int srid_id, SRIDMetadata& metadata) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = srids_.find(srid_id);
    if (it != srids_.end()) {
        metadata = it->second;
        return true;
    }
    return false;
}

std::string SRIDRegistry::getName(int srid_id) const {
    SRIDMetadata metadata;
    if (getMetadata(srid_id, metadata)) {
        return metadata.name;
    }
    return "";
}

std::string SRIDRegistry::getProj4(int srid_id) const {
    SRIDMetadata metadata;
    if (getMetadata(srid_id, metadata)) {
        return metadata.proj4_string;
    }
    return "";
}

bool SRIDRegistry::isGeographic(int srid_id) const {
    SRIDMetadata metadata;
    if (getMetadata(srid_id, metadata)) {
        return metadata.is_geographic;
    }
    return false;
}

bool SRIDRegistry::isProjected(int srid_id) const {
    SRIDMetadata metadata;
    if (getMetadata(srid_id, metadata)) {
        return !metadata.is_geographic;
    }
    return false;
}

std::string SRIDRegistry::getUnits(int srid_id) const {
    SRIDMetadata metadata;
    if (getMetadata(srid_id, metadata)) {
        return metadata.units;
    }
    return "";
}

bool SRIDRegistry::registerSRID(const SRIDMetadata& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);
    srids_[metadata.srid_id] = metadata;
    return true;
}

size_t SRIDRegistry::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return srids_.size();
}

#ifdef HAVE_PROJ

bool SRIDRegistry::loadFromPROJ(int epsg_code) {
    // Create a temporary PROJ context
    PJ_CONTEXT* ctx = proj_context_create();
    if (!ctx) {
        return false;
    }

    // Construct EPSG code string
    char epsg_str[64];
    snprintf(epsg_str, sizeof(epsg_str), "EPSG:%d", epsg_code);

    // Create CRS from EPSG code
    PJ* crs = proj_create(ctx, epsg_str);
    if (!crs) {
        proj_context_destroy(ctx);
        return false;
    }

    // Extract metadata
    SRIDMetadata metadata;
    metadata.srid_id = epsg_code;
    metadata.authority = "EPSG";
    metadata.authority_code = epsg_code;

    // Get CRS name
    const char* name = proj_get_name(crs);
    if (name) {
        metadata.name = name;
    }

    // Get PROJ string
    const char* proj_str = proj_as_proj_string(ctx, crs, PJ_PROJ_4, nullptr);
    if (proj_str) {
        metadata.proj4_string = proj_str;
    }

    // Determine if geographic or projected
    PJ_TYPE crs_type = proj_get_type(crs);
    metadata.is_geographic = (crs_type == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
                              crs_type == PJ_TYPE_GEOGRAPHIC_3D_CRS);

    // Get units
    if (metadata.is_geographic) {
        metadata.units = "degrees";
    } else {
        // For projected CRS, default to meters
        // Phase 4 Enhancement: Extract actual units from PROJ coordinate system once API is available
        metadata.units = "meters";  // Default is safe for most projected CRS
    }

    // Clean up
    proj_destroy(crs);
    proj_context_destroy(ctx);

    // Register the SRID
    std::lock_guard<std::mutex> lock(mutex_);
    srids_[metadata.srid_id] = metadata;

    return true;
}

#else

bool SRIDRegistry::loadFromPROJ(int epsg_code) {
    // PROJ not available, use fallback hardcoded definitions
    // This will only work for the most common SRIDs
    return false;
}

#endif // HAVE_PROJ

void SRIDRegistry::loadCommonSRIDs() {
    // Try to load from PROJ first
#ifdef HAVE_PROJ
    // WGS 84 (GPS coordinates)
    loadFromPROJ(4326);

    // Web Mercator (Google Maps, OpenStreetMap)
    loadFromPROJ(3857);

    // NAD83 (North American Datum 1983)
    loadFromPROJ(4269);

    // ETRS89 (European Terrestrial Reference System)
    loadFromPROJ(4258);

    // WGS 84 / UTM zones (sample: zone 31N)
    loadFromPROJ(32631);

    // Additional common projections
    loadFromPROJ(3395);  // WGS 84 / World Mercator
    loadFromPROJ(4267);  // NAD27 (older North American datum)
    loadFromPROJ(2154);  // RGF93 / Lambert-93 (France)
    loadFromPROJ(27700); // OSGB 1936 / British National Grid
    loadFromPROJ(3857);  // Web Mercator (pseudo-Mercator)
#else
    // Fallback: hardcoded definitions for common SRIDs
    // WGS 84
    {
        SRIDMetadata metadata;
        metadata.srid_id = 4326;
        metadata.name = "WGS 84";
        metadata.proj4_string = "+proj=longlat +datum=WGS84 +no_defs";
        metadata.authority = "EPSG";
        metadata.authority_code = 4326;
        metadata.is_geographic = true;
        metadata.units = "degrees";
        srids_[4326] = metadata;
    }

    // Web Mercator
    {
        SRIDMetadata metadata;
        metadata.srid_id = 3857;
        metadata.name = "WGS 84 / Pseudo-Mercator";
        metadata.proj4_string = "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 "
                                "+lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m "
                                "+nadgrids=@null +wktext +no_defs";
        metadata.authority = "EPSG";
        metadata.authority_code = 3857;
        metadata.is_geographic = false;
        metadata.units = "meters";
        srids_[3857] = metadata;
    }

    // NAD83
    {
        SRIDMetadata metadata;
        metadata.srid_id = 4269;
        metadata.name = "NAD83";
        metadata.proj4_string = "+proj=longlat +ellps=GRS80 +datum=NAD83 +no_defs";
        metadata.authority = "EPSG";
        metadata.authority_code = 4269;
        metadata.is_geographic = true;
        metadata.units = "degrees";
        srids_[4269] = metadata;
    }

    // ETRS89
    {
        SRIDMetadata metadata;
        metadata.srid_id = 4258;
        metadata.name = "ETRS89";
        metadata.proj4_string = "+proj=longlat +ellps=GRS80 +no_defs";
        metadata.authority = "EPSG";
        metadata.authority_code = 4258;
        metadata.is_geographic = true;
        metadata.units = "degrees";
        srids_[4258] = metadata;
    }
#endif
}

} // namespace geo
} // namespace scratchbird

/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#ifndef SCRATCHBIRD_GEO_SRID_H
#define SCRATCHBIRD_GEO_SRID_H

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace scratchbird {
namespace geo {

/**
 * @brief Metadata about a spatial reference system
 */
struct SRIDMetadata {
    int srid_id;
    std::string name;
    std::string proj4_string;
    std::string authority;  // e.g., "EPSG"
    int authority_code;     // e.g., 4326
    bool is_geographic;     // lat/lon (true) vs projected (false)
    std::string units;      // "degrees", "meters", etc.

    SRIDMetadata()
        : srid_id(0), authority_code(0), is_geographic(false) {}

    SRIDMetadata(int id, const std::string& n, const std::string& proj4,
                 const std::string& auth, int code, bool is_geo, const std::string& u)
        : srid_id(id), name(n), proj4_string(proj4), authority(auth),
          authority_code(code), is_geographic(is_geo), units(u) {}
};

/**
 * @brief Spatial Reference Identifier (SRID)
 *
 * Represents a coordinate system identified by an integer ID.
 * Common SRIDs include:
 * - 4326: WGS 84 (GPS coordinates)
 * - 3857: Web Mercator (web maps)
 * - 4269: NAD83 (North American Datum 1983)
 *
 * Thread-safe: Yes (immutable after construction)
 */
class SRID {
public:
    /**
     * @brief Construct SRID with ID
     * @param id SRID identifier (0 = undefined/no SRID)
     */
    explicit SRID(int id = 0);

    /**
     * @brief Get SRID identifier
     * @return SRID ID (0 if undefined)
     */
    int id() const { return id_; }

    /**
     * @brief Check if SRID is valid (non-zero and registered)
     * @return true if SRID > 0 and exists in registry
     */
    bool isValid() const;

    /**
     * @brief Get SRID name from registry
     * @return Name of coordinate system (e.g., "WGS 84")
     */
    std::string name() const;

    /**
     * @brief Get PROJ.4 definition string
     * @return PROJ.4 string for coordinate system
     */
    std::string proj4() const;

    /**
     * @brief Check if coordinate system is geographic (lat/lon)
     * @return true if geographic, false if projected
     */
    bool isGeographic() const;

    /**
     * @brief Check if coordinate system is projected (meters/feet)
     * @return true if projected, false if geographic
     */
    bool isProjected() const;

    /**
     * @brief Get units of coordinate system
     * @return "degrees", "meters", "feet", etc.
     */
    std::string getUnits() const;

    /**
     * @brief Get authority name (e.g., "EPSG")
     * @return Authority name or empty string
     */
    std::string getAuthority() const;

    /**
     * @brief Get authority code (e.g., 4326 for EPSG:4326)
     * @return Authority code or 0
     */
    int getAuthorityCode() const;

    // Comparison operators
    bool operator==(const SRID& other) const { return id_ == other.id_; }
    bool operator!=(const SRID& other) const { return id_ != other.id_; }
    bool operator<(const SRID& other) const { return id_ < other.id_; }

    // Common SRID constants
    static const SRID WGS84;          // 4326 - GPS coordinates
    static const SRID WebMercator;    // 3857 - Web maps
    static const SRID NAD83;          // 4269 - North American Datum 1983
    static const SRID WGS84_UTM_N;    // 32600 - WGS 84 UTM North base
    static const SRID ETRS89;         // 4258 - European Terrestrial Reference System 1989

private:
    int id_;
};

/**
 * @brief Registry for SRID metadata
 *
 * Singleton that manages SRID definitions loaded from PROJ database.
 * Thread-safe for concurrent access.
 */
class SRIDRegistry {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to global SRID registry
     */
    static SRIDRegistry& instance();

    /**
     * @brief Check if SRID is valid and registered
     * @param srid_id SRID identifier to check
     * @return true if SRID exists in registry
     */
    bool isValid(int srid_id) const;

    /**
     * @brief Get metadata for SRID
     * @param srid_id SRID identifier
     * @param metadata Output parameter for metadata
     * @return true if SRID found, false otherwise
     */
    bool getMetadata(int srid_id, SRIDMetadata& metadata) const;

    /**
     * @brief Get name of coordinate system
     * @param srid_id SRID identifier
     * @return Name or empty string if not found
     */
    std::string getName(int srid_id) const;

    /**
     * @brief Get PROJ.4 definition string
     * @param srid_id SRID identifier
     * @return PROJ.4 string or empty string if not found
     */
    std::string getProj4(int srid_id) const;

    /**
     * @brief Check if coordinate system is geographic
     * @param srid_id SRID identifier
     * @return true if geographic, false otherwise
     */
    bool isGeographic(int srid_id) const;

    /**
     * @brief Check if coordinate system is projected
     * @param srid_id SRID identifier
     * @return true if projected, false otherwise
     */
    bool isProjected(int srid_id) const;

    /**
     * @brief Get units of coordinate system
     * @param srid_id SRID identifier
     * @return Units string or empty string if not found
     */
    std::string getUnits(int srid_id) const;

    /**
     * @brief Register a custom SRID
     * @param metadata SRID metadata to register
     * @return true if registered successfully
     */
    bool registerSRID(const SRIDMetadata& metadata);

    /**
     * @brief Get number of registered SRIDs
     * @return Count of SRIDs in registry
     */
    size_t count() const;

private:
    SRIDRegistry();  // Private constructor for singleton
    ~SRIDRegistry() = default;

    // Prevent copying
    SRIDRegistry(const SRIDRegistry&) = delete;
    SRIDRegistry& operator=(const SRIDRegistry&) = delete;

    /**
     * @brief Load common SRID definitions from PROJ database
     */
    void loadCommonSRIDs();

    /**
     * @brief Load SRID from PROJ using EPSG code
     * @param epsg_code EPSG code (e.g., 4326)
     * @return true if loaded successfully
     */
    bool loadFromPROJ(int epsg_code);

    mutable std::mutex mutex_;
    std::unordered_map<int, SRIDMetadata> srids_;
};

} // namespace geo
} // namespace scratchbird

#endif // SCRATCHBIRD_GEO_SRID_H

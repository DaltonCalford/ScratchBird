/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#ifndef SCRATCHBIRD_GEO_GEODETIC_H
#define SCRATCHBIRD_GEO_GEODETIC_H

#include <cmath>
#include <vector>

namespace scratchbird {
namespace geo {

/**
 * @brief Geodetic calculations for geographic coordinate systems
 *
 * These functions implement geodetic calculations using the WGS84 ellipsoid
 * for SRID 4326 and other geographic coordinate systems.
 */
class Geodetic {
public:
    // WGS84 ellipsoid parameters
    static constexpr double SEMI_MAJOR_AXIS = 6378137.0;        // a (meters)
    static constexpr double SEMI_MINOR_AXIS = 6356752.314245;   // b (meters)
    static constexpr double FLATTENING = 1.0 / 298.257223563;   // f
    static constexpr double ECCENTRICITY_SQ = 0.00669437999014; // e^2
    static constexpr double PI = 3.141592653589793238462643383279502884;

    /**
     * @brief Calculate geodetic distance between two points using Vincenty formula
     *
     * This is the most accurate method for calculating distances on an ellipsoid.
     * Accurate to within 0.5mm for most cases.
     *
     * @param lon1 Longitude of first point (degrees)
     * @param lat1 Latitude of first point (degrees)
     * @param lon2 Longitude of second point (degrees)
     * @param lat2 Latitude of second point (degrees)
     * @return Distance in meters
     */
    static double vincentyDistance(double lon1, double lat1, double lon2, double lat2);

    /**
     * @brief Calculate geodetic distance using Haversine formula
     *
     * Simpler and faster than Vincenty, but assumes spherical Earth.
     * Accurate to within ~0.5% for most cases.
     *
     * @param lon1 Longitude of first point (degrees)
     * @param lat1 Latitude of first point (degrees)
     * @param lon2 Longitude of second point (degrees)
     * @param lat2 Latitude of second point (degrees)
     * @return Distance in meters
     */
    static double haversineDistance(double lon1, double lat1, double lon2, double lat2);

    /**
     * @brief Calculate geodetic area of a polygon on WGS84 ellipsoid
     *
     * Uses the algorithm from:
     * "Some algorithms for polygons on a sphere" by Robert G. Chamberlain and William H. Duquette
     *
     * @param lons Vector of longitudes (degrees)
     * @param lats Vector of latitudes (degrees)
     * @return Area in square meters
     */
    static double geodeticArea(const std::vector<double>& lons, const std::vector<double>& lats);

    /**
     * @brief Convert degrees to radians
     */
    static double toRadians(double degrees) {
        return degrees * PI / 180.0;
    }

    /**
     * @brief Convert radians to degrees
     */
    static double toDegrees(double radians) {
        return radians * 180.0 / PI;
    }
};

} // namespace geo
} // namespace scratchbird

#endif // SCRATCHBIRD_GEO_GEODETIC_H

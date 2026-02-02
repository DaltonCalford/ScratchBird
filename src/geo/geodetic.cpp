/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/geo/geodetic.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace scratchbird {
namespace geo {

// ============================================================================
// Vincenty Distance (most accurate)
// ============================================================================

double Geodetic::vincentyDistance(double lon1, double lat1, double lon2, double lat2) {
    // Convert to radians
    double lat1_rad = toRadians(lat1);
    double lat2_rad = toRadians(lat2);
    double lon1_rad = toRadians(lon1);
    double lon2_rad = toRadians(lon2);

    double L = lon2_rad - lon1_rad;

    double U1 = std::atan((1 - FLATTENING) * std::tan(lat1_rad));
    double U2 = std::atan((1 - FLATTENING) * std::tan(lat2_rad));

    double sinU1 = std::sin(U1);
    double cosU1 = std::cos(U1);
    double sinU2 = std::sin(U2);
    double cosU2 = std::cos(U2);

    double lambda = L;
    double lambdaP = 2 * M_PI;
    int iterLimit = 100;
    double cosSqAlpha, sinSigma, cos2SigmaM, cosSigma, sigma;

    while (std::abs(lambda - lambdaP) > 1e-12 && --iterLimit > 0) {
        double sinLambda = std::sin(lambda);
        double cosLambda = std::cos(lambda);

        sinSigma = std::sqrt(
            (cosU2 * sinLambda) * (cosU2 * sinLambda) +
            (cosU1 * sinU2 - sinU1 * cosU2 * cosLambda) *
            (cosU1 * sinU2 - sinU1 * cosU2 * cosLambda)
        );

        if (sinSigma == 0) {
            return 0.0; // Co-incident points
        }

        cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLambda;
        sigma = std::atan2(sinSigma, cosSigma);

        double sinAlpha = cosU1 * cosU2 * sinLambda / sinSigma;
        cosSqAlpha = 1 - sinAlpha * sinAlpha;

        if (cosSqAlpha == 0) {
            cos2SigmaM = 0; // Equatorial line
        } else {
            cos2SigmaM = cosSigma - 2 * sinU1 * sinU2 / cosSqAlpha;
        }

        double C = FLATTENING / 16 * cosSqAlpha * (4 + FLATTENING * (4 - 3 * cosSqAlpha));

        lambdaP = lambda;
        lambda = L + (1 - C) * FLATTENING * sinAlpha *
                 (sigma + C * sinSigma *
                  (cos2SigmaM + C * cosSigma *
                   (-1 + 2 * cos2SigmaM * cos2SigmaM)));
    }

    if (iterLimit == 0) {
        // Formula failed to converge, fall back to Haversine
        return haversineDistance(lon1, lat1, lon2, lat2);
    }

    double uSq = cosSqAlpha * (SEMI_MAJOR_AXIS * SEMI_MAJOR_AXIS -
                               SEMI_MINOR_AXIS * SEMI_MINOR_AXIS) /
                 (SEMI_MINOR_AXIS * SEMI_MINOR_AXIS);

    double A = 1 + uSq / 16384 * (4096 + uSq * (-768 + uSq * (320 - 175 * uSq)));
    double B = uSq / 1024 * (256 + uSq * (-128 + uSq * (74 - 47 * uSq)));

    double deltaSigma = B * sinSigma *
                        (cos2SigmaM + B / 4 *
                         (cosSigma * (-1 + 2 * cos2SigmaM * cos2SigmaM) -
                          B / 6 * cos2SigmaM * (-3 + 4 * sinSigma * sinSigma) *
                          (-3 + 4 * cos2SigmaM * cos2SigmaM)));

    double distance = SEMI_MINOR_AXIS * A * (sigma - deltaSigma);

    return distance;
}

// ============================================================================
// Haversine Distance (faster, less accurate)
// ============================================================================

double Geodetic::haversineDistance(double lon1, double lat1, double lon2, double lat2) {
    // Mean radius of Earth in meters
    const double R = 6371000.0;

    double dLat = toRadians(lat2 - lat1);
    double dLon = toRadians(lon2 - lon1);

    double lat1_rad = toRadians(lat1);
    double lat2_rad = toRadians(lat2);

    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1_rad) * std::cos(lat2_rad) *
               std::sin(dLon / 2) * std::sin(dLon / 2);

    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    double distance = R * c;

    return distance;
}

// ============================================================================
// Geodetic Area (spherical approximation)
// ============================================================================

double Geodetic::geodeticArea(const std::vector<double>& lons, const std::vector<double>& lats) {
    if (lons.size() != lats.size() || lons.size() < 3) {
        throw std::invalid_argument("Invalid polygon: need at least 3 points");
    }

    // Use spherical approximation for area calculation
    // Based on "Some algorithms for polygons on a sphere" by Chamberlain & Duquette

    const double R = SEMI_MAJOR_AXIS; // Use WGS84 semi-major axis

    // Convert to radians
    std::vector<double> lons_rad(lons.size());
    std::vector<double> lats_rad(lats.size());

    for (size_t i = 0; i < lons.size(); ++i) {
        lons_rad[i] = toRadians(lons[i]);
        lats_rad[i] = toRadians(lats[i]);
    }

    // Calculate area using spherical excess
    double area = 0.0;

    for (size_t i = 0; i < lons_rad.size() - 1; ++i) {
        size_t j = i + 1;

        double lon1 = lons_rad[i];
        double lat1 = lats_rad[i];
        double lon2 = lons_rad[j];
        double lat2 = lats_rad[j];

        // Spherical triangle area formula
        double dLon = lon2 - lon1;

        area += (dLon * (2 + std::sin(lat1) + std::sin(lat2)));
    }

    area = std::abs(area * R * R / 2.0);

    return area;
}

} // namespace geo
} // namespace scratchbird

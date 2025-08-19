/*
 *	PROGRAM:		Geometric Types
 *	MODULE:			GeometricTypes.cpp
 *	DESCRIPTION:	Implementation of 2D point and geometric operations
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by ScratchBird Development Team
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2025 ScratchBird Development Team
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): _______________________________________.
 *
 */

#include "GeometricTypes.h"
#include "classes/fb_string.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>

using namespace ScratchBird;

// Point constants
const Point PointConstants::ORIGIN(0.0, 0.0);
const Point PointConstants::UNIT_X(1.0, 0.0);
const Point PointConstants::UNIT_Y(0.0, 1.0);
const Point PointConstants::ONE(1.0, 1.0);

// Point implementation

Point::Point(const char* point_literal) : x(0.0), y(0.0) {
    if (point_literal && *point_literal) {
        parsePointLiteral(point_literal);
    }
}

bool Point::operator==(const Point& other) const {
    return (std::abs(x - other.x) < EPSILON) && (std::abs(y - other.y) < EPSILON);
}

Point Point::operator+(const Point& other) const {
    return Point(x + other.x, y + other.y);
}

Point Point::operator-(const Point& other) const {
    return Point(x - other.x, y - other.y);
}

Point Point::operator*(double scalar) const {
    return Point(x * scalar, y * scalar);
}

Point Point::operator/(double scalar) const {
    if (std::abs(scalar) < EPSILON) {
        invalid_point();
    }
    return Point(x / scalar, y / scalar);
}

double Point::distance(const Point& other) const {
    return std::sqrt(distanceSquared(other));
}

double Point::distanceSquared(const Point& other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    return dx * dx + dy * dy;
}

double Point::magnitude() const {
    return std::sqrt(magnitudeSquared());
}

double Point::magnitudeSquared() const {
    return x * x + y * y;
}

Point Point::normalize() const {
    double mag = magnitude();
    if (mag < EPSILON) {
        return Point(0.0, 0.0);
    }
    return Point(x / mag, y / mag);
}

double Point::dotProduct(const Point& other) const {
    return x * other.x + y * other.y;
}

double Point::crossProduct(const Point& other) const {
    return x * other.y - y * other.x;
}

bool Point::isCollinearWith(const Point& p1, const Point& p2) const {
    // Three points are collinear if the cross product of vectors is zero
    Point v1 = p1 - *this;
    Point v2 = p2 - *this;
    return std::abs(v1.crossProduct(v2)) < EPSILON;
}

double Point::angleWith(const Point& other) const {
    double dot = dotProduct(other);
    double mag1 = magnitude();
    double mag2 = other.magnitude();
    
    if (mag1 < EPSILON || mag2 < EPSILON) {
        return 0.0;
    }
    
    double cos_angle = dot / (mag1 * mag2);
    // Clamp to [-1, 1] to avoid numerical errors
    cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
    
    return std::acos(cos_angle);
}

void Point::toString(string& result) const {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "(%.6g,%.6g)", x, y);
    result = buffer;
}

string Point::toString() const {
    string result;
    toString(result);
    return result;
}

void Point::parsePointLiteral(const char* literal) {
    if (!literal || !isValidPointLiteral(literal)) {
        invalid_point();
    }
    
    // Skip whitespace and find opening parenthesis
    const char* p = literal;
    while (*p && std::isspace(*p)) p++;
    
    if (*p != '(') {
        invalid_point();
    }
    p++;
    
    // Parse x coordinate
    char* endptr;
    x = strtod(p, &endptr);
    if (endptr == p) {
        invalid_point();
    }
    p = endptr;
    
    // Skip whitespace and find comma
    while (*p && std::isspace(*p)) p++;
    if (*p != ',') {
        invalid_point();
    }
    p++;
    
    // Parse y coordinate
    y = strtod(p, &endptr);
    if (endptr == p) {
        invalid_point();
    }
    p = endptr;
    
    // Skip whitespace and find closing parenthesis
    while (*p && std::isspace(*p)) p++;
    if (*p != ')') {
        invalid_point();
    }
}

bool Point::isValidPointLiteral(const char* literal) {
    if (!literal) return false;
    
    try {
        Point temp;
        temp.parsePointLiteral(literal);
        return temp.isFinite();
    }
    catch (...) {
        return false;
    }
}

void Point::pack(UCHAR* buffer) const {
    if (!buffer) return;
    
    memcpy(buffer, &x, sizeof(double));
    memcpy(buffer + sizeof(double), &y, sizeof(double));
}

void Point::unpack(const UCHAR* buffer) {
    if (!buffer) {
        x = y = 0.0;
        return;
    }
    
    memcpy(&x, buffer, sizeof(double));
    memcpy(&y, buffer + sizeof(double), sizeof(double));
}

ULONG Point::makeIndexKey(vary* buf) const {
    if (!buf) return 0;
    
    UCHAR* p = reinterpret_cast<UCHAR*>(buf->vary_string);
    pack(p);
    
    buf->vary_length = getStorageSize();
    return buf->vary_length;
}

bool Point::isValid() const {
    return isFinite();
}

bool Point::isFinite() const {
    return std::isfinite(x) && std::isfinite(y);
}

void Point::invalid_point() {
    // throw exception or handle error
    // For now, just set to origin
}

// GeometricUtils implementation

double GeometricUtils::euclideanDistance(const Point& p1, const Point& p2) {
    return p1.distance(p2);
}

double GeometricUtils::manhattanDistance(const Point& p1, const Point& p2) {
    return std::abs(p1.x - p2.x) + std::abs(p1.y - p2.y);
}

double GeometricUtils::triangleArea(const Point& p1, const Point& p2, const Point& p3) {
    // Using the cross product formula: Area = |cross(p2-p1, p3-p1)| / 2
    Point v1 = p2 - p1;
    Point v2 = p3 - p1;
    return std::abs(v1.crossProduct(v2)) / 2.0;
}

bool GeometricUtils::areCollinear(const Point& p1, const Point& p2, const Point& p3) {
    return p1.isCollinearWith(p2, p3);
}

int GeometricUtils::orientation(const Point& p1, const Point& p2, const Point& p3) {
    Point v1 = p2 - p1;
    Point v2 = p3 - p1;
    double cross = v1.crossProduct(v2);
    
    const double EPSILON = 1e-10;
    if (std::abs(cross) < EPSILON) return 0;  // Collinear
    return (cross > 0) ? 2 : 1;  // Counter-clockwise : Clockwise
}

Point GeometricUtils::rotate(const Point& p, double angle) {
    double cos_a = std::cos(angle);
    double sin_a = std::sin(angle);
    return Point(p.x * cos_a - p.y * sin_a, p.x * sin_a + p.y * cos_a);
}

Point GeometricUtils::rotate(const Point& p, const Point& center, double angle) {
    Point translated = p - center;
    Point rotated = rotate(translated, angle);
    return rotated + center;
}

Point GeometricUtils::translate(const Point& p, double dx, double dy) {
    return Point(p.x + dx, p.y + dy);
}

Point GeometricUtils::scale(const Point& p, double factor) {
    return Point(p.x * factor, p.y * factor);
}

Point GeometricUtils::scale(const Point& p, double fx, double fy) {
    return Point(p.x * fx, p.y * fy);
}

Point GeometricUtils::lerp(const Point& p1, const Point& p2, double t) {
    return Point(p1.x + t * (p2.x - p1.x), p1.y + t * (p2.y - p1.y));
}

Point GeometricUtils::min(const Point& p1, const Point& p2) {
    return Point(std::min(p1.x, p2.x), std::min(p1.y, p2.y));
}

Point GeometricUtils::max(const Point& p1, const Point& p2) {
    return Point(std::max(p1.x, p2.x), std::max(p1.y, p2.y));
}

Point GeometricUtils::perpendicular(const Point& p) {
    return Point(-p.y, p.x);
}

Point GeometricUtils::reflect(const Point& p, const Point& normal) {
    Point n = normal.normalize();
    return p - n * (2.0 * p.dotProduct(n));
}
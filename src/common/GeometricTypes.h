/*
 *	PROGRAM:		Geometric Types
 *	MODULE:			GeometricTypes.h
 *	DESCRIPTION:	2D point and geometric operations
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

#ifndef SB_GEOMETRIC_TYPES_H
#define SB_GEOMETRIC_TYPES_H

#include "firebird/Interface.h"
#include "sb_exception.h"
#include "classes/fb_string.h"
#include <cmath>

namespace ScratchBird {

// 2D Point class for geometric operations
class Point {
public:
    double x;
    double y;
    
    // Constructors
    Point() : x(0.0), y(0.0) {}
    Point(double x_coord, double y_coord) : x(x_coord), y(y_coord) {}
    Point(const char* point_literal);
    Point(const Point& other) : x(other.x), y(other.y) {}
    
    // Assignment operator
    Point& operator=(const Point& other) {
        if (this != &other) {
            x = other.x;
            y = other.y;
        }
        return *this;
    }
    
    // Comparison operators
    bool operator==(const Point& other) const;
    bool operator!=(const Point& other) const { return !(*this == other); }
    
    // Arithmetic operators
    Point operator+(const Point& other) const;
    Point operator-(const Point& other) const;
    Point operator*(double scalar) const;
    Point operator/(double scalar) const;
    
    // Distance and geometric operations
    double distance(const Point& other) const;
    double distanceSquared(const Point& other) const;
    double magnitude() const;
    double magnitudeSquared() const;
    Point normalize() const;
    
    // Dot and cross products
    double dotProduct(const Point& other) const;
    double crossProduct(const Point& other) const;  // 2D cross product (scalar)
    
    // Geometric predicates
    bool isOrigin() const { return x == 0.0 && y == 0.0; }
    bool isCollinearWith(const Point& p1, const Point& p2) const;
    double angleWith(const Point& other) const;  // Angle between vectors
    
    // String representation
    void toString(string& result) const;
    string toString() const;
    
    // Parse from string representation: "(x,y)"
    void parsePointLiteral(const char* literal);
    static bool isValidPointLiteral(const char* literal);
    
    // Storage format for database
    void pack(UCHAR* buffer) const;
    void unpack(const UCHAR* buffer);
    static constexpr ULONG getStorageSize() { return 2 * sizeof(double); }
    
    // Index key generation for indices
    ULONG makeIndexKey(vary* buf) const;
    static ULONG getIndexKeyLength() { return 2 * sizeof(double) + sizeof(USHORT); }
    
    // Validation
    bool isValid() const;
    bool isFinite() const;
    
private:
    static constexpr double EPSILON = 1e-10;
    static void invalid_point();
};

// Geometric utility functions
class GeometricUtils {
public:
    // Distance calculations
    static double euclideanDistance(const Point& p1, const Point& p2);
    static double manhattanDistance(const Point& p1, const Point& p2);
    
    // Area calculations (for polygons defined by points)
    static double triangleArea(const Point& p1, const Point& p2, const Point& p3);
    
    // Geometric predicates
    static bool areCollinear(const Point& p1, const Point& p2, const Point& p3);
    static int orientation(const Point& p1, const Point& p2, const Point& p3);  // 0=collinear, 1=CW, 2=CCW
    
    // Point transformations
    static Point rotate(const Point& p, double angle);  // Rotate around origin
    static Point rotate(const Point& p, const Point& center, double angle);  // Rotate around center
    static Point translate(const Point& p, double dx, double dy);
    static Point scale(const Point& p, double factor);
    static Point scale(const Point& p, double fx, double fy);  // Non-uniform scaling
    
    // Interpolation
    static Point lerp(const Point& p1, const Point& p2, double t);  // Linear interpolation
    
    // Bounding operations
    static Point min(const Point& p1, const Point& p2);
    static Point max(const Point& p1, const Point& p2);
    
    // Vector operations
    static Point perpendicular(const Point& p);  // Perpendicular vector
    static Point reflect(const Point& p, const Point& normal);  // Reflect across normal
};

// Common point constants
class PointConstants {
public:
    static const Point ORIGIN;      // (0, 0)
    static const Point UNIT_X;      // (1, 0)
    static const Point UNIT_Y;      // (0, 1)
    static const Point ONE;         // (1, 1)
};

// Type aliases
using Point2D = Point;
using Vector2D = Point;  // Points can be used as 2D vectors

} // namespace ScratchBird

#endif // SB_GEOMETRIC_TYPES_H
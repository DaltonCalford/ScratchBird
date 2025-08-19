#include "SpatialDataTypes.h"
#include "common/classes/ByteChunk.h"
#include "common/StatusArg.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

using namespace ScratchBird;

//============================================================================
// MBR Implementation
//============================================================================

bool MBR::intersects(const MBR& other) const
{
    return !(other.minX > maxX || other.maxX < minX || 
             other.minY > maxY || other.maxY < minY);
}

bool MBR::contains(const MBR& other) const
{
    return (other.minX >= minX && other.maxX <= maxX &&
            other.minY >= minY && other.maxY <= maxY);
}

bool MBR::contains(double x, double y) const
{
    return (x >= minX && x <= maxX && y >= minY && y <= maxY);
}

double MBR::area() const
{
    if (!isValid()) return 0.0;
    return (maxX - minX) * (maxY - minY);
}

double MBR::enlargement(const MBR& other) const
{
    if (!isValid() || !other.isValid()) return 0.0;
    
    double newMinX = std::min(minX, other.minX);
    double newMinY = std::min(minY, other.minY);
    double newMaxX = std::max(maxX, other.maxX);
    double newMaxY = std::max(maxY, other.maxY);
    
    double newArea = (newMaxX - newMinX) * (newMaxY - newMinY);
    double currentArea = area();
    
    return newArea - currentArea;
}

void MBR::expand(const MBR& other)
{
    if (!other.isValid()) return;
    
    if (!isValid()) {
        *this = other;
        return;
    }
    
    minX = std::min(minX, other.minX);
    minY = std::min(minY, other.minY);
    maxX = std::max(maxX, other.maxX);
    maxY = std::max(maxY, other.maxY);
}

void MBR::expand(double x, double y)
{
    if (!SpatialUtils::isValidCoordinate(x, y)) return;
    
    if (!isValid()) {
        minX = maxX = x;
        minY = maxY = y;
        return;
    }
    
    minX = std::min(minX, x);
    minY = std::min(minY, y);
    maxX = std::max(maxX, x);
    maxY = std::max(maxY, y);
}

bool MBR::isValid() const
{
    return (minX <= maxX && minY <= maxY && 
            minX >= MIN_COORDINATE && maxX <= MAX_COORDINATE &&
            minY >= MIN_COORDINATE && maxY <= MAX_COORDINATE);
}

void MBR::reset()
{
    minX = minY = MAX_COORDINATE;
    maxX = maxY = MIN_COORDINATE;
}

//============================================================================
// Coordinate Implementation
//============================================================================

bool Coordinate::equals(const Coordinate& other, double tolerance) const
{
    return (std::abs(x - other.x) <= tolerance &&
            std::abs(y - other.y) <= tolerance &&
            (!hasZ || !other.hasZ || std::abs(z - other.z) <= tolerance) &&
            (!hasM || !other.hasM || std::abs(m - other.m) <= tolerance));
}

double Coordinate::distance2D(const Coordinate& other) const
{
    double dx = x - other.x;
    double dy = y - other.y;
    return std::sqrt(dx * dx + dy * dy);
}

double Coordinate::distance3D(const Coordinate& other) const
{
    double dx = x - other.x;
    double dy = y - other.y;
    double dz = (hasZ && other.hasZ) ? (z - other.z) : 0.0;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool Coordinate::isValid() const
{
    return SpatialUtils::isValidCoordinate(x, y) &&
           (!hasZ || (z >= MIN_COORDINATE && z <= MAX_COORDINATE)) &&
           (!hasM || (m >= MIN_COORDINATE && m <= MAX_COORDINATE));
}

//============================================================================
// Point Implementation
//============================================================================

MBR Point::getMBR() const
{
    return MBR(coordinate.x, coordinate.y, coordinate.x, coordinate.y);
}

string Point::toWKT() const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(WKT_MAX_PRECISION);
    oss << "POINT(";
    oss << coordinate.x << " " << coordinate.y;
    if (coordinate.hasZ) oss << " " << coordinate.z;
    if (coordinate.hasM) oss << " " << coordinate.m;
    oss << ")";
    return oss.str();
}

ByteChunk* Point::toWKB() const
{
    ULONG size = getWKBSize();
    ByteChunk* wkb = FB_NEW_POOL(pool) ByteChunk(pool, size);
    
    UCHAR* data = wkb->getBuffer();
    ULONG offset = 0;
    
    // Byte order (little endian)
    data[offset++] = 1;
    
    // Geometry type
    ULONG type = static_cast<ULONG>(geometryType);
    if (coordinate.hasZ) type |= 0x80000000;
    if (coordinate.hasM) type |= 0x40000000;
    memcpy(data + offset, &type, sizeof(ULONG));
    offset += sizeof(ULONG);
    
    // Coordinates
    memcpy(data + offset, &coordinate.x, sizeof(double));
    offset += sizeof(double);
    memcpy(data + offset, &coordinate.y, sizeof(double));
    offset += sizeof(double);
    
    if (coordinate.hasZ) {
        memcpy(data + offset, &coordinate.z, sizeof(double));
        offset += sizeof(double);
    }
    
    if (coordinate.hasM) {
        memcpy(data + offset, &coordinate.m, sizeof(double));
        offset += sizeof(double);
    }
    
    return wkb;
}

ULONG Point::getWKBSize() const
{
    ULONG size = 1 + sizeof(ULONG) + 2 * sizeof(double); // Header + X,Y
    if (coordinate.hasZ) size += sizeof(double);
    if (coordinate.hasM) size += sizeof(double);
    return size;
}

bool Point::intersects(const Geometry& other) const
{
    switch (other.getType()) {
        case GEOMETRY_POINT:
            return equals(other);
        case GEOMETRY_LINESTRING:
            return static_cast<const LineString&>(other).distanceToPoint(*this) < SPATIAL_PRECISION;
        case GEOMETRY_POLYGON:
            return static_cast<const Polygon&>(other).containsPoint(*this);
        default:
            return other.intersects(*this);
    }
}

bool Point::contains(const Geometry& other) const
{
    return other.getType() == GEOMETRY_POINT && equals(other);
}

bool Point::within(const Geometry& other) const
{
    return other.contains(*this);
}

bool Point::touches(const Geometry& other) const
{
    // Points can only touch other geometries at boundaries
    return false; // Points have no boundary
}

bool Point::crosses(const Geometry& other) const
{
    return false; // Points cannot cross other geometries
}

bool Point::overlaps(const Geometry& other) const
{
    return false; // Points cannot overlap (only equal)
}

bool Point::equals(const Geometry& other) const
{
    if (other.getType() != GEOMETRY_POINT) return false;
    const Point& otherPoint = static_cast<const Point&>(other);
    return coordinate.equals(otherPoint.coordinate) && srid == other.getSRID();
}

bool Point::disjoint(const Geometry& other) const
{
    return !intersects(other);
}

double Point::distance(const Geometry& other) const
{
    switch (other.getType()) {
        case GEOMETRY_POINT: {
            const Point& otherPoint = static_cast<const Point&>(other);
            return coordinate.distance2D(otherPoint.coordinate);
        }
        case GEOMETRY_LINESTRING: {
            const LineString& line = static_cast<const LineString&>(other);
            return line.distanceToPoint(*this);
        }
        case GEOMETRY_POLYGON: {
            const Polygon& poly = static_cast<const Polygon&>(other);
            if (poly.containsPoint(*this)) return 0.0;
            return poly.distance(*this);
        }
        default:
            return other.distance(*this);
    }
}

bool Point::isWithinDistance(const Geometry& other, double maxDistance) const
{
    return distance(other) <= maxDistance;
}

//============================================================================
// LineString Implementation
//============================================================================

void LineString::addCoordinate(const Coordinate& coord)
{
    coordinates.add(coord);
}

void LineString::addCoordinate(double x, double y)
{
    coordinates.add(Coordinate(x, y));
}

void LineString::addCoordinate(double x, double y, double z)
{
    coordinates.add(Coordinate(x, y, z));
}

void LineString::addCoordinate(double x, double y, double z, double m)
{
    coordinates.add(Coordinate(x, y, z, m));
}

const Coordinate& LineString::getCoordinateAt(ULONG index) const
{
    if (index >= coordinates.getCount()) {
        Arg::Gds(isc_range_err).raise();
    }
    return coordinates[index];
}

void LineString::setCoordinateAt(ULONG index, const Coordinate& coord)
{
    if (index >= coordinates.getCount()) {
        Arg::Gds(isc_range_err).raise();
    }
    coordinates[index] = coord;
}

const Coordinate& LineString::getStartPoint() const
{
    if (coordinates.getCount() == 0) {
        Arg::Gds(isc_range_err).raise();
    }
    return coordinates[0];
}

const Coordinate& LineString::getEndPoint() const
{
    if (coordinates.getCount() == 0) {
        Arg::Gds(isc_range_err).raise();
    }
    return coordinates[coordinates.getCount() - 1];
}

bool LineString::isClosed() const
{
    if (coordinates.getCount() < 3) return false;
    return getStartPoint().equals(getEndPoint());
}

bool LineString::isRing() const
{
    return isClosed() && isValid(); // Should also check for simplicity
}

MBR LineString::getMBR() const
{
    MBR mbr;
    for (ULONG i = 0; i < coordinates.getCount(); i++) {
        mbr.expand(coordinates[i].x, coordinates[i].y);
    }
    return mbr;
}

bool LineString::isValid() const
{
    if (coordinates.getCount() < 2) return false;
    
    for (ULONG i = 0; i < coordinates.getCount(); i++) {
        if (!coordinates[i].isValid()) return false;
    }
    
    return true;
}

double LineString::getLength() const
{
    double length = 0.0;
    for (ULONG i = 1; i < coordinates.getCount(); i++) {
        length += coordinates[i-1].distance2D(coordinates[i]);
    }
    return length;
}

string LineString::toWKT() const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(WKT_MAX_PRECISION);
    oss << "LINESTRING(";
    
    for (ULONG i = 0; i < coordinates.getCount(); i++) {
        if (i > 0) oss << ",";
        oss << coordinates[i].x << " " << coordinates[i].y;
        if (coordinates[i].hasZ) oss << " " << coordinates[i].z;
        if (coordinates[i].hasM) oss << " " << coordinates[i].m;
    }
    
    oss << ")";
    return oss.str();
}

ByteChunk* LineString::toWKB() const
{
    ULONG size = getWKBSize();
    ByteChunk* wkb = FB_NEW_POOL(pool) ByteChunk(pool, size);
    
    UCHAR* data = wkb->getBuffer();
    ULONG offset = 0;
    
    // Byte order (little endian)
    data[offset++] = 1;
    
    // Geometry type
    ULONG type = static_cast<ULONG>(geometryType);
    bool hasZ = coordinates.getCount() > 0 && coordinates[0].hasZ;
    bool hasM = coordinates.getCount() > 0 && coordinates[0].hasM;
    if (hasZ) type |= 0x80000000;
    if (hasM) type |= 0x40000000;
    memcpy(data + offset, &type, sizeof(ULONG));
    offset += sizeof(ULONG);
    
    // Number of points
    ULONG numPoints = coordinates.getCount();
    memcpy(data + offset, &numPoints, sizeof(ULONG));
    offset += sizeof(ULONG);
    
    // Coordinates
    for (ULONG i = 0; i < coordinates.getCount(); i++) {
        memcpy(data + offset, &coordinates[i].x, sizeof(double));
        offset += sizeof(double);
        memcpy(data + offset, &coordinates[i].y, sizeof(double));
        offset += sizeof(double);
        
        if (hasZ) {
            memcpy(data + offset, &coordinates[i].z, sizeof(double));
            offset += sizeof(double);
        }
        
        if (hasM) {
            memcpy(data + offset, &coordinates[i].m, sizeof(double));
            offset += sizeof(double);
        }
    }
    
    return wkb;
}

ULONG LineString::getWKBSize() const
{
    ULONG size = 1 + sizeof(ULONG) + sizeof(ULONG); // Header + type + num points
    ULONG coordSize = 2 * sizeof(double); // X, Y
    
    if (coordinates.getCount() > 0) {
        if (coordinates[0].hasZ) coordSize += sizeof(double);
        if (coordinates[0].hasM) coordSize += sizeof(double);
    }
    
    size += coordinates.getCount() * coordSize;
    return size;
}

double LineString::distanceToPoint(const Point& point) const
{
    if (coordinates.getCount() == 0) return INFINITY;
    if (coordinates.getCount() == 1) {
        return point.getCoordinate().distance2D(coordinates[0]);
    }
    
    double minDistance = INFINITY;
    const Coordinate& pointCoord = point.getCoordinate();
    
    for (ULONG i = 1; i < coordinates.getCount(); i++) {
        double segmentDistance = SpatialUtils::pointToLineDistance(
            pointCoord, coordinates[i-1], coordinates[i]);
        minDistance = std::min(minDistance, segmentDistance);
    }
    
    return minDistance;
}

bool LineString::intersectsLineString(const LineString& other) const
{
    // Simple MBR check first
    if (!getMBR().intersects(other.getMBR())) return false;
    
    // Check all segment pairs for intersection
    for (ULONG i = 1; i < coordinates.getCount(); i++) {
        for (ULONG j = 1; j < other.coordinates.getCount(); j++) {
            if (SpatialUtils::lineIntersectsLine(
                coordinates[i-1], coordinates[i],
                other.coordinates[j-1], other.coordinates[j])) {
                return true;
            }
        }
    }
    
    return false;
}

bool LineString::intersects(const Geometry& other) const
{
    switch (other.getType()) {
        case GEOMETRY_POINT:
            return distanceToPoint(static_cast<const Point&>(other)) < SPATIAL_PRECISION;
        case GEOMETRY_LINESTRING:
            return intersectsLineString(static_cast<const LineString&>(other));
        case GEOMETRY_POLYGON:
            return static_cast<const Polygon&>(other).intersects(*this);
        default:
            return other.intersects(*this);
    }
}

// Implement remaining LineString spatial relationship methods...
bool LineString::contains(const Geometry& other) const
{
    // LineString can only contain points that lie exactly on the line
    if (other.getType() == GEOMETRY_POINT) {
        return distanceToPoint(static_cast<const Point&>(other)) < SPATIAL_PRECISION;
    }
    return false;
}

bool LineString::within(const Geometry& other) const
{
    return other.contains(*this);
}

bool LineString::touches(const Geometry& other) const
{
    return intersects(other) && !overlaps(other);
}

bool LineString::crosses(const Geometry& other) const
{
    return intersects(other) && !within(other) && !contains(other);
}

bool LineString::overlaps(const Geometry& other) const
{
    if (other.getType() != GEOMETRY_LINESTRING) return false;
    // Complex geometric calculation needed
    return false; // Simplified implementation
}

bool LineString::equals(const Geometry& other) const
{
    if (other.getType() != GEOMETRY_LINESTRING) return false;
    const LineString& otherLine = static_cast<const LineString&>(other);
    
    if (coordinates.getCount() != otherLine.coordinates.getCount()) return false;
    if (srid != other.getSRID()) return false;
    
    for (ULONG i = 0; i < coordinates.getCount(); i++) {
        if (!coordinates[i].equals(otherLine.coordinates[i])) return false;
    }
    
    return true;
}

bool LineString::disjoint(const Geometry& other) const
{
    return !intersects(other);
}

double LineString::distance(const Geometry& other) const
{
    switch (other.getType()) {
        case GEOMETRY_POINT:
            return distanceToPoint(static_cast<const Point&>(other));
        case GEOMETRY_LINESTRING: {
            // Distance between two linestrings - complex calculation
            if (intersects(other)) return 0.0;
            // Simplified: minimum distance between any point on this line to other line
            double minDist = INFINITY;
            for (ULONG i = 0; i < coordinates.getCount(); i++) {
                Point p(coordinates[i], srid, pool);
                double dist = other.distance(p);
                minDist = std::min(minDist, dist);
            }
            return minDist;
        }
        default:
            return other.distance(*this);
    }
}

bool LineString::isWithinDistance(const Geometry& other, double maxDistance) const
{
    return distance(other) <= maxDistance;
}

//============================================================================
// LinearRing Implementation  
//============================================================================

bool LinearRing::isValid() const
{
    if (!LineString::isValid()) return false;
    if (coordinates.getCount() < 4) return false; // Minimum for a closed ring
    if (!isClosed()) return false;
    
    // Should also check for self-intersection, but simplified here
    return true;
}

void LinearRing::closeRing()
{
    if (coordinates.getCount() >= 2 && !isClosed()) {
        coordinates.add(coordinates[0]);
    }
}

double LinearRing::getSignedArea() const
{
    if (coordinates.getCount() < 3) return 0.0;
    
    double area = 0.0;
    ULONG n = coordinates.getCount();
    
    for (ULONG i = 0; i < n - 1; i++) {
        area += coordinates[i].x * coordinates[i + 1].y;
        area -= coordinates[i + 1].x * coordinates[i].y;
    }
    
    return area / 2.0;
}

bool LinearRing::isClockwise() const
{
    return getSignedArea() < 0.0;
}

void LinearRing::reverseOrientation()
{
    if (coordinates.getCount() <= 1) return;
    
    // Reverse all coordinates except the last (which should equal the first)
    ULONG n = isClosed() ? coordinates.getCount() - 1 : coordinates.getCount();
    
    for (ULONG i = 0; i < n / 2; i++) {
        Coordinate temp = coordinates[i];
        coordinates[i] = coordinates[n - 1 - i];
        coordinates[n - 1 - i] = temp;
    }
    
    // If closed, update the last coordinate to match the first
    if (isClosed()) {
        coordinates[coordinates.getCount() - 1] = coordinates[0];
    }
}

//============================================================================
// SpatialUtils Implementation
//============================================================================

namespace SpatialUtils
{
    double pointToPointDistance(const Coordinate& p1, const Coordinate& p2)
    {
        return p1.distance2D(p2);
    }
    
    double pointToLineDistance(const Coordinate& point, const Coordinate& lineStart, const Coordinate& lineEnd)
    {
        double dx = lineEnd.x - lineStart.x;
        double dy = lineEnd.y - lineStart.y;
        
        if (std::abs(dx) < SPATIAL_PRECISION && std::abs(dy) < SPATIAL_PRECISION) {
            // Line is a point
            return pointToPointDistance(point, lineStart);
        }
        
        double t = ((point.x - lineStart.x) * dx + (point.y - lineStart.y) * dy) / (dx * dx + dy * dy);
        
        if (t < 0.0) {
            // Closest point is lineStart
            return pointToPointDistance(point, lineStart);
        } else if (t > 1.0) {
            // Closest point is lineEnd
            return pointToPointDistance(point, lineEnd);
        } else {
            // Closest point is on the line segment
            Coordinate closest(lineStart.x + t * dx, lineStart.y + t * dy);
            return pointToPointDistance(point, closest);
        }
    }
    
    bool pointInPolygon(const Coordinate& point, const Polygon& polygon)
    {
        return polygon.containsCoordinate(point);
    }
    
    bool lineIntersectsLine(const Coordinate& l1p1, const Coordinate& l1p2, 
                           const Coordinate& l2p1, const Coordinate& l2p2)
    {
        // Using the cross product method to detect line intersection
        double d1 = (l2p2.x - l2p1.x) * (l1p1.y - l2p1.y) - (l2p2.y - l2p1.y) * (l1p1.x - l2p1.x);
        double d2 = (l2p2.x - l2p1.x) * (l1p2.y - l2p1.y) - (l2p2.y - l2p1.y) * (l1p2.x - l2p1.x);
        double d3 = (l1p2.x - l1p1.x) * (l2p1.y - l1p1.y) - (l1p2.y - l1p1.y) * (l2p1.x - l1p1.x);
        double d4 = (l1p2.x - l1p1.x) * (l2p2.y - l1p1.y) - (l1p2.y - l1p1.y) * (l2p2.x - l1p1.x);
        
        if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
            ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
            return true;
        }
        
        // Check for collinear cases
        if (std::abs(d1) < SPATIAL_PRECISION || std::abs(d2) < SPATIAL_PRECISION ||
            std::abs(d3) < SPATIAL_PRECISION || std::abs(d4) < SPATIAL_PRECISION) {
            // Lines are collinear or endpoint intersection - more complex check needed
            return false; // Simplified for now
        }
        
        return false;
    }
    
    bool mbrIntersects(const MBR& mbr1, const MBR& mbr2)
    {
        return mbr1.intersects(mbr2);
    }
    
    bool mbrContains(const MBR& container, const MBR& contained)
    {
        return container.contains(contained);
    }
    
    MBR mbrUnion(const MBR& mbr1, const MBR& mbr2)
    {
        MBR result = mbr1;
        result.expand(mbr2);
        return result;
    }
    
    double mbrArea(const MBR& mbr)
    {
        return mbr.area();
    }
    
    double mbrEnlargement(const MBR& mbr, const MBR& addition)
    {
        return mbr.enlargement(addition);
    }
    
    bool isValidCoordinate(double x, double y)
    {
        return (x >= MIN_COORDINATE && x <= MAX_COORDINATE &&
                y >= MIN_COORDINATE && y <= MAX_COORDINATE &&
                !std::isnan(x) && !std::isnan(y) &&
                !std::isinf(x) && !std::isinf(y));
    }
    
    bool isValidMBR(const MBR& mbr)
    {
        return mbr.isValid();
    }
    
    Coordinate transformCoordinate(const Coordinate& coord, SRID fromSRID, SRID toSRID)
    {
        // Basic implementation - in a full system this would use PROJ or similar
        if (fromSRID == toSRID) return coord;
        
        // For now, just return the coordinate unchanged
        // TODO: Implement proper coordinate transformation
        return coord;
    }
    
    MBR transformMBR(const MBR& mbr, SRID fromSRID, SRID toSRID)
    {
        if (fromSRID == toSRID) return mbr;
        
        // Transform corners and create new MBR
        Coordinate c1 = transformCoordinate(Coordinate(mbr.minX, mbr.minY), fromSRID, toSRID);
        Coordinate c2 = transformCoordinate(Coordinate(mbr.maxX, mbr.maxY), fromSRID, toSRID);
        
        return MBR(c1.x, c1.y, c2.x, c2.y);
    }
}

//============================================================================
// Factory Methods for Geometry
//============================================================================

Geometry* Geometry::fromWKT(const string& wkt, SRID srid, MemoryPool& pool)
{
    // TODO: Implement WKT parser
    // This would parse WKT strings like "POINT(1 2)" and create appropriate geometry objects
    Arg::Gds(isc_wish_list).raise(); // Not implemented yet
    return nullptr;
}

Geometry* Geometry::fromWKB(const ByteChunk* wkb, MemoryPool& pool)
{
    // TODO: Implement WKB parser  
    // This would parse WKB binary data and create appropriate geometry objects
    Arg::Gds(isc_wish_list).raise(); // Not implemented yet
    return nullptr;
}
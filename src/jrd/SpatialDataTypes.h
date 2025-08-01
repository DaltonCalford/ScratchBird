#ifndef SPATIAL_DATA_TYPES_H
#define SPATIAL_DATA_TYPES_H

#include "scratchbird.h"
#include "../common/classes/alloc.h"
#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/ByteChunk.h"
#include "../common/classes/fb_string.h"
#include "constants.h"
#include <vector>
#include <string>
#include <cmath>

namespace ScratchBird {

// Forward declarations
class Point;
class LineString;
class Polygon;
class MultiPoint;
class MultiLineString;
class MultiPolygon;
class GeometryCollection;

// Spatial Reference System ID type
typedef ULONG SRID;
const SRID DEFAULT_SRID = 0;  // Undefined SRS
const SRID WGS84_SRID = 4326; // WGS84 Geographic

// Geometry Type Constants (aligned with OGC standards)
enum GeometryType : UCHAR
{
    GEOMETRY_POINT = 1,
    GEOMETRY_LINESTRING = 2,
    GEOMETRY_POLYGON = 3,
    GEOMETRY_MULTIPOINT = 4,
    GEOMETRY_MULTILINESTRING = 5,
    GEOMETRY_MULTIPOLYGON = 6,
    GEOMETRY_GEOMETRYCOLLECTION = 7,
    
    // Extended types for indexing
    GEOMETRY_CIRCLE = 101,
    GEOMETRY_BOX = 102,
    GEOMETRY_MBR = 103
};

// Coordinate precision and limits
const double MIN_COORDINATE = -1e15;
const double MAX_COORDINATE = 1e15;
const double SPATIAL_PRECISION = 1e-9;
const USHORT MAX_SPATIAL_DIMENSIONS = 4; // X, Y, Z, M

// Well-Known Text (WKT) and Well-Known Binary (WKB) format constants
const USHORT WKB_HEADER_SIZE = 9; // 1 byte order + 4 bytes type + 4 bytes count
const USHORT WKT_MAX_PRECISION = 15;

// Minimum Bounding Rectangle (MBR) structure
struct MBR
{
    double minX, minY, maxX, maxY;
    
    MBR() : minX(MAX_COORDINATE), minY(MAX_COORDINATE), 
            maxX(MIN_COORDINATE), maxY(MIN_COORDINATE) {}
    
    MBR(double x1, double y1, double x2, double y2) 
        : minX(x1), minY(y1), maxX(x2), maxY(y2) {}
    
    // MBR operations
    bool intersects(const MBR& other) const;
    bool contains(const MBR& other) const;
    bool contains(double x, double y) const;
    double area() const;
    double enlargement(const MBR& other) const;
    void expand(const MBR& other);
    void expand(double x, double y);
    bool isValid() const;
    void reset();
};

// Base coordinate structure
struct Coordinate
{
    double x, y;
    double z, m;  // Optional Z (elevation) and M (measure) values
    bool hasZ, hasM;
    
    Coordinate() : x(0), y(0), z(0), m(0), hasZ(false), hasM(false) {}
    Coordinate(double x_, double y_) : x(x_), y(y_), z(0), m(0), hasZ(false), hasM(false) {}
    Coordinate(double x_, double y_, double z_) : x(x_), y(y_), z(z_), m(0), hasZ(true), hasM(false) {}
    Coordinate(double x_, double y_, double z_, double m_) : x(x_), y(y_), z(z_), m(m_), hasZ(true), hasM(true) {}
    
    bool equals(const Coordinate& other, double tolerance = SPATIAL_PRECISION) const;
    double distance2D(const Coordinate& other) const;
    double distance3D(const Coordinate& other) const;
    bool isValid() const;
};

// Abstract base class for all spatial geometries
class Geometry
{
protected:
    GeometryType geometryType;
    SRID srid;
    MemoryPool& pool;
    
public:
    Geometry(GeometryType type, SRID srid_, MemoryPool& p) 
        : geometryType(type), srid(srid_), pool(p) {}
    
    virtual ~Geometry() {}
    
    // Core geometry interface
    virtual GeometryType getType() const { return geometryType; }
    virtual SRID getSRID() const { return srid; }
    virtual void setSRID(SRID srid_) { srid = srid_; }
    
    // Spatial operations
    virtual MBR getMBR() const = 0;
    virtual bool isEmpty() const = 0;
    virtual bool isValid() const = 0;
    virtual ULONG getCoordinateCount() const = 0;
    virtual double getArea() const = 0;
    virtual double getLength() const = 0;
    
    // Serialization
    virtual ScratchBird::string toWKT() const = 0;
    virtual ByteChunk* toWKB() const = 0;
    virtual ULONG getWKBSize() const = 0;
    
    // Spatial relationships
    virtual bool intersects(const Geometry& other) const = 0;
    virtual bool contains(const Geometry& other) const = 0;
    virtual bool within(const Geometry& other) const = 0;
    virtual bool touches(const Geometry& other) const = 0;
    virtual bool crosses(const Geometry& other) const = 0;
    virtual bool overlaps(const Geometry& other) const = 0;
    virtual bool equals(const Geometry& other) const = 0;
    virtual bool disjoint(const Geometry& other) const = 0;
    
    // Distance operations
    virtual double distance(const Geometry& other) const = 0;
    virtual bool isWithinDistance(const Geometry& other, double distance) const = 0;
    
    // Factory methods
    static Geometry* fromWKT(const ScratchBird::string& wkt, SRID srid, MemoryPool& pool);
    static Geometry* fromWKB(const ByteChunk* wkb, MemoryPool& pool);
    
    // Type checking
    bool isPoint() const { return geometryType == GEOMETRY_POINT; }
    bool isLineString() const { return geometryType == GEOMETRY_LINESTRING; }
    bool isPolygon() const { return geometryType == GEOMETRY_POLYGON; }
    bool isCollection() const { return geometryType >= GEOMETRY_MULTIPOINT; }
};

// Point geometry implementation
class Point : public Geometry
{
private:
    Coordinate coordinate;
    
public:
    Point(double x, double y, SRID srid_, MemoryPool& p) 
        : Geometry(GEOMETRY_POINT, srid_, p), coordinate(x, y) {}
    
    Point(const Coordinate& coord, SRID srid_, MemoryPool& p) 
        : Geometry(GEOMETRY_POINT, srid_, p), coordinate(coord) {}
    
    // Coordinate access
    const Coordinate& getCoordinate() const { return coordinate; }
    void setCoordinate(const Coordinate& coord) { coordinate = coord; }
    double getX() const { return coordinate.x; }
    double getY() const { return coordinate.y; }
    double getZ() const { return coordinate.z; }
    double getM() const { return coordinate.m; }
    bool hasZ() const { return coordinate.hasZ; }
    bool hasM() const { return coordinate.hasM; }
    
    // Geometry interface implementation
    virtual MBR getMBR() const override;
    virtual bool isEmpty() const override { return false; }
    virtual bool isValid() const override { return coordinate.isValid(); }
    virtual ULONG getCoordinateCount() const override { return 1; }
    virtual double getArea() const override { return 0.0; }
    virtual double getLength() const override { return 0.0; }
    
    // Serialization
    virtual ScratchBird::string toWKT() const override;
    virtual ByteChunk* toWKB() const override;
    virtual ULONG getWKBSize() const override;
    
    // Spatial relationships
    virtual bool intersects(const Geometry& other) const override;
    virtual bool contains(const Geometry& other) const override;
    virtual bool within(const Geometry& other) const override;
    virtual bool touches(const Geometry& other) const override;
    virtual bool crosses(const Geometry& other) const override;
    virtual bool overlaps(const Geometry& other) const override;
    virtual bool equals(const Geometry& other) const override;
    virtual bool disjoint(const Geometry& other) const override;
    
    // Distance operations
    virtual double distance(const Geometry& other) const override;
    virtual bool isWithinDistance(const Geometry& other, double distance) const override;
};

// LineString geometry implementation
class LineString : public Geometry
{
private:
    ObjectsArray<Coordinate> coordinates;
    
public:
    LineString(SRID srid_, MemoryPool& p) 
        : Geometry(GEOMETRY_LINESTRING, srid_, p), coordinates(p) {}
    
    // Coordinate management
    void addCoordinate(const Coordinate& coord);
    void addCoordinate(double x, double y);
    void addCoordinate(double x, double y, double z);
    void addCoordinate(double x, double y, double z, double m);
    
    ULONG getNumPoints() const { return coordinates.getCount(); }
    const Coordinate& getCoordinateAt(ULONG index) const;
    void setCoordinateAt(ULONG index, const Coordinate& coord);
    
    // Special points
    const Coordinate& getStartPoint() const;
    const Coordinate& getEndPoint() const;
    bool isClosed() const;
    bool isRing() const; // Closed and simple (non-self-intersecting)
    
    // Geometry interface implementation
    virtual MBR getMBR() const override;
    virtual bool isEmpty() const override { return coordinates.getCount() == 0; }
    virtual bool isValid() const override;
    virtual ULONG getCoordinateCount() const override { return coordinates.getCount(); }
    virtual double getArea() const override { return 0.0; }
    virtual double getLength() const override;
    
    // Serialization
    virtual ScratchBird::string toWKT() const override;
    virtual ByteChunk* toWKB() const override;
    virtual ULONG getWKBSize() const override;
    
    // Spatial relationships
    virtual bool intersects(const Geometry& other) const override;
    virtual bool contains(const Geometry& other) const override;
    virtual bool within(const Geometry& other) const override;
    virtual bool touches(const Geometry& other) const override;
    virtual bool crosses(const Geometry& other) const override;
    virtual bool overlaps(const Geometry& other) const override;
    virtual bool equals(const Geometry& other) const override;
    virtual bool disjoint(const Geometry& other) const override;
    
    // Distance operations
    virtual double distance(const Geometry& other) const override;
    virtual bool isWithinDistance(const Geometry& other, double distance) const override;
    
    // LineString-specific operations
    double distanceToPoint(const Point& point) const;
    bool intersectsLineString(const LineString& other) const;
};

// Linear ring for polygon boundaries
class LinearRing : public LineString
{
public:
    LinearRing(SRID srid_, MemoryPool& p) : LineString(srid_, p) {}
    
    virtual bool isValid() const override;
    void closeRing(); // Ensure first and last points are the same
    double getSignedArea() const;
    bool isClockwise() const;
    void reverseOrientation();
};

// Polygon geometry implementation
class Polygon : public Geometry
{
private:
    LinearRing* exteriorRing;
    ObjectsArray<LinearRing> interiorRings;
    
public:
    Polygon(SRID srid_, MemoryPool& p) 
        : Geometry(GEOMETRY_POLYGON, srid_, p), exteriorRing(nullptr), interiorRings(p) {}
    
    ~Polygon();
    
    // Ring management
    void setExteriorRing(LinearRing* ring);
    LinearRing* getExteriorRing() const { return exteriorRing; }
    
    void addInteriorRing(LinearRing* ring);
    ULONG getNumInteriorRings() const { return interiorRings.getCount(); }
    LinearRing* getInteriorRingAt(ULONG index) const;
    
    // Geometry interface implementation
    virtual MBR getMBR() const override;
    virtual bool isEmpty() const override { return exteriorRing == nullptr; }
    virtual bool isValid() const override;
    virtual ULONG getCoordinateCount() const override;
    virtual double getArea() const override;
    virtual double getLength() const override; // Perimeter
    
    // Serialization
    virtual ScratchBird::string toWKT() const override;
    virtual ByteChunk* toWKB() const override;
    virtual ULONG getWKBSize() const override;
    
    // Spatial relationships
    virtual bool intersects(const Geometry& other) const override;
    virtual bool contains(const Geometry& other) const override;
    virtual bool within(const Geometry& other) const override;
    virtual bool touches(const Geometry& other) const override;
    virtual bool crosses(const Geometry& other) const override;
    virtual bool overlaps(const Geometry& other) const override;
    virtual bool equals(const Geometry& other) const override;
    virtual bool disjoint(const Geometry& other) const override;
    
    // Distance operations
    virtual double distance(const Geometry& other) const override;
    virtual bool isWithinDistance(const Geometry& other, double distance) const override;
    
    // Polygon-specific operations
    bool containsPoint(const Point& point) const;
    bool containsCoordinate(const Coordinate& coord) const;
    double getPerimeter() const { return getLength(); }
};

// Collection geometries base class
class GeometryCollection : public Geometry
{
protected:
    ObjectsArray<Geometry> geometries;
    
public:
    GeometryCollection(GeometryType type, SRID srid_, MemoryPool& p) 
        : Geometry(type, srid_, p), geometries(p) {}
    
    virtual ~GeometryCollection();
    
    // Collection management
    void addGeometry(Geometry* geom);
    ULONG getNumGeometries() const { return geometries.getCount(); }
    Geometry* getGeometryAt(ULONG index) const;
    
    // Geometry interface implementation
    virtual MBR getMBR() const override;
    virtual bool isEmpty() const override;
    virtual bool isValid() const override;
    virtual ULONG getCoordinateCount() const override;
    virtual double getArea() const override;
    virtual double getLength() const override;
    
    // Serialization  
    virtual ScratchBird::string toWKT() const override;
    virtual ByteChunk* toWKB() const override;
    virtual ULONG getWKBSize() const override;
    
    // Spatial relationships
    virtual bool intersects(const Geometry& other) const override;
    virtual bool contains(const Geometry& other) const override;
    virtual bool within(const Geometry& other) const override;
    virtual bool touches(const Geometry& other) const override;
    virtual bool crosses(const Geometry& other) const override;
    virtual bool overlaps(const Geometry& other) const override;
    virtual bool equals(const Geometry& other) const override;
    virtual bool disjoint(const Geometry& other) const override;
    
    // Distance operations
    virtual double distance(const Geometry& other) const override;
    virtual bool isWithinDistance(const Geometry& other, double distance) const override;
};

// MultiPoint collection
class MultiPoint : public GeometryCollection
{
public:
    MultiPoint(SRID srid_, MemoryPool& p) 
        : GeometryCollection(GEOMETRY_MULTIPOINT, srid_, p) {}
    
    void addPoint(Point* point);
    Point* getPointAt(ULONG index) const;
    
    virtual double getArea() const override { return 0.0; }
    virtual double getLength() const override { return 0.0; }
};

// MultiLineString collection
class MultiLineString : public GeometryCollection
{
public:
    MultiLineString(SRID srid_, MemoryPool& p) 
        : GeometryCollection(GEOMETRY_MULTILINESTRING, srid_, p) {}
    
    void addLineString(LineString* lineString);
    LineString* getLineStringAt(ULONG index) const;
    
    virtual double getArea() const override { return 0.0; }
    bool isClosed() const;
};

// MultiPolygon collection
class MultiPolygon : public GeometryCollection
{
public:
    MultiPolygon(SRID srid_, MemoryPool& p) 
        : GeometryCollection(GEOMETRY_MULTIPOLYGON, srid_, p) {}
    
    void addPolygon(Polygon* polygon);
    Polygon* getPolygonAt(ULONG index) const;
    
    virtual double getLength() const override; // Total perimeter
};

// Utility functions for spatial operations
namespace SpatialUtils
{
    // Geometric calculations
    double pointToPointDistance(const Coordinate& p1, const Coordinate& p2);
    double pointToLineDistance(const Coordinate& point, const Coordinate& lineStart, const Coordinate& lineEnd);
    bool pointInPolygon(const Coordinate& point, const Polygon& polygon);
    bool lineIntersectsLine(const Coordinate& l1p1, const Coordinate& l1p2, 
                           const Coordinate& l2p1, const Coordinate& l2p2);
    
    // MBR operations
    bool mbrIntersects(const MBR& mbr1, const MBR& mbr2);
    bool mbrContains(const MBR& container, const MBR& contained);
    MBR mbrUnion(const MBR& mbr1, const MBR& mbr2);
    double mbrArea(const MBR& mbr);
    double mbrEnlargement(const MBR& mbr, const MBR& addition);
    
    // Validation
    bool isValidCoordinate(double x, double y);
    bool isValidMBR(const MBR& mbr);
    
    // Coordinate transformations (basic support)
    Coordinate transformCoordinate(const Coordinate& coord, SRID fromSRID, SRID toSRID);
    MBR transformMBR(const MBR& mbr, SRID fromSRID, SRID toSRID);
}

} // namespace ScratchBird

#endif // SPATIAL_DATA_TYPES_H
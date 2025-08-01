#ifndef WKB_PARSER_H
#define WKB_PARSER_H

#include "SpatialDataTypes.h"
#include "common/classes/alloc.h"
#include "common/classes/ByteChunk.h"

namespace ScratchBird {

// WKB byte order constants
const UCHAR WKB_BIG_ENDIAN = 0;
const UCHAR WKB_LITTLE_ENDIAN = 1;

// WKB geometry type constants (aligned with OGC standards)
const ULONG WKB_POINT = 1;
const ULONG WKB_LINESTRING = 2;
const ULONG WKB_POLYGON = 3;
const ULONG WKB_MULTIPOINT = 4;
const ULONG WKB_MULTILINESTRING = 5;
const ULONG WKB_MULTIPOLYGON = 6;
const ULONG WKB_GEOMETRYCOLLECTION = 7;

// WKB extended type flags
const ULONG WKB_Z_FLAG = 0x80000000;
const ULONG WKB_M_FLAG = 0x40000000;
const ULONG WKB_SRID_FLAG = 0x20000000;

// WKB parsing utilities
class WKBReader
{
private:
    const UCHAR* data;
    ULONG size;
    ULONG position;
    bool bigEndian;
    MemoryPool& pool;
    SRID defaultSRID;
    
public:
    WKBReader(const ByteChunk* wkbData, MemoryPool& p, SRID srid = DEFAULT_SRID);
    WKBReader(const UCHAR* wkbBuffer, ULONG bufferSize, MemoryPool& p, SRID srid = DEFAULT_SRID);
    ~WKBReader();
    
    // Main parsing entry point
    Geometry* parseWKB();
    
    // Error handling
    class ParseError
    {
    private:
        string message;
        
    public:
        ParseError(const string& msg) : message(msg) {}
        const string& getMessage() const { return message; }
    };
    
private:
    // Basic data reading methods
    UCHAR readByte();
    ULONG readULong();
    double readDouble();
    void checkAvailable(ULONG bytes);
    
    // Endianness handling
    ULONG swapEndian32(ULONG value);
    ULONG64 swapEndian64(ULONG64 value);
    
    // Geometry parsing methods
    Geometry* parseGeometry();
    Point* parsePoint();
    LineString* parseLineString();
    Polygon* parsePolygon();
    MultiPoint* parseMultiPoint();
    MultiLineString* parseMultiLineString();
    MultiPolygon* parseMultiPolygon();
    GeometryCollection* parseGeometryCollection();
    
    // Helper parsing methods
    Coordinate parseCoordinate(bool hasZ, bool hasM);
    std::vector<Coordinate> parseCoordinateArray(ULONG count, bool hasZ, bool hasM);
    LinearRing* parseLinearRing(bool hasZ, bool hasM);
    
    // Utility methods
    void parseError(const string& message);
    GeometryType wkbTypeToGeometryType(ULONG wkbType);
};

// WKB writing utilities
class WKBWriter
{
private:
    UCHAR byteOrder;
    bool includeZ;
    bool includeM;
    bool includeSRID;
    
public:
    WKBWriter(UCHAR order = WKB_LITTLE_ENDIAN, bool z = false, bool m = false, bool srid = false);
    ~WKBWriter();
    
    // Configuration
    void setByteOrder(UCHAR order) { byteOrder = order; }
    void setIncludeZ(bool z) { includeZ = z; }
    void setIncludeM(bool m) { includeM = m; }
    void setIncludeSRID(bool srid) { includeSRID = srid; }
    
    // Main writing method
    ByteChunk* write(const Geometry& geometry, MemoryPool& pool);
    ULONG calculateWKBSize(const Geometry& geometry);
    
private:
    // Geometry-specific writers
    void writePoint(const Point& point, UCHAR* buffer, ULONG& offset);
    void writeLineString(const LineString& lineString, UCHAR* buffer, ULONG& offset);
    void writePolygon(const Polygon& polygon, UCHAR* buffer, ULONG& offset);
    void writeMultiPoint(const MultiPoint& multiPoint, UCHAR* buffer, ULONG& offset);
    void writeMultiLineString(const MultiLineString& multiLineString, UCHAR* buffer, ULONG& offset);
    void writeMultiPolygon(const MultiPolygon& multiPolygon, UCHAR* buffer, ULONG& offset);
    void writeGeometryCollection(const GeometryCollection& collection, UCHAR* buffer, ULONG& offset);
    
    // Helper writing methods
    void writeByte(UCHAR value, UCHAR* buffer, ULONG& offset);
    void writeULong(ULONG value, UCHAR* buffer, ULONG& offset);
    void writeDouble(double value, UCHAR* buffer, ULONG& offset);
    void writeCoordinate(const Coordinate& coord, UCHAR* buffer, ULONG& offset);
    void writeCoordinateArray(const ObjectsArray<Coordinate>& coords, UCHAR* buffer, ULONG& offset);
    void writeLinearRing(const LinearRing& ring, UCHAR* buffer, ULONG& offset);
    void writeGeometryHeader(GeometryType type, SRID srid, UCHAR* buffer, ULONG& offset);
    
    // Size calculation helpers
    ULONG getPointSize(const Point& point);
    ULONG getLineStringSize(const LineString& lineString);
    ULONG getPolygonSize(const Polygon& polygon);
    ULONG getMultiPointSize(const MultiPoint& multiPoint);
    ULONG getMultiLineStringSize(const MultiLineString& multiLineString);
    ULONG getMultiPolygonSize(const MultiPolygon& multiPolygon);
    ULONG getGeometryCollectionSize(const GeometryCollection& collection);
    ULONG getLinearRingSize(const LinearRing& ring);
    ULONG getCoordinateSize();
    ULONG getHeaderSize();
    
    // Endianness handling
    ULONG swapEndian32(ULONG value);
    ULONG64 swapEndian64(ULONG64 value);
    bool needsSwap();
    
    // Type conversion
    ULONG geometryTypeToWKB(GeometryType type, bool hasZ, bool hasM, bool hasSRID);
};

// Well-Known Binary utility functions
namespace WKBUtils
{
    // Validation functions
    bool isValidWKB(const ByteChunk* wkbData);
    bool isValidWKB(const UCHAR* buffer, ULONG size);
    GeometryType getWKBGeometryType(const ByteChunk* wkbData);
    GeometryType getWKBGeometryType(const UCHAR* buffer, ULONG size);
    
    // Conversion functions
    Geometry* fromWKB(const ByteChunk* wkbData, MemoryPool& pool);
    Geometry* fromWKB(const UCHAR* buffer, ULONG size, MemoryPool& pool);
    ByteChunk* toWKB(const Geometry& geometry, MemoryPool& pool);
    
    // Format information
    bool hasZDimension(const ByteChunk* wkbData);
    bool hasMDimension(const ByteChunk* wkbData);
    bool hasSRID(const ByteChunk* wkbData);
    SRID extractSRID(const ByteChunk* wkbData);
    
    // Byte order detection
    UCHAR getByteOrder(const ByteChunk* wkbData);
    UCHAR getByteOrder(const UCHAR* buffer);
    
    // Size calculations
    ULONG getWKBSize(const Geometry& geometry);
    ULONG getMinimumWKBSize(GeometryType type);
    
    // Format validation
    bool validateWKBFormat(const ByteChunk* wkbData, string& errorMessage);
    bool validateWKBFormat(const UCHAR* buffer, ULONG size, string& errorMessage);
    
    // WKB constants
    const ULONG WKB_HEADER_SIZE = 9;  // 1 byte order + 4 bytes type + 4 bytes count (for collections)
    const ULONG WKB_COORDINATE_SIZE = 16; // 8 bytes X + 8 bytes Y
    const ULONG WKB_COORDINATE_3D_SIZE = 24; // 8 bytes X + 8 bytes Y + 8 bytes Z
    const ULONG WKB_COORDINATE_4D_SIZE = 32; // 8 bytes X + 8 bytes Y + 8 bytes Z + 8 bytes M
}

// WKB format validation and repair
class WKBValidator
{
private:
    const UCHAR* data;
    ULONG size;
    
public:
    WKBValidator(const ByteChunk* wkbData);
    WKBValidator(const UCHAR* buffer, ULONG bufferSize);
    ~WKBValidator();
    
    // Validation methods
    bool isValid();
    bool validateStructure();
    bool validateGeometry();
    bool validateCoordinates();
    
    // Repair methods
    ByteChunk* repair(MemoryPool& pool);
    
    // Error reporting
    std::vector<string> getValidationErrors() const;
    
private:
    std::vector<string> errors;
    
    void addError(const string& error);
    bool validateGeometryStructure(ULONG& offset);
    bool validatePointStructure(ULONG& offset, bool hasZ, bool hasM);
    bool validateLineStringStructure(ULONG& offset, bool hasZ, bool hasM);
    bool validatePolygonStructure(ULONG& offset, bool hasZ, bool hasM);
    bool validateMultiGeometryStructure(ULONG& offset, bool hasZ, bool hasM);
    bool validateGeometryCollectionStructure(ULONG& offset);
    bool validateCoordinate(ULONG& offset, bool hasZ, bool hasM);
    ULONG readULongAt(ULONG offset, bool& success);
    double readDoubleAt(ULONG offset, bool& success);
};

} // namespace ScratchBird

#endif // WKB_PARSER_H
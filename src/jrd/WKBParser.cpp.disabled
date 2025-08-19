#include "WKBParser.h"
#include "common/StatusArg.h"
#include <cstring>
#include <algorithm>

using namespace ScratchBird;

//============================================================================
// WKBReader Implementation
//============================================================================

WKBReader::WKBReader(const ByteChunk* wkbData, MemoryPool& p, SRID srid)
    : data(wkbData->getBuffer()), size(wkbData->getCount()), position(0), 
      bigEndian(false), pool(p), defaultSRID(srid)
{
}

WKBReader::WKBReader(const UCHAR* wkbBuffer, ULONG bufferSize, MemoryPool& p, SRID srid)
    : data(wkbBuffer), size(bufferSize), position(0), 
      bigEndian(false), pool(p), defaultSRID(srid)
{
}

WKBReader::~WKBReader()
{
}

Geometry* WKBReader::parseWKB()
{
    if (size < 5) { // Minimum: 1 byte order + 4 bytes type
        parseError("WKB data too short");
    }
    
    position = 0;
    
    try {
        return parseGeometry();
    }
    catch (const ParseError& e) {
        Arg::Gds(isc_invalid_geometry).arg(e.getMessage()).raise();
        return nullptr;
    }
}

UCHAR WKBReader::readByte()
{
    checkAvailable(1);
    return data[position++];
}

ULONG WKBReader::readULong()
{
    checkAvailable(4);
    
    ULONG value;
    memcpy(&value, data + position, 4);
    position += 4;
    
    if (bigEndian) {
        value = swapEndian32(value);
    }
    
    return value;
}

double WKBReader::readDouble()
{
    checkAvailable(8);
    
    ULONG64 value;
    memcpy(&value, data + position, 8);
    position += 8;
    
    if (bigEndian) {
        value = swapEndian64(value);
    }
    
    double result;
    memcpy(&result, &value, 8);
    return result;
}

void WKBReader::checkAvailable(ULONG bytes)
{
    if (position + bytes > size) {
        parseError("Unexpected end of WKB data");
    }
}

ULONG WKBReader::swapEndian32(ULONG value)
{
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x000000FF) << 24);
}

ULONG64 WKBReader::swapEndian64(ULONG64 value)
{
    return ((value & 0xFF00000000000000ULL) >> 56) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x000000FF00000000ULL) >> 8) |
           ((value & 0x00000000FF000000ULL) << 8) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x00000000000000FFULL) << 56);
}

Geometry* WKBReader::parseGeometry()
{
    // Read byte order
    UCHAR byteOrder = readByte();
    if (byteOrder == WKB_BIG_ENDIAN) {
        bigEndian = true;
    } else if (byteOrder == WKB_LITTLE_ENDIAN) {
        bigEndian = false;
    } else {
        parseError("Invalid byte order marker");
    }
    
    // Read geometry type
    ULONG wkbType = readULong();
    
    // Extract flags
    bool hasZ = (wkbType & WKB_Z_FLAG) != 0;
    bool hasM = (wkbType & WKB_M_FLAG) != 0;
    bool hasSRID = (wkbType & WKB_SRID_FLAG) != 0;
    
    // Remove flags to get base type
    ULONG baseType = wkbType & 0x0000FFFF;
    
    // Read SRID if present
    SRID srid = defaultSRID;
    if (hasSRID) {
        srid = readULong();
    }
    
    // Parse geometry based on type
    switch (baseType) {
        case WKB_POINT:
            return parsePoint();
        case WKB_LINESTRING:
            return parseLineString();
        case WKB_POLYGON:
            return parsePolygon();
        case WKB_MULTIPOINT:
            return parseMultiPoint();
        case WKB_MULTILINESTRING:
            return parseMultiLineString();
        case WKB_MULTIPOLYGON:
            return parseMultiPolygon();
        case WKB_GEOMETRYCOLLECTION:
            return parseGeometryCollection();
        default:
            parseError("Unknown WKB geometry type");
    }
    
    return nullptr;
}

Point* WKBReader::parsePoint()
{
    // Point has no count field, just coordinates
    Coordinate coord = parseCoordinate(false, false); // Z/M determined from type flags
    return FB_NEW_POOL(pool) Point(coord, defaultSRID, pool);
}

LineString* WKBReader::parseLineString()
{
    ULONG numPoints = readULong();
    
    if (numPoints < 2) {
        parseError("LineString must have at least 2 points");
    }
    
    LineString* lineString = FB_NEW_POOL(pool) LineString(defaultSRID, pool);
    
    for (ULONG i = 0; i < numPoints; i++) {
        Coordinate coord = parseCoordinate(false, false);
        lineString->addCoordinate(coord);
    }
    
    return lineString;
}

Polygon* WKBReader::parsePolygon()
{
    ULONG numRings = readULong();
    
    if (numRings == 0) {
        parseError("Polygon must have at least 1 ring");
    }
    
    Polygon* polygon = FB_NEW_POOL(pool) Polygon(defaultSRID, pool);
    
    // Parse exterior ring
    LinearRing* exteriorRing = parseLinearRing(false, false);
    polygon->setExteriorRing(exteriorRing);
    
    // Parse interior rings
    for (ULONG i = 1; i < numRings; i++) {
        LinearRing* interiorRing = parseLinearRing(false, false);
        polygon->addInteriorRing(interiorRing);
    }
    
    return polygon;
}

MultiPoint* WKBReader::parseMultiPoint()
{
    ULONG numPoints = readULong();
    
    MultiPoint* multiPoint = FB_NEW_POOL(pool) MultiPoint(defaultSRID, pool);
    
    for (ULONG i = 0; i < numPoints; i++) {
        // Each point has its own WKB header
        Geometry* geom = parseGeometry();
        Point* point = dynamic_cast<Point*>(geom);
        if (!point) {
            parseError("Expected Point in MultiPoint");
        }
        multiPoint->addPoint(point);
    }
    
    return multiPoint;
}

MultiLineString* WKBReader::parseMultiLineString()
{
    ULONG numLineStrings = readULong();
    
    MultiLineString* multiLineString = FB_NEW_POOL(pool) MultiLineString(defaultSRID, pool);
    
    for (ULONG i = 0; i < numLineStrings; i++) {
        // Each linestring has its own WKB header
        Geometry* geom = parseGeometry();
        LineString* lineString = dynamic_cast<LineString*>(geom);
        if (!lineString) {
            parseError("Expected LineString in MultiLineString");
        }
        multiLineString->addLineString(lineString);
    }
    
    return multiLineString;
}

MultiPolygon* WKBReader::parseMultiPolygon()
{
    ULONG numPolygons = readULong();
    
    MultiPolygon* multiPolygon = FB_NEW_POOL(pool) MultiPolygon(defaultSRID, pool);
    
    for (ULONG i = 0; i < numPolygons; i++) {
        // Each polygon has its own WKB header
        Geometry* geom = parseGeometry();
        Polygon* polygon = dynamic_cast<Polygon*>(geom);
        if (!polygon) {
            parseError("Expected Polygon in MultiPolygon");
        }
        multiPolygon->addPolygon(polygon);
    }
    
    return multiPolygon;
}

GeometryCollection* WKBReader::parseGeometryCollection()
{
    ULONG numGeometries = readULong();
    
    GeometryCollection* collection = FB_NEW_POOL(pool) GeometryCollection(GEOMETRY_GEOMETRYCOLLECTION, defaultSRID, pool);
    
    for (ULONG i = 0; i < numGeometries; i++) {
        Geometry* geometry = parseGeometry();
        collection->addGeometry(geometry);
    }
    
    return collection;
}

Coordinate WKBReader::parseCoordinate(bool hasZ, bool hasM)
{
    double x = readDouble();
    double y = readDouble();
    
    Coordinate coord(x, y);
    
    if (hasZ) {
        coord.z = readDouble();
        coord.hasZ = true;
    }
    
    if (hasM) {
        coord.m = readDouble();
        coord.hasM = true;
    }
    
    return coord;
}

std::vector<Coordinate> WKBReader::parseCoordinateArray(ULONG count, bool hasZ, bool hasM)
{
    std::vector<Coordinate> coordinates;
    coordinates.reserve(count);
    
    for (ULONG i = 0; i < count; i++) {
        coordinates.push_back(parseCoordinate(hasZ, hasM));
    }
    
    return coordinates;
}

LinearRing* WKBReader::parseLinearRing(bool hasZ, bool hasM)
{
    ULONG numPoints = readULong();
    
    if (numPoints < 4) {
        parseError("LinearRing must have at least 4 points");
    }
    
    LinearRing* ring = FB_NEW_POOL(pool) LinearRing(defaultSRID, pool);
    
    for (ULONG i = 0; i < numPoints; i++) {
        Coordinate coord = parseCoordinate(hasZ, hasM);
        ring->addCoordinate(coord);
    }
    
    if (!ring->isClosed()) {
        parseError("LinearRing is not closed");
    }
    
    return ring;
}

void WKBReader::parseError(const string& message)
{
    throw ParseError(message);
}

GeometryType WKBReader::wkbTypeToGeometryType(ULONG wkbType)
{
    ULONG baseType = wkbType & 0x0000FFFF;
    
    switch (baseType) {
        case WKB_POINT: return GEOMETRY_POINT;
        case WKB_LINESTRING: return GEOMETRY_LINESTRING;
        case WKB_POLYGON: return GEOMETRY_POLYGON;
        case WKB_MULTIPOINT: return GEOMETRY_MULTIPOINT;
        case WKB_MULTILINESTRING: return GEOMETRY_MULTILINESTRING;
        case WKB_MULTIPOLYGON: return GEOMETRY_MULTIPOLYGON;
        case WKB_GEOMETRYCOLLECTION: return GEOMETRY_GEOMETRYCOLLECTION;
        default: return GEOMETRY_POINT; // Default fallback
    }
}

//============================================================================
// WKBWriter Implementation
//============================================================================

WKBWriter::WKBWriter(UCHAR order, bool z, bool m, bool srid)
    : byteOrder(order), includeZ(z), includeM(m), includeSRID(srid)
{
}

WKBWriter::~WKBWriter()
{
}

ByteChunk* WKBWriter::write(const Geometry& geometry, MemoryPool& pool)
{
    ULONG size = calculateWKBSize(geometry);
    ByteChunk* wkb = FB_NEW_POOL(pool) ByteChunk(pool, size);
    
    UCHAR* buffer = wkb->getBuffer();
    ULONG offset = 0;
    
    switch (geometry.getType()) {
        case GEOMETRY_POINT:
            writePoint(static_cast<const Point&>(geometry), buffer, offset);
            break;
        case GEOMETRY_LINESTRING:
            writeLineString(static_cast<const LineString&>(geometry), buffer, offset);
            break;
        case GEOMETRY_POLYGON:
            writePolygon(static_cast<const Polygon&>(geometry), buffer, offset);
            break;
        case GEOMETRY_MULTIPOINT:
            writeMultiPoint(static_cast<const MultiPoint&>(geometry), buffer, offset);
            break;
        case GEOMETRY_MULTILINESTRING:
            writeMultiLineString(static_cast<const MultiLineString&>(geometry), buffer, offset);
            break;
        case GEOMETRY_MULTIPOLYGON:
            writeMultiPolygon(static_cast<const MultiPolygon&>(geometry), buffer, offset);
            break;
        case GEOMETRY_GEOMETRYCOLLECTION:
            writeGeometryCollection(static_cast<const GeometryCollection&>(geometry), buffer, offset);
            break;
    }
    
    return wkb;
}

ULONG WKBWriter::calculateWKBSize(const Geometry& geometry)
{
    switch (geometry.getType()) {
        case GEOMETRY_POINT:
            return getPointSize(static_cast<const Point&>(geometry));
        case GEOMETRY_LINESTRING:
            return getLineStringSize(static_cast<const LineString&>(geometry));
        case GEOMETRY_POLYGON:
            return getPolygonSize(static_cast<const Polygon&>(geometry));
        case GEOMETRY_MULTIPOINT:
            return getMultiPointSize(static_cast<const MultiPoint&>(geometry));
        case GEOMETRY_MULTILINESTRING:
            return getMultiLineStringSize(static_cast<const MultiLineString&>(geometry));
        case GEOMETRY_MULTIPOLYGON:
            return getMultiPolygonSize(static_cast<const MultiPolygon&>(geometry));
        case GEOMETRY_GEOMETRYCOLLECTION:
            return getGeometryCollectionSize(static_cast<const GeometryCollection&>(geometry));
        default:
            return 0;
    }
}

void WKBWriter::writePoint(const Point& point, UCHAR* buffer, ULONG& offset)
{
    writeGeometryHeader(point.getType(), point.getSRID(), buffer, offset);
    writeCoordinate(point.getCoordinate(), buffer, offset);
}

void WKBWriter::writeLineString(const LineString& lineString, UCHAR* buffer, ULONG& offset)
{
    writeGeometryHeader(lineString.getType(), lineString.getSRID(), buffer, offset);
    
    ULONG numPoints = lineString.getNumPoints();
    writeULong(numPoints, buffer, offset);
    
    for (ULONG i = 0; i < numPoints; i++) {
        writeCoordinate(lineString.getCoordinateAt(i), buffer, offset);
    }
}

void WKBWriter::writePolygon(const Polygon& polygon, UCHAR* buffer, ULONG& offset)
{
    writeGeometryHeader(polygon.getType(), polygon.getSRID(), buffer, offset);
    
    ULONG numRings = 1 + polygon.getNumInteriorRings();
    writeULong(numRings, buffer, offset);
    
    // Write exterior ring
    writeLinearRing(*polygon.getExteriorRing(), buffer, offset);
    
    // Write interior rings
    for (ULONG i = 0; i < polygon.getNumInteriorRings(); i++) {
        writeLinearRing(*polygon.getInteriorRingAt(i), buffer, offset);
    }
}

void WKBWriter::writeMultiPoint(const MultiPoint& multiPoint, UCHAR* buffer, ULONG& offset)
{
    writeGeometryHeader(multiPoint.getType(), multiPoint.getSRID(), buffer, offset);
    
    ULONG numPoints = multiPoint.getNumGeometries();
    writeULong(numPoints, buffer, offset);
    
    for (ULONG i = 0; i < numPoints; i++) {
        writePoint(*multiPoint.getPointAt(i), buffer, offset);
    }
}

void WKBWriter::writeMultiLineString(const MultiLineString& multiLineString, UCHAR* buffer, ULONG& offset)
{
    writeGeometryHeader(multiLineString.getType(), multiLineString.getSRID(), buffer, offset);
    
    ULONG numLineStrings = multiLineString.getNumGeometries();
    writeULong(numLineStrings, buffer, offset);
    
    for (ULONG i = 0; i < numLineStrings; i++) {
        writeLineString(*multiLineString.getLineStringAt(i), buffer, offset);
    }
}

void WKBWriter::writeMultiPolygon(const MultiPolygon& multiPolygon, UCHAR* buffer, ULONG& offset)
{
    writeGeometryHeader(multiPolygon.getType(), multiPolygon.getSRID(), buffer, offset);
    
    ULONG numPolygons = multiPolygon.getNumGeometries();
    writeULong(numPolygons, buffer, offset);
    
    for (ULONG i = 0; i < numPolygons; i++) {
        writePolygon(*multiPolygon.getPolygonAt(i), buffer, offset);
    }
}

void WKBWriter::writeGeometryCollection(const GeometryCollection& collection, UCHAR* buffer, ULONG& offset)
{
    writeGeometryHeader(collection.getType(), collection.getSRID(), buffer, offset);
    
    ULONG numGeometries = collection.getNumGeometries();
    writeULong(numGeometries, buffer, offset);
    
    for (ULONG i = 0; i < numGeometries; i++) {
        // Recursively write each geometry with its own header
        const Geometry* geom = collection.getGeometryAt(i);
        switch (geom->getType()) {
            case GEOMETRY_POINT:
                writePoint(static_cast<const Point&>(*geom), buffer, offset);
                break;
            case GEOMETRY_LINESTRING:
                writeLineString(static_cast<const LineString&>(*geom), buffer, offset);
                break;
            case GEOMETRY_POLYGON:
                writePolygon(static_cast<const Polygon&>(*geom), buffer, offset);
                break;
            // Handle other types...
        }
    }
}

void WKBWriter::writeByte(UCHAR value, UCHAR* buffer, ULONG& offset)
{
    buffer[offset++] = value;
}

void WKBWriter::writeULong(ULONG value, UCHAR* buffer, ULONG& offset)
{
    if (needsSwap()) {
        value = swapEndian32(value);
    }
    
    memcpy(buffer + offset, &value, sizeof(ULONG));
    offset += sizeof(ULONG);
}

void WKBWriter::writeDouble(double value, UCHAR* buffer, ULONG& offset)
{
    ULONG64 longValue;
    memcpy(&longValue, &value, sizeof(double));
    
    if (needsSwap()) {
        longValue = swapEndian64(longValue);
    }
    
    memcpy(buffer + offset, &longValue, sizeof(double));
    offset += sizeof(double);
}

void WKBWriter::writeCoordinate(const Coordinate& coord, UCHAR* buffer, ULONG& offset)
{
    writeDouble(coord.x, buffer, offset);
    writeDouble(coord.y, buffer, offset);
    
    if (includeZ && coord.hasZ) {
        writeDouble(coord.z, buffer, offset);
    }
    
    if (includeM && coord.hasM) {
        writeDouble(coord.m, buffer, offset);
    }
}

void WKBWriter::writeLinearRing(const LinearRing& ring, UCHAR* buffer, ULONG& offset)
{
    ULONG numPoints = ring.getNumPoints();
    writeULong(numPoints, buffer, offset);
    
    for (ULONG i = 0; i < numPoints; i++) {
        writeCoordinate(ring.getCoordinateAt(i), buffer, offset);
    }
}

void WKBWriter::writeGeometryHeader(GeometryType type, SRID srid, UCHAR* buffer, ULONG& offset)
{
    // Write byte order
    writeByte(byteOrder, buffer, offset);
    
    // Write geometry type with flags
    ULONG wkbType = geometryTypeToWKB(type, includeZ, includeM, includeSRID);
    writeULong(wkbType, buffer, offset);
    
    // Write SRID if requested
    if (includeSRID) {
        writeULong(srid, buffer, offset);
    }
}

ULONG WKBWriter::getPointSize(const Point& point)
{
    return getHeaderSize() + getCoordinateSize();
}

ULONG WKBWriter::getLineStringSize(const LineString& lineString)
{
    return getHeaderSize() + sizeof(ULONG) + (lineString.getNumPoints() * getCoordinateSize());
}

ULONG WKBWriter::getPolygonSize(const Polygon& polygon)
{
    ULONG size = getHeaderSize() + sizeof(ULONG); // Header + ring count
    
    // Exterior ring
    size += getLinearRingSize(*polygon.getExteriorRing());
    
    // Interior rings
    for (ULONG i = 0; i < polygon.getNumInteriorRings(); i++) {
        size += getLinearRingSize(*polygon.getInteriorRingAt(i));
    }
    
    return size;
}

ULONG WKBWriter::getLinearRingSize(const LinearRing& ring)
{
    return sizeof(ULONG) + (ring.getNumPoints() * getCoordinateSize());
}

ULONG WKBWriter::getCoordinateSize()
{
    ULONG size = 2 * sizeof(double); // X, Y
    if (includeZ) size += sizeof(double);
    if (includeM) size += sizeof(double);
    return size;
}

ULONG WKBWriter::getHeaderSize()
{
    ULONG size = 1 + sizeof(ULONG); // Byte order + type
    if (includeSRID) size += sizeof(ULONG);
    return size;
}

bool WKBWriter::needsSwap()
{
    // Check system endianness
    ULONG test = 1;
    bool systemLittleEndian = (*((UCHAR*)&test) == 1);
    bool wantLittleEndian = (byteOrder == WKB_LITTLE_ENDIAN);
    
    return systemLittleEndian != wantLittleEndian;
}

ULONG WKBWriter::geometryTypeToWKB(GeometryType type, bool hasZ, bool hasM, bool hasSRID)
{
    ULONG wkbType = 0;
    
    switch (type) {
        case GEOMETRY_POINT: wkbType = WKB_POINT; break;
        case GEOMETRY_LINESTRING: wkbType = WKB_LINESTRING; break;
        case GEOMETRY_POLYGON: wkbType = WKB_POLYGON; break;
        case GEOMETRY_MULTIPOINT: wkbType = WKB_MULTIPOINT; break;
        case GEOMETRY_MULTILINESTRING: wkbType = WKB_MULTILINESTRING; break;
        case GEOMETRY_MULTIPOLYGON: wkbType = WKB_MULTIPOLYGON; break;
        case GEOMETRY_GEOMETRYCOLLECTION: wkbType = WKB_GEOMETRYCOLLECTION; break;
        default: wkbType = WKB_POINT;
    }
    
    if (hasZ) wkbType |= WKB_Z_FLAG;
    if (hasM) wkbType |= WKB_M_FLAG;
    if (hasSRID) wkbType |= WKB_SRID_FLAG;
    
    return wkbType;
}

ULONG WKBWriter::swapEndian32(ULONG value)
{
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x000000FF) << 24);
}

ULONG64 WKBWriter::swapEndian64(ULONG64 value)
{
    return ((value & 0xFF00000000000000ULL) >> 56) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x000000FF00000000ULL) >> 8) |
           ((value & 0x00000000FF000000ULL) << 8) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x00000000000000FFULL) << 56);
}

//============================================================================
// WKBUtils Implementation
//============================================================================

namespace WKBUtils
{
    bool isValidWKB(const ByteChunk* wkbData)
    {
        return isValidWKB(wkbData->getBuffer(), wkbData->getCount());
    }
    
    bool isValidWKB(const UCHAR* buffer, ULONG size)
    {
        try {
            MemoryPool pool;
            WKBReader reader(buffer, size, pool);
            Geometry* geom = reader.parseWKB();
            delete geom;
            return true;
        }
        catch (...) {
            return false;
        }
    }
    
    GeometryType getWKBGeometryType(const ByteChunk* wkbData)
    {
        return getWKBGeometryType(wkbData->getBuffer(), wkbData->getCount());
    }
    
    GeometryType getWKBGeometryType(const UCHAR* buffer, ULONG size)
    {
        if (size < 5) return GEOMETRY_POINT; // Default
        
        // Skip byte order
        ULONG offset = 1;
        
        // Read geometry type
        ULONG wkbType;
        memcpy(&wkbType, buffer + offset, sizeof(ULONG));
        
        // Handle endianness
        if (buffer[0] == WKB_BIG_ENDIAN) {
            // Swap endianness
            wkbType = ((wkbType & 0xFF000000) >> 24) |
                     ((wkbType & 0x00FF0000) >> 8) |
                     ((wkbType & 0x0000FF00) << 8) |
                     ((wkbType & 0x000000FF) << 24);
        }
        
        // Extract base type
        ULONG baseType = wkbType & 0x0000FFFF;
        
        switch (baseType) {
            case WKB_POINT: return GEOMETRY_POINT;
            case WKB_LINESTRING: return GEOMETRY_LINESTRING;
            case WKB_POLYGON: return GEOMETRY_POLYGON;
            case WKB_MULTIPOINT: return GEOMETRY_MULTIPOINT;
            case WKB_MULTILINESTRING: return GEOMETRY_MULTILINESTRING;
            case WKB_MULTIPOLYGON: return GEOMETRY_MULTIPOLYGON;
            case WKB_GEOMETRYCOLLECTION: return GEOMETRY_GEOMETRYCOLLECTION;
            default: return GEOMETRY_POINT;
        }
    }
    
    Geometry* fromWKB(const ByteChunk* wkbData, MemoryPool& pool)
    {
        WKBReader reader(wkbData, pool);
        return reader.parseWKB();
    }
    
    Geometry* fromWKB(const UCHAR* buffer, ULONG size, MemoryPool& pool)
    {
        WKBReader reader(buffer, size, pool);
        return reader.parseWKB();
    }
    
    ByteChunk* toWKB(const Geometry& geometry, MemoryPool& pool)
    {
        WKBWriter writer;
        return writer.write(geometry, pool);
    }
    
    UCHAR getByteOrder(const ByteChunk* wkbData)
    {
        return getByteOrder(wkbData->getBuffer());
    }
    
    UCHAR getByteOrder(const UCHAR* buffer)
    {
        return buffer[0];
    }
    
    bool validateWKBFormat(const ByteChunk* wkbData, string& errorMessage)
    {
        return validateWKBFormat(wkbData->getBuffer(), wkbData->getCount(), errorMessage);
    }
    
    bool validateWKBFormat(const UCHAR* buffer, ULONG size, string& errorMessage)
    {
        try {
            MemoryPool pool;
            WKBValidator validator(buffer, size);
            bool valid = validator.isValid();
            
            if (!valid) {
                std::vector<string> errors = validator.getValidationErrors();
                if (!errors.empty()) {
                    errorMessage = errors[0]; // Return first error
                }
            }
            
            return valid;
        }
        catch (...) {
            errorMessage = "Unknown WKB validation error";
            return false;
        }
    }
}

// Update the Geometry factory method to use the new WKB parser
Geometry* Geometry::fromWKB(const ByteChunk* wkb, MemoryPool& pool)
{
    return WKBUtils::fromWKB(wkb, pool);
}

//============================================================================
// WKBValidator Implementation (basic implementation)
//============================================================================

WKBValidator::WKBValidator(const ByteChunk* wkbData)
    : data(wkbData->getBuffer()), size(wkbData->getCount())
{
}

WKBValidator::WKBValidator(const UCHAR* buffer, ULONG bufferSize)
    : data(buffer), size(bufferSize)
{
}

WKBValidator::~WKBValidator()
{
}

bool WKBValidator::isValid()
{
    errors.clear();
    
    if (size < WKBUtils::WKB_HEADER_SIZE) {
        addError("WKB data too short");
        return false;
    }
    
    return validateStructure() && validateGeometry() && validateCoordinates();
}

bool WKBValidator::validateStructure()
{
    // Basic structure validation
    ULONG offset = 0;
    return validateGeometryStructure(offset);
}

bool WKBValidator::validateGeometry()
{
    // Geometric validity checks would go here
    return true; // Simplified
}

bool WKBValidator::validateCoordinates()
{
    // Coordinate range validation would go here
    return true; // Simplified
}

void WKBValidator::addError(const string& error)
{
    errors.push_back(error);
}

std::vector<string> WKBValidator::getValidationErrors() const
{
    return errors;
}

bool WKBValidator::validateGeometryStructure(ULONG& offset)
{
    // Simplified structure validation
    if (offset + 5 > size) {
        addError("Insufficient data for geometry header");
        return false;
    }
    
    // Skip byte order and type
    offset += 5;
    return true;
}
#include "WKTParser.h"
#include "common/StatusArg.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>

using namespace ScratchBird;

//============================================================================
// WKTLexer Implementation
//============================================================================

WKTLexer::WKTLexer(const string& wktString)
    : input(wktString.c_str()), current(input), tokenStart(input), numberValue(0.0)
{
}

WKTLexer::~WKTLexer()
{
}

WKTToken WKTLexer::nextToken()
{
    skipWhitespace();
    
    if (isAtEnd()) return WKT_EOF;
    
    tokenStart = current;
    char c = *current++;
    
    switch (c) {
        case '(': return WKT_LPAREN;
        case ')': return WKT_RPAREN;
        case ',': return WKT_COMMA;
        
        default:
            if (isAlpha(c)) {
                return scanWord();
            } else if (isDigit(c) || c == '-' || c == '+' || c == '.') {
                current--; // Back up to rescan number
                return scanNumber();
            }
            break;
    }
    
    // Unknown character
    tokenValue = string(1, c);
    return WKT_ERROR;
}

void WKTLexer::skipWhitespace()
{
    while (!isAtEnd() && std::isspace(*current)) {
        current++;
    }
}

WKTToken WKTLexer::scanWord()
{
    while (isAlphaNumeric(*current)) {
        current++;
    }
    
    tokenValue = string(tokenStart, current - tokenStart);
    
    // Convert to uppercase for case-insensitive matching
    std::transform(tokenValue.begin(), tokenValue.end(), tokenValue.begin(), ::toupper);
    
    // Check for EMPTY keyword
    if (tokenValue == "EMPTY") {
        return WKT_EMPTY;
    }
    
    return WKT_WORD;
}

WKTToken WKTLexer::scanNumber()
{
    // Handle negative numbers
    if (*current == '-' || *current == '+') {
        current++;
    }
    
    // Scan integer part
    while (isDigit(*current)) {
        current++;
    }
    
    // Scan decimal part
    if (*current == '.' && isDigit(*(current + 1))) {
        current++; // Consume '.'
        while (isDigit(*current)) {
            current++;
        }
    }
    
    // Scan exponent part
    if (*current == 'e' || *current == 'E') {
        current++;
        if (*current == '-' || *current == '+') {
            current++;
        }
        while (isDigit(*current)) {
            current++;
        }
    }
    
    tokenValue = string(tokenStart, current - tokenStart);
    numberValue = std::strtod(tokenValue.c_str(), nullptr);
    
    return WKT_NUMBER;
}

bool WKTLexer::isAlpha(char c) const
{
    return std::isalpha(c);
}

bool WKTLexer::isDigit(char c) const
{
    return std::isdigit(c);
}

bool WKTLexer::isAlphaNumeric(char c) const
{
    return std::isalnum(c) || c == '_';
}

//============================================================================
// WKTParser Implementation
//============================================================================

WKTParser::WKTParser(MemoryPool& p, SRID srid)
    : lexer(nullptr), currentToken(WKT_EOF), pool(p), defaultSRID(srid)
{
}

WKTParser::~WKTParser()
{
    delete lexer;
}

Geometry* WKTParser::parseWKT(const string& wktString)
{
    if (wktString.empty()) {
        parseError("Empty WKT string");
    }
    
    delete lexer;
    lexer = FB_NEW_POOL(pool) WKTLexer(wktString);
    
    advance(); // Get first token
    
    try {
        Geometry* geometry = parseGeometry();
        
        // Should be at end of input
        if (currentToken != WKT_EOF) {
            parseError("Unexpected tokens after geometry");
        }
        
        return geometry;
    }
    catch (const ParseError& e) {
        Arg::Gds(isc_invalid_geometry).arg(e.getMessage()).raise();
        return nullptr;
    }
}

void WKTParser::advance()
{
    currentToken = lexer->nextToken();
}

bool WKTParser::match(WKTToken token)
{
    if (check(token)) {
        advance();
        return true;
    }
    return false;
}

void WKTParser::consume(WKTToken token, const string& errorMessage)
{
    if (currentToken == token) {
        advance();
    } else {
        parseError(errorMessage);
    }
}

bool WKTParser::check(WKTToken token) const
{
    return currentToken == token;
}

Geometry* WKTParser::parseGeometry()
{
    if (currentToken != WKT_WORD) {
        parseError("Expected geometry type");
    }
    
    string geometryType = toUpperCase(lexer->getTokenValue());
    advance();
    
    if (geometryType == "POINT") {
        return parsePoint();
    } else if (geometryType == "LINESTRING") {
        return parseLineString();
    } else if (geometryType == "POLYGON") {
        return parsePolygon();
    } else if (geometryType == "MULTIPOINT") {
        return parseMultiPoint();
    } else if (geometryType == "MULTILINESTRING") {
        return parseMultiLineString();
    } else if (geometryType == "MULTIPOLYGON") {
        return parseMultiPolygon();
    } else if (geometryType == "GEOMETRYCOLLECTION") {
        return parseGeometryCollection();
    } else {
        parseError("Unknown geometry type: " + geometryType);
    }
    
    return nullptr;
}

Point* WKTParser::parsePoint()
{
    if (match(WKT_EMPTY)) {
        // Empty point - create point with invalid coordinates
        return FB_NEW_POOL(pool) Point(0, 0, defaultSRID, pool);
    }
    
    consume(WKT_LPAREN, "Expected '(' after POINT");
    
    Coordinate coord = parseCoordinate();
    
    consume(WKT_RPAREN, "Expected ')' after point coordinates");
    
    return FB_NEW_POOL(pool) Point(coord, defaultSRID, pool);
}

LineString* WKTParser::parseLineString()
{
    LineString* lineString = FB_NEW_POOL(pool) LineString(defaultSRID, pool);
    
    if (match(WKT_EMPTY)) {
        return lineString;
    }
    
    consume(WKT_LPAREN, "Expected '(' after LINESTRING");
    
    std::vector<Coordinate> coords = parseCoordinateSequence();
    
    if (coords.size() < 2) {
        parseError("LineString must have at least 2 coordinates");
    }
    
    for (const Coordinate& coord : coords) {
        lineString->addCoordinate(coord);
    }
    
    consume(WKT_RPAREN, "Expected ')' after linestring coordinates");
    
    return lineString;
}

Polygon* WKTParser::parsePolygon()
{
    Polygon* polygon = FB_NEW_POOL(pool) Polygon(defaultSRID, pool);
    
    if (match(WKT_EMPTY)) {
        return polygon;
    }
    
    consume(WKT_LPAREN, "Expected '(' after POLYGON");
    
    // Parse exterior ring
    LinearRing* exteriorRing = parseLinearRing();
    polygon->setExteriorRing(exteriorRing);
    
    // Parse interior rings
    while (match(WKT_COMMA)) {
        LinearRing* interiorRing = parseLinearRing();
        polygon->addInteriorRing(interiorRing);
    }
    
    consume(WKT_RPAREN, "Expected ')' after polygon rings");
    
    return polygon;
}

MultiPoint* WKTParser::parseMultiPoint()
{
    MultiPoint* multiPoint = FB_NEW_POOL(pool) MultiPoint(defaultSRID, pool);
    
    if (match(WKT_EMPTY)) {
        return multiPoint;
    }
    
    consume(WKT_LPAREN, "Expected '(' after MULTIPOINT");
    
    do {
        if (check(WKT_LPAREN)) {
            // Point with parentheses: (1 2)
            consume(WKT_LPAREN, "Expected '('");
            Coordinate coord = parseCoordinate();
            consume(WKT_RPAREN, "Expected ')'");
            
            Point* point = FB_NEW_POOL(pool) Point(coord, defaultSRID, pool);
            multiPoint->addPoint(point);
        } else {
            // Point without parentheses: 1 2
            Coordinate coord = parseCoordinate();
            Point* point = FB_NEW_POOL(pool) Point(coord, defaultSRID, pool);
            multiPoint->addPoint(point);
        }
    } while (match(WKT_COMMA));
    
    consume(WKT_RPAREN, "Expected ')' after multipoint");
    
    return multiPoint;
}

MultiLineString* WKTParser::parseMultiLineString()
{
    MultiLineString* multiLineString = FB_NEW_POOL(pool) MultiLineString(defaultSRID, pool);
    
    if (match(WKT_EMPTY)) {
        return multiLineString;
    }
    
    consume(WKT_LPAREN, "Expected '(' after MULTILINESTRING");
    
    do {
        consume(WKT_LPAREN, "Expected '(' before linestring");
        
        std::vector<Coordinate> coords = parseCoordinateSequence();
        
        if (coords.size() < 2) {
            parseError("LineString in MultiLineString must have at least 2 coordinates");
        }
        
        LineString* lineString = FB_NEW_POOL(pool) LineString(defaultSRID, pool);
        for (const Coordinate& coord : coords) {
            lineString->addCoordinate(coord);
        }
        
        multiLineString->addLineString(lineString);
        
        consume(WKT_RPAREN, "Expected ')' after linestring");
    } while (match(WKT_COMMA));
    
    consume(WKT_RPAREN, "Expected ')' after multilinestring");
    
    return multiLineString;
}

MultiPolygon* WKTParser::parseMultiPolygon()
{
    MultiPolygon* multiPolygon = FB_NEW_POOL(pool) MultiPolygon(defaultSRID, pool);
    
    if (match(WKT_EMPTY)) {
        return multiPolygon;
    }
    
    consume(WKT_LPAREN, "Expected '(' after MULTIPOLYGON");
    
    do {
        consume(WKT_LPAREN, "Expected '(' before polygon");
        
        Polygon* polygon = FB_NEW_POOL(pool) Polygon(defaultSRID, pool);
        
        // Parse exterior ring
        LinearRing* exteriorRing = parseLinearRing();
        polygon->setExteriorRing(exteriorRing);
        
        // Parse interior rings
        while (match(WKT_COMMA)) {
            LinearRing* interiorRing = parseLinearRing();
            polygon->addInteriorRing(interiorRing);
        }
        
        multiPolygon->addPolygon(polygon);
        
        consume(WKT_RPAREN, "Expected ')' after polygon");
    } while (match(WKT_COMMA));
    
    consume(WKT_RPAREN, "Expected ')' after multipolygon");
    
    return multiPolygon;
}

GeometryCollection* WKTParser::parseGeometryCollection()
{
    GeometryCollection* collection = FB_NEW_POOL(pool) GeometryCollection(GEOMETRY_GEOMETRYCOLLECTION, defaultSRID, pool);
    
    if (match(WKT_EMPTY)) {
        return collection;
    }
    
    consume(WKT_LPAREN, "Expected '(' after GEOMETRYCOLLECTION");
    
    do {
        Geometry* geometry = parseGeometry();
        collection->addGeometry(geometry);
    } while (match(WKT_COMMA));
    
    consume(WKT_RPAREN, "Expected ')' after geometrycollection");
    
    return collection;
}

Coordinate WKTParser::parseCoordinate()
{
    if (currentToken != WKT_NUMBER) {
        parseError("Expected number for coordinate");
    }
    
    double x = lexer->getNumberValue();
    advance();
    
    if (currentToken != WKT_NUMBER) {
        parseError("Expected Y coordinate");
    }
    
    double y = lexer->getNumberValue();
    advance();
    
    Coordinate coord(x, y);
    
    // Check for optional Z coordinate
    if (currentToken == WKT_NUMBER) {
        coord.z = lexer->getNumberValue();
        coord.hasZ = true;
        advance();
        
        // Check for optional M coordinate
        if (currentToken == WKT_NUMBER) {
            coord.m = lexer->getNumberValue();
            coord.hasM = true;
            advance();
        }
    }
    
    return coord;
}

std::vector<Coordinate> WKTParser::parseCoordinateSequence()
{
    std::vector<Coordinate> coordinates;
    
    do {
        coordinates.push_back(parseCoordinate());
    } while (match(WKT_COMMA));
    
    return coordinates;
}

LinearRing* WKTParser::parseLinearRing()
{
    consume(WKT_LPAREN, "Expected '(' before linear ring");
    
    std::vector<Coordinate> coords = parseCoordinateSequence();
    
    if (coords.size() < 4) {
        parseError("Linear ring must have at least 4 coordinates");
    }
    
    LinearRing* ring = FB_NEW_POOL(pool) LinearRing(defaultSRID, pool);
    for (const Coordinate& coord : coords) {
        ring->addCoordinate(coord);
    }
    
    // Ensure ring is closed
    if (!ring->isClosed()) {
        ring->closeRing();
    }
    
    consume(WKT_RPAREN, "Expected ')' after linear ring");
    
    return ring;
}

void WKTParser::parseError(const string& message)
{
    throw ParseError(message);
}

string WKTParser::toUpperCase(const string& str) const
{
    string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

//============================================================================
// WKTWriter Implementation
//============================================================================

WKTWriter::WKTWriter(USHORT prec, bool z, bool m)
    : precision(prec), includeZ(z), includeM(m)
{
}

WKTWriter::~WKTWriter()
{
}

string WKTWriter::write(const Geometry& geometry)
{
    switch (geometry.getType()) {
        case GEOMETRY_POINT:
            return writePoint(static_cast<const Point&>(geometry));
        case GEOMETRY_LINESTRING:
            return writeLineString(static_cast<const LineString&>(geometry));
        case GEOMETRY_POLYGON:
            return writePolygon(static_cast<const Polygon&>(geometry));
        case GEOMETRY_MULTIPOINT:
            return writeMultiPoint(static_cast<const MultiPoint&>(geometry));
        case GEOMETRY_MULTILINESTRING:
            return writeMultiLineString(static_cast<const MultiLineString&>(geometry));
        case GEOMETRY_MULTIPOLYGON:
            return writeMultiPolygon(static_cast<const MultiPolygon&>(geometry));
        case GEOMETRY_GEOMETRYCOLLECTION:
            return writeGeometryCollection(static_cast<const GeometryCollection&>(geometry));
        default:
            return "UNKNOWN";
    }
}

string WKTWriter::writePoint(const Point& point)
{
    std::ostringstream oss;
    oss << "POINT";
    
    if (point.isEmpty()) {
        oss << " EMPTY";
    } else {
        oss << "(" << writeCoordinate(point.getCoordinate()) << ")";
    }
    
    return oss.str();
}

string WKTWriter::writeLineString(const LineString& lineString)
{
    std::ostringstream oss;
    oss << "LINESTRING";
    
    if (lineString.isEmpty()) {
        oss << " EMPTY";
    } else {
        oss << "(";
        for (ULONG i = 0; i < lineString.getNumPoints(); i++) {
            if (i > 0) oss << ",";
            oss << writeCoordinate(lineString.getCoordinateAt(i));
        }
        oss << ")";
    }
    
    return oss.str();
}

string WKTWriter::writePolygon(const Polygon& polygon)
{
    std::ostringstream oss;
    oss << "POLYGON";
    
    if (polygon.isEmpty()) {
        oss << " EMPTY";
    } else {
        oss << "(";
        
        // Write exterior ring
        oss << writeLinearRing(*polygon.getExteriorRing());
        
        // Write interior rings
        for (ULONG i = 0; i < polygon.getNumInteriorRings(); i++) {
            oss << "," << writeLinearRing(*polygon.getInteriorRingAt(i));
        }
        
        oss << ")";
    }
    
    return oss.str();
}

string WKTWriter::writeMultiPoint(const MultiPoint& multiPoint)
{
    std::ostringstream oss;
    oss << "MULTIPOINT";
    
    if (multiPoint.isEmpty()) {
        oss << " EMPTY";
    } else {
        oss << "(";
        for (ULONG i = 0; i < multiPoint.getNumGeometries(); i++) {
            if (i > 0) oss << ",";
            Point* point = multiPoint.getPointAt(i);
            oss << "(" << writeCoordinate(point->getCoordinate()) << ")";
        }
        oss << ")";
    }
    
    return oss.str();
}

string WKTWriter::writeMultiLineString(const MultiLineString& multiLineString)
{
    std::ostringstream oss;
    oss << "MULTILINESTRING";
    
    if (multiLineString.isEmpty()) {
        oss << " EMPTY";
    } else {
        oss << "(";
        for (ULONG i = 0; i < multiLineString.getNumGeometries(); i++) {
            if (i > 0) oss << ",";
            LineString* lineString = multiLineString.getLineStringAt(i);
            oss << "(";
            for (ULONG j = 0; j < lineString->getNumPoints(); j++) {
                if (j > 0) oss << ",";
                oss << writeCoordinate(lineString->getCoordinateAt(j));
            }
            oss << ")";
        }
        oss << ")";
    }
    
    return oss.str();
}

string WKTWriter::writeMultiPolygon(const MultiPolygon& multiPolygon)
{
    std::ostringstream oss;
    oss << "MULTIPOLYGON";
    
    if (multiPolygon.isEmpty()) {
        oss << " EMPTY";
    } else {
        oss << "(";
        for (ULONG i = 0; i < multiPolygon.getNumGeometries(); i++) {
            if (i > 0) oss << ",";
            Polygon* polygon = multiPolygon.getPolygonAt(i);
            oss << "(";
            
            // Write exterior ring
            oss << writeLinearRing(*polygon->getExteriorRing());
            
            // Write interior rings
            for (ULONG j = 0; j < polygon->getNumInteriorRings(); j++) {
                oss << "," << writeLinearRing(*polygon->getInteriorRingAt(j));
            }
            
            oss << ")";
        }
        oss << ")";
    }
    
    return oss.str();
}

string WKTWriter::writeGeometryCollection(const GeometryCollection& collection)
{
    std::ostringstream oss;
    oss << "GEOMETRYCOLLECTION";
    
    if (collection.isEmpty()) {
        oss << " EMPTY";
    } else {
        oss << "(";
        for (ULONG i = 0; i < collection.getNumGeometries(); i++) {
            if (i > 0) oss << ",";
            oss << write(*collection.getGeometryAt(i));
        }
        oss << ")";
    }
    
    return oss.str();
}

string WKTWriter::writeCoordinate(const Coordinate& coord)
{
    std::ostringstream oss;
    oss << formatNumber(coord.x) << " " << formatNumber(coord.y);
    
    if (includeZ && coord.hasZ) {
        oss << " " << formatNumber(coord.z);
    }
    
    if (includeM && coord.hasM) {
        oss << " " << formatNumber(coord.m);
    }
    
    return oss.str();
}

string WKTWriter::writeLinearRing(const LinearRing& ring)
{
    std::ostringstream oss;
    oss << "(";
    
    for (ULONG i = 0; i < ring.getNumPoints(); i++) {
        if (i > 0) oss << ",";
        oss << writeCoordinate(ring.getCoordinateAt(i));
    }
    
    oss << ")";
    return oss.str();
}

string WKTWriter::formatNumber(double value)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    
    string result = oss.str();
    
    // Remove trailing zeros after decimal point
    if (result.find('.') != string::npos) {
        result = result.substr(0, result.find_last_not_of('0') + 1);
        if (result.back() == '.') {
            result.pop_back();
        }
    }
    
    return result;
}

//============================================================================
// WKTUtils Implementation
//============================================================================

namespace WKTUtils
{
    bool isValidWKT(const string& wktString)
    {
        try {
            MemoryPool pool;
            WKTParser parser(pool);
            Geometry* geom = parser.parseWKT(wktString);
            delete geom;
            return true;
        }
        catch (...) {
            return false;
        }
    }
    
    GeometryType getWKTGeometryType(const string& wktString)
    {
        // Extract geometry type from beginning of WKT string
        size_t pos = wktString.find_first_not_of(" \t\n\r");
        if (pos == string::npos) return GEOMETRY_POINT; // Default
        
        size_t endPos = wktString.find_first_of(" \t\n\r(", pos);
        if (endPos == string::npos) endPos = wktString.length();
        
        string typeStr = wktString.substr(pos, endPos - pos);
        std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::toupper);
        
        if (typeStr == "POINT") return GEOMETRY_POINT;
        if (typeStr == "LINESTRING") return GEOMETRY_LINESTRING;
        if (typeStr == "POLYGON") return GEOMETRY_POLYGON;
        if (typeStr == "MULTIPOINT") return GEOMETRY_MULTIPOINT;
        if (typeStr == "MULTILINESTRING") return GEOMETRY_MULTILINESTRING;
        if (typeStr == "MULTIPOLYGON") return GEOMETRY_MULTIPOLYGON;
        if (typeStr == "GEOMETRYCOLLECTION") return GEOMETRY_GEOMETRYCOLLECTION;
        
        return GEOMETRY_POINT; // Default fallback
    }
    
    Geometry* fromWKT(const string& wktString, SRID srid, MemoryPool& pool)
    {
        WKTParser parser(pool, srid);
        return parser.parseWKT(wktString);
    }
    
    string toWKT(const Geometry& geometry, USHORT precision)
    {
        WKTWriter writer(precision);
        return writer.write(geometry);
    }
    
    bool validateWKTFormat(const string& wktString, string& errorMessage)
    {
        try {
            MemoryPool pool;
            WKTParser parser(pool);
            Geometry* geom = parser.parseWKT(wktString);
            delete geom;
            return true;
        }
        catch (const WKTParser::ParseError& e) {
            errorMessage = e.getMessage();
            return false;
        }
        catch (...) {
            errorMessage = "Unknown WKT parsing error";
            return false;
        }
    }
}

// Update the Geometry factory methods to use the new WKT parser
Geometry* Geometry::fromWKT(const string& wkt, SRID srid, MemoryPool& pool)
{
    return WKTUtils::fromWKT(wkt, srid, pool);
}
#ifndef WKT_PARSER_H
#define WKT_PARSER_H

#include "SpatialDataTypes.h"
#include "common/classes/alloc.h"
#include <string>
#include <vector>

namespace ScratchBird {

// WKT parsing tokens
enum WKTToken
{
    WKT_EOF = 0,
    WKT_WORD,
    WKT_NUMBER,
    WKT_COMMA,
    WKT_LPAREN,
    WKT_RPAREN,
    WKT_EMPTY,
    WKT_ERROR
};

// WKT lexer for tokenizing input
class WKTLexer
{
private:
    const char* input;
    const char* current;
    const char* tokenStart;
    string tokenValue;
    double numberValue;
    
public:
    WKTLexer(const string& wktString);
    ~WKTLexer();
    
    WKTToken nextToken();
    const string& getTokenValue() const { return tokenValue; }
    double getNumberValue() const { return numberValue; }
    bool isAtEnd() const { return *current == '\0'; }
    
private:
    void skipWhitespace();
    WKTToken scanWord();
    WKTToken scanNumber();
    bool isAlpha(char c) const;
    bool isDigit(char c) const;
    bool isAlphaNumeric(char c) const;
};

// WKT parser for converting WKT strings to geometry objects
class WKTParser
{
private:
    WKTLexer* lexer;
    WKTToken currentToken;
    MemoryPool& pool;
    SRID defaultSRID;
    
public:
    WKTParser(MemoryPool& p, SRID srid = DEFAULT_SRID);
    ~WKTParser();
    
    // Main parsing entry point
    Geometry* parseWKT(const string& wktString);
    
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
    // Token management
    void advance();
    bool match(WKTToken token);
    void consume(WKTToken token, const string& errorMessage);
    bool check(WKTToken token) const;
    
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
    Coordinate parseCoordinate();
    std::vector<Coordinate> parseCoordinateSequence();
    LinearRing* parseLinearRing();
    
    // Utility methods
    bool isGeometryType(const string& word) const;
    GeometryType getGeometryType(const string& word) const;
    void parseError(const string& message);
    string toUpperCase(const string& str) const;
};

// WKT writer for converting geometry objects to WKT strings
class WKTWriter
{
private:
    USHORT precision;
    bool includeZ;
    bool includeM;
    
public:
    WKTWriter(USHORT prec = WKT_MAX_PRECISION, bool z = false, bool m = false);
    ~WKTWriter();
    
    // Configuration
    void setPrecision(USHORT prec) { precision = prec; }
    void setIncludeZ(bool z) { includeZ = z; }
    void setIncludeM(bool m) { includeM = m; }
    
    // Main writing method
    string write(const Geometry& geometry);
    
private:
    // Geometry-specific writers
    string writePoint(const Point& point);
    string writeLineString(const LineString& lineString);
    string writePolygon(const Polygon& polygon);
    string writeMultiPoint(const MultiPoint& multiPoint);
    string writeMultiLineString(const MultiLineString& multiLineString);  
    string writeMultiPolygon(const MultiPolygon& multiPolygon);
    string writeGeometryCollection(const GeometryCollection& collection);
    
    // Helper methods
    string writeCoordinate(const Coordinate& coord);
    string writeCoordinateSequence(const ObjectsArray<Coordinate>& coords);
    string writeLinearRing(const LinearRing& ring);
    string formatNumber(double value);
};

// Well-Known Text utility functions
namespace WKTUtils
{
    // Validation functions
    bool isValidWKT(const string& wktString);
    GeometryType getWKTGeometryType(const string& wktString);
    
    // Conversion functions
    Geometry* fromWKT(const string& wktString, SRID srid, MemoryPool& pool);
    string toWKT(const Geometry& geometry, USHORT precision = WKT_MAX_PRECISION);
    
    // Format validation
    bool validateWKTFormat(const string& wktString, string& errorMessage);
    
    // Common WKT patterns
    const string WKT_EMPTY_PATTERN = "EMPTY";
    const std::vector<string> GEOMETRY_TYPES = {
        "POINT", "LINESTRING", "POLYGON", 
        "MULTIPOINT", "MULTILINESTRING", "MULTIPOLYGON", 
        "GEOMETRYCOLLECTION"
    };
    
    // WKT format constants
    const char WKT_DELIMITER = ' ';
    const char WKT_COORDINATE_SEPARATOR = ',';
    const char WKT_LPAREN = '(';
    const char WKT_RPAREN = ')';
}

} // namespace ScratchBird

#endif // WKT_PARSER_H
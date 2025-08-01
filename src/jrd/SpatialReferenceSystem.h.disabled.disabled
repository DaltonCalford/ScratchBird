#ifndef SPATIAL_REFERENCE_SYSTEM_H
#define SPATIAL_REFERENCE_SYSTEM_H

#include "SpatialDataTypes.h"
#include "common/classes/alloc.h"
#include "common/classes/array.h"
#include "common/classes/objects_array.h"
#include <map>
#include <string>
#include <vector>

namespace ScratchBird {

// Forward declarations
class CoordinateTransformation;
class ProjectionParameters;

// Spatial Reference System types
enum SRSType : UCHAR
{
    SRS_UNKNOWN = 0,
    SRS_GEOGRAPHIC = 1,     // Geographic (latitude/longitude)
    SRS_PROJECTED = 2,      // Projected coordinate system
    SRS_GEOCENTRIC = 3,     // Geocentric coordinate system
    SRS_COMPOUND = 4,       // Compound coordinate system
    SRS_ENGINEERING = 5,    // Engineering/local coordinate system
    SRS_VERTICAL = 6        // Vertical coordinate system
};

// Coordinate system axis information
struct AxisInfo
{
    string name;           // Axis name (e.g., "Longitude", "Latitude", "X", "Y")
    string abbreviation;   // Axis abbreviation (e.g., "Lon", "Lat", "X", "Y")  
    string direction;      // Axis direction (e.g., "EAST", "NORTH", "UP", "DOWN")
    string units;          // Units (e.g., "degree", "metre", "foot")
    double unitsPerMeter;  // Conversion factor to meters
    
    AxisInfo() : unitsPerMeter(1.0) {}
    AxisInfo(const string& n, const string& abbr, const string& dir, const string& u, double upm = 1.0)
        : name(n), abbreviation(abbr), direction(dir), units(u), unitsPerMeter(upm) {}
};

// Datum information
struct DatumInfo
{
    string name;           // Datum name (e.g., "World Geodetic System 1984")
    string spheroidName;   // Spheroid name (e.g., "WGS 84")
    double semiMajorAxis;  // Semi-major axis in meters
    double flattening;     // Flattening ratio
    double inverseFlattening; // Inverse flattening
    
    // Transformation parameters (for datum shifts)
    double dx, dy, dz;     // Translation parameters
    double rx, ry, rz;     // Rotation parameters (arc seconds)
    double scale;          // Scale factor (ppm)
    
    DatumInfo() : semiMajorAxis(0), flattening(0), inverseFlattening(0),
                  dx(0), dy(0), dz(0), rx(0), ry(0), rz(0), scale(0) {}
};

// Projection parameters for projected coordinate systems
struct ProjectionInfo
{
    string name;                    // Projection name (e.g., "Transverse Mercator")
    string method;                  // Projection method
    std::map<string, double> parameters; // Projection parameters
    
    // Common projection parameters
    double centralMeridian;         // Central meridian
    double latitudeOfOrigin;        // Latitude of origin
    double standardParallel1;       // First standard parallel
    double standardParallel2;       // Second standard parallel
    double falseEasting;            // False easting
    double falseNorthing;           // False northing
    double scaleFactor;             // Scale factor
    
    ProjectionInfo() : centralMeridian(0), latitudeOfOrigin(0),
                      standardParallel1(0), standardParallel2(0),
                      falseEasting(0), falseNorthing(0), scaleFactor(1.0) {}
};

// Complete Spatial Reference System definition
class SpatialReferenceSystem
{
private:
    SRID srid;
    string name;
    string authorityName;       // Authority name (e.g., "EPSG")
    string authorityCode;       // Authority code (e.g., "4326")
    SRSType type;
    
    DatumInfo datum;
    ProjectionInfo projection;
    ObjectsArray<AxisInfo> axes;
    
    string wktDefinition;       // Well-Known Text definition
    string proj4Definition;     // PROJ.4 definition string
    
    mutable CoordinateTransformation* cachedTransformation;
    MemoryPool& pool;
    
public:
    SpatialReferenceSystem(SRID id, MemoryPool& p);
    SpatialReferenceSystem(SRID id, const string& wkt, MemoryPool& p);
    ~SpatialReferenceSystem();
    
    // Basic properties
    SRID getSRID() const { return srid; }
    const string& getName() const { return name; }
    const string& getAuthorityName() const { return authorityName; }
    const string& getAuthorityCode() const { return authorityCode; }
    SRSType getType() const { return type; }
    
    void setName(const string& n) { name = n; }
    void setAuthority(const string& authName, const string& authCode) {
        authorityName = authName;
        authorityCode = authCode;
    }
    void setType(SRSType t) { type = t; }
    
    // Datum access
    const DatumInfo& getDatum() const { return datum; }
    DatumInfo& getDatum() { return datum; }
    void setDatum(const DatumInfo& d) { datum = d; }
    
    // Projection access
    const ProjectionInfo& getProjection() const { return projection; }
    ProjectionInfo& getProjection() { return projection; }
    void setProjection(const ProjectionInfo& p) { projection = p; }
    
    // Axis management
    ULONG getAxisCount() const { return axes.getCount(); }
    const AxisInfo& getAxis(ULONG index) const;
    void addAxis(const AxisInfo& axis);
    void setAxis(ULONG index, const AxisInfo& axis);
    
    // Definition strings
    const string& getWKT() const { return wktDefinition; }
    const string& getProj4() const { return proj4Definition; }
    void setWKT(const string& wkt) { wktDefinition = wkt; }
    void setProj4(const string& proj4) { proj4Definition = proj4; }
    
    // Coordinate system properties
    bool isGeographic() const { return type == SRS_GEOGRAPHIC; }
    bool isProjected() const { return type == SRS_PROJECTED; }
    bool isGeocentric() const { return type == SRS_GEOCENTRIC; }
    bool hasZ() const { return axes.getCount() > 2; }
    bool hasM() const { return axes.getCount() > 3; }
    
    // Unit conversions
    double convertToMeters(double value, ULONG axisIndex = 0) const;
    double convertFromMeters(double value, ULONG axisIndex = 0) const;
    string getUnits(ULONG axisIndex = 0) const;
    
    // Coordinate validation
    bool isValidCoordinate(const Coordinate& coord) const;
    bool isValidCoordinate(double x, double y) const;
    MBR getValidCoordinateRange() const;
    
    // Transformation support
    CoordinateTransformation* getTransformationTo(const SpatialReferenceSystem& target) const;
    Coordinate transformCoordinate(const Coordinate& coord, const SpatialReferenceSystem& target) const;
    MBR transformMBR(const MBR& mbr, const SpatialReferenceSystem& target) const;
    
    // Comparison and equality
    bool equals(const SpatialReferenceSystem& other) const;
    bool isCompatible(const SpatialReferenceSystem& other) const;
    
    // Serialization
    string toWKT() const;
    string toProj4() const;
    ByteChunk* toBinary(MemoryPool& pool) const;
    
    // Factory methods
    static SpatialReferenceSystem* fromWKT(const string& wkt, MemoryPool& pool);
    static SpatialReferenceSystem* fromProj4(const string& proj4, MemoryPool& pool);
    static SpatialReferenceSystem* fromEPSG(ULONG epsgCode, MemoryPool& pool);
    static SpatialReferenceSystem* fromSRID(SRID srid, MemoryPool& pool);
};

// Coordinate transformation between spatial reference systems
class CoordinateTransformation
{
private:
    const SpatialReferenceSystem& sourceSRS;
    const SpatialReferenceSystem& targetSRS;
    MemoryPool& pool;
    
    // Transformation pipeline components
    bool needsDatumShift;
    bool needsProjection;
    bool needsInverseProjection;
    bool needsUnitConversion;
    
    // Cached transformation parameters
    mutable DatumInfo sourceDatum;
    mutable DatumInfo targetDatum;
    mutable ProjectionInfo sourceProjection;
    mutable ProjectionInfo targetProjection;
    
public:
    CoordinateTransformation(const SpatialReferenceSystem& source, 
                           const SpatialReferenceSystem& target,
                           MemoryPool& p);
    ~CoordinateTransformation();
    
    // Transformation operations
    Coordinate transform(const Coordinate& coord) const;
    MBR transform(const MBR& mbr) const;
    std::vector<Coordinate> transform(const std::vector<Coordinate>& coords) const;
    
    // Inverse transformation
    Coordinate inverseTransform(const Coordinate& coord) const;
    MBR inverseTransform(const MBR& mbr) const;
    
    // Transformation properties
    bool isIdentity() const;
    bool isReversible() const;
    double getAccuracy() const; // Estimated accuracy in meters
    
    // Error handling
    class TransformationError
    {
    private:
        string message;
        
    public:
        TransformationError(const string& msg) : message(msg) {}
        const string& getMessage() const { return message; }
    };
    
private:
    // Transformation pipeline methods
    Coordinate applyDatumShift(const Coordinate& coord, bool forward) const;
    Coordinate applyProjection(const Coordinate& coord, const ProjectionInfo& proj, bool forward) const;
    Coordinate applyUnitConversion(const Coordinate& coord, const AxisInfo& axis, bool forward) const;
    
    // Specific projection implementations
    Coordinate mercatorProjection(const Coordinate& coord, const ProjectionInfo& proj, bool forward) const;
    Coordinate transverseMercatorProjection(const Coordinate& coord, const ProjectionInfo& proj, bool forward) const;
    Coordinate lambertConformalConicProjection(const Coordinate& coord, const ProjectionInfo& proj, bool forward) const;
    Coordinate alambertEqualAreaProjection(const Coordinate& coord, const ProjectionInfo& proj, bool forward) const;
    
    // Datum transformation methods
    Coordinate helmertTransformation(const Coordinate& coord, const DatumInfo& source, const DatumInfo& target) const;
    Coordinate geographicToGeocentric(const Coordinate& coord, const DatumInfo& datum) const;
    Coordinate geocentricToGeographic(const Coordinate& coord, const DatumInfo& datum) const;
};

// Global spatial reference system registry
class SRSRegistry
{
private:
    static SRSRegistry* instance;
    std::map<SRID, SpatialReferenceSystem*> registry;
    MemoryPool& pool;
    
    SRSRegistry(MemoryPool& p);
    
public:
    static SRSRegistry* getInstance(MemoryPool& pool);
    ~SRSRegistry();
    
    // Registry management
    void registerSRS(SpatialReferenceSystem* srs);
    SpatialReferenceSystem* getSRS(SRID srid) const;
    bool isRegistered(SRID srid) const;
    void unregisterSRS(SRID srid);
    void clear();
    
    // Standard SRS initialization
    void initializeStandardSRS();
    void loadFromDatabase();
    void saveToDatabase() const;
    
    // Registry queries
    std::vector<SRID> getAllSRIDs() const;
    std::vector<SpatialReferenceSystem*> findByAuthority(const string& authority) const;
    std::vector<SpatialReferenceSystem*> findByType(SRSType type) const;
    
    // Factory methods with caching
    SpatialReferenceSystem* getOrCreateFromWKT(const string& wkt);
    SpatialReferenceSystem* getOrCreateFromEPSG(ULONG epsgCode);
    
    // Statistics
    ULONG getRegistrySize() const { return registry.size(); }
    
private:
    void createWGS84();
    void createWebMercator();
    void createUTMZones();
    void createCommonProjections();
    
    SRID generateSRID();
};

// Predefined SRID constants
namespace WellKnownSRIDs
{
    const SRID UNDEFINED = 0;
    const SRID WGS84 = 4326;              // WGS 84 Geographic
    const SRID WGS84_PSEUDO_MERCATOR = 3857; // Web Mercator
    const SRID WGS84_UTM_ZONE_1N = 32601;  // UTM Zone 1N
    const SRID NAD83 = 4269;               // NAD83 Geographic
    const SRID ETRS89 = 4258;              // ETRS89 Geographic
    const SRID GDA94 = 4283;               // GDA94 Geographic
    
    // Common projected systems
    const SRID BRITISH_NATIONAL_GRID = 27700;
    const SRID LAMBERT_93 = 2154;          // RGF93 Lambert 93
    const SRID ALBERS_EQUAL_AREA = 5070;   // NAD83 Albers Equal Area
}

// Utility functions for spatial reference systems
namespace SRSUtils
{
    // EPSG code parsing
    bool isValidEPSGCode(ULONG code);
    SRID epsgToSRID(ULONG epsgCode);
    ULONG sridToEPSG(SRID srid);
    
    // Authority string parsing
    std::pair<string, string> parseAuthorityString(const string& authority);
    string formatAuthorityString(const string& name, const string& code);
    
    // Well-Known Text parsing utilities
    bool isValidWKT(const string& wkt);
    string extractSRSName(const string& wkt);
    string extractAuthority(const string& wkt);
    SRSType determineSRSType(const string& wkt);
    
    // PROJ.4 string utilities
    bool isValidProj4(const string& proj4);
    std::map<string, string> parseProj4Parameters(const string& proj4);
    string formatProj4String(const std::map<string, string>& parameters);
    
    // Unit conversion utilities
    double degreesToRadians(double degrees);
    double radiansToDegrees(double radians);
    double metersToFeet(double meters);
    double feetToMeters(double feet);
    
    // Coordinate validation
    bool isValidLatitude(double latitude);
    bool isValidLongitude(double longitude);
    bool isValidGeographicCoordinate(double longitude, double latitude);
    
    // Distance and area calculations with SRS awareness
    double calculateDistance(const Coordinate& coord1, const Coordinate& coord2, const SpatialReferenceSystem& srs);
    double calculateArea(const std::vector<Coordinate>& polygon, const SpatialReferenceSystem& srs);
    double calculateLength(const std::vector<Coordinate>& linestring, const SpatialReferenceSystem& srs);
    
    // Coordinate system analysis
    bool isConformal(const SpatialReferenceSystem& srs);
    bool isEqualArea(const SpatialReferenceSystem& srs);
    bool isEquidistant(const SpatialReferenceSystem& srs);
    double getLinearUnitsPerDegree(const SpatialReferenceSystem& srs, const Coordinate& location);
    
    // Best practices and recommendations
    SpatialReferenceSystem* recommendSRSForRegion(const MBR& region, MemoryPool& pool);
    std::vector<SRID> getSuitableSRSForApplication(const string& application, const MBR& region);
    string getSRSUsageAdvice(const SpatialReferenceSystem& srs);
}

// Error handling for spatial reference operations
class SRSError
{
private:
    string message;
    SRID srid;
    
public:
    SRSError(const string& msg, SRID id = 0) : message(msg), srid(id) {}
    
    const string& getMessage() const { return message; }
    SRID getSRID() const { return srid; }
};

} // namespace ScratchBird

#endif // SPATIAL_REFERENCE_SYSTEM_H
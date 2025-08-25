# Phase 17 — JSON, Spatial, and Collations: Detailed Implementation Plan

## Overview

Phase 17 focuses on advanced data types and internationalization support to make ScratchBird a more complete database system. This includes JSON/JSONB for semi-structured data, spatial data types with PostGIS-like functionality, and proper collation support via ICU for international text handling.

## Goals and Scope

### Primary Objectives
- Implement JSON and JSONB data types with full operator support
- Add spatial data types and geometric operations
- Integrate ICU for deterministic collations and internationalization
- Provide seamless integration with existing SQL executor and optimizer

### Success Criteria
- JSON and spatial query suites pass comprehensive tests
- Collation rules are consistent across all text operations
- Performance is acceptable for typical workloads
- Integration with existing features (indexes, constraints, etc.) works correctly

## Detailed Implementation Plan

### 1. JSON and JSONB Data Types

#### 1.1 Storage and Representation
- **JSON Type**: Text-based storage with validation on input/output
- **JSONB Type**: Binary format for efficient access and indexing
- **Compression**: Optional compression for JSONB to save space
- **Toast Support**: Large JSON values stored in overflow pages

#### 1.2 JSON Operators and Functions
```
JSON Operators:
->          Get JSON object field by key
->>         Get JSON object field as text
#>          Get JSON object at specified path
#>>         Get JSON object at path as text
?           Key exists in JSON object
?|          Any of keys exist
?&          All keys exist
@>          JSON contains other JSON
<@          JSON is contained by other JSON

JSON Functions:
json_typeof()           Get type of JSON value
json_build_object()     Build JSON object from key-value pairs
json_build_array()      Build JSON array from values
json_object_keys()      Get keys from JSON object
json_array_length()     Get length of JSON array
json_extract_path()     Extract by path
json_set()              Set value at path
json_insert()           Insert value at path
json_delete()           Delete value at path
```

#### 1.3 JSON Path Support
- SQL/JSON path expressions for complex queries
- Support for `.key`, `[index]`, `.*`, `[*]` syntax
- Integration with WHERE clauses and projections

#### 1.4 Indexing Support
- GIN indexes for JSONB with path-specific operators
- B-Tree indexes on JSON scalar values
- Expression indexes for computed JSON values

### 2. Spatial Data Types and Operations

#### 2.1 Core Spatial Types
```
POINT       (x, y)
LINESTRING  Sequence of connected points
POLYGON     Closed linestring with interior
MULTIPOINT  Collection of points
MULTILINESTRING Collection of linestrings
MULTIPOLYGON     Collection of polygons
GEOMETRYCOLLECTION Collection of geometries
```

#### 2.2 Spatial Reference Systems (SRS)
- EPSG code support for coordinate systems
- Built-in common SRS (WGS84, Web Mercator, etc.)
- SRID validation and transformation support

#### 2.3 Geometric Operations
```
Basic Operations:
ST_AsText()     Convert to WKT format
ST_AsBinary()   Convert to WKB format
ST_GeomFromText() Parse WKT to geometry
ST_GeomFromWKB() Parse WKB to geometry
ST_SRID()       Get SRID of geometry
ST_SetSRID()    Set SRID of geometry

Spatial Relationships:
ST_Equals()     Geometries are equal
ST_Disjoint()   Geometries don't intersect
ST_Intersects() Geometries intersect
ST_Touches()    Geometries touch
ST_Crosses()    Geometries cross
ST_Within()     Geometry within another
ST_Contains()   Geometry contains another
ST_Overlaps()   Geometries overlap
ST_Covers()     Geometry covers another
ST_CoveredBy()  Geometry covered by another

Spatial Analysis:
ST_Distance()   Distance between geometries
ST_Area()       Area of geometry
ST_Length()     Length of geometry
ST_Perimeter()  Perimeter of geometry
ST_Centroid()   Centroid of geometry
ST_Buffer()     Buffer around geometry
ST_ConvexHull() Convex hull of geometry
ST_Intersection() Intersection of geometries
ST_Union()      Union of geometries
ST_Difference() Difference of geometries
ST_SymDifference() Symmetric difference
```

#### 2.4 Spatial Indexing
- R-Tree indexes for spatial data (foundation from Phase 9)
- GiST-like indexing for efficient spatial queries
- Distance ordering and nearest neighbor support

### 3. Collation and Internationalization

#### 3.1 ICU Integration
- Use ICU4C library for comprehensive collation support
- Unicode normalization (NFD, NFC, NFKD, NFKC)
- Case folding and accent-insensitive comparison
- Language-specific collation rules

#### 3.2 Collation Definition
```sql
CREATE COLLATION collation_name (
    LOCALE = 'locale_string',
    PROVIDER = icu,
    DETERMINISTIC = true/false,
    STRENGTH = primary/secondary/tertiary/identical,
    CASE_SENSITIVE = true/false,
    ACCENT_SENSITIVE = true/false
);
```

#### 3.3 Collation Application
- Column-level collation specification
- Expression-level collation casting
- Index collation support
- ORDER BY collation handling
- LIKE and regex collation awareness

#### 3.4 Character Set Support
- UTF-8 as primary encoding
- Support for other encodings via ICU
- Character set conversion functions
- Byte-order mark handling

### 4. Integration Points

#### 4.1 Type System Integration
- Register new types in catalog system
- Type casting and coercion rules
- Array types for JSON arrays and geometry collections
- Domain support for constrained types

#### 4.2 Parser and Expression Engine
- Extend parser for JSON operators and functions
- Add spatial function parsing
- Collation-aware expression evaluation
- JSON path expression parsing

#### 4.3 Executor Integration
- Custom scan nodes for JSON and spatial data
- Expression evaluation for spatial predicates
- Collation-aware sorting and comparison
- Index integration for spatial and JSON queries

#### 4.4 Optimizer Integration
- Cost estimation for JSON and spatial operations
- Selectivity estimation for spatial predicates
- Statistics collection for new data types
- Index selection for JSON and spatial queries

### 5. Implementation Strategy

#### Phase 5.1: JSON Foundation
1. Implement JSON and JSONB storage formats
2. Add basic JSON operators (->, ->>, ?, @>)
3. Create JSON validation and parsing
4. Add JSON to/from text conversion functions

#### Phase 5.2: JSON Advanced Features
1. Implement JSON path expressions (#>, #>>)
2. Add JSON construction functions (json_build_object, etc.)
3. Implement JSON modification functions (json_set, json_insert, json_delete)
4. Add GIN indexing support for JSONB

#### Phase 5.3: Spatial Foundation
1. Implement geometry storage and WKB/WKT formats
2. Add basic geometry constructors and accessors
3. Implement spatial relationship functions
4. Add basic spatial indexing support

#### Phase 5.4: Spatial Analysis
1. Implement spatial analysis functions (distance, area, etc.)
2. Add geometric operations (buffer, intersection, union)
3. Implement spatial aggregates
4. Add advanced spatial indexing and query optimization

#### Phase 5.5: Collation System
1. Integrate ICU4C library
2. Implement collation creation and management
3. Add collation-aware text operations
4. Implement collation for indexes and sorting

### 6. Testing Strategy

#### 6.1 JSON Tests
- JSON parsing and validation tests
- JSON operator functionality tests
- JSON function correctness tests
- JSON indexing and query performance tests
- JSON edge cases and error handling

#### 6.2 Spatial Tests
- Geometry parsing and validation tests
- Spatial relationship correctness tests
- Spatial analysis function tests
- Spatial indexing performance tests
- Coordinate system transformation tests

#### 6.3 Collation Tests
- ICU integration tests
- Collation rule correctness tests
- International sorting tests
- Case/accent sensitivity tests
- Performance comparison tests

#### 6.4 Integration Tests
- Mixed data type queries
- Index usage verification
- Optimizer plan quality tests
- Performance regression tests

### 7. Performance Considerations

#### 7.1 JSON Performance
- JSONB binary format for faster access
- Path-specific GIN indexes
- Lazy parsing for large JSON documents
- Memory-efficient JSON processing

#### 7.2 Spatial Performance
- R-Tree indexing for spatial queries
- Bounding box precomputation
- Spatial query optimization
- Memory-efficient geometry processing

#### 7.3 Collation Performance
- ICU collation cache
- Fast-path for simple collations
- Index optimization for collation-aware queries
- Memory-efficient string processing

### 8. Security and Validation

#### 8.1 JSON Security
- JSON depth limits to prevent DoS
- Size limits for JSON documents
- Input validation for JSON syntax
- Safe parsing without recursion limits

#### 8.2 Spatial Security
- Geometry validation (closed polygons, etc.)
- SRID validation and bounds checking
- Input size limits for geometry data
- Safe parsing of WKB/WKT formats

#### 8.3 Collation Security
- Safe ICU library usage
- Locale validation
- Memory limits for collation operations
- Safe string handling

### 9. Documentation and Examples

#### 9.1 User Documentation
- JSON usage guide with examples
- Spatial data tutorial
- Internationalization guide
- Best practices for performance

#### 9.2 API Documentation
- Function reference for JSON operations
- Spatial function reference
- Collation creation and usage
- Type conversion rules

#### 9.3 Migration Guide
- Migrating from other databases
- Collation migration considerations
- Spatial data migration
- JSON schema evolution

## Exit Criteria

- ✅ All JSON operators and functions working correctly
- ✅ All spatial relationship and analysis functions implemented
- ✅ ICU-based collation system fully functional
- ✅ Comprehensive test suites passing
- ✅ Performance benchmarks meeting targets
- ✅ Integration with existing features (indexes, constraints, etc.)
- ✅ Documentation complete and accurate

## Risk Assessment

### High Risk Items
1. ICU library integration complexity
2. Spatial algorithm correctness
3. JSON path expression performance
4. Collation memory usage

### Mitigation Strategies
1. Use proven ICU integration patterns
2. Implement comprehensive geometry validation
3. Add performance monitoring and optimization
4. Implement memory limits and monitoring

## Timeline Estimate

- **Phase 17.1**: JSON Foundation (4-6 weeks)
- **Phase 17.2**: JSON Advanced Features (4-6 weeks)
- **Phase 17.3**: Spatial Foundation (6-8 weeks)
- **Phase 17.4**: Spatial Analysis (6-8 weeks)
- **Phase 17.5**: Collation System (4-6 weeks)
- **Integration & Testing**: (4-6 weeks)
- **Documentation**: (2-3 weeks)

**Total Estimate**: 30-43 weeks (7-10 months)

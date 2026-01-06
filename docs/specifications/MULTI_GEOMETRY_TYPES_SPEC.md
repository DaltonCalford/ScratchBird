# Multi-Geometry Types Specification
**Version:** 1.0
**Date:** November 18, 2025
**Standard:** OGC Simple Features Specification / PostGIS Compatible
**Purpose:** Define implementation requirements for MULTIPOINT, MULTILINESTRING, MULTIPOLYGON, and GEOMETRYCOLLECTION types

---

## Table of Contents

1. [Overview](#overview)
2. [ScratchBird Canonical Encoding (Alpha)](#scratchbird-canonical-encoding-alpha)
3. [Type Definitions](#type-definitions)
4. [WKB Binary Format](#wkb-binary-format)
5. [C++ Structure Specifications](#c-structure-specifications)
6. [TypedValue Integration](#typedvalue-integration)
7. [Validation Rules](#validation-rules)
8. [PostgreSQL Compatibility](#postgresql-compatibility)

---

## Overview

Multi-geometry types represent collections of geometric primitives following the OGC Simple Features specification. These types enable storage of multiple geometries of the same or different types within a single value.

### Type Hierarchy

```
Geometry
├── Point
├── LineString
├── Polygon
├── MultiPoint        (Collection of Points)
├── MultiLineString   (Collection of LineStrings)
├── MultiPolygon      (Collection of Polygons)
└── GeometryCollection (Mixed collection)
```

### Standards Compliance

- **OGC Simple Features for SQL (SFSQL)** - Base specification
- **SQL/MM Part 3** - Extended spatial specification
- **PostGIS** - PostgreSQL spatial extension (de facto standard)
- **ISO 19125** - Geographic information Simple feature access

---

## ScratchBird Canonical Encoding (Alpha)

ScratchBird stores spatial values using the canonical `TypedValue::serializePlainValue` encoding, not WKB. This provides a consistent on-disk format across all spatial types.

**Key points:**
- Multi-geometry values are stored as structured payloads (SRID + counts + point lists), not OGC WKB bytes.
- `GEOMETRYCOLLECTION` stores nested geometry payloads with type tags and length prefixes.
- WKB is a target for interchange and client compatibility, but it is not the storage format.

---

## Type Definitions

### 1. MULTIPOINT

**Definition:** A collection of zero or more Point geometries.

**Properties:**
- Each point maintains independent coordinates
- Points may occupy the same location (non-simple)
- Simple MultiPoints have unique coordinates
- No connectivity between points

**Examples:**
```sql
-- Text representation
MULTIPOINT ((0 0), (1 2), (3 4))
MULTIPOINT EMPTY

-- Alternative syntax (without point parentheses)
MULTIPOINT (0 0, 1 2, 3 4)
```

**Use Cases:**
- Multiple sensor locations
- Waypoint collections
- Scatter plot data
- GPS trace points

---

### 2. MULTILINESTRING

**Definition:** A collection of zero or more LineString geometries.

**Properties:**
- Each LineString maintains independent vertices
- LineStrings may be disconnected (gaps allowed)
- Closed if all constituent LineStrings are closed
- LineStrings may intersect at points

**Validity Rules:**
- Each constituent LineString must be valid (≥2 points)
- No restriction on connectivity
- May form disconnected segments

**Examples:**
```sql
-- Multiple disconnected line segments
MULTILINESTRING ((0 0, 1 1, 1 2), (2 3, 3 2, 5 4))

-- Road network with multiple streets
MULTILINESTRING (
  (0 0, 5 0),     -- Street 1
  (0 1, 5 1),     -- Street 2 (parallel)
  (2 -1, 2 2)     -- Cross street
)
```

**Use Cases:**
- Road networks
- River systems
- Power line networks
- Transportation routes

---

### 3. MULTIPOLYGON

**Definition:** A collection of zero or more Polygon geometries with strict non-overlap constraints.

**Properties:**
- Polygons must not overlap (interiors disjoint)
- Polygons must not be adjacent along edges
- Polygons may touch only at finite points
- Each polygon may have holes (interior rings)

**Validity Rules:**
- Each constituent Polygon must be valid
- Interior(Polygon[i]) ∩ Interior(Polygon[j]) = ∅ for i ≠ j
- Polygons may share boundary points but not boundary segments
- No polygon may be contained within another

**Examples:**
```sql
-- Two separate polygons
MULTIPOLYGON (
  ((1 5, 5 5, 5 1, 1 1, 1 5)),          -- First polygon
  ((6 5, 9 1, 6 1, 6 5))                -- Second polygon (disjoint)
)

-- Polygons with holes
MULTIPOLYGON (
  ((0 0, 10 0, 10 10, 0 10, 0 0), (2 2, 2 8, 8 8, 8 2, 2 2)),  -- Polygon with hole
  ((15 0, 20 0, 20 5, 15 5, 15 0))                              -- Solid polygon
)
```

**Use Cases:**
- Property boundaries (non-contiguous parcels)
- Administrative regions (islands, territories)
- Land use zones
- Protected areas

---

### 4. GEOMETRYCOLLECTION

**Definition:** A heterogeneous collection containing any mix of geometry types.

**Properties:**
- Can contain Points, LineStrings, Polygons
- Can contain other Multi* types
- Can contain nested GeometryCollections
- Order of elements is significant
- Elements may overlap, touch, or be disjoint

**No Restrictions:**
- No validity constraints beyond constituent geometries
- Arbitrary mixing allowed
- Unlimited nesting depth

**Examples:**
```sql
-- Mixed geometry types
GEOMETRYCOLLECTION (
  POINT(2 3),
  LINESTRING(2 3, 3 4),
  POLYGON((0 0, 1 0, 1 1, 0 1, 0 0))
)

-- Complex nested collection
GEOMETRYCOLLECTION (
  MULTIPOINT ((0 0), (1 1)),
  MULTILINESTRING ((0 0, 1 1), (2 2, 3 3)),
  GEOMETRYCOLLECTION (
    POINT(5 5),
    LINESTRING(5 5, 6 6)
  )
)

-- Empty collection
GEOMETRYCOLLECTION EMPTY
```

**Use Cases:**
- Complex geographic features
- Mixed infrastructure data
- Arbitrary spatial data aggregation
- GIS interchange format

---

## WKB Binary Format

WKB is the external interchange format for spatial values. ScratchBird does not store WKB on disk; it stores canonical TypedValue payloads (see `DATA_TYPE_PERSISTENCE_AND_CASTS.md`).

All multi-geometry types use the OGC Well-Known Binary (WKB) format.

### Common Structure

```
[byte order] [type code] [count] [geometries...]
```

### Byte Order

```
0x00 = Big Endian (wkbXDR)
0x01 = Little Endian (wkbNDR) ← Most common
```

### Type Codes

| Type | Base Code | +Z | +M | +ZM | +SRID |
|------|-----------|----|----|-----|-------|
| MULTIPOINT | 4 | 1004 | 2004 | 3004 | 0x20000004 |
| MULTILINESTRING | 5 | 1005 | 2005 | 3005 | 0x20000005 |
| MULTIPOLYGON | 6 | 1006 | 2006 | 3006 | 0x20000006 |
| GEOMETRYCOLLECTION | 7 | 1007 | 2007 | 3007 | 0x20000007 |

**Extended WKB Flags:**
- `0x80000000` - Has Z coordinate
- `0x40000000` - Has M coordinate
- `0x20000000` - Has SRID

**ISO WKB (PostGIS 2.0+):**
- Add 1000 for Z dimension
- Add 2000 for M dimension
- Add 3000 for ZM

---

### 1. MULTIPOINT Binary Format

```c
struct WKBMultiPoint {
    uint8_t  byte_order;        // 0x01 (little-endian)
    uint32_t wkb_type;          // 4 (MULTIPOINT)
    uint32_t num_points;        // Number of points
    WKBPoint points[];          // Array of WKBPoint
};

struct WKBPoint {
    uint8_t  byte_order;        // 0x01
    uint32_t wkb_type;          // 1 (POINT)
    double   x;                 // 8 bytes
    double   y;                 // 8 bytes
};
```

**Size Calculation:**
```
Total = 9 bytes (header) + num_points × 21 bytes
      = 9 + 21n bytes (2D points)
```

**Example: MULTIPOINT ((0 0), (1 1))**
```
01                    # Little-endian
04 00 00 00           # Type: MULTIPOINT (4)
02 00 00 00           # Count: 2 points

# Point 1
01                    # Little-endian
01 00 00 00           # Type: POINT (1)
00 00 00 00 00 00 00 00  # X: 0.0
00 00 00 00 00 00 00 00  # Y: 0.0

# Point 2
01                    # Little-endian
01 00 00 00           # Type: POINT (1)
00 00 00 00 00 00 F0 3F  # X: 1.0
00 00 00 00 00 00 F0 3F  # Y: 1.0
```

---

### 2. MULTILINESTRING Binary Format

```c
struct WKBMultiLineString {
    uint8_t       byte_order;     // 0x01
    uint32_t      wkb_type;       // 5 (MULTILINESTRING)
    uint32_t      num_linestrings;
    WKBLineString linestrings[];
};

struct WKBLineString {
    uint8_t  byte_order;          // 0x01
    uint32_t wkb_type;            // 2 (LINESTRING)
    uint32_t num_points;          // ≥ 2
    Point    points[];            // Array of (x,y) pairs
};
```

**Size Calculation:**
```
Header: 9 bytes
Each LineString: 5 bytes + 8 bytes (type) + num_points × 16 bytes
```

---

### 3. MULTIPOLYGON Binary Format

```c
struct WKBMultiPolygon {
    uint8_t    byte_order;       // 0x01
    uint32_t   wkb_type;         // 6 (MULTIPOLYGON)
    uint32_t   num_polygons;
    WKBPolygon polygons[];
};

struct WKBPolygon {
    uint8_t  byte_order;         // 0x01
    uint32_t wkb_type;           // 3 (POLYGON)
    uint32_t num_rings;          // ≥ 1 (exterior + holes)
    struct {
        uint32_t num_points;     // ≥ 4 (closed ring)
        Point    points[];
    } rings[];
};
```

**Ring Rules:**
- First ring = exterior boundary (must be closed)
- Subsequent rings = holes (must be closed)
- Closing point = first point repeated

---

### 4. GEOMETRYCOLLECTION Binary Format

```c
struct WKBGeometryCollection {
    uint8_t     byte_order;      // 0x01
    uint32_t    wkb_type;        // 7 (GEOMETRYCOLLECTION)
    uint32_t    num_geometries;
    WKBGeometry geometries[];    // Mixed types
};

// Each geometry is a complete WKB structure
union WKBGeometry {
    WKBPoint             point;
    WKBLineString        linestring;
    WKBPolygon           polygon;
    WKBMultiPoint        multipoint;
    WKBMultiLineString   multilinestring;
    WKBMultiPolygon      multipolygon;
    WKBGeometryCollection nested_collection;  // Recursive
};
```

**Key Feature:** Each nested geometry includes full byte order + type header

---

## C++ Structure Specifications

### 1. MultiPoint Structure

```cpp
namespace scratchbird::core {

struct MultiPoint {
    std::vector<Point> points;
    int32_t srid;  // Spatial Reference ID (0 = undefined)

    // Constructors
    MultiPoint() : srid(0) {}
    explicit MultiPoint(std::vector<Point> pts)
        : points(std::move(pts)), srid(0) {}
    MultiPoint(std::vector<Point> pts, int32_t srid_)
        : points(std::move(pts)), srid(srid_) {}

    // Validation
    bool isEmpty() const { return points.empty(); }
    size_t numGeometries() const { return points.size(); }

    // Simple if all points have unique coordinates
    bool isSimple() const {
        std::set<std::pair<double, double>> coords;
        for (const auto& pt : points) {
            if (!coords.insert({pt.x, pt.y}).second)
                return false;  // Duplicate found
        }
        return true;
    }

    // SRID accessors
    int32_t getSRID() const { return srid; }
    void setSRID(int32_t new_srid) { srid = new_srid; }
    bool hasSRID() const { return srid != 0; }

    // Comparison
    bool operator==(const MultiPoint& other) const {
        return points == other.points && srid == other.srid;
    }
};

} // namespace scratchbird::core
```

---

### 2. MultiLineString Structure

```cpp
struct MultiLineString {
    std::vector<LineString> linestrings;
    int32_t srid;

    // Constructors
    MultiLineString() : srid(0) {}
    explicit MultiLineString(std::vector<LineString> lines)
        : linestrings(std::move(lines)), srid(0) {}
    MultiLineString(std::vector<LineString> lines, int32_t srid_)
        : linestrings(std::move(lines)), srid(srid_) {}

    // Validation
    bool isEmpty() const { return linestrings.empty(); }
    size_t numGeometries() const { return linestrings.size(); }

    bool isValid() const {
        for (const auto& line : linestrings) {
            if (!line.isValid())
                return false;
        }
        return true;
    }

    // Closed if all constituent linestrings are closed
    bool isClosed() const {
        if (isEmpty()) return false;
        for (const auto& line : linestrings) {
            if (line.points.empty() ||
                line.points.front() != line.points.back())
                return false;
        }
        return true;
    }

    // SRID accessors
    int32_t getSRID() const { return srid; }
    void setSRID(int32_t new_srid) { srid = new_srid; }
    bool hasSRID() const { return srid != 0; }

    bool operator==(const MultiLineString& other) const {
        return linestrings == other.linestrings && srid == other.srid;
    }
};
```

---

### 3. MultiPolygon Structure

```cpp
struct MultiPolygon {
    std::vector<Polygon> polygons;
    int32_t srid;

    // Constructors
    MultiPolygon() : srid(0) {}
    explicit MultiPolygon(std::vector<Polygon> polys)
        : polygons(std::move(polys)), srid(0) {}
    MultiPolygon(std::vector<Polygon> polys, int32_t srid_)
        : polygons(std::move(polys)), srid(srid_) {}

    // Validation
    bool isEmpty() const { return polygons.empty(); }
    size_t numGeometries() const { return polygons.size(); }

    bool isValid() const {
        // Each polygon must be valid
        for (const auto& poly : polygons) {
            if (!poly.isValid())
                return false;
        }

        // Polygons must not overlap (interiors disjoint)
        // Polygons may only touch at points, not edges
        // Note: Full topological validation requires computational geometry

        return true;
    }

    // SRID accessors
    int32_t getSRID() const { return srid; }
    void setSRID(int32_t new_srid) { srid = new_srid; }
    bool hasSRID() const { return srid != 0; }

    bool operator==(const MultiPolygon& other) const {
        return polygons == other.polygons && srid == other.srid;
    }
};
```

---

### 4. GeometryCollection Structure

```cpp
struct GeometryCollection {
    std::vector<std::shared_ptr<TypedValue>> geometries;
    int32_t srid;

    // Constructors
    GeometryCollection() : srid(0) {}
    explicit GeometryCollection(std::vector<std::shared_ptr<TypedValue>> geoms)
        : geometries(std::move(geoms)), srid(0) {}
    GeometryCollection(std::vector<std::shared_ptr<TypedValue>> geoms, int32_t srid_)
        : geometries(std::move(geoms)), srid(srid_) {}

    // Validation
    bool isEmpty() const { return geometries.empty(); }
    size_t numGeometries() const { return geometries.size(); }

    bool isValid() const {
        for (const auto& geom : geometries) {
            if (!geom) return false;

            // Each geometry must be a valid spatial type
            DataType type = geom->type();
            if (type != DataType::POINT &&
                type != DataType::LINESTRING &&
                type != DataType::POLYGON &&
                type != DataType::MULTIPOINT &&
                type != DataType::MULTILINESTRING &&
                type != DataType::MULTIPOLYGON &&
                type != DataType::GEOMETRYCOLLECTION) {
                return false;
            }
        }
        return true;
    }

    // SRID accessors
    int32_t getSRID() const { return srid; }
    void setSRID(int32_t new_srid) { srid = new_srid; }
    bool hasSRID() const { return srid != 0; }

    bool operator==(const GeometryCollection& other) const;
};
```

---

## TypedValue Integration

TypedValue integration is implemented in core.

**Factory methods (implemented):**
- `makePoint`, `makeLineString`, `makePolygon`
- `makeMultiPoint`, `makeMultiLineString`, `makeMultiPolygon`
- `makeGeometryCollection`

**Storage model:**
- Spatial values are stored in `TypedValue::spatial_data_`.
- `GEOMETRYCOLLECTION` stores nested `TypedValue` payloads with type tags and length prefixes.

**Serialization:**
- Uses `TypedValue::serializePlainValue` with canonical layouts (SRID + counts + point lists).
- WKT formatting is supported for `toString`; WKB conversion remains an interchange feature.

---

## Validation Rules

### MultiPoint Validation

- ✓ May be empty
- ✓ All points must have same SRID
- ✓ Coordinates must be finite (not NaN/Inf)

### MultiLineString Validation

- ✓ May be empty
- ✓ Each LineString must have ≥ 2 points
- ✓ All LineStrings must have same SRID
- ✓ No individual validation beyond constituents

### MultiPolygon Validation

- ✓ May be empty
- ✓ Each Polygon must be valid
- ✗ Polygons must not overlap (interiors disjoint)
- ✗ Polygons may touch only at points
- ✗ No polygon may contain another
- ✓ All Polygons must have same SRID

### GeometryCollection Validation

- ✓ May be empty
- ✓ All geometries must be valid
- ✓ All geometries must be spatial types
- ✓ Nested collections allowed
- ✓ All geometries should have same SRID (warning if not)

---

## PostgreSQL Compatibility

### SQL Types

```sql
CREATE TABLE spatial_data (
    id SERIAL PRIMARY KEY,
    location GEOMETRY(MULTIPOINT, 4326),
    roads GEOMETRY(MULTILINESTRING, 4326),
    parcels GEOMETRY(MULTIPOLYGON, 4326),
    mixed GEOMETRY(GEOMETRYCOLLECTION, 4326)
);
```

### PostGIS Functions Support

Must support these standard functions:

**Constructors:**
- `ST_Multi(geometry)` - Convert single to multi-geometry
- `ST_Collect(geometry[])` - Create collection

**Accessors:**
- `ST_NumGeometries(multi)` - Count elements
- `ST_GeometryN(multi, n)` - Get nth geometry (1-indexed)

**Predicates:**
- `ST_IsEmpty(multi)` - Check if empty
- `ST_IsSimple(multipoint)` - Check uniqueness
- `ST_IsClosed(multilinestring)` - Check closure

**Transformations:**
- `ST_Dump(multi)` - Explode to individual geometries
- `ST_CollectionExtract(collection, type)` - Extract specific type
- `ST_Union(multi)` - Merge into single geometry

---

## Implementation Priority

1. **WKB Serialization/Deserialization** (MUST HAVE)
   - Implement in `src/spatial/wkb.cpp`
   - Follow existing pattern for Point/LineString/Polygon

2. **Structure Definitions** (MUST HAVE)
   - Add structs to `include/scratchbird/core/types.h`
   - Add comparison operators

3. **TypedValue Integration** (MUST HAVE)
   - Add to variant
   - Implement factory methods
   - Implement getters

4. **TypeSerializer Integration** (MUST HAVE)
   - Add serialize() cases
   - Add deserialize() cases
   - Add getSerializedSize() cases

5. **Validation Functions** (SHOULD HAVE)
   - Basic validation (empty, counts)
   - Topological validation (overlaps) - defer to Phase 2

6. **SQL Function Support** (FUTURE)
   - ST_NumGeometries, ST_GeometryN
   - ST_Dump, ST_CollectionExtract
   - Constructors and predicates

---

## Testing Requirements

For each type, implement tests for:

1. ✓ Empty collection
2. ✓ Single element
3. ✓ Multiple elements
4. ✓ Serialization round-trip
5. ✓ Size calculation
6. ✓ SRID preservation
7. ✓ Comparison operators
8. ✓ Invalid data detection
9. ✓ WKB format compliance (interchange layer)
10. ✓ PostGIS interoperability

---

## References

- **OGC 06-103r4** - OpenGIS Implementation Standard for Geographic information - Simple feature access - Part 1: Common architecture
- **ISO 19125-1:2004** - Geographic information — Simple feature access — Part 1: Common architecture
- **PostGIS Manual** - https://postgis.net/docs/
- **PostgreSQL Documentation** - https://www.postgresql.org/docs/current/datatype-geometric.html
- **WKB Specification** - https://libgeos.org/specifications/wkb/

---

**Document Version:** 1.0
**Last Updated:** November 18, 2025
**Status:** Partially Implemented (canonical encoding in place; WKB interoperability pending)

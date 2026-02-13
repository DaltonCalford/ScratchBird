# Wave 3: Spatial Completion - Launch Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: October 28, 2025
**Wave**: 3 of 4 (Phase 2 completion)
**Objective**: Complete spatial support for GIS applications
**Timeline**: 1-2 weeks (with AI agents)

---

## Prerequisites ✅

### Environment Setup
- ✅ **GEOS 3.13.1** detected - Spatial functions library
- ✅ **PROJ 9.5.1** detected - Coordinate transformation library
- ✅ **CMakeLists.txt** updated - Library integration configured
- ✅ **Build system** ready - Libraries will link automatically

### Code Foundation
- ✅ **Task 9.1** complete - Spatial types (POINT, LINESTRING, POLYGON, WKT, WKB)
- ✅ **Wave 1** infrastructure - 2,095 lines of geometry code
- ✅ **Test framework** ready - 44/44 spatial type tests passing

---

## Wave 3 Tasks Overview

| Task | Agents | Effort | Priority | Lines | Deliverable |
|------|--------|--------|----------|-------|-------------|
| **9.2** R-tree Indexes | R1-R4 | 20-30h | 🎯 HIGH | ~2,000 | Fast spatial queries |
| **9.3** Spatial Functions | G1-G4 | 15-25h | 📊 MEDIUM | ~1,350 | ST_* function library |
| **9.4** Multi-Geometry | M1 | 10-15h | 📊 MEDIUM | ~950 | MULTI*, GEOMETRYCOLLECTION |
| **9.5** Coordinate Systems | S1-S3 | 10-15h | 📊 MEDIUM | ~800 | SRID, ST_Transform |
| **TOTAL** | **12 agents** | **55-85h** | - | **~5,100** | GIS-ready database |

---

## Task 9.2: R-tree Spatial Indexes

### Agent Configuration

**4 Agents in Parallel**:

#### Agent R1: R-tree Core Structure (7-10h)
**Scope**: Implement R-tree node hierarchy and MBR calculations

**Deliverables**:
1. `include/scratchbird/core/rtree.h` (~400 lines)
   - RTree class with insert/search/delete interfaces
   - Node statistics and tree metadata
   - Iterator support

2. `include/scratchbird/core/rtree_node.h` (~150 lines)
   - RTreeNode base class
   - InternalNode and LeafNode subclasses
   - MBR (Minimum Bounding Rectangle) struct

3. `src/core/rtree_node.cpp` (~250 lines)
   - Node construction/destruction
   - MBR calculation and union
   - Node serialization/deserialization

**Key Classes**:
```cpp
class RTree {
public:
    RTree(uint16_t max_entries = 50);

    bool insert(const BoundingBox& mbr, uint64_t row_id);
    std::vector<uint64_t> search(const BoundingBox& query);
    bool remove(const BoundingBox& mbr, uint64_t row_id);

    size_t height() const;
    size_t nodeCount() const;
};

struct BoundingBox {
    double min_x, min_y, max_x, max_y;

    bool intersects(const BoundingBox& other) const;
    bool contains(const BoundingBox& other) const;
    BoundingBox unionWith(const BoundingBox& other) const;
    double area() const;
    double enlargement(const BoundingBox& other) const;
};
```

---

#### Agent R2: Insertion & Balancing (4-6h)
**Scope**: Implement R*-tree insertion with forced reinsert

**Deliverables**:
1. `src/core/rtree.cpp` - Insertion methods (~300 lines)
   - `insert()` - Main entry point
   - `chooseLeaf()` - Find optimal insertion location
   - `splitNode()` - R*-tree split algorithm
   - `adjustTree()` - Propagate MBR changes upward
   - `forcedReinsert()` - R*-tree optimization

**Key Algorithms**:
```cpp
private:
    RTreeNode* chooseLeaf(const BoundingBox& mbr);
    void splitNode(RTreeNode* node);
    void adjustTree(RTreeNode* leaf);
    void forcedReinsert(RTreeNode* node, int level);

    // R*-tree split uses axis sorting
    void splitByAxis(RTreeNode* node, std::vector<Entry>& entries);
```

**Reference**: "The R*-tree: An Efficient and Robust Access Method" (Beckmann et al., 1990)

---

#### Agent R3: Search & Deletion (7-11h)
**Scope**: Implement search and deletion with tree maintenance

**Deliverables**:
1. `src/core/rtree.cpp` - Search/delete methods (~300 lines)
   - `search()` - Bounding box query
   - `remove()` - Delete entry and condense tree
   - `findLeaf()` - Locate entry to delete
   - `condenseTree()` - Handle underflow
   - `reinsertOrphans()` - Reinsert deleted entries

2. `tests/unit/test_rtree.cpp` (~200 lines)
   - Insert 10K random points
   - Search performance tests
   - Deletion correctness tests
   - Tree balance validation

**Key Algorithms**:
```cpp
public:
    std::vector<uint64_t> search(const BoundingBox& query) {
        std::vector<uint64_t> results;
        searchRecursive(root_, query, results);
        return results;
    }

private:
    void searchRecursive(RTreeNode* node, const BoundingBox& query,
                        std::vector<uint64_t>& results);
    RTreeNode* findLeaf(const BoundingBox& mbr, uint64_t row_id);
    void condenseTree(RTreeNode* leaf, std::vector<Entry>& orphans);
```

---

#### Agent R4: Query Planner Integration (2-3h)
**Scope**: Integrate R-tree with query optimizer

**Deliverables**:
1. `src/core/catalog_manager.cpp` - R-tree catalog support (~50 lines)
   - Add IndexType::RTREE enumeration
   - Store/load R-tree metadata

2. `src/optimizer/query_planner.cpp` - Spatial index path (~150 lines)
   - Recognize spatial predicates (ST_Contains, ST_Intersects, ST_DWithin)
   - Generate R-tree index scan paths
   - Choose between seq scan and R-tree scan

3. `src/optimizer/cost_model.cpp` - R-tree cost estimation (~100 lines)
   - Estimate selectivity for bounding box queries
   - Cost R-tree traversal vs. sequential scan

**Integration Points**:
```cpp
// Query planner recognizes spatial predicates
if (isSpatialPredicate(where_clause)) {
    // Check for R-tree index
    auto rtree_indexes = findRTreeIndexes(table_id);
    if (!rtree_indexes.empty()) {
        // Generate R-tree index scan path
        auto path = generateRTreeScanPath(rtree_indexes[0], where_clause);
        paths.push_back(path);
    }
}

// Cost model
double costRTreeScan(RTree* index, BoundingBox query) {
    // Estimate nodes visited = log(N) * selectivity
    double selectivity = estimateSpatialSelectivity(query);
    return log2(index->nodeCount()) * selectivity * CPU_COST_PER_NODE;
}
```

---

## Task 9.3: Spatial Functions via GEOS

### Agent Configuration

**4 Agents in Parallel**:

#### Agent G1: GEOS Integration (5-7h)
**Scope**: Create C++ wrapper for GEOS library

**Deliverables**:
1. `include/scratchbird/geo/geos_wrapper.h` (~150 lines)
   - GEOSContext RAII wrapper
   - Conversion functions: ScratchBird ↔ GEOS

2. `src/geo/geos_wrapper.cpp` (~400 lines)
   - `toGEOSGeometry()` - Convert Point/LineString/Polygon to GEOS
   - `fromGEOSGeometry()` - Convert GEOS back to ScratchBird
   - Error handling and cleanup
   - Context caching for performance

**Key Classes**:
```cpp
class GEOSContext {
public:
    GEOSContext();
    ~GEOSContext();

    GEOSContextHandle_t handle() { return ctx_; }

private:
    GEOSContextHandle_t ctx_;
};

GEOSGeometry* toGEOSGeometry(const Geometry& geom, GEOSContext& ctx);
std::unique_ptr<Geometry> fromGEOSGeometry(const GEOSGeometry* geos_geom);
```

**GEOS C API**:
- Header: `#include <geos_c.h>`
- Link: `-lgeos_c`
- Thread-safe: Use context per thread

---

#### Agent G2: Measurement Functions (3-5h)
**Scope**: Implement distance, area, length, perimeter

**Deliverables**:
1. `src/sblr/executor.cpp` - Measurement handlers (~200 lines)
   - `EXT_ST_DISTANCE` - Cartesian distance between geometries
   - `EXT_ST_AREA` - Area of polygon in square units
   - `EXT_ST_LENGTH` - Length of linestring
   - `EXT_ST_PERIMETER` - Perimeter of polygon

2. `src/sblr/bytecode_generator.cpp` - Function bytecode (~50 lines)
   - Wire up ST_Distance, ST_Area, ST_Length, ST_Perimeter

**Implementation**:
```cpp
case Opcode::EXT_ST_DISTANCE: {
    Value geom2 = pop();
    Value geom1 = pop();

    if (geom1.isNull() || geom2.isNull()) {
        push(Value::makeNull());
        break;
    }

    GEOSContext ctx;
    auto g1 = toGEOSGeometry(geom1.toGeometry(), ctx);
    auto g2 = toGEOSGeometry(geom2.toGeometry(), ctx);

    double distance;
    GEOSDistance_r(ctx.handle(), g1, g2, &distance);

    push(Value::makeFloat(distance));
    break;
}
```

---

#### Agent G3: Relationship Functions (6-10h)
**Scope**: Implement 7 spatial relationship predicates

**Deliverables**:
1. `src/sblr/executor.cpp` - Relationship handlers (~300 lines)
   - `EXT_ST_CONTAINS` - Does geom1 contain geom2?
   - `EXT_ST_WITHIN` - Is geom1 within geom2?
   - `EXT_ST_INTERSECTS` - Do geometries intersect?
   - `EXT_ST_CROSSES` - Do geometries cross?
   - `EXT_ST_OVERLAPS` - Do geometries overlap?
   - `EXT_ST_TOUCHES` - Do geometries touch?
   - `EXT_ST_DISJOINT` - Are geometries disjoint?

**Optimization**: Cache GEOS conversions in query context

```cpp
// Efficient batch processing
class SpatialQueryContext {
    std::unordered_map<uint64_t, GEOSGeometry*> cache_;
    GEOSContext geos_ctx_;

public:
    GEOSGeometry* getGEOSGeometry(const Geometry& geom, uint64_t row_id);
    ~SpatialQueryContext() { /* cleanup cache */ }
};
```

---

#### Agent G4: Geometric Operations (3-5h)
**Scope**: Implement buffer, intersection, union, difference

**Deliverables**:
1. `src/sblr/executor.cpp` - Operation handlers (~200 lines)
   - `EXT_ST_BUFFER` - Create buffer around geometry
   - `EXT_ST_INTERSECTION` - Compute intersection of two geometries
   - `EXT_ST_UNION` - Compute union of two geometries
   - `EXT_ST_DIFFERENCE` - Compute difference (A - B)

2. `tests/unit/test_spatial_functions.cpp` (~150 lines)
   - Test each function with known geometries
   - Validate results against PostGIS
   - Performance benchmarks

**Implementation**:
```cpp
case Opcode::EXT_ST_BUFFER: {
    Value distance = pop();
    Value geom = pop();

    GEOSContext ctx;
    auto g = toGEOSGeometry(geom.toGeometry(), ctx);
    auto buffered = GEOSBuffer_r(ctx.handle(), g, distance.toFloat(), 8);

    auto result = fromGEOSGeometry(buffered);
    push(Value::makeGeometry(*result));
    break;
}
```

---

## Task 9.4: Multi-Geometry Types

### Agent Configuration

**Single Agent M1** (10-15h):

**Deliverables**:
1. `include/scratchbird/geo/multi_geometry.h` (~250 lines)
   - MultiPoint, MultiLineString, MultiPolygon, GeometryCollection classes
   - Iterator support for component geometries

2. `src/geo/multi_geometry.cpp` (~400 lines)
   - Construction and serialization
   - WKT/WKB parser extensions for MULTI* types
   - MBR calculation for collections

3. `tests/unit/test_multi_geometry.cpp` (~150 lines)
   - Parse/serialize all 4 multi-types
   - Test nested collections
   - Validate MBR calculations

**Key Classes**:
```cpp
class MultiPoint : public Geometry {
    std::vector<Point> points_;
public:
    size_t numPoints() const { return points_.size(); }
    const Point& pointAt(size_t i) const { return points_[i]; }

    std::string toWKT() const override;
    BoundingBox getMBR() const override;
};

class GeometryCollection : public Geometry {
    std::vector<std::unique_ptr<Geometry>> geometries_;
public:
    size_t numGeometries() const;
    const Geometry& geometryAt(size_t i) const;
};
```

---

## Task 9.5: Coordinate Systems

### Agent Configuration

**3 Agents in Sequence**:

#### Agent S1: SRID Infrastructure (2-3h)
**Scope**: Add SRID field to geometry types

**Deliverables**:
1. `include/scratchbird/geo/srid.h` (~100 lines)
   - SRID constants (4326 = WGS84, 3857 = Web Mercator, etc.)
   - SRID validation

2. `src/geo/geometry.cpp` - Modify geometry base class (~50 lines)
   - Add `uint32_t srid_` field
   - Update WKT/WKB to include SRID
   - Update catalog to store SRID metadata

**WKT with SRID**: `SRID=4326;POINT(-122.4 37.8)`

---

#### Agent S2: PROJ Integration (5-7h)
**Scope**: Coordinate transformation via PROJ library

**Deliverables**:
1. `include/scratchbird/geo/proj_wrapper.h` (~50 lines)
   - PROJContext RAII wrapper
   - Transformation caching

2. `src/geo/proj_wrapper.cpp` (~250 lines)
   - Create PJ transformation objects
   - Transform point coordinates
   - Handle datum shifts and axis order

3. `src/sblr/executor.cpp` - ST_Transform handler (~100 lines)
   - `EXT_ST_TRANSFORM(geom, from_srid, to_srid)`

**PROJ API**:
```cpp
class PROJContext {
    PJ_CONTEXT* ctx_;
    std::unordered_map<uint64_t, PJ*> transformations_;
public:
    PJ* getTransformation(uint32_t from_srid, uint32_t to_srid);
    void transform(Geometry& geom, uint32_t from_srid, uint32_t to_srid);
};
```

---

#### Agent S3: Geographic Operations (3-5h)
**Scope**: Great-circle distance and geodetic operations

**Deliverables**:
1. `src/sblr/executor.cpp` - Geographic handlers (~150 lines)
   - `EXT_ST_DISTANCE_SPHERE` - Haversine formula for lat/lon
   - `EXT_ST_AREA_GEOGRAPHY` - Geodetic area calculation
   - Detect SRID 4326 and use geodetic math

2. `tests/unit/test_srid.cpp` (~100 lines)
   - Test transformations (4326 → 3857)
   - Test geodetic distance vs. Cartesian
   - Validate against PostGIS results

**Haversine Formula** (for SRID 4326):
```cpp
double distanceSphere(double lon1, double lat1, double lon2, double lat2) {
    const double R = 6371000.0; // Earth radius in meters
    double dLat = toRadians(lat2 - lat1);
    double dLon = toRadians(lon2 - lon1);
    double a = sin(dLat/2) * sin(dLat/2) +
               cos(toRadians(lat1)) * cos(toRadians(lat2)) *
               sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return R * c;
}
```

---

## Agent Deployment Strategy

### Phase 1: Launch All 12 Agents (Day 1)

**Parallel Launch**:
```
Group 1 (R-tree):    R1, R2, R3, R4  - 20-30h total
Group 2 (GEOS):      G1, G2, G3, G4  - 15-25h total
Group 3 (Multi-geo): M1              - 10-15h total
Group 4 (PROJ):      S1, S2, S3      - 10-15h total
```

**Agent Instructions Template**:
```
Task: [Agent ID] - [Task Name]
Context: Phase 2 Wave 3, ScratchBird Database Engine
Code Style: Follow existing patterns in /src/geo/, /src/core/, /src/sblr/
Testing: Write comprehensive unit tests with 95%+ coverage
Dependencies: GEOS 3.13.1, PROJ 9.5.1 (available via HAVE_GEOS, HAVE_PROJ macros)
Reference: Existing spatial types in /src/geo/geometry.cpp (2,095 lines)
Deliverables: [File list with line estimates]
Success Criteria: Compiles without errors, all tests pass
```

---

### Phase 2: Integration & Testing (Days 2-7)

**Developer Tasks** (50-70 hours):
1. **Monitor Agent Progress** (Daily, 1-2h)
   - Check agent outputs for compilation errors
   - Provide clarifications and debugging assistance

2. **Code Review** (Days 2-4, 15-20h)
   - Review all agent-generated code
   - Check for memory leaks, error handling, edge cases
   - Verify RAII patterns for GEOS/PROJ contexts

3. **Integration Testing** (Days 5-6, 20-30h)
   - Link GEOS and PROJ libraries in CMakeLists
   - Run full test suite
   - Fix integration issues (type mismatches, API changes)

4. **Performance Optimization** (Days 6-7, 10-15h)
   - Profile R-tree insertion/search
   - Cache GEOS conversions
   - Benchmark against PostGIS

5. **Documentation** (Day 7, 5-10h)
   - Update FEATURE_PARITY_ROADMAP.md
   - Create WAVE_3_COMPLETION_REPORT.md
   - Write spatial function reference guide

---

## Success Criteria

### Technical Metrics
- ✅ All 12 agents complete without blocking errors
- ✅ ~5,100 lines of production code delivered
- ✅ 95%+ test coverage for new code
- ✅ Zero regressions in Phase 1/2 tests
- ✅ GEOS/PROJ libraries linked successfully

### Functional Metrics
- ✅ R-tree index: Insert 100K points, search < 10ms
- ✅ Spatial functions: All 15+ ST_* functions work correctly
- ✅ Multi-geometry: Parse/serialize MULTI* and GEOMETRYCOLLECTION
- ✅ Coordinate systems: Transform coordinates (4326 ↔ 3857)

### Acceptance Test
```sql
-- Create spatial index
CREATE INDEX idx_stores_location ON stores USING RTREE(location);

-- Spatial query with functions
SELECT
    s.name,
    ST_Distance(s.location, ST_Point(-122.4, 37.8)) as dist_meters,
    ST_Area(s.delivery_zone) as area_sqm,
    s.location::geography  -- Uses SRID
FROM stores s
WHERE ST_DWithin(
    ST_Transform(s.location, 4326),  -- Transform to WGS84
    ST_Point(-122.4, 37.8),
    5000.0
)
ORDER BY dist_meters
LIMIT 10;
```

---

## Risk Mitigation

### High-Risk Items

1. **R-tree Performance** (Agent R3)
   - Risk: May not match PostGIS
   - Monitor: Benchmark insertion/search early
   - Fallback: Use libspatialindex if needed

2. **GEOS Type Conversion** (Agent G1)
   - Risk: Conversion overhead
   - Monitor: Profile hot paths
   - Fallback: Cache conversions aggressively

3. **PROJ Thread Safety** (Agent S2)
   - Risk: Context leaks
   - Monitor: Run with Valgrind
   - Fallback: Use thread-local contexts

### Medium-Risk Items

4. **Agent Code Quality**
   - Risk: Bugs in generated code
   - Monitor: Comprehensive testing
   - Mitigation: Manual code review

5. **Library API Changes**
   - Risk: GEOS/PROJ API differences
   - Monitor: Check versions at runtime
   - Mitigation: Test on multiple versions

---

## Timeline

**Week 1** (Days 1-7):
- Day 1: Launch all 12 agents
- Days 2-4: Agent outputs, code review, first integration
- Days 5-6: Testing, bug fixes, performance tuning
- Day 7: Documentation, final testing

**Week 2** (Days 8-10, if needed):
- Days 8-9: Performance optimization, edge case handling
- Day 10: Final acceptance test, Wave 3 completion report

**Best Case**: 7 days
**Expected**: 10-12 days
**Worst Case**: 14 days

---

## Next Steps

1. ✅ **Prerequisites Complete**:
   - GEOS and PROJ libraries detected
   - CMakeLists.txt configured
   - Build system ready

2. **Ready to Launch**:
   - Review this plan
   - Prepare agent prompts (12 agents)
   - Set up monitoring dashboard

3. **Launch Command** (when ready):
   ```bash
   # Example: Launch Agent R1
   claude-code --agent "Agent R1: R-tree Core Structure" \
     --context "/home/dcalford/CliWork/ScratchBird" \
     --scope "20-30 hours" \
     --deliverables "include/scratchbird/core/rtree.h, src/core/rtree_node.cpp"
   ```

---

**Status**: ✅ **READY TO LAUNCH**
**Libraries**: ✅ GEOS 3.13.1, PROJ 9.5.1 detected
**Build System**: ✅ CMake configured
**Foundation**: ✅ Wave 1 spatial types complete (2,095 lines)

**Recommendation**: Launch all 12 agents in parallel for maximum speed (1-2 weeks to complete)

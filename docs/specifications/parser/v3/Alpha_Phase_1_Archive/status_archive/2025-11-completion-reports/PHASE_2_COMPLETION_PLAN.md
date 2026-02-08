# Phase 2: Completion Plan and Current Status

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: October 28, 2025
**Author**: Analysis based on codebase audit and FEATURE_PARITY_ROADMAP.md
**Purpose**: Comprehensive plan for completing Phase 2: Competitive Parity

---

## Current Status Summary

### ✅ **Completed Tasks** (60% of Phase 2 complete)

**Wave 1 (October 28, 2025)** - 4,713 lines:
- ✅ **Task 9.1**: Core Spatial Types (POINT, LINESTRING, POLYGON, WKT, WKB) - 2,095 lines
- ✅ **Task 12**: Array Functions SQL Integration - 1,175 lines (verified Oct 28)
- ✅ **Task 13**: Text Search Functions SQL Integration - 1,878 lines (verified Oct 28)

**Wave 2 (October 28, 2025)** - ~2,400 lines:
- ✅ **Task 10.1**: Triggers (BEFORE/AFTER on INSERT/UPDATE/DELETE) - ~885 lines
- ✅ **Task 11.1**: Common Table Expressions (CTEs) - ~707 lines
- ✅ **Task 11.2**: Subqueries (SCALAR, IN, EXISTS, NOT IN) - ~725 lines

**Total Completed**: ~7,113 lines across 6 major features

---

### ⏳ **Remaining Tasks** (40% of Phase 2 remaining)

| Task | Effort (Manual) | Effort (AI-Assisted) | Priority | Complexity |
|------|----------------|---------------------|----------|------------|
| **9.2** R-tree Indexes | 120-180h (3-4 weeks) | 20-30h | 🎯 HIGH | Very High |
| **9.3** Spatial Functions | 100-150h (2.5-3.5 weeks) | 15-25h | 📊 MEDIUM | High |
| **9.4** Multi-Geometry Types | 60-90h (1.5-2 weeks) | 10-15h | 📊 MEDIUM | Medium |
| **9.5** Coordinate Systems | 60-90h (1.5-2 weeks) | 10-15h | 📊 MEDIUM | Medium |
| **10.2** Stored Procedures | 120-180h (3-4 weeks) | 20-30h | 🎯 HIGH | Very High |
| **TOTAL** | **460-690 hours** | **75-115 hours** | - | - |

**Timeline Estimate**:
- Manual implementation: 3-4 months (1 developer, full-time)
- AI-assisted: 2-3 weeks (85% time reduction via autonomous agents)

---

## Task Breakdown

### Task 9.2: R-tree Spatial Indexes 🎯 HIGH PRIORITY

**Effort**: 120-180 hours → **20-30 hours with AI agents**
**Complexity**: Very High (advanced data structure, query planner integration)
**Prerequisite**: Task 9.1 (Spatial Types) ✅ Complete

#### Scope

Implement R-tree (R*-tree variant) spatial index for efficient geometric queries.

#### Components Required

1. **R-tree Data Structure** (40-60h → 7-10h with AI)
   - Internal and leaf node structures
   - Minimum Bounding Rectangle (MBR) calculations
   - Tree balancing algorithms (R*-tree split strategies)
   - Page-level serialization (fits ScratchBird's page model)

2. **Insertion Algorithm** (25-35h → 4-6h with AI)
   - ChooseLeaf - find optimal insertion location
   - SplitNode - quadratic or R*-tree split
   - AdjustTree - propagate MBR changes upward
   - Overflow handling

3. **Search Algorithm** (20-30h → 3-5h with AI)
   - Bounding box intersection tests
   - Recursive tree traversal
   - Result set collection

4. **Deletion Algorithm** (25-35h → 4-6h with AI)
   - FindLeaf - locate entry to delete
   - CondenseTree - handle underflow
   - Reinsert orphaned entries

5. **Query Planner Integration** (10-20h → 2-3h with AI)
   - Cost model for spatial queries
   - Index selection for ST_* predicates
   - Statistics collection for spatial data

#### Files to Create/Modify

**New Files** (~1,800 lines total):
- `include/scratchbird/core/rtree.h` (~400 lines)
- `src/core/rtree.cpp` (~800 lines)
- `include/scratchbird/core/rtree_node.h` (~150 lines)
- `src/core/rtree_node.cpp` (~250 lines)
- `tests/unit/test_rtree.cpp` (~200 lines)

**Modified Files**:
- `include/scratchbird/core/index.h` - Add IndexType::RTREE
- `src/core/catalog_manager.cpp` - R-tree catalog support
- `src/optimizer/query_planner.cpp` - Spatial index path generation
- `src/optimizer/cost_model.cpp` - R-tree cost estimation
- `src/optimizer/selectivity_estimator.cpp` - Spatial selectivity

#### Implementation Strategy

**Recommended**: AI agent approach (similar to Wave 1/Wave 2)

**Agent Configuration**:
- **Agent R1**: R-tree Core Structure (7-10h scope)
  - Implement RTreeNode class hierarchy
  - Implement MBR calculations
  - Implement tree navigation

- **Agent R2**: Insertion & Balancing (4-6h scope)
  - Implement ChooseLeaf algorithm
  - Implement SplitNode (R*-tree strategy)
  - Implement AdjustTree

- **Agent R3**: Search & Deletion (7-11h scope)
  - Implement bounding box search
  - Implement FindLeaf and CondenseTree
  - Handle reinserts

- **Agent R4**: Query Planner Integration (2-3h scope)
  - Add R-tree cost model
  - Add spatial predicate recognition
  - Generate R-tree index paths

**Total Agent Time**: 20-30 hours → **3-4 days of developer oversight**

#### Reference Implementation

**Model**: PostgreSQL's GiST (Generalized Search Tree) for spatial data
**Algorithm**: R*-tree (enhanced R-tree with forced reinsert)
**Paper**: "The R*-tree: An Efficient and Robust Access Method for Points and Rectangles" (Beckmann et al., 1990)

#### Success Criteria

```sql
-- Create spatial index
CREATE INDEX idx_stores_location ON stores USING RTREE(location);

-- Efficient bounding box query
EXPLAIN SELECT * FROM stores
WHERE ST_Contains(
    ST_MakeEnvelope(-122.5, 37.7, -122.3, 37.9),
    location
);
-- Should show "Index Scan using idx_stores_location"

-- Performance target: <10ms for 100K points
```

---

### Task 9.3: Spatial Functions 📊 MEDIUM PRIORITY

**Effort**: 100-150 hours → **15-25 hours with AI + GEOS**
**Complexity**: High (requires GEOS library integration)
**Prerequisite**: Task 9.1 (Spatial Types) ✅ Complete

#### Scope

Implement 15+ spatial relationship and measurement functions via GEOS library.

#### Functions to Implement

**Distance/Measurement** (30-45h → 5-7h with AI):
- `ST_Distance(geom1, geom2) → FLOAT64`
- `ST_Area(polygon) → FLOAT64`
- `ST_Length(linestring) → FLOAT64`
- `ST_Perimeter(polygon) → FLOAT64`

**Spatial Relationships** (40-60h → 6-10h with AI):
- `ST_Contains(geom1, geom2) → BOOLEAN`
- `ST_Within(geom1, geom2) → BOOLEAN`
- `ST_Intersects(geom1, geom2) → BOOLEAN`
- `ST_Crosses(geom1, geom2) → BOOLEAN`
- `ST_Overlaps(geom1, geom2) → BOOLEAN`
- `ST_Touches(geom1, geom2) → BOOLEAN`
- `ST_Disjoint(geom1, geom2) → BOOLEAN`

**Geometric Operations** (20-30h → 3-5h with AI):
- `ST_Buffer(geom, distance) → GEOMETRY`
- `ST_Intersection(geom1, geom2) → GEOMETRY`
- `ST_Union(geom1, geom2) → GEOMETRY`
- `ST_Difference(geom1, geom2) → GEOMETRY`

**Already Implemented** ✅:
- ST_AsText(geom) → WKT - Wave 1 Agent 1
- ST_AsBinary(geom) → WKB - Wave 1 Agent 1
- ST_GeomFromText(wkt) → GEOMETRY - Wave 1 Agent 1
- ST_GeomFromWKB(wkb) → GEOMETRY - Wave 1 Agent 1

#### GEOS Integration Strategy

**Library**: GEOS (Geometry Engine, Open Source)
**Rationale**: Industry-standard C++ library used by PostGIS, QGIS, etc.
**Integration Point**: Thin wrapper around ScratchBird's geometry types

**Files to Create/Modify** (~1,200 lines):
- `include/scratchbird/geo/geos_wrapper.h` (~150 lines)
- `src/geo/geos_wrapper.cpp` (~400 lines) - Convert ScratchBird → GEOS
- `src/sblr/executor.cpp` - 15 function handlers (~500 lines)
- `tests/unit/test_spatial_functions.cpp` (~150 lines)

#### Implementation Strategy

**Agent Configuration**:
- **Agent G1**: GEOS Integration (5-7h scope)
  - Implement ScratchBird ↔ GEOS conversions
  - Wrap GEOS API
  - Handle error cases

- **Agent G2**: Measurement Functions (3-5h scope)
  - Implement ST_Distance, ST_Area, ST_Length, ST_Perimeter
  - Add executor handlers
  - Add parser/bytecode support

- **Agent G3**: Relationship Functions (6-10h scope)
  - Implement 7 relationship functions
  - Add executor handlers
  - Optimize predicate evaluation

- **Agent G4**: Geometric Operations (3-5h scope)
  - Implement ST_Buffer, ST_Intersection, ST_Union, ST_Difference
  - Handle complex geometry results

**Total Agent Time**: 15-25 hours → **2-3 days of developer oversight**

#### Success Criteria

```sql
-- Distance calculation
SELECT name, ST_Distance(location, ST_Point(-122.4, 37.8)) as dist_meters
FROM stores
WHERE ST_Distance(location, ST_Point(-122.4, 37.8)) < 1000
ORDER BY dist_meters;

-- Containment check
SELECT COUNT(*) FROM buildings
WHERE ST_Contains(
    (SELECT boundary FROM parcels WHERE id = 123),
    footprint
);

-- Buffer operation
UPDATE flood_zones SET buffer_100m = ST_Buffer(boundary, 100);
```

---

### Task 9.4: Multi-Geometry Types 📊 MEDIUM PRIORITY

**Effort**: 60-90 hours → **10-15 hours with AI**
**Complexity**: Medium (similar to existing geometry types)
**Prerequisite**: Task 9.1 (Spatial Types) ✅ Complete

#### Scope

Implement composite geometry types for multi-part features.

#### Types to Implement

1. **MULTIPOINT** (15-20h → 2-3h with AI)
   - Collection of points
   - WKT: `MULTIPOINT((0 0), (1 1))`

2. **MULTILINESTRING** (15-20h → 2-3h with AI)
   - Collection of linestrings
   - WKT: `MULTILINESTRING((0 0, 1 1), (2 2, 3 3))`

3. **MULTIPOLYGON** (20-30h → 4-6h with AI)
   - Collection of polygons with holes
   - WKT: `MULTIPOLYGON(((0 0, 4 0, 4 4, 0 4, 0 0)))`

4. **GEOMETRYCOLLECTION** (10-20h → 2-3h with AI)
   - Heterogeneous collection
   - WKT: `GEOMETRYCOLLECTION(POINT(1 1), LINESTRING(0 0, 1 1))`

#### Files to Create (~800 lines):
- `include/scratchbird/geo/multi_geometry.h` (~250 lines)
- `src/geo/multi_geometry.cpp` (~400 lines)
- `tests/unit/test_multi_geometry.cpp` (~150 lines)

#### Implementation Strategy

**Single Agent**: Agent M1 (10-15h scope)
- Implement all 4 multi-geometry types
- Extend WKT/WKB parsers
- Add type conversions
- Integrate with existing spatial functions

---

### Task 9.5: Coordinate Systems 📊 MEDIUM PRIORITY

**Effort**: 60-90 hours → **10-15 hours with AI + PROJ**
**Complexity**: Medium (mostly PROJ library integration)
**Prerequisite**: Task 9.1 (Spatial Types) ✅ Complete

#### Scope

Add SRID (Spatial Reference ID) support and coordinate transformations.

#### Components

1. **SRID Storage** (10-15h → 2-3h with AI)
   - Add `srid` field to geometry types
   - Modify WKT/WKB to include SRID
   - Update catalog for SRID metadata

2. **PROJ Integration** (30-45h → 5-7h with AI)
   - Integrate PROJ library (coordinate transformations)
   - Implement ST_Transform(geom, from_srid, to_srid)
   - Cache transformation contexts

3. **Geographic vs. Projected** (20-30h → 3-5h with AI)
   - Distance calculations in geographic coordinates
   - ST_DistanceSphere for lat/lon data
   - Geodetic vs. Cartesian operations

#### Files to Create/Modify (~700 lines):
- `include/scratchbird/geo/srid.h` (~100 lines)
- `src/geo/srid.cpp` (~200 lines)
- `include/scratchbird/geo/proj_wrapper.h` (~50 lines)
- `src/geo/proj_wrapper.cpp` (~250 lines)
- `tests/unit/test_srid.cpp` (~100 lines)

#### Implementation Strategy

**Agent Configuration**:
- **Agent S1**: SRID Infrastructure (2-3h scope)
- **Agent S2**: PROJ Integration (5-7h scope)
- **Agent S3**: Geographic Operations (3-5h scope)

---

### Task 10.2: Stored Procedures 🎯 HIGH PRIORITY

**Effort**: 120-180 hours → **20-30 hours with AI agents**
**Complexity**: Very High (full procedural language design + implementation)
**Prerequisite**: Task 10.1 (Triggers) ✅ Complete

#### Scope

Design and implement PL/ScratchBird procedural language.

#### Language Features Required

1. **Variable Declarations** (15-20h → 3-4h with AI)
   ```sql
   DECLARE
       v_count INT;
       v_name VARCHAR(100);
   ```

2. **Control Flow** (30-40h → 5-7h with AI)
   - IF/ELSIF/ELSE
   - LOOP/EXIT/CONTINUE
   - WHILE loops
   - FOR loops (range and cursor)

3. **SQL Execution** (25-35h → 4-6h with AI)
   - SELECT INTO
   - Dynamic SQL (EXECUTE)
   - Cursor support

4. **Exception Handling** (20-30h → 3-5h with AI)
   - BEGIN/EXCEPTION/END blocks
   - RAISE statements
   - Exception handlers

5. **Functions & Procedures** (30-45h → 5-8h with AI)
   - CREATE FUNCTION
   - CREATE PROCEDURE
   - RETURN statements
   - OUT parameters

#### Architecture

**4-Layer Implementation**:
1. **Parser** - Extend SQL parser for procedural syntax
2. **Semantic Analyzer** - Type checking, scope resolution
3. **Bytecode Compiler** - Compile to SBLR bytecode (extended opcodes)
4. **Execution Engine** - Interpret procedural bytecode

#### Files to Create/Modify (~3,500 lines):
- `include/scratchbird/parser/pl_parser.h` (~200 lines)
- `src/parser/pl_parser.cpp` (~600 lines)
- `include/scratchbird/sblr/pl_compiler.h` (~150 lines)
- `src/sblr/pl_compiler.cpp` (~800 lines)
- `src/sblr/executor.cpp` - Add procedural opcodes (~600 lines)
- `include/scratchbird/core/procedure_catalog.h` (~100 lines)
- `src/core/procedure_catalog.cpp` (~400 lines)
- `tests/unit/test_plscratchbird.cpp` (~450 lines)
- `tests/integration/test_procedures.cpp` (~200 lines)

#### Implementation Strategy

**Recommended**: 4-agent parallel approach (like Wave 2)

**Agent Configuration**:
- **Agent P1**: Parser + AST (30-40h scope → 5-7h real time)
- **Agent P2**: Semantic Analysis (20-30h scope → 3-5h real time)
- **Agent P3**: Bytecode Compiler (35-50h scope → 6-8h real time)
- **Agent P4**: Execution Engine (35-50h scope → 6-8h real time)

**Total Agent Time**: 20-30 hours → **3-5 days of developer oversight**

#### Example Procedure

```sql
CREATE FUNCTION calculate_tax(
    amount DECIMAL,
    state VARCHAR(2)
) RETURNS DECIMAL AS $$
DECLARE
    tax_rate DECIMAL;
    tax_amount DECIMAL;
BEGIN
    -- Lookup tax rate
    SELECT rate INTO tax_rate
    FROM tax_rates
    WHERE state_code = state;

    IF NOT FOUND THEN
        RAISE EXCEPTION 'Unknown state: %', state;
    END IF;

    -- Calculate tax
    tax_amount := amount * tax_rate;

    RETURN tax_amount;
EXCEPTION
    WHEN OTHERS THEN
        RETURN 0;
END;
$$ LANGUAGE plscratchbird;
```

#### Success Criteria

```sql
-- Complex business logic works
SELECT calculate_bonus(emp_id) FROM employees;

-- Loops and control flow work
CALL process_pending_orders();

-- Exception handling works
SELECT safe_divide(10, 0); -- Returns NULL instead of error
```

---

## Implementation Roadmap

### Recommended Phase 2 Completion Order

**Wave 3: Spatial Completion** (Parallel agents, 1-2 weeks)
1. Task 9.2: R-tree Indexes (Agents R1-R4, 20-30h)
2. Task 9.3: Spatial Functions (Agents G1-G4, 15-25h)
3. Task 9.4: Multi-Geometry (Agent M1, 10-15h)
4. Task 9.5: Coordinate Systems (Agents S1-S3, 10-15h)

**Wave 4: Business Logic Completion** (Parallel agents, 1 week)
5. Task 10.2: Stored Procedures (Agents P1-P4, 20-30h)

**Total Time**: 75-115 hours of agent work → **2-3 weeks of developer oversight**

---

## Resource Requirements

### Developer Time

**Wave 3**: 5-10 hours/day × 10-14 days = **50-140 hours of oversight**
- Agent monitoring and debugging
- Integration testing
- Code review and refinement

**Wave 4**: 5-8 hours/day × 5-7 days = **25-56 hours of oversight**
- PL/ScratchBird language design decisions
- Complex debugging
- Performance optimization

**Total Developer Time**: 75-196 hours (2-5 weeks)

### Compute Resources

**Per Agent**:
- Claude Opus/Sonnet API usage
- ~$5-20 per agent depending on complexity
- Total cost: ~$50-200 for all agents

### External Dependencies

**Libraries to Integrate**:
1. **GEOS** (Geometry Engine) - Spatial functions
   - Installation: `sudo apt-get install libgeos-dev`
   - License: LGPL (compatible)

2. **PROJ** (Coordinate transformations) - CRS support
   - Installation: `sudo apt-get install libproj-dev`
   - License: MIT (compatible)

---

## Risk Assessment

### High-Risk Items

1. **R-tree Performance** (Task 9.2)
   - Risk: Implementation may not match PostGIS performance
   - Mitigation: Benchmark early, use R*-tree variant
   - Fallback: Integrate libspatialindex

2. **GEOS Integration** (Task 9.3)
   - Risk: Type conversion overhead
   - Mitigation: Cache conversions, profile hotspots
   - Fallback: Implement critical functions natively

3. **PL/ScratchBird Complexity** (Task 10.2)
   - Risk: Language design incompatibility with PostgreSQL
   - Mitigation: Follow PL/pgSQL closely
   - Fallback: Simplify feature set initially

### Medium-Risk Items

1. **Agent Code Quality** (All tasks)
   - Risk: Generated code may have bugs
   - Mitigation: Comprehensive testing, code review
   - Fallback: Manual refinement

2. **Integration Complexity** (All tasks)
   - Risk: New features may break existing code
   - Mitigation: Run full test suite, regression testing
   - Fallback: Feature flags for gradual rollout

---

## Success Metrics

### Phase 2 Completion Criteria

**Technical Metrics**:
- ✅ All 5 remaining tasks implemented
- ✅ 95%+ test coverage for new code
- ✅ Zero regressions in existing tests
- ✅ Performance within 2x of PostgreSQL/PostGIS

**Functional Metrics**:
- ✅ GIS application can run (spatial queries < 100ms)
- ✅ Business logic works (stored procedures execute correctly)
- ✅ Complex analytical queries work (CTEs, subqueries, window functions)
- ✅ Array and text operations work
- ✅ Triggers enforce business rules

**Acceptance Test**:
```sql
-- Combined Phase 2 functionality test
CREATE FUNCTION find_nearby_stores(
    user_location GEOMETRY,
    max_distance FLOAT
) RETURNS TABLE(store_id INT, distance FLOAT, tags TEXT[]) AS $$
BEGIN
    RETURN QUERY
    SELECT
        s.id,
        ST_Distance(s.location, user_location) as dist,
        s.tags
    FROM stores s
    WHERE ST_DWithin(s.location, user_location, max_distance)
        AND s.tags && ARRAY['open', 'delivery']
    ORDER BY dist
    LIMIT 10;
END;
$$ LANGUAGE plscratchbird;

-- Execute
SELECT * FROM find_nearby_stores(
    ST_Point(-122.4, 37.8),
    5000.0
);
```

---

## Conclusion

**Phase 2 Status**: **60% Complete** (6 of 10 major tasks done)

**Remaining Work**: 5 major tasks, 460-690 hours manual (75-115 hours with AI)

**Recommended Approach**:
1. Deploy AI agent waves for parallel implementation
2. Integrate GEOS and PROJ libraries
3. Focus developer time on oversight, testing, and refinement

**Timeline**: **2-3 weeks** with AI-assisted development

**Next Step**: Launch Wave 3 agents for spatial completion tasks

---

**See Also**:
- `/docs/development/AI_PARALLEL_DEVELOPMENT_GUIDE.md` - Agent deployment guide
- `/docs/specifications/parser/v3/types/MULTI_GEOMETRY_TYPES_SPEC.md` - Detailed spatial design
- `/docs/specifications/parser/v3/parser/05_PSQL_PROCEDURAL_LANGUAGE.md` - PL/ScratchBird language spec
- `/docs/specifications/parser/v3/status/WAVE_1_COMPLETION_REPORT.md` - Wave 1 results
- `/docs/specifications/parser/v3/status/WAVE_2_COMPLETION_SUMMARY.md` - Wave 2 results

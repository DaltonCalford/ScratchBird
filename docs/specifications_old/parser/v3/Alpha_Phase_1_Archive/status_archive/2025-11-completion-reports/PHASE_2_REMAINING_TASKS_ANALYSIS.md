# Phase 2: Competitive Parity - Remaining Tasks Analysis

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: October 28, 2025
**Status**: Analysis Complete
**Purpose**: Detailed breakdown of all remaining Phase 2 tasks with priorities and implementation strategy

---

## Executive Summary

**Phase 2 Current Status**: 3 of 5 major task groups complete (60%)

**Completed** ✅:
- Task 10.1: Triggers (Wave 2) - 100% complete
- Task 11: CTEs and Subqueries (Wave 2) - 100% complete

**Partially Complete** ⚠️:
- Task 9: Spatial Types - 9.1 complete (infrastructure), 9.2-9.5 pending
- Task 12: Array Functions - Executor complete, SQL integration pending (2-3h)
- Task 13: Text Search - Foundation complete, SQL integration pending (4-6h)

**Not Started** ❌:
- Task 10.2: Stored Procedures (120-180h)

**Total Remaining Effort**: 512-812 hours (~3-5 months with 1 developer)

---

## Priority Ranking (Recommended Order)

### 🔥 Quick Wins (6-9 hours total)
1. **Task 12: Array Functions SQL Integration** (2-3h) - Highest ROI
2. **Task 13: Text Search SQL Integration** (4-6h) - Highest ROI

### 🎯 High Priority (240-360 hours)
3. **Task 10.2: Stored Procedures** (120-180h) - Critical for business logic
4. **Task 9.2: Spatial Indexes (R-tree)** (120-180h) - Required for GIS performance

### 📊 Medium Priority (260-390 hours)
5. **Task 9.3: Spatial Functions** (100-150h) - Completes spatial support
6. **Task 9.4: Additional Spatial Types** (60-90h) - Multi-geometry support
7. **Task 9.5: CRS Support** (60-90h) - Coordinate transformations

---

## Task 12: Array Functions SQL Integration ⚡ QUICK WIN

**Status**: ⚠️ 90% complete (executor done, SQL integration pending)
**Effort**: 2-3 hours
**Priority**: 🔥 **HIGHEST** - Immediate business value

### What's Complete ✅
- ✅ 14 array functions implemented in executor (750 lines)
- ✅ 3 array operators (&&, @>, <@)
- ✅ 33 opcodes defined
- ✅ Full NULL handling
- ✅ GROUP BY support for ARRAY_AGG

**Functions Ready**:
- ARRAY_AGG, ARRAY_TO_STRING, STRING_TO_ARRAY
- ARRAY_APPEND, ARRAY_PREPEND, ARRAY_CAT
- ARRAY_REMOVE, ARRAY_REPLACE
- ARRAY_LENGTH, ARRAY_DIMS, ARRAY_UPPER, ARRAY_LOWER

### What's Missing ⏳

1. **Parser Support** (45-60 min)
   - Add ARRAY[...] literal syntax
   - Add array operator precedence (&&, @>, <@)
   - Add function name mappings

2. **Bytecode Generation** (30-45 min)
   - Wire up array literal bytecode
   - Wire up array operator bytecode
   - Wire up function call bytecode

3. **Testing** (45-60 min)
   - Create integration tests
   - Test ARRAY_AGG with GROUP BY
   - Test array operators in WHERE clauses

### Implementation Steps

```sql
-- Target functionality:
SELECT ARRAY[1, 2, 3];
SELECT ARRAY_AGG(id) FROM users GROUP BY dept;
SELECT * FROM products WHERE tags && ARRAY['sale', 'new'];
SELECT ARRAY_LENGTH(ARRAY[1,2,3], 1); -- Returns 3
```

**Files to Modify**:
1. `src/parser/parser.cpp` - Add parseArrayLiteral()
2. `src/sblr/bytecode_generator.cpp` - Add array literal/operator bytecode
3. `tests/integration/test_array_functions.cpp` - 20+ tests

**Estimated Lines**: ~200 lines (parser ~80, bytecode ~60, tests ~60)

---

## Task 13: Text Search SQL Integration ⚡ QUICK WIN

**Status**: ⚠️ 85% complete (foundation done, handlers/SQL integration pending)
**Effort**: 4-6 hours
**Priority**: 🔥 **HIGHEST** - Immediate business value

### What's Complete ✅
- ✅ Regex engine helpers (matchRegex, regexMatches, regexReplace, regexSplit)
- ✅ 21 opcodes defined
- ✅ 87 test cases written
- ✅ ILIKE ready for integration
- ✅ All function designs complete

**Functions/Operators Ready**:
- ILIKE (case-insensitive LIKE)
- Regex operators: ~, ~*, !~, !~*
- REGEXP_MATCHES, REGEXP_REPLACE
- SPLIT_PART, STRING_TO_TABLE, UNNEST_TEXT
- STRPOS, POSITION, OVERLAY, QUOTE
- INITCAP, ASCII, CHR, REPEAT, REVERSE

### What's Missing ⏳

1. **Opcode Handlers** (3-4 hours)
   - Implement 21 opcode handlers in executor
   - Wire up regex engine helpers
   - Handle multi-row results for STRING_TO_TABLE

2. **Parser Support** (1-2 hours)
   - Add regex operator parsing (~, ~*, !~, !~*)
   - Add function name mappings
   - Add ILIKE keyword

3. **Bytecode Generation** (45-60 min)
   - Wire up regex operator bytecode
   - Wire up function call bytecode

### Implementation Steps

```sql
-- Target functionality:
SELECT * FROM users WHERE email ~ '^[a-z]+@example\.com$';
SELECT * FROM logs WHERE message ILIKE '%error%';
SELECT REGEXP_MATCHES('test123', '\d+');
SELECT SPLIT_PART('a,b,c', ',', 2); -- Returns 'b'
```

**Files to Modify**:
1. `src/sblr/executor.cpp` - Add 21 opcode handlers (~300 lines)
2. `src/parser/parser.cpp` - Add regex operator parsing (~80 lines)
3. `src/parser/lexer.cpp` - Add ILIKE keyword (~5 lines)
4. `src/sblr/bytecode_generator.cpp` - Wire up bytecode (~60 lines)
5. `tests/integration/test_text_search.cpp` - Integration tests (~100 lines)

**Estimated Lines**: ~545 lines

---

## Task 10.2: Stored Procedures 🎯 HIGH PRIORITY

**Status**: ❌ Not started
**Effort**: 120-180 hours (3-4 weeks)
**Priority**: 🎯 **HIGH** - Critical for business logic

### Scope

Design and implement PL/ScratchBird - a PostgreSQL/PL/pgSQL-inspired procedural language for stored procedures and functions.

### Requirements

1. **Language Design** (20-30h)
   - Variable declarations (DECLARE section)
   - Data types (same as SQL)
   - Assignment statements (variable := expression)
   - Control flow (IF, CASE, LOOP, WHILE, FOR)
   - Exception handling (BEGIN/EXCEPTION/END)
   - RETURN statements (for functions)
   - SQL statement execution (SELECT INTO, INSERT, UPDATE, DELETE)

2. **Parser Extensions** (30-45h)
   - CREATE FUNCTION/PROCEDURE syntax
   - PL/ScratchBird block structure
   - Variable declarations
   - Control flow statements
   - Exception blocks

3. **Catalog Support** (15-25h)
   - Function catalog (pg_proc equivalent)
   - Store function metadata (parameters, return type, body)
   - Function lookup and versioning

4. **Execution Engine** (40-60h)
   - PL/ScratchBird bytecode compiler
   - Variable scope management
   - Control flow execution
   - Exception handling runtime
   - SQL statement integration

5. **Testing** (15-20h)
   - Unit tests for each language feature
   - Integration tests with triggers
   - Performance tests

### Example Syntax

```sql
CREATE FUNCTION calculate_bonus(emp_id INT) RETURNS NUMERIC AS $$
DECLARE
    salary NUMERIC;
    years_service INT;
    bonus NUMERIC;
BEGIN
    SELECT s.amount, EXTRACT(YEAR FROM AGE(NOW(), e.hire_date))
    INTO salary, years_service
    FROM employees e
    JOIN salaries s ON e.id = s.emp_id
    WHERE e.id = emp_id;

    IF years_service < 1 THEN
        bonus := 0;
    ELSIF years_service < 5 THEN
        bonus := salary * 0.05;
    ELSIF years_service < 10 THEN
        bonus := salary * 0.10;
    ELSE
        bonus := salary * 0.15;
    END IF;

    RETURN bonus;
EXCEPTION
    WHEN NO_DATA_FOUND THEN
        RETURN 0;
END;
$$ LANGUAGE plscratchbird;
```

### Implementation Strategy

**Recommended Approach**: Use AI agents similar to Wave 2

**Agent Breakdown**:
- Agent 1: Parser + AST (30-40h scope)
- Agent 2: Catalog + Semantic Analysis (20-30h scope)
- Agent 3: Bytecode Compiler (35-50h scope)
- Agent 4: Execution Engine (35-50h scope)

**Total**: 120-170 hours of agent work → 20-30 hours of developer time

---

## Task 9.2: Spatial Indexes (R-tree) 🎯 HIGH PRIORITY

**Status**: ❌ Not started
**Effort**: 120-180 hours (3-4 weeks)
**Priority**: 🎯 **HIGH** - Required for GIS performance

### Scope

Implement R-tree spatial index for efficient bounding box queries on geometric data.

### Requirements

1. **R-tree Data Structure** (40-60h)
   - Node structure (internal vs. leaf nodes)
   - Minimum Bounding Rectangle (MBR) calculations
   - Tree balancing (R*-tree variant recommended)
   - Serialization/deserialization

2. **Insertion Algorithm** (25-35h)
   - ChooseLeaf algorithm
   - SplitNode algorithm (quadratic or linear)
   - AdjustTree algorithm
   - Overflow handling

3. **Search Algorithm** (20-30h)
   - Bounding box intersection tests
   - Recursive tree traversal
   - Result collection

4. **Deletion Algorithm** (25-35h)
   - FindLeaf algorithm
   - CondenseTree algorithm
   - Underflow handling

5. **Query Planner Integration** (10-20h)
   - Cost estimation for spatial queries
   - Index selection for spatial predicates
   - Statistics collection

### Reference Implementation

**Model**: PostgreSQL's GiST (Generalized Search Tree) for spatial data

### Files to Create/Modify

**New Files**:
- `include/scratchbird/core/rtree.h` (~400 lines)
- `src/core/rtree.cpp` (~800 lines)
- `tests/unit/test_rtree.cpp` (~300 lines)

**Modified Files**:
- `include/scratchbird/core/index.h` - Add R-tree index type
- `src/optimizer/query_planner.cpp` - Add spatial index selection
- `src/optimizer/cost_model.cpp` - Add R-tree cost estimation

**Estimated Lines**: ~1,800 lines

---

## Task 9.3: Spatial Functions 📊 MEDIUM PRIORITY

**Status**: ❌ Not started
**Effort**: 100-150 hours (2.5-3.5 weeks)
**Priority**: 📊 **MEDIUM** - Completes spatial support

### Scope

Implement core spatial relationship and measurement functions.

### Functions to Implement

**Distance/Measurement** (30-45h):
- ST_Distance(geom1, geom2) → FLOAT64
- ST_Area(polygon) → FLOAT64
- ST_Length(linestring) → FLOAT64
- ST_Perimeter(polygon) → FLOAT64

**Spatial Relationships** (40-60h):
- ST_Contains(geom1, geom2) → BOOLEAN
- ST_Within(geom1, geom2) → BOOLEAN
- ST_Intersects(geom1, geom2) → BOOLEAN
- ST_Crosses(geom1, geom2) → BOOLEAN
- ST_Overlaps(geom1, geom2) → BOOLEAN
- ST_Touches(geom1, geom2) → BOOLEAN
- ST_Disjoint(geom1, geom2) → BOOLEAN

**Geometric Operations** (20-30h):
- ST_Buffer(geom, distance) → GEOMETRY
- ST_Intersection(geom1, geom2) → GEOMETRY
- ST_Union(geom1, geom2) → GEOMETRY
- ST_Difference(geom1, geom2) → GEOMETRY

**Format Conversion** (10-15h):
- ST_AsText(geom) → VARCHAR (WKT)
- ST_AsBinary(geom) → BYTEA (WKB)
- ST_GeomFromText(wkt) → GEOMETRY
- ST_GeomFromWKB(wkb) → GEOMETRY

### Implementation Strategy

**Dependencies**: GEOS library (Geometry Engine Open Source)
- Handles complex geometric operations
- Industry-standard, used by PostGIS
- C++ API available

**Integration Approach**:
1. Add GEOS as CMake dependency
2. Create GEOS wrapper functions
3. Implement executor opcodes
4. Add parser support
5. Add bytecode generation

**Files to Modify**:
- `src/sblr/executor.cpp` - Add spatial function handlers (~600 lines)
- `include/scratchbird/sblr/opcodes.h` - Add ~20 opcodes
- `src/parser/parser.cpp` - Add function mappings (~50 lines)
- `CMakeLists.txt` - Add GEOS dependency
- `tests/integration/test_spatial_functions.cpp` (~400 lines)

**Estimated Lines**: ~1,200 lines

---

## Task 9.4: Additional Spatial Types 📊 MEDIUM PRIORITY

**Status**: ❌ Not started
**Effort**: 60-90 hours (1.5-2 weeks)
**Priority**: 📊 **MEDIUM** - Multi-geometry support

### Scope

Extend spatial type system to support collections of geometries.

### Types to Implement

1. **MULTIPOINT** (15-20h)
   - Collection of points
   - WKT: `MULTIPOINT((0 0), (1 1), (2 2))`
   - Storage: Array of points

2. **MULTILINESTRING** (15-20h)
   - Collection of linestrings
   - WKT: `MULTILINESTRING((0 0, 1 1), (2 2, 3 3))`
   - Storage: Array of linestrings

3. **MULTIPOLYGON** (20-30h)
   - Collection of polygons
   - WKT: `MULTIPOLYGON(((0 0, 4 0, 4 4, 0 4, 0 0)))`
   - Storage: Array of polygons

4. **GEOMETRYCOLLECTION** (10-20h)
   - Heterogeneous collection
   - WKT: `GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0, 1 1))`
   - Storage: Tagged union array

### Implementation

**Files to Modify**:
- `include/scratchbird/core/types.h` - Add multi-geometry types (~100 lines)
- `src/core/wkt_parser.cpp` - Add multi-geometry parsing (~200 lines)
- `src/core/wkb.cpp` - Add multi-geometry WKB support (~150 lines)
- `tests/unit/test_spatial_types.cpp` - Add tests (~200 lines)

**Estimated Lines**: ~650 lines

---

## Task 9.5: Coordinate Reference Systems 📊 MEDIUM PRIORITY

**Status**: ❌ Not started
**Effort**: 60-90 hours (1.5-2 weeks)
**Priority**: 📊 **MEDIUM** - Professional GIS features

### Scope

Add coordinate reference system (CRS) support for professional GIS applications.

### Requirements

1. **SRID Support** (20-30h)
   - Add SRID field to geometry types
   - Store SRID in spatial_ref_sys catalog table
   - Enforce SRID matching in operations
   - SRID validation

2. **PROJ Integration** (30-45h)
   - Integrate PROJ library
   - Implement ST_Transform(geom, target_srid)
   - Coordinate transformation pipeline
   - Error handling

3. **Geographic vs. Projected** (10-15h)
   - Distance calculations in meters (geographic)
   - Distance calculations in units (projected)
   - Spheroid/ellipsoid support

### Implementation

**Dependencies**: PROJ library (coordinate transformation)

**Files to Create/Modify**:
- `include/scratchbird/core/srid.h` (~150 lines)
- `src/core/srid.cpp` (~300 lines)
- `src/core/spatial_ref_sys.cpp` - Catalog table (~200 lines)
- `src/sblr/executor.cpp` - Add ST_Transform (~100 lines)
- `tests/unit/test_coordinate_systems.cpp` (~200 lines)

**Estimated Lines**: ~950 lines

---

## Recommended Implementation Order

### Phase 2A: Quick Wins (1 week, 6-9 hours)
**Goal**: Complete partially-done features for immediate value

1. ✅ **Array Functions SQL Integration** (2-3h)
   - Highest ROI: ~200 lines for full feature completion
   - Immediate PostgreSQL compatibility win

2. ✅ **Text Search SQL Integration** (4-6h)
   - High ROI: ~545 lines for full feature completion
   - Critical for log analysis, search features

**Deliverable**: Two complete feature sets ready for production

---

### Phase 2B: Business Logic (3-4 weeks, 120-180 hours)
**Goal**: Enable business logic in database

3. 🎯 **Stored Procedures** (120-180h)
   - Critical for business logic
   - Enables trigger procedures to be user-defined
   - Use AI agents for 85% time savings

**Deliverable**: PL/ScratchBird language with functions and procedures

---

### Phase 2C: Spatial Performance (3-4 weeks, 120-180 hours)
**Goal**: GIS query performance

4. 🎯 **R-tree Spatial Indexes** (120-180h)
   - Required for GIS performance
   - Enables fast bounding box queries
   - Use AI agents for implementation

**Deliverable**: Fast spatial queries with R-tree indexes

---

### Phase 2D: Spatial Completeness (6-8 weeks, 260-390 hours)
**Goal**: Full GIS feature parity

5. 📊 **Spatial Functions** (100-150h)
   - Complete geometric operations
   - GEOS integration

6. 📊 **Multi-Geometry Types** (60-90h)
   - MULTIPOINT, MULTILINESTRING, MULTIPOLYGON
   - GEOMETRYCOLLECTION

7. 📊 **Coordinate Systems** (60-90h)
   - SRID support
   - PROJ integration
   - ST_Transform

**Deliverable**: Full PostGIS-equivalent feature set

---

## Time Estimates Summary

| Phase | Tasks | Hours | Developer Time (with AI) | Duration |
|-------|-------|-------|-------------------------|----------|
| **2A: Quick Wins** | 12, 13 | 6-9h | 6-9h (manual) | 1 week |
| **2B: Business Logic** | 10.2 | 120-180h | 20-30h (AI agents) | 1 week |
| **2C: Spatial Perf** | 9.2 | 120-180h | 20-30h (AI agents) | 1 week |
| **2D: Spatial Complete** | 9.3-9.5 | 260-390h | 40-60h (AI agents) | 2-3 weeks |
| **TOTAL** | 8 tasks | 506-759h | 86-129h | 5-7 weeks |

**Time Savings with AI Agents**: ~85% (from 18-30 weeks to 5-7 weeks)

---

## Success Criteria

**Phase 2 Complete When**:
- ✅ All 8 remaining tasks implemented
- ✅ All tests passing
- ✅ Documentation complete
- ✅ Performance benchmarks meet targets
- ✅ Acceptance tests pass

**Acceptance Test Coverage**:
```sql
-- Array operations
SELECT ARRAY_AGG(id) FROM users GROUP BY dept;
SELECT * FROM products WHERE tags && ARRAY['sale'];

-- Text search
SELECT * FROM logs WHERE message ~ 'error|warning';
SELECT * FROM users WHERE email ILIKE '%@example.com';

-- Stored procedures
CREATE FUNCTION calculate_bonus(emp_id INT) RETURNS NUMERIC AS $$
  -- PL/ScratchBird code
END;

-- Spatial with indexes
CREATE INDEX idx_location ON stores USING RTREE (location);
SELECT * FROM stores WHERE ST_DWithin(location, ST_Point(0,0), 1000);

-- Spatial functions
SELECT ST_Area(boundary), ST_Distance(p1, p2) FROM parcels;

-- Multi-geometries
SELECT ST_AsText(geom) FROM features WHERE geom_type = 'MULTIPOLYGON';

-- Coordinate systems
SELECT ST_Transform(geom, 4326) FROM parcels WHERE srid = 3857;
```

---

## Next Steps

1. ✅ Complete this analysis document
2. 🔄 Start Phase 2A: Quick Wins
   - Begin with Task 12: Array Functions (2-3h)
   - Then Task 13: Text Search (4-6h)
3. ⏳ Plan Phase 2B: Design AI agent specs for stored procedures
4. ⏳ Execute remaining phases in order

**Estimated Completion**: 5-7 weeks from start of Phase 2A

---

**Analysis Complete**: October 28, 2025
**Next Action**: Begin Task 12 (Array Functions SQL Integration)

# Phase 2: Competitive Parity - Kickoff Plan

**Date**: October 28, 2025
**Status**: Phase 1 Complete ✅ - Starting Phase 2
**Total Effort**: 800-1,200 hours (5-7.5 months, 1 developer)
**Goal**: Enable ScratchBird to compete with PostgreSQL, MySQL, SQL Server for real-world applications

---

## Phase 1 Achievement Summary

Before starting Phase 2, let's acknowledge what was accomplished:

✅ **8/8 Critical Tasks Complete**
- Query Optimizer (cost-based planning, statistics, EXPLAIN)
- UPDATE/DELETE operations (MGA-compliant)
- All JOIN types (INNER, LEFT, RIGHT, FULL, CROSS, NATURAL)
- Aggregation (GROUP BY, HAVING, 5 aggregate functions)
- Sorting/Limiting (ORDER BY, LIMIT, OFFSET, NULLS handling)
- Window Functions (8 functions with frame clauses)
- JSON Functions (14 functions with nlohmann/json)
- Conditional Functions (COALESCE, NULLIF, CASE)

**Total Implementation**: 12,981 lines + 200+ tests

**Capabilities**: Basic OLTP and OLAP workloads

---

## Phase 2 Overview

### Strategic Goal

Enable ScratchBird to compete for **four critical market segments**:

1. **GIS/Mapping Applications** (Spatial Types + Functions)
2. **Business Applications** (Triggers + Stored Procedures)
3. **Analytics Applications** (CTEs + Subqueries)
4. **PostgreSQL Compatibility** (Array + Text Search Functions)

### Effort Breakdown

| Task | Feature | Hours | Priority |
|------|---------|-------|----------|
| 9 | Spatial Types + Functions | 420-630 | CRITICAL |
| 10 | Triggers + Stored Procedures | 200-300 | HIGH |
| 11 | CTEs + Subqueries | 110-170 | HIGH |
| 12 | Array Functions | 40-60 | MEDIUM |
| 13 | Text Search Functions | 50-80 | MEDIUM |

**Total**: 820-1,240 hours

---

## Task 9: Spatial Types and Functions (CRITICAL)

**Why First**: Largest single market segment - every mapping/GIS application needs this.

**Total Effort**: 420-630 hours (2.5-4 months)

### Subtask Breakdown

#### 9.1 Core Spatial Types (80-120 hours)

**Foundation for all spatial functionality**

**Implementation Order**:
1. **POINT Type** (20-30h)
   - 2D coordinate storage (x, y)
   - WKT input: `POINT(1.5 2.5)`
   - WKB binary format
   - Type system integration
   - Serialization/deserialization

2. **LINESTRING Type** (25-35h)
   - Array of points
   - WKT input: `LINESTRING(0 0, 1 1, 2 2)`
   - WKB binary format
   - Length calculation

3. **POLYGON Type** (30-45h)
   - Exterior ring + interior rings (holes)
   - WKT input: `POLYGON((0 0, 4 0, 4 4, 0 4, 0 0))`
   - WKB binary format
   - Area calculation
   - Point-in-polygon test

4. **WKT/WKB Parsers** (5-10h)
   - Parse Well-Known Text format
   - Generate WKT output
   - Binary serialization (WKB)

**Deliverable**: `CREATE TABLE locations (name VARCHAR, point POINT)` works

#### 9.2 Spatial Indexes (120-180 hours)

**Required for efficient spatial queries**

**R-tree Implementation**:
1. **R-tree Structure** (30-45h)
   - Node layout (internal + leaf nodes)
   - Minimum Bounding Rectangle (MBR)
   - Tree balancing
   - Page management

2. **R-tree Insertion** (30-45h)
   - Choose subtree algorithm
   - Split node algorithm (quadratic split)
   - Adjust tree propagation
   - MBR updates

3. **R-tree Search** (25-35h)
   - Bounding box queries
   - Range queries
   - K-nearest neighbor (KNN)

4. **R-tree Deletion** (20-30h)
   - Find leaf algorithm
   - Condense tree algorithm
   - Reinsert orphaned entries

5. **Query Planner Integration** (15-25h)
   - Spatial scan path
   - Cost estimation for spatial queries
   - Index selection for spatial predicates

**Deliverable**: Spatial queries use R-tree index automatically

#### 9.3 Spatial Functions (100-150 hours)

**Core PostGIS-compatible functions**

**Implementation Order**:
1. **Distance Functions** (20-30h)
   - `ST_Distance(geom1, geom2)` - Euclidean distance
   - `ST_DistanceSphere(geom1, geom2)` - Great circle distance

2. **Relationship Functions** (35-50h)
   - `ST_Contains(geom1, geom2)` - Full containment
   - `ST_Intersects(geom1, geom2)` - Any intersection
   - `ST_Within(geom1, geom2)` - Inverse of contains
   - `ST_Overlaps(geom1, geom2)` - Partial overlap

3. **Processing Functions** (25-35h)
   - `ST_Buffer(geom, distance)` - Create buffer zone
   - `ST_Intersection(geom1, geom2)` - Compute intersection
   - `ST_Union(geom1, geom2)` - Compute union

4. **Accessor Functions** (10-15h)
   - `ST_AsText(geom)` - Output as WKT
   - `ST_AsBinary(geom)` - Output as WKB
   - `ST_Area(geom)` - Calculate area
   - `ST_Length(geom)` - Calculate length

5. **Constructor Functions** (10-20h)
   - `ST_Point(x, y)` - Create point
   - `ST_MakeLine(points)` - Create linestring
   - `ST_MakePolygon(linestring)` - Create polygon

**Deliverable**: PostGIS-style spatial queries work

#### 9.4 Additional Spatial Types (60-90 hours)

**Multi-geometry support**

1. **MULTIPOINT** (15-20h)
2. **MULTILINESTRING** (15-20h)
3. **MULTIPOLYGON** (20-30h)
4. **GEOMETRYCOLLECTION** (10-20h)

**Deliverable**: Complex geometries supported

#### 9.5 Coordinate Reference Systems (60-90 hours)

**Geographic coordinate support**

1. **SRID Support** (20-30h)
   - SRID storage in geometry
   - SRID validation
   - Default SRID (4326 - WGS 84)

2. **Coordinate Transformations** (30-45h)
   - PROJ library integration
   - `ST_Transform(geom, srid)` function
   - Transformation caching

3. **Geographic vs Projected** (10-15h)
   - Geography type (ellipsoidal calculations)
   - Distinguish from geometry (planar)

**Deliverable**: Real-world geographic queries work

### Phase 2.1 Acceptance Test

```sql
-- GIS application example
CREATE TABLE restaurants (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    location POINT
);

CREATE INDEX idx_restaurant_location ON restaurants USING RTREE (location);

INSERT INTO restaurants VALUES
    (1, 'Pizza Place', ST_Point(-122.4194, 37.7749)),
    (2, 'Burger Joint', ST_Point(-122.4195, 37.7750)),
    (3, 'Sushi Bar', ST_Point(-122.4100, 37.7800));

-- Find restaurants within 1km of a point
SELECT name, ST_Distance(location, ST_Point(-122.4194, 37.7749)) as distance
FROM restaurants
WHERE ST_DWithin(location, ST_Point(-122.4194, 37.7749), 1000)
ORDER BY distance
LIMIT 5;
```

---

## Task 10: Triggers and Stored Procedures (HIGH)

**Why Second**: Business logic enforcement is critical for enterprise applications.

**Total Effort**: 200-300 hours (1-1.5 months)

### Subtask Breakdown

#### 10.1 Trigger Support (80-120 hours)

**Implementation Order**:
1. **Trigger Parser** (20-30h)
   - `CREATE TRIGGER` syntax
   - `DROP TRIGGER` syntax
   - Trigger timing (BEFORE/AFTER)
   - Trigger events (INSERT/UPDATE/DELETE)
   - FOR EACH ROW

2. **Trigger Catalog** (15-20h)
   - `pg_trigger` catalog table
   - Trigger metadata storage
   - Trigger enable/disable flag

3. **Trigger Execution** (35-50h)
   - Executor integration
   - OLD/NEW row references
   - Trigger ordering (multiple triggers)
   - Cascade handling

4. **Testing** (10-20h)
   - Audit trail triggers
   - Constraint enforcement
   - Cascading updates

**Deliverable**: Triggers fire on data modifications

#### 10.2 Stored Procedure Language (120-180 hours)

**PL/ScratchBird - Procedural Language**

**Implementation Order**:
1. **Language Design** (20-30h)
   - Syntax specification
   - Variable scoping rules
   - Type system integration

2. **Parser** (35-50h)
   - Variable declarations (DECLARE)
   - Control flow (IF, LOOP, WHILE, FOR)
   - Exception handling (BEGIN/EXCEPTION/END)
   - RETURN statement

3. **AST + Semantic Analysis** (25-35h)
   - Procedural AST nodes
   - Variable resolution
   - Type checking

4. **Bytecode Generation** (20-30h)
   - Procedural opcodes
   - Control flow compilation
   - Variable access

5. **Executor** (30-45h)
   - Procedural statement execution
   - Stack frame management
   - Exception handling

6. **Function Catalog** (10-15h)
   - `pg_proc` catalog table
   - Function metadata storage
   - Overloading support

**Deliverable**: User-defined functions work

### Phase 2.2 Acceptance Test

```sql
-- Audit trigger
CREATE TRIGGER audit_users
AFTER UPDATE ON users
FOR EACH ROW
BEGIN
    INSERT INTO audit_log (table_name, operation, old_data, new_data, timestamp)
    VALUES ('users', 'UPDATE', OLD.*, NEW.*, NOW());
END;

-- Stored procedure
CREATE FUNCTION calculate_order_total(order_id INTEGER)
RETURNS DECIMAL(10, 2)
LANGUAGE plscratchbird
AS $$
DECLARE
    total DECIMAL(10, 2);
BEGIN
    SELECT SUM(quantity * price) INTO total
    FROM order_items
    WHERE order_items.order_id = calculate_order_total.order_id;

    RETURN COALESCE(total, 0.00);
END;
$$;
```

---

## Task 11: CTEs and Subqueries (HIGH)

**Why Third**: Complex analytical queries require these features.

**Total Effort**: 110-170 hours (3-4 weeks)

### Subtask Breakdown

#### 11.1 Common Table Expressions (50-80 hours)

**WITH clause support**

1. **Parser** (15-20h)
   - `WITH` clause syntax
   - Multiple CTEs
   - CTE column aliases

2. **Planner** (25-40h)
   - CTE materialization strategy
   - CTE inlining optimization
   - Scope resolution

3. **Executor** (10-20h)
   - CTE result caching
   - Multiple CTE execution

**Deliverable**: `WITH` queries work

#### 11.2 Subqueries (60-90 hours)

**Nested SELECT support**

1. **Scalar Subqueries** (15-20h)
   - `SELECT (SELECT ...) FROM`
   - Single value guarantee

2. **IN Subqueries** (15-25h)
   - `WHERE col IN (SELECT ...)`
   - Optimization (semi-join)

3. **EXISTS Subqueries** (15-25h)
   - `WHERE EXISTS (SELECT ...)`
   - Short-circuit evaluation

4. **Correlated Subqueries** (15-20h)
   - Outer reference resolution
   - Decorrelation optimization

**Deliverable**: Complex nested queries work

### Phase 2.3 Acceptance Test

```sql
-- CTE example
WITH top_customers AS (
    SELECT customer_id, SUM(total) as revenue
    FROM orders
    GROUP BY customer_id
    HAVING SUM(total) > 10000
)
SELECT c.name, tc.revenue
FROM top_customers tc
JOIN customers c ON c.id = tc.customer_id
ORDER BY tc.revenue DESC;

-- Subquery example
SELECT name, salary
FROM employees
WHERE salary > (SELECT AVG(salary) FROM employees)
  AND department_id IN (SELECT id FROM departments WHERE budget > 1000000);
```

---

## Task 12: Array Functions (MEDIUM)

**Why Fourth**: PostgreSQL compatibility for array operations.

**Total Effort**: 40-60 hours (1 week)

**Implementation**:
- `ARRAY_AGG()` aggregate function
- `UNNEST()` table function
- `ARRAY_TO_STRING()`, `STRING_TO_ARRAY()`
- `ARRAY_APPEND()`, `ARRAY_PREPEND()`, `ARRAY_CAT()`
- Array operators (`&&`, `@>`, `<@`)

**Deliverable**: PostgreSQL-style array operations work

---

## Task 13: Full-Text Search Functions (MEDIUM)

**Why Fifth**: Text pattern matching and manipulation.

**Total Effort**: 50-80 hours (1-1.5 weeks)

**Implementation**:
- `ILIKE` (case-insensitive LIKE)
- `REGEXP_MATCHES()`, `REGEXP_REPLACE()`
- String tokenization functions
- (Full `tsvector`/`tsquery` deferred to Phase 3)

**Deliverable**: Basic text search works

---

## Recommended Implementation Strategy

### Option 1: Sequential (Full Features)

**Approach**: Complete each task fully before moving to next

**Timeline**:
- Month 1-4: Task 9 (Spatial)
- Month 5-6: Task 10 (Triggers/Procedures)
- Month 6-7: Task 11 (CTEs/Subqueries)
- Month 7.5: Task 12 (Arrays)
- Month 7.5: Task 13 (Text Search)

**Pros**: Each feature fully complete
**Cons**: Long time before any Phase 2 features available

### Option 2: Iterative (Incremental Value)

**Approach**: Implement minimum viable version of each, then enhance

**Phase 2a (Month 1-2)**: Core Features
- Task 9.1: Core spatial types (POINT, LINESTRING, POLYGON)
- Task 11.1: Basic CTEs (no recursion)
- Task 12: Array functions (all)

**Phase 2b (Month 3-4)**: Advanced Spatial
- Task 9.2: R-tree indexes
- Task 9.3: Spatial functions

**Phase 2c (Month 5-6)**: Procedural Code
- Task 10.1: Triggers
- Task 10.2: Stored procedures

**Phase 2d (Month 7)**: Polish
- Task 9.4: Multi-geometries
- Task 9.5: CRS/SRID support
- Task 11.2: Subqueries
- Task 13: Text search

**Pros**: Faster time-to-value, early feedback
**Cons**: Features incomplete initially

### Option 3: Parallel (Team Approach)

**Approach**: Split work across multiple developers

**Developer 1**: Spatial (Task 9)
**Developer 2**: Procedural (Task 10)
**Developer 3**: Query (Task 11, 12, 13)

**Timeline**: 2.5-3.75 months with 2-3 developers

**Pros**: Fastest completion
**Cons**: Requires team coordination

---

## Recommendation

**Start with Task 9.1 (Core Spatial Types)** for these reasons:

1. **Highest Market Value**: GIS/mapping is the largest market segment
2. **Clear Scope**: Well-defined types (POINT, LINESTRING, POLYGON)
3. **Foundation**: Required for all other spatial work
4. **Quick Win**: Can demonstrate spatial storage in 2-3 weeks

**Estimated Time for Task 9.1**: 80-120 hours (2-3 weeks)

---

## Next Steps

1. **Review this plan** and decide on strategy (Sequential / Iterative / Parallel)
2. **Read existing spatial code** to understand current foundation
3. **Start Task 9.1.1 (POINT type)** as first implementation
4. **Create tests** as features are built
5. **Update roadmap** after each subtask completion

---

## Success Metrics

**Phase 2 Complete When**:
- ✅ GIS application can store and query spatial data
- ✅ Triggers enforce business rules
- ✅ Stored procedures implement business logic
- ✅ Complex queries with CTEs work
- ✅ Array operations work (PostgreSQL compat)
- ✅ Text pattern matching works

**Target**: Enable ScratchBird to compete with PostgreSQL/MySQL/SQL Server for 80% of real-world applications

---

**Ready to start Phase 2?** Let me know which strategy you prefer, and I'll begin implementation!

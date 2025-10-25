# ScratchBird 1:1 Feature Parity Implementation Roadmap
**Created**: October 25, 2025
**Purpose**: Implementation plan to achieve 1:1 feature parity with Firebird, MySQL, PostgreSQL, SQL Server
**Total Estimated Effort**: 2,020-3,145 hours (~12-19 months with 1 dev, ~6-10 months with 2 devs)

---

## Overview

This roadmap outlines the work required to achieve **1:1 feature parity** with all 4 target databases. The work is organized into 3 phases based on market priority.

**Key Principle**: If a feature exists in ANY of the 4 target databases, it must be implemented in ScratchBird for market competitiveness.

---

## Phase Summary

| Phase | Priority | Estimated Hours | Timeline (1 dev) | Timeline (2 devs) | Focus |
|-------|----------|----------------|------------------|-------------------|-------|
| **Phase 1** | MUST HAVE | 400-600 | 2.5-4 months | 1.25-2 months | Critical blockers preventing ANY market use |
| **Phase 2** | SHOULD HAVE | 800-1,200 | 5-7.5 months | 2.5-3.75 months | Features needed to compete with existing DBs |
| **Phase 3** | NICE TO HAVE | 800-1,300 | 5-8 months | 2.5-4 months | Complete feature parity for all use cases |
| **TOTAL** | - | **2,000-3,100** | **12.5-19.5 months** | **6.25-9.75 months** | Full 1:1 parity |

---

## Phase 1: Critical Blockers (MUST HAVE)

**Timeline**: 2.5-4 months (1 dev) or 1.25-2 months (2 devs)
**Total Effort**: 400-600 hours
**Goal**: Enable basic SQL applications to use ScratchBird

### Blockers Addressed
Without these features, ScratchBird **cannot be used** for even simple applications:
- ❌ Cannot modify or delete data (no UPDATE/DELETE)
- ❌ Cannot perform multi-table queries (no JOINs)
- ❌ Cannot aggregate data (no GROUP BY)
- ❌ Cannot perform analytics (no window functions)
- ❌ Cannot use modern JSON data (no JSON functions)
- ❌ All queries execute without optimization (no query optimizer)

### Tasks (Priority Order)

#### 1. Query Optimizer Foundation (100-160 hours) - CRITICAL ⏳ IN PROGRESS
**Why First**: Every query needs optimization. Without this, all queries are slow.
**Status**: Started October 25, 2025

- [ ] **1.1 Statistics Collection** (30-40 hours) ⏳ IN PROGRESS
  - [x] Create statistics catalog tables (pg_statistic equivalent) ✅ Done Oct 25
  - [ ] Implement ANALYZE command parser support
  - [ ] Implement table sampling (Vitter's Algorithm S)
  - [ ] Implement column statistics collection (null fraction, n_distinct, avg_width)
  - [ ] Implement histogram generation (equal-height, equal-width)
  - [ ] Implement MCV (Most Common Values) collection
  - [ ] Store statistics in catalog
  - **Deliverable**: `ANALYZE table_name` command works
  - **Progress**: Catalog structures created, stubs implemented, API designed

- [ ] **1.2 Cost Model** (25-35 hours)
  - [ ] Implement cost configuration structure (seq_page_cost, random_page_cost, cpu_tuple_cost)
  - [ ] Implement sequential scan cost estimation
  - [ ] Implement index scan cost estimation
  - [ ] Implement cache effect modeling
  - [ ] Add cost estimation to plan nodes
  - **Deliverable**: Cost estimates for seq scan and index scan

- [ ] **1.3 Basic Query Planner** (30-50 hours)
  - [ ] Create PlanNode structures (SeqScan, IndexScan, NestLoop, etc.)
  - [ ] Implement path generation for single-table queries
  - [ ] Implement index selection (choose best index)
  - [ ] Implement cheapest path selection
  - [ ] Integrate planner between parser and bytecode generator
  - **Deliverable**: Single-table SELECT chooses best access path

- [ ] **1.4 Selectivity Estimation** (15-25 hours)
  - [ ] Implement equality selectivity (= operator)
  - [ ] Implement range selectivity (>, <, >=, <=, BETWEEN)
  - [ ] Implement LIKE selectivity
  - [ ] Implement IN selectivity
  - [ ] Implement AND/OR/NOT selectivity
  - **Deliverable**: WHERE clause selectivity estimates

- [ ] **1.5 EXPLAIN Command** (10-15 hours)
  - [ ] Add EXPLAIN parser support
  - [ ] Implement EXPLAIN text output format
  - [ ] Show plan tree with costs and row estimates
  - **Deliverable**: `EXPLAIN SELECT ...` shows query plan

**Phase 1.1 Completion Criteria**: Query optimizer produces plans with cost estimates

---

#### 2. Core CRUD Operations (35-55 hours) - CRITICAL
**Why Second**: Cannot modify or delete data without these.

- [ ] **2.1 UPDATE Statement** (20-30 hours)
  - [ ] Add UPDATE parser support (UPDATE table SET col=val WHERE condition)
  - [ ] Implement UPDATE AST node
  - [ ] Implement UPDATE semantic analysis
  - [ ] Generate SBLR bytecode for UPDATE
  - [ ] Implement UPDATE executor logic (with MGA versioning)
  - [ ] Handle indexed column updates (update indexes)
  - [ ] Add transaction isolation for UPDATE
  - **Deliverable**: `UPDATE table SET col=val WHERE condition` works

- [ ] **2.2 DELETE Statement** (15-25 hours)
  - [ ] Add DELETE parser support (DELETE FROM table WHERE condition)
  - [ ] Implement DELETE AST node
  - [ ] Implement DELETE semantic analysis
  - [ ] Generate SBLR bytecode for DELETE
  - [ ] Implement DELETE executor logic (mark deleted in MGA)
  - [ ] Handle index cleanup for deleted rows
  - [ ] Add transaction isolation for DELETE
  - **Deliverable**: `DELETE FROM table WHERE condition` works

**Phase 1.2 Completion Criteria**: Full CRUD operations (Create, Read, Update, Delete)

---

#### 3. JOIN Support (40-60 hours) - CRITICAL
**Why Third**: Most queries need multi-table joins.

- [ ] **3.1 Parser Support** (15-25 hours)
  - [ ] Add JOIN clause parsing (INNER JOIN, LEFT JOIN, RIGHT JOIN, FULL OUTER JOIN)
  - [ ] Add CROSS JOIN parsing
  - [ ] Add JOIN ... ON condition parsing
  - [ ] Add JOIN ... USING (columns) parsing
  - [ ] Add NATURAL JOIN parsing (optional)
  - [ ] Implement JOIN AST nodes
  - **Deliverable**: Parser recognizes all JOIN syntax

- [ ] **3.2 Query Planner for Joins** (15-25 hours)
  - [ ] Implement join path generation (NestLoop, HashJoin, MergeJoin)
  - [ ] Implement nested loop join cost estimation
  - [ ] Implement hash join cost estimation
  - [ ] Implement join ordering (dynamic programming for small joins)
  - [ ] Select cheapest join method
  - **Deliverable**: Optimizer chooses best join strategy

- [ ] **3.3 JOIN Execution** (10-15 hours)
  - [ ] Generate SBLR bytecode for nested loop join
  - [ ] Generate SBLR bytecode for hash join (if implemented)
  - [ ] Implement JOIN executor in SBLR
  - [ ] Handle NULL handling in outer joins
  - **Deliverable**: Multi-table SELECT with JOIN works

**Phase 1.3 Completion Criteria**: Multi-table queries with JOINs work correctly

---

#### 4. Aggregation and Grouping (60-90 hours) - HIGH
**Why Fourth**: Most reporting queries need GROUP BY and aggregation.

- [ ] **4.1 GROUP BY Parser** (15-20 hours)
  - [ ] Add GROUP BY clause parsing
  - [ ] Add HAVING clause parsing
  - [ ] Validate GROUP BY columns vs SELECT columns
  - [ ] Implement GROUP BY AST nodes
  - **Deliverable**: Parser recognizes GROUP BY and HAVING

- [ ] **4.2 Aggregation Planning** (20-30 hours)
  - [ ] Implement aggregate plan node
  - [ ] Detect aggregate functions in SELECT (COUNT, SUM, AVG, MIN, MAX)
  - [ ] Generate grouping keys
  - [ ] Plan sort for GROUP BY (if needed)
  - [ ] Estimate aggregate cost
  - **Deliverable**: Planner generates aggregate plan

- [ ] **4.3 Aggregation Execution** (25-40 hours)
  - [ ] Generate SBLR bytecode for GROUP BY
  - [ ] Implement hash-based grouping in SBLR executor
  - [ ] Implement aggregate functions (COUNT, SUM, AVG, MIN, MAX)
  - [ ] Implement HAVING filter
  - [ ] Handle NULL values in aggregation
  - **Deliverable**: `SELECT col, COUNT(*) FROM table GROUP BY col` works

**Phase 1.4 Completion Criteria**: Aggregation queries with GROUP BY work

---

#### 5. Sorting and Limiting (30-45 hours) - HIGH
**Why Fifth**: Most queries need sorted or paginated results.

- [ ] **5.1 ORDER BY Support** (20-30 hours)
  - [ ] Add ORDER BY clause parsing
  - [ ] Add ASC/DESC parsing
  - [ ] Add NULLS FIRST/NULLS LAST parsing
  - [ ] Implement ORDER BY AST nodes
  - [ ] Generate sort plan node
  - [ ] Estimate sort cost
  - [ ] Generate SBLR bytecode for ORDER BY
  - [ ] Implement sort executor (quicksort or merge sort)
  - **Deliverable**: `SELECT * FROM table ORDER BY col ASC` works

- [ ] **5.2 LIMIT/OFFSET Support** (10-15 hours)
  - [ ] Add LIMIT clause parsing
  - [ ] Add OFFSET clause parsing
  - [ ] Implement LIMIT/OFFSET AST nodes
  - [ ] Generate limit plan node
  - [ ] Generate SBLR bytecode for LIMIT/OFFSET
  - [ ] Implement limit executor (early termination)
  - **Deliverable**: `SELECT * FROM table LIMIT 10 OFFSET 20` works

**Phase 1.5 Completion Criteria**: Sorted and paginated queries work

---

#### 6. Window Functions (60-90 hours) - CRITICAL FOR ANALYTICS
**Why Sixth**: Analytics applications cannot function without window functions.

- [ ] **6.1 Window Function Parser** (15-25 hours)
  - [ ] Add OVER clause parsing
  - [ ] Add PARTITION BY parsing
  - [ ] Add ORDER BY in window parsing
  - [ ] Add frame clause parsing (ROWS BETWEEN, RANGE BETWEEN)
  - [ ] Implement window function AST nodes
  - **Deliverable**: Parser recognizes window function syntax

- [ ] **6.2 Window Function Planning** (20-30 hours)
  - [ ] Implement window aggregate plan node
  - [ ] Detect window functions in SELECT
  - [ ] Plan partition sorting
  - [ ] Plan frame calculation
  - [ ] Estimate window function cost
  - **Deliverable**: Planner generates window function plan

- [ ] **6.3 Window Function Execution** (25-35 hours)
  - [ ] Implement ROW_NUMBER()
  - [ ] Implement RANK() and DENSE_RANK()
  - [ ] Implement LAG() and LEAD()
  - [ ] Implement FIRST_VALUE() and LAST_VALUE()
  - [ ] Implement NTH_VALUE()
  - [ ] Implement window frame handling
  - [ ] Generate SBLR bytecode for window functions
  - **Deliverable**: `SELECT ROW_NUMBER() OVER (PARTITION BY col ORDER BY col2)` works

**Phase 1.6 Completion Criteria**: Basic analytics queries with window functions work

---

#### 7. JSON Functions (80-120 hours) - CRITICAL FOR MODERN APPS
**Why Seventh**: Modern web applications heavily use JSON.

- [ ] **7.1 JSON Extraction Functions** (30-45 hours)
  - [ ] Implement JSON_EXTRACT (path-based extraction)
  - [ ] Implement jsonb_extract_path (PostgreSQL style)
  - [ ] Implement -> and ->> operators
  - [ ] Implement #> and #>> operators (path arrays)
  - [ ] Add SBLR opcodes for JSON extraction
  - **Deliverable**: `SELECT data->>'field' FROM table` works

- [ ] **7.2 JSON Construction Functions** (30-45 hours)
  - [ ] Implement JSON_OBJECT (build JSON from key-value pairs)
  - [ ] Implement JSON_ARRAY (build JSON array)
  - [ ] Implement jsonb_build_object (PostgreSQL style)
  - [ ] Implement jsonb_build_array (PostgreSQL style)
  - [ ] Add SBLR opcodes for JSON construction
  - **Deliverable**: `SELECT JSON_OBJECT('key', value)` works

- [ ] **7.3 JSON Modification Functions** (20-30 hours)
  - [ ] Implement JSON_SET (set value at path)
  - [ ] Implement JSON_INSERT (insert value)
  - [ ] Implement JSON_REMOVE (remove value)
  - [ ] Implement jsonb_set (PostgreSQL style)
  - [ ] Add SBLR opcodes for JSON modification
  - **Deliverable**: `SELECT JSON_SET(data, '$.field', 'value')` works

**Phase 1.7 Completion Criteria**: JSON data manipulation works

---

#### 8. Conditional Functions (20-30 hours) - CRITICAL
**Why Eighth**: Basic SQL queries need conditional expressions.

- [ ] **8.1 COALESCE and NULLIF** (10-15 hours)
  - [ ] Add COALESCE parser support
  - [ ] Add NULLIF parser support
  - [ ] Implement COALESCE/NULLIF in SBLR executor
  - [ ] Add SBLR opcodes
  - **Deliverable**: `SELECT COALESCE(col, 'default') FROM table` works

- [ ] **8.2 CASE Expression** (10-15 hours)
  - [ ] Add CASE WHEN parser support (simple and searched)
  - [ ] Implement CASE AST node
  - [ ] Generate SBLR bytecode for CASE
  - [ ] Implement CASE executor
  - **Deliverable**: `SELECT CASE WHEN col > 10 THEN 'high' ELSE 'low' END` works

**Phase 1.8 Completion Criteria**: Conditional expressions work in queries

---

### Phase 1 Completion Criteria

**Definition of Done**:
- ✅ Query optimizer selects optimal plans for single and multi-table queries
- ✅ Full CRUD operations work (INSERT, SELECT, UPDATE, DELETE)
- ✅ Multi-table queries with JOINs work
- ✅ Aggregation with GROUP BY and HAVING works
- ✅ Sorting with ORDER BY and pagination with LIMIT/OFFSET works
- ✅ Analytics queries with window functions work
- ✅ JSON data can be queried and manipulated
- ✅ Conditional expressions (COALESCE, NULLIF, CASE) work
- ✅ EXPLAIN shows query plans with costs

**Acceptance Test**:
```sql
-- This query should work end-to-end:
EXPLAIN
SELECT
    u.name,
    COUNT(o.id) as order_count,
    SUM(o.total) as total_spent,
    COALESCE(u.email, 'no-email') as email,
    ROW_NUMBER() OVER (ORDER BY SUM(o.total) DESC) as rank,
    o.metadata->>'preferences' as prefs
FROM users u
LEFT JOIN orders o ON u.id = o.user_id
WHERE u.created_at > '2024-01-01'
GROUP BY u.id, u.name, u.email, o.metadata
HAVING COUNT(o.id) > 5
ORDER BY total_spent DESC
LIMIT 100;
```

**Milestone**: ScratchBird can handle **basic OLTP and OLAP workloads**

---

## Phase 2: Competitive Parity (SHOULD HAVE)

**Timeline**: 5-7.5 months (1 dev) or 2.5-3.75 months (2 devs)
**Total Effort**: 800-1,200 hours
**Goal**: Compete with existing databases for real-world applications

### Features Addressed
These features are needed to compete with PostgreSQL, MySQL, SQL Server:
- GIS/mapping applications (spatial support)
- Business logic in database (triggers, procedures)
- Complex analytical queries (CTEs, subqueries)
- Array and text search operations

### Tasks (Priority Order)

#### 9. Spatial Types and Functions (420-630 hours) - CRITICAL FOR GIS
**Why First in Phase 2**: Largest single market segment (GIS/mapping).

**See**: `/docs/planning/SPATIAL_IMPLEMENTATION_PLAN.md` for detailed breakdown

- [ ] **9.1 Core Spatial Types** (80-120 hours)
  - [ ] Implement POINT type
  - [ ] Implement LINESTRING type
  - [ ] Implement POLYGON type
  - [ ] Implement WKT (Well-Known Text) input/output
  - [ ] Implement WKB (Well-Known Binary) storage format
  - [ ] Add spatial type serialization

- [ ] **9.2 Spatial Indexes** (120-180 hours)
  - [ ] Implement R-tree index structure
  - [ ] Implement R-tree insertion
  - [ ] Implement R-tree search (bounding box queries)
  - [ ] Implement R-tree deletion
  - [ ] Integrate R-tree with query planner

- [ ] **9.3 Spatial Functions** (100-150 hours)
  - [ ] Implement ST_Distance
  - [ ] Implement ST_Contains
  - [ ] Implement ST_Intersects
  - [ ] Implement ST_Within
  - [ ] Implement ST_Buffer
  - [ ] Implement ST_AsText, ST_AsBinary
  - [ ] Implement ST_Area, ST_Length

- [ ] **9.4 Additional Spatial Types** (60-90 hours)
  - [ ] Implement MULTIPOINT
  - [ ] Implement MULTILINESTRING
  - [ ] Implement MULTIPOLYGON
  - [ ] Implement GEOMETRYCOLLECTION

- [ ] **9.5 Coordinate Reference Systems** (60-90 hours)
  - [ ] Implement SRID (Spatial Reference Identifier) support
  - [ ] Implement coordinate transformations (PROJ integration)
  - [ ] Implement geographic vs. projected coordinate systems

**Phase 2.1 Completion Criteria**: GIS applications can use ScratchBird

---

#### 10. Triggers and Stored Procedures (200-300 hours) - HIGH
**Why Second in Phase 2**: Business logic enforcement in database.

**See**: `/docs/planning/PROCEDURAL_CODE_PLAN.md` for detailed breakdown

- [ ] **10.1 Trigger Support** (80-120 hours)
  - [ ] Add CREATE TRIGGER parser support
  - [ ] Implement trigger catalog (pg_trigger equivalent)
  - [ ] Implement BEFORE/AFTER INSERT/UPDATE/DELETE triggers
  - [ ] Implement FOR EACH ROW triggers
  - [ ] Implement trigger execution in executor
  - [ ] Add OLD and NEW row references
  - [ ] Implement trigger enable/disable

- [ ] **10.2 Stored Procedure Language** (120-180 hours)
  - [ ] Design procedural language (PL/ScratchBird)
  - [ ] Implement variable declarations
  - [ ] Implement control flow (IF, LOOP, WHILE, FOR)
  - [ ] Implement exception handling (BEGIN/EXCEPTION/END)
  - [ ] Implement RETURN statement
  - [ ] Add CREATE FUNCTION parser support
  - [ ] Implement function catalog (pg_proc equivalent)
  - [ ] Implement function execution

**Phase 2.2 Completion Criteria**: Business logic can be implemented in database

---

#### 11. CTEs and Subqueries (110-170 hours) - HIGH
**Why Third in Phase 2**: Complex queries require these.

- [ ] **11.1 Common Table Expressions (CTEs)** (50-80 hours)
  - [ ] Add WITH clause parser support
  - [ ] Implement CTE AST nodes
  - [ ] Implement CTE planner integration
  - [ ] Implement CTE materialization
  - [ ] Implement CTE inlining (optimization)
  - [ ] Add RECURSIVE CTE support
  - [ ] Implement recursion termination checks

- [ ] **11.2 Subqueries** (60-90 hours)
  - [ ] Implement scalar subqueries (SELECT (SELECT ...))
  - [ ] Implement subqueries in WHERE (col IN (SELECT ...))
  - [ ] Implement EXISTS subqueries
  - [ ] Implement correlated subqueries
  - [ ] Implement subquery decorrelation (optimization)
  - [ ] Implement ANY/ALL operators

**Phase 2.3 Completion Criteria**: Complex analytical queries work

---

#### 12. Array Functions (40-60 hours) - MEDIUM
**Why Fourth in Phase 2**: PostgreSQL array operations.

- [ ] **12.1 Array Functions** (40-60 hours)
  - [ ] Implement ARRAY_AGG aggregate function
  - [ ] Implement UNNEST table function
  - [ ] Implement ARRAY_TO_STRING
  - [ ] Implement STRING_TO_ARRAY
  - [ ] Implement ARRAY_APPEND, ARRAY_PREPEND
  - [ ] Implement ARRAY_CAT (concatenation)
  - [ ] Implement ARRAY_POSITION
  - [ ] Implement array operators (&&, @>, <@)

**Phase 2.4 Completion Criteria**: PostgreSQL-style array operations work

---

#### 13. Full-Text Search Functions (50-80 hours) - MEDIUM
**Why Fifth in Phase 2**: Text search requires functions (types in Phase 3).

- [ ] **13.1 Text Search Functions** (50-80 hours)
  - [ ] Implement basic LIKE optimization
  - [ ] Implement ILIKE (case-insensitive LIKE)
  - [ ] Implement REGEXP_MATCHES
  - [ ] Implement REGEXP_REPLACE
  - [ ] Implement string tokenization functions
  - [ ] (Note: Full tsvector/tsquery in Phase 3)

**Phase 2.5 Completion Criteria**: Basic text search works

---

### Phase 2 Completion Criteria

**Definition of Done**:
- ✅ GIS applications can store and query spatial data
- ✅ Triggers enforce business rules on INSERT/UPDATE/DELETE
- ✅ Stored procedures implement complex business logic
- ✅ Complex queries with CTEs and subqueries work
- ✅ Array operations work (PostgreSQL compatibility)
- ✅ Text pattern matching and manipulation works

**Acceptance Test**:
```sql
-- Spatial query
SELECT name, ST_Distance(location, ST_Point(0, 0)) as distance
FROM locations
WHERE ST_Contains(ST_Buffer(ST_Point(0, 0), 10), location)
ORDER BY distance LIMIT 10;

-- Trigger example
CREATE TRIGGER audit_log
AFTER UPDATE ON users
FOR EACH ROW
EXECUTE PROCEDURE log_user_change();

-- CTE example
WITH regional_sales AS (
    SELECT region, SUM(amount) as total
    FROM orders
    GROUP BY region
)
SELECT * FROM regional_sales WHERE total > 1000;
```

**Milestone**: ScratchBird is **competitive with PostgreSQL, MySQL, SQL Server** for most applications

---

## Phase 3: Full Parity (NICE TO HAVE)

**Timeline**: 5-8 months (1 dev) or 2.5-4 months (2 devs)
**Total Effort**: 800-1,300 hours
**Goal**: Complete 1:1 feature parity for all use cases

### Features Addressed
These features complete the feature set for full database replacement:
- PostgreSQL full-text search (tsvector/tsquery types)
- Temporal database features (range types)
- Network administration (network types)
- Advanced indexing (expression indexes, filtered indexes)
- Extended function library
- Firebird 4.0+ compatibility (DECFLOAT)

### Tasks (Priority Order)

#### 14. Text Search Types (130-200 hours)
- [ ] Implement tsvector type
- [ ] Implement tsquery type
- [ ] Implement @@ match operator
- [ ] Implement to_tsvector function
- [ ] Implement to_tsquery function
- [ ] Implement ts_rank function
- [ ] Implement text search configurations
- [ ] Integrate with GIN indexes

**Phase 3.1 Completion Criteria**: PostgreSQL-style full-text search works

---

#### 15. Range Types (100-150 hours)
- [ ] Implement generic range type infrastructure
- [ ] Implement int4range, int8range, numrange
- [ ] Implement tsrange, tstzrange, daterange
- [ ] Implement range operators (&&, @>, <@, <<, >>)
- [ ] Implement range functions (lower, upper, isempty)
- [ ] Integrate with GiST indexes for ranges

**Phase 3.2 Completion Criteria**: Temporal databases work

---

#### 16. Network Types (40-60 hours)
- [ ] Implement inet type (IPv4/IPv6)
- [ ] Implement cidr type
- [ ] Implement macaddr/macaddr8 types
- [ ] Implement network operators (<<, >>, &&, ~, &, |)
- [ ] Implement network functions (inet_same_family, inet_merge)

**Phase 3.3 Completion Criteria**: Network administration applications work

---

#### 17. Expression and Filtered Indexes (120-180 hours)
- [ ] Implement expression indexes (CREATE INDEX ON table((expression)))
- [ ] Implement partial indexes (CREATE INDEX WHERE condition)
- [ ] Integrate with query planner
- [ ] Implement expression matching
- [ ] Implement predicate matching

**Phase 3.4 Completion Criteria**: Advanced indexing strategies work

---

#### 18. Extended Function Library (300-450 hours)
- [ ] String functions (CONCAT, REPLACE, POSITION, REGEXP_*, etc.) - 40-60h
- [ ] Numeric functions (ROUND, CEIL, FLOOR, TRUNC, ABS, POWER, SQRT, etc.) - 30-50h
- [ ] Date/time functions (EXTRACT, DATE_PART, DATE_TRUNC, MAKE_DATE, etc.) - 40-60h
- [ ] System information functions (CURRENT_USER, VERSION, pg_database_size, etc.) - 20-30h
- [ ] Cryptographic functions (MD5, SHA256, etc.) - 20-30h
- [ ] Additional aggregate functions (STDDEV, VARIANCE, CORR, etc.) - 30-40h
- [ ] Additional window functions (PERCENT_RANK, CUME_DIST, NTILE, etc.) - 20-30h
- [ ] Conversion functions (TO_CHAR, TO_NUMBER, TO_DATE, etc.) - 30-50h
- [ ] String formatting (FORMAT, LPAD, RPAD, etc.) - 20-30h
- [ ] Miscellaneous (GREATEST, LEAST, generate_series, etc.) - 50-80h

**Phase 3.5 Completion Criteria**: Comprehensive function library matches target DBs

---

#### 19. Remaining DDL and DCL (80-140 hours)
- [ ] Sequences (CREATE SEQUENCE, NEXTVAL, CURRVAL) - 30-50h
- [ ] Views (CREATE VIEW, DROP VIEW, ALTER VIEW) - 40-60h
- [ ] Permissions (GRANT, REVOKE) - 40-60h
- [ ] ALTER TABLE enhancements (ADD/DROP COLUMN, ADD/DROP CONSTRAINT) - Already in Phase 1?
- [ ] DROP TABLE with CASCADE/RESTRICT - 10-15h

**Phase 3.6 Completion Criteria**: Complete DDL/DCL support

---

#### 20. Specialized Type Compatibility (90-140 hours)
- [ ] Bit string types (BIT(n), BIT VARYING(n)) - 30-50h
- [ ] DECFLOAT for Firebird 4.0+ (DECFLOAT(16), DECFLOAT(34)) - 40-60h
- [ ] ENUM/SET native syntax for MySQL - 20-30h

**Phase 3.7 Completion Criteria**: Full type compatibility with all 4 databases

---

### Phase 3 Completion Criteria

**Definition of Done**:
- ✅ Full-text search applications work (PostgreSQL tsvector/tsquery)
- ✅ Temporal databases work (range types)
- ✅ Network administration tools work (inet, cidr, macaddr)
- ✅ Advanced indexing strategies work (expression, filtered indexes)
- ✅ Comprehensive function library (200+ functions)
- ✅ Complete DDL/DCL support (sequences, views, permissions)
- ✅ Full type compatibility (bit strings, DECFLOAT, ENUM/SET)

**Milestone**: ScratchBird achieves **100% feature parity** with all 4 target databases

---

## Implementation Strategy

### Recommended Approach

**Option 1: Sequential Implementation (1 developer)**
- Implement Phase 1 → Phase 2 → Phase 3
- Timeline: 12.5-19.5 months
- Advantage: Focused, no coordination overhead
- Disadvantage: Longest time to market

**Option 2: Parallel Implementation (2 developers)**
- Dev 1: Query Optimizer + CRUD + JOINs (Phase 1 tasks 1-3)
- Dev 2: Aggregation + Sorting + Window Functions (Phase 1 tasks 4-6)
- Then merge and continue
- Timeline: 6.25-9.75 months
- Advantage: 2x faster
- Disadvantage: Requires coordination

**Option 3: Phased Release (2+ developers)**
- Release Alpha after Phase 1 (2.5-4 months)
- Release Beta after Phase 2 (7.5-11.5 months total)
- Release Production after Phase 3 (12.5-19.5 months total)
- Advantage: Earlier market feedback
- Disadvantage: Managing multiple releases

### Prioritization Within Phases

Each phase is ordered by **dependency** and **market impact**:
1. **Dependency**: Tasks that other tasks depend on come first (e.g., query optimizer before complex queries)
2. **Market Impact**: Tasks that unblock the largest user segments come first (e.g., JOINs before CTEs)

### Testing Strategy

**Per-Task Testing**:
- Unit tests for each function/operator
- Integration tests for each SQL feature
- Regression tests to prevent breakage

**Per-Phase Testing**:
- End-to-end application tests
- Performance benchmarks
- Compatibility tests against target databases

**Continuous Testing**:
- All Phase 1 tests must pass before starting Phase 2
- All Phase 1+2 tests must pass before starting Phase 3

---

## Success Metrics

### Phase 1 Success Metrics
- [ ] TPC-H benchmark queries 1-5 execute successfully
- [ ] Basic OLTP application (e-commerce) works end-to-end
- [ ] Basic OLAP queries (analytics dashboard) work
- [ ] JSON-based API backend works

### Phase 2 Success Metrics
- [ ] GIS application (mapping service) works
- [ ] Triggers enforce referential integrity
- [ ] Complex analytical queries (data warehouse) work
- [ ] PostgreSQL pg_bench runs successfully

### Phase 3 Success Metrics
- [ ] Full-text search application works
- [ ] Temporal database (scheduling system) works
- [ ] Network monitoring tool works
- [ ] All TPC-H queries execute successfully
- [ ] Migration from PostgreSQL/MySQL/Firebird/SQL Server works

---

## Risk Management

### High-Risk Areas

1. **Query Optimizer Complexity**
   - Risk: Underestimating optimizer complexity
   - Mitigation: Start simple (no join reordering), add features incrementally
   - Fallback: Use rule-based optimization initially

2. **Spatial Index Performance**
   - Risk: R-tree implementation may not perform well
   - Mitigation: Benchmark against PostGIS early
   - Fallback: Use existing library (libspatialindex)

3. **Procedural Language Design**
   - Risk: Language design may be incompatible with targets
   - Mitigation: Study PL/pgSQL, T-SQL, Firebird PSQL early
   - Fallback: Support only basic triggers initially

4. **Testing Coverage**
   - Risk: Insufficient testing leads to bugs in production
   - Mitigation: Write tests concurrently with code
   - Fallback: Freeze features for stabilization period

---

## Next Steps

### Immediate Actions (This Week)

1. **Confirm Scope with Project Owner**:
   - [ ] Review this roadmap
   - [ ] Decide: Full Phase 1+2+3 or subset?
   - [ ] Decide: Target timeline (aggressive vs. realistic)?
   - [ ] Decide: Team size (1 dev, 2 devs, or more)?

2. **Set Up Project Tracking**:
   - [ ] Create GitHub/GitLab issues for Phase 1 tasks
   - [ ] Set up project board (TODO, In Progress, Done)
   - [ ] Define sprint duration (1 week, 2 weeks?)

3. **Start Phase 1, Task 1: Query Optimizer Foundation**:
   - [ ] Read `/docs/specifications/QUERY_OPTIMIZER_SPEC.md`
   - [ ] Create statistics catalog schema
   - [ ] Implement ANALYZE command parser stub

---

## Appendix: Detailed Task Breakdowns

For detailed task breakdowns, see:
- `/docs/planning/PHASE1_DETAILED_TASKS.md` - Phase 1 task breakdown
- `/docs/planning/PHASE2_DETAILED_TASKS.md` - Phase 2 task breakdown
- `/docs/planning/PHASE3_DETAILED_TASKS.md` - Phase 3 task breakdown
- `/docs/planning/SPATIAL_IMPLEMENTATION_PLAN.md` - Spatial features detailed plan
- `/docs/planning/PROCEDURAL_CODE_PLAN.md` - Triggers/procedures detailed plan

---

**Document Status**: Implementation Started
**Next Review**: After Phase 1, Task 1 completion
**Owner**: Project Lead
**Last Updated**: October 25, 2025

---

## Implementation Log

### October 25, 2025

**Morning Session - Planning & Roadmap**
- **Started**: Phase 1, Task 1: Query Optimizer Foundation
- **Started**: Task 1.1: Statistics Collection
- **Current Focus**: Creating statistics catalog tables

**Afternoon Session - Statistics Infrastructure**
- **Completed**: Created statistics catalog data structures
  - `include/scratchbird/optimizer/statistics.h` - ColumnStatistics, TableStatistics, MCVEntry, HistogramBucket structures
  - `include/scratchbird/optimizer/statistics_manager.h` - StatisticsManager class API design
  - `src/optimizer/statistics_manager.cpp` - Implementation stubs for all Phase 1.1 methods
  - `src/CMakeLists.txt` - Added scratchbird_optimizer library
- **Commit**: 4a7d7a1 - "Phase 1, Task 1.1: Create statistics catalog infrastructure (foundation)"
- **Progress**: Task 1.1.1 complete (catalog structures created)
- **Blocker Found**: Existing tid_resolver build errors preventing compilation
- **Next**: Fix tid_resolver errors, then implement ANALYZE parser support

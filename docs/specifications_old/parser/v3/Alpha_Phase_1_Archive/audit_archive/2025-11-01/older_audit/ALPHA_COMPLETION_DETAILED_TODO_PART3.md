# SCRATCHBIRD ALPHA COMPLETION - DETAILED TODO (PART 3)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Part**: 3 of 3 (Phases 10-12)
**Date**: October 16, 2025
**Status**: Planning Phase
**Prerequisites**: ALPHA_COMPLETION_DETAILED_TODO_PART2.md (Phases 7-9)

---

## TABLE OF CONTENTS

- [Phase 10: System Tables & Metadata](#phase-10-system-tables--metadata-125-days-25-weeks)
- [Phase 11: Query Optimizer](#phase-11-query-optimizer-175-days-35-weeks)
- [Phase 12: Embedded Engine Integration](#phase-12-embedded-engine-integration-125-days-25-weeks)
- [Final Alpha Checklist](#final-alpha-checklist)
- [Post-Alpha: Beta Roadmap Preview](#post-alpha-beta-roadmap-preview)

---

## PHASE 10: System Tables & Metadata (12.5 days, 2.5 weeks)

**Goal**: Implement complete Firebird-compatible system catalog tables and metadata queries

**Total Features**: 8
**Dependencies**: Phases 1-9 (all DDL/DML features)

---

### SYS-001: Complete System Catalog Tables
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Catalog Manager
- **Estimate**: 3 days
- **Dependencies**: None

**Tasks**:
1. Audit existing system tables (from previous phases):
   - ✅ `pg_namespace` (schemas)
   - ✅ `pg_class` (tables)
   - ✅ `pg_attribute` (columns)
   - ✅ `pg_index` (indexes)
   - ✅ `pg_proc` (procedures/functions)
   - ✅ `pg_trigger` (triggers)
2. Add missing Firebird-compatible tables:
   - `RDB$DATABASE` - database metadata
   - `RDB$RELATIONS` - tables/views (alias for pg_class)
   - `RDB$RELATION_FIELDS` - columns (alias for pg_attribute)
   - `RDB$INDICES` - indexes (alias for pg_index)
   - `RDB$PROCEDURES` - stored procedures (alias for pg_proc)
   - `RDB$FUNCTIONS` - UDFs/functions (alias for pg_proc where prorettype != void)
   - `RDB$TRIGGERS` - triggers (alias for pg_trigger)
   - `RDB$DEPENDENCIES` - object dependencies
   - `RDB$TYPES` - data types
   - `RDB$FIELDS` - domain definitions
   - `RDB$SEQUENCES` / `RDB$GENERATORS` - sequences
   - `RDB$CHECK_CONSTRAINTS` - CHECK constraints
   - `RDB$REF_CONSTRAINTS` - foreign key constraints
   - `RDB$RELATION_CONSTRAINTS` - table constraints (PK, UNIQUE, FK, CHECK)
3. Create views mapping RDB$ names to pg_ tables for Firebird compatibility

**Files to Modify**:
- `src/core/catalog_manager.cpp`
- `src/core/database.cpp` (initialize system tables on create)
- `docs/system_tables/RDB_SCHEMA.md` (new - document Firebird compatibility layer)

**Acceptance Criteria**:
```sql
SELECT * FROM RDB$RELATIONS WHERE RDB$RELATION_NAME = 'EMPLOYEES'
-- Returns table metadata in Firebird format
```

---

### SYS-002: INFORMATION_SCHEMA Views
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Catalog Manager
- **Estimate**: 2 days
- **Dependencies**: SYS-001

**Tasks**:
1. Implement SQL:2003 INFORMATION_SCHEMA views (alternative to RDB$ tables):
   - `INFORMATION_SCHEMA.TABLES`
   - `INFORMATION_SCHEMA.COLUMNS`
   - `INFORMATION_SCHEMA.VIEWS`
   - `INFORMATION_SCHEMA.ROUTINES` (procedures/functions)
   - `INFORMATION_SCHEMA.PARAMETERS` (routine parameters)
   - `INFORMATION_SCHEMA.TABLE_CONSTRAINTS`
   - `INFORMATION_SCHEMA.KEY_COLUMN_USAGE`
   - `INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS`
   - `INFORMATION_SCHEMA.CHECK_CONSTRAINTS`
   - `INFORMATION_SCHEMA.TRIGGERS`
2. Implement as views on top of pg_ system tables

**Files to Modify**:
- `src/core/catalog_manager.cpp`
- `docs/system_tables/INFORMATION_SCHEMA.md` (new)

**Acceptance Criteria**:
```sql
SELECT TABLE_NAME, TABLE_TYPE FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'public'
-- Returns all tables in public schema
```

---

### SYS-003: Metadata Query Functions
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor (Built-in Functions)
- **Estimate**: 1.5 days
- **Dependencies**: SYS-001

**Tasks**:
1. Implement metadata functions:
   - `RDB$GET_CONTEXT(namespace, variable)` - get context variable
   - `RDB$SET_CONTEXT(namespace, variable, value)` - set context variable
   - Namespaces: 'SYSTEM', 'USER_SESSION', 'USER_TRANSACTION'
   - System variables: 'CURRENT_USER', 'CURRENT_ROLE', 'SESSION_ID', 'TRANSACTION_ID', 'ISOLATION_LEVEL'
2. Implement:
   - `CURRENT_USER` - current username (for Alpha: 'SYSDBA')
   - `CURRENT_ROLE` - current role (for Alpha: NULL)
   - `CURRENT_CONNECTION` - connection ID

**Files to Modify**:
- `src/sblr/builtin_functions.cpp`

**Acceptance Criteria**:
```sql
SELECT RDB$GET_CONTEXT('SYSTEM', 'CURRENT_USER'), CURRENT_CONNECTION
-- Returns: 'SYSDBA', 1
```

---

### SYS-004: Sequence/Generator Support
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Full Stack
- **Estimate**: 2.5 days
- **Dependencies**: None

**Tasks**:
1. Add `pg_sequence` / `RDB$GENERATORS` system table:
   - `oid`, `seqname`, `seqnamespace`
   - `seqstart`, `seqincrement`, `seqmin`, `seqmax`, `seqcycle`
   - `last_value`, `is_called`
2. Parse DDL:
   - `CREATE SEQUENCE seq_name [START WITH n] [INCREMENT BY n]`
   - `CREATE GENERATOR gen_name` (Firebird alias)
   - `SET GENERATOR gen_name TO n` (Firebird)
   - `ALTER SEQUENCE seq_name RESTART WITH n`
   - `DROP SEQUENCE seq_name`
3. Generate bytecode and execute
4. Implement functions:
   - `NEXT VALUE FOR seq_name` (SQL standard)
   - `GEN_ID(gen_name, increment)` (Firebird, returns next value with custom increment)
5. Implement in catalog:
   - `createSequence()`, `getNextValue(seq_name)`, `setSequenceValue(seq_name, value)`

**Files to Modify**:
- `src/parser/parser.cpp`
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/executor.cpp`
- `src/core/catalog_manager.cpp`
- `src/sblr/builtin_functions.cpp` (GEN_ID)

**Acceptance Criteria**:
```sql
CREATE SEQUENCE emp_id_seq START WITH 100 INCREMENT BY 1;
SELECT NEXT VALUE FOR emp_id_seq;  -- Returns 100
SELECT NEXT VALUE FOR emp_id_seq;  -- Returns 101
SELECT GEN_ID(emp_id_seq, 5);      -- Returns 106
```

---

### SYS-005: View Support
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Full Stack
- **Estimate**: 2 days
- **Dependencies**: Phase 3 (subqueries)

**Tasks**:
1. Add `pg_views` / `RDB$VIEW_RELATIONS` table:
   - `oid`, `viewname`, `viewnamespace`
   - `viewdefinition` (TEXT, SELECT query)
   - `viewsrc` (TEXT, original SQL)
2. Parse DDL:
   - `CREATE VIEW view_name [(columns)] AS SELECT ...`
   - `CREATE OR REPLACE VIEW ...`
   - `DROP VIEW view_name`
3. Generate bytecode and execute:
   - Store view definition in catalog
4. Query execution:
   - When view is referenced in FROM clause, inline its SELECT query (view expansion)
   - Alternatively, materialize view as subquery
5. Implement `catalog_mgr->createView()`, `getView()`, `listViews()`

**Files to Modify**:
- `src/parser/parser.cpp`
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/executor.cpp`
- `src/core/catalog_manager.cpp`

**Acceptance Criteria**:
```sql
CREATE VIEW high_earners AS
    SELECT * FROM employees WHERE salary > 100000;

SELECT * FROM high_earners WHERE department = 'Engineering';
-- Executes expanded query: SELECT * FROM employees WHERE salary > 100000 AND department = 'Engineering'
```

---

### SYS-006: Domain Support
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Full Stack
- **Estimate**: 1.5 days
- **Dependencies**: Phase 5 (constraints)

**Tasks**:
1. Add `pg_domain` / `RDB$FIELDS` table for domain definitions:
   - `oid`, `domainname`, `domainnamespace`
   - `base_type`, `max_length`, `precision`, `scale`
   - `default_value`, `not_null`, `check_constraint`
2. Parse DDL:
   - `CREATE DOMAIN domain_name AS base_type [DEFAULT value] [NOT NULL] [CHECK (condition)]`
   - `ALTER DOMAIN domain_name {SET DEFAULT value | DROP DEFAULT | ADD CHECK (...) | DROP CONSTRAINT}`
   - `DROP DOMAIN domain_name`
3. Implement domain usage:
   - Allow column definitions: `CREATE TABLE t (col domain_name)`
   - Inherit domain's base type, default, NOT NULL, CHECK constraint
   - Domain constraint checked on INSERT/UPDATE
4. Implement in catalog: `createDomain()`, `getDomain()`, `alterDomain()`, `dropDomain()`

**Files to Modify**:
- `src/parser/parser.cpp`
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/executor.cpp`
- `src/core/catalog_manager.cpp`

**Acceptance Criteria**:
```sql
CREATE DOMAIN d_email AS VARCHAR(100) CHECK (VALUE LIKE '%@%');
CREATE TABLE users (id INTEGER, email d_email);
INSERT INTO users VALUES (1, 'invalid');  -- Error: CHECK constraint fails
INSERT INTO users VALUES (1, 'user@example.com');  -- Success
```

---

### SYS-007: Collation Support (Basic)
- **Priority**: LOW
- **Status**: Not Started
- **Component**: Full Stack
- **Estimate**: 1 day (deferred to Beta for full Unicode collation)
- **Dependencies**: None

**Tasks**:
1. For Alpha: support basic collations:
   - `COLLATE BINARY` - byte-by-byte comparison
   - `COLLATE NOCASE` - case-insensitive ASCII (already implemented in comparisons)
2. Parse `col VARCHAR(100) COLLATE collation_name`
3. Store collation in `pg_attribute.attcollation`
4. Apply collation in comparisons, ORDER BY, GROUP BY
5. Full Unicode collation (ICU) deferred to Beta

**Files to Modify**:
- `src/parser/parser.cpp`
- `src/core/catalog_manager.cpp`
- `src/sblr/executor.cpp` (apply collation in comparisons)

---

### SYS-008: Transaction Metadata
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Transaction Manager
- **Estimate**: 1 day
- **Dependencies**: None

**Tasks**:
1. Add system views:
   - `RDB$TRANSACTIONS` - currently active transactions
     - Columns: `transaction_id`, `start_time`, `isolation_level`, `read_only`, `state`
   - `MON$TRANSACTIONS` (Firebird monitoring table alias)
2. Implement in TransactionManager:
   - `getActiveTransactions()` returns list of active XIDs with metadata
3. Allow querying: `SELECT * FROM RDB$TRANSACTIONS WHERE TRANSACTION_ID = CURRENT_TRANSACTION`

**Files to Modify**:
- `src/core/transaction_manager.cpp`
- `src/core/catalog_manager.cpp` (register view)

**Acceptance Criteria**:
```sql
BEGIN;
SELECT * FROM RDB$TRANSACTIONS WHERE TRANSACTION_ID = CURRENT_TRANSACTION;
-- Returns: current XID, start time, isolation level
COMMIT;
```

---

**Phase 10 Summary**:
- **Total Effort**: 12.5 days (2.5 weeks)
- **High Features**: 4
- **Medium Features**: 3
- **Low Features**: 1
- **Deliverables**:
  - Complete Firebird-compatible system catalog (RDB$ tables)
  - INFORMATION_SCHEMA views
  - Metadata query functions (RDB$GET_CONTEXT, CURRENT_USER)
  - Sequence/Generator support (CREATE SEQUENCE, GEN_ID)
  - View support (CREATE VIEW, view expansion)
  - Domain support (CREATE DOMAIN)
  - Basic collation support
  - Transaction metadata queries

---

## PHASE 11: Query Optimizer (17.5 days, 3.5 weeks)

**Goal**: Implement basic cost-based query optimizer for efficient query execution

**Total Features**: 10
**Dependencies**: All query execution features (Phases 1-3)

---

### OPT-001: Statistics Collection Framework
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Catalog Manager
- **Estimate**: 2.5 days
- **Dependencies**: None

**Tasks**:
1. Add `pg_statistic` / `RDB$STATISTICS` table:
   - `starelid` (table OID), `staattnum` (column number)
   - `stanullfrac` (fraction of NULLs)
   - `stawidth` (average width in bytes)
   - `stadistinct` (number of distinct values, -1 = all unique)
   - `stakind1..stakind5` (statistic kinds: most_common_vals, histogram, etc.)
   - `stavalues1..stavalues5` (arrays of values for statistics)
2. Implement `ANALYZE table_name` command:
   - Parse DDL: `ANALYZE TABLE table_name`
   - Execute: scan table, compute statistics for each column
   - Store in `pg_statistic`
3. Statistics to collect:
   - Row count
   - NULL fraction
   - Distinct value count (HyperLogLog for large tables)
   - Most common values (top 10) with frequencies
   - Min/max values
   - Histogram buckets (equi-depth or equi-width)

**Files to Modify**:
- `src/parser/parser.cpp`
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/executor.cpp`
- `src/core/catalog_manager.cpp`
- `src/core/statistics_collector.cpp` (new)
- `include/scratchbird/core/statistics_collector.h` (new)

**Acceptance Criteria**:
```sql
ANALYZE TABLE employees;
-- Collects statistics on employees table
SELECT stadistinct FROM pg_statistic WHERE starelid = (SELECT oid FROM pg_class WHERE relname = 'employees') AND staattnum = 1;
-- Returns: estimated distinct count for first column
```

---

### OPT-002: Cardinality Estimation
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Optimizer
- **Estimate**: 2 days
- **Dependencies**: OPT-001

**Tasks**:
1. Implement selectivity estimation for predicates:
   - `col = value`: selectivity = 1 / distinct_count (if value in most_common_vals, use actual frequency)
   - `col < value`: use histogram to estimate fraction of rows
   - `col > value`, `col <= value`, `col >= value`: similar
   - `col BETWEEN a AND b`: histogram-based
   - `col IS NULL`: use null_frac
   - `col LIKE 'prefix%'`: heuristic (0.1 for general pattern, higher for specific prefix)
   - `col IN (v1, v2, ...)`: sum of individual selectivities
2. Combine selectivities:
   - AND: multiply (assuming independence)
   - OR: 1 - (1 - s1) * (1 - s2)
   - NOT: 1 - s
3. Estimate cardinality: `card(result) = card(base_table) * selectivity`

**Files to Modify**:
- `src/optimizer/cardinality_estimator.cpp` (new)
- `include/scratchbird/optimizer/cardinality_estimator.h` (new)

**Acceptance Criteria**:
```cpp
auto stats = catalog_mgr->getStatistics(table_id, col_num);
double selectivity = estimateSelectivity(col == 100, stats);
double cardinality = base_cardinality * selectivity;
// Returns accurate estimate based on statistics
```

---

### OPT-003: Join Cardinality Estimation
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Optimizer
- **Estimate**: 1.5 days
- **Dependencies**: OPT-002

**Tasks**:
1. Estimate join cardinality:
   - Cross join: `card(R × S) = card(R) * card(S)`
   - Equi-join `R.a = S.b`:
     - If foreign key: `card = card(R)` (each R row joins with 1 S row)
     - Else: `card = card(R) * card(S) / max(distinct(R.a), distinct(S.b))`
   - Non-equi join: use cross join estimate * selectivity
2. Multi-way joins: apply formula iteratively

**Files to Modify**:
- `src/optimizer/cardinality_estimator.cpp`

---

### OPT-004: Cost Model
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Optimizer
- **Estimate**: 2 days
- **Dependencies**: OPT-002

**Tasks**:
1. Define cost units:
   - Seq scan: `cost = pages_read * seq_page_cost + rows * cpu_tuple_cost`
   - Index scan: `cost = index_pages * random_page_cost + rows * cpu_index_tuple_cost + rows * cpu_tuple_cost`
   - Sort: `cost = N * log(N) * cpu_operator_cost + memory_cost`
   - Hash join: `cost = build_cost + probe_cost + hash_cost`
   - Nested loop join: `cost = outer_cost + inner_cost * outer_rows`
2. Cost parameters (tunable):
   - `seq_page_cost = 1.0`
   - `random_page_cost = 4.0` (SSD: 1.5, HDD: 4.0)
   - `cpu_tuple_cost = 0.01`
   - `cpu_index_tuple_cost = 0.005`
   - `cpu_operator_cost = 0.0025`
3. Implement cost functions for each operator

**Files to Modify**:
- `src/optimizer/cost_model.cpp` (new)
- `include/scratchbird/optimizer/cost_model.h` (new)
- `src/core/database.cpp` (store cost parameters in database header)

---

### OPT-005: Index Selection
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Optimizer
- **Estimate**: 2 days
- **Dependencies**: OPT-004

**Tasks**:
1. For each table scan, consider:
   - Sequential scan (always possible)
   - Index scans (if WHERE clause matches index columns)
2. Index applicability:
   - B-Tree: `=`, `<`, `>`, `<=`, `>=`, `BETWEEN`, `IN`, `IS NULL`
   - Hash: `=` only
   - GIN: array contains, full-text search
   - Bitmap: combine multiple indexes
3. For each applicable index:
   - Estimate cost of index scan
   - Compare with seq scan cost
   - Choose cheaper option
4. Multi-column indexes: check if WHERE clause matches index prefix

**Files to Modify**:
- `src/optimizer/index_selector.cpp` (new)
- `include/scratchbird/optimizer/index_selector.h` (new)

**Acceptance Criteria**:
```sql
-- Given: CREATE INDEX idx_emp_dept ON employees(department);
-- Query: SELECT * FROM employees WHERE department = 'Engineering';
-- Optimizer chooses index scan on idx_emp_dept (cost < seq scan)
```

---

### OPT-006: Join Order Optimization
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Optimizer
- **Estimate**: 3 days
- **Dependencies**: OPT-003, OPT-004

**Tasks**:
1. For 2-3 tables: exhaustive search (all permutations)
2. For 4+ tables: dynamic programming (DPccp algorithm):
   - Build optimal plans bottom-up
   - For each subset S of tables:
     - For each partition (S1, S2) of S:
       - Cost(S) = min(Cost(S1) + Cost(S2) + JoinCost(S1, S2))
3. Heuristics:
   - Push down selections (apply WHERE early)
   - Join smaller tables first
   - Avoid Cartesian products (prefer joins with predicates)
4. Output: optimal join tree

**Files to Modify**:
- `src/optimizer/join_order_optimizer.cpp` (new)
- `include/scratchbird/optimizer/join_order_optimizer.h` (new)

**Acceptance Criteria**:
```sql
SELECT * FROM orders o
JOIN customers c ON o.customer_id = c.id
JOIN products p ON o.product_id = p.id
WHERE c.country = 'USA' AND p.price > 100;
-- Optimizer chooses: (customers WHERE country='USA') ⨝ orders ⨝ (products WHERE price>100)
-- Instead of: orders ⨝ customers ⨝ products, then filter
```

---

### OPT-007: Join Algorithm Selection
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Optimizer
- **Estimate**: 1.5 days
- **Dependencies**: OPT-004

**Tasks**:
1. Implement join algorithm selection:
   - **Nested Loop Join**: good for small outer, indexed inner
   - **Hash Join**: good for equi-joins, medium-large tables
   - **Sort-Merge Join**: good for sorted inputs, equi-joins
2. Cost each algorithm:
   - Nested loop: `outer_rows * (inner_scan_cost)`
   - Hash: `build_table_cost + probe_table_cost + hash(build_rows) * cpu_cost`
   - Sort-merge: `sort(outer) + sort(inner) + merge_cost`
3. Choose algorithm with minimum cost
4. Consider available memory for hash join (spill to disk if needed)

**Files to Modify**:
- `src/optimizer/join_algorithm_selector.cpp` (new)
- `include/scratchbird/optimizer/join_algorithm_selector.h` (new)

---

### OPT-008: Predicate Pushdown
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Optimizer
- **Estimate**: 1.5 days
- **Dependencies**: OPT-006

**Tasks**:
1. For each WHERE clause predicate:
   - If predicate references only one table, push down to that table scan
   - If predicate references two tables, push to join operator
2. Benefits:
   - Reduce rows scanned early
   - Enable index usage
3. Rewrite query plan to apply filters at lowest possible level

**Files to Modify**:
- `src/optimizer/predicate_pushdown.cpp` (new)

**Acceptance Criteria**:
```sql
SELECT * FROM orders o JOIN customers c ON o.customer_id = c.id WHERE c.country = 'USA'
-- Optimizer pushes 'c.country = USA' down to customers scan
-- Plan: (SELECT * FROM customers WHERE country='USA') JOIN orders
```

---

### OPT-009: Projection Pushdown
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Optimizer
- **Estimate**: 1 day
- **Dependencies**: None

**Tasks**:
1. Analyze SELECT list and determine required columns
2. Push projections down to scans: only read needed columns from storage
3. Benefits:
   - Reduce I/O (fewer columns read)
   - Reduce memory usage
4. Handle:
   - Explicit SELECT columns
   - Columns needed for WHERE, JOIN, GROUP BY, ORDER BY
   - Add implicit columns, then project them out later

**Files to Modify**:
- `src/optimizer/projection_pushdown.cpp` (new)

---

### OPT-010: Query Plan Caching
- **Priority**: LOW
- **Status**: Not Started
- **Component**: Optimizer
- **Estimate**: 1.5 days
- **Dependencies**: All optimizer features

**Tasks**:
1. Implement plan cache:
   - Key: normalized query string (parameters replaced with '?')
   - Value: optimized execution plan
2. On query execution:
   - Normalize query
   - Check cache
   - If hit: reuse plan (substitute actual parameter values)
   - If miss: optimize, cache plan
3. Cache eviction: LRU, max 1000 entries
4. Invalidate on DDL changes (ALTER TABLE, CREATE INDEX, DROP)

**Files to Modify**:
- `src/optimizer/plan_cache.cpp` (new)
- `include/scratchbird/optimizer/plan_cache.h` (new)
- `src/sblr/bytecode_generator.cpp` (integrate cache)

**Acceptance Criteria**:
```sql
SELECT * FROM employees WHERE id = 1;  -- Optimize, cache plan
SELECT * FROM employees WHERE id = 2;  -- Reuse cached plan
```

---

**Phase 11 Summary**:
- **Total Effort**: 17.5 days (3.5 weeks)
- **High Features**: 7
- **Medium Features**: 2
- **Low Features**: 1
- **Deliverables**:
  - Statistics collection (ANALYZE TABLE)
  - Cardinality estimation for selections and joins
  - Cost model with tunable parameters
  - Index selection (choose between seq scan and index scan)
  - Join order optimization (dynamic programming for 4+ tables)
  - Join algorithm selection (nested loop, hash, sort-merge)
  - Predicate and projection pushdown
  - Query plan caching

---

## PHASE 12: Embedded Engine Integration (12.5 days, 2.5 weeks)

**Goal**: Create embedded engine API and integrate with Firebird-compatible SQL parser for ISQL tool

**Total Features**: 6
**Dependencies**: All phases 1-11 complete

---

### EMB-001: Embedded Engine API Design
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: API Layer
- **Estimate**: 2 days
- **Dependencies**: None

**Tasks**:
1. Design C++ API for embedded engine:
   ```cpp
   class ScratchBirdEngine {
   public:
       Status open(const std::string& db_path);
       Status close();
       Status execute(const std::string& sql, ResultSet& result);
       Status prepare(const std::string& sql, PreparedStatement& stmt);
       Status beginTransaction(IsolationLevel level);
       Status commit();
       Status rollback();
   };

   class ResultSet {
   public:
       bool next();
       Value getValue(int col_index);
       std::string getColumnName(int col_index);
       int getColumnCount();
   };

   class PreparedStatement {
   public:
       Status setParameter(int index, const Value& value);
       Status execute(ResultSet& result);
   };
   ```
2. Implement in-process embedding:
   - No network layer
   - Direct function calls to Database, Parser, Executor
3. Thread safety: one engine instance per thread (or use mutex)

**Files to Modify**:
- `include/scratchbird/embedded/engine.h` (new)
- `src/embedded/engine.cpp` (new)

---

### EMB-002: Result Set Implementation
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: API Layer
- **Estimate**: 1.5 days
- **Dependencies**: EMB-001

**Tasks**:
1. Implement `ResultSet` class:
   - Wrap executor result (vector of tuples)
   - Provide iterator interface: `next()`, `getValue(col)`
   - Support column metadata: names, types
2. Implement `Value` variant type:
   - Union of all data types: int32, int64, double, string, timestamp, etc.
   - `Value::getInt()`, `Value::getString()`, `Value::isNull()`

**Files to Modify**:
- `src/embedded/result_set.cpp` (new)
- `include/scratchbird/embedded/result_set.h` (new)

**Acceptance Criteria**:
```cpp
ScratchBirdEngine engine;
engine.open("test.db");
ResultSet rs;
engine.execute("SELECT id, name FROM employees", rs);
while (rs.next()) {
    int id = rs.getValue(0).getInt();
    std::string name = rs.getValue(1).getString();
    std::cout << id << ": " << name << std::endl;
}
engine.close();
```

---

### EMB-003: Prepared Statement Support
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: API Layer
- **Estimate**: 2 days
- **Dependencies**: EMB-002

**Tasks**:
1. Extend parser to support parameter placeholders:
   - Parse `?` as parameter marker
   - Generate `ParameterExpression` AST node with index
2. Bytecode generator:
   - Emit `OP_LOAD_PARAM` opcode with parameter index
3. PreparedStatement:
   - Store parsed AST and bytecode
   - `setParameter(index, value)`: bind value to parameter slot
   - `execute()`: run bytecode with bound parameters
4. Benefits:
   - Parse once, execute many times
   - SQL injection prevention (parameterized queries)

**Files to Modify**:
- `src/parser/parser.cpp` (parse ?)
- `src/sblr/bytecode_generator.cpp` (OP_LOAD_PARAM)
- `src/sblr/executor.cpp` (execute with parameters)
- `src/embedded/prepared_statement.cpp` (new)
- `include/scratchbird/embedded/prepared_statement.h` (new)

**Acceptance Criteria**:
```cpp
PreparedStatement stmt;
engine.prepare("SELECT * FROM employees WHERE id = ?", stmt);
stmt.setParameter(0, Value(1));
ResultSet rs;
stmt.execute(rs);  // SELECT * FROM employees WHERE id = 1
```

---

### EMB-004: Firebird SQL Parser Integration
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Parser
- **Estimate**: 2.5 days
- **Dependencies**: All DDL/DML/PSQL features

**Tasks**:
1. Create Firebird-compatible parser mode:
   - Use existing ScratchBird parser
   - Add Firebird-specific syntax quirks:
     - `GEN_ID(generator, increment)` instead of `NEXT VALUE FOR`
     - `EXECUTE BLOCK` for anonymous blocks
     - `RETURNING` clause for INSERT/UPDATE/DELETE
     - `ROWS n TO m` instead of `LIMIT m-n+1 OFFSET n-1`
2. Add parser flag: `FIREBIRD_COMPAT_MODE`
3. Map Firebird syntax to ScratchBird AST
4. Test against Firebird SQL test suite (subset)

**Files to Modify**:
- `src/parser/parser.cpp` (add Firebird compat mode)
- `src/parser/lexer.cpp` (Firebird keywords)
- `docs/FIREBIRD_COMPATIBILITY.md` (new - document differences)

**Acceptance Criteria**:
```cpp
engine.setCompatMode(FIREBIRD_COMPAT_MODE);
engine.execute("SELECT * FROM employees ROWS 10 TO 20");  // Firebird LIMIT syntax
// Executes successfully
```

---

### EMB-005: ISQL Command-Line Tool
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Tools
- **Estimate**: 3 days
- **Dependencies**: EMB-001, EMB-002, EMB-004

**Tasks**:
1. Create `isql` command-line tool (Firebird-compatible interface):
   ```bash
   isql test.db
   SQL> SELECT * FROM employees;
   ID  NAME          SALARY
   ==  ============  ======
   1   Alice         50000
   2   Bob           60000
   SQL> \q
   ```
2. Features:
   - Interactive SQL execution
   - Display results in table format
   - Meta-commands:
     - `\d table_name` - describe table
     - `\dt` - list tables
     - `\df` - list functions
     - `\l` - list databases
     - `\q` - quit
     - `\i filename.sql` - execute script file
   - Command history (readline library)
   - Multi-line input (detect `;` terminator)
3. Use embedded engine API
4. Output formatting: aligned columns, borders

**Files to Modify**:
- `tools/isql/main.cpp` (new)
- `tools/isql/result_formatter.cpp` (new)
- `CMakeLists.txt` (add isql target)

**Acceptance Criteria**:
```bash
$ isql test.db
SQL> CREATE TABLE t (id INTEGER, name VARCHAR(50));
SQL> INSERT INTO t VALUES (1, 'Alice');
SQL> SELECT * FROM t;
 id | name
----+------
  1 | Alice
(1 row)
SQL> \q
```

---

### EMB-006: SQL Script Execution
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Tools
- **Estimate**: 1.5 days
- **Dependencies**: EMB-005

**Tasks**:
1. Extend `isql` to execute script files:
   ```bash
   isql test.db < schema.sql
   isql test.db -i schema.sql
   ```
2. Features:
   - Parse file line-by-line
   - Detect statement boundaries (`;` or Firebird `SET TERM`)
   - Execute each statement
   - Handle errors: abort on error vs. continue
   - Transaction control: auto-commit vs. manual
3. Implement `SET TERM` (Firebird):
   ```sql
   SET TERM !! ;
   CREATE PROCEDURE foo AS BEGIN ... END !!
   SET TERM ; !!
   ```
   - Allows changing statement terminator for procedure/trigger definitions

**Files to Modify**:
- `tools/isql/script_executor.cpp` (new)
- `tools/isql/main.cpp` (add -i flag)

**Acceptance Criteria**:
```bash
$ cat schema.sql
CREATE TABLE employees (id INTEGER, name VARCHAR(50));
INSERT INTO employees VALUES (1, 'Alice');
INSERT INTO employees VALUES (2, 'Bob');

$ isql test.db -i schema.sql
Executed 3 statements successfully.
```

---

**Phase 12 Summary**:
- **Total Effort**: 12.5 days (2.5 weeks)
- **Critical Features**: 3
- **High Features**: 3
- **Deliverables**:
  - Embedded engine API (C++ interface)
  - ResultSet and PreparedStatement support
  - Firebird SQL parser compatibility mode
  - ISQL command-line tool (Firebird-compatible)
  - SQL script execution
  - Complete embedded mode: application → parser → engine (no network)

---

## FINAL ALPHA CHECKLIST

### Functional Completeness
- [x] Storage Engine (100% - MGA, MVCC, buffer pool, indexes)
- [x] Transaction Manager (100% - ACID, isolation levels, group commit)
- [ ] DDL Support:
  - [ ] CREATE/ALTER/DROP TABLE
  - [ ] CREATE/DROP INDEX
  - [ ] CREATE/DROP VIEW
  - [ ] CREATE/DROP SEQUENCE
  - [ ] CREATE/DROP DOMAIN
  - [ ] CREATE/ALTER/DROP PROCEDURE/FUNCTION/TRIGGER
- [ ] DML Support:
  - [ ] SELECT (basic, JOINs, subqueries, GROUP BY, ORDER BY, LIMIT)
  - [ ] INSERT
  - [ ] UPDATE
  - [ ] DELETE
- [ ] PSQL Support:
  - [ ] Variables, IF/WHILE/FOR loops
  - [ ] Cursors, exception handling
  - [ ] Stored procedures, functions, triggers
- [ ] Advanced Features:
  - [ ] CTEs (WITH, WITH RECURSIVE)
  - [ ] Window functions
  - [ ] Built-in functions (math, string, date/time, conditional, conversion)
- [ ] System Catalog:
  - [ ] Firebird-compatible RDB$ tables
  - [ ] INFORMATION_SCHEMA views
  - [ ] Metadata query functions
- [ ] Query Optimizer:
  - [ ] Statistics collection (ANALYZE)
  - [ ] Cardinality estimation
  - [ ] Index selection
  - [ ] Join order optimization
  - [ ] Cost-based plan selection
- [ ] Embedded Engine:
  - [ ] C++ API
  - [ ] Prepared statements
  - [ ] Firebird SQL parser
  - [ ] ISQL tool

### Quality Assurance
- [ ] All critical issues from audit resolved (5 issues)
- [ ] All high-priority issues resolved (8 issues)
- [ ] Test coverage ≥ 60%
- [ ] ThreadSanitizer tests passing
- [ ] Valgrind/ASan tests passing
- [ ] Concurrency stress tests (100+ threads)
- [ ] TPC-C benchmark (basic workload)

### Documentation
- [ ] API documentation (embedded engine)
- [ ] SQL reference (DDL/DML/PSQL)
- [ ] System table reference (RDB$, INFORMATION_SCHEMA)
- [ ] Query optimizer guide
- [ ] ISQL user guide
- [ ] Firebird compatibility matrix

### Deliverables
- [ ] ScratchBird embedded library (`libscratchbird_embedded.so`)
- [ ] ISQL tool (`isql` executable)
- [ ] SQL test suite (1000+ tests)
- [ ] Example applications (CRUD app using embedded engine)
- [ ] Migration guide (Firebird → ScratchBird)

---

## POST-ALPHA: BETA ROADMAP PREVIEW

**Goal**: Transform Alpha (embedded-only) into Beta (client-server architecture)

### Beta Phase 1: Network Layer (8 weeks)
- [ ] Wire protocol design (Firebird-compatible or PostgreSQL protocol)
- [ ] Server process (`scratchbird-server`)
- [ ] Client library (`libscratchbird_client.so`)
- [ ] Connection pooling
- [ ] Authentication & authorization (users, roles, grants)
- [ ] SSL/TLS support

### Beta Phase 2: Concurrency & Scalability (6 weeks)
- [ ] Multi-threaded query execution (parallel scans, parallel joins)
- [ ] Background writer process
- [ ] Checkpoint process (coordinated with WAL)
- [ ] Autovacuum (automated garbage collection)
- [ ] Connection scaling (1000+ concurrent connections)

### Beta Phase 3: WAL & Durability (6 weeks)
- [ ] Write-Ahead Logging (WAL)
- [ ] Point-in-time recovery (PITR)
- [ ] Streaming replication (master-replica)
- [ ] Logical replication (pub/sub)

### Beta Phase 4: Advanced Features (8 weeks)
- [ ] Full-text search (FTS indexes, ranking)
- [ ] JSON/JSONB data type
- [ ] Array operations (advanced indexing, array_agg)
- [ ] Full Unicode collation (ICU integration)
- [ ] Geospatial data (PostGIS-compatible)
- [ ] B-Tree prefix compression (deferred from Alpha)

### Beta Phase 5: Enterprise Features (6 weeks)
- [ ] Backup/restore (online backup)
- [ ] Import/export (CSV, JSON)
- [ ] Database encryption (at-rest)
- [ ] Audit logging
- [ ] Performance monitoring dashboard
- [ ] Query profiling (EXPLAIN ANALYZE with actual timings)

### Beta Phase 6: Tooling & Ecosystem (4 weeks)
- [ ] Language drivers (C, Python, JavaScript, Go, Rust, Java)
- [ ] ORM support (SQLAlchemy, Hibernate, Sequelize)
- [ ] Admin GUI tool
- [ ] Migration tool (Firebird, PostgreSQL, MySQL → ScratchBird)
- [ ] Cloud deployment guides (AWS, GCP, Azure)

**Estimated Beta Timeline**: 38 weeks (~9 months) after Alpha completion

---

## EFFORT SUMMARY

### Alpha Completion (Phases 1-12)
| Phase | Effort | Duration |
|-------|--------|----------|
| Phase 1: Core DML Execution | 30 days | 6 weeks |
| Phase 2: Aggregation & Grouping | 17.5 days | 3.5 weeks |
| Phase 3: Subqueries & Set Operations | 20 days | 4 weeks |
| Phase 4: DDL Expansion | 27.5 days | 5.5 weeks |
| Phase 5: Constraints & Indexes | 25 days | 5 weeks |
| Phase 6: PSQL Basics | 30 days | 6 weeks |
| Phase 7: Stored Procedures & Triggers | 27.5 days | 5.5 weeks |
| Phase 8: Advanced Features (CTEs, Windows) | 25 days | 5 weeks |
| Phase 9: Built-in Functions | 17.5 days | 3.5 weeks |
| Phase 10: System Tables & Metadata | 12.5 days | 2.5 weeks |
| Phase 11: Query Optimizer | 17.5 days | 3.5 weeks |
| Phase 12: Embedded Engine Integration | 12.5 days | 2.5 weeks |
| **TOTAL** | **262.5 days** | **52.5 weeks** |

### Team Size Scenarios
- **Solo developer**: 262.5 days ≈ **12-14 months** (accounting for testing, debugging, rework)
- **2 developers**: ~7-9 months (parallelizable phases: 1-3, 4-6, 7-9, 10-12)
- **3 developers**: ~5-7 months (3-way parallelization)

### Critical Path
The following phases MUST be completed sequentially:
1. Phase 1 → Phase 2 → Phase 3 (core query execution)
2. Phase 4 → Phase 5 (DDL and constraints)
3. Phase 6 → Phase 7 (PSQL basics → procedures/triggers)
4. Phase 10 → Phase 11 (metadata → optimizer)
5. Phase 12 (embedded integration - requires all previous phases)

Parallelizable:
- Phases 8-9 can run in parallel with Phase 7
- Phase 10 can start after Phase 4
- Phase 11 can start after Phase 3

---

## SUCCESS CRITERIA

**Alpha is complete when**:
1. ✅ All 227 features implemented
2. ✅ All critical and high-priority issues from audit resolved
3. ✅ ISQL tool can execute complex Firebird SQL scripts
4. ✅ Test coverage ≥ 60%
5. ✅ ThreadSanitizer and Valgrind tests pass
6. ✅ TPC-C benchmark runs successfully
7. ✅ Embedded engine API stable and documented
8. ✅ Migration guide from Firebird available
9. ✅ Example applications demonstrate all major features

**Then**: Begin Beta development (network layer, WAL, replication)

---

**End of Alpha Implementation Plan**

**Document Version**: 1.0
**Last Updated**: October 16, 2025
**Total Pages**: 3 (Parts 1, 2, 3)
**Total Features**: 227
**Total Effort**: 262.5 days (52.5 weeks / 12-14 months solo)

---

**Next Steps**:
1. Review and approve implementation plan
2. Set up project tracking (GitHub Projects, Jira, etc.)
3. Begin Phase 1: Core DML Execution
4. Resolve critical issues from audit in parallel
5. Establish CI/CD with TSAN, Valgrind, coverage reporting

**Questions for Project Lead**:
1. Preferred team size for Alpha development?
2. Target completion date for Alpha?
3. Priority: speed vs. quality (aggressive timeline vs. thorough testing)?
4. Firebird compatibility: 100% strict or "mostly compatible"?
5. Which phases should be prioritized if timeline is compressed?

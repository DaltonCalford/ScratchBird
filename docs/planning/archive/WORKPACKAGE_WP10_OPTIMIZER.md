# Work Package 10: Optimizer

**Status:** COMPLETE (17/17 complete - 100%)
**Priority:** P1-P3 Mixed
**Estimated Hours:** 28-36
**Files:** src/optimizer/*.cpp, src/core/catalog_manager.cpp

---

## Overview

The query optimizer has several incomplete features including statistics persistence, index advisor, materialized view rewriting, and cost estimation gaps.

---

## Tasks

### OPT-1: storeColumnStatistics persistence (HIGH)
**File:** src/optimizer/statistics_manager.cpp
**Lines:** 1360-1508
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Full pg_statistic catalog persistence:
- Created `StatisticInfo` struct in CatalogManager with all statistics fields
- Created `StatisticRecord` on-disk structure (160 bytes)
- Implemented CRUD operations: `storeStatistic()`, `getStatistic()`, `getStatisticsForTable()`, `deleteStatistic()`, `deleteStatisticsForTable()`
- MCVs serialized to JSON format and stored via TOAST (using `storeStringInToast()`)
- Histograms serialized to JSON format and stored via TOAST
- Statistics cached in-memory for fast access
- Basic stats stored inline: num_rows, num_nulls, null_fraction, num_distinct, avg_width

**Verification:**
- [X] ANALYZE stores stats to disk
- [X] Stats survive restart (via catalog persistence)
- [X] Query planner uses persisted stats

---

### OPT-2: loadColumnStatistics (HIGH)
**File:** src/optimizer/statistics_manager.cpp
**Lines:** 1510-1696
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Full statistics loading from pg_statistic catalog:
- Loads `StatisticInfo` via `CatalogManager::getStatistic()`
- MCVs loaded from TOAST and parsed from JSON format
- Histograms loaded from TOAST and parsed from JSON format
- Reconstructs full `ColumnStatistics` struct from catalog data
- Results cached for subsequent queries

**Verification:**
- [X] After restart, stats are loaded
- [X] Query plans use loaded stats

---

### OPT-3: suggestIndexesForQuery (HIGH)
**File:** src/optimizer/index_advisor.cpp
**Lines:** 406-678
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Parses SQL queries and analyzes predicates to generate index recommendations:
- Uses Parser to parse SELECT statements
- Extracts equality predicates (col = value) - highest priority
- Extracts range predicates (col < value, col > value, etc.) - medium priority
- Extracts join condition columns
- Checks existing indexes via CatalogManager
- Generates recommendations with:
  - CREATE INDEX SQL statements
  - Benefit/cost scores
  - Priority and confidence values
  - Estimated speedup factors
- Also suggests composite indexes for multi-column equality predicates

**Verification:**
- [X] Recommends index for unindexed equality predicate
- [X] Recommends composite index for multi-column predicate

---

### OPT-4: findCandidates (MV rewriter) (HIGH)
**File:** src/optimizer/mv_rewriter.cpp
**Lines:** 149-289
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Implemented full MV candidate search using new catalog APIs:
- Added `getAllMaterializedViews()` to CatalogManager
- Uses `extractPatternFromDefinition()` to parse MV SQL definitions
- Compares query pattern against MV patterns using `checkSubsumption()`
- Calculates staleness from `last_refresh_time` (OPT-M9)
- Estimates cost using statistics when available (OPT-M10)
- Also searches pattern cache for pre-registered MVs
- Sorts candidates by estimated cost (cheapest first)

**Verification:**
- [X] Query on table with MV finds MV candidate
- [X] Query with incompatible predicates doesn't find MV

---

### OPT-5: MV info retrieval (HIGH)
**File:** src/optimizer/mv_rewriter.cpp
**Lines:** 228-253
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Added `getViewById()` to CatalogManager for direct view lookup by ID:
- Returns ViewInfo directly from view_cache_ by ID
- Used in findCandidates() for pattern cache MVs
- Allows retrieval of full MV metadata including:
  - View name and definition
  - Materialized flag and backing table ID
  - Last refresh time for staleness calculation

**Verification:**
- [X] MV candidates have complete metadata

---

### OPT-M1: num_pages statistics (MEDIUM)
**File:** src/optimizer/statistics_manager.cpp
**Line:** 368-448
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Estimates num_pages from row count and average row size:
- Gets column info to estimate row size
- Uses PAGE_SIZE (8KB), TUPLE_HEADER_SIZE (24 bytes), PAGE_FILL_FACTOR (0.8)
- Calculates: num_pages = ceil(num_rows / rows_per_page)

**Verification:**
- [X] Table statistics include accurate page count

---

### OPT-M2: Column type support (MEDIUM)
**File:** src/optimizer/statistics_manager.cpp
**Line:** 769+
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Added statistics extraction for additional types:
- BOOL, INT8, INT16, UINT types
- TIMESTAMP, DATE, TIME, INTERVAL
- UUID
- DECIMAL, MONEY
- BYTEA, BLOB
- Spatial types (POINT, LINE, POLYGON, etc.)
- Network types (INET, MACADDR)
- JSON/JSONB

**Verification:**
- [X] ANALYZE works for all column types

---

### OPT-M3: isIndexApplicable (MEDIUM)
**File:** src/optimizer/query_planner.cpp
**Lines:** 655-678
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Checks if predicate columns match index columns and operator compatibility:
- Extracts column name from predicate
- Compares against index column names
- Verifies operator is compatible with index type

**Verification:**
- [X] Only applicable indexes considered

---

### OPT-M4: isSpatialPredicate (MEDIUM)
**File:** src/optimizer/query_planner.cpp
**Lines:** 680-717
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Resolves actual column and function names from spatial predicates:
- Extracts column name from spatial function argument
- Identifies spatial function name from AST
- Returns correct identifiers instead of placeholders

**Verification:**
- [X] Spatial predicates identified correctly

---

### OPT-M5: calculateQualCost (MEDIUM)
**File:** src/optimizer/query_planner.cpp
**Lines:** 1305-1450+
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Added calculateExpressionCost() that traverses expression tree:
- Handles BINARY_OP with type-specific costs (EQ, NE, LT, LE, GT, GE, AND, OR, LIKE)
- Handles FUNCTION_CALL with base cost plus argument costs
- Handles CASE expressions with per-branch costs
- Handles COALESCE, NULLIF, IN_LIST, BETWEEN, etc.
- Uses cost_model_.operatorCost() for accurate per-operator costs

**Verification:**
- [X] Complex WHERE clauses have higher cost

---

### OPT-M6: extractHashKeys (MEDIUM)
**File:** src/optimizer/query_planner.cpp
**Lines:** 1785-1860
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Verifies columns belong to correct tables:
- Uses qualifier_ from IdentifierExpr to get table name
- Handles all 4 cases: both qualified, left only, right only, neither
- Passes table names from generateJoinPaths via getTableName helper
- Correctly assigns left/right keys based on table membership

**Verification:**
- [X] Hash join uses correct columns

---

### OPT-M7: LSM merge cost (MEDIUM)
**File:** src/optimizer/cost_model.cpp
**Lines:** 157-172
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Estimates K-way merge cost for LSM-tree range scans:
```cpp
double log_k = std::log2(static_cast<double>(total_sstables));
merge_cost = index_tuples * log_k * params_.cpu_operator_cost;
```
- Only applies when index_tuples > 1 and total_sstables > 1
- Models heap data structure for k-way merge

**Verification:**
- [X] LSM range scan cost reflects merge overhead

---

### OPT-M8: estimateIndexSize (MEDIUM)
**File:** src/optimizer/index_advisor.cpp
**Lines:** 736-859
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Estimates index size from row count and column sizes:
- Falls back to catalog if statistics unavailable
- Gets row_count from TableInfo if stats manager fails
- Uses estimateColumnWidthFromType() helper with type-specific widths
- Accounts for B-tree overhead (16 bytes per entry)
- Returns size in MB

**Verification:**
- [X] Index size estimate is reasonable

---

### OPT-M9: MV staleness calculation (MEDIUM)
**File:** src/optimizer/mv_rewriter.cpp
**Lines:** 186-194, 239-247
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Full staleness calculation from ViewInfo.last_refresh_time:
- Gets current timestamp via std::chrono::system_clock
- Calculates staleness_seconds = now_timestamp - mv_info.last_refresh_time
- For never-refreshed MVs (last_refresh_time = 0), treats as current (staleness = 0)
- For pattern cache MVs, uses getViewById() to retrieve ViewInfo with refresh time
- Staleness compared against max_staleness_seconds_ threshold in tryRewrite()

**Verification:**
- [X] Old MVs have higher staleness

---

### OPT-M10: MV cost estimation (MEDIUM)
**File:** src/optimizer/mv_rewriter.cpp
**Lines:** 196-210, 255-269
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Cost estimation using statistics when available:
- Uses estimateOriginalCost() for base table costs
- Applies cost factor based on MV type:
  - Aggregate MVs: 0.15x (aggregation pre-computed)
  - Non-aggregate MVs: 0.40x (smaller subset of data)
- Minimum cost floor of 10.0
- Falls back to 50.0 when stats unavailable

**Verification:**
- [X] MV cost reflects actual size

---

### OPT-L1: invalidateCache (LOW)
**File:** src/optimizer/statistics_manager.cpp
**Lines:** 506-552
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Targeted invalidation for specific tables:
- Accepts table_id parameter
- If table_id is zero, clears entire cache
- Otherwise removes only entries for that table
- Removes both table-level and column-level statistics

**Verification:**
- [X] Other table stats preserved on invalidation

---

### OPT-L2: Filter placeholders (LOW)
**File:** src/optimizer/query_planner.cpp
**Lines:** 899-1010
**Status:** [X] COMPLETE

**Implementation:** (December 2025)
Created expression-to-string infrastructure for EXPLAIN output:
- Added `include/scratchbird/parser/expression_to_string.h` header
- Added `src/parser/expression_to_string.cpp` implementation
- Implemented `expressionToString()` for all expression types:
  - LiteralExpr (INTEGER, FLOAT, STRING, NULL, RANGE)
  - IdentifierExpr (qualified and unqualified)
  - BinaryOpExpr (arithmetic, comparison, logical, array, regex operators)
  - CastExpr (CAST and TRY_CAST)
  - FunctionCallExpr, SequenceFunctionExpr, ExtractExpr
  - AggregateExpr, CoalesceExpr, NullIfExpr
  - CaseExpr (simple and searched)
  - SubqueryExpr (placeholder), GroupingExpr
- Added `binaryOpToString()` and `aggregateFuncToString()` helpers
- Added `dataTypeToSqlString()` for CAST type names
- Updated CMakeLists.txt to link scratchbird_optimizer with scratchbird_parser

**Verification:**
- [X] EXPLAIN shows actual filter expression

---

## Dependencies

- ~~OPT-1, OPT-2 need catalog table for pg_statistic~~ (RESOLVED - pg_statistic implemented)
- OPT-3 needs parser access
- OPT-4, OPT-5 need MV catalog integration

---

## Testing Plan

1. ANALYZE and restart test
2. Query plan comparison with/without stats
3. Index advisor recommendation tests
4. MV rewriting tests
5. Cost model accuracy tests

---

## Completion Summary

| Task | Priority | Status |
|------|----------|--------|
| OPT-1 | HIGH | COMPLETE |
| OPT-2 | HIGH | COMPLETE |
| OPT-3 | HIGH | COMPLETE |
| OPT-4 | HIGH | COMPLETE |
| OPT-5 | HIGH | COMPLETE |
| OPT-M1 | MEDIUM | COMPLETE |
| OPT-M2 | MEDIUM | COMPLETE |
| OPT-M3 | MEDIUM | COMPLETE |
| OPT-M4 | MEDIUM | COMPLETE |
| OPT-M5 | MEDIUM | COMPLETE |
| OPT-M6 | MEDIUM | COMPLETE |
| OPT-M7 | MEDIUM | COMPLETE |
| OPT-M8 | MEDIUM | COMPLETE |
| OPT-M9 | MEDIUM | COMPLETE |
| OPT-M10 | MEDIUM | COMPLETE |
| OPT-L1 | LOW | COMPLETE |
| OPT-L2 | LOW | COMPLETE |

**Complete:** 17/17 (100%)

---

**Last Updated:** December 4, 2025


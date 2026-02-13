# Query Optimization Completeness Audit (Alpha Priority 6)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

**Date**: October 25, 2025
**Auditor**: Claude Code
**Scope**: Verify query optimization infrastructure implementation vs. specification

---

## Executive Summary

**Status**: ❌ **NOT IMPLEMENTED (0% complete)**

ScratchBird has a comprehensive Query Optimizer Specification (`/docs/specifications/parser/v3/QUERY_OPTIMIZER_SPEC.md`) totaling **~1,249 lines** of detailed design, but **zero implementation code** exists in the codebase. This represents a critical gap between planning and implementation.

### Key Findings:
- **Specification Found**: 1,249 lines of comprehensive query optimizer design
- **Implementation Found**: 0 lines (no query planner, cost model, statistics, or plan cache)
- **Coverage**: 0% implemented vs. specification
- **Alpha Priority**: 6 (Query Optimization)
- **Impact**: Without query optimization, all queries execute without cost analysis or plan selection

### What's Specified (But Not Implemented):
1. **Statistics System**: Table/column statistics, histograms, ANALYZE command
2. **Cost Model**: Sequential scan costs, index scan costs, join costs
3. **Query Planner**: Plan generation, join ordering, path selection
4. **Selectivity Estimation**: Equality, range, LIKE, IN clause estimation
5. **Adaptive Query Execution**: Runtime statistics, plan adaptation
6. **Plan Caching**: Cached plans with BLR compilation
7. **Parallel Query Execution**: Parallel workers, gather nodes
8. **Transformation Rules**: Predicate pushdown, join reordering, subquery unnesting
9. **EXPLAIN Output**: Text/JSON/XML/YAML explain formats

### Current State:
- Parser: ✅ Exists (~6,083 lines) - can parse SQL statements
- SBLR Executor: ✅ Exists (~4,458 lines) - can execute bytecode
- Query Optimizer: ❌ Missing entirely (0 lines)
- **Gap**: Parser generates AST → ??? → SBLR bytecode (no optimizer in between)

---

## 1. Specification Analysis

### 1.1 QUERY_OPTIMIZER_SPEC.md Overview

**File**: `/docs/specifications/parser/v3/QUERY_OPTIMIZER_SPEC.md`
**Lines**: 1,249 lines
**Last Modified**: Unknown (planning document)

**Specification Structure**:
```
Section 1: Statistics System (lines 8-257)
  - Table and column statistics structures
  - Histogram implementation (N-dimensional)
  - ANALYZE command for statistics collection
  - Reservoir sampling (Vitter's Algorithm S)

Section 2: Cost Model (lines 259-413)
  - Cost configuration (I/O, CPU, parallel, network, memory)
  - Sequential scan cost estimation
  - Index scan cost estimation
  - Cache effect modeling

Section 3: Query Planning (lines 415-647)
  - Query plan node structure (15+ node types)
  - Main query planner (7-step planning process)
  - Join ordering (dynamic programming + genetic algorithm)
  - Join path generation (nested loop, hash, merge)

Section 4: Selectivity Estimation (lines 649-753)
  - Equality selectivity (MCV lists)
  - Range selectivity (histogram-based)
  - LIKE, IN, AND, OR, NOT selectivity

Section 5: Adaptive Query Execution (lines 755-887)
  - Runtime statistics collection
  - Plan adaptation during execution
  - Mid-execution replanning

Section 6: Query Plan Caching (lines 889-982)
  - Cached plan structure
  - LRU eviction policy
  - Plan invalidation on statistics changes

Section 7: Parallel Query Execution (lines 984-1061)
  - Parallel context and workers
  - Parallel degree determination
  - Gather nodes

Section 8: Transformation Rules (lines 1063-1142)
  - Predicate pushdown
  - Join reordering
  - Subquery unnesting
  - Index usage
  - Materialization

Section 9: EXPLAIN Output (lines 1144-1238)
  - Text, JSON, XML, YAML formats
  - ANALYZE mode with runtime statistics
  - Verbose output options

Implementation Timeline (lines 1240-1249)
  - Phase 13: Basic cost-based optimizer
  - Enhancements: Multi-column statistics, adaptive execution
  - Future: Machine learning cost model
```

### 1.2 Specification Highlights

**Key Structures Defined** (but not implemented):
```c
// From QUERY_OPTIMIZER_SPEC.md:14-44
typedef struct sb_statistics {
    UUID            stat_object_uuid;
    ObjectType      stat_object_type;
    uint64_t        n_tuples;
    uint64_t        n_dead_tuples;
    uint64_t        n_pages;
    ColumnStats**   column_stats;
    MultiColumnStats** multi_stats;
    QueryFeedback*  feedback_stats;
} SBStatistics;

// From QUERY_OPTIMIZER_SPEC.md:264-293
typedef struct sb_cost_config {
    double seq_page_cost;         // 1.0
    double random_page_cost;      // 4.0 (HDD) / 1.1 (SSD)
    double cpu_tuple_cost;        // 0.01
    double cpu_index_tuple_cost;  // 0.005
    double parallel_setup_cost;   // 1000.0
    // ... 10+ more cost parameters
} SBCostConfig;

// From QUERY_OPTIMIZER_SPEC.md:420-444
typedef struct plan_node {
    NodeType        type;
    PlanCost        cost;
    List*           targetlist;
    List*           quals;
    struct plan_node* left_child;
    struct plan_node* right_child;
    uint16_t        parallel_workers;
    void*           node_data;
} PlanNode;

// 15+ node types defined (lines 447-474):
// T_SeqScan, T_IndexScan, T_IndexOnlyScan, T_BitmapIndexScan,
// T_BitmapHeapScan, T_TidScan, T_NestLoop, T_HashJoin,
// T_MergeJoin, T_Hash, T_Sort, T_Aggregate, T_WindowAgg,
// T_Material, T_Unique, T_Limit, T_Append, etc.
```

**Cost Estimation Algorithm** (lines 336-373):
- Sequential scan: I/O cost + CPU cost with cache discount
- Index scan: Index I/O + table I/O (random access) + CPU cost
- Correlation adjustment for physical ordering

**Join Ordering** (lines 551-610):
- Small joins (<= threshold): Dynamic programming (exact)
- Large joins (> threshold): Genetic algorithm (heuristic)
- Level-by-level join construction

**Adaptive Execution** (lines 802-834):
- Error threshold checking (actual vs. estimated rows)
- Mid-execution replanning if estimates are off
- Memory pressure detection

---

## 2. Implementation Search Results

### 2.1 Code Search Strategy

Searched for query optimization evidence using multiple patterns:

**Search 1**: Query planner structures
```bash
grep -r "(PlanNode|QueryPlanner|cost_seqscan|cost_indexscan)" src/ include/
```
**Result**: 0 matches (only found "adaptive" in buffer_pool.cpp for adaptive flushing, unrelated)

**Search 2**: Statistics and selectivity
```bash
grep -ri "(statistics|analyze|histogram|selectivity)" src/core/
```
**Result**: 19 files matched, but all references were to:
- Index statistics (index page counts, entry counts) - not query optimizer statistics
- ANALYZE comments in index code (not ANALYZE command implementation)
- No histogram structures, no column statistics, no selectivity estimation

**Search 3**: Plan caching
```bash
grep -r "(CachedPlan|plan_cache|PlanCache)" src/ include/
```
**Result**: 0 matches

**Search 4**: File name search
```bash
find . -name "*query*" -o -name "*plan*" -o -name "*optim*"
```
**Result**: Only found planning **documentation** directories, no implementation files

### 2.2 What Was Found (False Positives)

**File**: `/src/core/buffer_pool.cpp:813-828`
```cpp
// Algorithm based on PostgreSQL's bgwriter and MySQL InnoDB's adaptive flushing
// Spec: /docs/specifications/parser/v3/STORAGE_ENGINE_BUFFER_POOL.md (background writer)
```
**Context**: This is "adaptive flushing" for the buffer pool background writer, not query optimization.

**File**: `/src/core/garbage_collector.cpp:27, 323, 610, 623-632`
```cpp
adaptive_tuning_enabled_(true)  // Adaptive GC tuning
void GarbageCollector::performAdaptiveTuning()
void GarbageCollector::setAdaptiveTuning(bool enabled)
```
**Context**: This is adaptive garbage collection tuning (adjusting GC intervals), not adaptive query execution.

**File**: Index implementation files (btree.cpp, gin_index.cpp, etc.)
```cpp
// Various references to "index statistics" like:
uint64_t num_entries;     // Number of index entries
uint64_t leaf_pages;      // Number of leaf pages
double correlation;       // Physical order correlation
```
**Context**: These are index-level statistics used **within** the index implementation for maintenance (e.g., choosing split points), not query optimizer statistics.

### 2.3 Confirmed Gap: Parser → SBLR Without Optimizer

**What Exists**:
1. **Parser** (`src/parser/parser.cpp`: 1,921 lines) - parses SQL → AST
2. **SBLR Bytecode Generator** (`src/sblr/bytecode_generator.cpp`: ~1,372 lines) - AST → SBLR bytecode
3. **SBLR Executor** (`src/sblr/executor.cpp`: 2,898 lines) - executes SBLR bytecode

**What's Missing**:
- No query planner between parser and bytecode generator
- No cost estimation
- No plan alternatives (always uses first plan generated)
- No statistics collection (ANALYZE command missing)
- No plan caching

**Current Flow** (simplified):
```
SQL Text → Parser → AST → Bytecode Generator → SBLR → Executor
                           ^
                           |
                    NO OPTIMIZER HERE
```

**Should Be** (per spec):
```
SQL Text → Parser → AST → Query Planner → Best Plan → Bytecode Generator → SBLR → Executor
                                 ↓
                           Cost Estimation
                           Join Ordering
                           Path Selection
                           Statistics Lookup
                           Plan Caching
```

---

## 3. Comparison with Target Databases

### 3.1 Query Optimization Features Comparison

| Feature | Firebird | MySQL | PostgreSQL | SQL Server | ScratchBird |
|---------|----------|-------|------------|------------|-------------|
| **Cost-Based Optimizer** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes | ❌ No |
| **Table Statistics** | ✅ SET STATISTICS | ✅ ANALYZE TABLE | ✅ ANALYZE | ✅ UPDATE STATISTICS | ❌ No |
| **Column Histograms** | ❌ No | ✅ Yes (8.0+) | ✅ Yes | ✅ Yes | ❌ No |
| **Join Ordering** | ✅ Yes (limited) | ✅ Yes | ✅ Yes (DP) | ✅ Yes | ❌ No |
| **Index Selection** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes | ❌ No |
| **Plan Caching** | ✅ Yes | ✅ Yes | ✅ Yes (prepared) | ✅ Yes | ❌ No |
| **Parallel Query** | ❌ No | ✅ Yes (8.0+) | ✅ Yes (9.6+) | ✅ Yes | ❌ No |
| **Adaptive Execution** | ❌ No | ❌ No | ❌ No | ✅ Yes (2017+) | ❌ No |
| **EXPLAIN Command** | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes | ❌ No |
| **Selectivity Estimation** | ✅ Basic | ✅ Yes | ✅ Advanced | ✅ Advanced | ❌ No |

**Summary**:
- **Firebird**: 6/10 features (no histograms, no parallel, no adaptive, but has core optimizer)
- **MySQL**: 8/10 features (missing adaptive execution)
- **PostgreSQL**: 8/10 features (missing adaptive execution)
- **SQL Server**: 10/10 features (most advanced)
- **ScratchBird**: **0/10 features** (specification only, no implementation)

### 3.2 What Should Exist for "Alpha Complete"

Based on the specification and comparison, **minimal viable optimizer** would include:

**Phase 1: Core Optimizer (Essential - ~3,000-5,000 lines)**:
1. ✅ **Specification**: 1,249 lines (DONE)
2. ❌ **Statistics Collection**: ANALYZE command, table/column stats (~500-800 lines)
3. ❌ **Cost Model**: Sequential scan, index scan cost estimation (~400-600 lines)
4. ❌ **Plan Generation**: Basic planner, path creation (~800-1,200 lines)
5. ❌ **Selectivity Estimation**: Equality, range selectivity (~300-500 lines)
6. ❌ **EXPLAIN Command**: Basic text output (~200-400 lines)

**Phase 2: Advanced Features (Nice-to-Have - ~5,000-8,000 lines)**:
7. ❌ **Join Ordering**: Dynamic programming join search (~800-1,200 lines)
8. ❌ **Plan Caching**: LRU plan cache with BLR (~600-1,000 lines)
9. ❌ **Transformation Rules**: Predicate pushdown, join reordering (~1,000-1,500 lines)
10. ❌ **Parallel Planning**: Parallel degree determination (~500-800 lines)

**Phase 3: Advanced Optimizations (Future)**:
11. ❌ **Adaptive Execution**: Runtime statistics, mid-execution replanning
12. ❌ **Multi-column Statistics**: N-dimensional histograms
13. ❌ **Genetic Algorithm**: For large join searches

---

## 4. Assessment

### 4.1 Implementation Status

**Overall Status**: ❌ **NOT IMPLEMENTED (0%)**

| Component | Specified | Implemented | Status |
|-----------|-----------|-------------|--------|
| Statistics System | ✅ Yes (257 lines) | ❌ No (0 lines) | ❌ 0% |
| Cost Model | ✅ Yes (155 lines) | ❌ No (0 lines) | ❌ 0% |
| Query Planner | ✅ Yes (233 lines) | ❌ No (0 lines) | ❌ 0% |
| Selectivity Estimation | ✅ Yes (105 lines) | ❌ No (0 lines) | ❌ 0% |
| Adaptive Execution | ✅ Yes (133 lines) | ❌ No (0 lines) | ❌ 0% |
| Plan Caching | ✅ Yes (94 lines) | ❌ No (0 lines) | ❌ 0% |
| Parallel Execution | ✅ Yes (78 lines) | ❌ No (0 lines) | ❌ 0% |
| Transformation Rules | ✅ Yes (80 lines) | ❌ No (0 lines) | ❌ 0% |
| EXPLAIN Output | ✅ Yes (95 lines) | ❌ No (0 lines) | ❌ 0% |
| **TOTAL** | **1,249 lines** | **0 lines** | **0%** |

### 4.2 Impact Analysis

**Without Query Optimization**:

1. **All Queries Use First Plan Generated**:
   - Parser generates AST
   - Bytecode generator directly converts AST → SBLR
   - No alternative plans considered
   - No cost comparison

2. **No Index Selection**:
   - Even if indexes exist (B-Tree, GIN, Hash, etc. are implemented)
   - Optimizer cannot choose which index to use
   - Likely defaults to sequential scan always

3. **No Join Ordering**:
   - Multi-table joins execute in parse order
   - May result in Cartesian products or inefficient join sequences
   - No consideration of table sizes

4. **No Statistics**:
   - Cannot estimate selectivity (how many rows match WHERE clause)
   - Cannot estimate join cardinality
   - Cannot make informed decisions

5. **No EXPLAIN**:
   - Users cannot see query execution plan
   - Debugging slow queries is impossible

**Example Impact**:
```sql
-- Query: Find users in large table who bought specific product
SELECT u.name
FROM users u
JOIN orders o ON u.id = o.user_id
WHERE o.product_id = 12345;

-- Optimal plan (with optimizer):
-- 1. Index scan on orders(product_id) → 10 rows
-- 2. Nested loop join with users → 10 lookups
-- Cost: ~50 units

-- Actual plan (without optimizer):
-- 1. Sequential scan users → 1,000,000 rows
-- 2. Sequential scan orders → 10,000,000 rows
-- 3. Nested loop join → 1,000,000 × 10,000,000 comparisons
-- Cost: ~10,000,000,000 units (200,000× slower!)
```

### 4.3 Gap Analysis

**Specification Completeness**: ⭐⭐⭐⭐⭐ **Excellent** (5/5 stars)
- Comprehensive 1,249-line specification
- Covers all modern optimizer features
- Includes PostgreSQL, MySQL, SQL Server, Firebird best practices
- Code examples provided
- Clear implementation timeline

**Implementation Completeness**: ⭐☆☆☆☆ **Non-existent** (0/5 stars)
- Zero lines of optimizer code
- No statistics collection
- No cost estimation
- No plan generation
- No EXPLAIN command

**Gap Severity**: 🔴 **CRITICAL**
- Query optimization is fundamental to database performance
- Without it, ScratchBird cannot be used for production workloads
- Specification exists but is completely unimplemented
- Represents ~5,000-10,000 lines of missing code

---

## 5. Code References

### 5.1 Specification Location

**File**: `/docs/specifications/parser/v3/QUERY_OPTIMIZER_SPEC.md`
**Lines**: 1-1,249 (entire file)
**Evidence**: Comprehensive specification with 9 major sections

### 5.2 Implementation Locations (NONE FOUND)

**Expected Locations** (based on project structure):
```
src/query/             ← Directory does not exist
src/query/planner.cpp  ← File does not exist (0 lines)
src/query/cost.cpp     ← File does not exist (0 lines)
src/query/stats.cpp    ← File does not exist (0 lines)

include/scratchbird/query/        ← Directory does not exist
include/scratchbird/query/planner.h ← File does not exist (0 lines)
include/scratchbird/query/cost.h   ← File does not exist (0 lines)
include/scratchbird/query/stats.h  ← File does not exist (0 lines)
```

**Actual Search Results**:
```bash
$ find . -path "*/query/*" -name "*.cpp" -o -name "*.h"
(no results)

$ grep -r "QueryPlanner\|PlanNode\|cost_seqscan" src/ include/
(no results)

$ wc -l /docs/specifications/parser/v3/QUERY_OPTIMIZER_SPEC.md
1249 /docs/specifications/parser/v3/QUERY_OPTIMIZER_SPEC.md
```

### 5.3 Related Existing Code (Context Only)

**Parser** (produces AST that optimizer should consume):
- `/src/parser/parser.cpp:1-1921` - SQL parser
- `/include/scratchbird/parser/ast.h:29-46` - AST node definitions

**SBLR** (consumes optimized plan):
- `/src/sblr/bytecode_generator.cpp` - Converts AST/plan → SBLR bytecode
- `/src/sblr/executor.cpp:1-2898` - Executes SBLR bytecode

**Indexes** (should be selected by optimizer):
- `/src/core/btree.cpp` - B-Tree index (~4,000 lines)
- `/src/core/gin_index.cpp` - GIN index (~3,935 lines)
- `/src/core/hash_index.cpp` - Hash index (~1,451 lines)
- etc. (6 index types total)

---

## 6. Recommendations

### 6.1 Immediate Actions (Phase 1: Core Optimizer)

**Priority**: 🔴 **CRITICAL**
**Estimated Effort**: ~60-100 developer hours
**Estimated Lines of Code**: ~3,000-5,000 lines

**Task 1: Statistics Infrastructure** (~15-25 hours, ~500-800 lines)
1. Create `src/query/` directory
2. Implement `SBStatistics` structure (from spec lines 14-44)
3. Implement `ColumnStats` structure (from spec lines 46-77)
4. Implement basic ANALYZE command (from spec lines 176-216)
5. Store statistics in catalog (extend `catalog_manager.cpp`)

**Task 2: Cost Model** (~12-20 hours, ~400-600 lines)
1. Implement `SBCostConfig` structure (from spec lines 264-293)
2. Implement `cost_seqscan()` (from spec lines 336-373)
3. Implement `cost_indexscan()` (from spec lines 375-412)
4. Add cost configuration loading

**Task 3: Basic Planner** (~20-30 hours, ~800-1,200 lines)
1. Implement `PlanNode` structure (from spec lines 420-444)
2. Implement node types enum (from spec lines 447-474)
3. Implement basic planner skeleton (from spec lines 499-545)
4. Generate sequential scan paths
5. Generate index scan paths
6. Select cheapest path

**Task 4: Selectivity Estimation** (~8-15 hours, ~300-500 lines)
1. Implement `estimate_qual_selectivity()` (from spec lines 654-684)
2. Implement `estimate_equality_selectivity()` (from spec lines 686-717)
3. Implement `estimate_range_selectivity()` (from spec lines 719-753)
4. Add default selectivity constants

**Task 5: EXPLAIN Command** (~5-10 hours, ~200-400 lines)
1. Implement EXPLAIN parser support
2. Implement `explain_plan()` text format (from spec lines 1168-1238)
3. Add to SQL parser grammar

**Integration**:
- Modify `bytecode_generator.cpp` to accept `PlanNode*` instead of `AST*`
- Insert planner call between parser and bytecode generator

### 6.2 Medium-Term Actions (Phase 2: Advanced Features)

**Estimated Effort**: ~40-60 hours
**Estimated Lines of Code**: ~2,500-4,000 lines

**Task 6: Join Ordering** (~15-25 hours, ~800-1,200 lines)
- Implement dynamic programming join search (from spec lines 568-610)
- Implement join path generation (from spec lines 612-647)

**Task 7: Plan Caching** (~10-15 hours, ~600-1,000 lines)
- Implement `CachedPlan` structure (from spec lines 894-921)
- Implement `PlanCache` with LRU eviction (from spec lines 923-981)

**Task 8: Transformation Rules** (~15-20 hours, ~1,000-1,500 lines)
- Implement predicate pushdown (from spec lines 1078-1084)
- Implement join reordering (from spec lines 1086-1092)
- Implement transformation framework (from spec lines 1120-1141)

### 6.3 Long-Term Actions (Phase 3: Advanced Optimizations)

**Task 9: Adaptive Execution** (~20-30 hours)
- Implement runtime statistics collection (from spec lines 760-780)
- Implement mid-execution replanning (from spec lines 802-887)

**Task 10: Multi-column Statistics** (~15-25 hours)
- Implement N-dimensional histograms (from spec lines 100-171)
- Implement multi-column stats (from spec lines 79-97)

**Task 11: Parallel Query** (~25-40 hours)
- Implement parallel planning (from spec lines 1009-1034)
- Implement worker coordination (from spec lines 989-1007)

### 6.4 Testing Strategy

**Unit Tests Required**:
1. Statistics collection accuracy
2. Cost estimation correctness (compare to actual runtime)
3. Selectivity estimation accuracy
4. Join ordering optimality (small test cases)
5. Plan caching correctness (invalidation on schema changes)

**Integration Tests Required**:
1. End-to-end query optimization (SQL → optimized plan → execution)
2. EXPLAIN output correctness
3. Index selection verification
4. Join ordering for complex queries

**Performance Tests Required**:
1. Optimizer overhead (planning time vs. execution time)
2. Plan cache hit rate
3. Cost estimation accuracy (predicted vs. actual)

---

## 7. Conclusion

### 7.1 Summary

Query optimization is **completely unimplemented** in ScratchBird despite having an excellent 1,249-line specification. This represents:

- **0% implementation** of Alpha Priority 6
- **~5,000-10,000 lines** of missing code
- **~100-160 developer hours** of work remaining
- **Critical performance impact**: Queries execute without cost analysis or plan selection

### 7.2 Comparison to Other Priorities

| Priority | Component | Status | Lines |
|----------|-----------|--------|-------|
| 1 | Type System | ✅ 90-95% | ~3,407 |
| 2 | Index Types | ✅ 95-100% | ~11,376 |
| 3 | Functions | ⚠️ 25-30% | ~4,458 |
| 4 | Schema Structure | ✅ 100% | ~6,432 |
| 6 | **Query Optimization** | ❌ **0%** | **~0** |
| 7 | Parser | ⚠️ 64% | ~6,083 |

**Query Optimization is the LEAST complete Alpha priority component** at 0% implementation.

### 7.3 Final Assessment

**Specification Quality**: ⭐⭐⭐⭐⭐ (5/5) - Excellent, comprehensive, well-researched
**Implementation Status**: ❌ (0/5) - Non-existent
**Alpha Readiness**: 🔴 **NOT READY** - Critical gap blocking production use

**Recommendation**: **Implement Phase 1 (Core Optimizer) before Alpha release**. Without basic cost-based optimization, ScratchBird cannot perform reasonable query execution for anything beyond trivial single-table queries.

---

**End of Query Optimization Audit**

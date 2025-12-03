# Work Package 10: Optimizer

**Status:** NOT STARTED
**Priority:** P1-P3 Mixed
**Estimated Hours:** 28-36
**Files:** src/optimizer/*.cpp

---

## Overview

The query optimizer has several incomplete features including statistics persistence, index advisor, materialized view rewriting, and cost estimation gaps.

---

## Tasks

### OPT-1: storeColumnStatistics persistence (HIGH)
**File:** src/optimizer/statistics_manager.cpp
**Lines:** 1160-1187
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Only stores in in-memory cache
// Phase 4 Enhancement: Persist to pg_statistic catalog
```

**Required Changes:**
1. Create/use pg_statistic catalog table
2. Serialize statistics (histogram, MCVs, null_frac, etc.)
3. Write to catalog
4. Update on subsequent ANALYZE

**Catalog Structure:**
```cpp
struct PgStatisticRecord {
    ID table_id;
    uint32_t column_num;
    float null_frac;
    uint32_t n_distinct;
    std::vector<TypedValue> most_common_vals;
    std::vector<float> most_common_freqs;
    std::vector<float> histogram_bounds;
    // etc.
};
```

**Verification:**
- [ ] ANALYZE stores stats to disk
- [ ] Stats survive restart
- [ ] Query planner uses persisted stats

---

### OPT-2: loadColumnStatistics (HIGH)
**File:** src/optimizer/statistics_manager.cpp
**Lines:** 1191-1215
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
return Status::NOT_FOUND;  // Always fails
```

**Required Changes:**
1. Query pg_statistic catalog
2. Deserialize statistics
3. Populate stats structure
4. Cache for performance

**Verification:**
- [ ] After restart, stats are loaded
- [ ] Query plans use loaded stats

---

### OPT-3: suggestIndexesForQuery (HIGH)
**File:** src/optimizer/index_advisor.cpp
**Lines:** 406-421
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// This would require query parsing - placeholder for now
return {};  // Returns empty list
```

**Required Changes:**
1. Parse SQL query
2. Extract predicates and join conditions
3. Identify beneficial indexes
4. Return recommendations

**Algorithm:**
```cpp
std::vector<IndexRecommendation> suggestIndexesForQuery(const std::string& sql) {
    auto ast = parser_.parse(sql);
    auto predicates = extractPredicates(ast);
    auto joins = extractJoinConditions(ast);

    std::vector<IndexRecommendation> recs;
    for (auto& pred : predicates) {
        if (pred.isEquality() && !hasIndex(pred.column())) {
            recs.push_back({pred.column(), IndexType::BTREE});
        }
    }
    // etc.
    return recs;
}
```

**Verification:**
- [ ] Recommends index for unindexed equality predicate
- [ ] Recommends composite index for multi-column predicate

---

### OPT-4: findCandidates (MV rewriter) (HIGH)
**File:** src/optimizer/mv_rewriter.cpp
**Lines:** 149-236
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Has extensive comments about what "would go here"
// (void)table_info;  // Not used
```

**Required Changes:**
1. Build MV registry indexed by base tables
2. For each table in query, look up MVs
3. Check if MV can satisfy query
4. Return candidate MVs

**Verification:**
- [ ] Query on table with MV finds MV candidate
- [ ] Query with incompatible predicates doesn't find MV

---

### OPT-5: MV info retrieval (HIGH)
**File:** src/optimizer/mv_rewriter.cpp
**Lines:** 202-206
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Cannot retrieve view info by ID
// For now skip if we can't retrieve
```

**Required Changes:**
1. Use CatalogManager::getView() or equivalent
2. Retrieve full MV metadata

**Verification:**
- [ ] MV candidates have complete metadata

---

### OPT-M1: num_pages statistics (MEDIUM)
**File:** src/optimizer/statistics_manager.cpp
**Line:** 368
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
stats.num_pages = 0;  // Phase 4 Enhancement
```

**Required Changes:**
1. Query table metadata for page count
2. Or calculate from file size / page size

**Verification:**
- [ ] Table statistics include accurate page count

---

### OPT-M2: Column type support (MEDIUM)
**File:** src/optimizer/statistics_manager.cpp
**Line:** 769
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Phase 4 Enhancement: Add support for other types
// Only handles INT32, INT64, FLOAT64, VARCHAR
```

**Required Changes:**
Add statistics extraction for:
- BOOL
- TIMESTAMP, DATE, TIME
- UUID
- DECIMAL
- BYTEA

**Verification:**
- [ ] ANALYZE works for all column types

---

### OPT-M3: isIndexApplicable (MEDIUM)
**File:** src/optimizer/query_planner.cpp
**Lines:** 655-678
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
return true;  // Conservative assumption
```

**Required Changes:**
1. Check if predicate columns match index columns
2. Check operator compatibility
3. Return true only if index actually helps

**Verification:**
- [ ] Only applicable indexes considered

---

### OPT-M4: isSpatialPredicate (MEDIUM)
**File:** src/optimizer/query_planner.cpp
**Lines:** 680-717
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
return {"(spatial_column)", "(spatial_function)"};  // Placeholders
```

**Required Changes:**
1. Resolve actual column and function names
2. Return correct identifiers

**Verification:**
- [ ] Spatial predicates identified correctly

---

### OPT-M5: calculateQualCost (MEDIUM)
**File:** src/optimizer/query_planner.cpp
**Lines:** 1153-1168
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Hardcoded as one comparison operator per WHERE clause
```

**Required Changes:**
1. Traverse expression tree
2. Count operators
3. Estimate cost per operator type

**Verification:**
- [ ] Complex WHERE clauses have higher cost

---

### OPT-M6: extractHashKeys (MEDIUM)
**File:** src/optimizer/query_planner.cpp
**Lines:** 1526-1558
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Simplified Phase 1 implementation
// Doesn't verify columns belong to correct tables
```

**Required Changes:**
1. Verify left column from left table
2. Verify right column from right table
3. Handle table aliases correctly

**Verification:**
- [ ] Hash join uses correct columns

---

### OPT-M7: LSM merge cost (MEDIUM)
**File:** src/optimizer/cost_model.cpp
**Lines:** 157-159
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Phase 4 Enhancement: Add merge cost for range scans
```

**Required Changes:**
Estimate K-way merge cost for LSM-tree range scans:
```cpp
double mergeCost = numLevels * log2(numFilesPerLevel) * rowsPerFile;
```

**Verification:**
- [ ] LSM range scan cost reflects merge overhead

---

### OPT-M8: estimateIndexSize (MEDIUM)
**File:** src/optimizer/index_advisor.cpp
**Lines:** 736-756
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
return 0.0;  // If statistics unavailable
```

**Required Changes:**
1. Estimate from row count and column sizes
2. Account for index overhead

**Verification:**
- [ ] Index size estimate is reasonable

---

### OPT-M9: MV staleness calculation (MEDIUM)
**File:** src/optimizer/mv_rewriter.cpp
**Lines:** 212-213
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
staleness_seconds = 0.0;  // Hardcoded
```

**Required Changes:**
1. Get last_refresh_time from MV metadata
2. Calculate difference from current time

**Verification:**
- [ ] Old MVs have higher staleness

---

### OPT-M10: MV cost estimation (MEDIUM)
**File:** src/optimizer/mv_rewriter.cpp
**Line:** 216
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
double cost = 50.0;  // Default placeholder
```

**Required Changes:**
1. Get MV row count from statistics
2. Estimate scan cost

**Verification:**
- [ ] MV cost reflects actual size

---

### OPT-L1: invalidateCache (LOW)
**File:** src/optimizer/statistics_manager.cpp
**Lines:** 456-461
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Always clears entire cache
```

**Required Changes:**
1. Accept table_id parameter
2. Only invalidate entries for that table

**Verification:**
- [ ] Other table stats preserved on invalidation

---

### OPT-L2: Filter placeholders (LOW)
**File:** src/optimizer/query_planner.cpp
**Lines:** 899-1010
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Sets filter to "(filter present)" placeholder string
```

**Required Changes:**
1. Convert actual expression to string representation
2. Or store expression reference

**Verification:**
- [ ] EXPLAIN shows actual filter expression

---

## Dependencies

- OPT-1, OPT-2 need catalog table for pg_statistic
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

## Completion Checklist

- [ ] All 17 tasks implemented
- [ ] All 1020 existing tests pass
- [ ] Statistics persist across restart
- [ ] Query plans improve with statistics
- [ ] Code compiles without warnings

---

**Last Updated:** December 2, 2025

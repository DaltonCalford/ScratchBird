# Statistics Collection Design Document

**Phase**: Phase 1, Task 1.1 - Query Optimizer Foundation
**Component**: Statistics Collection
**Date**: October 25, 2025
**Status**: In Development (~75% Complete)

---

## Overview

This document describes the design and implementation of the statistics collection subsystem for the ScratchBird query optimizer. Statistics collection enables cost-based query optimization by providing accurate estimates of data distributions, cardinalities, and column characteristics.

## Architecture

### Components

1. **StatisticsManager** - Main API for statistics collection and retrieval
2. **ColumnStatistics** - Per-column statistics (null_fraction, n_distinct, histograms, MCVs)
3. **TableStatistics** - Per-table statistics (num_rows, num_pages, avg_row_size)
4. **pg_statistic Catalog** - Persistent storage of statistics
5. **ANALYZE Command** - SQL interface for statistics collection

### Data Flow

```
SQL: ANALYZE table_name
  ↓
Parser → AnalyzeStmt AST
  ↓
Executor → StatisticsManager::analyzeTable()
  ↓
1. Sample table (Vitter's Algorithm S)
  ↓
2. Compute column statistics
  ↓
3. Generate histograms (equal-height/equal-width)
  ↓
4. Identify Most Common Values (MCVs)
  ↓
5. Estimate n_distinct (HyperLogLog)
  ↓
6. Store in pg_statistic catalog
  ↓
7. Update statistics cache
```

## Implementation Status

### ✅ Completed (Tasks 1.1.1 - 1.1.7 except 1.1.8)

#### 1. Statistics Data Structures
**File**: `include/scratchbird/optimizer/statistics.h`

```cpp
struct ColumnStatistics {
    ID table_id;
    ID column_id;
    std::string column_name;
    core::DataType data_type;
    
    // Basic statistics
    uint64_t num_rows;
    uint64_t num_nulls;
    float null_fraction;
    uint64_t num_distinct;
    float avg_width;
    
    // Most Common Values
    std::vector<MCVEntry> mcv_list;
    
    // Histogram
    HistogramType histogram_type;
    std::vector<HistogramBucket> histogram_buckets;
    
    // Metadata
    uint64_t last_analyzed_time;
    uint64_t sample_size;
    float sample_rate;
};
```

#### 2. StatisticsManager API
**File**: `include/scratchbird/optimizer/statistics_manager.h`

```cpp
class StatisticsManager {
public:
    // Main API
    Status analyzeTable(const ID &table_id, float sample_rate = 0.0f, 
                       ErrorContext *ctx = nullptr);
    
    Status getColumnStatistics(const ID &table_id, const ID &column_id,
                              ColumnStatistics &stats, ErrorContext *ctx = nullptr);
    
    // Private implementation methods
private:
    Status sampleTable(...);              // Vitter's Algorithm S
    Status computeColumnStats(...);       // Statistics computation
    Status generateHistogram(...);        // Histogram generation
    Status identifyMCVs(...);             // Most Common Values
    uint64_t estimateNDistinct(...);      // HyperLogLog estimation
};
```

#### 3. ANALYZE Command Parser
**Syntax**: `ANALYZE table_name [COLUMN column_name] [SAMPLE sample_rate]`

**Examples**:
- `ANALYZE users` - Analyze all columns
- `ANALYZE users COLUMN email` - Analyze single column
- `ANALYZE users SAMPLE 0.1` - 10% sample rate
- `ANALYZE users COLUMN age SAMPLE 0.25` - Combined

**Implementation**:
- Added ANALYZE, COLUMN, SAMPLE keywords to lexer
- Created AnalyzeStmt AST node
- Implemented parseAnalyze() with validation
- Semantic analysis validates table/column existence

### ✅ Completed (Task 1.1.3)

#### Vitter's Algorithm S (Reservoir Sampling)

**Purpose**: Efficiently sample large tables with uniform random distribution

**Algorithm** (from Vitter, J.S., 1985):

```
Input: table with N rows, desired sample size n
Output: uniform random sample of n rows

Phase 1: Fill reservoir
  for i = 0 to n-1:
    reservoir[i] = row[i]

  if N <= n:
    return reservoir  # Table smaller than sample size

Phase 2: Geometric skipping
  W = exp(log(random()) / n)

  while i < N:
    # Skip rows according to geometric distribution
    i = i + floor(log(random()) / log(1 - W)) + 1

    if i < N:
      # Replace random item in reservoir
      j = random_int(0, n-1)
      reservoir[j] = row[i]

      # Update W for next iteration
      W = W * exp(log(random()) / n)

  return reservoir
```

**Characteristics**:
- **Time Complexity**: O(n * (1 + log(N/n))) - nearly linear
- **Space Complexity**: O(n) - only reservoir in memory
- **Uniformity**: Each row has exactly n/N probability of selection
- **Single-Pass**: Requires only one pass through the data

**Implementation**:
- Fully implemented in `StatisticsManager::sampleTable()`
- Uses `HeapScanIterator` for sequential table scan
- `std::mt19937` Mersenne Twister for high-quality random numbers
- Proper error handling and debug logging
- Handles edge cases (small tables, empty tables)

**File**: `src/optimizer/statistics_manager.cpp:149-272`

#### 4. Column Statistics Computation (Task 1.1.4)

**Purpose**: Extract column values from sampled tuples and compute statistics

**Implementation**:
- Integrated with CatalogManager to get table schema
- Parses TupleHeader and null bitmap from raw tuple bytes
- Type-aware column value extraction (INT32, INT64, FLOAT64, VARCHAR)
- Computes null_fraction, avg_width, num_rows, num_nulls
- Handles variable-length types with proper offset calculation
- Proper bounds checking to prevent buffer overruns

**File**: `src/optimizer/statistics_manager.cpp:274-493`

#### 5. n_distinct Estimation (Task 1.1.7)

**Purpose**: Estimate number of distinct values in full table from sample

**Algorithm**:
```
1. Count distinct values in sample using hash set
2. If sample_size >= total_rows: return exact count
3. If distinct_count < 100 or < 10% of sample: return exact count (likely saw everything)
4. Otherwise: extrapolate linearly with cap at total_rows
   estimate = distinct_in_sample * (total_rows / sample_size)
```

**Implementation**:
- Uses `std::unordered_set` with custom `VectorHash` functor
- Simple linear extrapolation (can be enhanced with HyperLogLog later)
- Heuristics for small cardinalities
- Logarithmic complexity for distinct counting: O(n log d) where d = distinct values

**File**: `src/optimizer/statistics_manager.cpp:543-602`

#### 6. Histogram Generation (Task 1.1.5)

**Purpose**: Generate histogram buckets for selectivity estimation

**Equal-Height Algorithm** (PostgreSQL-style):
```
1. Filter NULLs and sort values
2. Calculate values_per_bucket = total_values / bucket_count
3. Distribute remainder rows evenly (first R buckets get +1 row)
4. For each bucket:
   - lower_bound = first value in bucket
   - upper_bound = last value in bucket
   - row_count = number of values in bucket
   - frequency = row_count / total_values
```

**Equal-Width Algorithm** (MySQL-style):
- Currently falls back to equal-height for complex types
- Future: implement range-based bucketing for numeric types

**Implementation**:
- Handles edge cases (all NULLs, single value, fewer values than buckets)
- Caps bucket count at number of distinct values
- Zero-pads values smaller than 256-byte buffer
- TOAST OID = 0 (inline data)

**File**: `src/optimizer/statistics_manager.cpp:510-662`

#### 7. Most Common Values Identification (Task 1.1.6)

**Purpose**: Identify top-k most frequent values for accurate selectivity

**Algorithm**:
```
1. Build frequency map: value -> count (using hash table)
2. Convert to vector of (value, count) pairs
3. Sort by count descending
4. Take top max_mcv_count entries
5. Compute frequency = count / total_non_null_values
```

**Implementation**:
- Uses `std::unordered_map` with `VectorHash` functor
- Filters NULLs (not counted as MCV)
- Default max_mcv_count = 100 (PostgreSQL default)
- Stores value bytes and frequency fraction
- Time complexity: O(n) for frequency map + O(d log d) for sorting where d = distinct values

**File**: `src/optimizer/statistics_manager.cpp:664-756`

### 📋 Remaining Tasks (Task 1.1.8 only)

#### Task 1.1.8: Catalog Persistence
**Estimated**: 6-10 hours

```cpp
Status computeColumnStats(const ID &table_id, const ID &column_id,
                          const vector<vector<uint8_t>> &sample_rows,
                          ColumnStatistics &stats, ErrorContext *ctx) {
    // 1. Extract column values from sample
    // 2. Count NULLs → null_fraction
    // 3. Estimate n_distinct (HyperLogLog or exact if small)
    // 4. Compute avg_width (bytes per value)
    // 5. Call identifyMCVs()
    // 6. Call generateHistogram()
    // 7. Set metadata (timestamp, sample_size)
}
```

#### Task 1.1.5: Histogram Generation
**Estimated**: 6-10 hours

**Equal-Height Histogram** (PostgreSQL-style):
```
- Sort values
- Divide into k buckets with ~equal number of values
- Store [min, max] for each bucket
- Best for skewed distributions
```

**Equal-Width Histogram** (MySQL-style):
```
- Find global min/max
- Divide value range into k equal intervals
- Count values in each bucket
- Best for uniform distributions
```

#### Task 1.1.6: Most Common Values (MCV) Identification
**Estimated**: 4-6 hours

```cpp
Status identifyMCVs(const vector<uint8_t> &values, uint32_t max_mcv_count,
                   vector<MCVEntry> &mcv_list, ErrorContext *ctx) {
    // 1. Build frequency map
    // 2. Sort by frequency (descending)
    // 3. Take top max_mcv_count entries
    // 4. Compute frequency as fraction
    // 5. Store in mcv_list
}
```

#### Task 1.1.7: n_distinct Estimation
**Estimated**: 4-6 hours

**HyperLogLog Algorithm**:
- For small cardinality: exact count
- For large cardinality: HyperLogLog with ~2% error
- Alternative: Chao's estimator using singleton/doubleton counts

#### Task 1.1.8: Catalog Persistence
**Estimated**: 6-10 hours

```cpp
Status storeColumnStatistics(const ColumnStatistics &stats, ErrorContext *ctx) {
    // 1. Serialize MCVs → TOAST if large
    // 2. Serialize histogram → TOAST if large  
    // 3. Create StatisticsRecord
    // 4. Write to pg_statistic catalog page
    // 5. Update cache
}
```

## Design Decisions

### 1. Sampling Strategy

**Choice**: Vitter's Algorithm S

**Alternatives Considered**:
- Simple random sampling: Requires two passes (one to count rows)
- Systematic sampling: Not truly random, vulnerable to patterns
- Algorithm R: Simpler but slower (O(N) vs O(n))

**Rationale**: Algorithm S provides optimal performance for large tables with true uniform sampling in a single pass.

### 2. Histogram Type

**Choice**: Support both equal-height and equal-width

**Rationale**:
- PostgreSQL uses equal-height (better for skewed data)
- MySQL uses equal-width (simpler, better for uniform data)
- Supporting both provides flexibility

### 3. Default Sample Rate

**Choice**: Auto-select based on table size

```
if table_rows < 10,000:      sample_rate = 1.0 (100%)
elif table_rows < 100,000:   sample_rate = 0.5 (50%)
elif table_rows < 1,000,000: sample_rate = 0.1 (10%)
else:                        sample_rate = min(30000 / table_rows, 0.05)
```

**Rationale**: PostgreSQL default is ~30,000 rows, which balances accuracy vs performance.

### 4. Statistics Invalidation

**Triggers for Re-analysis**:
- After significant data changes (>10% of rows modified)
- Manual ANALYZE command
- Periodic background job (optional)

**Cache Invalidation**:
- On table DROP/TRUNCATE: immediate
- On bulk INSERT/UPDATE: mark stale
- On DDL changes: invalidate affected columns

## Integration Points

### 1. Query Planner

```cpp
// Selectivity estimation using statistics
float estimate_selectivity(const Predicate &pred) {
    ColumnStatistics stats;
    stats_manager_->getColumnStatistics(table_id, column_id, stats);
    
    if (pred.op == "=") {
        // Check MCVs first
        for (const auto &mcv : stats.mcv_list) {
            if (mcv.value == pred.constant)
                return mcv.frequency;
        }
        // Use n_distinct if not in MCVs
        return 1.0 / stats.num_distinct;
    }
    // Use histogram for range predicates...
}
```

### 2. ANALYZE Executor

```cpp
// Execute ANALYZE statement
Status execute_analyze(const AnalyzeStmt *stmt) {
    ID table_id = catalog_->resolveTable(stmt->tableName());
    
    if (stmt->analyzeAllColumns()) {
        return stats_manager_->analyzeTable(table_id, stmt->sampleRate());
    } else {
        ID column_id = catalog_->resolveColumn(stmt->columnName());
        return stats_manager_->analyzeColumn(table_id, column_id, 
                                             stmt->sampleRate());
    }
}
```

## Testing Strategy

### Unit Tests
- [ ] Vitter's Algorithm S uniformity (chi-square test)
- [ ] Histogram generation accuracy
- [ ] MCV frequency calculation
- [ ] n_distinct estimation error bounds

### Integration Tests
- [ ] ANALYZE command end-to-end
- [ ] Statistics persistence/retrieval
- [ ] Cache invalidation
- [ ] Multi-column analysis

### Performance Tests
- [ ] Sample 1M rows in < 1 second
- [ ] Analyze 10-column table in < 5 seconds
- [ ] Memory usage < sample_size * avg_row_size

## Future Enhancements (Post-Alpha)

1. **Multi-Dimensional Histograms**
   - Correlation between columns
   - Joint selectivity estimation

2. **Adaptive Sampling**
   - Dynamic sample size based on data distribution
   - Stratified sampling for partitioned tables

3. **Incremental Statistics**
   - Update statistics without full rescan
   - Merge old and new samples

4. **Background Auto-ANALYZE**
   - Automatic statistics collection
   - Triggered by data modification thresholds

## References

1. Vitter, J. S. (1985). "Random Sampling with a Reservoir". ACM Transactions on Mathematical Software, 11(1), 37-57.

2. Flajolet, P., et al. (2007). "HyperLogLog: the analysis of a near-optimal cardinality estimation algorithm". DMTCS Proceedings.

3. PostgreSQL Documentation: "Statistics Used by the Planner"
   https://www.postgresql.org/docs/current/planner-stats.html

4. MySQL Documentation: "InnoDB Persistent Statistics Tables"
   https://dev.mysql.com/doc/refman/8.0/en/innodb-persistent-stats.html

---

**Document Version**: 1.0  
**Last Updated**: October 25, 2025  
**Author**: Claude Code  
**Status**: Living Document - Updated as implementation progresses

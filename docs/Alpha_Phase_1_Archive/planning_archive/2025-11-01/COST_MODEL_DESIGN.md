# Cost Model Design Document

**Phase**: Phase 1, Task 1.2 - Query Optimizer Foundation
**Component**: Cost Model
**Date**: October 25, 2025
**Status**: In Development (0% Complete)

---

## Overview

The cost model assigns estimated execution costs to query plans, enabling the optimizer to choose the most efficient execution strategy. The cost model uses statistics (from Task 1.1) to estimate:
- Number of rows processed
- Number of disk I/O operations
- CPU processing time
- Memory usage

## Architecture

### Cost Components

Following PostgreSQL's proven approach, costs are measured in arbitrary units where:
- **1.0** = cost of reading one sequential page from disk
- All other costs are relative to this baseline

#### Key Cost Parameters

```cpp
struct CostParameters {
    // I/O costs (relative to seq_page_cost = 1.0)
    double seq_page_cost;        // Cost of sequential page read (baseline = 1.0)
    double random_page_cost;     // Cost of random page read (default = 4.0)

    // CPU costs (relative to seq_page_cost)
    double cpu_tuple_cost;       // Cost of processing one row (default = 0.01)
    double cpu_index_tuple_cost; // Cost of processing one index entry (default = 0.005)
    double cpu_operator_cost;    // Cost of evaluating an operator (default = 0.0025)

    // Memory/sorting costs
    double sort_mem_cost;        // Cost per byte of sort memory (default = 0.0001)

    // Parallelism (future)
    double parallel_setup_cost;  // Cost of starting parallel workers (default = 1000.0)
    double parallel_tuple_cost;  // Extra cost per tuple for parallelism (default = 0.1)

    // Cache effectiveness
    double effective_cache_size; // Estimated OS cache size in pages (default = 16384 pages = 128MB)
};
```

### Cost Estimation Functions

#### 1. Sequential Scan Cost

```
cost_seqscan = startup_cost + run_cost

startup_cost = 0  (no setup needed for sequential scan)

run_cost = disk_cost + cpu_cost

disk_cost = pages * seq_page_cost

cpu_cost = tuples * cpu_tuple_cost
           + tuples * qual_cost  (WHERE clause evaluation)

where:
  pages = number of pages in table (from statistics)
  tuples = estimated number of rows to scan
  qual_cost = sum of cpu_operator_cost for each predicate
```

**Example**:
```
Table: users (10,000 rows, 100 pages)
Query: SELECT * FROM users WHERE age > 25

pages = 100
tuples = 10,000
qual_cost = cpu_operator_cost (one comparison)

disk_cost = 100 * 1.0 = 100.0
cpu_cost = 10,000 * 0.01 + 10,000 * 0.0025 = 125.0
total_cost = 0 + 100.0 + 125.0 = 225.0
```

#### 2. Index Scan Cost

```
cost_indexscan = startup_cost + run_cost

startup_cost = index_startup_cost
             = cpu_operator_cost * index_height

run_cost = index_run_cost + table_run_cost

index_run_cost = index_pages * random_page_cost
                 + index_tuples * cpu_index_tuple_cost

table_run_cost = table_pages * random_page_cost  (for heap fetch)
                 + tuples * cpu_tuple_cost
                 + tuples * qual_cost

where:
  index_height = B-tree height (typically 2-4)
  index_pages = estimated index pages accessed
  index_tuples = number of index entries scanned
  table_pages = number of heap pages accessed (with correlation factor)
```

**Correlation Factor**:
Physical ordering correlation between index and heap:
- **1.0** = perfectly correlated (index order = heap order)
- **0.0** = random correlation
- Affects table_pages: fewer pages if well-correlated

```
correlation = statistics.correlation  (from ANALYZE)

if correlation > 0.8:
    table_pages = tuples / tuples_per_page  (sequential-like)
else:
    table_pages = min(tuples, total_table_pages)  (assume random)
```

**Example**:
```
Table: users (10,000 rows, 100 pages)
Index: idx_users_id on id (B-tree, height=3)
Query: SELECT * FROM users WHERE id = 42

Selectivity: 1/10,000 = 0.0001
tuples = 10,000 * 0.0001 = 1

startup_cost = 0.0025 * 3 = 0.0075

index_pages = 3 (root + 2 internal nodes + 1 leaf)
index_tuples = 1

index_run_cost = 3 * 4.0 + 1 * 0.005 = 12.005

table_pages = 1 (fetch one heap page)
table_run_cost = 1 * 4.0 + 1 * 0.01 = 4.01

total_cost = 0.0075 + 12.005 + 4.01 = 16.0225

Compare to seqscan cost of 225.0 → Index scan is 14x cheaper!
```

#### 3. Cache Effect Modeling

When pages are likely cached in memory, random I/O is much cheaper:

```
effective_random_page_cost = random_page_cost * (1 - cache_hit_ratio)
                           + seq_page_cost * cache_hit_ratio

cache_hit_ratio = min(1.0, effective_cache_size / total_table_pages)

Example:
  table_pages = 100
  effective_cache_size = 16384 pages (128MB)
  cache_hit_ratio = min(1.0, 16384 / 100) = 1.0  (entire table fits in cache!)

  effective_random_page_cost = 4.0 * 0.0 + 1.0 * 1.0 = 1.0

  → Random access becomes as cheap as sequential when cached
```

## Implementation Plan

### Task 1.2.1: Cost Configuration Structure

Create `include/scratchbird/optimizer/cost_model.h`:

```cpp
namespace scratchbird::optimizer {

struct CostParameters {
    double seq_page_cost = 1.0;
    double random_page_cost = 4.0;
    double cpu_tuple_cost = 0.01;
    double cpu_index_tuple_cost = 0.005;
    double cpu_operator_cost = 0.0025;
    double effective_cache_size = 16384.0;  // pages (128MB with 8KB pages)
};

struct CostEstimate {
    double startup_cost;  // One-time setup cost
    double run_cost;      // Per-execution cost
    double total_cost;    // startup_cost + run_cost
    uint64_t rows;        // Estimated rows returned
};

class CostModel {
public:
    explicit CostModel(const CostParameters &params = CostParameters{});

    // Sequential scan cost
    auto costSeqScan(uint64_t num_pages, uint64_t num_tuples,
                     double qual_cost, ErrorContext *ctx = nullptr)
        -> CostEstimate;

    // Index scan cost
    auto costIndexScan(uint64_t index_height, uint64_t index_pages,
                       uint64_t index_tuples, uint64_t heap_pages,
                       uint64_t heap_tuples, double qual_cost,
                       double correlation = 0.0, ErrorContext *ctx = nullptr)
        -> CostEstimate;

    // Cache-adjusted random page cost
    auto effectiveRandomPageCost(uint64_t table_pages) const -> double;

    // Operator evaluation cost
    auto operatorCost(const std::string &op) const -> double;

private:
    CostParameters params_;
};

}
```

### Task 1.2.2: Sequential Scan Cost Estimation

Implement `CostModel::costSeqScan()`:

```cpp
auto CostModel::costSeqScan(uint64_t num_pages, uint64_t num_tuples,
                            double qual_cost, ErrorContext *ctx)
    -> CostEstimate
{
    CostEstimate cost;

    // Sequential scan has no startup cost
    cost.startup_cost = 0.0;

    // Disk cost: read all pages sequentially
    double disk_cost = static_cast<double>(num_pages) * params_.seq_page_cost;

    // CPU cost: process all tuples + evaluate WHERE clause
    double cpu_cost = static_cast<double>(num_tuples) * params_.cpu_tuple_cost
                    + static_cast<double>(num_tuples) * qual_cost;

    cost.run_cost = disk_cost + cpu_cost;
    cost.total_cost = cost.startup_cost + cost.run_cost;
    cost.rows = num_tuples;

    return cost;
}
```

### Task 1.2.3: Index Scan Cost Estimation

Implement `CostModel::costIndexScan()`:

```cpp
auto CostModel::costIndexScan(uint64_t index_height, uint64_t index_pages,
                              uint64_t index_tuples, uint64_t heap_pages,
                              uint64_t heap_tuples, double qual_cost,
                              double correlation, ErrorContext *ctx)
    -> CostEstimate
{
    CostEstimate cost;

    // Startup: traverse B-tree to first entry
    cost.startup_cost = static_cast<double>(index_height) * params_.cpu_operator_cost;

    // Index scan cost: random I/O for index pages
    double index_io_cost = static_cast<double>(index_pages) * params_.random_page_cost;
    double index_cpu_cost = static_cast<double>(index_tuples) * params_.cpu_index_tuple_cost;

    // Heap fetch cost: depends on correlation
    double effective_heap_pages;
    if (correlation > 0.8) {
        // Well-correlated: sequential-like access
        effective_heap_pages = static_cast<double>(heap_tuples) / 100.0;  // assume ~100 tuples/page
    } else {
        // Poor correlation: random access
        effective_heap_pages = static_cast<double>(heap_pages);
    }

    double heap_io_cost = effective_heap_pages * effectiveRandomPageCost(heap_pages);
    double heap_cpu_cost = static_cast<double>(heap_tuples) * params_.cpu_tuple_cost
                         + static_cast<double>(heap_tuples) * qual_cost;

    cost.run_cost = index_io_cost + index_cpu_cost + heap_io_cost + heap_cpu_cost;
    cost.total_cost = cost.startup_cost + cost.run_cost;
    cost.rows = heap_tuples;

    return cost;
}
```

### Task 1.2.4: Cache Effect Modeling

Implement `CostModel::effectiveRandomPageCost()`:

```cpp
auto CostModel::effectiveRandomPageCost(uint64_t table_pages) const -> double
{
    // Calculate cache hit ratio
    double cache_hit_ratio = std::min(1.0, params_.effective_cache_size /
                                           static_cast<double>(table_pages));

    // Blend random and sequential costs based on cache hit ratio
    return params_.random_page_cost * (1.0 - cache_hit_ratio)
         + params_.seq_page_cost * cache_hit_ratio;
}
```

### Task 1.2.5: Operator Cost Mapping

Implement `CostModel::operatorCost()`:

```cpp
auto CostModel::operatorCost(const std::string &op) const -> double
{
    // Simple operators: comparison, arithmetic
    if (op == "=" || op == "!=" || op == "<" || op == ">" ||
        op == "<=" || op == ">=" || op == "+" || op == "-") {
        return params_.cpu_operator_cost;
    }

    // More expensive operators: multiplication, division
    if (op == "*" || op == "/") {
        return params_.cpu_operator_cost * 2.0;
    }

    // String operations: more expensive
    if (op == "LIKE" || op == "ILIKE") {
        return params_.cpu_operator_cost * 10.0;
    }

    // Function calls: depends on function
    if (op == "substr" || op == "concat") {
        return params_.cpu_operator_cost * 5.0;
    }

    // Default
    return params_.cpu_operator_cost;
}
```

## Integration with Statistics

The cost model uses statistics from Task 1.1:

```cpp
// Example: Cost estimation for "SELECT * FROM users WHERE age > 25"

// 1. Get table statistics
TableStatistics table_stats;
stats_manager->getTableStatistics(users_table_id, table_stats);

// 2. Get column statistics for selectivity
ColumnStatistics age_stats;
stats_manager->getColumnStatistics(users_table_id, age_column_id, age_stats);

// 3. Estimate selectivity (Task 1.4)
double selectivity = estimateRangeSelectivity(age_stats, ">", 25);

// 4. Calculate cost
uint64_t num_tuples = static_cast<uint64_t>(table_stats.num_rows * selectivity);
double qual_cost = cost_model.operatorCost(">");

CostEstimate cost = cost_model.costSeqScan(
    table_stats.num_pages,
    num_tuples,
    qual_cost
);
```

## Testing Strategy

### Unit Tests
- [ ] Cost parameter defaults match PostgreSQL
- [ ] Sequential scan cost calculation
- [ ] Index scan cost calculation
- [ ] Cache effect reduces random I/O cost
- [ ] Operator cost mapping

### Integration Tests
- [ ] Cost estimates decrease with better selectivity
- [ ] Index scan cheaper than seqscan for selective queries
- [ ] Seqscan cheaper than index scan for low selectivity
- [ ] Cache effect lowers costs for small tables

### Performance Tests
- [ ] Cost estimation completes in < 1ms
- [ ] Cost model overhead negligible vs query execution

## Design Decisions

### 1. Why PostgreSQL's Cost Model?

**Choice**: Adopt PostgreSQL's proven cost parameters

**Rationale**:
- Battle-tested across millions of production deployments
- Well-documented and understood
- Provides good defaults for most hardware
- Easily tunable for different workloads

### 2. Cost Units

**Choice**: Relative costs (seq_page_cost = 1.0 baseline)

**Rationale**:
- Hardware-independent (scales with actual performance)
- Easier to reason about (4x more expensive vs 0.004 seconds)
- Matches PostgreSQL for compatibility

### 3. Cache Modeling

**Choice**: Simple linear cache hit ratio model

**Rationale**:
- Good enough for most cases
- Complexity of LRU/ARC modeling not worth it
- Can be refined later based on profiling

## Future Enhancements

1. **Parallel Query Costs** (Phase 2)
   - Model parallel workers
   - Communication overhead
   - Shared vs local memory

2. **Sort/Hash Costs** (Phase 2)
   - Memory vs disk sort
   - Hash table size estimation
   - Spill-to-disk costs

3. **Network Costs** (Phase 3)
   - Client/server communication
   - Result set transmission

4. **Adaptive Costs** (Phase 3)
   - Learn from actual execution times
   - Adjust parameters dynamically

## References

1. PostgreSQL Documentation: "Cost Estimation"
   https://www.postgresql.org/docs/current/planner-stats.html

2. "Query Optimization" by Jarke & Koch (1984)
   Classic paper on cost-based optimization

3. PostgreSQL Source: `src/backend/optimizer/path/costsize.c`
   Reference implementation

---

**Document Version**: 1.0
**Last Updated**: October 25, 2025
**Author**: Claude Code
**Status**: Design Complete - Ready for Implementation

# Selectivity Estimation Design Document

**Phase**: Phase 1, Task 1.4 - Query Optimizer Foundation
**Component**: Selectivity Estimation
**Date**: October 25, 2025
**Status**: In Development (0% Complete)

---

## Overview

Selectivity estimation predicts what fraction of rows will satisfy a WHERE clause predicate. Accurate selectivity estimates are critical for cost-based query optimization:
- **Too high**: Optimizer may choose index scan when sequential scan is better
- **Too low**: Optimizer may choose sequential scan when index scan is better
- **Goal**: Estimate within 2-3x of actual selectivity

Selectivity is expressed as a value between 0.0 and 1.0:
- **0.0**: No rows match (predicate is always false)
- **1.0**: All rows match (predicate is always true)
- **0.5**: Half of rows match

---

## Selectivity Formula Integration

Given a table with `N` rows and a predicate with selectivity `s`:
```
estimated_rows = N * s
```

Example:
```sql
SELECT * FROM users WHERE age > 25
```
If `users` has 10,000 rows and `age > 25` has selectivity 0.6:
```
estimated_rows = 10,000 * 0.6 = 6,000
```

---

## Architecture

### SelectivityEstimator Class

The `SelectivityEstimator` class provides methods to estimate selectivity for different predicate types:

```cpp
class SelectivityEstimator {
public:
    // Estimate selectivity for a complete WHERE clause
    auto estimateWhereClause(const Expression *where_clause,
                            const ID &table_id,
                            ErrorContext *ctx) -> double;

    // Estimate selectivity for individual predicate types
    auto estimateEquality(const ID &table_id, const ID &column_id,
                         const Value &value, ErrorContext *ctx) -> double;

    auto estimateRange(const ID &table_id, const ID &column_id,
                      const std::string &op, const Value &value,
                      ErrorContext *ctx) -> double;

    auto estimateLike(const ID &table_id, const ID &column_id,
                     const std::string &pattern, ErrorContext *ctx) -> double;

    auto estimateIn(const ID &table_id, const ID &column_id,
                   const std::vector<Value> &values, ErrorContext *ctx) -> double;

    auto estimateAnd(double sel1, double sel2) -> double;
    auto estimateOr(double sel1, double sel2) -> double;
    auto estimateNot(double sel) -> double;
};
```

---

## Estimation Algorithms

### 1. Equality Selectivity (= operator)

**Formula**:
```
selectivity = 1.0 / n_distinct
```

**Rationale**: Assumes uniform distribution. If a column has 100 distinct values, each value appears in ~1% of rows.

**Example**:
```sql
SELECT * FROM users WHERE country = 'USA'
```
If `country` has `n_distinct = 50`:
```
selectivity = 1.0 / 50 = 0.02 (2% of rows)
```

**With Most Common Values (MCVs)**:
If the value is in the MCV list, use its actual frequency:
```
if value in mcv_list:
    selectivity = mcv_frequency
else:
    # Remaining values share leftover probability uniformly
    remaining_selectivity = 1.0 - sum(mcv_frequencies)
    remaining_distinct = n_distinct - len(mcv_list)
    selectivity = remaining_selectivity / remaining_distinct
```

**Example with MCVs**:
```
n_distinct = 100
mcv_list = [('USA', 0.40), ('Canada', 0.20), ('UK', 0.10)]
sum(mcv_frequencies) = 0.70

Query: WHERE country = 'USA'
  → selectivity = 0.40 (from MCV)

Query: WHERE country = 'France'
  → remaining_selectivity = 1.0 - 0.70 = 0.30
  → remaining_distinct = 100 - 3 = 97
  → selectivity = 0.30 / 97 = 0.0031 (0.31%)
```

---

### 2. Range Selectivity (>, <, >=, <=)

**Histogram-based Estimation**:

Use equal-height histograms to estimate what fraction of values fall in the range.

**Algorithm**:
1. Find histogram buckets overlapping the range
2. Sum full buckets completely in range
3. Interpolate partial buckets at boundaries

**Example**:
```sql
SELECT * FROM users WHERE age > 25
```

Histogram (4 buckets, each with 25% of rows):
```
Bucket 1: [18, 25]  → 25% of rows
Bucket 2: [26, 35]  → 25% of rows
Bucket 3: [36, 50]  → 25% of rows
Bucket 4: [51, 80]  → 25% of rows
```

Query: `age > 25`
- Bucket 1: 0% (all values ≤ 25)
- Bucket 2: 100% (all values > 25)
- Bucket 3: 100% (all values > 25)
- Bucket 4: 100% (all values > 25)

```
selectivity = 0.25 * 0 + 0.25 * 1 + 0.25 * 1 + 0.25 * 1
            = 0.75 (75% of rows)
```

**Partial Bucket Interpolation**:

Query: `age > 30` (value inside Bucket 2: [26, 35])

Bucket 2 interpolation:
```
bucket_min = 26
bucket_max = 35
value = 30

fraction = (bucket_max - value) / (bucket_max - bucket_min)
         = (35 - 30) / (35 - 26)
         = 5 / 9
         = 0.556

Bucket 2 contribution: 0.25 * 0.556 = 0.139
```

Total selectivity:
```
selectivity = 0.25 * 0         (Bucket 1: all ≤ 30)
            + 0.25 * 0.556     (Bucket 2: partial)
            + 0.25 * 1.0       (Bucket 3: all > 30)
            + 0.25 * 1.0       (Bucket 4: all > 30)
            = 0.139 + 0.25 + 0.25
            = 0.639 (64% of rows)
```

---

### 3. BETWEEN Selectivity

**Formula**:
```
selectivity(col BETWEEN a AND b) = selectivity(col >= a) - selectivity(col > b)
```

**Example**:
```sql
SELECT * FROM users WHERE age BETWEEN 25 AND 35
```

Using histogram:
```
selectivity(age >= 25) = 0.75
selectivity(age > 35)  = 0.50

selectivity(BETWEEN 25 AND 35) = 0.75 - 0.50 = 0.25
```

---

### 4. LIKE Selectivity

**Pattern Analysis**:

```sql
SELECT * FROM users WHERE name LIKE 'John%'
```

**Heuristics**:
1. **Prefix match (`'John%'`)**: Use histogram range estimation on prefix
   ```
   # Treat as range: name >= 'John' AND name < 'Joho'
   selectivity = estimateRange('>=', 'John') - estimateRange('>=', 'Joho')
   ```

2. **Suffix match (`'%Smith'`)**: Cannot use index, assume moderate selectivity
   ```
   selectivity = 0.05  (5% default)
   ```

3. **Contains match (`'%John%'`)**: Cannot use index, assume low selectivity
   ```
   selectivity = 0.01  (1% default)
   ```

4. **Exact match (`'John'`)**: Same as equality
   ```
   selectivity = 1.0 / n_distinct
   ```

**Default Selectivity Values**:
```
'literal%'    → 0.10  (10% - prefix match)
'%literal'    → 0.05  (5% - suffix match)
'%literal%'   → 0.01  (1% - contains match)
'%'           → 1.00  (100% - matches all)
```

---

### 5. IN Selectivity

**Formula**:
```
selectivity(col IN (v1, v2, v3)) = sum(selectivity(col = vi))
```

Capped at 1.0 to avoid over-estimation.

**Example**:
```sql
SELECT * FROM users WHERE country IN ('USA', 'Canada', 'UK')
```

```
selectivity('USA')    = 0.40 (from MCV)
selectivity('Canada') = 0.20 (from MCV)
selectivity('UK')     = 0.10 (from MCV)

total = 0.40 + 0.20 + 0.10 = 0.70 (70% of rows)
```

---

### 6. Compound Predicates (AND/OR/NOT)

#### AND Selectivity

**Formula** (independence assumption):
```
selectivity(P1 AND P2) = selectivity(P1) * selectivity(P2)
```

**Example**:
```sql
SELECT * FROM users WHERE age > 25 AND country = 'USA'
```

```
selectivity(age > 25)      = 0.75
selectivity(country = 'USA') = 0.40

selectivity(AND) = 0.75 * 0.40 = 0.30 (30% of rows)
```

**Correlation Adjustment** (future enhancement):
If columns are correlated, the independence assumption breaks down. For now, we assume independence.

---

#### OR Selectivity

**Formula**:
```
selectivity(P1 OR P2) = selectivity(P1) + selectivity(P2) - selectivity(P1 AND P2)
                      = sel1 + sel2 - (sel1 * sel2)
```

**Example**:
```sql
SELECT * FROM users WHERE age > 60 OR country = 'USA'
```

```
selectivity(age > 60)      = 0.10
selectivity(country = 'USA') = 0.40

selectivity(OR) = 0.10 + 0.40 - (0.10 * 0.40)
                = 0.50 - 0.04
                = 0.46 (46% of rows)
```

---

#### NOT Selectivity

**Formula**:
```
selectivity(NOT P) = 1.0 - selectivity(P)
```

**Example**:
```sql
SELECT * FROM users WHERE NOT (age > 60)
```

```
selectivity(age > 60) = 0.10

selectivity(NOT) = 1.0 - 0.10 = 0.90 (90% of rows)
```

---

## Default Selectivity Values

When statistics are unavailable, use these conservative defaults:

| Predicate Type | Default Selectivity | Rationale |
|----------------|---------------------|-----------|
| `col = value` | 0.01 (1%) | Assume 100 distinct values |
| `col != value` | 0.99 (99%) | Opposite of equality |
| `col > value` | 0.33 (33%) | Conservative guess |
| `col < value` | 0.33 (33%) | Conservative guess |
| `col >= value` | 0.33 (33%) | Conservative guess |
| `col <= value` | 0.33 (33%) | Conservative guess |
| `col BETWEEN a AND b` | 0.10 (10%) | Narrow range assumption |
| `col LIKE 'prefix%'` | 0.10 (10%) | Prefix match |
| `col LIKE '%suffix'` | 0.05 (5%) | Suffix match |
| `col LIKE '%contains%'` | 0.01 (1%) | Contains match |
| `col IN (values)` | `0.01 * len(values)` | Multiple equality |
| `col IS NULL` | 0.05 (5%) | Few nulls typically |
| `col IS NOT NULL` | 0.95 (95%) | Most values non-null |

---

## Implementation Plan

### Task 1.4.1: SelectivityEstimator Class Structure (3-5 hours)

Create `include/scratchbird/optimizer/selectivity_estimator.h`:

```cpp
namespace scratchbird::optimizer {

class SelectivityEstimator {
public:
    SelectivityEstimator(StatisticsManager *stats_manager)
        : stats_manager_(stats_manager) {}

    // Main entry point: estimate WHERE clause selectivity
    auto estimateWhereClause(const parser::Expression *where_clause,
                            const core::ID &table_id,
                            core::ErrorContext *ctx) -> double;

    // Individual predicate types
    auto estimateEquality(const core::ID &table_id,
                         const core::ID &column_id,
                         const std::vector<uint8_t> &value,
                         core::ErrorContext *ctx) -> double;

    auto estimateRange(const core::ID &table_id,
                      const core::ID &column_id,
                      const std::string &op,
                      const std::vector<uint8_t> &value,
                      core::ErrorContext *ctx) -> double;

    auto estimateLike(const core::ID &table_id,
                     const core::ID &column_id,
                     const std::string &pattern,
                     core::ErrorContext *ctx) -> double;

    auto estimateIn(const core::ID &table_id,
                   const core::ID &column_id,
                   const std::vector<std::vector<uint8_t>> &values,
                   core::ErrorContext *ctx) -> double;

    // Compound predicates
    auto estimateAnd(double sel1, double sel2) const -> double;
    auto estimateOr(double sel1, double sel2) const -> double;
    auto estimateNot(double sel) const -> double;

private:
    StatisticsManager *stats_manager_;

    // Default selectivity values
    static constexpr double DEFAULT_EQUALITY_SEL = 0.01;
    static constexpr double DEFAULT_RANGE_SEL = 0.33;
    static constexpr double DEFAULT_LIKE_PREFIX_SEL = 0.10;
    static constexpr double DEFAULT_LIKE_SUFFIX_SEL = 0.05;
    static constexpr double DEFAULT_LIKE_CONTAINS_SEL = 0.01;
};

}
```

---

### Task 1.4.2: Equality Selectivity (3-5 hours)

Implement `estimateEquality()`:

```cpp
auto SelectivityEstimator::estimateEquality(
    const core::ID &table_id,
    const core::ID &column_id,
    const std::vector<uint8_t> &value,
    core::ErrorContext *ctx) -> double
{
    // Get column statistics
    ColumnStatistics col_stats;
    Status status = stats_manager_->getColumnStatistics(
        table_id, column_id, col_stats, ctx);

    if (status != Status::OK) {
        return DEFAULT_EQUALITY_SEL;
    }

    // Check if value is NULL
    if (value.empty()) {
        return col_stats.null_fraction;
    }

    // Check MCVs first
    for (const auto &mcv : col_stats.mcv_list) {
        if (valueEquals(mcv.value_data, value)) {
            return mcv.frequency;
        }
    }

    // Value not in MCV list
    // Distribute remaining probability among remaining values
    if (col_stats.num_distinct <= col_stats.mcv_list.size()) {
        // All distinct values are in MCV list
        return 0.0;
    }

    double mcv_total_freq = 0.0;
    for (const auto &mcv : col_stats.mcv_list) {
        mcv_total_freq += mcv.frequency;
    }

    double remaining_freq = 1.0 - mcv_total_freq - col_stats.null_fraction;
    uint64_t remaining_distinct = col_stats.num_distinct - col_stats.mcv_list.size();

    if (remaining_distinct == 0) {
        return 0.0;
    }

    return remaining_freq / static_cast<double>(remaining_distinct);
}
```

---

### Task 1.4.3: Range Selectivity (5-8 hours)

Implement `estimateRange()` with histogram interpolation:

```cpp
auto SelectivityEstimator::estimateRange(
    const core::ID &table_id,
    const core::ID &column_id,
    const std::string &op,
    const std::vector<uint8_t> &value,
    core::ErrorContext *ctx) -> double
{
    // Get column statistics
    ColumnStatistics col_stats;
    Status status = stats_manager_->getColumnStatistics(
        table_id, column_id, col_stats, ctx);

    if (status != Status::OK) {
        return DEFAULT_RANGE_SEL;
    }

    // Use histogram to estimate
    if (col_stats.histogram_buckets.empty()) {
        return DEFAULT_RANGE_SEL;
    }

    double selectivity = 0.0;

    for (const auto &bucket : col_stats.histogram_buckets) {
        if (op == ">") {
            if (compareValues(value, bucket.upper_bound) < 0) {
                // Value is below upper bound
                if (compareValues(value, bucket.lower_bound) < 0) {
                    // Entire bucket is above value
                    selectivity += bucket.frequency;
                } else {
                    // Value is inside bucket - interpolate
                    double fraction = interpolateBucket(
                        value, bucket.lower_bound, bucket.upper_bound);
                    selectivity += bucket.frequency * fraction;
                }
            }
        }
        // Similar logic for <, >=, <=
    }

    return std::min(1.0, std::max(0.0, selectivity));
}
```

---

### Task 1.4.4: LIKE Selectivity (2-4 hours)

Implement `estimateLike()` with pattern analysis:

```cpp
auto SelectivityEstimator::estimateLike(
    const core::ID &table_id,
    const core::ID &column_id,
    const std::string &pattern,
    core::ErrorContext *ctx) -> double
{
    // Analyze pattern
    if (pattern == "%") {
        return 1.0;  // Matches everything
    }

    bool starts_with_wildcard = (pattern[0] == '%');
    bool ends_with_wildcard = (pattern.back() == '%');

    if (!starts_with_wildcard && ends_with_wildcard) {
        // Prefix match: 'John%'
        // Can use histogram range estimation
        return DEFAULT_LIKE_PREFIX_SEL;
    } else if (starts_with_wildcard && !ends_with_wildcard) {
        // Suffix match: '%Smith'
        return DEFAULT_LIKE_SUFFIX_SEL;
    } else if (starts_with_wildcard && ends_with_wildcard) {
        // Contains match: '%John%'
        return DEFAULT_LIKE_CONTAINS_SEL;
    } else {
        // Exact match: 'John'
        // Treat as equality
        std::vector<uint8_t> value(pattern.begin(), pattern.end());
        return estimateEquality(table_id, column_id, value, ctx);
    }
}
```

---

### Task 1.4.5: IN and Compound Selectivity (2-3 hours)

Implement `estimateIn()`, `estimateAnd()`, `estimateOr()`, `estimateNot()`:

```cpp
auto SelectivityEstimator::estimateIn(
    const core::ID &table_id,
    const core::ID &column_id,
    const std::vector<std::vector<uint8_t>> &values,
    core::ErrorContext *ctx) -> double
{
    double total_sel = 0.0;

    for (const auto &value : values) {
        double sel = estimateEquality(table_id, column_id, value, ctx);
        total_sel += sel;
    }

    return std::min(1.0, total_sel);
}

auto SelectivityEstimator::estimateAnd(double sel1, double sel2) const -> double
{
    return sel1 * sel2;
}

auto SelectivityEstimator::estimateOr(double sel1, double sel2) const -> double
{
    return sel1 + sel2 - (sel1 * sel2);
}

auto SelectivityEstimator::estimateNot(double sel) const -> double
{
    return 1.0 - sel;
}
```

---

## Integration with Query Planner

Update `QueryPlanner::estimateSelectivity()` to use `SelectivityEstimator`:

```cpp
auto QueryPlanner::estimateSelectivity(const parser::SelectStmt *select_stmt,
                                        const core::ID &table_id,
                                        core::ErrorContext *ctx) const
    -> double
{
    if (!select_stmt->whereClause()) {
        return 1.0;
    }

    SelectivityEstimator estimator(stats_manager_);
    return estimator.estimateWhereClause(
        select_stmt->whereClause(), table_id, ctx);
}
```

---

## Testing Strategy

### Unit Tests

- [ ] Equality selectivity with MCVs
- [ ] Equality selectivity without MCVs
- [ ] Range selectivity with histograms
- [ ] Range selectivity without histograms
- [ ] LIKE patterns (prefix, suffix, contains)
- [ ] IN with multiple values
- [ ] AND/OR/NOT compound predicates

### Integration Tests

- [ ] Full WHERE clause estimation
- [ ] Query planner uses correct selectivity
- [ ] Cost estimates change with selectivity

### Accuracy Tests

- [ ] Compare estimated vs actual selectivity
- [ ] Measure estimation error on real data
- [ ] Target: within 2-3x of actual

---

## Design Decisions

### 1. Independence Assumption for AND/OR

**Choice**: Assume predicate independence

**Rationale**:
- Correlation detection requires joint distribution statistics
- PostgreSQL also assumes independence by default
- Errors typically within acceptable range (2-3x)
- Can be enhanced later with correlation statistics

### 2. Uniform Distribution Within Buckets

**Choice**: Linear interpolation within histogram buckets

**Rationale**:
- Simple and fast
- Good enough for most workloads
- PostgreSQL uses same approach

### 3. Default Selectivity Values

**Choice**: Conservative defaults (lean toward lower selectivity)

**Rationale**:
- Underestimating selectivity → choose index scan → slower than expected but correct results
- Overestimating selectivity → choose seq scan → much slower on large tables
- Better to be conservative

---

## Future Enhancements

1. **Correlation Statistics** (Phase 2)
   - Detect correlated columns
   - Adjust AND/OR selectivity

2. **Multi-column Histograms** (Phase 2)
   - Joint distribution statistics
   - More accurate for correlated predicates

3. **Sampling-based Estimation** (Phase 3)
   - When statistics are missing, sample rows
   - Dynamic selectivity estimation

4. **Machine Learning** (Phase 3)
   - Learn selectivity patterns from workload
   - Adapt to data distribution changes

---

**Document Version**: 1.0
**Last Updated**: October 25, 2025
**Author**: Claude Code
**Status**: Design Complete - Ready for Implementation

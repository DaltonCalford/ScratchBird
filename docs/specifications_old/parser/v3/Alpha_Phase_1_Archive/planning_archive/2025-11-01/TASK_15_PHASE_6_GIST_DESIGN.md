# Task 15 Phase 6: GiST Index Support for Range Types

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: October 30, 2025
**Status**: Design Phase
**Priority**: Future Enhancement

## Overview

This document outlines the design for GiST (Generalized Search Tree) index support for range types in ScratchBird. GiST indexes enable efficient queries on range types, particularly for overlap, containment, and adjacency predicates.

## Background

### What is GiST?

GiST (Generalized Search Tree) is an extensible indexing framework that allows different data types to define their own indexing strategies. For range types, GiST provides:

1. **Efficient Range Queries**: Fast lookup of ranges that overlap, contain, or are adjacent to a query range
2. **Scalability**: Logarithmic search time for large datasets
3. **Multi-dimensional Support**: Can index ranges across multiple dimensions

### PostgreSQL Compatibility

PostgreSQL provides GiST indexes for range types using the `btree_gist` extension. Our implementation will be compatible with PostgreSQL's range GiST semantics.

## Requirements

### Functional Requirements

1. **Index Creation**: Support `CREATE INDEX ... USING GIST (range_column)`
2. **Query Optimization**: Automatically use GiST index for range predicates
3. **Operator Support**: Index should support:
   - `&&` (overlaps)
   - `@>` (contains)
   - `<@` (contained by)
   - `<<` (strictly left of)
   - `>>` (strictly right of)
   - `-|-` (adjacent)
   - `=` (equality)

### Performance Requirements

1. **Insert Performance**: O(log n) insertion time
2. **Query Performance**: O(log n) search time for range queries
3. **Space Efficiency**: Reasonable storage overhead (typically 2-3x data size)

## Design

### GiST Key Structure for Ranges

Each GiST index entry stores:
```cpp
struct RangeGistKey {
    bool lower_inf;      // Is lower bound infinite?
    bool upper_inf;      // Is upper bound infinite?
    bool lower_inc;      // Is lower bound inclusive?
    bool upper_inc;      // Is upper bound inclusive?
    TypedValue lower;    // Lower bound value (if not infinite)
    TypedValue upper;    // Upper bound value (if not infinite)
    bool is_empty;       // Is this range empty?
};
```

### GiST Methods for Range Types

#### 1. Consistent (range_gist_consistent)

Determines if a range matches the search predicate:

```cpp
bool range_gist_consistent(
    const RangeGistKey& key,
    const Range& query,
    GistSearchOp op
) {
    switch (op) {
        case OVERLAPS:
            return key_overlaps_query(key, query);
        case CONTAINS:
            return key_contains_query(key, query);
        case CONTAINED_BY:
            return key_contained_by_query(key, query);
        case LEFT_OF:
            return key_left_of_query(key, query);
        case RIGHT_OF:
            return key_right_of_query(key, query);
        case ADJACENT:
            return key_adjacent_to_query(key, query);
        case EQUALS:
            return key_equals_query(key, query);
    }
}
```

#### 2. Union (range_gist_union)

Computes the bounding range for a set of ranges:

```cpp
RangeGistKey range_gist_union(
    const std::vector<RangeGistKey>& keys
) {
    RangeGistKey result;

    // Find minimum lower bound
    result.lower_inf = any_lower_inf(keys);
    if (!result.lower_inf) {
        result.lower = min_lower_bound(keys);
        result.lower_inc = any_lower_inc_at_min(keys);
    }

    // Find maximum upper bound
    result.upper_inf = any_upper_inf(keys);
    if (!result.upper_inf) {
        result.upper = max_upper_bound(keys);
        result.upper_inc = any_upper_inc_at_max(keys);
    }

    return result;
}
```

#### 3. Penalty (range_gist_penalty)

Calculates the cost of adding a range to a subtree:

```cpp
float range_gist_penalty(
    const RangeGistKey& original,
    const RangeGistKey& new_key
) {
    RangeGistKey union_key = range_gist_union({original, new_key});

    // Penalty is the increase in range width
    float original_width = range_width(original);
    float union_width = range_width(union_key);

    return union_width - original_width;
}
```

#### 4. PickSplit (range_gist_picksplit)

Splits a node when it overflows:

```cpp
std::pair<std::vector<RangeGistKey>, std::vector<RangeGistKey>>
range_gist_picksplit(const std::vector<RangeGistKey>& keys) {
    // Use the "linear split" algorithm:
    // 1. Find the two ranges that are furthest apart
    // 2. Assign each range to the closer of the two seeds

    auto [seed1, seed2] = find_extremes(keys);

    std::vector<RangeGistKey> left, right;
    for (const auto& key : keys) {
        if (penalty(seed1, key) < penalty(seed2, key)) {
            left.push_back(key);
        } else {
            right.push_back(key);
        }
    }

    return {left, right};
}
```

#### 5. Same (range_gist_same)

Checks if two keys are identical:

```cpp
bool range_gist_same(
    const RangeGistKey& a,
    const RangeGistKey& b
) {
    return a.lower_inf == b.lower_inf &&
           a.upper_inf == b.upper_inf &&
           a.lower_inc == b.lower_inc &&
           a.upper_inc == b.upper_inc &&
           a.is_empty == b.is_empty &&
           (a.lower_inf || a.lower == b.lower) &&
           (a.upper_inf || a.upper == b.upper);
}
```

### Integration with Existing GiST Infrastructure

ScratchBird already has GiST index support for spatial types. Range GiST integration will:

1. **Extend GistKeyOps**: Add range-specific operations to the GiST operator class system
2. **Register Range Operator Classes**: Register each range type (INT4RANGE, DATERANGE, etc.) as a GiST opclass
3. **Query Planner Integration**: Teach the query planner to use GiST indexes for range predicates

## Implementation Plan

### Phase 6.1: Core GiST Methods (Estimated: 2-3 days)

1. Implement `RangeGistKey` structure
2. Implement the 5 core GiST methods:
   - `consistent`
   - `union`
   - `penalty`
   - `picksplit`
   - `same`
3. Add unit tests for each method

### Phase 6.2: Index Creation (Estimated: 1-2 days)

1. Add `USING GIST` parser support for range columns
2. Create GiST index metadata for range types
3. Add DDL tests for GiST range indexes

### Phase 6.3: Query Optimization (Estimated: 2-3 days)

1. Extend query planner to recognize range predicates
2. Add cost estimation for GiST range scans
3. Implement GiST scan execution for ranges
4. Add query performance tests

### Phase 6.4: Multi-Column Indexes (Estimated: 1-2 days)

1. Support composite GiST indexes with range columns
2. Add tests for multi-column range indexes

## Testing Strategy

### Unit Tests

1. Test each GiST method in isolation
2. Verify correct key computation for all range types
3. Test edge cases (empty ranges, unbounded ranges)

### Integration Tests

1. Create GiST indexes on range columns
2. Execute queries with range predicates
3. Verify index is used (via EXPLAIN)
4. Verify correct query results

### Performance Tests

1. Benchmark GiST vs sequential scan for various dataset sizes
2. Measure insertion overhead
3. Test scalability with large datasets (1M+ rows)

## Example Usage

```sql
-- Create table with range column
CREATE TABLE reservations (
    id INT PRIMARY KEY,
    room_id INT,
    booking_period DATERANGE
);

-- Create GiST index
CREATE INDEX idx_booking_period_gist
ON reservations USING GIST (booking_period);

-- Query overlapping reservations
SELECT * FROM reservations
WHERE booking_period && '[2023-12-20,2023-12-27)'::daterange;

-- Find rooms available in a period (no overlaps)
SELECT room_id FROM reservations
WHERE NOT (booking_period && '[2023-12-25,2023-12-31)'::daterange)
GROUP BY room_id;
```

## Dependencies

### Existing Infrastructure

- **GiST Framework**: Already implemented for spatial types (Task 9)
- **Range Types**: Core implementation complete (Phases 1-4)
- **Query Planner**: Basic index selection logic exists

### External Dependencies

None. Implementation uses only internal ScratchBird components.

## Performance Expectations

### Query Performance

| Dataset Size | Sequential Scan | GiST Scan | Speedup |
|--------------|----------------|-----------|---------|
| 1,000 rows   | 0.5ms          | 0.1ms     | 5x      |
| 10,000 rows  | 5ms            | 0.2ms     | 25x     |
| 100,000 rows | 50ms           | 0.5ms     | 100x    |
| 1,000,000 rows | 500ms        | 1ms       | 500x    |

### Storage Overhead

- Index size: ~2-3x the size of indexed range data
- Memory usage: ~100KB per 1000 index entries

## Future Enhancements

1. **SP-GiST Support**: Space-partitioned GiST for better performance on certain workloads
2. **BRIN Integration**: Block Range Indexes for very large tables
3. **Parallel Index Build**: Multi-threaded index construction
4. **Index-Only Scans**: Return results directly from index without table access

## References

1. PostgreSQL GiST Documentation: https://www.postgresql.org/docs/current/gist.html
2. "GiST: A Generalized Search Tree for Database Systems" (Hellerstein et al., 1995)
3. PostgreSQL `btree_gist` extension source code
4. ScratchBird Spatial GiST Implementation (Task 9)

## Conclusion

GiST index support for range types will provide significant performance improvements for range queries, making ScratchBird competitive with PostgreSQL for range-heavy workloads. The implementation leverages existing GiST infrastructure and requires approximately 6-10 days of development effort.

**Status**: Design Complete - Ready for Implementation
**Next Step**: Begin Phase 6.1 (Core GiST Methods) when range type usage warrants optimization

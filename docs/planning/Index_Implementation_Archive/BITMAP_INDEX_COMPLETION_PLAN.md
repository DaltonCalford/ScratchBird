# Bitmap Index - Implementation Completion Plan

**Project**: ScratchBird Database Engine
**Component**: Bitmap Index - Complete Remaining Features
**Status**: 80% Complete (Core bitmap operations functional, missing intersection/union/NOT)
**Estimated Effort**: 20-30 hours
**Priority**: MEDIUM (Basic bitmap works, missing logical operations)
**Created**: 2025-11-04

---

## ⚠️ CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules for Bitmap Index**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- All bitmap operations must respect xmin/xmax visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- NO PostgreSQL MVCC contamination
- Bitmap segments reference stable TIDs (never change)

**Reference**: `/MGA_RULES.md` Section 4 (Visibility Rules)

**If context is lost during compaction, re-read `/MGA_RULES.md` immediately.**

---

## Table of Contents

1. [Current Status](#1-current-status)
2. [Implementation Phases](#2-implementation-phases)
3. [Phase-by-Phase Tasks](#3-phase-by-phase-tasks)
4. [Progress Tracking](#4-progress-tracking)
5. [Risk Mitigation](#5-risk-mitigation)
6. [Total Effort Estimate](#6-total-effort-estimate)

---

## 1. Current Status

### 1.1 What Works (80% Complete)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/bitmap_index.cpp` (1,378 lines)

**Implemented Features**:
- ✅ Bitmap page structure (SBBitmapPage, SBBitmapSegment)
- ✅ Insert with bitmap segment creation
- ✅ Scan with basic bitmap retrieval
- ✅ MGA compliance (xmin/xmax on segments)
- ✅ Compression (run-length encoding for sparse bitmaps)
- ✅ Multi-value support (multiple key values per index)
- ✅ Root page allocation and initialization
- ✅ Segment management (add, find, iterate)

### 1.2 What's Missing (20% = 20-30 hours)

**Missing Feature 1**: Bitmap intersection (Lines 890-896)
- **Current**: Stub that logs operation but doesn't compute intersection
- **Required**: Bitwise AND of multiple bitmaps for multi-condition queries
- **Impact**: Cannot combine multiple index scans efficiently
- **Effort**: 6-8 hours

**Missing Feature 2**: Bitmap union (Lines 898-904)
- **Current**: Stub that logs operation but doesn't compute union
- **Required**: Bitwise OR of multiple bitmaps for OR queries
- **Impact**: Cannot combine results from multiple predicates
- **Effort**: 6-8 hours

**Missing Feature 3**: Bitmap NOT (Lines 906-912)
- **Current**: Stub that logs operation but doesn't compute complement
- **Required**: Bitwise NOT for negative conditions (WHERE NOT x)
- **Impact**: Cannot efficiently handle negation queries
- **Effort**: 4-6 hours

**Missing Feature 4**: Statistics (Lines 1290-1295)
- **Current**: Returns placeholder values (cardinality=0, selectivity=0.0)
- **Required**: Calculate actual cardinality and selectivity from bitmaps
- **Impact**: Query planner cannot estimate bitmap effectiveness
- **Effort**: 4-6 hours

### 1.3 Code Locations

**Reference File**: `/home/dcalford/CliWork/ScratchBird/docs/specifications/BITMAP_INDEX_COMPLETION_SPEC.md`

**Key Functions**:
- `BitmapIndex::intersect()` - Line 890 (STUB)
- `BitmapIndex::union_bitmaps()` - Line 898 (STUB)
- `BitmapIndex::negate()` - Line 906 (STUB)
- `BitmapIndex::getStats()` - Line 1290 (PLACEHOLDER)

---

## 2. Implementation Phases

### Phase 1: Bitmap Intersection (6-8 hours)
**Goal**: Implement bitwise AND for combining multiple bitmap scans

### Phase 2: Bitmap Union (6-8 hours)
**Goal**: Implement bitwise OR for combining multiple predicates

### Phase 3: Bitmap NOT (4-6 hours)
**Goal**: Implement bitwise NOT for negation queries

### Phase 4: Statistics Calculation (4-6 hours)
**Goal**: Calculate real cardinality and selectivity values

---

## 3. Phase-by-Phase Tasks

---

### PHASE 1: Bitmap Intersection (6-8 hours)

**Goal**: Implement `intersect()` for combining multiple bitmap scans with bitwise AND

**MGA Compliance**: Must handle compressed segments with xmin/xmax visibility

#### Task 1.1: Add Bitmap Decompression Helper (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/bitmap_index.cpp`

**What to Do**:
```cpp
// Add to bitmap_index.h private section:
Status decompress_segment(const SBBitmapSegment* segment,
                         std::vector<bool>* bitmap_out,
                         ErrorContext* ctx);

// Implementation:
// - Handle run-length encoded segments
// - Expand compressed runs to full bitmap
// - Validate segment integrity
```

**Acceptance Criteria**:
- [ ] Helper correctly expands RLE-encoded segments
- [ ] Handles sparse bitmaps (mostly zeros)
- [ ] Handles dense bitmaps (mostly ones)
- [ ] Validates segment boundaries

**Code Location**: Add after Line 860 in `bitmap_index.cpp`

**MGA Notes**: Decompression must respect segment xmin/xmax visibility

**Estimated Effort**: 2-3 hours

---

#### Task 1.2: Implement intersect() Core Logic (3-4 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/bitmap_index.cpp`

**What to Do**:
```cpp
Status BitmapIndex::intersect(const std::vector<BitmapScanResult>& bitmaps,
                              BitmapScanResult* result_out,
                              ErrorContext* ctx)
{
    // 1. Decompress all input bitmaps
    // 2. Find common bitmap size (max TID range)
    // 3. Perform bitwise AND across all bitmaps
    // 4. Compress result bitmap
    // 5. Store in result_out
}
```

**Acceptance Criteria**:
- [ ] Correctly computes bitwise AND of 2+ bitmaps
- [ ] Handles bitmaps of different sizes (zero-pad shorter ones)
- [ ] Result bitmap correctly compressed
- [ ] Handles empty intersection (all bits zero)

**Code Location**: Replace stub at Line 890

**MGA Notes**: Only include TIDs visible to current transaction

**Estimated Effort**: 3-4 hours

---

#### Task 1.3: Unit Tests for Intersection (1 hour)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_bitmap_intersection.cpp` (NEW)

**Test Cases**:
1. Intersect two bitmaps with overlapping TIDs
2. Intersect two bitmaps with no overlap (empty result)
3. Intersect three bitmaps (multi-way intersection)
4. Intersect bitmaps of different sizes
5. Intersect with compressed segments (RLE)

**Acceptance Criteria**:
- [ ] All 5 tests pass
- [ ] Result contains only common TIDs
- [ ] Empty intersection returns empty bitmap
- [ ] Compression preserved in result

**Estimated Effort**: 1 hour

---

### PHASE 2: Bitmap Union (6-8 hours)

**Goal**: Implement `union_bitmaps()` for OR queries with bitwise OR

**MGA Compliance**: Must merge segments respecting visibility

#### Task 2.1: Implement union_bitmaps() Core Logic (4-5 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/bitmap_index.cpp`

**What to Do**:
```cpp
Status BitmapIndex::union_bitmaps(const std::vector<BitmapScanResult>& bitmaps,
                                  BitmapScanResult* result_out,
                                  ErrorContext* ctx)
{
    // 1. Decompress all input bitmaps
    // 2. Find max bitmap size (max TID range)
    // 3. Perform bitwise OR across all bitmaps
    // 4. Compress result bitmap
    // 5. Store in result_out
}
```

**Acceptance Criteria**:
- [ ] Correctly computes bitwise OR of 2+ bitmaps
- [ ] Handles bitmaps of different sizes (zero-pad shorter ones)
- [ ] Result bitmap correctly compressed
- [ ] Handles empty union (all bitmaps empty)

**Code Location**: Replace stub at Line 898

**MGA Notes**: Union must include all TIDs visible to current transaction

**Estimated Effort**: 4-5 hours

---

#### Task 2.2: Optimize Union for Sparse Bitmaps (1-2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/bitmap_index.cpp`

**What to Do**:
- Detect when bitmaps are mostly empty
- Use sparse representation (set of TIDs) instead of full bitmap
- Only expand to full bitmap if density > 10%

**Acceptance Criteria**:
- [ ] Sparse union faster than dense union (benchmark)
- [ ] Sparse union produces correct results
- [ ] Automatic selection between sparse/dense

**Code Location**: Add helper at Line 920

**Estimated Effort**: 1-2 hours

---

#### Task 2.3: Unit Tests for Union (1 hour)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_bitmap_union.cpp` (NEW)

**Test Cases**:
1. Union two bitmaps with partial overlap
2. Union two bitmaps with no overlap (all TIDs included)
3. Union three bitmaps (multi-way union)
4. Union with empty bitmap (identity operation)
5. Union with compressed segments (RLE)

**Acceptance Criteria**:
- [ ] All 5 tests pass
- [ ] Result contains all unique TIDs
- [ ] Compression preserved in result
- [ ] Empty bitmap handled correctly

**Estimated Effort**: 1 hour

---

### PHASE 3: Bitmap NOT (4-6 hours)

**Goal**: Implement `negate()` for negation queries with bitwise NOT

**MGA Compliance**: NOT must respect table size and visibility

#### Task 3.1: Implement negate() Core Logic (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/bitmap_index.cpp`

**What to Do**:
```cpp
Status BitmapIndex::negate(const BitmapScanResult& bitmap,
                          uint64_t max_tid,
                          BitmapScanResult* result_out,
                          ErrorContext* ctx)
{
    // 1. Decompress input bitmap
    // 2. Resize to max_tid (table size)
    // 3. Flip all bits (0 → 1, 1 → 0)
    // 4. Compress result bitmap
    // 5. Store in result_out
}
```

**Acceptance Criteria**:
- [ ] Correctly computes bitwise NOT
- [ ] Result size matches table size (max_tid)
- [ ] Handles sparse bitmaps efficiently
- [ ] Result correctly compressed

**Code Location**: Replace stub at Line 906

**MGA Notes**: NOT must only include TIDs visible to current transaction

**Estimated Effort**: 2-3 hours

---

#### Task 3.2: Unit Tests for NOT (1-2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_bitmap_not.cpp` (NEW)

**Test Cases**:
1. Negate bitmap with 50% density
2. Negate sparse bitmap (< 1% density)
3. Negate dense bitmap (> 99% density)
4. Negate empty bitmap (all bits → 1)
5. Negate full bitmap (all bits → 0)

**Acceptance Criteria**:
- [ ] All 5 tests pass
- [ ] Result inverts all bits correctly
- [ ] Compression preserved in result
- [ ] Edge cases handled (empty, full)

**Estimated Effort**: 1-2 hours

---

#### Task 3.3: Integration Test: Combined Operations (1 hour)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_bitmap_combined.cpp` (NEW)

**Test Cases**:
1. (A AND B) OR C
2. (A OR B) AND NOT C
3. A AND (B OR C)
4. NOT (A AND B) = (NOT A) OR (NOT B) (De Morgan's law)

**Acceptance Criteria**:
- [ ] All 4 tests pass
- [ ] Combined operations produce correct results
- [ ] De Morgan's law verified
- [ ] Performance acceptable (< 50ms for 10K TIDs)

**Estimated Effort**: 1 hour

---

### PHASE 4: Statistics Calculation (4-6 hours)

**Goal**: Calculate real cardinality and selectivity from bitmap segments

**MGA Compliance**: Statistics must reflect visible TIDs only

#### Task 4.1: Implement Cardinality Calculation (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/bitmap_index.cpp`

**What to Do**:
```cpp
Status BitmapIndex::getStats(BitmapStats* stats_out, ErrorContext* ctx)
{
    // 1. Scan all segments in index
    // 2. Count total set bits across all bitmaps
    // 3. Calculate cardinality per key value
    // 4. Calculate overall cardinality
    // 5. Calculate selectivity = cardinality / table_size
}
```

**Acceptance Criteria**:
- [ ] Cardinality accurately counts set bits
- [ ] Selectivity correctly calculated (0.0 to 1.0)
- [ ] Handles compressed segments (RLE)
- [ ] Statistics updated in real-time

**Code Location**: Replace placeholder at Line 1290

**MGA Notes**: Only count TIDs visible to current transaction

**Estimated Effort**: 2-3 hours

---

#### Task 4.2: Add Statistics Caching (1-2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/bitmap_index.cpp`

**What to Do**:
- Cache calculated statistics in index metadata
- Invalidate cache on insert/delete
- Refresh cache periodically (e.g., every 1000 operations)

**Acceptance Criteria**:
- [ ] Cache reduces getStats() latency (benchmark)
- [ ] Cache invalidated correctly on modifications
- [ ] Cached values match freshly calculated values

**Code Location**: Add cache fields to SBBitmapIndex structure

**Estimated Effort**: 1-2 hours

---

#### Task 4.3: Unit Tests for Statistics (1 hour)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_bitmap_stats.cpp` (NEW)

**Test Cases**:
1. Insert 100 keys, verify cardinality = 100
2. Insert duplicate keys, verify cardinality unchanged
3. Calculate selectivity for sparse bitmap (< 10%)
4. Calculate selectivity for dense bitmap (> 90%)
5. Verify statistics cache invalidation

**Acceptance Criteria**:
- [ ] All 5 tests pass
- [ ] Cardinality matches expected values
- [ ] Selectivity within expected range
- [ ] Cache behavior correct

**Estimated Effort**: 1 hour

---

## 4. Progress Tracking

### Overall Completion Checklist

**Phase 1: Bitmap Intersection (6-8 hours)**
- [ ] Task 1.1: Add bitmap decompression helper (2-3h)
- [ ] Task 1.2: Implement intersect() core logic (3-4h)
- [ ] Task 1.3: Unit tests for intersection (1h)

**Phase 2: Bitmap Union (6-8 hours)**
- [ ] Task 2.1: Implement union_bitmaps() core logic (4-5h)
- [ ] Task 2.2: Optimize union for sparse bitmaps (1-2h)
- [ ] Task 2.3: Unit tests for union (1h)

**Phase 3: Bitmap NOT (4-6 hours)**
- [ ] Task 3.1: Implement negate() core logic (2-3h)
- [ ] Task 3.2: Unit tests for NOT (1-2h)
- [ ] Task 3.3: Integration test: combined operations (1h)

**Phase 4: Statistics Calculation (4-6 hours)**
- [ ] Task 4.1: Implement cardinality calculation (2-3h)
- [ ] Task 4.2: Add statistics caching (1-2h)
- [ ] Task 4.3: Unit tests for statistics (1h)

### Testing Checklist

**Unit Tests**:
- [ ] test_bitmap_intersection.cpp (5 tests)
- [ ] test_bitmap_union.cpp (5 tests)
- [ ] test_bitmap_not.cpp (5 tests)
- [ ] test_bitmap_stats.cpp (5 tests)

**Integration Tests**:
- [ ] test_bitmap_combined.cpp (4 tests)

**Total**: 24 new tests

### MGA Compliance Checklist

- [x] Current implementation uses TransactionId (uint64_t)
- [x] Bitmap segments have xmin/xmax fields
- [ ] intersect() respects visibility
- [ ] union_bitmaps() respects visibility
- [ ] negate() respects visibility
- [ ] Statistics calculation respects visibility
- [ ] No Snapshot structures used

---

## 5. Risk Mitigation

### 5.1 Technical Risks

**Risk 1: Bitmap Size Explosion**
- **Problem**: Large bitmaps may not fit in memory
- **Mitigation**: Use compression (RLE) aggressively, stream processing
- **Severity**: MEDIUM

**Risk 2: Performance Degradation**
- **Problem**: Bitwise operations may be slow for large bitmaps
- **Mitigation**: Use SIMD instructions (AVX2), optimize sparse bitmaps
- **Severity**: LOW (already using compression)

**Risk 3: Memory Fragmentation**
- **Problem**: Many small bitmap allocations
- **Mitigation**: Use buffer pool, pre-allocate bitmap buffers
- **Severity**: LOW

### 5.2 MGA Compliance Risks

**Risk**: Forgetting to check visibility during bitmap operations
- **Mitigation**: Mandatory visibility checks in all operations, unit tests verify
- **Prevention**: Re-read `/MGA_RULES.md` before each phase

### 5.3 Testing Risks

**Risk**: Edge cases not covered by tests
- **Mitigation**: Add fuzz testing for random bitmap combinations
- **Prevention**: Review test coverage with code coverage tools

---

## 6. Total Effort Estimate

### 6.1 Effort Breakdown

| Phase | Tasks | Hours (Min-Max) | Hours (Realistic) |
|-------|-------|-----------------|-------------------|
| Phase 1: Bitmap Intersection | 3 | 6-8 | 7 |
| Phase 2: Bitmap Union | 3 | 6-8 | 7 |
| Phase 3: Bitmap NOT | 3 | 4-6 | 5 |
| Phase 4: Statistics | 3 | 4-6 | 5 |
| **TOTAL** | **12** | **20-28** | **24** |

**Buffer for debugging/edge cases**: +6 hours
**TOTAL WITH BUFFER**: 20-30 hours

### 6.2 Timeline Estimates

**Single Developer (Full-Time)**:
- Optimistic: 3-4 days
- Realistic: 4-5 days
- Conservative: 5-6 days

**Part-Time Development**:
- Realistic: 1-2 weeks

### 6.3 Critical Path

**Longest Dependency Chain**:
1. Phase 1 (Intersection) → 6-8 hours
2. Phase 2 (Union) → 6-8 hours (can parallelize with Phase 1)
3. Phase 3 (NOT) → 4-6 hours (depends on Phase 1)
4. Phase 4 (Statistics) → 4-6 hours (independent, can parallelize)

**Recommended Order**:
1. Phase 1 (Intersection) - Most critical for query optimization
2. Phase 2 (Union) - Enables OR queries
3. Phase 3 (NOT) - Completes logical operations
4. Phase 4 (Statistics) - Query planner optimization

---

## 7. Success Criteria

### 7.1 Functional Completion

**Must Have**:
- [ ] All 24 tests pass
- [ ] intersect() produces correct results
- [ ] union_bitmaps() produces correct results
- [ ] negate() produces correct results
- [ ] Statistics accurate (cardinality, selectivity)

### 7.2 Performance Targets

**Must Achieve**:
- [ ] Intersection: < 10ms for 10K TIDs per bitmap
- [ ] Union: < 10ms for 10K TIDs per bitmap
- [ ] NOT: < 5ms for 10K TIDs
- [ ] Combined operations: < 50ms for complex queries
- [ ] Statistics calculation: < 100ms for 100K TIDs

### 7.3 MGA Compliance

**Must Verify**:
- [ ] All operations respect xmin/xmax visibility
- [ ] No Snapshot structures used
- [ ] Visibility checks via TransactionManager::isVersionVisible()
- [ ] Deleted segments invisible to new transactions
- [ ] Deleted segments visible to old transactions

---

## 8. References

**Specification**: `/home/dcalford/CliWork/ScratchBird/docs/specifications/BITMAP_INDEX_COMPLETION_SPEC.md`
**Implementation**: `/home/dcalford/CliWork/ScratchBird/src/core/bitmap_index.cpp` (1,378 lines)
**MGA Rules**: `/home/dcalford/CliWork/ScratchBird/MGA_RULES.md`
**Project Context**: `/home/dcalford/CliWork/ScratchBird/PROJECT_CONTEXT.md`

---

**Document Version**: 1.0
**Created**: 2025-11-04
**Status**: READY FOR IMPLEMENTATION
**Next Action**: Begin Phase 1 (Bitmap Intersection)

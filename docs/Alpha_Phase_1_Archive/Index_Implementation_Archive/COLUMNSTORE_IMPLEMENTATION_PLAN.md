# Columnstore Index - Detailed Implementation Plan

**Project**: ScratchBird Database Engine
**Component**: Columnstore Index (Column-Oriented Storage)
**Status**: PLANNING COMPLETE - IMPLEMENTATION PENDING
**Estimated Effort**: 140-180 hours
**Specification**: `/docs/specifications/COLUMNSTORE_SPEC.md`

---

## ⚠️ CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules for Columnstore**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- Every segment has `xmin`/`xmax` for MGA visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- Soft delete: Set `xmax`, physical removal via VACUUM/garbage collection
- NO PostgreSQL MVCC contamination

**MGA References in Code**:
```cpp
// CORRECT: Firebird MGA
bool isSegmentVisible(uint64_t segment_xmin, uint64_t segment_xmax,
                      uint64_t current_xid, TransactionManager* txn_mgr);

// WRONG: PostgreSQL MVCC (DO NOT USE)
bool isSegmentVisible(Snapshot snapshot, ...);  // ❌ WRONG!
```

**If context is lost during compaction, re-read `/MGA_RULES.md` immediately.**

---

## Table of Contents

1. [Overview](#1-overview)
2. [Phase 1: RLE Compression (20-30 hours)](#2-phase-1-rle-compression-20-30-hours)
3. [Phase 2: Dictionary Encoding (30-40 hours)](#3-phase-2-dictionary-encoding-30-40-hours)
4. [Phase 3: Bit-Packing (20-30 hours)](#4-phase-3-bit-packing-20-30-hours)
5. [Phase 4: Predicate Pushdown (30-40 hours)](#5-phase-4-predicate-pushdown-30-40-hours)
6. [Phase 5: Batch Processing (20-30 hours)](#6-phase-5-batch-processing-20-30-hours)
7. [Phase 6: Segment Management (30-40 hours)](#7-phase-6-segment-management-30-40-hours)
8. [Phase 7: Testing & Optimization (20-30 hours)](#8-phase-7-testing--optimization-20-30-hours)
9. [Progress Tracking](#9-progress-tracking)
10. [Risk Mitigation](#10-risk-mitigation)

---

## 1. Overview

### 1.1 Current Status

**Existing Infrastructure**:
- ✅ `include/scratchbird/core/columnstore.h` (378 lines) - Page format and API
- ✅ `src/core/columnstore.cpp` (398 lines) - Method stubs (ALL EMPTY)
- ✅ `SBColumnstorePage` structure defined (ondisk.h)
- ✅ Build system configured

**Completion Status**:
- ❌ NOT COMPLETE - All methods are stubs returning `Status::OK`
- ❌ Does NOT count toward project completion percentage
- ❌ Blocks Alpha Phase 1 completion

### 1.2 Implementation Strategy

**Approach**: Implement compression algorithms sequentially, with complete testing at each phase.

**Order**:
1. RLE compression (simplest, works for all types)
2. Dictionary encoding (strings, low-cardinality)
3. Bit-packing (integers with small range)
4. Predicate pushdown (min/max pruning, compressed evaluation)
5. Batch processing (vectorized scans)
6. Segment management (compaction, garbage collection)
7. Testing & optimization

**Each phase must**:
- Complete implementation (NO stubs)
- Pass unit tests (100% coverage)
- Pass integration tests
- Document performance characteristics

---

## 2. Phase 1: RLE Compression (20-30 hours)

### 2.1 Overview

Implement Run-Length Encoding for repeated values.

**Algorithm**: Encode consecutive identical values as `(value, count)` pairs.

**Example**:
- Input: `[1, 1, 1, 2, 2, 3, 3, 3, 3]`
- Output: `[(1, 3), (2, 2), (3, 4)]`

### 2.2 Tasks

#### Task 2.1: RLE Compression Implementation (6-8 hours)

**File**: `src/core/columnstore.cpp` (method: `compressRLE`)

**Requirements**:
- Scan through values, count consecutive identical values
- Encode as `(value, count)` pairs
- Support all integer types (INT8, INT16, INT32, INT64)
- Handle NULL values (separate NULL bitmap)
- Return compressed size

**Acceptance Criteria**:
- [ ] All integer types compressed correctly
- [ ] NULL values preserved in separate bitmap
- [ ] Compression ratio calculated correctly
- [ ] Handles empty input
- [ ] Handles all-NULL input
- [ ] Handles single value repeated N times (best case)
- [ ] Handles all different values (worst case)

**Code Reference** (see `/docs/specifications/COLUMNSTORE_SPEC.md` Section 2.2):
```cpp
Status compressRLE(const std::vector<int64_t>& values,
                   std::vector<uint8_t>* compressed);
```

**MGA Notes**: Compression is MGA-agnostic (operates on raw values).

---

#### Task 2.2: RLE Decompression Implementation (4-6 hours)

**File**: `src/core/columnstore.cpp` (method: `decompressRLE`)

**Requirements**:
- Read `(value, count)` pairs from compressed data
- Expand each run by repeating value `count` times
- Reconstruct NULL bitmap
- Support all integer types

**Acceptance Criteria**:
- [ ] Decompressed values match original input
- [ ] NULL flags preserved correctly
- [ ] Handles corrupted compressed data (return error)
- [ ] Performance: O(n) where n = number of output values

**Code Reference** (see `/docs/specifications/COLUMNSTORE_SPEC.md` Section 2.3).

---

#### Task 2.3: Unit Tests for RLE (6-8 hours)

**File**: `tests/unit/test_columnstore_rle.cpp` (create new)

**Test Cases**:
1. [ ] Compress and decompress integers (INT32)
2. [ ] Compress and decompress big integers (INT64)
3. [ ] Best case: All same value (1000x compression)
4. [ ] Worst case: All different values (1.2x overhead)
5. [ ] NULL value handling
6. [ ] Empty input
7. [ ] Single value
8. [ ] Alternating values (1, 2, 1, 2, ...)
9. [ ] Large dataset (1M values)
10. [ ] Round-trip: compress → decompress → verify identical

**Command to run tests**:
```bash
make test_columnstore_rle
./tests/test_columnstore_rle
```

**Acceptance Criteria**: All 10 tests pass with 100% code coverage.

---

#### Task 2.4: Integration with Insert/Scan (4-6 hours)

**File**: `src/core/columnstore.cpp` (methods: `insert`, `scan`)

**Requirements**:
- Buffer values in memory until segment is full (1024 rows)
- Compress buffered values using `compressRLE()`
- Write compressed segment to new page
- On scan, decompress segment using `decompressRLE()`
- Apply MGA visibility filtering

**Acceptance Criteria**:
- [ ] Insert 1024 values, verify segment created
- [ ] Insert 2048 values, verify 2 segments created
- [ ] Scan returns correct values in correct order
- [ ] MGA visibility filtering works (test with xmin/xmax)

**MGA Reference**: See `/MGA_RULES.md` Section 4 (Visibility Rules).

**Code Example**:
```cpp
// Insert with MGA compliance
Status insert(const ID& column_uuid, uint64_t tid,
              const void* value, size_t value_len, bool is_null,
              ErrorContext* ctx) {
    uint64_t xmin = db_->transaction_manager()->getCurrentXid();

    // Buffer value
    buffer_.push_back({value, is_null, xmin, 0});  // xmax = 0 (active)

    // If buffer full (1024 rows), flush to segment
    if (buffer_.size() >= 1024) {
        Status s = flushSegment(column_uuid, ctx);
        if (!s.ok()) return s;
    }
    return Status::OK;
}
```

---

**Phase 1 Total**: 20-28 hours

**Milestone**: RLE compression fully implemented and tested.

---

## 3. Phase 2: Dictionary Encoding (30-40 hours)

### 3.1 Overview

Implement dictionary encoding for string columns and low-cardinality integer columns.

**Algorithm**: Build dictionary of unique values, replace values with integer codes.

**Example**:
- Input: `["Alice", "Bob", "Alice", "Carol", "Bob"]`
- Dictionary: `{0: "Alice", 1: "Bob", 2: "Carol"}`
- Output: `[0, 1, 0, 2, 1]` (compressed)

### 3.2 Tasks

#### Task 3.1: Dictionary Builder (8-10 hours)

**File**: `src/core/columnstore_dict.cpp` (create new)

**Requirements**:
- Build dictionary from input values (unique values only)
- Assign integer codes (0, 1, 2, ...)
- Support strings (VARCHAR, TEXT)
- Support low-cardinality integers (detect when beneficial)
- Serialize dictionary to disk

**Acceptance Criteria**:
- [ ] Handles duplicate values correctly
- [ ] Assigns sequential codes (0, 1, 2, ...)
- [ ] Supports up to 65,536 unique values (uint16_t codes)
- [ ] Detects when dictionary encoding is beneficial (cardinality < 10% of rows)
- [ ] Serializes/deserializes dictionary correctly

**Code Reference** (see `/docs/specifications/COLUMNSTORE_SPEC.md` Section 3.2).

---

#### Task 3.2: Dictionary Compression (8-10 hours)

**File**: `src/core/columnstore_dict.cpp` (method: `compressDictionary`)

**Requirements**:
- Replace values with dictionary codes
- Store dictionary separately (at segment start)
- Handle NULL values (special code)
- Support variable-length strings

**Acceptance Criteria**:
- [ ] Compression ratio calculated correctly
- [ ] Dictionary stored at beginning of segment
- [ ] Codes stored compactly (uint8_t or uint16_t depending on dictionary size)
- [ ] NULL values handled correctly

---

#### Task 3.3: Dictionary Decompression (6-8 hours)

**File**: `src/core/columnstore_dict.cpp` (method: `decompressDictionary`)

**Requirements**:
- Read dictionary from segment
- Replace codes with original values
- Reconstruct strings from dictionary

**Acceptance Criteria**:
- [ ] Decompressed values match original input
- [ ] Handles missing dictionary (return error)
- [ ] Handles invalid codes (return error)

---

#### Task 3.4: Unit Tests for Dictionary (6-8 hours)

**File**: `tests/unit/test_columnstore_dict.cpp` (create new)

**Test Cases**:
1. [ ] Build dictionary from strings
2. [ ] Compress and decompress strings
3. [ ] Low-cardinality integers (10 unique values in 1000 rows)
4. [ ] High-cardinality detection (don't use dictionary)
5. [ ] NULL value handling
6. [ ] Dictionary with 256 values (uint8_t codes)
7. [ ] Dictionary with 65,536 values (uint16_t codes)
8. [ ] Large dataset (100K strings, 1K unique)

**Acceptance Criteria**: All 8 tests pass.

---

**Phase 2 Total**: 28-36 hours

**Milestone**: Dictionary encoding fully implemented and tested.

---

## 4. Phase 3: Bit-Packing (20-30 hours)

### 3.3 Overview

Implement bit-packing for integer columns with small value range.

**Algorithm**: Minimize bits per value based on max value.

**Example**:
- Input: `[5, 7, 3, 6, 4]` (max = 7, requires 3 bits)
- Output: `101 111 011 110 100` (15 bits instead of 160 bits for int32_t)

### 3.4 Tasks

#### Task 4.1: Bit-Packing Compression (8-10 hours)

**File**: `src/core/columnstore_bitpack.cpp` (create new)

**Requirements**:
- Calculate min/max values in segment
- Determine bits needed: `ceil(log2(max - min + 1))`
- Pack values into bit array
- Handle NULL values (separate bitmap)

**Acceptance Criteria**:
- [ ] Correctly calculates bits needed
- [ ] Packs values tightly (no wasted bits)
- [ ] Handles negative values (subtract min value)
- [ ] Handles NULL values

**Code Reference** (see `/docs/specifications/COLUMNSTORE_SPEC.md` Section 4.2).

---

#### Task 4.2: Bit-Packing Decompression (6-8 hours)

**File**: `src/core/columnstore_bitpack.cpp` (method: `decompressBitpack`)

**Requirements**:
- Read bit width from segment header
- Unpack values from bit array
- Add min value back (if subtracted)

**Acceptance Criteria**:
- [ ] Decompressed values match original input
- [ ] Handles all bit widths (1-64 bits)
- [ ] Performance: O(n) where n = number of values

---

#### Task 4.3: Unit Tests for Bit-Packing (4-6 hours)

**File**: `tests/unit/test_columnstore_bitpack.cpp` (create new)

**Test Cases**:
1. [ ] Small range (0-7, 3 bits)
2. [ ] Large range (0-1000000, 20 bits)
3. [ ] Negative values (-100 to 100)
4. [ ] NULL value handling
5. [ ] Best case: All values in range 0-1 (1 bit)
6. [ ] Worst case: Full int32_t range (32 bits)
7. [ ] Large dataset (1M values)

**Acceptance Criteria**: All 7 tests pass.

---

**Phase 3 Total**: 18-24 hours

**Milestone**: Bit-packing fully implemented and tested.

---

## 5. Phase 4: Predicate Pushdown (30-40 hours)

### 5.1 Overview

Implement predicate pushdown: filter data at the segment level BEFORE decompression.

**Optimizations**:
1. **Min/Max Pruning**: Skip segments where `segment.max < predicate.value`
2. **Compressed Evaluation**: Evaluate predicates on compressed data (RLE runs)
3. **Batch Filtering**: Apply predicates to 1024 values at once

### 5.2 Tasks

#### Task 5.1: Min/Max Segment Pruning (6-8 hours)

**File**: `src/core/columnstore.cpp` (method: `scan`)

**Requirements**:
- Read `cs_min_value` and `cs_max_value` from segment header
- Skip segment if predicate cannot match
- Examples:
  - Predicate: `age > 50`, Segment: `min=20, max=40` → Skip
  - Predicate: `age = 25`, Segment: `min=20, max=40` → Scan

**Acceptance Criteria**:
- [ ] Correctly skips segments based on min/max
- [ ] Handles all predicates (=, !=, <, >, <=, >=)
- [ ] Handles NULL predicates (IS NULL, IS NOT NULL)
- [ ] Performance: O(1) per segment (no decompression)

**Code Example**:
```cpp
Status scan(const ID& column_uuid, const ColumnPredicate* predicate,
            uint64_t current_xid, ColumnScanBatch* batch_out, ErrorContext* ctx) {
    for (uint32_t segment_page : segment_chain_) {
        SBColumnstorePage* seg = pinSegment(segment_page);

        // Min/Max pruning
        if (canSkipSegment(seg, predicate)) {
            unpinSegment(segment_page);
            continue;  // Skip segment!
        }

        // Decompress and scan...
    }
}
```

---

#### Task 5.2: Compressed RLE Evaluation (10-12 hours)

**File**: `src/core/columnstore.cpp` (method: `evaluatePredicateRLE`)

**Requirements**:
- Evaluate predicates on RLE runs WITHOUT decompressing
- Example:
  - RLE run: `(value=5, count=1000)`
  - Predicate: `value > 3`
  - Result: Add 1000 rows to result set (no decompression!)

**Acceptance Criteria**:
- [ ] Correctly evaluates predicates on runs
- [ ] Handles all predicates
- [ ] Avoids decompression when possible
- [ ] Performance: O(R) where R = number of runs (not number of values)

**Code Reference** (see `/docs/specifications/COLUMNSTORE_SPEC.md` Section 5.3).

---

#### Task 5.3: Batch Predicate Evaluation (8-10 hours)

**File**: `src/core/columnstore.cpp` (method: `applyPredicate`)

**Requirements**:
- Apply predicate to 1024 values at once (batch processing)
- Use SIMD instructions if available (optional)
- Return bitmap of matching offsets

**Acceptance Criteria**:
- [ ] Processes 1024 values per batch
- [ ] Returns correct matching offsets
- [ ] Handles partial batches (last batch < 1024 values)
- [ ] Performance: 10x faster than row-by-row evaluation

---

#### Task 5.4: MGA Visibility Filtering (8-10 hours)

**File**: `src/core/columnstore.cpp` (method: `isValueVisible`)

**Requirements**:
- Check MGA visibility for each segment
- Use `TransactionManager::isVersionVisible()` for TIP-based visibility
- Filter invisible values BEFORE decompression

**Acceptance Criteria**:
- [ ] Correctly filters based on xmin/xmax
- [ ] Uses TIP-based visibility (Firebird rules)
- [ ] NO PostgreSQL MVCC (verify no Snapshot usage)
- [ ] Performance: O(1) per segment (check segment xmin/xmax)

**MGA Reference**: See `/MGA_RULES.md` Section 4.

**Code Example**:
```cpp
bool isSegmentVisible(const SBColumnstorePage* segment,
                      uint64_t current_xid,
                      TransactionManager* txn_mgr) {
    // Firebird MGA rules (NOT PostgreSQL MVCC!)
    if (segment->cs_xmin > current_xid) return false;
    if (segment->cs_xmax != 0 && segment->cs_xmax <= current_xid) return false;

    // TIP-based visibility check
    if (!txn_mgr->isVersionVisible(segment->cs_xmin, current_xid)) return false;
    if (segment->cs_xmax != 0 &&
        txn_mgr->isVersionVisible(segment->cs_xmax, current_xid)) {
        return false;
    }

    return true;
}
```

---

#### Task 5.5: Unit Tests for Predicate Pushdown (6-8 hours)

**File**: `tests/unit/test_columnstore_predicate.cpp` (create new)

**Test Cases**:
1. [ ] Min/Max pruning skips segment
2. [ ] RLE compressed evaluation (no decompression)
3. [ ] Batch predicate evaluation (1024 values)
4. [ ] MGA visibility filtering (xmin/xmax)
5. [ ] IS NULL predicate
6. [ ] Range predicate (age BETWEEN 20 AND 40)
7. [ ] Complex predicate (age > 30 AND age < 50)

**Acceptance Criteria**: All 7 tests pass.

---

**Phase 4 Total**: 38-48 hours

**Milestone**: Predicate pushdown fully implemented and tested.

---

## 6. Phase 5: Batch Processing (20-30 hours)

### 6.1 Overview

Implement batch processing for efficient column scans (1024 values at a time).

### 6.2 Tasks

#### Task 6.1: Batch Scan Implementation (10-12 hours)

**File**: `src/core/columnstore.cpp` (method: `scan`)

**Requirements**:
- Scan segments in order
- Decompress 1024 values at a time
- Apply predicate to batch
- Apply MGA visibility filtering
- Return batch when full or scan complete

**Acceptance Criteria**:
- [ ] Returns batches of up to 1024 values
- [ ] Handles partial batches (last batch)
- [ ] Applies predicate and visibility filtering
- [ ] Maintains segment order

**Code Reference** (see `/docs/specifications/COLUMNSTORE_SPEC.md` Section 6).

---

#### Task 6.2: Batch Result Reconstruction (6-8 hours)

**File**: `src/core/columnstore.cpp`

**Requirements**:
- Reconstruct row values from column batches
- Combine multiple column scans (for multi-column queries)
- Handle NULL values correctly

**Acceptance Criteria**:
- [ ] Correctly reconstructs rows from columns
- [ ] Handles multi-column queries
- [ ] NULL values preserved

---

#### Task 6.3: Unit Tests for Batch Processing (4-6 hours)

**File**: `tests/unit/test_columnstore_batch.cpp` (create new)

**Test Cases**:
1. [ ] Scan 1024 values (full batch)
2. [ ] Scan 500 values (partial batch)
3. [ ] Scan 2048 values (2 batches)
4. [ ] Scan with predicate filtering
5. [ ] Multi-column scan and reconstruction

**Acceptance Criteria**: All 5 tests pass.

---

**Phase 5 Total**: 20-26 hours

**Milestone**: Batch processing fully implemented and tested.

---

## 7. Phase 6: Segment Management (30-40 hours)

### 7.1 Overview

Implement segment chain management, compaction, and garbage collection.

### 7.2 Tasks

#### Task 7.1: Segment Chain Traversal (6-8 hours)

**File**: `src/core/columnstore.cpp` (method: `findSegment`)

**Requirements**:
- Traverse segment chain using `cs_next_segment` pointers
- Find segment containing specific TID
- Handle forward and backward traversal

**Acceptance Criteria**:
- [ ] Correctly traverses chain
- [ ] Finds segment containing TID
- [ ] Handles empty chain
- [ ] Handles broken chain (return error)

---

#### Task 7.2: Segment Creation (8-10 hours)

**File**: `src/core/columnstore.cpp` (method: `createSegment`)

**Requirements**:
- Compress buffered values
- Allocate new page via `PageManager`
- Write compressed data to page
- Update min/max values
- Set xmin for MGA
- Link to previous segment

**Acceptance Criteria**:
- [ ] Segment created correctly
- [ ] Compressed data written
- [ ] Min/max values calculated
- [ ] MGA fields set (xmin, xmax, lsn)
- [ ] Linked to previous segment

**MGA Reference**: See `/MGA_RULES.md` Section 3 (xmin/xmax).

---

#### Task 7.3: Segment Compaction (10-12 hours)

**File**: `src/core/columnstore.cpp` (method: `compactSegments`)

**Requirements**:
- Merge multiple segments into one
- Remove deleted values (xmax != 0)
- Remove invisible values (garbage collection)
- Recompress merged data
- Update segment chain pointers

**Acceptance Criteria**:
- [ ] Segments merged correctly
- [ ] Deleted values removed
- [ ] Garbage collection works (Firebird rules)
- [ ] Segment chain updated atomically

**MGA Reference**: See `/MGA_RULES.md` Section 6 (Garbage Collection).

---

#### Task 7.4: Garbage Collection (8-10 hours)

**File**: `src/core/columnstore.cpp` (method: `garbageCollect`)

**Requirements**:
- Identify invisible values (xmin/xmax committed, no active transactions can see)
- Remove invisible values during compaction
- Update statistics (reclaimed space)

**Acceptance Criteria**:
- [ ] Correctly identifies invisible values
- [ ] Removes values safely (Firebird rules)
- [ ] NO PostgreSQL MVCC (verify no Snapshot usage)
- [ ] Statistics updated

**MGA Reference**: See `/MGA_RULES.md` Section 6.

---

#### Task 7.5: Unit Tests for Segment Management (4-6 hours)

**File**: `tests/unit/test_columnstore_segment.cpp` (create new)

**Test Cases**:
1. [ ] Segment chain traversal
2. [ ] Segment creation and linking
3. [ ] Segment compaction (merge 3 segments)
4. [ ] Garbage collection (remove old versions)
5. [ ] Concurrent compaction (thread safety)

**Acceptance Criteria**: All 5 tests pass.

---

**Phase 6 Total**: 36-46 hours

**Milestone**: Segment management fully implemented and tested.

---

## 8. Phase 7: Testing & Optimization (20-30 hours)

### 8.1 Tasks

#### Task 8.1: Integration Tests (8-10 hours)

**File**: `tests/integration/test_columnstore_integration.cpp` (create new)

**Test Cases**:
1. [ ] Insert 100K values, verify all readable
2. [ ] Scan with predicates (filter 90% of data)
3. [ ] Multi-column queries (reconstruct rows)
4. [ ] MGA isolation (concurrent transactions)
5. [ ] Segment compaction under load
6. [ ] Garbage collection after deletes
7. [ ] Crash recovery (verify durability)

**Acceptance Criteria**: All 7 tests pass.

---

#### Task 8.2: Performance Benchmarks (6-8 hours)

**File**: `tests/benchmark/benchmark_columnstore.cpp` (create new)

**Benchmarks**:
1. [ ] Insert throughput (rows/sec)
2. [ ] Scan throughput (rows/sec)
3. [ ] Compression ratio (different data distributions)
4. [ ] Predicate pushdown effectiveness (% segments skipped)
5. [ ] Batch processing speedup (vs row-by-row)

**Acceptance Criteria**:
- Insert: 50K-200K rows/sec
- Scan: 100K-500K rows/sec
- Compression: 5x-10x for low-cardinality data
- Predicate pushdown: 80%+ segments skipped
- Batch speedup: 10x-50x vs row-by-row

---

#### Task 8.3: Memory Profiling (4-6 hours)

**Requirements**:
- Detect memory leaks (use valgrind)
- Optimize memory usage (reduce allocations)
- Verify no memory leaks in compression/decompression

**Acceptance Criteria**:
- [ ] No memory leaks detected
- [ ] Memory usage within budget (<100 MB for 1M rows)

---

#### Task 8.4: Documentation (2-4 hours)

**Files**:
- Update `/docs/status/COLUMNSTORE_COMPLETION_REPORT_2025-11-03.md`
- Update `/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md` (mark complete)

**Acceptance Criteria**:
- [ ] Completion report updated with actual implementation
- [ ] Master plan updated (92% completion)

---

**Phase 7 Total**: 20-28 hours

**Milestone**: Columnstore fully tested, optimized, and documented.

---

## 9. Progress Tracking

### 9.1 Completion Checklist

**Phase 1: RLE Compression** (20-30 hours)
- [ ] Task 2.1: RLE Compression Implementation (6-8 hours)
- [ ] Task 2.2: RLE Decompression Implementation (4-6 hours)
- [ ] Task 2.3: Unit Tests for RLE (6-8 hours)
- [ ] Task 2.4: Integration with Insert/Scan (4-6 hours)

**Phase 2: Dictionary Encoding** (30-40 hours)
- [ ] Task 3.1: Dictionary Builder (8-10 hours)
- [ ] Task 3.2: Dictionary Compression (8-10 hours)
- [ ] Task 3.3: Dictionary Decompression (6-8 hours)
- [ ] Task 3.4: Unit Tests for Dictionary (6-8 hours)

**Phase 3: Bit-Packing** (20-30 hours)
- [ ] Task 4.1: Bit-Packing Compression (8-10 hours)
- [ ] Task 4.2: Bit-Packing Decompression (6-8 hours)
- [ ] Task 4.3: Unit Tests for Bit-Packing (4-6 hours)

**Phase 4: Predicate Pushdown** (30-40 hours)
- [ ] Task 5.1: Min/Max Segment Pruning (6-8 hours)
- [ ] Task 5.2: Compressed RLE Evaluation (10-12 hours)
- [ ] Task 5.3: Batch Predicate Evaluation (8-10 hours)
- [ ] Task 5.4: MGA Visibility Filtering (8-10 hours)
- [ ] Task 5.5: Unit Tests for Predicate Pushdown (6-8 hours)

**Phase 5: Batch Processing** (20-30 hours)
- [ ] Task 6.1: Batch Scan Implementation (10-12 hours)
- [ ] Task 6.2: Batch Result Reconstruction (6-8 hours)
- [ ] Task 6.3: Unit Tests for Batch Processing (4-6 hours)

**Phase 6: Segment Management** (30-40 hours)
- [ ] Task 7.1: Segment Chain Traversal (6-8 hours)
- [ ] Task 7.2: Segment Creation (8-10 hours)
- [ ] Task 7.3: Segment Compaction (10-12 hours)
- [ ] Task 7.4: Garbage Collection (8-10 hours)
- [ ] Task 7.5: Unit Tests for Segment Management (4-6 hours)

**Phase 7: Testing & Optimization** (20-30 hours)
- [ ] Task 8.1: Integration Tests (8-10 hours)
- [ ] Task 8.2: Performance Benchmarks (6-8 hours)
- [ ] Task 8.3: Memory Profiling (4-6 hours)
- [ ] Task 8.4: Documentation (2-4 hours)

### 9.2 Estimated Total Effort

| Phase | Minimum | Maximum |
|-------|---------|---------|
| Phase 1: RLE Compression | 20 hours | 30 hours |
| Phase 2: Dictionary Encoding | 30 hours | 40 hours |
| Phase 3: Bit-Packing | 20 hours | 30 hours |
| Phase 4: Predicate Pushdown | 30 hours | 40 hours |
| Phase 5: Batch Processing | 20 hours | 30 hours |
| Phase 6: Segment Management | 30 hours | 40 hours |
| Phase 7: Testing & Optimization | 20 hours | 30 hours |
| **TOTAL** | **170 hours** | **240 hours** |

**Realistic Estimate**: 140-180 hours (using existing infrastructure).

---

## 10. Risk Mitigation

### 10.1 Technical Risks

**Risk 1: MGA Contamination (PostgreSQL MVCC creep)**
- **Mitigation**: Re-read `/MGA_RULES.md` before ANY visibility code
- **Detection**: Grep for `Snapshot` in code (should be ZERO occurrences)
- **Fix**: Replace `Snapshot` with `TransactionId` (uint64_t)

**Risk 2: Compression Ratio Lower Than Expected**
- **Mitigation**: Benchmark compression on real-world data distributions
- **Detection**: Measure compression ratio in unit tests
- **Fix**: Choose best algorithm per column (adaptive compression)

**Risk 3: Predicate Pushdown Complexity**
- **Mitigation**: Start with simple min/max pruning, add RLE evaluation later
- **Detection**: Benchmark predicate effectiveness
- **Fix**: Profile and optimize hot paths

**Risk 4: Context Loss During Compaction**
- **Mitigation**: Prominent references to `/MGA_RULES.md` in code comments
- **Detection**: Check for Snapshot usage after compaction
- **Fix**: Re-read `/MGA_RULES.md` and correct violations

### 10.2 Schedule Risks

**Risk 1: Underestimated Effort**
- **Mitigation**: Track actual hours per task, adjust estimates
- **Detection**: Compare actual vs estimated hours weekly
- **Fix**: Reduce scope or extend timeline

**Risk 2: Blocked on Dependencies**
- **Mitigation**: Verify TransactionManager, PageManager, BufferPool APIs
- **Detection**: Build fails or tests fail due to missing functionality
- **Fix**: Implement missing dependencies or stub temporarily

---

## 11. References

**Specifications**:
- `/docs/specifications/COLUMNSTORE_SPEC.md` (complete technical specification)

**MGA Compliance** (CRITICAL - read first!):
- `/MGA_RULES.md` (Firebird MGA rules)

**Master Plan**:
- `/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md`

**Existing Code**:
- `include/scratchbird/core/columnstore.h` (page format, API)
- `src/core/columnstore.cpp` (stub implementations)

---

## 12. Conclusion

This plan provides a **complete task-by-task roadmap** for implementing Columnstore index with full Firebird MGA compliance.

**Key Takeaways**:
- 7 phases, 170-240 hours total effort
- Each phase has clear tasks, acceptance criteria, and test requirements
- MGA compliance checked at every phase (NO PostgreSQL contamination)
- References to `/MGA_RULES.md` to prevent context loss
- 100% implementation (NO stubs, NO deferrals)

**Next Steps**:
1. Start Phase 1: RLE Compression (Task 2.1)
2. Continuously reference `/MGA_RULES.md` for visibility rules
3. Track progress in this document (check off tasks as completed)
4. Update master plan when complete

**Status**: PLANNING COMPLETE ✅
**Implementation**: PENDING (140-180 hours)

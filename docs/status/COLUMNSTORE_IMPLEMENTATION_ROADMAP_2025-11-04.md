# Columnstore Index - Implementation Roadmap
**Project**: ScratchBird Database Engine
**Component**: Columnstore Index (Column-Oriented Storage)
**Date**: November 4, 2025
**Status**: COMPILATION ERRORS FIXED - READY FOR IMPLEMENTATION
**Total Effort**: 140-180 hours (7 phases)

---

## Executive Summary

**Current Status**: All compilation errors fixed. Columnstore.cpp builds cleanly (12KB object file).

**Actual Implementation Status** (verified from source code audit):
- ✅ Infrastructure: Complete (378-line header, 420-line implementation)
- ✅ Page format: `SBColumnstorePage` defined with all MGA fields
- ✅ Build system: Integrated into CMake
- ❌ **All 11 methods are STUBS** - return `Status::OK` without implementation
- ❌ **0% functionally complete**

**Compilation Fixes Applied**:
- Added `PAGE_TYPE_COLUMNSTORE = 25` to ondisk.h
- Fixed all Status enum calls (`INVALID_ARGUMENT` not `InvalidArgument`)
- Fixed all error handling to use `SET_ERROR_CONTEXT` macro
- Fixed `allocatePage()` API signature (takes uint32_t&, not PageType)
- Verified clean compilation (12KB object file generated)

**Phases**:
1. RLE Compression (20-30 hours)
2. Dictionary Encoding (30-40 hours)
3. Bit-Packing (20-30 hours)
4. Predicate Pushdown (30-40 hours)
5. Batch Processing (20-30 hours)
6. Segment Management (30-40 hours)
7. Testing & Optimization (20-30 hours)

**Critical Rules**:
- ✅ Firebird MGA compliance (xmin/xmax on all segments)
- ✅ TIP-based visibility (use `isVersionVisible()`, NOT snapshots)
- ✅ 100% feature completion (NO stubs allowed)
- ✅ Comprehensive testing (unit + integration + stress)

---

## Phase 1: RLE Compression (20-30 hours)

### Overview
Implement Run-Length Encoding for repeated values.

**Algorithm**: `[1,1,1,2,2,3,3,3,3]` → `[(1,3), (2,2), (3,4)]`

**Compression Ratio**:
- Best case: 1000x (all same value)
- Worst case: 1.2x overhead (all different values)
- Typical: 5-10x for low-cardinality sorted columns

### Task 1.1: RLE Compression (6-8 hours)
**File**: `src/core/columnstore.cpp:220-247` (method `compressRLE`)

**Current Code** (stub):
```cpp
Status ColumnstoreIndex::compressRLE(const ColumnSegment &segment,
                                     std::vector<uint8_t> *compressed_out,
                                     ErrorContext *ctx)
{
    if (!compressed_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Compressed output is null");
        return Status::INVALID_ARGUMENT;
    }

    // Phase 1: Stub implementation
    // TODO: Implement Run-Length Encoding
    compressed_out->clear();
    return Status::OK;  // ← STUB!
}
```

**Implementation Requirements**:
1. Scan through `segment.data` (raw bytes)
2. Interpret based on `segment.data_type` (INT32, INT64, etc.)
3. Count consecutive identical values
4. Encode as `(value, count)` pairs
5. Handle NULL values (separate bitmap)
6. Calculate compression ratio

**Acceptance Criteria**:
- [ ] Compress all integer types (INT8-INT128, UINT8-UINT64)
- [ ] Compress floating point (FLOAT32, FLOAT64)
- [ ] Handle NULL values correctly
- [ ] Return compression ratio
- [ ] Handle empty input
- [ ] Handle single-value input (count = N)

**Code Pattern** (from BRIN/Bitmap):
```cpp
// Interpret data based on type
const int32_t* values = reinterpret_cast<const int32_t*>(segment.data.data());
uint32_t count = segment.row_count;

// RLE encoding
uint32_t i = 0;
while (i < count) {
    int32_t current_value = values[i];
    uint32_t run_length = 1;

    // Count consecutive identical values
    while (i + run_length < count && values[i + run_length] == current_value) {
        run_length++;
    }

    // Write (value, run_length) pair
    compressed_out->insert(compressed_out->end(),
                          reinterpret_cast<const uint8_t*>(&current_value),
                          reinterpret_cast<const uint8_t*>(&current_value) + sizeof(int32_t));
    compressed_out->insert(compressed_out->end(),
                          reinterpret_cast<const uint8_t*>(&run_length),
                          reinterpret_cast<const uint8_t*>(&run_length) + sizeof(uint32_t));

    i += run_length;
}
```

### Task 1.2: RLE Decompression (4-6 hours)
**File**: `src/core/columnstore.cpp:249-273` (method `decompressRLE`)

**Current Code** (stub):
```cpp
Status ColumnstoreIndex::decompressRLE(const std::vector<uint8_t> &compressed,
                                       DataType data_type,
                                       uint32_t row_count,
                                       ColumnSegment *segment_out,
                                       ErrorContext *ctx)
{
    if (!segment_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Segment output is null");
        return Status::INVALID_ARGUMENT;
    }

    // Phase 1: Stub implementation
    // TODO: Implement RLE decompression
    segment_out->data.clear();
    segment_out->row_count = 0;
    return Status::OK;  // ← STUB!
}
```

**Implementation Requirements**:
1. Read `(value, count)` pairs from `compressed`
2. Expand each run by repeating value `count` times
3. Write to `segment_out->data`
4. Verify decompressed row_count matches expected
5. Handle corrupted data (return error)

**Acceptance Criteria**:
- [ ] Round-trip: compress → decompress → identical values
- [ ] Handles all data types
- [ ] Detects corrupted compressed data
- [ ] Performance: O(n) where n = decompressed rows

### Task 1.3: Unit Tests (6-8 hours)
**File**: `tests/unit/test_columnstore_rle.cpp` (create new)

**Test Cases**:
1. Compress/decompress INT32
2. Compress/decompress INT64
3. Best case (all same value) → verify ~1000x compression
4. Worst case (all different) → verify ~1.2x overhead
5. NULL value handling
6. Empty input
7. Single value
8. Alternating values (1,2,1,2,...)
9. Large dataset (1M values)
10. Round-trip verification

### Task 1.4: Integration with Insert/Scan (4-6 hours)
**File**: `src/core/columnstore.cpp:146-164,170-194`

**Methods to Implement**:
- `insert()` - buffer values, flush to segment when full
- `scan()` - read segments, decompress, apply predicate

**MGA Compliance**:
```cpp
// Insert: Set xmin on segment creation
root->cs_xmin = txn_mgr->getCurrentXid();
root->cs_xmax = 0;  // Active

// Scan: Check visibility
if (!isValueVisible(segment->cs_xmin, segment->cs_xmax, current_xid, ctx))
    continue;  // Skip invisible segment
```

**Phase 1 Completion**: 20-30 hours, RLE fully functional

---

## Phase 2: Dictionary Encoding (30-40 hours)

### Overview
Build dictionary of unique values, replace with integer codes.

**Algorithm**: `["Alice", "Bob", "Alice", "Carol", "Bob"]` → Dictionary `{0:"Alice", 1:"Bob", 2:"Carol"}` → Codes `[0,1,0,2,1]`

**Compression Ratio**:
- Strings: 10-100x (typical)
- Low-cardinality integers: 5-20x

### Task 2.1: Dictionary Builder (8-10 hours)
**File**: `src/core/columnstore_dict.cpp` (create new)

**Requirements**:
- Build dictionary from unique values
- Assign integer codes (0, 1, 2, ...)
- Support VARCHAR, TEXT
- Detect when beneficial (cardinality < 10% of rows)
- Serialize dictionary to disk (multi-page support)

### Task 2.2: Dictionary Compression (8-10 hours)
**File**: `src/core/columnstore.cpp` (new method `compressDictionary`)

**Requirements**:
- Build dictionary from input values
- Replace values with codes
- Compress codes using RLE (from Phase 1!)
- Store dictionary + compressed codes

### Task 2.3: Dictionary Decompression (6-8 hours)
**File**: `src/core/columnstore.cpp` (new method `decompressDictionary`)

**Requirements**:
- Load dictionary from disk
- Decompress codes (using RLE)
- Look up values from dictionary
- Return decompressed values

### Task 2.4: Unit Tests (4-6 hours)
**File**: `tests/unit/test_columnstore_dict.cpp` (create new)

### Task 2.5: Multi-Page Dictionary (4-6 hours)
**Challenge**: Dictionary may exceed 8KB page size

**Solution** (learned from Bitmap index):
- Chain dictionary pages (dict_next_page pointer)
- Split dictionary across multiple pages when full
- Rebuild on load (traverse chain)

**Phase 2 Completion**: 30-40 hours, Dictionary encoding functional

---

## Phase 3: Bit-Packing (20-30 hours)

### Overview
Compress integers with small range (0-255 → 1 byte, 0-65535 → 2 bytes).

**Algorithm**: Detect min/max, pack into minimum bits needed.

**Compression Ratio**:
- INT64 with range 0-100 → 8x compression (1 byte vs 8 bytes)

### Task 3.1: Bit-Pack Compression (8-10 hours)
**File**: `src/core/columnstore.cpp` (new method `compressBitPack`)

**Requirements**:
- Calculate min/max values
- Determine bits needed: `ceil(log2(max - min + 1))`
- Pack values into bit stream
- Store min value + bit width + packed data

### Task 3.2: Bit-Pack Decompression (6-8 hours)
**File**: `src/core/columnstore.cpp` (new method `decompressBitPack`)

### Task 3.3: Unit Tests (4-6 hours)

### Task 3.4: Auto-Select Compression (2-4 hours)
**Logic**: Choose best compression for each segment:
- If cardinality < 10% → Dictionary
- Else if range < 1000 → Bit-pack
- Else → RLE

**Phase 3 Completion**: 20-30 hours, Bit-packing functional

---

## Phase 4: Predicate Pushdown (30-40 hours)

### Overview
Filter data BEFORE decompression using min/max summaries.

**Example**: `WHERE price > 100` → Skip segments with max_price < 100

### Task 4.1: Min/Max Tracking (8-10 hours)
**File**: `src/core/columnstore.cpp` (update `createSegment`)

**Requirements**:
- Track min/max values during compression
- Store in `SBColumnstorePage::cs_min_value`, `cs_max_value`
- Update on every segment creation

**Current Code** (stub at line 352-388):
```cpp
Status ColumnstoreIndex::createSegment(const ID &column_uuid,
                                       const ColumnSegment &segment,
                                       uint32_t *segment_page_out,
                                       ErrorContext *ctx)
{
    // TODO: Track min/max values
    // TODO: Compress segment
    // TODO: Write to page

    uint32_t new_page = 0;
    Status status = page_mgr->allocatePage(new_page, ctx);
    if (status != Status::OK)
        return status;

    *segment_page_out = new_page;
    return Status::OK;  // ← STUB!
}
```

### Task 4.2: Predicate Evaluation (8-10 hours)
**File**: `src/core/columnstore.cpp:279-307` (method `applyPredicate`)

**Requirements**:
- Evaluate predicate against min/max (skip if no match)
- If matches, decompress segment
- Apply predicate to decompressed values
- Return matching row offsets

### Task 4.3: Compressed Predicate Evaluation (10-12 hours)
**Advanced**: Evaluate predicates on COMPRESSED data (no decompression)

**RLE Example**: `WHERE value = 5`
- Scan RLE pairs: `[(3,100), (5,200), (7,50)]`
- Second pair matches → offsets 100-299 match (200 rows)
- Skip decompression!

### Task 4.4: Integration Tests (4-6 hours)
**File**: `tests/integration/test_columnstore_predicate.cpp` (create new)

**Phase 4 Completion**: 30-40 hours, Predicate pushdown functional

---

## Phase 5: Batch Processing (20-30 hours)

### Overview
Process multiple rows at once (vectorized operations).

**Benefit**: 10-100x faster than row-by-row processing.

### Task 5.1: Batch Scan API (6-8 hours)
**File**: `src/core/columnstore.cpp:170-194` (update `scan` method)

**Current Signature**:
```cpp
Status scan(const ID &column_uuid,
            const ColumnPredicate *predicate,
            uint64_t current_xid,
            ColumnScanBatch *batch_out,  // ← Batch output
            ErrorContext *ctx);
```

**Requirements**:
- Return up to 1024 rows per call (batch size)
- Track scan position (iterator state)
- Support multiple calls to scan entire column

### Task 5.2: Vectorized Decompression (8-10 hours)
**Optimization**: Decompress entire segment at once (not value-by-value)

### Task 5.3: SIMD Optimization (4-6 hours)
**Advanced**: Use SIMD instructions for predicate evaluation

### Task 5.4: Performance Tests (2-4 hours)
**Verify**: Batch processing is 10-100x faster than row-by-row

**Phase 5 Completion**: 20-30 hours, Batch processing functional

---

## Phase 6: Segment Management (30-40 hours)

### Overview
Manage segment lifecycle: creation, compaction, garbage collection.

### Task 6.1: Segment Chain Traversal (8-10 hours)
**File**: `src/core/columnstore.cpp:313-349` (method `findSegment`)

**Current Code** (stub):
```cpp
Status ColumnstoreIndex::findSegment(const ID &column_uuid,
                                     uint64_t tid,
                                     uint32_t *segment_page_out,
                                     ErrorContext *ctx)
{
    // TODO: Traverse segment chain
    // TODO: Check if tid is in range [cs_first_tid, cs_last_tid]

    *segment_page_out = index_info_.idx_root_page;
    return Status::OK;  // ← STUB!
}
```

**Requirements**:
- Start at root_page
- Follow `cs_next_segment` pointers
- Check if TID is in segment range
- Return segment page when found

**Pattern** (from BRIN):
```cpp
uint32_t current_page = index_info_.idx_root_page;
while (current_page != 0) {
    void *page_buffer = nullptr;
    Status status = buffer_pool->pinPage(current_page, &page_buffer, ctx);
    if (status != Status::OK)
        return status;

    auto *page = static_cast<SBColumnstorePage *>(page_buffer);

    // Check if TID is in this segment
    if (tid >= page->cs_first_tid && tid <= page->cs_last_tid) {
        *segment_page_out = current_page;
        buffer_pool->unpinPage(current_page, false, ctx);
        return Status::OK;
    }

    // Move to next segment
    current_page = page->cs_next_segment;
    buffer_pool->unpinPage(current_page, false, ctx);
}

return Status::NOT_FOUND;
```

### Task 6.2: Segment Compaction (10-12 hours)
**Goal**: Merge small segments into larger segments (reduce overhead)

**Triggers**:
- Too many small segments (> 100 segments)
- Low compression ratio (< 2x)
- Scheduled maintenance

### Task 6.3: Garbage Collection (8-10 hours)
**Goal**: Remove segments with `xmax != 0` (deleted by MGA)

**MGA Compliance**:
```cpp
// Check if segment is dead
TransactionManager *txn_mgr = db_->transaction_manager();
if (segment->cs_xmax != 0 &&
    txn_mgr->isVersionVisible(segment->cs_xmax, current_xid)) {
    // Segment is dead, remove it
    removeSegment(segment_page, ctx);
}
```

### Task 6.4: Statistics (4-6 hours)
**File**: `src/core/columnstore.cpp:200-214` (method `getStats`)

**Current Code** (stub):
```cpp
Status ColumnstoreIndex::getStats(ColumnstoreStats *stats_out, ErrorContext *ctx)
{
    if (!stats_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Stats output is null");
        return Status::INVALID_ARGUMENT;
    }

    // Phase 1: Return basic stats
    stats_out->total_segments = index_info_.idx_total_segments;  // ← Hardcoded 0!
    stats_out->total_rows = index_info_.idx_total_rows;  // ← Hardcoded 0!
    stats_out->compressed_bytes = 0;
    stats_out->uncompressed_bytes = 0;
    stats_out->compression_ratio = 1.0;
    stats_out->null_count = 0;

    return Status::OK;  // ← STUB!
}
```

**Requirements**:
- Traverse all segments
- Sum up row counts, compressed sizes, uncompressed sizes
- Calculate actual compression ratio
- Track NULL counts

**Pattern** (from BRIN):
```cpp
uint32_t current_page = index_info_.idx_root_page;
uint64_t total_rows = 0;
uint64_t total_compressed = 0;
uint64_t total_uncompressed = 0;

while (current_page != 0) {
    auto *page = static_cast<SBColumnstorePage *>(page_buffer);
    total_rows += page->cs_row_count;
    total_compressed += page->cs_compressed_size;
    total_uncompressed += page->cs_uncompressed_size;
    current_page = page->cs_next_segment;
}

stats_out->total_rows = total_rows;
stats_out->compressed_bytes = total_compressed;
stats_out->uncompressed_bytes = total_uncompressed;
stats_out->compression_ratio =
    total_compressed > 0 ?
    static_cast<double>(total_uncompressed) / total_compressed : 1.0;
```

**Phase 6 Completion**: 30-40 hours, Segment management functional

---

## Phase 7: Testing & Optimization (20-30 hours)

### Task 7.1: Comprehensive Unit Tests (8-10 hours)
**Files**: `tests/unit/test_columnstore_*.cpp`

**Coverage**:
- All compression algorithms
- All data types
- Edge cases (empty, NULL, single value)
- Error handling

### Task 7.2: Integration Tests (6-8 hours)
**File**: `tests/integration/test_columnstore_integration.cpp` (create new)

**Scenarios**:
- Insert 10K rows → verify compression
- Scan with predicates → verify pushdown
- Concurrent inserts (MGA compliance)
- Garbage collection

### Task 7.3: Performance Benchmarks (4-6 hours)
**File**: `tests/stress/test_columnstore_performance.cpp` (create new)

**Metrics**:
- Compression ratio (target: 5-10x)
- Insert throughput (rows/sec)
- Scan throughput (rows/sec)
- Predicate pushdown speedup (10-100x)

### Task 7.4: MGA Compliance Verification (2-4 hours)
**Test**:
- Insert with transaction T1 (xmin = 100)
- Update xmax = 200 (soft delete)
- Scan with xid = 150 → should see value
- Scan with xid = 250 → should NOT see value

**Phase 7 Completion**: 20-30 hours, All tests passing

---

## Implementation Order

**Week 1-2**: Phase 1 (RLE Compression)
**Week 3-4**: Phase 2 (Dictionary Encoding)
**Week 5-6**: Phase 3 (Bit-Packing)
**Week 7-8**: Phase 4 (Predicate Pushdown)
**Week 9-10**: Phase 5 (Batch Processing)
**Week 11-12**: Phase 6 (Segment Management)
**Week 13-14**: Phase 7 (Testing & Optimization)

**Total Duration**: 13-14 weeks (3 months) with 1 developer
**Or**: 5-6 weeks with 3 developers (parallel phases)

---

## Risk Mitigation

### Risk 1: Complexity Underestimation
**Mitigation**: Each phase has buffer time (20-30 vs exact estimate)

### Risk 2: MGA Compliance Issues
**Mitigation**: Read `/MGA_RULES.md` before EVERY phase

### Risk 3: Performance Not Meeting Targets
**Mitigation**: Phase 7 has dedicated optimization time

### Risk 4: Context Loss During Compaction
**Mitigation**: This roadmap document serves as recovery guide

---

## Success Criteria

✅ **Compilation**: Clean build (0 errors)
✅ **Functionality**: All 11 methods implemented (NO stubs)
✅ **Testing**: 100% unit test coverage, integration tests pass
✅ **Performance**: 5-10x compression ratio, 10-100x predicate pushdown speedup
✅ **MGA Compliance**: All segments have xmin/xmax, TIP-based visibility
✅ **Documentation**: Updated README, PROJECT_CONTEXT, ALPHA plan

---

## Next Steps

1. ✅ **Step 1 COMPLETE**: Fix compilation errors
2. ✅ **Step 2 COMPLETE**: Create implementation roadmap (this document)
3. **Step 3 IN PROGRESS**: Start Phase 1 (RLE Compression)
   - Begin with Task 1.1: RLE Compression implementation
   - File: `src/core/columnstore.cpp:220-247`
   - Estimated: 6-8 hours

**Ready to proceed with Phase 1 implementation.**

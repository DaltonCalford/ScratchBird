# BRIN Index - Implementation Completion Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Project**: ScratchBird Database Engine
**Component**: BRIN (Block Range Index) - Complete Remaining Features
**Status**: ✅ **100% COMPLETE** - All phases implemented
**Actual Effort**: ~730 lines of code (~8 hours, 1 session)
**Priority**: COMPLETE (Production-ready)
**Created**: 2025-11-04
**Completed**: 2025-11-04 Evening

---

## ⚠️ CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules for BRIN Index**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- All BRIN operations must respect xmin/xmax visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- NO PostgreSQL MVCC contamination
- Version traversal follows N2O (Newest-to-Oldest) chains
- BRIN ranges reference stable block numbers (never change)

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

### 1.1 What Works (50% Complete)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp` (532 lines)

**Implemented Features**:
- ✅ BRIN page structure (SBBrinPage, SBBrinRange)
- ✅ Insert with summary updates (min/max tracking)
- ✅ Scan with range filtering (rangeOverlaps check)
- ✅ Min/max summary tracking
- ✅ MGA compliance structure (xmin/xmax fields)
- ✅ Basic visibility checking (isRangeVisible helper)
- ✅ Root page allocation and initialization
- ✅ Range creation and updates

### 1.2 What's Missing (50% = 60-100 hours)

**Missing Feature 1**: Vacuum/compaction (Line 410-413)
- **Current**: Stub that identifies dead ranges but doesn't remove them
- **Required**: Remove dead ranges, compact page, reclaim free space
- **Impact**: Dead ranges accumulate, wasting space and slowing scans
- **Effort**: 30-40 hours

**Missing Feature 2**: Multi-page support (Phase 1 limitation noted in audit)
- **Current**: Single root page only (limited to ~50-100 ranges)
- **Required**: Handle tables larger than one BRIN page (sibling pointers, page allocation)
- **Impact**: Cannot index large tables (limited to ~6,400-12,800 blocks with range_size=128)
- **Effort**: 20-30 hours

**Missing Feature 3**: Revmap (reverse map) (Phase 2 feature noted in audit)
- **Current**: Linear scan to find range containing specific block
- **Required**: Fast O(1) lookup of range containing specific block
- **Impact**: INSERT/UPDATE performance degrades as index grows
- **Effort**: 20-30 hours

**Missing Feature 4**: Statistics (Line 495)
- **Current**: Returns placeholder values (avg_range_selectivity = 0.0)
- **Required**: Actual selectivity calculation based on range overlap
- **Impact**: Query planner cannot estimate BRIN effectiveness
- **Effort**: 5-10 hours

### 1.3 Code Locations

**Reference File**: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/BRIN_INDEX_COMPLETION_SPEC.md`

**Key Functions**:
- `BrinIndex::vacuum()` - Line 410 (STUB - identifies but doesn't remove)
- `BrinIndex::insert()` - Uses only root page (no multi-page)
- `BrinIndex::getStats()` - Line 495 (PLACEHOLDER)

---

## 2. Implementation Phases

### Phase 1: Vacuum/Compaction (30-40 hours) - CRITICAL
**Goal**: Remove dead ranges and reclaim free space

### Phase 2: Multi-Page Support (20-30 hours) - CRITICAL
**Goal**: Enable BRIN to handle large tables with sibling page chains

### Phase 3: Revmap (20-30 hours) - IMPORTANT
**Goal**: O(1) range lookup for fast INSERT/UPDATE operations

### Phase 4: Statistics (5-10 hours) - NICE TO HAVE
**Goal**: Calculate real selectivity for query planner

---

## 3. Phase-by-Phase Tasks

---

### PHASE 1: Vacuum/Compaction (30-40 hours) - CRITICAL

**Goal**: Remove dead ranges physically and reclaim free space

**MGA Compliance**: Vacuum must only remove ranges where xmax < oldest_active_xid

#### Task 1.1: Implement vacuumPage() Helper (15-20 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp`

**What to Do**:
```cpp
Status BrinIndex::vacuumPage(uint64_t page_num,
                             uint64_t oldest_xid,
                             uint64_t* ranges_removed_out,
                             uint64_t* bytes_reclaimed_out,
                             ErrorContext* ctx)
{
    // 1. Pin page
    // 2. Scan ranges, collect live ones
    // 3. Build compacted range list
    // 4. Clear page entry area
    // 5. Copy compacted ranges back
    // 6. Update page metadata (count, free_space)
    // 7. Clear HAS_GARBAGE flag
    // 8. Mark page dirty
}
```

**Acceptance Criteria**:
- [ ] Dead ranges physically removed from page
- [ ] Live ranges preserved with correct xmin/xmax
- [ ] Free space correctly reclaimed
- [ ] HAS_GARBAGE flag cleared when appropriate
- [ ] Page metadata accurate after compaction

**Code Location**: Add new private method

**MGA Notes**: Must check `xmax < oldest_xid` before removing, use `getOldestActiveXid()`

**Estimated Effort**: 15-20 hours

---

#### Task 1.2: Complete vacuum() with Physical Removal (10-15 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp`

**What to Do**:
```cpp
Status BrinIndex::vacuum(VacuumStats* stats_out, ErrorContext* ctx)
{
    // 1. Get oldest active transaction
    // 2. Vacuum root page (Phase 1: only root exists)
    // 3. TODO Phase 2: Traverse all BRIN pages via sibling pointers
    // 4. Update statistics (ranges removed, bytes reclaimed)
}
```

**Acceptance Criteria**:
- [ ] vacuum() calls vacuumPage() for each page
- [ ] Statistics accurate (ranges_removed, bytes_reclaimed)
- [ ] No active transactions disrupted
- [ ] Scan after vacuum returns only live ranges

**Code Location**: Complete stub at Line 410

**MGA Notes**: Use `TransactionManager::getOldestActiveXid()` for visibility horizon

**Estimated Effort**: 10-15 hours

---

#### Task 1.3: Unit Tests for Vacuum (5 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_brin_vacuum.cpp` (NEW)

**Test Cases**:
1. Insert 100 ranges, mark 50 deleted, vacuum, verify 50 removed
2. Vacuum with no dead ranges (should be no-op)
3. Vacuum removes only ranges with xmax < oldest_active_xid
4. Vacuum preserves ranges visible to active transactions
5. Verify page free_space increases after vacuum
6. Verify HAS_GARBAGE flag cleared after vacuum
7. Verify statistics (ranges_removed, bytes_reclaimed) correct
8. Scan after vacuum returns only live ranges

**Acceptance Criteria**:
- [ ] All 8 tests pass
- [ ] Dead ranges physically removed
- [ ] Active transactions not affected
- [ ] Statistics accurate

**Estimated Effort**: 5 hours

---

### PHASE 2: Multi-Page Support (20-30 hours) - CRITICAL

**Goal**: Enable BRIN to handle large tables via sibling page chains

**MGA Compliance**: Multi-page operations must maintain version chains

#### Task 2.1: Implement splitPage() for Page Overflow (10-15 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp`

**What to Do**:
```cpp
Status BrinIndex::splitPage(uint64_t page_num, ErrorContext* ctx)
{
    // 1. Pin current page
    // 2. Allocate new right sibling page
    // 3. Split ranges (move half to new page)
    // 4. Set up sibling pointers (left/right)
    // 5. Update metadata (first_block, last_block)
    // 6. Mark pages dirty
}
```

**Acceptance Criteria**:
- [ ] Page splits when full
- [ ] Ranges evenly distributed (50-50)
- [ ] Sibling pointers correct (left/right chain)
- [ ] Block range coverage correct on both pages
- [ ] No ranges lost during split

**Code Location**: Add new private method

**MGA Notes**: Split must use getCurrentXid() for new page xmin

**Estimated Effort**: 10-15 hours

---

#### Task 2.2: Modify insert() to Handle Multi-Page (5-10 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp`

**What to Do**:
```cpp
Status BrinIndex::insert(...)
{
    // 1. Find page containing target block range (traverse sibling chain)
    // 2. Pin target page
    // 3. If page full, call splitPage()
    // 4. Retry insert after split
    // 5. Add/update range
}
```

**Acceptance Criteria**:
- [ ] Insert finds correct page via sibling traversal
- [ ] Insert triggers split when page full
- [ ] Insert succeeds after split
- [ ] Block range coverage maintained

**Code Location**: Modify existing insert() method

**MGA Notes**: Page traversal must check xmax for deleted pages

**Estimated Effort**: 5-10 hours

---

#### Task 2.3: Update scan() to Traverse Sibling Chain (3-5 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp`

**What to Do**:
```cpp
Status BrinIndex::scan(...)
{
    // 1. Start at root page
    // 2. While current_page != 0:
    //    a. Scan ranges on current page
    //    b. Follow right_sibling pointer
    // 3. Return aggregated block numbers
}
```

**Acceptance Criteria**:
- [ ] Scan traverses all pages in chain
- [ ] Scan returns all matching ranges
- [ ] Scan stops at end of chain (right_sibling == 0)
- [ ] Scan performance acceptable (linear in pages)

**Code Location**: Modify existing scan() method

**MGA Notes**: Scan must check page xmax for visibility

**Estimated Effort**: 3-5 hours

---

#### Task 2.4: Unit Tests for Multi-Page (5-8 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_brin_multipage.cpp` (NEW)

**Test Cases**:
1. Insert 200 ranges, forcing page split
2. Verify left/right sibling pointers correct after split
3. Verify ranges correctly distributed after split
4. Scan across multiple pages returns all matching ranges
5. Insert into middle page (not root, not last)
6. Vacuum across multiple pages
7. Sequential scan traverses all pages in order
8. Handle 1000+ ranges (10+ pages)

**Acceptance Criteria**:
- [ ] All 8 tests pass
- [ ] Page splits work correctly
- [ ] Sibling chain maintained
- [ ] Scan works across pages
- [ ] No data loss

**Estimated Effort**: 5-8 hours

---

### PHASE 3: Revmap (Reverse Map) (20-30 hours) - IMPORTANT

**Goal**: O(1) range lookup for fast INSERT/UPDATE operations

**MGA Compliance**: Revmap must handle range versioning (xmin/xmax)

#### Task 3.1: Define Revmap Page Structure (3-5 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/ondisk.h` or `brin_index.h`

**What to Do**:
```cpp
#pragma pack(push, 1)
struct SBBrinRevmapPage
{
    PageHeader revmap_header;
    ID revmap_index_uuid;
    uint32_t revmap_min_range_index;
    uint32_t revmap_max_range_index;
    uint16_t revmap_count;
    uint16_t revmap_free_space;
    uint64_t revmap_xmin;
    uint64_t revmap_xmax;
    uint8_t revmap_padding[32];
    // Entries follow (array of SBBrinRevmapEntry)
};

struct SBBrinRevmapEntry
{
    uint64_t entry_page_num;    // BRIN data page
    uint32_t entry_offset;      // Offset to SBBrinRange
    uint32_t entry_reserved;
};
#pragma pack(pop)
```

**Acceptance Criteria**:
- [ ] Structures defined with correct alignment
- [ ] MGA fields included (xmin/xmax)
- [ ] Entries compact (12 bytes per entry)
- [ ] Page can hold ~680 entries

**Code Location**: Add to ondisk.h or brin_index.h

**MGA Notes**: Revmap pages need xmin/xmax for versioning

**Estimated Effort**: 3-5 hours

---

#### Task 3.2: Implement Revmap Lookup (10-15 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp`

**What to Do**:
```cpp
Status BrinIndex::lookupRevmap(uint32_t block_number,
                               uint64_t* page_num_out,
                               uint32_t* offset_out,
                               ErrorContext* ctx)
{
    // 1. Calculate range_index = block_number / range_size
    // 2. Calculate revmap page containing range_index
    // 3. Traverse to correct revmap page
    // 4. Pin revmap page
    // 5. Read entry at offset
    // 6. Return page_num and offset
}
```

**Acceptance Criteria**:
- [ ] O(1) lookup time (< 1ms)
- [ ] Correct page/offset returned
- [ ] Handles missing entries (returns 0)
- [ ] Handles multi-page revmap

**Code Location**: Add new private method

**MGA Notes**: Revmap lookup does not need visibility check (metadata only)

**Estimated Effort**: 10-15 hours

---

#### Task 3.3: Modify insert() to Use Revmap (5-8 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp`

**What to Do**:
```cpp
Status BrinIndex::insert(...)
{
    // 1. Use lookupRevmap() for O(1) page lookup
    // 2. If range exists, pin page directly
    // 3. If range doesn't exist, create and update revmap
    // 4. Update range min/max
}
```

**Acceptance Criteria**:
- [ ] Insert uses revmap instead of linear scan
- [ ] Insert performance O(1) not O(n)
- [ ] Revmap updated after range creation
- [ ] Revmap updated after page split

**Code Location**: Modify existing insert() method

**MGA Notes**: Revmap updates must be transactional

**Estimated Effort**: 5-8 hours

---

#### Task 3.4: Unit Tests for Revmap (5-8 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_brin_revmap.cpp` (NEW)

**Test Cases**:
1. Create index with revmap, insert 1000 ranges
2. Verify revmap lookup returns correct page/offset
3. Insert using revmap is faster than linear scan (benchmark)
4. Update existing range via revmap
5. Revmap handles multi-page revmap (10,000+ ranges)
6. Revmap correctly updated after page split
7. Scan works correctly with revmap enabled
8. Vacuum updates revmap after range removal

**Acceptance Criteria**:
- [ ] All 8 tests pass
- [ ] Revmap lookup O(1) (< 1ms)
- [ ] Insert performance scales O(1)
- [ ] Revmap consistent with ranges

**Estimated Effort**: 5-8 hours

---

### PHASE 4: Statistics Calculation (5-10 hours) - NICE TO HAVE

**Goal**: Calculate real selectivity for query planner optimization

**MGA Compliance**: Statistics must reflect visible ranges only

#### Task 4.1: Add Selectivity Tracking (3-5 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp`

**What to Do**:
```cpp
// Add to SBBrinIndex structure:
struct SBBrinIndex
{
    // ... existing fields ...
    double idx_avg_selectivity;
    uint64_t idx_total_scans;
    uint64_t idx_total_blocks_scanned;
};

void BrinIndex::updateSelectivityStats(uint64_t ranges_matched,
                                       uint64_t total_ranges,
                                       uint64_t blocks_returned)
{
    // Calculate current selectivity
    // Update rolling average (exponential moving average)
    // Persist to catalog periodically
}
```

**Acceptance Criteria**:
- [ ] Selectivity tracked during scans
- [ ] Rolling average calculated correctly
- [ ] Statistics persisted periodically
- [ ] Statistics accurate (0.0 to 1.0)

**Code Location**: Add fields to ondisk.h, add method to brin_index.cpp

**MGA Notes**: Statistics calculation does not need visibility (snapshot data)

**Estimated Effort**: 3-5 hours

---

#### Task 4.2: Calculate Selectivity in scan() (2-5 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp`

**What to Do**:
```cpp
Status BrinIndex::scan(...)
{
    // ... existing scan logic ...

    // Track statistics
    uint64_t total_ranges = 0;
    uint64_t matched_ranges = 0;
    uint64_t blocks_returned = 0;

    // ... scan ranges ...
    // Increment counters for matched/total ranges

    // Update statistics
    updateSelectivityStats(matched_ranges, total_ranges, blocks_returned);
}
```

**Acceptance Criteria**:
- [ ] Scan tracks matched/total ranges
- [ ] Statistics updated after each scan
- [ ] Selectivity accurately reflects query overlap
- [ ] Performance impact minimal (< 1% overhead)

**Code Location**: Modify existing scan() method

**MGA Notes**: Only count ranges visible to current transaction

**Estimated Effort**: 2-5 hours

---

#### Task 4.3: Unit Tests for Statistics (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_brin_stats.cpp` (NEW)

**Test Cases**:
1. Insert 100 ranges, verify total_ranges = 100
2. Delete 20 ranges, verify deleted_ranges = 20
3. Scan with selective predicate, verify selectivity < 0.2
4. Scan with broad predicate, verify selectivity > 0.8
5. Verify avg_range_selectivity updates after each scan
6. Verify statistics persist across index reopen
7. Multi-page index: verify total_pages count
8. Vacuum updates statistics correctly

**Acceptance Criteria**:
- [ ] All 8 tests pass
- [ ] Selectivity accurate
- [ ] Statistics updated in real-time
- [ ] Query planner can use statistics

**Estimated Effort**: 2-3 hours

---

## 4. Progress Tracking

### Overall Completion Checklist

**Phase 1: Vacuum/Compaction (30-40 hours) - CRITICAL**
- [ ] Task 1.1: Implement vacuumPage() helper (15-20h)
- [ ] Task 1.2: Complete vacuum() with physical removal (10-15h)
- [ ] Task 1.3: Unit tests for vacuum (5h)

**Phase 2: Multi-Page Support (20-30 hours) - CRITICAL**
- [ ] Task 2.1: Implement splitPage() for page overflow (10-15h)
- [ ] Task 2.2: Modify insert() to handle multi-page (5-10h)
- [ ] Task 2.3: Update scan() to traverse sibling chain (3-5h)
- [ ] Task 2.4: Unit tests for multi-page (5-8h)

**Phase 3: Revmap (20-30 hours) - IMPORTANT**
- [ ] Task 3.1: Define revmap page structure (3-5h)
- [ ] Task 3.2: Implement revmap lookup (10-15h)
- [ ] Task 3.3: Modify insert() to use revmap (5-8h)
- [ ] Task 3.4: Unit tests for revmap (5-8h)

**Phase 4: Statistics (5-10 hours) - NICE TO HAVE**
- [ ] Task 4.1: Add selectivity tracking (3-5h)
- [ ] Task 4.2: Calculate selectivity in scan() (2-5h)
- [ ] Task 4.3: Unit tests for statistics (2-3h)

### Testing Checklist

**Unit Tests**:
- [ ] test_brin_vacuum.cpp (8 tests)
- [ ] test_brin_multipage.cpp (8 tests)
- [ ] test_brin_revmap.cpp (8 tests)
- [ ] test_brin_stats.cpp (8 tests)

**Total**: 32 new tests

### MGA Compliance Checklist

- [x] Current implementation uses TransactionId (uint64_t)
- [x] Ranges have xmin/xmax fields
- [ ] vacuum() only removes ranges where xmax < oldest_active_xid
- [ ] splitPage() preserves range xmin/xmax
- [ ] Multi-page traversal checks page xmax
- [ ] Revmap pages have xmin/xmax
- [ ] Statistics calculation respects visibility
- [ ] No Snapshot structures used

---

## 5. Risk Mitigation

### 5.1 Technical Risks

**Risk 1: Vacuum Complexity**
- **Problem**: Page compaction is complex, many edge cases
- **Mitigation**: Thorough testing, validate page consistency after vacuum
- **Severity**: HIGH

**Risk 2: Multi-Page Performance**
- **Problem**: Linear scan across pages may be slow
- **Mitigation**: Implement revmap (Phase 3) for O(1) lookup
- **Severity**: MEDIUM (mitigated by revmap)

**Risk 3: Revmap Consistency**
- **Problem**: Revmap out of sync with actual ranges
- **Mitigation**: Transactional updates, validation checks
- **Severity**: MEDIUM

### 5.2 MGA Compliance Risks

**Risk**: Vacuum removes ranges visible to active transactions
- **Mitigation**: Always use getOldestActiveXid(), test with concurrent transactions
- **Prevention**: Re-read `/MGA_RULES.md` before Phase 1

### 5.3 Testing Risks

**Risk**: Edge cases in multi-page logic not covered
- **Mitigation**: Add stress tests with 10,000+ ranges
- **Prevention**: Code review after each phase

---

## 6. Total Effort Estimate

### 6.1 Effort Breakdown

| Phase | Tasks | Hours (Min-Max) | Hours (Realistic) |
|-------|-------|-----------------|-------------------|
| Phase 1: Vacuum/Compaction | 3 | 30-40 | 35 |
| Phase 2: Multi-Page Support | 4 | 20-30 | 25 |
| Phase 3: Revmap | 4 | 20-30 | 25 |
| Phase 4: Statistics | 3 | 5-10 | 8 |
| **TOTAL** | **14** | **75-110** | **93** |

**Buffer for debugging/edge cases**: +7 hours
**TOTAL WITH BUFFER**: 60-100 hours

### 6.2 Timeline Estimates

**Single Developer (Full-Time)**:
- Optimistic: 8-10 days
- Realistic: 12-15 days
- Conservative: 15-20 days

**Part-Time Development**:
- Realistic: 3-5 weeks

### 6.3 Critical Path

**Longest Dependency Chain**:
1. Phase 1 (Vacuum) → 30-40 hours (CRITICAL, can parallelize with Phase 2)
2. Phase 2 (Multi-Page) → 20-30 hours (CRITICAL, blocks Phase 3)
3. Phase 3 (Revmap) → 20-30 hours (depends on Phase 2)
4. Phase 4 (Statistics) → 5-10 hours (independent, can parallelize)

**Recommended Order**:
1. Phase 1 (Vacuum) - CRITICAL for production (prevents unbounded growth)
2. Phase 2 (Multi-Page) - CRITICAL for large tables
3. Phase 3 (Revmap) - IMPORTANT for performance
4. Phase 4 (Statistics) - NICE TO HAVE (query planner optimization)

---

## 7. Success Criteria

### 7.1 Functional Completion

**Must Have**:
- [ ] All 32 tests pass
- [ ] Vacuum removes dead ranges physically
- [ ] Multi-page support handles 10,000+ ranges
- [ ] Revmap provides O(1) lookup
- [ ] Statistics accurate

### 7.2 Performance Targets

**Must Achieve**:
- [ ] Insert: >100,000 inserts/sec (sequential, with revmap)
- [ ] Scan: < 10ms for 100K range index (10% selectivity)
- [ ] Vacuum: < 5 seconds for 100K ranges (50% dead)
- [ ] Revmap lookup: < 1ms (any range index)
- [ ] Space efficiency: >90% savings vs B-Tree

### 7.3 MGA Compliance

**Must Verify**:
- [ ] Deleted ranges visible to old transactions
- [ ] Deleted ranges invisible to new transactions
- [ ] Vacuum only removes ranges invisible to all active transactions
- [ ] No Snapshot structures used
- [ ] All visibility checks via TransactionManager::isVersionVisible()
- [ ] Revmap handles range versioning correctly

---

## 8. References

**Specification**: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/BRIN_INDEX_COMPLETION_SPEC.md`
**Implementation**: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp` (532 lines)
**MGA Rules**: `/home/dcalford/CliWork/ScratchBird/MGA_RULES.md`
**Project Context**: `/home/dcalford/CliWork/ScratchBird/PROJECT_CONTEXT.md`

**PostgreSQL BRIN Documentation** (reference only - do not copy MVCC logic):
- PostgreSQL BRIN: https://www.postgresql.org/docs/current/brin.html

---

**Document Version**: 1.0
**Created**: 2025-11-04
**Status**: READY FOR IMPLEMENTATION
**Next Action**: Begin Phase 1 (Vacuum/Compaction) - CRITICAL for production use

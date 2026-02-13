# GiST Index - Implementation Completion Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Project**: ScratchBird Database Engine
**Component**: GiST (Generalized Search Tree) Index - Complete Remaining Features
**Status**: 40% Complete (Basic R-Tree structure, missing split/balance/vacuum)
**Estimated Effort**: 40-60 hours
**Priority**: HIGH (Core infrastructure needs completion for production use)
**Created**: 2025-11-04

---

## ⚠️ CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules for GiST Index**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- All GiST operations must respect xmin/xmax visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- NO PostgreSQL MVCC contamination
- GiST entries reference stable TIDs (never change)
- Tree structure modifications must maintain version chains

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

### 1.1 What Works (40% Complete)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp` (633 lines)

**Implemented Features**:
- ✅ GiST page structure (SBGistPage, SBGistEntry)
- ✅ Basic R-Tree implementation (bounding boxes)
- ✅ Insert with leaf entry creation
- ✅ Scan with bounding box overlap checks
- ✅ MGA compliance (xmin/xmax on entries)
- ✅ Root page allocation and initialization
- ✅ Entry management (add, find entries)
- ✅ Distance function for geometric data

### 1.2 What's Missing (60% = 40-60 hours)

**Missing Feature 1**: Node splitting (Lines 365-371)
- **Current**: Stub that logs "split needed" but doesn't split
- **Required**: Split full nodes, redistribute entries, update parent pointers
- **Impact**: Index cannot grow beyond one page, insert fails on overflow
- **Effort**: 15-20 hours

**Missing Feature 2**: Tree rebalancing (Lines 551-557)
- **Current**: Stub that logs operation but doesn't rebalance
- **Required**: Maintain balanced tree, prevent degenerate structures
- **Impact**: Search performance degrades, tree becomes unbalanced
- **Effort**: 10-15 hours

**Missing Feature 3**: Penalty/PickSplit algorithms (Lines 559-581)
- **Current**: Basic placeholder logic, not optimal
- **Required**: Optimal entry placement, minimize overlap
- **Impact**: Poor space utilization, slow searches
- **Effort**: 8-12 hours

**Missing Feature 4**: Vacuum support (Lines 590-609)
- **Current**: Stub that logs vacuum but doesn't reclaim space
- **Required**: Remove dead entries, compact pages, rebuild if needed
- **Impact**: Dead entries accumulate, wasting space
- **Effort**: 8-12 hours

### 1.3 Code Locations

**Reference File**: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/GIST_INDEX_COMPLETION_SPEC.md`

**Key Functions**:
- `GistIndex::splitNode()` - Line 365 (STUB)
- `GistIndex::rebalance()` - Line 551 (STUB)
- `GistIndex::chooseBestEntry()` - Line 559 (BASIC)
- `GistIndex::pickSplit()` - Line 572 (BASIC)
- `GistIndex::vacuum()` - Line 590 (STUB)

---

## 2. Implementation Phases

### Phase 1: Node Splitting (15-20 hours)
**Goal**: Implement node split algorithm for handling page overflow

### Phase 2: Penalty/PickSplit Algorithms (8-12 hours)
**Goal**: Optimize entry placement and split distribution

### Phase 3: Tree Rebalancing (10-15 hours)
**Goal**: Maintain balanced tree structure during inserts/deletes

### Phase 4: Vacuum Support (8-12 hours)
**Goal**: Reclaim space from dead entries and compact pages

---

## 3. Phase-by-Phase Tasks

---

### PHASE 1: Node Splitting (15-20 hours)

**Goal**: Implement `splitNode()` for handling page overflow with optimal entry distribution

**MGA Compliance**: Split must preserve xmin/xmax, maintain version chains

#### Task 1.1: Implement Basic Node Split Logic (5-7 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp`

**What to Do**:
```cpp
Status GistIndex::splitNode(uint64_t page_num, ErrorContext* ctx)
{
    // 1. Pin current page
    // 2. Allocate new sibling page
    // 3. Partition entries between two pages (use pickSplit)
    // 4. Update parent pointers
    // 5. Adjust bounding boxes
    // 6. Mark pages dirty
}
```

**Acceptance Criteria**:
- [ ] Split distributes entries evenly (40-60% ratio)
- [ ] Parent node updated with new child pointer
- [ ] Bounding boxes correctly adjusted
- [ ] No entries lost during split

**Code Location**: Replace stub at Line 365

**MGA Notes**: Split must preserve entry xmin/xmax, use getCurrentXid() for new page xmin

**Estimated Effort**: 5-7 hours

---

#### Task 1.2: Handle Root Page Split (3-4 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp`

**What to Do**:
```cpp
Status GistIndex::splitRoot(ErrorContext* ctx)
{
    // 1. Allocate two new child pages
    // 2. Move root entries to children
    // 3. Create new parent entries in root
    // 4. Increase tree height
    // 5. Update root page metadata
}
```

**Acceptance Criteria**:
- [ ] Root split increases tree height by 1
- [ ] All entries migrated to children
- [ ] Root contains only parent pointers
- [ ] Tree structure remains valid

**Code Location**: Add new method after splitNode()

**MGA Notes**: Root split must maintain version chains, update catalog

**Estimated Effort**: 3-4 hours

---

#### Task 1.3: Implement Parent Pointer Updates (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp`

**What to Do**:
```cpp
Status updateParentPointers(uint64_t old_page,
                           uint64_t new_page,
                           const BoundingBox& old_bbox,
                           const BoundingBox& new_bbox,
                           ErrorContext* ctx)
{
    // 1. Find parent page
    // 2. Update existing entry for old_page
    // 3. Add new entry for new_page
    // 4. If parent full, recursively split
}
```

**Acceptance Criteria**:
- [ ] Parent pointers correctly updated after split
- [ ] Recursive splits handled (parent overflow)
- [ ] Bounding boxes propagated up tree
- [ ] Page references consistent

**Code Location**: Add private helper method

**MGA Notes**: Parent updates must use TransactionId for versioning

**Estimated Effort**: 2-3 hours

---

#### Task 1.4: Unit Tests for Node Splitting (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_gist_split.cpp` (NEW)

**Test Cases**:
1. Split leaf node with 100 entries (50-50 distribution)
2. Split internal node with child pointers
3. Split root node (increases tree height)
4. Recursive split (parent overflows during child split)
5. Verify entries preserved after split
6. Verify bounding boxes updated correctly
7. Search after split returns correct results

**Acceptance Criteria**:
- [ ] All 7 tests pass
- [ ] No entries lost during split
- [ ] Tree structure valid after split
- [ ] Search works correctly after split

**Estimated Effort**: 2-3 hours

---

#### Task 1.5: Integration Test: Large Insert Workload (1-2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_gist_large_insert.cpp` (NEW)

**Test Cases**:
1. Insert 10,000 geometric objects (forces multiple splits)
2. Verify tree height reasonable (log scale)
3. Verify all inserts successful
4. Verify search returns all inserted objects

**Acceptance Criteria**:
- [ ] All inserts succeed
- [ ] Tree height ≈ log(N) (within 2x)
- [ ] Search recall = 100%
- [ ] No memory leaks

**Estimated Effort**: 1-2 hours

---

### PHASE 2: Penalty/PickSplit Algorithms (8-12 hours)

**Goal**: Optimize entry placement and split distribution for better performance

**MGA Compliance**: Algorithms must work with versioned entries

#### Task 2.1: Implement Penalty Function (3-4 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp`

**What to Do**:
```cpp
double GistIndex::penalty(const BoundingBox& existing,
                         const BoundingBox& new_entry) const
{
    // Calculate area enlargement penalty
    // penalty = area(union(existing, new_entry)) - area(existing)
    // Lower penalty = better fit
}
```

**Acceptance Criteria**:
- [ ] Penalty correctly calculates area enlargement
- [ ] Zero penalty for contained entries
- [ ] Larger penalty for distant entries
- [ ] Handles edge cases (empty boxes, overlapping boxes)

**Code Location**: Replace basic logic at Line 559

**MGA Notes**: Penalty calculation does not need visibility checks

**Estimated Effort**: 3-4 hours

---

#### Task 2.2: Implement Optimal PickSplit (Quadratic Algorithm) (4-6 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp`

**What to Do**:
```cpp
Status GistIndex::pickSplit(const std::vector<GistEntry*>& entries,
                            std::vector<GistEntry*>* left_out,
                            std::vector<GistEntry*>* right_out,
                            ErrorContext* ctx)
{
    // Quadratic split algorithm (Guttman 1984):
    // 1. Pick two seed entries (maximize separation)
    // 2. Assign remaining entries to minimize penalty
    // 3. Balance distribution (40-60% ratio)
}
```

**Acceptance Criteria**:
- [ ] Seeds chosen maximize initial separation
- [ ] Entries distributed to minimize total area
- [ ] Distribution balanced (40-60% ratio)
- [ ] Bounding boxes minimized

**Code Location**: Replace basic logic at Line 572

**MGA Notes**: pickSplit must handle entries with different xmin values

**Estimated Effort**: 4-6 hours

---

#### Task 2.3: Unit Tests for Penalty/PickSplit (1-2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_gist_penalty.cpp` (NEW)

**Test Cases**:
1. Penalty for contained entry (should be zero)
2. Penalty for distant entry (should be large)
3. PickSplit with 100 random entries
4. Verify split minimizes overlap
5. Verify split balance (40-60% ratio)

**Acceptance Criteria**:
- [ ] All 5 tests pass
- [ ] Penalty values reasonable
- [ ] Split produces low overlap
- [ ] Split distribution balanced

**Estimated Effort**: 1-2 hours

---

### PHASE 3: Tree Rebalancing (10-15 hours)

**Goal**: Maintain balanced tree structure during inserts/deletes

**MGA Compliance**: Rebalancing must respect visibility, not disrupt active transactions

#### Task 3.1: Implement Basic Rebalancing Logic (4-6 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp`

**What to Do**:
```cpp
Status GistIndex::rebalance(uint64_t page_num, ErrorContext* ctx)
{
    // 1. Check if page is underfull (< 30% capacity)
    // 2. If underfull, merge with sibling or redistribute
    // 3. Update parent pointers
    // 4. Recursively check parent
}
```

**Acceptance Criteria**:
- [ ] Underfull pages merged with siblings
- [ ] Entries redistributed to balance occupancy
- [ ] Parent pointers updated correctly
- [ ] Tree height reduced if root becomes empty

**Code Location**: Replace stub at Line 551

**MGA Notes**: Rebalancing must not affect entries visible to active transactions

**Estimated Effort**: 4-6 hours

---

#### Task 3.2: Implement Page Merge (3-4 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp`

**What to Do**:
```cpp
Status mergePages(uint64_t left_page,
                 uint64_t right_page,
                 ErrorContext* ctx)
{
    // 1. Copy all entries from right_page to left_page
    // 2. Update parent to remove right_page pointer
    // 3. Mark right_page as deleted (set xmax)
    // 4. Update bounding box for left_page
}
```

**Acceptance Criteria**:
- [ ] All entries moved to left page
- [ ] Right page marked deleted (xmax set)
- [ ] Parent updated correctly
- [ ] No entries lost

**Code Location**: Add private helper method

**MGA Notes**: Merge must set xmax on deleted page, preserve entry xmin

**Estimated Effort**: 3-4 hours

---

#### Task 3.3: Unit Tests for Rebalancing (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_gist_rebalance.cpp` (NEW)

**Test Cases**:
1. Insert 100 entries, delete 80, verify rebalancing
2. Verify underfull pages merged
3. Verify tree height decreases after mass delete
4. Verify search works after rebalancing
5. Verify no entries lost during merge

**Acceptance Criteria**:
- [ ] All 5 tests pass
- [ ] Underfull pages handled correctly
- [ ] Tree structure valid after rebalance
- [ ] Search recall = 100%

**Estimated Effort**: 2-3 hours

---

#### Task 3.4: Integration Test: Delete Workload (1-2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_gist_delete.cpp` (NEW)

**Test Cases**:
1. Insert 10,000 entries
2. Delete 9,000 entries (90% deletion)
3. Verify rebalancing reduces tree size
4. Verify remaining 1,000 entries searchable

**Acceptance Criteria**:
- [ ] Deletes succeed
- [ ] Tree size reduced significantly
- [ ] Search works correctly
- [ ] No memory leaks

**Estimated Effort**: 1-2 hours

---

### PHASE 4: Vacuum Support (8-12 hours)

**Goal**: Reclaim space from dead entries and compact pages

**MGA Compliance**: Vacuum must only remove entries invisible to all active transactions

#### Task 4.1: Implement Entry-Level Vacuum (3-4 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp`

**What to Do**:
```cpp
Status GistIndex::vacuum(VacuumStats* stats_out, ErrorContext* ctx)
{
    // 1. Get oldest active transaction ID
    // 2. Scan all pages in tree
    // 3. Identify dead entries (xmax < oldest_active_xid)
    // 4. Remove dead entries from pages
    // 5. Compact pages (remove free space)
    // 6. Update statistics
}
```

**Acceptance Criteria**:
- [ ] Dead entries identified correctly
- [ ] Only entries invisible to all transactions removed
- [ ] Pages compacted after removal
- [ ] Statistics accurate (entries removed, bytes reclaimed)

**Code Location**: Replace stub at Line 590

**MGA Notes**: Vacuum must use getOldestActiveXid(), respect visibility

**Estimated Effort**: 3-4 hours

---

#### Task 4.2: Implement Page-Level Vacuum (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp`

**What to Do**:
```cpp
Status vacuumPage(uint64_t page_num,
                 uint64_t oldest_xid,
                 VacuumStats* stats,
                 ErrorContext* ctx)
{
    // 1. Pin page
    // 2. Scan entries, identify dead ones
    // 3. Create compacted entry list
    // 4. Clear page, copy compacted entries
    // 5. Update page metadata
}
```

**Acceptance Criteria**:
- [ ] Dead entries removed from page
- [ ] Live entries preserved
- [ ] Free space reclaimed
- [ ] Page metadata updated

**Code Location**: Add private helper method

**MGA Notes**: Page vacuum must preserve live entry xmin/xmax

**Estimated Effort**: 2-3 hours

---

#### Task 4.3: Implement Full Index Rebuild (Optional) (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp`

**What to Do**:
```cpp
Status rebuild(ErrorContext* ctx)
{
    // 1. Scan all live entries
    // 2. Build new optimized tree from scratch
    // 3. Swap old tree with new tree
    // 4. Delete old tree pages
}
```

**Acceptance Criteria**:
- [ ] Rebuild creates optimal tree structure
- [ ] All live entries preserved
- [ ] Old tree deleted
- [ ] Rebuild faster than incremental vacuum for large indexes

**Code Location**: Add new public method

**MGA Notes**: Rebuild must maintain entry TIDs (stable references)

**Estimated Effort**: 2-3 hours (OPTIONAL)

---

#### Task 4.4: Unit Tests for Vacuum (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_gist_vacuum.cpp` (NEW)

**Test Cases**:
1. Insert 100 entries, delete 50, vacuum
2. Verify 50 entries removed
3. Verify free space reclaimed
4. Search after vacuum returns correct results
5. Vacuum with no dead entries (no-op)
6. Verify statistics accurate

**Acceptance Criteria**:
- [ ] All 6 tests pass
- [ ] Dead entries removed correctly
- [ ] Live entries preserved
- [ ] Search works after vacuum

**Estimated Effort**: 2-3 hours

---

## 4. Progress Tracking

### Overall Completion Checklist

**Phase 1: Node Splitting (15-20 hours)**
- [ ] Task 1.1: Implement basic node split logic (5-7h)
- [ ] Task 1.2: Handle root page split (3-4h)
- [ ] Task 1.3: Implement parent pointer updates (2-3h)
- [ ] Task 1.4: Unit tests for node splitting (2-3h)
- [ ] Task 1.5: Integration test: large insert workload (1-2h)

**Phase 2: Penalty/PickSplit Algorithms (8-12 hours)**
- [ ] Task 2.1: Implement penalty function (3-4h)
- [ ] Task 2.2: Implement optimal pickSplit (4-6h)
- [ ] Task 2.3: Unit tests for penalty/pickSplit (1-2h)

**Phase 3: Tree Rebalancing (10-15 hours)**
- [ ] Task 3.1: Implement basic rebalancing logic (4-6h)
- [ ] Task 3.2: Implement page merge (3-4h)
- [ ] Task 3.3: Unit tests for rebalancing (2-3h)
- [ ] Task 3.4: Integration test: delete workload (1-2h)

**Phase 4: Vacuum Support (8-12 hours)**
- [ ] Task 4.1: Implement entry-level vacuum (3-4h)
- [ ] Task 4.2: Implement page-level vacuum (2-3h)
- [ ] Task 4.3: Implement full index rebuild (2-3h) [OPTIONAL]
- [ ] Task 4.4: Unit tests for vacuum (2-3h)

### Testing Checklist

**Unit Tests**:
- [ ] test_gist_split.cpp (7 tests)
- [ ] test_gist_penalty.cpp (5 tests)
- [ ] test_gist_rebalance.cpp (5 tests)
- [ ] test_gist_vacuum.cpp (6 tests)

**Integration Tests**:
- [ ] test_gist_large_insert.cpp (1 test)
- [ ] test_gist_delete.cpp (1 test)

**Total**: 25 new tests

### MGA Compliance Checklist

- [x] Current implementation uses TransactionId (uint64_t)
- [x] Entries have xmin/xmax fields
- [ ] splitNode() preserves entry xmin/xmax
- [ ] rebalance() respects visibility
- [ ] vacuum() only removes invisible entries
- [ ] All operations use TransactionManager for visibility
- [ ] No Snapshot structures used

---

## 5. Risk Mitigation

### 5.1 Technical Risks

**Risk 1: Complex Split Logic**
- **Problem**: Node splitting is complex, many edge cases
- **Mitigation**: Start with simple splits, add optimizations incrementally
- **Severity**: HIGH

**Risk 2: Tree Corruption During Split**
- **Problem**: Incorrect parent updates can break tree
- **Mitigation**: Validate tree structure after each split, comprehensive tests
- **Severity**: HIGH

**Risk 3: Performance Regression**
- **Problem**: Suboptimal pickSplit degrades search performance
- **Mitigation**: Use proven quadratic algorithm, benchmark against baseline
- **Severity**: MEDIUM

### 5.2 MGA Compliance Risks

**Risk**: Rebalancing violates visibility rules
- **Mitigation**: Always check xmax before removing entries
- **Prevention**: Re-read `/MGA_RULES.md` before Phase 3

### 5.3 Testing Risks

**Risk**: Edge cases in tree manipulation not covered
- **Mitigation**: Add property-based testing (invariants: all entries reachable, no orphans)
- **Prevention**: Code review after each phase

---

## 6. Total Effort Estimate

### 6.1 Effort Breakdown

| Phase | Tasks | Hours (Min-Max) | Hours (Realistic) |
|-------|-------|-----------------|-------------------|
| Phase 1: Node Splitting | 5 | 15-20 | 18 |
| Phase 2: Penalty/PickSplit | 3 | 8-12 | 10 |
| Phase 3: Tree Rebalancing | 4 | 10-15 | 13 |
| Phase 4: Vacuum Support | 4 | 8-12 | 10 |
| **TOTAL** | **16** | **41-59** | **51** |

**Buffer for debugging/edge cases**: +9 hours
**TOTAL WITH BUFFER**: 40-60 hours

### 6.2 Timeline Estimates

**Single Developer (Full-Time)**:
- Optimistic: 5-6 days
- Realistic: 7-8 days
- Conservative: 10-12 days

**Part-Time Development**:
- Realistic: 2-3 weeks

### 6.3 Critical Path

**Longest Dependency Chain**:
1. Phase 1 (Node Splitting) → 15-20 hours (CRITICAL, blocks all others)
2. Phase 2 (Penalty/PickSplit) → 8-12 hours (depends on Phase 1)
3. Phase 3 (Rebalancing) → 10-15 hours (depends on Phase 1)
4. Phase 4 (Vacuum) → 8-12 hours (independent, can parallelize)

**Recommended Order**:
1. Phase 1 (Node Splitting) - CRITICAL for index growth
2. Phase 2 (Penalty/PickSplit) - Improves split quality
3. Phase 3 (Rebalancing) - Maintains performance after deletes
4. Phase 4 (Vacuum) - Reclaims space

---

## 7. Success Criteria

### 7.1 Functional Completion

**Must Have**:
- [ ] All 25 tests pass
- [ ] Index can grow beyond single page
- [ ] Tree remains balanced during inserts/deletes
- [ ] Vacuum reclaims space correctly
- [ ] Search works correctly after all operations

### 7.2 Performance Targets

**Must Achieve**:
- [ ] Insert: < 1ms per entry (average)
- [ ] Search: < 10ms for 100K entries
- [ ] Split: < 50ms per operation
- [ ] Rebalance: < 100ms per operation
- [ ] Vacuum: < 5 seconds for 100K entries

### 7.3 MGA Compliance

**Must Verify**:
- [ ] All operations respect xmin/xmax visibility
- [ ] No Snapshot structures used
- [ ] Visibility checks via TransactionManager::isVersionVisible()
- [ ] Deleted entries invisible to new transactions
- [ ] Deleted entries visible to old transactions
- [ ] Tree modifications don't disrupt active transactions

---

## 8. References

**Specification**: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/GIST_INDEX_COMPLETION_SPEC.md`
**Implementation**: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp` (633 lines)
**MGA Rules**: `/home/dcalford/CliWork/ScratchBird/MGA_RULES.md`
**Project Context**: `/home/dcalford/CliWork/ScratchBird/PROJECT_CONTEXT.md`

**Academic References**:
- Guttman, A. (1984). "R-Trees: A Dynamic Index Structure for Spatial Searching"
- Hellerstein et al. (1995). "Generalized Search Trees for Database Systems"

---

**Document Version**: 1.0
**Created**: 2025-11-04
**Status**: READY FOR IMPLEMENTATION
**Next Action**: Begin Phase 1 (Node Splitting)

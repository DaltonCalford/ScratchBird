# SP-GiST Index - Implementation Completion Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Project**: ScratchBird Database Engine
**Component**: SP-GiST (Space-Partitioned Generalized Search Tree) Index - Complete Remaining Features
**Status**: 50% Complete (Basic quad-tree structure, missing split/rebalance/vacuum)
**Estimated Effort**: 30-40 hours
**Priority**: MEDIUM (Core structure works, missing production features)
**Created**: 2025-11-04

---

## ⚠️ CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules for SP-GiST Index**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- All SP-GiST operations must respect xmin/xmax visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- NO PostgreSQL MVCC contamination
- SP-GiST nodes reference stable TIDs (never change)
- Partition-based splits must maintain version chains

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

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/spgist_index.cpp` (526 lines)

**Implemented Features**:
- ✅ SP-GiST page structure (SBSpgistPage, SBSpgistNode)
- ✅ Basic quad-tree partitioning (4-way splits for 2D data)
- ✅ Insert with leaf node creation
- ✅ Scan with partition pruning
- ✅ MGA compliance (xmin/xmax on nodes)
- ✅ Root page allocation and initialization
- ✅ Node management (inner nodes, leaf nodes)
- ✅ Prefix compression for leaf values

### 1.2 What's Missing (50% = 30-40 hours)

**Missing Feature 1**: Node splitting (Lines 280-286)
- **Current**: Stub that logs "split needed" but doesn't split
- **Required**: Split full nodes, repartition entries
- **Impact**: Index cannot grow beyond one page, insert fails on overflow
- **Effort**: 12-15 hours

**Missing Feature 2**: Choose partitioning strategy (Lines 390-396)
- **Current**: Hardcoded quad-tree, not optimal for all data types
- **Required**: Dynamic partitioning based on data distribution
- **Impact**: Poor space utilization for non-uniform data
- **Effort**: 8-10 hours

**Missing Feature 3**: Vacuum support (Lines 469-488)
- **Current**: Stub that logs vacuum but doesn't reclaim space
- **Required**: Remove dead nodes, compact pages
- **Impact**: Dead nodes accumulate, wasting space
- **Effort**: 8-10 hours

**Missing Feature 4**: Statistics (Lines 490-495)
- **Current**: Returns placeholder values (deleted_nodes=0, avg_depth=0.0)
- **Required**: Calculate actual tree depth and node statistics
- **Impact**: Cannot monitor index health
- **Effort**: 3-5 hours

### 1.3 Code Locations

**Reference File**: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SPGIST_INDEX_COMPLETION_SPEC.md`

**Key Functions**:
- `SpgistIndex::splitNode()` - Line 280 (STUB)
- `SpgistIndex::choosePartition()` - Line 390 (HARDCODED)
- `SpgistIndex::vacuum()` - Line 469 (STUB)
- `SpgistIndex::getStats()` - Line 490 (PLACEHOLDER)

---

## 2. Implementation Phases

### Phase 1: Node Splitting (12-15 hours)
**Goal**: Implement partition-based node split for handling overflow

### Phase 2: Partitioning Strategy (8-10 hours)
**Goal**: Dynamic partition selection based on data distribution

### Phase 3: Vacuum Support (8-10 hours)
**Goal**: Reclaim space from dead nodes and compact pages

### Phase 4: Statistics Calculation (3-5 hours)
**Goal**: Calculate real tree depth and node statistics

---

## 3. Phase-by-Phase Tasks

---

### PHASE 1: Node Splitting (12-15 hours)

**Goal**: Implement `splitNode()` for partition-based node splitting

**MGA Compliance**: Split must preserve xmin/xmax, maintain version chains

#### Task 1.1: Implement Basic Node Split Logic (5-6 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/spgist_index.cpp`

**What to Do**:
```cpp
Status SpgistIndex::splitNode(uint64_t page_num, ErrorContext* ctx)
{
    // 1. Pin current page
    // 2. Determine partitioning strategy (choosePartition)
    // 3. Allocate child pages for partitions
    // 4. Distribute entries to partitions
    // 5. Create inner node with partition boundaries
    // 6. Update parent pointers
}
```

**Acceptance Criteria**:
- [ ] Split distributes entries evenly across partitions
- [ ] Partition boundaries minimize overlap
- [ ] Inner node created with correct partition info
- [ ] No entries lost during split

**Code Location**: Replace stub at Line 280

**MGA Notes**: Split must preserve node xmin/xmax, use getCurrentXid() for new pages

**Estimated Effort**: 5-6 hours

---

#### Task 1.2: Implement Partition Distribution Logic (3-4 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/spgist_index.cpp`

**What to Do**:
```cpp
Status distributeToPartitions(const std::vector<SpgistEntry*>& entries,
                             const PartitionScheme& scheme,
                             std::vector<std::vector<SpgistEntry*>>* partitions_out,
                             ErrorContext* ctx)
{
    // 1. For each entry, determine target partition
    // 2. Assign entry to partition based on key value
    // 3. Validate partition balance (no empty partitions)
}
```

**Acceptance Criteria**:
- [ ] Entries correctly assigned to partitions
- [ ] Partition balance reasonable (no partition > 70%)
- [ ] No empty partitions created
- [ ] Partition boundaries non-overlapping

**Code Location**: Add private helper method

**MGA Notes**: Distribution must handle entries with different xmin values

**Estimated Effort**: 3-4 hours

---

#### Task 1.3: Handle Root Page Split (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/spgist_index.cpp`

**What to Do**:
```cpp
Status SpgistIndex::splitRoot(ErrorContext* ctx)
{
    // 1. Allocate new child pages for partitions
    // 2. Move root entries to children
    // 3. Create inner node in root with partition info
    // 4. Increase tree height
}
```

**Acceptance Criteria**:
- [ ] Root split increases tree height by 1
- [ ] All entries migrated to child partitions
- [ ] Root contains only partition metadata
- [ ] Tree structure valid

**Code Location**: Add new method after splitNode()

**MGA Notes**: Root split must maintain version chains, update catalog

**Estimated Effort**: 2-3 hours

---

#### Task 1.4: Unit Tests for Node Splitting (2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_spgist_split.cpp` (NEW)

**Test Cases**:
1. Split leaf node with 100 entries (quad-tree)
2. Split inner node with partition info
3. Split root node (increases tree height)
4. Verify entries preserved after split
5. Verify partition boundaries correct
6. Search after split returns correct results

**Acceptance Criteria**:
- [ ] All 6 tests pass
- [ ] No entries lost during split
- [ ] Tree structure valid after split
- [ ] Search works correctly after split

**Estimated Effort**: 2 hours

---

### PHASE 2: Partitioning Strategy (8-10 hours)

**Goal**: Dynamic partition selection based on data distribution

**MGA Compliance**: Partitioning must work with versioned nodes

#### Task 2.1: Implement Data Distribution Analysis (3-4 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/spgist_index.cpp`

**What to Do**:
```cpp
PartitionScheme SpgistIndex::analyzeDistribution(
    const std::vector<SpgistEntry*>& entries) const
{
    // 1. Calculate data statistics (min, max, median)
    // 2. Detect data distribution pattern (uniform, skewed, clustered)
    // 3. Choose partitioning scheme (quad-tree, k-d tree, radix)
    // 4. Return partition boundaries
}
```

**Acceptance Criteria**:
- [ ] Detects uniform distribution (quad-tree optimal)
- [ ] Detects skewed distribution (k-d tree optimal)
- [ ] Detects clustered distribution (adaptive partitions)
- [ ] Partition boundaries minimize overlap

**Code Location**: Add new private method

**MGA Notes**: Analysis does not need visibility checks (works on snapshot)

**Estimated Effort**: 3-4 hours

---

#### Task 2.2: Implement choosePartition() with Multiple Strategies (3-4 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/spgist_index.cpp`

**What to Do**:
```cpp
Status SpgistIndex::choosePartition(const std::vector<SpgistEntry*>& entries,
                                   PartitionScheme* scheme_out,
                                   ErrorContext* ctx)
{
    // 1. Analyze data distribution
    // 2. Select partitioning strategy:
    //    - Quad-tree (default for 2D uniform data)
    //    - K-d tree (for high-dimensional or skewed data)
    //    - Radix tree (for string/prefix data)
    // 3. Compute partition boundaries
}
```

**Acceptance Criteria**:
- [ ] Chooses quad-tree for uniform 2D data
- [ ] Chooses k-d tree for skewed data
- [ ] Chooses radix tree for string data
- [ ] Partition boundaries optimal for chosen strategy

**Code Location**: Replace hardcoded logic at Line 390

**MGA Notes**: choosePartition must handle entries with different xmin values

**Estimated Effort**: 3-4 hours

---

#### Task 2.3: Unit Tests for Partitioning Strategy (2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_spgist_partition.cpp` (NEW)

**Test Cases**:
1. Uniform 2D data → quad-tree selected
2. Skewed 2D data → k-d tree selected
3. String data → radix tree selected
4. Verify partition boundaries minimize overlap
5. Verify partition balance (no partition > 70%)

**Acceptance Criteria**:
- [ ] All 5 tests pass
- [ ] Strategy selection correct for data type
- [ ] Partition boundaries optimal
- [ ] Partition balance reasonable

**Estimated Effort**: 2 hours

---

### PHASE 3: Vacuum Support (8-10 hours)

**Goal**: Reclaim space from dead nodes and compact pages

**MGA Compliance**: Vacuum must only remove nodes invisible to all active transactions

#### Task 3.1: Implement Node-Level Vacuum (3-4 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/spgist_index.cpp`

**What to Do**:
```cpp
Status SpgistIndex::vacuum(VacuumStats* stats_out, ErrorContext* ctx)
{
    // 1. Get oldest active transaction ID
    // 2. Traverse tree (depth-first)
    // 3. Identify dead nodes (xmax < oldest_active_xid)
    // 4. Remove dead leaf nodes
    // 5. Compact inner nodes (remove empty partitions)
    // 6. Update statistics
}
```

**Acceptance Criteria**:
- [ ] Dead nodes identified correctly
- [ ] Only nodes invisible to all transactions removed
- [ ] Empty partitions removed from inner nodes
- [ ] Statistics accurate (nodes removed, bytes reclaimed)

**Code Location**: Replace stub at Line 469

**MGA Notes**: Vacuum must use getOldestActiveXid(), respect visibility

**Estimated Effort**: 3-4 hours

---

#### Task 3.2: Implement Page-Level Compaction (3-4 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/spgist_index.cpp`

**What to Do**:
```cpp
Status compactPage(uint64_t page_num,
                  uint64_t oldest_xid,
                  VacuumStats* stats,
                  ErrorContext* ctx)
{
    // 1. Pin page
    // 2. Scan nodes, identify dead ones
    // 3. Create compacted node list
    // 4. Clear page, copy compacted nodes
    // 5. Update page metadata
}
```

**Acceptance Criteria**:
- [ ] Dead nodes removed from page
- [ ] Live nodes preserved
- [ ] Free space reclaimed
- [ ] Page metadata updated

**Code Location**: Add private helper method

**MGA Notes**: Page compaction must preserve live node xmin/xmax

**Estimated Effort**: 3-4 hours

---

#### Task 3.3: Unit Tests for Vacuum (2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_spgist_vacuum.cpp` (NEW)

**Test Cases**:
1. Insert 100 nodes, delete 50, vacuum
2. Verify 50 nodes removed
3. Verify free space reclaimed
4. Search after vacuum returns correct results
5. Vacuum with no dead nodes (no-op)
6. Verify statistics accurate

**Acceptance Criteria**:
- [ ] All 6 tests pass
- [ ] Dead nodes removed correctly
- [ ] Live nodes preserved
- [ ] Search works after vacuum

**Estimated Effort**: 2 hours

---

### PHASE 4: Statistics Calculation (3-5 hours)

**Goal**: Calculate real tree depth and node statistics

**MGA Compliance**: Statistics must reflect visible nodes only

#### Task 4.1: Implement Tree Depth Calculation (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/spgist_index.cpp`

**What to Do**:
```cpp
Status SpgistIndex::getStats(SpgistStats* stats_out, ErrorContext* ctx)
{
    // 1. Traverse tree (depth-first)
    // 2. Track max depth reached
    // 3. Count total nodes (inner + leaf)
    // 4. Count deleted nodes (xmax != 0)
    // 5. Calculate average partition fanout
    // 6. Calculate average depth
}
```

**Acceptance Criteria**:
- [ ] Tree depth correctly calculated
- [ ] Node counts accurate
- [ ] Deleted nodes counted correctly
- [ ] Average depth reasonable (log scale)

**Code Location**: Replace placeholder at Line 490

**MGA Notes**: Only count nodes visible to current transaction

**Estimated Effort**: 2-3 hours

---

#### Task 4.2: Unit Tests for Statistics (1-2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_spgist_stats.cpp` (NEW)

**Test Cases**:
1. Insert 100 nodes, verify total_nodes = 100
2. Delete 20 nodes, verify deleted_nodes = 20
3. Verify tree depth ≈ log_k(N) where k = partition fanout
4. Verify average depth reasonable
5. Empty index returns all stats as 0

**Acceptance Criteria**:
- [ ] All 5 tests pass
- [ ] Node counts accurate
- [ ] Tree depth matches expected value
- [ ] Statistics calculation fast (< 100ms for 10K nodes)

**Estimated Effort**: 1-2 hours

---

## 4. Progress Tracking

### Overall Completion Checklist

**Phase 1: Node Splitting (12-15 hours)**
- [ ] Task 1.1: Implement basic node split logic (5-6h)
- [ ] Task 1.2: Implement partition distribution logic (3-4h)
- [ ] Task 1.3: Handle root page split (2-3h)
- [ ] Task 1.4: Unit tests for node splitting (2h)

**Phase 2: Partitioning Strategy (8-10 hours)**
- [ ] Task 2.1: Implement data distribution analysis (3-4h)
- [ ] Task 2.2: Implement choosePartition() with multiple strategies (3-4h)
- [ ] Task 2.3: Unit tests for partitioning strategy (2h)

**Phase 3: Vacuum Support (8-10 hours)**
- [ ] Task 3.1: Implement node-level vacuum (3-4h)
- [ ] Task 3.2: Implement page-level compaction (3-4h)
- [ ] Task 3.3: Unit tests for vacuum (2h)

**Phase 4: Statistics Calculation (3-5 hours)**
- [ ] Task 4.1: Implement tree depth calculation (2-3h)
- [ ] Task 4.2: Unit tests for statistics (1-2h)

### Testing Checklist

**Unit Tests**:
- [ ] test_spgist_split.cpp (6 tests)
- [ ] test_spgist_partition.cpp (5 tests)
- [ ] test_spgist_vacuum.cpp (6 tests)
- [ ] test_spgist_stats.cpp (5 tests)

**Total**: 22 new tests

### MGA Compliance Checklist

- [x] Current implementation uses TransactionId (uint64_t)
- [x] Nodes have xmin/xmax fields
- [ ] splitNode() preserves node xmin/xmax
- [ ] choosePartition() works with versioned nodes
- [ ] vacuum() only removes invisible nodes
- [ ] All operations use TransactionManager for visibility
- [ ] No Snapshot structures used

---

## 5. Risk Mitigation

### 5.1 Technical Risks

**Risk 1: Complex Partitioning Logic**
- **Problem**: Multiple partitioning strategies increase complexity
- **Mitigation**: Start with quad-tree only, add others incrementally
- **Severity**: MEDIUM

**Risk 2: Partition Imbalance**
- **Problem**: Poor partitioning leads to skewed tree
- **Mitigation**: Add rebalancing logic, validate partition balance
- **Severity**: MEDIUM

**Risk 3: Performance Regression**
- **Problem**: Dynamic partitioning overhead may slow inserts
- **Mitigation**: Cache partition decisions, benchmark against baseline
- **Severity**: LOW

### 5.2 MGA Compliance Risks

**Risk**: Vacuum removes nodes visible to active transactions
- **Mitigation**: Always check xmax before removing nodes
- **Prevention**: Re-read `/MGA_RULES.md` before Phase 3

### 5.3 Testing Risks

**Risk**: Edge cases in partition logic not covered
- **Mitigation**: Add property-based testing (invariants: all entries reachable)
- **Prevention**: Code review after each phase

---

## 6. Total Effort Estimate

### 6.1 Effort Breakdown

| Phase | Tasks | Hours (Min-Max) | Hours (Realistic) |
|-------|-------|-----------------|-------------------|
| Phase 1: Node Splitting | 4 | 12-15 | 14 |
| Phase 2: Partitioning Strategy | 3 | 8-10 | 9 |
| Phase 3: Vacuum Support | 3 | 8-10 | 9 |
| Phase 4: Statistics | 2 | 3-5 | 4 |
| **TOTAL** | **12** | **31-40** | **36** |

**Buffer for debugging/edge cases**: +4 hours
**TOTAL WITH BUFFER**: 30-40 hours

### 6.2 Timeline Estimates

**Single Developer (Full-Time)**:
- Optimistic: 4-5 days
- Realistic: 5-6 days
- Conservative: 7-8 days

**Part-Time Development**:
- Realistic: 1.5-2 weeks

### 6.3 Critical Path

**Longest Dependency Chain**:
1. Phase 1 (Node Splitting) → 12-15 hours (CRITICAL, blocks others)
2. Phase 2 (Partitioning Strategy) → 8-10 hours (depends on Phase 1)
3. Phase 3 (Vacuum) → 8-10 hours (independent, can parallelize)
4. Phase 4 (Statistics) → 3-5 hours (independent, can parallelize)

**Recommended Order**:
1. Phase 1 (Node Splitting) - CRITICAL for index growth
2. Phase 2 (Partitioning Strategy) - Improves split quality
3. Phase 3 (Vacuum) - Reclaims space
4. Phase 4 (Statistics) - Monitoring and diagnostics

---

## 7. Success Criteria

### 7.1 Functional Completion

**Must Have**:
- [ ] All 22 tests pass
- [ ] Index can grow beyond single page
- [ ] Partitioning strategy adapts to data
- [ ] Vacuum reclaims space correctly
- [ ] Statistics accurate

### 7.2 Performance Targets

**Must Achieve**:
- [ ] Insert: < 1ms per entry (average)
- [ ] Search: < 10ms for 100K entries
- [ ] Split: < 50ms per operation
- [ ] Vacuum: < 5 seconds for 100K entries
- [ ] Statistics: < 100ms for 10K nodes

### 7.3 MGA Compliance

**Must Verify**:
- [ ] All operations respect xmin/xmax visibility
- [ ] No Snapshot structures used
- [ ] Visibility checks via TransactionManager::isVersionVisible()
- [ ] Deleted nodes invisible to new transactions
- [ ] Deleted nodes visible to old transactions

---

## 8. References

**Specification**: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SPGIST_INDEX_COMPLETION_SPEC.md`
**Implementation**: `/home/dcalford/CliWork/ScratchBird/src/core/spgist_index.cpp` (526 lines)
**MGA Rules**: `/home/dcalford/CliWork/ScratchBird/MGA_RULES.md`
**Project Context**: `/home/dcalford/CliWork/ScratchBird/PROJECT_CONTEXT.md`

**Academic References**:
- Aref & Samet (1994). "Optimization Strategies for Spatial Query Processing"
- Kornacker et al. (2000). "High-Concurrency Locking in R-Trees"

---

**Document Version**: 1.0
**Created**: 2025-11-04
**Status**: READY FOR IMPLEMENTATION
**Next Action**: Begin Phase 1 (Node Splitting)

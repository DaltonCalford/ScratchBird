# HNSW Index - Implementation Completion Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Project**: ScratchBird Database Engine
**Component**: HNSW (Hierarchical Navigable Small World) Index - Complete Remaining Features
**Status**: 80% Complete (Core k-NN search functional, missing graph maintenance)
**Estimated Effort**: 30-40 hours
**Priority**: MEDIUM (Search works, missing features improve quality and maintenance)
**Created**: 2025-11-04

---

## ⚠️ CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules for HNSW Index**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- All HNSW operations must respect xmin/xmax visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- NO PostgreSQL MVCC contamination
- Version traversal follows stable TIDs (nodes never move)
- Graph modifications must maintain version chains

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

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/hnsw_index.cpp` (1,147 lines)

**Implemented Features**:
- ✅ Multi-layer graph construction with exponential layer selection
- ✅ k-NN search with greedy best-first search algorithm
- ✅ Layer selection via exponential decay (1/ln(M) probability)
- ✅ Entry point management (highest layer node)
- ✅ Distance functions (Euclidean, Cosine, Inner Product)
- ✅ Node creation with neighbor list storage
- ✅ MGA compliance (xmin/xmax visibility via `is_node_visible()`)
- ✅ Soft deletion (sets xmax, keeps links intact)
- ✅ Vacuum infrastructure (counts dead nodes)
- ✅ Root page allocation and initialization
- ✅ Thread-safe TID conversion (legacy format)

### 1.2 What's Missing (20% = 30-40 hours)

**Missing Feature 1**: Link management (Lines 788-803)
- **Current**: `add_link()` and `remove_link()` are stubs that only log
- **Required**: Actual graph edge manipulation (add/remove neighbor connections)
- **Impact**: Cannot maintain bi-directional links, graph connectivity degrades
- **Effort**: 15-20 hours

**Missing Feature 2**: Connection pruning (Lines 851-857)
- **Current**: `prune_connections()` is stub that only logs
- **Required**: Heuristic pruning to maintain M connections per node
- **Impact**: Node degree unbounded, graph quality degrades, search slows
- **Effort**: 10-15 hours

**Missing Feature 3**: Statistics calculation (Lines 488-492)
- **Current**: Returns placeholder values (deleted_nodes=0, avg_connections=0.0, avg_path_length=0.0)
- **Required**: Actual calculation from graph traversal
- **Impact**: Cannot monitor index health, no performance metrics
- **Effort**: 5-10 hours

### 1.3 Code Locations

**Reference File**: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/HNSW_INDEX_COMPLETION_SPEC.md`

**Key Functions**:
- `HnswIndex::add_link()` - Line 788 (STUB - logs only)
- `HnswIndex::remove_link()` - Line 798 (STUB - logs only)
- `HnswIndex::prune_connections()` - Line 851 (STUB - logs only)
- `HnswIndex::getStats()` - Line 488 (PLACEHOLDER)

---

## 2. Implementation Phases

### Phase 1: Link Management (15-20 hours) - CRITICAL
**Goal**: Implement add_link() and remove_link() for bi-directional graph

### Phase 2: Connection Pruning (10-15 hours) - IMPORTANT
**Goal**: Maintain M connections per node with heuristic pruning

### Phase 3: Statistics Calculation (5-10 hours) - NICE TO HAVE
**Goal**: Calculate real graph statistics for monitoring

---

## 3. Phase-by-Phase Tasks

---

### PHASE 1: Link Management (15-20 hours) - CRITICAL

**Goal**: Implement actual graph edge manipulation with page reorganization

**MGA Compliance**: Link operations must preserve node xmin/xmax, stable TIDs

#### Task 1.1: Add Node Size Calculation Helpers (1-2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/hnsw_index.cpp`

**What to Do**:
```cpp
// Add to hnsw_index.h private section:
size_t calculate_node_size(const SBHnswNode* node) const;
size_t calculate_node_size(uint16_t num_neighbors, uint16_t vector_len) const;

// Implementation:
size_t HnswIndex::calculate_node_size(const SBHnswNode* node) const
{
    return sizeof(SBHnswNode) +
           node->node_num_neighbors * sizeof(uint64_t) +
           node->node_vector_len;
}
```

**Acceptance Criteria**:
- [ ] Size calculation accounts for variable neighbors
- [ ] Size calculation accounts for vector data
- [ ] Handles edge cases (zero neighbors, zero vector)
- [ ] Accurate for all node configurations

**Code Location**: Add after Line 660

**MGA Notes**: Size calculation is metadata only, no visibility checks needed

**Estimated Effort**: 1-2 hours

---

#### Task 1.2: Implement Page Reorganization Helper (3-5 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/hnsw_index.cpp`

**What to Do**:
```cpp
Status HnswIndex::reorganize_page_for_node_update(
    uint64_t page_num,
    uint64_t target_tid,
    uint16_t new_num_neighbors,
    const std::vector<uint64_t>& new_neighbors,
    ErrorContext* ctx)
{
    // 1. Pin page
    // 2. Scan nodes, collect in temp buffer
    // 3. Update target node with new neighbors
    // 4. Clear page, copy reorganized data back
    // 5. Update page metadata
    // 6. Mark page dirty
}
```

**Acceptance Criteria**:
- [ ] Page reorganization handles variable-size nodes
- [ ] Target node updated with new neighbor list
- [ ] Other nodes preserved unchanged
- [ ] Page metadata accurate after reorganization
- [ ] Returns PAGE_FULL if insufficient space

**Code Location**: Add new private method after Line 803

**MGA Notes**: Must preserve all node xmin/xmax values during reorganization

**Estimated Effort**: 3-5 hours

---

#### Task 1.3: Implement add_link() (5-7 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/hnsw_index.cpp`

**What to Do**:
```cpp
Status HnswIndex::add_link(uint64_t from_tid, uint64_t to_tid,
                           uint16_t layer, ErrorContext* ctx)
{
    // 1. Find source node (from_tid)
    // 2. Check if link already exists (idempotent)
    // 3. Check if node at max connections (trigger prune)
    // 4. Build new neighbor list with added link
    // 5. Reorganize page to update node
    // 6. Log success
}
```

**Acceptance Criteria**:
- [ ] Link added to neighbor list
- [ ] Idempotent (adding existing link is no-op)
- [ ] Triggers pruning if node at max M
- [ ] Page reorganization successful
- [ ] Graph remains navigable

**Code Location**: Replace stub at Line 788

**MGA Notes**: add_link must not change node_tuple_id (stable TID)

**Estimated Effort**: 5-7 hours

---

#### Task 1.4: Implement remove_link() (5-7 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/hnsw_index.cpp`

**What to Do**:
```cpp
Status HnswIndex::remove_link(uint64_t from_tid, uint64_t to_tid,
                              uint16_t layer, ErrorContext* ctx)
{
    // 1. Find source node (from_tid)
    // 2. Check if link exists
    // 3. Build new neighbor list without removed link
    // 4. Reorganize page to update node
    // 5. Log success
}
```

**Acceptance Criteria**:
- [ ] Link removed from neighbor list
- [ ] Idempotent (removing non-existent link is no-op)
- [ ] Page reorganization successful
- [ ] Graph remains navigable
- [ ] Node size reduced correctly

**Code Location**: Replace stub at Line 798

**MGA Notes**: remove_link must not change node_tuple_id (stable TID)

**Estimated Effort**: 5-7 hours

---

#### Task 1.5: Unit Tests for Link Management (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_hnsw_links.cpp` (NEW)

**Test Cases**:
1. Insert 10 nodes, verify all bi-directional links created
2. Add link to node, verify neighbor count increases
3. Remove link from node, verify neighbor count decreases
4. Add link that already exists (idempotent)
5. Remove link that doesn't exist (idempotent)
6. Add link when node is at max connections (triggers pruning)
7. Add link that causes page overflow (returns PAGE_FULL)
8. Verify graph connectivity after 100 inserts with random links

**Acceptance Criteria**:
- [ ] All 8 tests pass
- [ ] Bi-directional links maintained
- [ ] Page reorganization preserves other nodes
- [ ] Node size calculations correct
- [ ] Graph navigable

**Estimated Effort**: 2-3 hours

---

### PHASE 2: Connection Pruning (10-15 hours) - IMPORTANT

**Goal**: Maintain M connections per node with heuristic pruning algorithm

**MGA Compliance**: Pruning must preserve node xmin/xmax

#### Task 2.1: Implement get_node_vector() Helper (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/hnsw_index.cpp`

**What to Do**:
```cpp
Status HnswIndex::get_node_vector(uint64_t tuple_id,
                                  VectorValue* vector_out,
                                  ErrorContext* ctx)
{
    // 1. Find node (tuple_id)
    // 2. Extract vector data from node
    // 3. Deserialize to VectorValue
    // 4. Return vector
}
```

**Acceptance Criteria**:
- [ ] Correctly extracts vector from node
- [ ] Handles different vector dimensions
- [ ] Handles different data types (float32, int8)
- [ ] Unpins page after extraction

**Code Location**: Add new private method after Line 665

**MGA Notes**: get_node_vector must check node visibility (xmin/xmax)

**Estimated Effort**: 2-3 hours

---

#### Task 2.2: Implement prune_connections() with Heuristic (6-10 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/hnsw_index.cpp`

**What to Do**:
```cpp
Status HnswIndex::prune_connections(uint64_t node_tid, uint16_t layer,
                                   ErrorContext* ctx)
{
    // Heuristic pruning algorithm (HNSW paper):
    // 1. If neighbors <= M, no-op
    // 2. Get node's vector
    // 3. Build candidate list with distances
    // 4. Sort candidates by distance (closest first)
    // 5. Select M diverse neighbors (minimize redundancy)
    // 6. Update node with pruned neighbor list
}
```

**Acceptance Criteria**:
- [ ] Pruning reduces neighbors to M
- [ ] Selected neighbors are diverse (heuristic)
- [ ] Search quality not degraded after pruning
- [ ] Graph remains connected
- [ ] Performance acceptable (< 50ms per node)

**Code Location**: Replace stub at Line 851

**MGA Notes**: Pruning must preserve node xmin/xmax values

**Estimated Effort**: 6-10 hours

---

#### Task 2.3: Unit Tests for Connection Pruning (2-3 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_hnsw_pruning.cpp` (NEW)

**Test Cases**:
1. Insert 100 nodes with M=16, verify no node exceeds M connections
2. Force pruning by adding 20 links to single node
3. Verify pruned neighbors are most diverse (not all from same cluster)
4. Verify search quality after pruning (recall >= 95%)
5. Prune node with M=16 neighbors to M=16 (no-op)
6. Concurrent inserts with pruning (stress test)
7. Verify graph connectivity preserved after aggressive pruning

**Acceptance Criteria**:
- [ ] All 7 tests pass
- [ ] Pruning maintains M connections
- [ ] Selected neighbors are diverse
- [ ] Search quality preserved
- [ ] Graph connected

**Estimated Effort**: 2-3 hours

---

### PHASE 3: Statistics Calculation (5-10 hours) - NICE TO HAVE

**Goal**: Calculate real graph statistics for monitoring and diagnostics

**MGA Compliance**: Statistics must reflect visible nodes only

#### Task 3.1: Implement Basic Statistics (Scan-Based) (3-6 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/hnsw_index.cpp`

**What to Do**:
```cpp
Status HnswIndex::getStats(HnswStats* stats_out, ErrorContext* ctx)
{
    // 1. Scan root page
    // 2. Count total nodes
    // 3. Count deleted nodes (xmax != 0)
    // 4. Sum total connections
    // 5. Calculate average connections
    // 6. Estimate average path length (log formula)
    // 7. Track max layer
}
```

**Acceptance Criteria**:
- [ ] Total nodes accurate
- [ ] Deleted nodes counted correctly
- [ ] Average connections matches expected (≈ M)
- [ ] Max layer reasonable
- [ ] Average path length ≈ log(N)

**Code Location**: Replace placeholder at Line 488

**MGA Notes**: Only count nodes visible to current transaction

**Estimated Effort**: 3-6 hours

---

#### Task 3.2: Optional: Sample-Based Path Length (2-4 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/src/core/hnsw_index.cpp`

**What to Do**:
```cpp
double HnswIndex::estimate_avg_path_length_sampling(uint32_t num_samples,
                                                     ErrorContext* ctx)
{
    // 1. Generate random query vectors
    // 2. Perform k-NN search for each
    // 3. Track path length (hops taken)
    // 4. Calculate mean path length
}
```

**Acceptance Criteria**:
- [ ] Sampling produces accurate estimate
- [ ] Path length ≈ log(N) (within 50%)
- [ ] Sampling fast (< 1 second for 100 samples)
- [ ] Results stable across runs

**Code Location**: Add new private method

**MGA Notes**: Sampling works on snapshot, no visibility issues

**Estimated Effort**: 2-4 hours (OPTIONAL)

---

#### Task 3.3: Unit Tests for Statistics (1-2 hours)

**File**: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_hnsw_stats.cpp` (NEW)

**Test Cases**:
1. Insert 100 nodes, verify total_nodes = 100
2. Delete 20 nodes, verify deleted_nodes = 20
3. Verify avg_connections ≈ M (within 20%)
4. Verify max_layer increases with index size
5. Verify avg_path_length ≈ log(N) (within 50%)
6. Empty index returns all stats as 0
7. Statistics consistent across multiple calls

**Acceptance Criteria**:
- [ ] All 7 tests pass
- [ ] Node counts accurate
- [ ] Average connections match expected
- [ ] Path length reasonable
- [ ] Performance acceptable (< 100ms for 10K nodes)

**Estimated Effort**: 1-2 hours

---

## 4. Progress Tracking

### Overall Completion Checklist

**Phase 1: Link Management (15-20 hours) - CRITICAL**
- [ ] Task 1.1: Add node size calculation helpers (1-2h)
- [ ] Task 1.2: Implement page reorganization helper (3-5h)
- [ ] Task 1.3: Implement add_link() (5-7h)
- [ ] Task 1.4: Implement remove_link() (5-7h)
- [ ] Task 1.5: Unit tests for link management (2-3h)

**Phase 2: Connection Pruning (10-15 hours) - IMPORTANT**
- [ ] Task 2.1: Implement get_node_vector() helper (2-3h)
- [ ] Task 2.2: Implement prune_connections() with heuristic (6-10h)
- [ ] Task 2.3: Unit tests for connection pruning (2-3h)

**Phase 3: Statistics Calculation (5-10 hours) - NICE TO HAVE**
- [ ] Task 3.1: Implement basic statistics (scan-based) (3-6h)
- [ ] Task 3.2: Optional: Sample-based path length (2-4h) [OPTIONAL]
- [ ] Task 3.3: Unit tests for statistics (1-2h)

### Testing Checklist

**Unit Tests**:
- [ ] test_hnsw_links.cpp (8 tests)
- [ ] test_hnsw_pruning.cpp (7 tests)
- [ ] test_hnsw_stats.cpp (7 tests)

**Total**: 22 new tests

### MGA Compliance Checklist

- [x] Current implementation uses TransactionId (uint64_t)
- [x] Nodes have xmin/xmax fields
- [x] is_node_visible() uses TIP lookups
- [ ] add_link() must not change node_tuple_id
- [ ] remove_link() must not change node_tuple_id
- [ ] prune_connections() must preserve xmin/xmax
- [ ] Page reorganization must preserve MGA fields
- [ ] No Snapshot structures used

---

## 5. Risk Mitigation

### 5.1 Technical Risks

**Risk 1: Page Reorganization Complexity**
- **Problem**: Variable-size nodes make reorganization complex
- **Mitigation**: Thorough testing, validate page consistency
- **Severity**: HIGH

**Risk 2: Graph Connectivity Degradation**
- **Problem**: Poor pruning breaks graph structure
- **Mitigation**: Use proven heuristic from HNSW paper, test search quality
- **Severity**: MEDIUM

**Risk 3: Performance Regression**
- **Problem**: Link operations may slow inserts
- **Mitigation**: Benchmark against baseline, optimize hot paths
- **Severity**: LOW

### 5.2 MGA Compliance Risks

**Risk**: Page reorganization violates visibility rules
- **Mitigation**: Always preserve xmin/xmax, never change TIDs
- **Prevention**: Re-read `/MGA_RULES.md` before Phase 1

### 5.3 Testing Risks

**Risk**: Edge cases in graph manipulation not covered
- **Mitigation**: Add property-based testing (invariants: graph connected, no orphans)
- **Prevention**: Code review after each phase

---

## 6. Total Effort Estimate

### 6.1 Effort Breakdown

| Phase | Tasks | Hours (Min-Max) | Hours (Realistic) |
|-------|-------|-----------------|-------------------|
| Phase 1: Link Management | 5 | 15-20 | 18 |
| Phase 2: Connection Pruning | 3 | 10-15 | 13 |
| Phase 3: Statistics | 3 | 5-10 | 8 |
| **TOTAL** | **11** | **30-45** | **39** |

**Buffer for debugging/edge cases**: +1 hour
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
1. Phase 1 (Link Management) → 15-20 hours (CRITICAL, blocks Phase 2)
2. Phase 2 (Connection Pruning) → 10-15 hours (depends on Phase 1)
3. Phase 3 (Statistics) → 5-10 hours (independent, can parallelize)

**Recommended Order**:
1. Phase 1 (Link Management) - CRITICAL for bi-directional graph
2. Phase 2 (Connection Pruning) - IMPORTANT for graph quality
3. Phase 3 (Statistics) - NICE TO HAVE for monitoring

---

## 7. Success Criteria

### 7.1 Functional Completion

**Must Have**:
- [ ] All 22 tests pass
- [ ] Bi-directional links maintained
- [ ] Node degree bounded by M
- [ ] Graph remains connected and navigable
- [ ] Statistics accurate

### 7.2 Performance Targets

**Must Achieve**:
- [ ] Insert: >1,000 vectors/sec (single thread)
- [ ] Search: < 10ms for 100K vectors (average)
- [ ] Recall@10: >95% (compared to brute-force)
- [ ] Link management overhead: < 10%
- [ ] Pruning time: < 50ms per node (M=16)

### 7.3 MGA Compliance

**Must Verify**:
- [ ] Deleted nodes visible to old transactions
- [ ] Deleted nodes invisible to new transactions
- [ ] Node TIDs never change (stable references)
- [ ] No Snapshot structures used
- [ ] All visibility checks via TransactionManager::isVersionVisible()

---

## 8. References

**Specification**: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/HNSW_INDEX_COMPLETION_SPEC.md`
**Implementation**: `/home/dcalford/CliWork/ScratchBird/src/core/hnsw_index.cpp` (1,147 lines)
**MGA Rules**: `/home/dcalford/CliWork/ScratchBird/MGA_RULES.md`
**Project Context**: `/home/dcalford/CliWork/ScratchBird/PROJECT_CONTEXT.md`

**Academic References**:
- Malkov & Yashunin (2018). "Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs"
- HNSW Paper: https://arxiv.org/abs/1603.09320

---

**Document Version**: 1.0
**Created**: 2025-11-04
**Status**: READY FOR IMPLEMENTATION
**Next Action**: Begin Phase 1 (Link Management) - CRITICAL for bi-directional graph

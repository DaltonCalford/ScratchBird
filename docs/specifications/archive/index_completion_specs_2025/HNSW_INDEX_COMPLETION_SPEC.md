# HNSW Index - Completion Specification

**Project**: ScratchBird Database Engine
**Component**: HNSW (Hierarchical Navigable Small World) Index - Complete Remaining Features
**Current Status**: 80% Complete (Core k-NN search functional, missing graph maintenance)
**Remaining Effort**: 30-40 hours
**Priority**: MEDIUM (Search works, missing features improve quality and maintenance)

---

## ⚠️ CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- All HNSW operations must respect xmin/xmax visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- NO PostgreSQL MVCC contamination
- Version traversal follows stable TIDs (nodes never move)

**If context is lost during compaction, re-read `/MGA_RULES.md` immediately.**

---

## Table of Contents

1. [Current Status](#1-current-status)
2. [Missing Feature 1: Link Management](#2-missing-feature-1-link-management)
3. [Missing Feature 2: Connection Pruning](#3-missing-feature-2-connection-pruning)
4. [Missing Feature 3: Statistics Calculation](#4-missing-feature-3-statistics-calculation)
5. [Testing Requirements](#5-testing-requirements)
6. [Implementation Breakdown](#6-implementation-breakdown)

---

## 1. Current Status

### What Works (80% Complete)

**File**: `src/core/hnsw_index.cpp` (1,147 lines)

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

### What's Missing (20% = 30-40 hours)

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

---

## 2. Missing Feature 1: Link Management

### 2.1 Problem Statement

**Current Code** (Lines 788-803):
```cpp
Status HnswIndex::add_link(uint64_t from_tid, uint64_t to_tid,
                           uint16_t layer, ErrorContext *ctx)
{
    // Simplified: link addition not fully implemented in Phase 1
    // Would require node update and potential page reorganization
    LOG_DEBUG(GENERAL, "HNSW: add_link %lu -> %lu (layer %u) - stub", from_tid, to_tid, layer);
    return Status::OK;
}

Status HnswIndex::remove_link(uint64_t from_tid, uint64_t to_tid,
                              uint16_t layer, ErrorContext *ctx)
{
    // Simplified: link removal not fully implemented in Phase 1
    LOG_DEBUG(GENERAL, "HNSW: remove_link %lu -> %lu (layer %u) - stub", from_tid, to_tid, layer);
    return Status::OK;
}
```

**Impact**:
- Insert operation creates initial forward links but never adds reverse links
- When node A is inserted with neighbor B, B never gets link back to A
- Graph becomes unidirectional (not navigable in both directions)
- Search quality degrades because neighbors cannot navigate back
- Node insertion at line 265-268 calls `add_link()` but it does nothing

### 2.2 Solution: Variable-Size Node Update with Page Reorganization

**Architecture**:
```
Initial State (Node B):
  node_tuple_id: 50
  node_num_neighbors: 2
  neighbors[]: [10, 20]
  vector_data: [0.1, 0.2, ...]

After add_link(B, A) where A=30:
  node_tuple_id: 50
  node_num_neighbors: 3  ← Incremented
  neighbors[]: [10, 20, 30]  ← Added A's TID
  vector_data: [0.1, 0.2, ...]  ← Unchanged

Challenge: Variable-size node (neighbors[] grows)
Solution: Page reorganization (copy-on-write)
```

**Why This Is Complex**:
- HNSW nodes are variable-size (header + neighbors + vector data)
- Adding a link increases node size by 8 bytes (uint64_t neighbor TID)
- Cannot simply append to neighbors array (would overwrite next node)
- Must reorganize entire page to create space
- Must maintain page consistency during update (atomic operation)

### 2.3 Implementation Details

**Step 1: Add Helper Method for Node Size Calculation** (1-2 hours)

```cpp
// Add to hnsw_index.h private section:
size_t calculate_node_size(const SBHnswNode* node) const;
size_t calculate_node_size(uint16_t num_neighbors, uint16_t vector_len) const;

// Implementation in hnsw_index.cpp:
size_t HnswIndex::calculate_node_size(const SBHnswNode* node) const
{
    return sizeof(SBHnswNode) +
           node->node_num_neighbors * sizeof(uint64_t) +
           node->node_vector_len;
}

size_t HnswIndex::calculate_node_size(uint16_t num_neighbors, uint16_t vector_len) const
{
    return sizeof(SBHnswNode) +
           num_neighbors * sizeof(uint64_t) +
           vector_len;
}
```

**Step 2: Add Page Reorganization Helper** (3-5 hours)

```cpp
// Add to hnsw_index.h private section:
Status reorganize_page_for_node_update(uint64_t page_num,
                                       uint64_t target_tid,
                                       uint16_t new_num_neighbors,
                                       const std::vector<uint64_t>& new_neighbors,
                                       ErrorContext* ctx);

// Implementation:
Status HnswIndex::reorganize_page_for_node_update(
    uint64_t page_num,
    uint64_t target_tid,
    uint16_t new_num_neighbors,
    const std::vector<uint64_t>& new_neighbors,
    ErrorContext* ctx)
{
    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    // Pin page
    void* page_buffer = nullptr;
    Status status = buffer_pool->pinPage(page_num, &page_buffer, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint8_t* page_data = static_cast<uint8_t*>(page_buffer);
    SBHnswPage* page = reinterpret_cast<SBHnswPage*>(page_data);

    // Create temporary buffer for reorganized nodes
    std::vector<uint8_t> temp_buffer;
    temp_buffer.reserve(8192 - sizeof(SBHnswPage));

    // Scan all nodes and collect them
    size_t offset = sizeof(SBHnswPage);
    bool target_found = false;
    uint16_t target_vector_len = 0;

    for (uint16_t i = 0; i < page->hnsw_count; ++i)
    {
        SBHnswNode* node = reinterpret_cast<SBHnswNode*>(page_data + offset);
        size_t node_size = calculate_node_size(node);

        if (node->node_tuple_id == target_tid)
        {
            // This is the node we're updating
            target_found = true;
            target_vector_len = node->node_vector_len;

            // Calculate new node size
            size_t new_node_size = calculate_node_size(new_num_neighbors, node->node_vector_len);

            // Check if page has space for larger node
            size_t size_delta = new_node_size - node_size;
            if (page->hnsw_free_space < size_delta)
            {
                buffer_pool->unpinPage(page_num, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL,
                    "HNSW page full, cannot add link (need page split)");
                return Status::PAGE_FULL;
            }

            // Create updated node in temp buffer
            size_t node_start = temp_buffer.size();
            temp_buffer.resize(node_start + new_node_size);

            SBHnswNode* new_node = reinterpret_cast<SBHnswNode*>(
                temp_buffer.data() + node_start);

            // Copy header fields
            new_node->node_tuple_id = node->node_tuple_id;
            new_node->node_flags = node->node_flags;
            new_node->node_layer = node->node_layer;
            new_node->node_num_neighbors = new_num_neighbors;
            new_node->node_vector_len = node->node_vector_len;
            new_node->node_xmin = node->node_xmin;
            new_node->node_xmax = node->node_xmax;

            // Copy new neighbors array
            uint64_t* neighbor_array = reinterpret_cast<uint64_t*>(new_node + 1);
            for (size_t j = 0; j < new_neighbors.size(); ++j)
            {
                neighbor_array[j] = new_neighbors[j];
            }

            // Copy vector data
            const uint8_t* old_vector_data = reinterpret_cast<const uint8_t*>(node + 1) +
                                            node->node_num_neighbors * sizeof(uint64_t);
            uint8_t* new_vector_data = reinterpret_cast<uint8_t*>(neighbor_array + new_num_neighbors);
            std::memcpy(new_vector_data, old_vector_data, node->node_vector_len);
        }
        else
        {
            // Copy unchanged node to temp buffer
            temp_buffer.insert(temp_buffer.end(),
                             page_data + offset,
                             page_data + offset + node_size);
        }

        offset += node_size;
    }

    if (!target_found)
    {
        buffer_pool->unpinPage(page_num, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Target node not found on page");
        return Status::NOT_FOUND;
    }

    // Clear node area and copy reorganized data back
    std::memset(page_data + sizeof(SBHnswPage), 0, 8192 - sizeof(SBHnswPage));
    std::memcpy(page_data + sizeof(SBHnswPage), temp_buffer.data(), temp_buffer.size());

    // Update page metadata
    page->hnsw_free_space = 8192 - sizeof(SBHnswPage) - temp_buffer.size();

    // Mark page dirty
    buffer_pool->unpinPage(page_num, true, ctx);

    LOG_DEBUG(GENERAL, "HNSW: Reorganized page %lu for node %lu (new neighbors: %u)",
             page_num, target_tid, new_num_neighbors);

    return Status::OK;
}
```

**Step 3: Implement add_link()** (5-7 hours)

```cpp
Status HnswIndex::add_link(uint64_t from_tid, uint64_t to_tid,
                           uint16_t layer, ErrorContext* ctx)
{
    // 1. Find the source node
    SBHnswNode* from_node = nullptr;
    uint64_t page_num = 0;
    Status status = find_node(from_tid, &from_node, &page_num, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // 2. Check if link already exists
    const uint64_t* neighbors = reinterpret_cast<const uint64_t*>(from_node + 1);
    bool already_linked = false;
    for (uint16_t i = 0; i < from_node->node_num_neighbors; ++i)
    {
        if (neighbors[i] == to_tid)
        {
            already_linked = true;
            break;
        }
    }

    // Unpin page (will repin during reorganization)
    db_->buffer_pool()->unpinPage(page_num, false, ctx);

    if (already_linked)
    {
        // Link already exists, no-op
        LOG_DEBUG(GENERAL, "HNSW: Link %lu -> %lu already exists", from_tid, to_tid);
        return Status::OK;
    }

    // 3. Check if node has reached max connections (M)
    if (from_node->node_num_neighbors >= index_info_.idx_m)
    {
        // Need to prune first
        LOG_DEBUG(GENERAL, "HNSW: Node %lu at max connections (%u), pruning before add_link",
                 from_tid, index_info_.idx_m);

        status = prune_connections(from_tid, layer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Refetch node after pruning
        status = find_node(from_tid, &from_node, &page_num, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        db_->buffer_pool()->unpinPage(page_num, false, ctx);
    }

    // 4. Build new neighbor list with added link
    std::vector<uint64_t> new_neighbors;
    new_neighbors.reserve(from_node->node_num_neighbors + 1);

    for (uint16_t i = 0; i < from_node->node_num_neighbors; ++i)
    {
        new_neighbors.push_back(neighbors[i]);
    }
    new_neighbors.push_back(to_tid);

    // 5. Reorganize page to update node with new neighbor
    status = reorganize_page_for_node_update(page_num, from_tid,
                                            new_neighbors.size(),
                                            new_neighbors, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    LOG_DEBUG(GENERAL, "HNSW: Added link %lu -> %lu (layer %u)", from_tid, to_tid, layer);
    return Status::OK;
}
```

**Step 4: Implement remove_link()** (5-7 hours)

```cpp
Status HnswIndex::remove_link(uint64_t from_tid, uint64_t to_tid,
                              uint16_t layer, ErrorContext* ctx)
{
    // 1. Find the source node
    SBHnswNode* from_node = nullptr;
    uint64_t page_num = 0;
    Status status = find_node(from_tid, &from_node, &page_num, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // 2. Check if link exists
    const uint64_t* neighbors = reinterpret_cast<const uint64_t*>(from_node + 1);
    bool link_found = false;
    for (uint16_t i = 0; i < from_node->node_num_neighbors; ++i)
    {
        if (neighbors[i] == to_tid)
        {
            link_found = true;
            break;
        }
    }

    // Unpin page (will repin during reorganization)
    db_->buffer_pool()->unpinPage(page_num, false, ctx);

    if (!link_found)
    {
        // Link doesn't exist, no-op (idempotent)
        LOG_DEBUG(GENERAL, "HNSW: Link %lu -> %lu does not exist", from_tid, to_tid);
        return Status::OK;
    }

    // 3. Build new neighbor list without removed link
    std::vector<uint64_t> new_neighbors;
    new_neighbors.reserve(from_node->node_num_neighbors - 1);

    for (uint16_t i = 0; i < from_node->node_num_neighbors; ++i)
    {
        if (neighbors[i] != to_tid)
        {
            new_neighbors.push_back(neighbors[i]);
        }
    }

    // 4. Reorganize page to update node with removed neighbor
    status = reorganize_page_for_node_update(page_num, from_tid,
                                            new_neighbors.size(),
                                            new_neighbors, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    LOG_DEBUG(GENERAL, "HNSW: Removed link %lu -> %lu (layer %u)", from_tid, to_tid, layer);
    return Status::OK;
}
```

### 2.4 Testing Requirements

**Unit Tests** (`tests/unit/test_hnsw_links.cpp`):
1. [ ] Insert 10 nodes, verify all bi-directional links are created
2. [ ] Add link to node, verify neighbor count increases
3. [ ] Remove link from node, verify neighbor count decreases
4. [ ] Add link that already exists (idempotent)
5. [ ] Remove link that doesn't exist (idempotent)
6. [ ] Add link when node is at max connections (triggers pruning)
7. [ ] Add link that causes page overflow (returns PAGE_FULL)
8. [ ] Verify graph connectivity after 100 inserts with random links

**Acceptance Criteria**:
- All 8 tests pass
- Bi-directional links maintained during insert
- Page reorganization preserves all other nodes
- Node size calculations correct
- Graph remains connected and navigable

---

## 3. Missing Feature 2: Connection Pruning

### 3.1 Problem Statement

**Current Code** (Lines 851-857):
```cpp
Status HnswIndex::prune_connections(uint64_t node_tid, uint16_t layer,
                                   ErrorContext* ctx)
{
    // Simplified: connection pruning not fully implemented in Phase 1
    LOG_DEBUG(GENERAL, "HNSW: prune_connections for node %lu (layer %u) - stub", node_tid, layer);
    return Status::OK;
}
```

**Impact**:
- When node has M connections and a new link is added, no pruning occurs
- Node degree grows unbounded (violates HNSW invariant)
- Search quality degrades (too many neighbors to explore)
- Memory usage increases (more edges stored)
- Graph structure becomes unbalanced

### 3.2 Solution: Heuristic Pruning Algorithm

**Architecture**:
```
Pruning Scenario:
  Node A has M=16 connections (max)
  Want to add new link to Node B

Pruning Algorithm (based on HNSW paper):
  1. Create candidate set: existing neighbors + new neighbor
  2. For each candidate, compute:
     - Distance from node to candidate
     - Diversity penalty (how similar candidate is to already selected)
  3. Select M candidates with best score
  4. Discard the rest

Goal: Keep diverse, close neighbors (not redundant ones)
```

**Heuristic Pruning (from HNSW paper Section 4)**:
```
Algorithm: Select M neighbors from candidate set C

1. Sort candidates by distance from query node (closest first)
2. Initialize selected set R = empty
3. For each candidate c in C (in sorted order):
   a. If |R| >= M, stop
   b. Compute "redundancy" of c relative to R:
      redundancy(c) = min { dist(c, r) : r in R }
   c. If dist(node, c) < redundancy(c):
      - c is closer to node than to any selected neighbor
      - Add c to R (diverse neighbor)
   d. Else:
      - c is redundant (close to existing neighbor)
      - Skip c
4. Return R
```

### 3.3 Implementation Details

**Step 1: Add Distance Computation Helper** (1-2 hours)

```cpp
// Implementation already exists at line 660-665:
double HnswIndex::compute_distance(const VectorValue& a, const VectorValue& b) const
{
    DistanceMetric metric = static_cast<DistanceMetric>(index_info_.idx_distance_metric);
    auto result = a.distance(b, metric);
    return result.value_or(0.0);
}

// Add helper to get node's vector:
Status get_node_vector(uint64_t tuple_id, VectorValue* vector_out, ErrorContext* ctx);
```

**Step 2: Implement get_node_vector() Helper** (2-3 hours)

```cpp
// Add to hnsw_index.h private section:
Status get_node_vector(uint64_t tuple_id, VectorValue* vector_out, ErrorContext* ctx);

// Implementation:
Status HnswIndex::get_node_vector(uint64_t tuple_id,
                                  VectorValue* vector_out,
                                  ErrorContext* ctx)
{
    SBHnswNode* node = nullptr;
    uint64_t page_num = 0;
    Status status = find_node(tuple_id, &node, &page_num, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Extract vector data
    const uint8_t* node_data = reinterpret_cast<const uint8_t*>(node);
    const uint8_t* vector_data = node_data + sizeof(SBHnswNode) +
                                 node->node_num_neighbors * sizeof(uint64_t);

    // Deserialize vector
    uint32_t dimensions = node->node_vector_len / sizeof(float);
    const float* float_data = reinterpret_cast<const float*>(vector_data);

    std::vector<float> vec(float_data, float_data + dimensions);
    *vector_out = VectorValue::fromFloat32(vec);

    db_->buffer_pool()->unpinPage(page_num, false, ctx);
    return Status::OK;
}
```

**Step 3: Implement prune_connections()** (6-10 hours)

```cpp
Status HnswIndex::prune_connections(uint64_t node_tid, uint16_t layer,
                                   ErrorContext* ctx)
{
    // 1. Find the node
    SBHnswNode* node = nullptr;
    uint64_t page_num = 0;
    Status status = find_node(node_tid, &node, &page_num, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint16_t current_neighbors = node->node_num_neighbors;
    uint32_t M = index_info_.idx_m;

    if (current_neighbors <= M)
    {
        // No pruning needed
        db_->buffer_pool()->unpinPage(page_num, false, ctx);
        LOG_DEBUG(GENERAL, "HNSW: Node %lu has %u connections (<= M=%u), no pruning needed",
                 node_tid, current_neighbors, M);
        return Status::OK;
    }

    // 2. Get node's vector
    VectorValue node_vector;
    status = get_node_vector(node_tid, &node_vector, ctx);
    if (status != Status::OK)
    {
        db_->buffer_pool()->unpinPage(page_num, false, ctx);
        return status;
    }

    // 3. Build candidate list with distances
    struct Candidate
    {
        uint64_t tid;
        double distance;
        VectorValue vector;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(current_neighbors);

    const uint64_t* neighbors = reinterpret_cast<const uint64_t*>(node + 1);

    for (uint16_t i = 0; i < current_neighbors; ++i)
    {
        uint64_t neighbor_tid = neighbors[i];

        VectorValue neighbor_vector;
        status = get_node_vector(neighbor_tid, &neighbor_vector, ctx);
        if (status != Status::OK)
        {
            continue; // Skip this neighbor
        }

        double dist = compute_distance(node_vector, neighbor_vector);
        candidates.push_back({neighbor_tid, dist, neighbor_vector});
    }

    db_->buffer_pool()->unpinPage(page_num, false, ctx);

    if (candidates.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "No valid candidates for pruning");
        return Status::INTERNAL_ERROR;
    }

    // 4. Sort candidates by distance (closest first)
    std::sort(candidates.begin(), candidates.end(),
             [](const Candidate& a, const Candidate& b) {
                 return a.distance < b.distance;
             });

    // 5. Heuristic pruning: Select M diverse neighbors
    std::vector<uint64_t> selected;
    selected.reserve(M);

    for (const Candidate& candidate : candidates)
    {
        if (selected.size() >= M)
        {
            break;
        }

        // Check redundancy: Is candidate too close to already selected neighbors?
        double min_redundancy = std::numeric_limits<double>::max();

        for (uint64_t selected_tid : selected)
        {
            // Find selected neighbor's vector in candidates
            auto it = std::find_if(candidates.begin(), candidates.end(),
                                  [selected_tid](const Candidate& c) {
                                      return c.tid == selected_tid;
                                  });

            if (it != candidates.end())
            {
                double redundancy = compute_distance(candidate.vector, it->vector);
                min_redundancy = std::min(min_redundancy, redundancy);
            }
        }

        // Add candidate if it's closer to node than to any selected neighbor
        // (diversity heuristic)
        if (selected.empty() || candidate.distance < min_redundancy)
        {
            selected.push_back(candidate.tid);
        }
    }

    if (selected.size() < M)
    {
        // Fill remaining slots with closest neighbors (fallback)
        for (const Candidate& candidate : candidates)
        {
            if (selected.size() >= M)
            {
                break;
            }

            bool already_selected = std::find(selected.begin(), selected.end(),
                                             candidate.tid) != selected.end();
            if (!already_selected)
            {
                selected.push_back(candidate.tid);
            }
        }
    }

    // 6. Update node with pruned neighbor list
    status = reorganize_page_for_node_update(page_num, node_tid,
                                            selected.size(),
                                            selected, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    LOG_INFO(GENERAL, "HNSW: Pruned node %lu connections from %u to %zu (M=%u)",
             node_tid, current_neighbors, selected.size(), M);

    return Status::OK;
}
```

### 3.4 Testing Requirements

**Unit Tests** (`tests/unit/test_hnsw_pruning.cpp`):
1. [ ] Insert 100 nodes with M=16, verify no node exceeds M connections
2. [ ] Force pruning by adding 20 links to single node
3. [ ] Verify pruned neighbors are most diverse (not all from same cluster)
4. [ ] Verify search quality after pruning (recall >= 95%)
5. [ ] Prune node with M=16 neighbors to M=16 (no-op)
6. [ ] Concurrent inserts with pruning (stress test)
7. [ ] Verify graph connectivity preserved after aggressive pruning

**Acceptance Criteria**:
- All 7 tests pass
- Pruning maintains M connections per node
- Selected neighbors are diverse (heuristic working)
- Search quality not significantly degraded after pruning
- Graph remains connected

---

## 4. Missing Feature 3: Statistics Calculation

### 4.1 Problem Statement

**Current Code** (Lines 479-495):
```cpp
Status HnswIndex::getStats(HnswStats* stats_out, ErrorContext* ctx)
{
    if (!stats_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid stats output");
        return Status::INVALID_ARGUMENT;
    }

    stats_out->total_nodes = index_info_.idx_total_nodes;
    stats_out->deleted_nodes = 0; // TODO: Count from page
    stats_out->total_pages = 1; // Simplified: single page in Phase 1
    stats_out->max_layer = get_max_layer();
    stats_out->avg_connections = 0.0; // TODO: Calculate
    stats_out->avg_path_length = 0.0; // TODO: Calculate

    return Status::OK;
}
```

**Impact**:
- Cannot monitor index health (deleted_nodes unknown)
- Cannot assess graph quality (avg_connections unknown)
- Cannot optimize search parameters (avg_path_length unknown)
- No metrics for performance analysis

### 4.2 Solution: Graph Traversal Statistics

**Architecture**:
```
Statistics to Calculate:
1. deleted_nodes: Count nodes with xmax != 0
2. avg_connections: Mean of node_num_neighbors across all nodes
3. avg_path_length: Estimated by sampling k-NN queries

Implementation:
- Scan all pages and nodes (deleted_nodes, avg_connections)
- Sample 100 random k-NN queries (avg_path_length)
- Cache results (expensive operation)
```

### 4.3 Implementation Details

**Step 1: Implement Statistics Calculation** (3-6 hours)

```cpp
Status HnswIndex::getStats(HnswStats* stats_out, ErrorContext* ctx)
{
    if (!stats_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid stats output");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool* buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    // 1. Scan root page for basic statistics
    void* page_buffer = nullptr;
    Status status = buffer_pool->pinPage(index_info_.idx_root_page, &page_buffer, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint8_t* page_data = static_cast<uint8_t*>(page_buffer);
    SBHnswPage* page = reinterpret_cast<SBHnswPage*>(page_data);

    uint64_t total_nodes = 0;
    uint64_t deleted_nodes = 0;
    uint64_t total_connections = 0;
    uint16_t max_layer = 0;

    // Scan all nodes
    size_t offset = sizeof(SBHnswPage);
    for (uint16_t i = 0; i < page->hnsw_count; ++i)
    {
        SBHnswNode* node = reinterpret_cast<SBHnswNode*>(page_data + offset);

        total_nodes++;

        if (node->node_xmax != 0)
        {
            deleted_nodes++;
        }

        total_connections += node->node_num_neighbors;

        if (node->node_layer > max_layer)
        {
            max_layer = node->node_layer;
        }

        size_t node_size = calculate_node_size(node);
        offset += node_size;
    }

    buffer_pool->unpinPage(index_info_.idx_root_page, false, ctx);

    // 2. Calculate average connections
    double avg_connections = (total_nodes > 0) ?
        static_cast<double>(total_connections) / static_cast<double>(total_nodes) : 0.0;

    // 3. Estimate average path length (sample-based)
    // For simplicity, use formula: avg_path_length ≈ log(N) / log(M)
    // where N = total nodes, M = avg connections
    double avg_path_length = 0.0;
    if (total_nodes > 1 && avg_connections > 1.0)
    {
        avg_path_length = std::log(static_cast<double>(total_nodes)) /
                         std::log(avg_connections);
    }

    // 4. Fill output
    stats_out->total_nodes = total_nodes;
    stats_out->deleted_nodes = deleted_nodes;
    stats_out->total_pages = 1; // Phase 1: single page
    stats_out->max_layer = max_layer;
    stats_out->avg_connections = avg_connections;
    stats_out->avg_path_length = avg_path_length;

    LOG_INFO(GENERAL, "HNSW stats: %lu nodes (%lu deleted), max_layer=%u, "
             "avg_connections=%.2f, avg_path_length=%.2f",
             total_nodes, deleted_nodes, max_layer, avg_connections, avg_path_length);

    return Status::OK;
}
```

**Step 2: Add Sample-Based Path Length (Optional Enhancement)** (2-4 hours)

```cpp
// Add private helper for accurate path length measurement:
double estimate_avg_path_length_sampling(uint32_t num_samples, ErrorContext* ctx);

double HnswIndex::estimate_avg_path_length_sampling(uint32_t num_samples,
                                                     ErrorContext* ctx)
{
    // Sample random queries and measure search path length
    std::vector<uint64_t> path_lengths;
    path_lengths.reserve(num_samples);

    TransactionManager* txn_mgr = db_->transaction_manager();
    if (!txn_mgr)
    {
        return 0.0;
    }

    uint64_t current_xid = txn_mgr->getCurrentXid();

    for (uint32_t i = 0; i < num_samples; ++i)
    {
        // Generate random query vector
        std::vector<float> random_vec(index_info_.idx_dimensions);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (uint32_t j = 0; j < index_info_.idx_dimensions; ++j)
        {
            random_vec[j] = dist(rng_);
        }
        VectorValue query = VectorValue::fromFloat32(random_vec);

        // Perform search (tracks path length internally)
        std::vector<HnswSearchResult> results;
        Status status = search(query, 10, current_xid, &results, ctx);
        if (status == Status::OK && !results.empty())
        {
            // Path length = max_layer traversed
            // For simplicity, use max_layer as proxy
            uint16_t path_len = get_max_layer() + 1;
            path_lengths.push_back(path_len);
        }
    }

    if (path_lengths.empty())
    {
        return 0.0;
    }

    // Calculate mean
    double sum = 0.0;
    for (uint64_t len : path_lengths)
    {
        sum += len;
    }
    return sum / path_lengths.size();
}
```

### 4.4 Testing Requirements

**Unit Tests** (`tests/unit/test_hnsw_stats.cpp`):
1. [ ] Insert 100 nodes, verify total_nodes = 100
2. [ ] Delete 20 nodes, verify deleted_nodes = 20
3. [ ] Verify avg_connections ≈ M (within 20%)
4. [ ] Verify max_layer increases with index size
5. [ ] Verify avg_path_length ≈ log(N) (within 50%)
6. [ ] Empty index returns all stats as 0
7. [ ] Statistics consistent across multiple calls

**Acceptance Criteria**:
- All 7 tests pass
- deleted_nodes count accurate
- avg_connections matches expected value
- avg_path_length reasonable (log scale)
- Performance acceptable (<100ms for 10K nodes)

---

## 5. Testing Requirements

### 5.1 Unit Tests

**New Test Files**:
1. `tests/unit/test_hnsw_links.cpp` (8 tests)
2. `tests/unit/test_hnsw_pruning.cpp` (7 tests)
3. `tests/unit/test_hnsw_stats.cpp` (7 tests)

**Total**: 22 new tests

### 5.2 Integration Tests

**Existing Integration Tests** (`tests/integration/test_hnsw_index.cpp`):
- [ ] Verify all existing tests still pass after changes
- [ ] Add stress test: 10,000 inserts with bi-directional links
- [ ] Add concurrency test: 4 threads inserting, 2 threads searching
- [ ] Add MGA test: delete node, verify old transaction still sees it
- [ ] Add recall test: Search quality >= 95% after pruning

### 5.3 Performance Benchmarks

**Benchmarks** (`tests/benchmark/benchmark_hnsw_index.cpp`):
- [ ] Measure insert throughput (vectors/sec) before and after completion
- [ ] Measure search latency (ms) for 100K vectors
- [ ] Measure recall@10 with varying M and ef_search parameters
- [ ] Compare HNSW vs brute-force k-NN (accuracy validation)
- [ ] Measure link management overhead (add_link/remove_link time)

---

## 6. Implementation Breakdown

### 6.1 Task Breakdown

| Task | Effort (hours) | Dependency |
|------|----------------|------------|
| **Feature 1: Link Management** | **15-20** | - |
| 1.1 Add node size calculation helpers | 1-2 | - |
| 1.2 Implement page reorganization helper | 3-5 | 1.1 |
| 1.3 Implement add_link() | 5-7 | 1.2 |
| 1.4 Implement remove_link() | 5-7 | 1.2 |
| 1.5 Unit tests for link management | 2-3 | 1.3, 1.4 |
| **Feature 2: Connection Pruning** | **10-15** | Feature 1 |
| 2.1 Implement get_node_vector() helper | 2-3 | - |
| 2.2 Implement prune_connections() | 6-10 | 2.1 |
| 2.3 Unit tests for pruning | 2-3 | 2.2 |
| **Feature 3: Statistics** | **5-10** | - |
| 3.1 Implement basic statistics (scan-based) | 3-6 | - |
| 3.2 Optional: Sample-based path length | 2-4 | 3.1 |
| 3.3 Unit tests for statistics | 2-3 | 3.1, 3.2 |
| **Integration & Performance** | **3-5** | All |
| 4.1 Integration test updates | 2-3 | All |
| 4.2 Performance benchmarks | 1-2 | All |
| **TOTAL** | **33-50** | - |

### 6.2 Estimated Total Effort

**Realistic Estimate**: 30-40 hours (includes buffer time for debugging)

**Timeline**:
- Single developer (full-time): 4-5 days
- Part-time: 1-2 weeks

### 6.3 Critical Path

**Critical Path** (longest dependency chain):
1. Feature 1 (Link Management) must be completed first (enables bi-directional graph)
2. Feature 2 (Pruning) depends on Feature 1 (needs add_link/remove_link)
3. Feature 3 (Statistics) is independent but low priority
4. Integration tests depend on all features

**Recommended Order**:
1. Feature 1 (Link Management) - Unblocks graph construction
2. Feature 2 (Connection Pruning) - Maintains graph quality
3. Feature 3 (Statistics) - Monitoring and diagnostics
4. Integration tests and benchmarks

---

## 7. MGA Compliance Checklist

**All HNSW operations must respect MGA rules:**

- [x] Current implementation uses `TransactionId` (uint64_t), not `Snapshot`
- [x] Visibility checks use `is_node_visible()` with TIP lookups
- [x] Search operations pass `current_xid`, not `snapshot`
- [x] remove() sets node_xmax correctly (already implemented)
- [x] Index nodes store stable TIDs (never change)
- [x] MGA version chains respected (nodes have xmin/xmax)

**New Code Requirements**:
- [ ] add_link() must not change node_tuple_id (stable TID)
- [ ] remove_link() must not change node_tuple_id (stable TID)
- [ ] prune_connections() must preserve xmin/xmax
- [ ] Page reorganization must preserve MGA fields
- [ ] No use of `Snapshot` structures anywhere
- [ ] All visibility checks via `TransactionManager` TIP lookups

**Reference**: See `/MGA_RULES.md` Section 4 (Visibility Rules)

---

## 8. Dependencies and Assumptions

### 8.1 External Dependencies

**Requires**:
- `TransactionManager::isVersionVisible()` - For MGA visibility checks
- `TransactionManager::getCurrentXid()` - For xmin/xmax assignment
- `BufferPool::pinPage()` / `unpinPage()` - For page access
- `BufferPool::markPageDirty()` - For write-ahead logging
- `VectorValue` class with distance methods - For distance computation

**Assumes**:
- Buffer pool is thread-safe
- Transaction manager TIP is correctly maintained
- Vector distance functions are correct
- Single-page HNSW index (Phase 1 limitation)

### 8.2 Phase 1 Limitations

**Current Limitations**:
- Single-page HNSW index (multi-page support in Phase 2)
- No concurrent modifications (exclusive lock during insert/delete)
- No dynamic M adjustment (fixed at index creation)
- No vector compression (future optimization)

**These are OK for Phase 1, can be added later.**

---

## 9. Known Limitations and Future Work

### 9.1 Current Limitations (After Completion)

**Not Included in This Spec**:
- Multi-page HNSW index (future Phase 2)
- Dynamic layer adjustment (future optimization)
- Vector compression (future optimization)
- Incremental graph refinement (future optimization)
- Concurrent modifications (currently uses exclusive lock)

**These are OK for Phase 1, can be added later.**

### 9.2 Future Enhancements

**Phase 2 Enhancements** (not required now):
1. **Multi-Page Index** (30-40 hours)
   - Split index across multiple pages
   - Layer-specific page allocation
   - Cross-page neighbor links

2. **Dynamic M Adjustment** (10-15 hours)
   - Adjust M based on index size
   - Automatic graph rebalancing

3. **Vector Compression** (15-20 hours)
   - Product quantization for large vectors
   - Reduces memory footprint 4-8x

4. **Concurrent Modifications** (20-30 hours)
   - Lock-free search
   - Optimistic concurrency control for inserts

---

## 10. Acceptance Criteria

### 10.1 Functional Requirements

**Must Pass**:
- [ ] All 22 unit tests pass
- [ ] All integration tests pass
- [ ] Can insert 1,000 vectors with bi-directional links
- [ ] Can delete 500 vectors (50%)
- [ ] Pruning maintains M connections per node
- [ ] Search returns correct k nearest neighbors
- [ ] Statistics reflect actual index state
- [ ] Graph remains connected after insertions/deletions

### 10.2 Performance Requirements

**Must Achieve**:
- [ ] Insert throughput: >1,000 vectors/sec (single thread)
- [ ] Search latency: <10ms for 100K vectors (avg)
- [ ] Recall@10: >95% (compared to brute-force)
- [ ] Memory overhead: <10% for link management
- [ ] Pruning time: <50ms per node (M=16)

### 10.3 MGA Compliance Requirements

**Must Verify**:
- [ ] Deleted nodes visible to old transactions (snapshot isolation)
- [ ] Deleted nodes invisible to new transactions
- [ ] Node TIDs never change (stable references)
- [ ] No `Snapshot` structures used anywhere
- [ ] All visibility checks via `TransactionManager::isVersionVisible()`

---

## 11. Conclusion

This specification provides complete implementation details for the 3 missing features in the HNSW index.

**Key Takeaways**:
- **Link Management** is most critical (15-20 hours) - enables bi-directional graph
- **Connection Pruning** is most complex (10-15 hours) - maintains graph quality
- **Statistics** is most impactful (5-10 hours) - enables monitoring

**Completion Criteria**:
- All 22 unit tests pass
- All integration tests pass
- MGA visibility rules respected throughout
- Performance meets requirements

**Next Steps**:
1. Implement Feature 1 (Link Management) first - unblocks graph construction
2. Implement Feature 2 (Connection Pruning) second - maintains quality
3. Implement Feature 3 (Statistics) third - completes the feature set
4. Integration testing and performance benchmarking

**Status**: SPECIFICATION COMPLETE
**Implementation**: PENDING (30-40 hours)

---

**Specification Created**: November 4, 2025
**Based On**: Index Implementation Audit (2025-11-04)
**Follows Format**: GiST Index Completion Specification
**MGA Compliance**: Full adherence to `/MGA_RULES.md`

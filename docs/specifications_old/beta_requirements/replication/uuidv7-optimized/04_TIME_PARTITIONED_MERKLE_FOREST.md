# Time-Partitioned Merkle Forest for Anti-Entropy

**Document:** 04_TIME_PARTITIONED_MERKLE_FOREST.md
**Status:** BETA SPECIFICATION
**Version:** 1.0
**Date:** 2026-01-09
**Authority:** Chief Architect

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Anti-Entropy Problem](#anti-entropy-problem)
3. [Traditional Merkle Trees](#traditional-merkle-trees)
4. [Time-Partitioned Innovation](#time-partitioned-innovation)
5. [Tree Structure](#tree-structure)
6. [Verification Algorithm](#verification-algorithm)
7. [Read Repair](#read-repair)
8. [LSM Compaction Integration](#lsm-compaction-integration)
9. [Implementation Details](#implementation-details)
10. [Testing and Verification](#testing-and-verification)

---

## Executive Summary

### Purpose

This specification defines **Time-Partitioned Merkle Forests**, an anti-entropy verification system optimized for time-ordered UUIDs (UUIDv7/UUIDv8-HLC). By partitioning Merkle trees by time windows (e.g., hourly), ScratchBird achieves sub-2% CPU overhead for divergence detection, compared to 10-20% for traditional hash-based Merkle trees used in systems like Cassandra.

**Key Innovation**: Leverage the temporal locality of UUIDv7/UUIDv8 to create **sealed, immutable** Merkle trees for past time windows, enabling O(1) root hash comparisons (no re-computation). Only the active tree (current hour) requires dynamic updates.

### Design Principles

1. **Exploit Time-Ordering**: UUIDv7/UUIDv8 time component enables time-based partitioning
2. **Immutable Sealed Trees**: Past trees never change → Root hash computed once, cached forever
3. **Minimize Re-Computation**: Sealed trees eliminate perpetual Merkle tree rebuilding
4. **Align with LSM Compaction**: Tree sealing triggers align with LSM compaction cycles
5. **Deterministic Divergence Detection**: Detect which time window diverged, not just "replicas differ"

### Performance Targets (Beta)

- **Anti-Entropy Overhead**: < 2% CPU (vs 10-20% for hash-based Merkle trees)
- **Verification Frequency**: Every 60 seconds (background gossip)
- **Time to Detect Divergence**: < 10 seconds
- **Repair Bandwidth**: Proportional to differences (not dataset size)
- **Scalability**: 1M+ rows per shard with < 100MB tree metadata

---

## Anti-Entropy Problem

### Replica Divergence

**Cause**: In leaderless quorum replication (Mode 3), replicas may diverge due to:
1. **Hinted Handoff Failures**: Coordinator crashes before delivering hints
2. **Network Partitions**: Replica temporarily unreachable, misses writes
3. **Disk Corruption**: Silent data corruption (bit flips, hardware failure)
4. **Software Bugs**: Replication logic errors

**Example Divergence**:

```
Shard 001 (N=3 replicas):

Node A:
  Row 1: user_id='user_123', name='Alice', version_id=(ts=1000, counter=5)
  Row 2: user_id='user_456', name='Bob', version_id=(ts=1001, counter=0)

Node B:
  Row 1: user_id='user_123', name='Alice', version_id=(ts=1000, counter=5)
  Row 2: user_id='user_456', name='Bob', version_id=(ts=1001, counter=0)

Node C (diverged):
  Row 1: user_id='user_123', name='Alice', version_id=(ts=1000, counter=5)
  Row 2: MISSING (network partition during write)

Divergence: Node C missing Row 2
```

### Detection Challenge

**Naive Approach**: Compare all rows across replicas
- **Problem**: O(N) comparisons per replica (N = total rows)
- **Example**: 1M rows → 1M hash computations per comparison (expensive)

**Merkle Tree Approach**: Hierarchical hash tree
- **Benefit**: O(log N) comparisons to find divergent rows
- **Example**: 1M rows, tree depth 20 → 20 hash comparisons (vs 1M)

**Problem with Traditional Merkle Trees**: Continuous re-computation
- **Issue**: Every write changes leaf → Propagate hash updates to root
- **Cost**: 10-20% CPU overhead for Merkle tree maintenance (Cassandra experience)

### ScratchBird's Solution: Time-Partitioned Merkle Forests

**Key Insight**: Most writes have recent timestamps (UUIDv7/UUIDv8 time-ordered).

**Optimization**:
1. Partition Merkle trees by time windows (e.g., hourly)
2. **Seal** trees for past hours (immutable, root hash cached)
3. Only **active tree** (current hour) requires dynamic updates

**Result**:
- Sealed trees: 0% CPU (no re-computation)
- Active tree: ~2% CPU (only current hour's writes)
- Total overhead: < 2% (vs 10-20% for hash-based)

---

## Traditional Merkle Trees

### Structure

**Merkle Tree**: Binary tree where each leaf is a row hash, each internal node is hash of children.

```
                  Root Hash
                 /          \
            Hash_AB        Hash_CD
            /    \         /    \
        Hash_A Hash_B  Hash_C Hash_D
          |      |       |      |
        Row_A  Row_B   Row_C  Row_D
```

**Leaf Node**: `Hash(row_id || column_values || version_id)`

**Internal Node**: `Hash(left_child_hash || right_child_hash)`

**Root Hash**: Single hash representing entire dataset.

### Verification Algorithm

**Step 1**: Compare root hashes across replicas

```
Node A: root_hash = 0xABCDEF
Node B: root_hash = 0xABCDEF  ← Match!
Node C: root_hash = 0x123456  ← Divergence detected
```

**Step 2**: Drill down to find divergent subtree

```
Node A vs Node C:
  Root mismatch → Compare children
    Left child: Hash_AB (A)=0x111, Hash_AB (C)=0x111  ← Match
    Right child: Hash_CD (A)=0x222, Hash_CD (C)=0x333  ← Mismatch

Continue drill-down in right subtree:
  Hash_C (A)=0xAAA, Hash_C (C)=0xAAA  ← Match
  Hash_D (A)=0xBBB, Hash_D (C)=0x000  ← Mismatch (Row_D diverged)
```

**Step 3**: Repair divergent row

```
Node A → Node C: Send Row_D data
Node C: Apply Row_D (conflict resolution via UUIDv8-HLC if needed)
```

### Traditional Merkle Tree Problems

**Problem 1: Continuous Re-Computation**

```
Write to Row_A:
  1. Update Row_A leaf hash
  2. Propagate to parent: Update Hash_A
  3. Propagate to grandparent: Update Hash_AB
  4. Propagate to root: Update Root Hash

Every write → O(log N) hash updates
High write throughput → 10-20% CPU on Merkle tree maintenance
```

**Problem 2: Stochastic Verification**

```
Cassandra approach:
  - Periodically rebuild entire Merkle tree (e.g., every hour)
  - Compare with replicas
  - Problem: Rebuild cost is O(N) (read all rows)

ScratchBird's solution:
  - Time-partitioned trees (sealed past windows)
  - No rebuild needed for sealed trees
```

---

## Time-Partitioned Innovation

### Time Windows

**Concept**: Partition data by time windows based on UUIDv7/UUIDv8 timestamp.

**Window Size**: 1 hour (configurable: `merkle_tree_window_ms = 3600000`)

**Example Timeline**:

```
Timeline (milliseconds since epoch):
  Window 0: [0, 3600000)           → Tree_0 (sealed)
  Window 1: [3600000, 7200000)     → Tree_1 (sealed)
  Window 2: [7200000, 10800000)    → Tree_2 (sealed)
  ...
  Window N: [current_hour, ...)    → Tree_N (active)
```

**Row Assignment**: Each row assigned to window based on version_id timestamp.

```
Row 1: version_id=(ts=5000000, ...)  → Window 1
Row 2: version_id=(ts=8000000, ...)  → Window 2
Row 3: version_id=(ts=11000000, ...) → Window N (active)
```

### Sealed Trees

**Sealed Tree**: Merkle tree for a past time window (no new writes expected).

**Properties**:
1. **Immutable**: Once sealed, tree never changes
2. **Root Hash Cached**: Computed once at sealing time, cached in memory
3. **No Re-Computation**: Future writes go to active tree, not sealed trees

**Sealing Trigger**:

```
Current time: 2026-01-09 15:30:00 (Window N)

Window N-1 (14:00-15:00):
  - Current time > end of window N-1 + grace period (e.g., 10 minutes)
  - Seal Window N-1 tree (no more writes expected)
  - Compute root hash, store in metadata

Grace period: Allow late-arriving writes (clock skew, network delay)
```

**Sealing Pseudocode**:

```cpp
auto MerkleForest::sealTree(WindowId window_id) -> void {
    auto tree = trees_[window_id];

    // 1. Compute root hash (one-time cost)
    auto root_hash = tree->computeRootHash();

    // 2. Store metadata (durable)
    SealedTreeMetadata metadata{
        .window_id = window_id,
        .root_hash = root_hash,
        .num_rows = tree->getNumRows(),
        .sealed_at = currentTimeMs()
    };
    metadata_store_->save(metadata);

    // 3. Mark tree as sealed (no more updates)
    tree->seal();

    // 4. Optional: Serialize tree to disk, free memory
    tree->serializeToDisk(metadata_store_path_);
    trees_.erase(window_id);  // Keep only active tree in memory
}
```

### Active Tree

**Active Tree**: Merkle tree for current time window (ongoing writes).

**Properties**:
1. **Mutable**: Accepts new writes (leaf updates, propagate to root)
2. **Dynamic Root**: Root hash changes with every write
3. **Memory Resident**: Always in memory (no disk serialization)

**Update on Write**:

```cpp
auto MerkleForest::insertRow(const Row& row) -> void {
    // 1. Determine window based on version_id timestamp
    auto ts_ms = row.version_id.getTimestamp();
    auto window_id = ts_ms / window_size_ms_;

    // 2. Get or create tree for this window
    auto tree = getOrCreateTree(window_id);

    // 3. Insert row into tree (update leaf, propagate to root)
    tree->insertLeaf(row.row_id, row.hash());

    // 4. Check if previous window should be sealed
    auto current_window = currentTimeMs() / window_size_ms_;
    if (window_id < current_window - 1) {
        // Late-arriving write (within grace period)
        // Tree not yet sealed, accept write
    } else if (window_id < current_window) {
        // Window passed, seal previous windows
        for (auto wid = sealed_window_id_ + 1; wid < current_window; ++wid) {
            sealTree(wid);
        }
        sealed_window_id_ = current_window - 1;
    }
}
```

### Forest Structure

**Forest**: Collection of time-partitioned Merkle trees.

```
Merkle Forest (Shard 001):

  Sealed Trees (on disk, root hashes in memory):
    Window 0: root_hash=0xAAA, num_rows=50000
    Window 1: root_hash=0xBBB, num_rows=52000
    Window 2: root_hash=0xCCC, num_rows=51000
    ...
    Window N-1: root_hash=0xDDD, num_rows=53000

  Active Tree (in memory):
    Window N: root_hash=0x??? (changes with every write), num_rows=1200
```

---

## Tree Structure

### Leaf Node

**Content**: Row identifier + row hash.

```cpp
struct LeafNode {
    UuidV8HLC row_id;        // Primary key (UUIDv7 or UUIDv8-HLC)
    std::array<uint8_t, 32> row_hash;  // SHA-256(row_data || version_id)

    auto hash() const -> std::array<uint8_t, 32> {
        return sha256(row_id.bytes, row_hash);
    }
};
```

**Row Hash Computation**:

```cpp
auto computeRowHash(const Row& row) -> std::array<uint8_t, 32> {
    // Concatenate: row_id || all column values || version_id
    std::vector<uint8_t> data;
    data.insert(data.end(), row.row_id.bytes.begin(), row.row_id.bytes.end());

    for (const auto& col : row.columns) {
        auto serialized = col.serialize();  // Canonical serialization
        data.insert(data.end(), serialized.begin(), serialized.end());
    }

    data.insert(data.end(), row.version_id.bytes.begin(), row.version_id.bytes.end());

    return sha256(data);
}
```

### Internal Node

**Content**: Hash of left and right children.

```cpp
struct InternalNode {
    std::array<uint8_t, 32> left_hash;
    std::array<uint8_t, 32> right_hash;

    auto hash() const -> std::array<uint8_t, 32> {
        std::vector<uint8_t> data;
        data.insert(data.end(), left_hash.begin(), left_hash.end());
        data.insert(data.end(), right_hash.begin(), right_hash.end());
        return sha256(data);
    }
};
```

### Tree Construction

**Binary Merkle Tree** (balanced):

```cpp
class MerkleTree {
public:
    auto insertLeaf(const UuidV8HLC& row_id, const RowHash& row_hash) -> void {
        leaves_[row_id] = row_hash;
        markDirty();  // Root hash needs recomputation
    }

    auto computeRootHash() -> std::array<uint8_t, 32> {
        if (leaves_.empty()) {
            return std::array<uint8_t, 32>{};  // Empty tree
        }

        // Sort leaves by row_id (deterministic ordering)
        std::vector<LeafNode> sorted_leaves;
        for (const auto& [row_id, row_hash] : leaves_) {
            sorted_leaves.push_back({row_id, row_hash});
        }
        std::sort(sorted_leaves.begin(), sorted_leaves.end(),
                  [](const auto& a, const auto& b) { return a.row_id < b.row_id; });

        // Build tree bottom-up
        std::vector<std::array<uint8_t, 32>> level_hashes;
        for (const auto& leaf : sorted_leaves) {
            level_hashes.push_back(leaf.hash());
        }

        // Pair up and hash until single root
        while (level_hashes.size() > 1) {
            std::vector<std::array<uint8_t, 32>> next_level;
            for (size_t i = 0; i < level_hashes.size(); i += 2) {
                if (i + 1 < level_hashes.size()) {
                    // Pair exists
                    InternalNode node{level_hashes[i], level_hashes[i+1]};
                    next_level.push_back(node.hash());
                } else {
                    // Odd number of nodes, promote last node
                    next_level.push_back(level_hashes[i]);
                }
            }
            level_hashes = std::move(next_level);
        }

        return level_hashes[0];  // Root hash
    }

private:
    std::map<UuidV8HLC, RowHash> leaves_;  // Sorted by row_id
    bool dirty_ = true;
};
```

---

## Verification Algorithm

### Gossip Protocol

**Frequency**: Every 60 seconds (configurable: `anti_entropy_interval_ms = 60000`)

**Participants**: Random pairs of replicas (node A ↔ node B)

**Message**: Exchange sealed tree root hashes

```
Node A → Node B:
  Gossip Message {
    shard_id: 001,
    sealed_trees: [
      {window_id: 0, root_hash: 0xAAA, num_rows: 50000},
      {window_id: 1, root_hash: 0xBBB, num_rows: 52000},
      ...
    ],
    active_tree: {window_id: N, num_rows: 1200}  // No root hash (dynamic)
  }
```

### Sealed Tree Verification

**Algorithm** (O(1) per tree):

```cpp
auto AntiEntropyCoordinator::verifySealedTrees(
    const std::vector<SealedTreeMetadata>& local_trees,
    const std::vector<SealedTreeMetadata>& remote_trees
) -> std::vector<WindowId> {
    std::vector<WindowId> divergent_windows;

    // Compare sealed trees by window_id
    for (const auto& local_tree : local_trees) {
        auto remote_tree = findTreeByWindowId(remote_trees, local_tree.window_id);

        if (!remote_tree.has_value()) {
            // Remote missing this window (replica out of sync)
            divergent_windows.push_back(local_tree.window_id);
            continue;
        }

        if (local_tree.root_hash != remote_tree->root_hash) {
            // Root hash mismatch → Divergence detected
            divergent_windows.push_back(local_tree.window_id);
        }
    }

    return divergent_windows;
}
```

**Cost**: O(T) comparisons where T = number of sealed trees (typically ~720 for 30 days at 1-hour windows).

### Divergence Repair

**Step 1**: Identify divergent window (e.g., Window 5)

```
Node A: Window 5 root_hash = 0xCCCCC
Node B: Window 5 root_hash = 0xDDDDD
```

**Step 2**: Drill down into divergent tree

```
Node A → Node B: Request Window 5 tree structure

Node B responds:
  Window 5 Tree (depth 16):
    Level 0 (root): Hash = 0xDDDDD
    Level 1 (children): Left=0x111, Right=0x222

Node A compares local Level 1:
  Local: Left=0x111, Right=0x333  ← Right child diverged
```

**Step 3**: Continue drill-down to leaf level

```
Drill down into right subtree...
  Level 2: Left=0xAAA, Right=0xBBB
  Level 3: ...
  ...
  Level 16 (leaves): Row_X hash diverged

Identify divergent row: Row_X (row_id='user_456')
```

**Step 4**: Sync divergent row

```
Node B → Node A: Send Row_X full data
  {row_id='user_456', name='Bob', version_id=(ts=1001, counter=0)}

Node A:
  1. Check if Row_X exists locally
     - If missing: Insert Row_X
     - If exists: Conflict resolution (UUIDv8-HLC comparison)
  2. Update leaf hash in Window 5 tree
  3. Recompute Window 5 root hash → Should now match Node B
```

**Pseudocode**:

```cpp
auto AntiEntropyCoordinator::repairWindow(
    ShardId shard_id,
    WindowId window_id,
    NodeId local_node,
    NodeId remote_node
) -> Status {
    // 1. Fetch tree structure from remote
    auto remote_tree = fetchTree(remote_node, shard_id, window_id);
    auto local_tree = getLocalTree(shard_id, window_id);

    // 2. Drill down to find divergent leaves
    auto divergent_rows = compareTrees(local_tree, remote_tree);

    // 3. Sync each divergent row
    for (const auto& row_id : divergent_rows) {
        auto remote_row = fetchRow(remote_node, shard_id, row_id);
        auto local_row = getLocalRow(shard_id, row_id);

        if (!local_row.has_value()) {
            // Missing locally, insert
            insertRow(shard_id, remote_row);
        } else {
            // Conflict: Resolve via UUIDv8-HLC
            auto winner = resolveConflict(local_row.value(), remote_row);
            if (winner.version_id != local_row->version_id) {
                updateRow(shard_id, winner);
            }
        }
    }

    return Status::OK();
}
```

### Active Tree Verification

**Challenge**: Active tree root hash changes with every write (can't compare directly).

**Solution**: Read repair (approximate verification).

**Read Repair Algorithm**:

```cpp
auto AntiEntropyCoordinator::verifyActiveTree(
    ShardId shard_id,
    WindowId active_window
) -> void {
    // 1. Sample random rows from active window (e.g., 1000 rows)
    auto sampled_rows = sampleRows(shard_id, active_window, /*count=*/1000);

    // 2. For each sampled row, query all replicas
    for (const auto& row_id : sampled_rows) {
        auto versions = queryAllReplicas(shard_id, row_id);

        // 3. Find latest version (UUIDv8-HLC)
        auto latest = std::max_element(versions.begin(), versions.end(),
            [](const auto& a, const auto& b) { return a.version_id < b.version_id; });

        // 4. Repair replicas with older versions
        for (const auto& [replica, version] : versions) {
            if (version.version_id < latest->version_id) {
                sendReadRepair(replica, shard_id, *latest);
            }
        }
    }
}
```

**Frequency**: Every 10 minutes (configurable: `active_tree_repair_interval_ms = 600000`)

**Trade-off**: Probabilistic verification (may miss some divergences), but low overhead.

---

## Read Repair

### Read-Time Repair

**Trigger**: During normal read path (quorum read), detect version divergence.

**Workflow**:

```sql
-- Client: SELECT * FROM users WHERE user_id = 'user_123'

Coordinator:
  1. Query R replicas (R=2, N=3)
     - Node A: {version_id=(ts=1000, counter=5), name='Alice'}
     - Node B: {version_id=(ts=1000, counter=10), name='Alice Updated'}

  2. Detect divergence: version_id_A < version_id_B
  3. Return latest version to client: version_id_B
  4. Asynchronously repair Node A:
     Coordinator → Node A: Update user_id='user_123' to version_id_B
```

**Implementation**:

```cpp
auto ReplicationCoordinator::coordinateRead(
    const ReadRequest& req,
    const ReplicaSet& replicas,
    size_t read_quorum
) -> Result<RowData> {
    // 1. Query replicas
    auto versions = queryReplicas(replicas, req, read_quorum);

    // 2. Find latest version
    auto latest = findLatestVersion(versions);

    // 3. Trigger read repair (async)
    for (size_t i = 0; i < versions.size(); ++i) {
        if (versions[i].version_id < latest.version_id) {
            std::async(std::launch::async, [&]() {
                sendReadRepair(replicas[i], latest);
            });
        }
    }

    return latest;
}
```

### Background Repair

**Trigger**: Anti-entropy verification detects divergence.

**Workflow**:

```
Anti-Entropy Process (every 60 seconds):
  1. Compare sealed tree root hashes
  2. Detect divergent window (e.g., Window 5)
  3. Drill down to find divergent rows
  4. Repair each divergent row (fetch from replica, apply locally)
```

**Priority**: Background task (low priority, doesn't block user queries).

---

## LSM Compaction Integration

### LSM Tree Compaction

**ScratchBird Storage**: LSM tree (Log-Structured Merge Tree) with levels L0, L1, ..., Ln.

**Compaction**: Merge SSTables (Sorted String Tables) from lower levels to higher levels.

**Compaction Trigger**: L0 reaches threshold (e.g., 4 SSTables), merge to L1.

### Merkle Tree Sealing Alignment

**Observation**: LSM compaction is expensive (I/O bound), already reads all rows in level.

**Optimization**: Compute Merkle tree during compaction (amortize cost).

**Workflow**:

```
LSM Compaction (L0 → L1):
  1. Read all SSTables in L0
  2. For each row:
     a. Merge row (resolve version conflicts)
     b. Write row to L1 SSTable
     c. Compute row hash → Add to Merkle tree leaf

  3. After compaction completes:
     a. Seal Merkle tree for compacted time window
     b. Compute root hash
     c. Store metadata
```

**Implementation**:

```cpp
auto LSMCompactor::compactLevel(Level level) -> Status {
    auto sstables = level.getSSTables();
    MerkleTree merkle_tree;

    // 1. Merge SSTables
    auto merged_rows = mergeSSTables(sstables);

    // 2. Write to next level + build Merkle tree
    for (const auto& row : merged_rows) {
        // Write to next level
        next_level_.appendRow(row);

        // Add to Merkle tree
        auto row_hash = computeRowHash(row);
        merkle_tree.insertLeaf(row.row_id, row_hash);
    }

    // 3. Seal Merkle tree (if time window passed)
    auto window_id = level.getWindowId();
    auto current_window = currentTimeMs() / window_size_ms_;
    if (window_id < current_window) {
        merkle_forest_->sealTree(window_id, merkle_tree.computeRootHash());
    }

    return Status::OK();
}
```

**Benefit**: Zero additional cost for Merkle tree computation (piggyback on compaction).

### Compaction-Aligned Sealing

**Sealing Interval**: Align with LSM compaction frequency (e.g., every 4 hours).

**Configuration**:

```yaml
# ScratchBird config
lsm:
  compaction_interval_hours: 4

merkle_forest:
  window_size_hours: 1  # 1-hour windows
  sealing_grace_period_minutes: 10  # Wait 10 min after window ends
  seal_on_compaction: true  # Seal during LSM compaction
```

**Result**: Merkle tree sealing is "free" (no separate I/O pass).

---

## Implementation Details

### In-Memory vs On-Disk

**Active Tree**: In-memory (frequent updates, needs fast access)

```cpp
class ActiveMerkleTree {
    std::map<UuidV8HLC, RowHash> leaves_;  // Sorted map (in-memory)
    std::array<uint8_t, 32> root_hash_;
    bool dirty_ = true;
};
```

**Sealed Trees**: On-disk (immutable, only root hash in memory)

```cpp
struct SealedTreeMetadata {
    WindowId window_id;
    std::array<uint8_t, 32> root_hash;
    uint64_t num_rows;
    uint64_t sealed_at_ms;
    std::string tree_file_path;  // Path to serialized tree on disk
};

class MerkleForest {
    std::vector<SealedTreeMetadata> sealed_trees_;  // In-memory metadata
    std::unique_ptr<ActiveMerkleTree> active_tree_;
};
```

**Serialization** (sealed tree to disk):

```cpp
auto MerkleTree::serializeToDisk(const std::string& file_path) -> Status {
    std::ofstream file(file_path, std::ios::binary);

    // 1. Write number of leaves
    uint64_t num_leaves = leaves_.size();
    file.write(reinterpret_cast<const char*>(&num_leaves), sizeof(num_leaves));

    // 2. Write each leaf (row_id + row_hash)
    for (const auto& [row_id, row_hash] : leaves_) {
        file.write(reinterpret_cast<const char*>(row_id.bytes.data()), 16);
        file.write(reinterpret_cast<const char*>(row_hash.data()), 32);
    }

    return Status::OK();
}
```

### Memory Footprint

**Sealed Tree Metadata**:
- Per tree: 32 bytes (root hash) + 8 bytes (window_id) + 8 bytes (num_rows) = 48 bytes
- 720 trees (30 days, 1-hour windows): 48 * 720 = 34 KB (negligible)

**Active Tree**:
- Per leaf: 16 bytes (row_id) + 32 bytes (row_hash) = 48 bytes
- 1000 rows in active window: 48 * 1000 = 48 KB
- 10,000 rows in active window: 48 * 10,000 = 480 KB (acceptable)

**Total Memory**: < 1 MB per shard (even with 30 days of sealed trees + busy active tree)

### Gossip Message Format

```cpp
struct AntiEntropyGossip {
    ShardId shard_id;
    NodeId sender_node_id;
    uint64_t timestamp_ms;

    // Sealed trees
    std::vector<SealedTreeMetadata> sealed_trees;

    // Active tree (no root hash, just metadata)
    struct {
        WindowId window_id;
        uint64_t num_rows;
    } active_tree;
};
```

**Message Size**:
- 720 sealed trees * 48 bytes = 34 KB
- Active tree metadata: 16 bytes
- Total: ~35 KB per gossip message (acceptable)

**Compression**: Optional gzip compression for large forests (e.g., > 1000 trees).

---

## Testing and Verification

### Unit Tests

**Merkle Tree Construction**:

```cpp
TEST(MerkleTree, RootHashDeterministic) {
    MerkleTree tree1;
    tree1.insertLeaf(UuidV8HLC{/* row_1 */}, RowHash{/* hash_1 */});
    tree1.insertLeaf(UuidV8HLC{/* row_2 */}, RowHash{/* hash_2 */});
    auto root1 = tree1.computeRootHash();

    MerkleTree tree2;
    tree2.insertLeaf(UuidV8HLC{/* row_2 */}, RowHash{/* hash_2 */});  // Different order
    tree2.insertLeaf(UuidV8HLC{/* row_1 */}, RowHash{/* hash_1 */});
    auto root2 = tree2.computeRootHash();

    ASSERT_EQ(root1, root2);  // Deterministic (sorted by row_id)
}
```

**Tree Sealing**:

```cpp
TEST(MerkleForest, SealedTreeImmutable) {
    MerkleForest forest;

    // Insert rows in Window 0
    forest.insertRow(Row{/* version_id.ts=1000 */});
    forest.insertRow(Row{/* version_id.ts=2000 */});

    // Advance time, seal Window 0
    advanceTimeTo(3600000 + 600000);  // 1 hour + 10 min grace
    forest.sealTree(0);

    auto sealed_root = forest.getSealedTreeRootHash(0);

    // Insert more rows in Window 1 (should not affect Window 0)
    forest.insertRow(Row{/* version_id.ts=3700000 */});

    ASSERT_EQ(forest.getSealedTreeRootHash(0), sealed_root);  // Unchanged
}
```

### Integration Tests

**Anti-Entropy Verification**:

```cpp
TEST(AntiEntropyIntegration, DetectDivergence) {
    Cluster cluster(3);  // 3 nodes

    // Write to Node A and Node B (W=2)
    Node("A").execute("INSERT INTO users VALUES ('user_123', 'Alice')");

    // Simulate Node C missing the write (network partition)
    Node("C").setOffline(true);

    // Perform anti-entropy
    cluster.performAntiEntropy();

    // Verify divergence detected
    auto divergent_windows = Node("A").getAntiEntropyReport();
    ASSERT_EQ(divergent_windows.size(), 1);  // Current window diverged
}
```

**Repair Verification**:

```cpp
TEST(AntiEntropyIntegration, RepairDivergentReplica) {
    Cluster cluster(3);

    // Node C missing Row 1
    Node("A").execute("INSERT INTO users VALUES ('user_123', 'Alice')");
    Node("B").execute("INSERT INTO users VALUES ('user_123', 'Alice')");
    // Node C offline, missed write

    Node("C").setOnline(true);

    // Anti-entropy repair
    cluster.performAntiEntropy();

    // Verify Node C repaired
    auto result = Node("C").query("SELECT * FROM users WHERE user_id='user_123'");
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0].name, "Alice");
}
```

### Performance Tests

**Anti-Entropy Overhead**:

```bash
# Benchmark: 1M rows, 720 sealed trees (30 days)
./scratchbird_benchmark --mode=anti_entropy --rows=1000000 --sealed_trees=720
# Expected: < 2% CPU overhead
```

**Verification Latency**:

```bash
# Benchmark: Compare 720 sealed trees across 3 replicas
./scratchbird_benchmark --mode=verify_sealed_trees --trees=720 --replicas=3
# Expected: < 100ms (O(1) per tree, 720 comparisons)
```

**Repair Bandwidth**:

```bash
# Benchmark: Repair 1000 divergent rows (out of 1M)
./scratchbird_benchmark --mode=repair_divergence --total_rows=1000000 --divergent_rows=1000
# Expected: Transfer only 1000 rows (~50 KB), not entire dataset
```

---

## Appendix: Comparison with Industry Systems

### Apache Cassandra

**Approach**: Hash-based Merkle trees (per SSTable)

**Problems**:
- **Perpetual Re-Computation**: Every compaction rebuilds entire Merkle tree (10-20% CPU)
- **Stochastic Verification**: Periodic full-tree comparisons (expensive)

**ScratchBird Advantage**:
- **Sealed Trees**: Past windows never re-computed (0% CPU)
- **Deterministic Verification**: Compare sealed roots in O(1), drill down only on mismatch

### Amazon DynamoDB

**Approach**: Merkle trees for anti-entropy, but details not public

**Speculation**: Likely hash-based (similar to Cassandra)

**ScratchBird Advantage**: Time-partitioning explicitly exploits UUIDv7 temporal locality

### CockroachDB

**Approach**: No Merkle trees (uses Raft consensus for strong consistency)

**Trade-off**: Raft guarantees no divergence (CP system), but sacrifices availability

**ScratchBird Context**: Mode 3 (leaderless) is AP system, Merkle trees needed for eventual consistency

---

**Document Status:** DRAFT (Beta Specification Phase)
**Last Updated:** 2026-01-09
**Next Review:** Weekly during implementation
**Dependencies:** 01_UUIDV8_HLC_ARCHITECTURE.md, LSM storage engine

---

**End of Document 04**

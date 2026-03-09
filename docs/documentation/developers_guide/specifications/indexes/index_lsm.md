# Specification: LSM-Tree Index

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage/indexes |
| **Spec Version** | 1.0.0 |
| **Status** | 🟡 Review |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Development Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/lsm_tree_index.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/lsm_tree.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/lsm_tree_index.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/lsm_tree_components.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_lsm_tree_index.cpp:1`

## Synopsis

LSM-Tree (Log-Structured Merge-Tree) provides write-optimized indexing using tiered sorted runs. Writes go to an in-memory memtable, then flush to disk as immutable sorted files (SSTables), with periodic compaction merging overlapping ranges.

## Scope

### In Scope

- Memtable structure (skip list or B-tree)
- SSTable format (sorted string table)
- Leveled and tiered compaction
- Bloom filter for SSTable pruning
- Block-based compression
- Point and range queries

### Out of Scope

- In-place updates (immutable SSTables)
- Secondary indexes (use separate LSM)
- Complex transactions across levels

## Background

LSM-Tree advantages:
- **Write amplification**: ~10-30x better than B-tree
- **Sequential writes**: Only append to SSTables
- **Compression**: Better due to sorted data
- **Trade-off**: Higher read amplification (check multiple levels)

Structure:
- **Memtable**: In-memory write buffer (skip list)
- **WAL**: Write-ahead log for durability
- **Level 0**: Recently flushed SSTables (may overlap)
- **Level N+1**: 10x larger than level N, no overlaps

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:669
enum class IndexType : uint8_t {
    LSM = 11,         // LSM-Tree (Log-Structured Merge-Tree)
    // ... other types
};
```

### LSM-Tree Architecture

```
Writes:                Reads:
┌──────────┐          ┌──────────────┐
│  Client  │          │   Memtable   │ ← Check first (newest)
└────┬─────┘          └──────┬───────┘
     │                       │
     ▼                       ▼
┌──────────┐          ┌──────────────┐
│ Memtable │ ←─────── │  Level 0     │ ← Check SSTables
│(Skip List)│          │ (SSTables)   │   (may overlap)
└────┬─────┘          └──────┬───────┘
     │                       │
     │ (when full)           ▼
     ▼               ┌──────────────┐
┌──────────┐         │  Level 1     │ ← No overlaps within level
│   WAL    │         │ (SSTables)   │   Binary search
└──────────┘         └──────┬───────┘
                            │
                            ▼
                    ┌──────────────┐
                    │  Level 2+    │ ← Exponentially larger
                    └──────────────┘

Compaction: Level N SSTables merge into Level N+1
```

### Memtable Structure

```cpp
// Source: scratchbird/core/lsm_tree.h
struct Memtable {
    // Skip list for O(log n) insert/lookup
    SkipList<Key, Value> data;
    
    size_t size_bytes;           // Current size
    size_t size_limit;           // Flush threshold
    uint64_t sequence_number;    // For versioning
    
    // MGA support
    std::map<uint64_t, Memtable*> snapshots;  // Active snapshots
};

// Skip list node
struct SkipListNode {
    Key key;
    Value value;
    uint64_t sequence;           // Version sequence
    bool is_deleted;             // Tombstone marker
    std::vector<SkipListNode*> forward;  // Level pointers
};
```

### SSTable Format

```cpp
// Source: scratchbird/core/lsm_tree_components.h
struct SSTable {
    SSTableMetadata metadata;
    std::vector<Block> index_blocks;
    std::vector<Block> data_blocks;
    BloomFilter bloom_filter;
};

struct SSTableMetadata {
    uint64_t file_id;
    uint32_t level;              // LSM level
    Key smallest_key;            // First key in SSTable
    Key largest_key;             // Last key in SSTable
    uint64_t entry_count;
    uint64_t file_size;
    uint64_t creation_time;
};

// Block structure (typically 4KB-64KB)
struct Block {
    uint32_t uncompressed_size;
    uint32_t compressed_size;
    CompressionType codec;       // LZ4, ZSTD, SNAPPY
    
    // Restart points for binary search within block
    uint32_t restart_count;
    uint32_t restart_offsets[];
    
    // Key-value pairs (prefix compressed)
    uint8_t data[];
};
```

### Block Entry Format

```
┌─────────────────────────────────────────┐
│ Shared prefix length (varint)           │
├─────────────────────────────────────────┤
│ Unshared key suffix length (varint)     │
├─────────────────────────────────────────┤
│ Key suffix bytes                        │
├─────────────────────────────────────────┤
│ Value length (varint)                   │
├─────────────────────────────────────────┤
│ Value bytes                             │
└─────────────────────────────────────────┘
```

## Algorithms

### Algorithm: Write (Put)

```
Input:  key, value
Output: Status

1. Append to WAL (Write-Ahead Log):
   - Write (key, value, sequence) to log
   - fsync if synchronous_writes enabled

2. Insert into Memtable:
   - Create skip list node
   - Set sequence number
   - Update memtable size

3. If memtable size >= size_limit:
   a. Freeze current memtable
   b. Create new empty memtable
   c. Schedule background flush

4. Return OK
```

### Algorithm: Flush Memtable

```
Input:  Frozen memtable
Output: New SSTable at Level 0

1. Create new SSTable file

2. Iterate memtable in key order:
   a. Group entries into blocks (target block_size)
   b. Apply prefix compression within block
   c. Compress block with codec
   d. Write block to file
   e. Add to block index (restart points)

3. Build Bloom filter from all keys

4. Write metadata + block index + bloom filter

5. Sync file to disk

6. Add SSTable to Level 0 list

7. Delete corresponding WAL segments

8. Free memtable memory
```

### Algorithm: Read (Get)

```
Input:  key
Output: Value or not found

1. Search memtable:
   - If found: return value (or deleted)

2. Search immutable memtables (if any):
   - Check in reverse chronological order

3. For each level from 0 to max:
   a. Use bloom filters to skip SSTables:
      - If bloom filter says "definitely not": skip
   
   b. For candidate SSTables:
      - If key < smallest or key > largest: skip
      - Search SSTable using block index
      - If found: return value

4. Return NOT_FOUND
```

### Algorithm: SSTable Search

```
Input:  key, SSTable
Output: Value or not found

1. Binary search block index:
   - Find block where key might exist

2. Load and decompress block (may be cached)

3. Binary search within block:
   - Use restart points for coarse search
   - Linear scan between restart points
   - Reconstruct full keys from prefix compression

4. If found: return value
   Else: return NOT_FOUND
```

### Algorithm: Leveled Compaction

```
Input:  Level N that needs compaction
Output: Compacted level

1. Select SSTable(s) from Level N to compact
   - Choose SSTable with most overlapping in Level N+1
   - Or: oldest SSTable

2. Find all overlapping SSTables in Level N+1

3. Merge sort all input SSTables:
   - Maintain heap of iterators
   - Output sorted stream to new SSTable(s)
   - Handle duplicate keys (keep newest)
   - Remove tombstones if safe (no snapshot needs them)

4. Atomically replace:
   - Remove old SSTables
   - Add new SSTables to Level N+1

5. Delete old SSTable files

Compaction trigger:
- Level 0: #SSTables > threshold
- Level N: Total size > 10^N MB
```

### Algorithm: Range Scan

```
Input:  start_key, end_key
Output: Iterator over range

1. Create iterators for all sources:
   - Memtable iterator
   - Immutable memtable iterators
   - SSTable iterators (for each level)

2. Advance all iterators to start_key

3. Return merging iterator:
   - Min-heap ordered by current key
   - For each Next():
     a. Pop smallest key from heap
     b. Advance that iterator
     c. Push new position to heap
     d. Handle duplicates (skip older versions)
```

## Bloom Filter Configuration

```cpp
// Source: scratchbird/core/lsm_bloom_filter.h
struct BloomFilter {
    uint32_t bits_per_key;       // Default: 10
    uint32_t num_hashes;         // Default: 6
    
    // False positive rate ≈ (1 - e^(-kn/m))^k
    // With 10 bits/key, 6 hashes: ~1% FP rate
};
```

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | Keys within SSTable are sorted | Build check |
| I2 | No overlapping SSTables in L1+ | Compaction check |
| I3 | Bloom filter never false negative | Implementation |
| I4 | WAL entry has corresponding memtable entry | Recovery check |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `lsm.memtable_size` | 64MB | Flush threshold |
| `lsm.block_size` | 4KB | SSTable block size |
| `lsm.level0_file_num` | 4 | L0 SSTable limit |
| `lsm.bloom_bits_per_key` | 10 | Bloom filter density |
| `lsm.compression` | ZSTD | Block compression |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_lsm_tree_index.cpp` | Core LSM operations |
| `test_lsm_compaction.cpp` | Compaction algorithms |
| `test_lsm_recovery.cpp` | WAL recovery |

## Related Specifications

- [index_btree.md](./index_btree.md) - Read-optimized alternative
- [index_art.md](./index_art.md) - Memory-optimized index

## References

- O'Neil, P. et al. (1996). The Log-Structured Merge-Tree.
- RocksDB documentation

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification |

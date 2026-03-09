# Specification: Hash Index

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/hash_index.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/hash_index.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_hash_index.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_hash_index_concurrent.cpp:1`

## Synopsis

Hash index provides O(1) average-case lookup for exact equality queries using an extendible hashing scheme. It supports dynamic resizing via directory expansion and bucket splitting, with MGA-compliant transaction visibility tracking.

## Scope

### In Scope

- Extendible hash table structure
- Directory and bucket page layouts
- Hash function selection (MurmurHash3)
- Insert, search, and delete operations
- Dynamic expansion and bucket splitting
- MGA transaction visibility

### Out of Scope

- Range queries (use B-tree)
- Partial key matching
- Full-text search
- Multi-column index optimization

## Background

Hash indexes are optimized for equality comparisons (=). Unlike B-trees which maintain order, hash indexes distribute keys uniformly across buckets using a hash function. This provides:
- O(1) average lookup time
- O(1) average insert time (amortized)
- Efficient duplicate detection for unique constraints

ScratchBird uses extendible hashing with:
- Global depth (directory size)
- Local depth (per-bucket split history)
- Overflow chains for collision handling
- Dynamic directory expansion

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:658
enum class IndexType : uint8_t {
    HASH = 1,         // Hash index
    // ... other types
};
```

### Page Types

| Page Type | Value | Description |
|-----------|-------|-------------|
| `PAGE_TYPE_HASH_META` | 0x30 | Metadata page with directory pointer |
| `PAGE_TYPE_HASH_DIRECTORY` | 0x31 | Directory mapping hash to buckets |
| `PAGE_TYPE_HASH_BUCKET` | 0x32 | Bucket storing hash entries |
| `PAGE_TYPE_HASH_OVERFLOW` | 0x33 | Overflow chain page |

### Meta Page Layout (112+ bytes)

```cpp
// Source: scratchbird/core/hash_index.h:54
struct SBHashIndexMetaPage {
    PageHeader hip_header;          // Standard page header (80 bytes)
    uint8_t hip_index_uuid[16];     // Index UUID bytes (16 bytes)
    uint32_t hip_hash_func_id;      // Hash function ID (4 bytes)
    uint32_t hip_global_depth;      // Global depth (4 bytes) - max 20
    uint64_t hip_directory_page;    // First directory page number (8 bytes)
    uint64_t hip_num_tuples;        // Total number of indexed tuples (8 bytes)
    uint64_t hip_num_deleted;       // Number of deleted entries (8 bytes)
    uint8_t hip_reserved[];         // Reserved for future use
};
```

### Directory Page Layout

```cpp
// Source: scratchbird/core/hash_index.h:67
struct SBHashDirectoryPage {
    PageHeader hdp_header;          // Standard page header (80 bytes)
    uint64_t hdp_next_page;         // Next directory page (0 if last) (8 bytes)
    uint64_t hdp_bucket_pointers[]; // Bucket page numbers (flexible array)
};
```

**Directory Capacity:**
- 8KB page: ~1016 bucket pointers per directory page
- 16KB page: ~2040 bucket pointers per directory page

### Hash Entry Structure (36 bytes)

```cpp
// Source: scratchbird/core/hash_index.h:77
struct HashEntry {
    uint64_t he_key_hash;   // Full 64-bit hash of the key (8 bytes)
    GPID he_gpid;           // Global Page ID (8 bytes)
    uint16_t he_slot;       // Slot number within page (2 bytes)
    uint16_t he_padding;    // Padding to maintain alignment (2 bytes)
    uint64_t he_xmin;       // Transaction that created this entry (8 bytes)
    uint64_t he_xmax;       // Transaction that deleted this entry (8 bytes)

    // Helper methods
    TID getTID() const { return TID(he_gpid, he_slot); }
    void setTID(const TID &tid) { he_gpid = tid.gpid; he_slot = tid.slot; }
};
```

Static assert: `sizeof(HashEntry) == 36`

### Bucket Page Layout

```cpp
// Source: scratchbird/core/hash_index.h:94
struct SBHashBucketPage {
    PageHeader hbp_header;          // Standard page header (80 bytes)
    uint16_t hbp_entry_count;       // Number of entries in this page (2 bytes)
    uint16_t hbp_local_depth;       // Local depth of this bucket (2 bytes)
    uint32_t hbp_deleted_count;     // Number of deleted entries (4 bytes)
    uint64_t hbp_overflow_page;     // Next overflow page (0 if none) (8 bytes)
    uint8_t hbp_reserved[16];       // Reserved for alignment (16 bytes)
    HashEntry hbp_entries[];        // Hash entries (flexible array)
};
```

**Bucket Capacity:**
- 8KB page: ~220 entries (36 bytes each + overhead)
- Split threshold: 90% full

## Algorithms

### Algorithm: Hash Computation

```cpp
// Source: scratchbird/core/hash_functions.h
uint64_t murmurhash3_64(const void* key, size_t len, uint64_t seed) {
    // MurmurHash3 64-bit variant
    // High-quality hash with good distribution
    // Seed typically = index UUID hash
}

// Directory index calculation
uint32_t getDirectoryIndex(uint64_t hash, uint32_t global_depth) {
    return hash & ((1u << global_depth) - 1);
}
```

### Algorithm: Insert

```
Input:  key_data, key_len, TID, xid
Output: Status

1. Compute hash = murmurhash3_64(key_data, key_len, seed)

2. Acquire shared lock on directory_mutex_

3. Compute directory_index = getDirectoryIndex(hash, global_depth)

4. Look up bucket_page = directory[directory_index]

5. Pin bucket page in buffer pool

6. Search bucket for existing key:
   For each entry in bucket and overflow chain:
   - If entry.he_key_hash == hash:
     - Verify actual key equality (key_data vs stored key)
     - If equal and visible: Check unique constraint

7. If unique violation found:
   - Unpin bucket page
   - Release directory lock
   - Return SB_ERR_DUPLICATE_KEY

8. Check if bucket needs split:
   - If entry_count >= max_entries_per_bucket * 0.9
   - Or overflow chain length >= 5

9. If split needed:
   a. Upgrade to exclusive directory lock
   b. If local_depth == global_depth:
      - Expand directory (double size)
      - Increment global_depth
   c. Split bucket
   d. Redistribute entries
   e. Retry insert

10. Add new entry:
    - he_key_hash = hash
    - he_gpid = tid.gpid
    - he_slot = tid.slot
    - he_xmin = xid
    - he_xmax = 0

11. Mark bucket page dirty, unpin

12. Release directory lock

13. Return OK
```

### Algorithm: Search

```
Input:  key_data, key_len, current_xid
Output: List of matching TIDs

1. Compute hash = murmurhash3_64(key_data, key_len, seed)

2. Acquire shared lock on directory_mutex_

3. Compute directory_index = getDirectoryIndex(hash, global_depth)

4. Look up bucket_page = directory[directory_index]

5. Pin bucket page

6. results = []

7. Traverse bucket and overflow chain:
   For each entry in bucket and overflow pages:
   a. If entry.he_key_hash != hash: continue
   b. Verify actual key equality
   c. Check MGA visibility:
      - Skip if entry.he_xmax != 0 AND entry.he_xmax < OIT
      - Skip if entry.he_xmin > current_xid AND in active set
      - Skip if entry.he_xmin is from aborted transaction
   d. If visible: Add TID to results

8. Unpin all pages

9. Release directory lock

10. Return results
```

### Algorithm: Bucket Split

```
Input:  Bucket page B, hash H causing split
Output: Status, bucket split into B and B'

1. Allocate new bucket page B'

2. Increment local_depth for both buckets:
   - B.hbp_local_depth++
   - B'.hbp_local_depth = B.hbp_local_depth

3. For each entry in B (including overflow chain):
   a. Compute bit = 1 << (B.hbp_local_depth - 1)
   b. If (entry.hash & bit) == 0:
      - Keep in B
   c. Else:
      - Move to B'

4. Update directory:
   For each directory slot pointing to B:
   - If slot's high bit matches B':
     - Update to point to B'

5. Redistribute overflow pages if needed

6. Update statistics

7. Return OK
```

### Algorithm: Directory Expansion

```
Input:  Current directory D at depth d
Output: Expanded directory at depth d+1

1. Allocate new directory pages (double current size)

2. For each entry in old directory at index i:
   a. Copy to new directory at index i
   b. Copy to new directory at index i + 2^d

3. Atomically update:
   - hip_directory_page to new directory
   - hip_global_depth = d + 1

4. Schedule old directory pages for deallocation
   (after all transactions complete)
```

### Algorithm: Remove (Logical Delete)

```
Input:  key_data, key_len, TID, xid
Output: Status

1. Compute hash = murmurhash3_64(key_data, key_len, seed)

2. Acquire shared lock on directory_mutex_

3. Compute directory_index = getDirectoryIndex(hash, global_depth)

4. Look up and pin bucket page

5. Search for exact match:
   - hash matches
   - TID matches
   - entry.he_xmax == 0 (not already deleted)

6. If found:
   - Set entry.he_xmax = xid
   - Increment bucket.hbp_deleted_count
   - Increment meta.hip_num_deleted

7. Mark dirty, unpin

8. Release directory lock

9. Return OK
```

### Algorithm: GC Compaction

```
Input:  OIT (Oldest Interesting Transaction)
Output: Number of entries removed

1. For each bucket in index:
   a. Pin bucket page
   
   b. Scan all entries:
      - If entry.he_xmax != 0 AND entry.he_xmax < OIT:
        * Remove entry (shift remaining entries)
        * Decrement hbp_entry_count
        * Increment removed_count
   
   c. Compact bucket (remove gaps)
   
   d. If bucket now empty and has overflow pages:
      * Free overflow chain
   
   e. Mark dirty, unpin

2. If many buckets empty:
   - Consider directory contraction (optional)

3. Update meta.hip_num_deleted

4. Return removed_count
```

## MGA Visibility

Hash entries track transaction IDs for MGA compliance:

```cpp
// Source: hash_index.h:77
struct HashEntry {
    uint64_t he_xmin;  // Creating transaction
    uint64_t he_xmax;  // Deleting transaction (0 = live)
    // ...
};
```

**Visibility Rules:**
1. Entry visible if: `he_xmin` committed AND (`he_xmax` == 0 OR `he_xmax` not committed)
2. Entry dead if: `he_xmax` committed AND `he_xmax` < OIT
3. Entry in-progress if: `he_xmin` is in active transaction set

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | All entries in bucket share same hash prefix (per local_depth) | Split validation |
| I2 | Directory size = 2^global_depth | Meta page check |
| I3 | Local depth <= Global depth | Bucket header validation |
| I4 | No duplicate (hash, TID) pairs with xmax=0 | Insert check |
| I5 | Overflow chain length < MAX_OVERFLOW_CHAIN | Split trigger |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `SB_ERR_DUPLICATE_KEY` | Unique constraint violation | Return error to caller |
| `SB_ERR_HASH_OVERFLOW` | Overflow chain too long | Force bucket split |
| `SB_ERR_DIRECTORY_FULL` | Global depth at MAX_GLOBAL_DEPTH | Cannot expand further |
| `SB_ERR_LOCK_TIMEOUT` | Directory lock contention | Retry with backoff |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `hash.initial_global_depth` | 4 | Initial directory entries (16 buckets) |
| `hash.max_global_depth` | 20 | Maximum directory entries (~1M) |
| `hash.bucket_fill_threshold` | 90 | Split when bucket % full |
| `hash.max_overflow_chain` | 5 | Max overflow pages before split |
| `hash.hash_func` | MurmurHash3 | Hash function selection |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_hash_index.cpp` | Basic operations, collision handling |
| `test_hash_index_concurrent.cpp` | Concurrent inserts, splits |
| `test_hash_index_gc.cpp` | Garbage collection, compaction |

## Related Specifications

- [index_btree.md](./index_btree.md) - B-tree for range queries
- [index_dml_integration.md](./index_dml_integration.md) - DML maintenance

## Glossary

| Term | Definition |
|------|------------|
| Extendible Hashing | Dynamic hash table with directory |
| Global Depth | Log2 of directory size |
| Local Depth | Per-bucket split history |
| Overflow Chain | Linked list of pages for collisions |
| MurmurHash3 | High-quality hash function |

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification with source anchors |

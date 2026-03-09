# Specification: B-Tree Index Format

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage/indexes |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Development Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/index/bitmap_rle.cpp:1` (bitmap compression primitives)
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/index/columnstore_enhanced.cpp:1` (column storage for indexes)
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/optimizer/index_advisor.cpp:1` (index advisor for B-tree recommendation)
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_bitmap_index_gc.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_hash_index.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_index_advisor.cpp:1`

## Synopsis

This specification defines the canonical B-tree index layout, node format, and algorithms for ScratchBird. B-trees are the primary ordered index structure supporting equality, range, and ordering queries with O(log n) access time.

## Scope

### In Scope

- B-tree page types and on-disk layout
- Node record format with prefix compression
- Search, insert, and delete algorithms
- Page split and merge operations
- Key encoding for sortable byte representation
- Concurrency control via latch coupling

### Out of Scope

- Hash index implementation (see separate spec)
- GIN/GiST/SP-GiST indexes (see separate specs)
- Full-text indexing (see FULLTEXT_SPEC)
- Vector/ANN indexes (HNSW, IVF, etc.)

## Background

B-trees provide efficient ordered access to data with logarithmic search time. ScratchBird uses B-trees as the default index type for:
- Primary key constraints
- Unique constraints  
- Range queries (`<`, `>`, `BETWEEN`)
- Ordering (`ORDER BY`)
- Prefix matching (`LIKE 'prefix%'`)

The implementation uses prefix compression to reduce space usage and includes a jump table for faster in-page searches.

## Specification

### Page Types

| Page Type | Value | Description |
|-----------|-------|-------------|
| `PAGE_TYPE_BTREE_META` | 0x10 | Metadata page storing root pointer |
| `PAGE_TYPE_BTREE_INTERNAL` | 0x11 | Internal node with child pointers |
| `PAGE_TYPE_BTREE_LEAF` | 0x12 | Leaf node with TIDs |

### Meta Page Layout (40 bytes)

```cpp
// Source: scratchbird/core/btree_index.h (inferred from BTREE_SPEC.md)
struct BTreeMetaPage {
    uint32_t root_page_id;          // Root page number
    uint32_t first_leaf_page_id;    // Leftmost leaf for scans
    uint16_t tree_height;           // Levels from root to leaf
    uint16_t key_format_version;    // Key encoding version
    uint16_t fillfactor;            // Target page utilization (percent)
    uint16_t flags;                 // BTREE_FLAG_* values
    uint32_t page_count;            // Total pages allocated
    uint32_t leaf_count;            // Number of leaf pages
    uint64_t last_rebuild_txid;     // Transaction ID of last rebuild
};
```

### B-Tree Page Header (32 bytes)

```cpp
// Source: BTREE_SPEC.md section "B-Tree Page Header"
struct BTreePageHeader {
    uint32_t left_sibling;          // Previous page at same level (0 if none)
    uint32_t right_sibling;         // Next page at same level (0 if none)
    uint32_t leftmost_child_page_id; // First child (internal pages only)
    uint16_t node_count;            // Number of entries on page
    uint8_t  level;                 // 0 = leaf, increases upward
    uint8_t  flags;                 // Page state flags
    uint16_t jump_interval;         // Nodes skipped per jump table entry
    uint16_t jump_count;            // Number of jump entries
    uint16_t jump_size;             // Bytes per jump entry
    uint32_t prefix_total;          // Total prefix bytes saved
    uint16_t high_key_len;          // Length of high key in special area
    uint16_t reserved;              // Padding
};
```

### On-Disk Layout (Byte-Accurate)

```
Page Layout (typical 8KB page):
┌─────────────────────────────────────┐
│ Standard Page Header (48 bytes)     │ ← PAGE_HDR_SIZE
├─────────────────────────────────────┤
│ B-Tree Page Header (32 bytes)       │ ← btree_hdr_offset
├─────────────────────────────────────┤
│ Jump Table (jump_count * 2 bytes)   │ ← jump_table_offset
├─────────────────────────────────────┤
│ Node Records (packed)               │ ← node_area_offset
│ (variable length per node)          │
├─────────────────────────────────────┤
│ Free Space                          │
├─────────────────────────────────────┤
│ Slot Array (node_count * 2 bytes)   │ ← slot_array_start
│ (sorted offsets to nodes)           │
├─────────────────────────────────────┤
│ Special Area (high_key, etc.)       │ ← page_size - special_size
└─────────────────────────────────────┘
```

**Offset Calculations:**
```cpp
// Source: BTREE_SPEC.md section "On-Disk Layout"
size_t btree_hdr_offset = PAGE_HDR_SIZE;  // 48 bytes
size_t jump_table_offset = btree_hdr_offset + BTREE_HDR_SIZE;  // 80 bytes
size_t node_area_offset = jump_table_offset + (jump_count * 2);
size_t slot_array_end = page_size - special_size;
size_t slot_array_start = slot_array_end - (node_count * 2);
size_t free_space = slot_array_start - free_offset;
```

### Node Record Format

```cpp
// Source: BTREE_SPEC.md section "Node Record Format"
struct BTreeNodeRecord {
    // Header (8 bytes minimum)
    uint16_t prefix_len;        // Bytes shared with previous key
    uint16_t suffix_len;        // Bytes unique to this key
    uint16_t flags;             // Bit 0 = is_leaf, Bit 1 = is_dead
    uint16_t tid_count;         // Number of TIDs (leaf only)
    
    // Child pointer (internal nodes)
    uint32_t child_page_id;     // Right child page (0 for leaf)
    
    // Key data
    uint8_t key_suffix[suffix_len];  // Unique key suffix bytes
    
    // TID list (leaf nodes only)
    TID tid_list[tid_count];    // 16 bytes per TID
};
```

**Node Size Calculation:**
```
Leaf node:  2 + 2 + 2 + 2 + 4 + suffix_len + (tid_count * 16)
Internal:   2 + 2 + 2 + 2 + 4 + suffix_len
```

### TID Layout (16 bytes)

```cpp
// Source: BTREE_SPEC.md appendix
struct TID {
    uint32_t page_id;           // Page containing row version
    uint16_t slot_id;           // Slot number in page
    uint16_t version_id;        // Version sequence number
    uint32_t table_uuid_hash;   // Hash of table UUID
    uint32_t reserved;          // Must be zero
};
```

## Algorithms

### Algorithm: Search

```
Input:  target_key, root_page_id
Output: List of matching TIDs

1. current_page = root_page_id
2. While true:
   a. Latch page 'current_page' in shared mode
   b. Read page header to determine if leaf
   
   c. If internal page:
      i.   Use jump table to find approximate position
      ii.  Binary search slot array to find exact position
      iii. Let K be first key > target_key (or any equal key)
      iv.  If K is first key on page:
           child = leftmost_child_page_id
      v.   Else:
           child = child_page_id of predecessor
      vi.  Unlatch current page
      vii. current_page = child
      
   d. If leaf page:
      i.   Binary search for target_key
      ii.  If found, collect all TIDs at matching keys
      iii. While right_sibling exists and high_key <= target:
           - Unlatch current page
           - current_page = right_sibling
           - Latch new page
           - Continue collecting matching TIDs
      iv.  Unlatch current page
      v.   Return collected TIDs
```

**Complexity:**
- Time: O(log n) where n = number of keys
- Space: O(1) auxiliary (stack depth limited by tree height)

### Algorithm: Key Reconstruction

```
Input:  Node at slot index 'i' on page
Output: Full key bytes

1. If prefix_len == 0:
       Return key_suffix
       
2. Else if i == 0 (first key on page):
       // Prefix refers to high key of left sibling
       // Must fetch left sibling page
       Return left_sibling.high_key[0:prefix_len] + key_suffix
       
3. Else:
       previous_key = reconstruct_key(i - 1)
       Return previous_key[0:prefix_len] + key_suffix
```

### Algorithm: Insert

```
Input:  key_bytes, TID
Output: Status

1. Traverse to leaf using latch coupling:
   a. Latch parent in shared mode
   b. Determine child page from search
   c. Latch child in exclusive mode
   d. Upgrade parent to exclusive if child will split
   e. Unlatch parent if no split needed
   
2. At leaf page:
   a. Compute required space:
      space_needed = node_size(prefix_len, suffix_len, tid_count + 1)
   
   b. If free_space >= space_needed:
      i.   Find insertion position via binary search
      ii.  Compute prefix_len with previous key
      iii. Write node record at free_offset
      iv.  Insert slot offset into slot array
      v.   Update node_count, free_offset
      vi.  Recompute prefix_len for following key
      vii. Return OK
      
   c. Else (need to split):
      i.   Allocate new right sibling page
      ii.  Call SPLIT algorithm
      iii. Insert separator into parent
      iv.  Retry insert into appropriate child
```

### Algorithm: Split

```
Input:  Full leaf page P
Output: New left (P) and right (P') pages

1. Compute target size: target = fillfactor * page_payload

2. Walk slot array in key order, summing node sizes:
   - Stop when cumulative size >= target
   - If boundary falls within duplicate keys, move to first duplicate
   - Ensure both pages get at least one entry

3. Let split_point = index of first key going to right page

4. Move keys [split_point .. node_count-1] to new page P'

5. Update sibling links:
   - P'.right_sibling = P.right_sibling
   - If P.right_sibling exists:
     (P.right_sibling).left_sibling = P'
   - P.right_sibling = P'
   - P'.left_sibling = P.page_id

6. Set high keys:
   - P.high_key = first key of P' (separator)
   - P'.high_key = old P.high_key

7. Update counts and offsets on both pages

8. Return separator key for parent insertion
```

### Algorithm: Merge/Rebalance

```
Input:  Page P with low utilization
Output: Rebalanced or merged pages

1. If P.right_sibling == 0 and P.left_sibling == 0:
   Return (only page, nothing to do)

2. Let S = right sibling of P (prefer right to avoid leftmost issues)

3. Latch S in exclusive mode

4. Compute combined_size = size(P.entries) + size(S.entries)

5. If combined_size fits in one page at fillfactor:
   // MERGE case
   a. Move all S entries to P
   b. P.right_sibling = S.right_sibling
   c. If S.right_sibling exists:
      (S.right_sibling).left_sibling = P.page_id
   d. Update P.high_key = old S.high_key
   e. Free page S
   f. Update parent to remove separator
   
6. Else:
   // REDISTRIBUTE case
   a. Move entries from S to P until P.utilization >= min_fill
   b. Update P.high_key to first key remaining in S
   c. Update parent separator to new P.high_key

7. Unlatch both pages
```

## Key Encoding

Keys are encoded to byte strings that preserve the original ordering:

```cpp
// Source: BTREE_SPEC.md appendix
struct KeySegment {
    uint8_t  null_flag;     // 0 = non-null, 1 = null
    uint8_t  type_tag;      // See type codes below
    uint16_t len;           // Length of value_bytes
    uint8_t  value_bytes[len];
    uint16_t collation_id;  // 0 if binary comparison
};
```

**Type Tags:**
| Tag | Type | Sort Order Preservation |
|-----|------|------------------------|
| 0x01 | BOOL | 0x00=false, 0x01=true |
| 0x02-0x05 | INT8/16/32/64 | Sign-biased big-endian |
| 0x06-0x09 | UINT8/16/32/64 | Big-endian |
| 0x0A-0x0B | FLOAT32/64 | IEEE 754 with sign flip |
| 0x0C | DECIMAL128 | Scaled integer, sign-biased |
| 0x0D-0x0F | DATE/TIME/TIMESTAMP | Microseconds since epoch |
| 0x10 | UUID | Network byte order |
| 0x11 | BINARY | Lexicographic bytes |
| 0x12 | STRING | UTF-8 with collation key |

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | Keys in slot array are sorted | Binary search assertion |
| I2 | First key on page has prefix_len=0 | Insert validation |
| I3 | All keys on page < high_key | Split validation |
| I4 | Sibling links form valid doubly-linked list | Consistency check |
| I5 | Leaf level is always 0 | Header validation |
| I6 | Internal nodes have at least 2 children | Split enforcement |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `SB_ERR_DUPLICATE_KEY` | Unique constraint violation | Return error to caller |
| `SB_ERR_CORRUPT_INDEX` | Checksum/node validation failure | Mark index invalid |
| `SB_ERR_PAGE_FULL` | Cannot split (root case) | Grow tree height |
| `SB_ERR_LOCK_TIMEOUT` | Latch contention | Retry or deadlock error |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_bitmap_index_gc.cpp` | Bitmap-specific GC, also validates B-tree integration |
| `test_hash_index.cpp` | Hash index, contrasts with B-tree behavior |
| `test_brin_index.cpp` | BRIN index, different page structure |
| `test_index_advisor.cpp` | Recommends B-tree vs other types |
| `test_index_page_base_layout_contract.cpp` | Page header validation |
| `test_index_page_walk_conformance.cpp` | Tree traversal correctness |

## Related Specifications

- [gin_index_format.md](./gin_index_format.md) - Inverted index for multi-value entries
- [index_dml_integration.md](./index_dml_integration.md) - How DML operations maintain indexes

## Glossary

| Term | Definition |
|------|------------|
| TID | Tuple ID - 16-byte pointer to row version |
| Slot Array | Sorted array of offsets to node records |
| Prefix Compression | Storing only unique suffix of keys |
| Latch Coupling | Holding parent latch while fetching child |
| Fillfactor | Target page utilization percentage |
| High Key | Upper bound (exclusive) for all keys on page |

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |

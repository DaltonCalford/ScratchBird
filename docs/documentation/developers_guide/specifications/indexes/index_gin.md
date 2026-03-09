# Specification: GIN Index (Generalized Inverted Index)

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/gin_index.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/gin_index.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/gin_compression.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_gin_index_gc.cpp:1`

## Synopsis

GIN (Generalized Inverted Index) supports efficient indexing of composite values where a single row produces multiple index entries. Used for arrays, full-text search, and JSONB. Features pending lists for fast updates and compressed posting lists.

## Scope

### In Scope

- Entry tree (B-tree of keys to posting lists)
- Posting list format and compression
- Posting tree for large posting lists
- Pending list for fast updates
- Pending list flush algorithm
- Opclass interface for key extraction

### Out of Scope

- B-tree implementation details (see index_btree.md)
- Full-text configuration (see index_fulltext.md)
- Array operator semantics (see SQL dialect specs)
- GiST/SP-GiST indexes

## Background

GIN indexes are "inverted" indexes that map from content values to row locations. Unlike B-trees where one row produces one index entry, GIN handles cases where one row produces many entries:
- Array columns: each element becomes an index entry
- Full-text search: each word becomes a posting
- JSONB: each key/path becomes an indexable value

The implementation uses a pending list for fast INSERT performance and a posting tree for large posting lists.

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:662
enum class IndexType : uint8_t {
    GIN = 4,          // Generalized Inverted Index
    // ... other types
};
```

### Page Types

| Page Type | Value | Description |
|-----------|-------|-------------|
| `PAGE_TYPE_GIN_META` | 0x20 | Metadata page |
| `PAGE_TYPE_GIN_ENTRY` | 0x21 | Entry tree (B-tree) page |
| `PAGE_TYPE_GIN_DATA` | 0x22 | Posting list/tree page |
| `PAGE_TYPE_GIN_PENDING` | 0x23 | Pending list page |

### Meta Page Layout (48 bytes)

```cpp
// Source: scratchbird/core/gin_index.h:89
struct GINMetaPage {
    PageHeader header;                // Standard header (80 bytes)
    uint32_t root_entry_page_id;      // Root of entry B-tree
    uint32_t pending_head_page_id;    // First pending list page
    uint32_t pending_tail_page_id;    // Last pending list page
    uint64_t pending_count;           // Number of pending entries
    uint32_t pending_limit;           // Threshold to trigger flush
    uint32_t posting_tree_threshold;  // Entries before converting to tree
    ID index_uuid;                    // Index UUID
};
```

### Entry Tree Structure

The entry tree is a B-tree mapping `key_bytes` to a posting list location:

```cpp
// Source: scratchbird/core/gin_index.h:105
struct GINEntry {
    uint8_t key_bytes[key_len];       // The index key
    uint8_t location_type;             // 0 = inline, 1 = separate page
    
    union {
        // Inline posting list (small number of entries)
        struct {
            uint32_t tid_count;
            TID tids[tid_count];
        } inline_list;
        
        // Pointer to external posting list/tree
        struct {
            uint32_t page_id;          // First page of posting list
            uint32_t byte_offset;      // Offset within page
        } external;
    };
};
```

### Posting List Format

Posting lists store sorted TIDs for a given key:

```cpp
// Source: scratchbird/core/gin_index.h:132
struct PostingList {
    uint32_t tid_count;                // Number of TIDs
    uint32_t compressed_len;           // Bytes of compressed data
    uint8_t compressed_tids[];         // Delta-encoded, varint compressed
};
```

**Compression Algorithm:**

```
Input:  Sorted TIDs by (page_id, slot_id, version_id)
Output: Compressed byte stream

1. For first TID:
   - Encode full page_id (LEB128 varint)
   - Encode full slot_id (LEB128 varint)
   - Encode full version_id (LEB128 varint)

2. For subsequent TIDs:
   - Encode delta(page_id) as LEB128 (often 0)
   - Encode delta(slot_id) as LEB128
   - Encode delta(version_id) as LEB128
   
3. When page_id changes, full page_id is encoded
```

**LEB128 Encoding:**
```cpp
// Variable-length encoding for unsigned integers
void write_leb128(uint64_t value, std::vector<uint8_t>& out) {
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        out.push_back(byte);
    } while (value != 0);
}

uint64_t read_leb128(const uint8_t*& ptr) {
    uint64_t result = 0;
    uint32_t shift = 0;
    uint8_t byte;
    do {
        byte = *ptr++;
        result |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    return result;
}
```

### Posting Tree

When a posting list exceeds `posting_tree_threshold` entries:

1. Convert posting list to B-tree of TIDs
2. Root page_id stored in entry tree
3. Leaf pages contain sorted TIDs directly (no compression)
4. Internal pages use TID as separator keys

```cpp
// Source: scratchbird/core/gin_index.h:154
struct PostingTreeHeader {
    uint32_t root_page_id;
    uint32_t leaf_page_count;
    uint64_t total_entries;
    float sparsity_ratio;           // dead_entries / total_entries
};
```

### Pending List Format

Pending list pages store unsorted `(key_bytes, TID)` pairs for fast insertion:

```
Pending Page Layout:
┌─────────────────────────────────┐
│ Next Page ID (4 bytes)          │ ← 0 if last page
├─────────────────────────────────┤
│ Entry 1: key_len (2)            │
│          key_bytes[key_len]     │
│          tid (16 bytes)         │
├─────────────────────────────────┤
│ Entry 2...                      │
├─────────────────────────────────┤
│ Free space                      │
└─────────────────────────────────┘
```

## Algorithms

### Algorithm: Insert with Pending List

```
Input:  row_value, TID, key_extractor function
Output: Status

1. Call key_extractor(row_value) to get list of keys

2. For each key in keys:
   a. Append to pending list tail page:
      - Write key_len (2 bytes)
      - Write key_bytes
      - Write TID (16 bytes)
   b. If tail page full:
      - Allocate new page
      - Link from current tail
      - Update tail pointer in meta
   c. Increment pending_count

3. If pending_count >= pending_limit:
   a. Trigger pending list flush (async or sync)

4. Return OK
```

**Complexity:**
- Time: O(k) where k = number of keys extracted (amortized O(1) per key)
- Space: O(k * avg_key_size) bytes added to pending list

### Algorithm: Pending List Flush

```
Input:  GIN index with pending entries
Output: Status, entries merged into entry tree

1. Read all pending pages into memory

2. Sort entries by (key_bytes, TID)

3. Group by key_bytes

4. For each key group:
   a. Search entry tree for key
   
   b. If key not exists:
      i.   Create new posting list from group TIDs
      ii.  Compress posting list
      iii. Insert into entry tree
      
   c. If key exists with inline posting:
      i.   Decompress existing posting list
      ii.  Merge with new TIDs (maintain sort)
      iii. If combined size < threshold:
           - Recompress and store inline
      iv.  Else:
           - Build posting tree
           - Update entry to point to tree
           
   d. If key exists with posting tree:
      i.   Insert new TIDs into posting tree
      ii.  Update sparsity ratio if needed

5. Free all pending pages

6. Reset pending_count, head/tail pointers

7. Return OK
```

### Algorithm: Search

```
Input:  query_value, consistent function, key_extractor
Output: List of matching TIDs

1. Call key_extractor(query_value) to get query keys

2. If pending entries exist:
   a. Scan pending list for matching keys
   b. Collect candidate TIDs

3. For each query key:
   a. Search entry tree for key
   b. If found:
      i.   If inline posting: decompress all TIDs
      ii.  If posting tree: traverse and collect all TIDs
   c. Add TIDs to result set

4. Apply consistent function to combine results:
   - For array @> operator: all keys must match (AND)
   - For array && operator: any key matches (OR)
   - For full-text: depends on tsquery

5. Verify row visibility using MGA rules

6. Return visible TIDs
```

### Algorithm: Delete/GC

```
Input:  List of dead TIDs
Output: Number of entries removed

1. For each dead_tid:
   a. Scan pending list (if any):
      - Remove entries matching dead_tid
      - Decrement pending_count
      
   b. For each entry in entry tree:
      i.   If inline posting: linear scan and remove
      ii.  If posting tree:
           - Mark TID as dead (tombstone) OR
           - Physically remove and compact
           - Update sparsity_ratio

2. After processing all dead_tids:
   
   a. For posting trees with sparsity_ratio > threshold:
      i.   Scan all leaf pages
      ii.  Copy live TIDs to new tree
      iii. Swap root pointers
      iv.  Free old pages after OIT advances

3. Return total entries removed
```

## Opclass Interface

GIN opclasses provide three functions:

```cpp
// Source: scratchbird/core/gin_index.h:245
class GINOpClass {
public:
    // Extract indexable keys from value
    // Returns: vector of key byte sequences
    virtual std::vector<std::vector<uint8_t>> extractValue(
        const void* data, size_t len) = 0;
    
    // Extract query keys from query operand
    virtual std::vector<std::vector<uint8_t>> extractQuery(
        const void* data, size_t len) = 0;
    
    // Check if stored posting satisfies query
    // strategy: comparison strategy number
    // query_keys: from extractQuery
    // n_keys: number of keys in query
    // Returns: true if this posting should be returned
    virtual bool consistent(
        uint32_t strategy,
        const std::vector<uint8_t>& query_keys,
        uint32_t n_keys,
        const void* extra_data) = 0;
};
```

**Supported Opclasses:**
| Opclass | Data Type | Strategies |
|---------|-----------|------------|
| `gin_array_ops` | Arrays | @>, <@, && |
| `gin_tsvector_ops` | TsVector | @@ |
| `gin_jsonb_ops` | JSONB | @>, @?, path ops |

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | TIDs in posting lists are sorted | Insert sort check |
| I2 | Pending list entries are unsorted | Design constraint |
| I3 | Entry tree keys are unique | B-tree property |
| I4 | Posting tree sparsity < threshold | GC trigger check |
| I5 | Pending count matches actual entries | Consistency check |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `gin.pending_list_limit` | 4096 | Entries before forced flush |
| `gin.fastupdate` | true | Use pending list (vs direct insert) |
| `gin.posting_tree_threshold` | 256 | TIDs before tree conversion |
| `gin.posting_tree_rebuild_threshold` | 0.40 | Sparsity requiring rebuild |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `SB_ERR_GIN_OPCLASS` | Unsupported opclass | Reject at CREATE INDEX |
| `SB_ERR_PENDING_OVERFLOW` | Pending list too large | Force flush |
| `SB_ERR_CORRUPT_POSTING` | Posting decode failure | Mark index invalid |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_gin_index_gc.cpp` | Pending list GC, multi-key removal |
| `test_gin_array_ops.cpp` | Array operator support |
| `test_inverted_index_basic.cpp` | Basic inverted index operations |

## Related Specifications

- [btree_index_format.md](./btree_index_format.md) - Entry tree implementation
- [index_dml_integration.md](./index_dml_integration.md) - DML maintenance
- [index_fulltext.md](./index_fulltext.md) - Full-text search on GIN

## Glossary

| Term | Definition |
|------|------------|
| Posting | A (key, TID) pair in the inverted index |
| Posting List | Compressed list of TIDs for one key |
| Entry Tree | B-tree mapping keys to posting lists |
| Pending List | Unsorted buffer for fast inserts |
| Opclass | Operator class defining key extraction |
| LEB128 | Little-endian base-128 variable encoding |

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification |

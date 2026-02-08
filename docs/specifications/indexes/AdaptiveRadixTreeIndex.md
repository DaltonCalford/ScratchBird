# Adaptive Radix Tree (ART) Index Specification for ScratchBird


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**Version:** 1.0
**Date:** January 22, 2026
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Beta (Core Index Type)
**Features:** In-memory optimized, cache-aware, MGA compliant

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Decision](#architecture-decision)
3. [Data Model](#data-model)
4. [On-Disk Structures](#on-disk-structures)
5. [Page Size Considerations](#page-size-considerations)
6. [MGA Compliance](#mga-compliance)
7. [Core API](#core-api)
8. [DML Integration](#dml-integration)
9. [Garbage Collection](#garbage-collection)
10. [Query Planner Integration](#query-planner-integration)
11. [DDL and Catalog](#ddl-and-catalog)
12. [Implementation Steps](#implementation-steps)
13. [Testing Requirements](#testing-requirements)
14. [Performance Targets](#performance-targets)
15. [Future Enhancements](#future-enhancements)

---

## Overview

### Purpose

Adaptive Radix Tree (ART) is a cache-efficient trie-based index that adapts node size to key distribution. It is highly efficient for in-memory workloads and supports prefix and range queries.

### Primary Use Cases

- High-performance primary key lookup
- Prefix search on strings
- Memory-resident index for hot data

---

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Core Data Structures

1. **Node header**  
   - `type`: one of `NODE4`, `NODE16`, `NODE48`, `NODE256`.  
   - `prefix_len`: number of compressed bytes (0..MAX_PREFIX).  
   - `prefix[MAX_PREFIX]`: first `min(prefix_len, MAX_PREFIX)` bytes.  
   - `child_count`: number of live children (0..type capacity).  
   - `version_chain`: optional node versioning for concurrent writers (RCU-style).  
2. **Node4/16**  
   - `keys[cap]`: sorted array of child key bytes.  
   - `children[cap]`: child pointers (either node or leaf).  
3. **Node48**  
   - `child_index[256]`: 1-byte index into `children[]`, value 0xFF means empty.  
   - `children[48]`: child pointers.  
4. **Node256**  
   - `children[256]`: direct array (NULL if absent).  
5. **Leaf**  
   - `key_ptr`: pointer to full, binary-comparable key bytes.  
   - `key_len`: length.  
   - `entries[]`: array of `SBIndexEntryMeta` (one per record version).  

### Key Normalization (Binary Comparable)

1. For fixed-width numeric types, encode to big-endian with sign-bit flip for signed types.  
2. For variable-length strings, use raw bytes + terminator `0x00` to ensure prefix ordering.  
3. For composite keys, concatenate normalized field encodings with length-prefix where needed.  
4. Record normalized byte sequence in the leaf; internal nodes only store prefixes and next-byte keys.

### Search (Point Lookup)

1. Normalize key to byte sequence `K`.  
2. Start at root node `N`.  
3. While `N` is not NULL:  
   1. Compare `N.prefix` with `K[pos..]` for `min(prefix_len, MAX_PREFIX)` bytes.  
   2. If mismatch, return NOT FOUND.  
   3. Advance `pos` by `prefix_len`.  
   4. If `pos == len(K)`:
      - If a leaf is stored at this node, compare full key bytes to disambiguate; return match.  
      - Else NOT FOUND.  
   5. Use byte `K[pos]` to select child from `N` (linear/binary search in Node4/16, index map in Node48, direct in Node256).  
   6. If child is leaf: compare full key bytes; return match if equal.  
   7. Else set `N = child` and continue.

### Insert (Upsert)

1. Normalize key `K` and locate insertion point using search steps.  
2. If existing leaf matches, append a new `SBIndexEntryMeta` for the new record version.  
3. If search hits NULL child:  
   - Insert a new leaf into current node; if node full, grow (Node4→16→48→256).  
4. If search hits leaf with different key:  
   1. Find longest common prefix between existing leaf key and `K` from `pos`.  
   2. Create a new node `Nnew` with `prefix = common_prefix`, `prefix_len = lcp`.  
   3. Add existing leaf under `Nnew` using next differing byte.  
   4. Add new leaf under `Nnew` using next differing byte.  
   5. Replace parent child pointer with `Nnew`.  
5. If search hits node but prefix mismatch:  
   1. Split node: create `Nnew` with `prefix = common_prefix`.  
   2. Adjust existing node’s prefix by removing common prefix + 1 byte.  
   3. Insert existing node and new leaf as children of `Nnew`.  
   4. Replace parent child pointer with `Nnew`.

### Delete

1. Search for leaf; if not found, return.  
2. Remove leaf from its parent node.  
3. If parent `child_count` falls below shrink threshold:  
   - Node256→48→16→4 (rebuild child arrays).  
4. If parent has only one child left and is not root:  
   - Merge parent prefix with child prefix (path compression), replace parent with child.  
5. Track deleted leaf and node versions for MGA GC (tombstones).

### Range / Prefix Scan

1. Normalize prefix key `P`.  
2. Descend to the node matching `P` (like search, allowing mid-node match).  
3. Perform DFS in lexicographic order by child key byte.  
4. Emit leaves in order; compare full key bytes for final ordering stability.

### Concurrency + MGA

1. Writers publish new node versions atomically (RCU-style).  
2. Readers traverse a consistent prefix path without blocking.  
3. Leaf entries are filtered by record visibility (`sb_find_visible_version`).  
4. Old node/leaf versions are reclaimed by index GC when safe.  

### Complexity Targets

- Search/insert/delete: `O(k)` where `k` is key length in bytes.  
- Range scan: `O(k + output)` plus traversal overhead.

### References (for algorithmic definitions)

- Leis, Kemper, Neumann, “The Adaptive Radix Tree: ARTful indexing for main-memory databases,” ICDE 2013.  

---

## Architecture Decision

### Design Choice

Implement ART as a **memory-first index** with periodic persistence:

- In-memory ART nodes for speed
- Snapshot pages for persistence
- Optional write-after log (WAL) for durability (optional extension)

---

## Data Model

### Node Types

- Node4, Node16, Node48, Node256 (adaptive child fan-out)
- Each node stores a partial key prefix to compress paths

### Key Support

- Variable-length keys (VARCHAR, VARBINARY)
- Fixed-length keys (INT, UUID, DECIMAL)

---

## On-Disk Structures

### Meta Page


**Logical Fields:**

- `art_header` (PageHeader)
- `art_index_uuid[16]` (uint8_t)
- `art_table_uuid[16]` (uint8_t)
- `art_column_id` (uint16_t)
- `art_key_len` (uint16_t): 0 for variable
- `art_root_page` (uint32_t): snapshot root
- `art_snapshot_epoch` (uint32_t)
- `art_total_keys` (uint64_t)
- `art_total_nodes` (uint64_t)
- `art_padding[]` (uint8_t)


### Snapshot Node Layout


**Logical Fields:**

- `type` (uint8_t): SBARTNodeType
- `prefix_len` (uint8_t): bytes
- `child_count` (uint16_t)
- `reserved` (uint32_t)
- `prefix[8]` (uint8_t): inline prefix
- `hdr` (SBARTNodeHeader)
- `keys[4]` (uint8_t)
- `children[4]` (uint32_t)
- `hdr` (SBARTNodeHeader)
- `keys[16]` (uint8_t)
- `children[16]` (uint32_t)
- `hdr` (SBARTNodeHeader)
- `child_index[256]` (uint8_t)
- `children[48]` (uint32_t)
- `hdr` (SBARTNodeHeader)
- `children[256]` (uint32_t)


### Leaf Representation

Leaf stores key suffix and `SBIndexEntryMeta` entries. Variable-length keys store a pointer to an overflow area.

---

## Page Size Considerations

- Nodes are compact and cache-friendly
- Snapshot pages should be aligned to 8K+ for efficient scan
- Node48 and Node256 pages can be grouped for better locality

---

## MGA Compliance

- ART leaves store `SBIndexEntryMeta` for **record versions**, not tuple headers.  
- Visibility is determined by record header + TIP (`sb_find_visible_version`).  
- Updates to indexed keys create **new entries**; old entries remain until sweep.  
- Snapshot rebuild uses TIP state and MGA rules, not PostgreSQL snapshots.  
 - Leaf entries must use `record_uuid` with optional `SBRecordPtr` cache hints.  

---

## Core API

```cpp
Status art_insert(UUID index_uuid, const void* key, size_t key_len,
                  const SBIndexEntryMeta& meta);
Status art_delete(UUID index_uuid, const void* key, size_t key_len,
                  const UUID& record_uuid);
ARTResult art_lookup(UUID index_uuid, const void* key, size_t key_len);
ARTIterator art_prefix_scan(UUID index_uuid, const void* prefix, size_t prefix_len);
```

---

## DML Integration

- INSERT: insert key + `SBIndexEntryMeta` for the new record version
- UPDATE: if key changes, insert new entry; old entry remains until sweep
- DELETE: create `RHD_DELETED` record version; index entry remains until sweep

---

## Garbage Collection

ART implements `IndexGCInterface` and removes dead record versions by scanning leaf
entries. Each leaf can store multiple `SBIndexEntryMeta` entries per key.

GC behavior:

- Build a hash set of dead `record_uuid`s from heap sweep batches.
- Walk all leaves and remove any entry with a dead `record_uuid`.
- If a leaf has no remaining entries, remove the leaf and prune the parent path.
- If node fanout drops below the lower threshold, shrink node type
  (Node256 -> Node48 -> Node16 -> Node4).

Concurrency:

- Readers use shared locks; GC uses write locks on modified nodes only.
- GC never moves live entries; it only removes dead references.

Persistence:

- After GC, emit a new snapshot root and increment `art_snapshot_epoch`.
- Old snapshot pages remain valid until the next safe reclaim boundary.

See `INDEX_GC_PROTOCOL.md` for contract requirements.

---

## Query Planner Integration

- Equality and prefix predicates prefer ART when in-memory
- Range scans can be performed via ART iterator

---

## DDL and Catalog

### Syntax

```sql
CREATE INDEX idx_users_art
ON users(username)
USING art
WITH (prefix_compression = true, snapshot_interval = 300);
```

### Catalog Additions

- `sys.indexes.index_type = 'ART'`
- `sys.indexes.index_options` stores snapshot policy and key length

---

## Implementation Steps

1. Implement in-memory ART node operations
2. Add snapshot persistence to pages
3. Add background snapshot rebuild
4. Add planner integration and catalog persistence

---

## Testing Requirements

- Correctness for variable-length and fixed-length keys
- Prefix scan accuracy
- Snapshot rebuild consistency
- MGA visibility correctness

---

## Performance Targets

- 2-5x faster than B-tree for in-memory lookups
- Prefix scans < 1 ms for 10k results

---

## Future Enhancements

- Hot/cold tiering between ART and B-tree
- Adaptive memory pressure eviction
- Optional write-after log for faster recovery

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.

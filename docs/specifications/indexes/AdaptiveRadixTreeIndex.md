# Adaptive Radix Tree (ART) Index Specification for ScratchBird

**Version:** 1.0
**Date:** January 22, 2026
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Beta (Optional Index Type)
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

## Architecture Decision

### Design Choice

Implement ART as a **memory-first index** with periodic persistence:

- In-memory ART nodes for speed
- Snapshot pages for persistence
- Optional write-after log (WAL) for durability (post-gold)

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

```cpp
#pragma pack(push, 1)

struct SBARTIndexMetaPage {
    PageHeader art_header;

    uint8_t art_index_uuid[16];
    uint8_t art_table_uuid[16];

    uint16_t art_column_id;
    uint16_t art_key_len;          // 0 for variable

    uint32_t art_root_page;        // snapshot root
    uint32_t art_snapshot_epoch;

    uint64_t art_total_keys;
    uint64_t art_total_nodes;

    uint8_t art_padding[];
} __attribute__((packed));

#pragma pack(pop)
```

### Snapshot Node Layout

```cpp
#pragma pack(push, 1)

enum SBARTNodeType : uint8_t {
    ART_NODE4 = 4,
    ART_NODE16 = 16,
    ART_NODE48 = 48,
    ART_NODE256 = 256
};

struct SBARTNodeHeader {
    uint8_t type;          // SBARTNodeType
    uint8_t prefix_len;    // bytes
    uint16_t child_count;
    uint32_t reserved;
    uint8_t prefix[8];     // inline prefix
};

struct SBARTNode4 {
    SBARTNodeHeader hdr;
    uint8_t keys[4];
    uint32_t children[4];
};

struct SBARTNode16 {
    SBARTNodeHeader hdr;
    uint8_t keys[16];
    uint32_t children[16];
};

struct SBARTNode48 {
    SBARTNodeHeader hdr;
    uint8_t child_index[256];
    uint32_t children[48];
};

struct SBARTNode256 {
    SBARTNodeHeader hdr;
    uint32_t children[256];
};

#pragma pack(pop)
```

### Leaf Representation

Leaf stores key suffix and TID. Variable-length keys store a pointer to an overflow area.

---

## Page Size Considerations

- Nodes are compact and cache-friendly
- Snapshot pages should be aligned to 8K+ for efficient scan
- Node48 and Node256 pages can be grouped for better locality

---

## MGA Compliance

- ART nodes store TID pointers; visibility checks are applied at lookup
- Updates create new leaf entries or versioned leaf chains
- Snapshot rebuild uses consistent transaction snapshot

---

## Core API

```cpp
Status art_insert(UUID index_uuid, const void* key, size_t key_len, TID tid);
Status art_delete(UUID index_uuid, const void* key, size_t key_len, TID tid);
ARTResult art_lookup(UUID index_uuid, const void* key, size_t key_len);
ARTIterator art_prefix_scan(UUID index_uuid, const void* prefix, size_t prefix_len);
```

---

## DML Integration

- INSERT: insert key and TID
- UPDATE: delete old key and insert new key if changed
- DELETE: mark or remove leaf (MGA rules)

---

## Garbage Collection

ART implements `IndexGCInterface` and removes dead TIDs by scanning leaf
entries. Each leaf can store multiple TIDs per key (duplicates or
multi-version rows).

TID references use `TID { gpid, slot }`. Legacy packed TID encodings are
not permitted in v2 on-disk formats.

GC behavior:

- Build a hash set of dead TIDs from the heap sweep.
- Walk all leaves and remove any TID in the dead set.
- If a leaf has no remaining TIDs, remove the leaf and prune the parent path.
- If node fanout drops below the lower threshold, shrink node type
  (Node256 -> Node48 -> Node16 -> Node4).

Concurrency:

- Readers use shared locks; GC uses write locks on modified nodes only.
- GC never moves live TIDs; it only removes dead references.

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

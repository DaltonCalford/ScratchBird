# Suffix Tree / Suffix Array Index Specification for ScratchBird

**Version:** 1.0
**Date:** January 22, 2026
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Beta (Optional Index Type)
**Features:** Page-size agnostic, MGA compliant, substring search

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

Suffix indexes support fast substring search (e.g., `LIKE '%pattern%'`) by indexing all suffixes of a text column. A suffix array is more space-efficient than a suffix tree and is the default implementation choice.

### Primary Use Cases

- Substring search in large text columns
- Bioinformatics or log pattern matching
- Complex pattern queries where full-text search is insufficient

---

## Architecture Decision

### Design Choice

Implement **suffix arrays** as the primary structure, with an optional suffix tree mode for in-memory deployments.

- Suffix array stores sorted suffix references
- LCP (longest common prefix) array speeds pattern search
- Segment-based, immutable suffix arrays for MGA compliance

---

## Data Model

### Suffix Entry

Each suffix references a document row and an offset within that row.

```
SuffixRef {
    TID tid;
    uint32 offset;     // byte offset in UTF-8
}
```

### Search

Substring search uses binary search on the suffix array with LCP acceleration.

---

## On-Disk Structures

### Meta Page

```cpp
#pragma pack(push, 1)

struct SBSuffixIndexMetaPage {
    PageHeader sx_header;

    uint8_t sx_index_uuid[16];
    uint8_t sx_table_uuid[16];

    uint16_t sx_column_id;         // Indexed column
    uint16_t sx_segment_count;     // Number of segments

    uint32_t sx_root_page;         // Root segment page
    uint32_t sx_reserved1;

    uint64_t sx_total_suffixes;
    uint64_t sx_total_docs;

    uint8_t sx_padding[];
} __attribute__((packed));

#pragma pack(pop)
```

### Suffix Array Page

```cpp
#pragma pack(push, 1)

struct SBSuffixArrayEntry {
    TID tid;
    uint32 offset;
    uint32 reserved;
};

struct SBSuffixArrayPage {
    PageHeader sa_header;
    uint32 sa_next_page;
    uint32 sa_count;
    SBSuffixArrayEntry sa_entries[];
} __attribute__((packed));

#pragma pack(pop)
```

### LCP Page

```cpp
#pragma pack(push, 1)

struct SBLCPPage {
    PageHeader lcp_header;
    uint32 lcp_next_page;
    uint32 lcp_count;
    uint16 lcp_values[];  // LCP lengths per suffix entry
} __attribute__((packed));

#pragma pack(pop)
```

---

## Page Size Considerations

- Suffix array entry size: 16 bytes
- 8K pages fit about 500 entries
- LCP values are 2 bytes each (up to 64K prefix length)

---

## MGA Compliance

- Segment immutability provides version safety
- Updates create new segments; old segments are swept
- Deletes are tracked via visibility checks on TID

---

## Core API

```cpp
Status suffix_build_segment(UUID index_uuid, SuffixBuilder* builder);
SuffixMatch suffix_search(UUID index_uuid, const char* pattern);
SuffixIterator suffix_scan(UUID index_uuid, const char* pattern);
```

---

## DML Integration

- INSERT: add text to current segment builder
- UPDATE: add new suffixes; old suffixes remain until sweep
- DELETE: mark TID deleted; suffix entries filtered by visibility

---

## Garbage Collection

Suffix segments are immutable. GC is performed during **segment merge**:

- `removeDeadEntries()` filters suffix entries whose TID is dead.
- If a suffix entry list becomes empty, it is removed from the merged segment.
- LCP arrays are rebuilt during merge to remain consistent.

TID storage must use `TID` (GPID + slot). Legacy packed TIDs are not
permitted in v2 on-disk formats.

See `INDEX_GC_PROTOCOL.md` for the GC contract.

---

## Query Planner Integration

- `LIKE '%pattern%'` and `CONTAINS` map to suffix index
- Planner chooses suffix index for large text columns when selectivity is high

---

## DDL and Catalog

### Syntax

```sql
CREATE INDEX idx_body_suffix
ON docs(body)
USING suffix_array
WITH (max_doc_len = 1048576, min_pattern_len = 3);
```

### Catalog Additions

- `sys.indexes.index_type = 'SUFFIX_ARRAY'` or `'SUFFIX_TREE'`
- `sys.indexes.index_options` stores mode and limits

---

## Implementation Steps

1. Implement suffix array builder (SA-IS or prefix-doubling)
2. Build LCP arrays and storage
3. Add binary search with LCP acceleration
4. Wire planner rules for substring predicates
5. Add segment merge and sweep

---

## Testing Requirements

- Suffix array correctness for small and large inputs
- Binary search results vs brute force substring search
- MGA visibility with updates and deletes

---

## Performance Targets

- Substring search 10-50x faster than LIKE scan
- Build rate: 50-100MB/sec per core

---

## Future Enhancements

- Compressed suffix array (CSA) for smaller footprint
- Optional suffix tree for in-memory deployments
- GPU-accelerated search for very large text

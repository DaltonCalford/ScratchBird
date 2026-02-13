# Suffix Tree / Suffix Array Index Specification for ScratchBird


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

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Suffix Array Build (Doubling Method)

1. Concatenate document text with unique separators per document.  
2. Initialize rank array by first character.  
3. For k = 1, 2, 4, ...:
   - Sort suffixes by pair `(rank[i], rank[i+k])`.  
   - Assign new ranks.  
4. Stop when ranks are unique.  

### LCP Array (Kasai)

1. Build inverse suffix array `pos`.  
2. For each i in text order, compute LCP to previous suffix in SA.  
3. Store LCP values for accelerated search.  

### Query (Substring Search)

1. Binary search on suffix array for pattern using lexicographic compare.  
2. Use LCP to skip comparisons when possible.  
3. Return all suffixes in the matching range; map to record UUIDs.  

### Updates / Deletes

1. Suffix arrays are immutable; inserts go to **delta segment**.  
2. Periodically rebuild and merge base + delta.  
3. Deletes emit tombstones in delta; applied during rebuild.  

### MGA / Versioning

- Base segments are immutable per epoch.  
- Delta segments carry version metadata.  

### Complexity Targets

- Build: `O(n log n)` (doubling).  
- Query: `O(m log n)` where `m` is pattern length.  

### References (for algorithmic definitions)

- Ukkonen, “On‑line construction of suffix trees,” 1995 (suffix trees).  
- Suffix array algorithm references (e.g., Wikipedia/standard texts).  

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
    UUID record_uuid;
    uint32 offset;     // byte offset in UTF-8
}
```

### Search

Substring search uses binary search on the suffix array with LCP acceleration.

---

## On-Disk Structures

### Meta Page


**Logical Fields:**

- `sx_header` (PageHeader)
- `sx_index_uuid[16]` (uint8_t)
- `sx_table_uuid[16]` (uint8_t)
- `sx_column_id` (uint16_t): Indexed column
- `sx_segment_count` (uint16_t): Number of segments
- `sx_root_page` (uint32_t): Root segment page
- `sx_reserved1` (uint32_t)
- `sx_total_suffixes` (uint64_t)
- `sx_total_docs` (uint64_t)
- `sx_padding[]` (uint8_t)


### Suffix Array Page


**Logical Fields:**

- `record_uuid` (UUID)
- `offset` (uint32)
- `reserved` (uint32)
- `sa_header` (PageHeader)
- `sa_next_page` (uint32)
- `sa_count` (uint32)
- `sa_entries[]` (SBSuffixArrayEntry)


### LCP Page


**Logical Fields:**

- `lcp_header` (PageHeader)
- `lcp_next_page` (uint32)
- `lcp_count` (uint32)
- `lcp_values[]` (uint16): LCP lengths per suffix entry


---

## Page Size Considerations

- Suffix array entry size: 16 bytes
- 8K pages fit about 500 entries
- LCP values are 2 bytes each (up to 64K prefix length)

---

## MGA Compliance

- Segment immutability provides version safety
- Updates create new segments; old segments are swept
- Deletes are tracked via visibility checks on record versions

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
- DELETE: create deleted record version; suffix entries filtered by visibility

---

## Garbage Collection

Suffix segments are immutable. GC is performed during **segment merge**:

- `removeDeadEntries()` filters suffix entries whose `record_uuid` is dead.
- If a suffix entry list becomes empty, it is removed from the merged segment.
- LCP arrays are rebuilt during merge to remain consistent.

Record locators must use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are not
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

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.

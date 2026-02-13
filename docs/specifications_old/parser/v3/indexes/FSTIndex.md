# FST (Finite State Transducer) Index Specification for ScratchBird


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


Status: Authoritative (V3)
Last Updated: 2026-02-08
**Features:** Page-size agnostic, MGA compliant, segment-based

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
15. [Reserved Enhancements (Not Supported in V3)](#reserved-enhancements-not-supported-in-v3)

---

## Overview

### Purpose

FST (Finite State Transducer) indexes provide compact dictionaries with fast prefix, exact, and fuzzy lookup. They are commonly used for autocomplete, prefix search, and term dictionaries for full-text search.

### Primary Use Cases

- `LIKE 'prefix%'` and `STARTS WITH` acceleration
- Autocomplete and typeahead suggestions
- Term dictionary for inverted index segments

---

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Build (Incremental, Sorted Terms)

1. Collect all unique terms in sorted order.  
2. Insert terms into a minimal FST using incremental construction:
   - Compare current term with previous term to find common prefix.  
   - Minimize suffixes of the previous term (merge equivalent states).  
3. Store arcs in sorted label order to enable binary search during traversal.  

### Lookup (Exact)

1. Start at root state.  
2. For each byte/char in term:
   - Follow arc with matching label.  
   - If missing, return NOT FOUND.  
3. If final state is accepting, return its output payload.  

### Prefix Search

1. Traverse FST to prefix node.  
2. DFS/BFS from prefix node, emitting all accepted terms.  
3. Use output payloads to fetch postings or row lists.  

### Updates / Deletes

1. FST segments are immutable.  
2. Inserts go to a **delta segment** (small FST).  
3. Periodically merge segments into a new FST.  

### MGA / Versioning

- FST segments are immutable per epoch; delta segments carry version metadata.  

### Complexity Targets

- Lookup: `O(m)` where `m` is term length.  
- Build: `O(total_terms)` for incremental minimal FST on sorted input.  

### References (for algorithmic definitions)

- Daciuk et al., “Incremental Construction of Minimal Acyclic Finite-State Automata,” 2000.  
- Lucene FST implementation notes (sorted terms, minimal FST).  

---

## Architecture Decision

### Design Choice

Implement an immutable, segment-based FST index:

- Each segment contains a minimal FST for its terms
- Segments are merged in the background (similar to inverted index)
- The FST output points to postings lists or row lists

### Tokenization

For text columns, normalization uses the same analyzer pipeline as the inverted index (lowercase, stopword, stemming where configured). For simple VARCHAR columns, raw tokens are indexed.

---

## Data Model

### Term Mapping

Each term maps to an output payload:

- `term -> doclist pointer` (for prefix search)
- `term -> posting list pointer` (if used as dictionary for inverted index)

### Arc Encoding

Arcs are stored in sorted order by label. Nodes can be compacted with shared suffixes.

---

## On-Disk Structures

**Storage Layout Authority:** On-disk page headers, slot arrays, free-space rules, and page-type layouts are authoritative in `../storage/PAGE_TYPES_AND_LAYOUTS.md`. Any structs here are logical field groupings; do not infer byte-accurate layout from this file.

### Meta Page


**Logical Fields:**

- `fst_header` (PageHeader)
- `fst_index_uuid[16]` (uint8_t)
- `fst_table_uuid[16]` (uint8_t)
- `fst_column_id` (uint16_t): Indexed column
- `fst_segment_count` (uint16_t): Number of FST segments
- `fst_root_page` (uint32_t): Root of current segment
- `fst_terms_count` (uint32_t): Total terms
- `fst_total_lookups` (uint64_t)
- `fst_total_prefix_scans` (uint64_t)
- `fst_reserved1` (uint32_t)
- `fst_reserved2` (uint32_t)
- `fst_padding[]` (uint8_t)


### FST Node and Arc


**Logical Fields:**

- `label` (uint8_t): UTF-8 byte
- `flags` (uint8_t): FINAL, LAST, HAS_OUTPUT
- `output_len` (uint16_t): Output length (bytes)
- `target_node` (uint32_t): Node ID or page offset
- `node_id` (uint32_t)
- `arc_count` (uint16_t)
- `reserved` (uint16_t)


### Output Payload

For a prefix index, each term output references a posting list page or a row list:

```
FSTOutput {
    uint32 list_page_id;
    uint32 list_offset;
}
```

---

## Page Size Considerations

- FST nodes are compact and fit well in 8K pages
- Arc payloads are variable length; use prefix compression for outputs
- Segment files should target 16-64MB for good cache behavior

---

## MGA Compliance

- Segment immutability supports MGA visibility
- New terms create a new segment with transaction visibility metadata
- Old segments are swept when no longer visible

---

## Core API

```cpp
Status fst_insert_term(UUID index_uuid, const char* term, const FSTOutput* out);
Status fst_build_segment(UUID index_uuid, FSTBuilder* builder);
FSTResult fst_lookup(UUID index_uuid, const char* term);
FSTIterator fst_prefix_scan(UUID index_uuid, const char* prefix);
```

---

## DML Integration

- INSERT: tokenize value and append to current segment builder
- UPDATE: delete old terms, add new terms
- DELETE: mark term occurrences as deleted in postings

---

## Garbage Collection

FST segments are immutable. GC is implemented via **segment merge**:

- `removeDeadEntries()` does not edit existing FST nodes in place.
- During merge, posting lists are filtered against dead `record_uuid`s.
- Terms whose posting lists become empty are dropped from the merged FST.
- The merged FST replaces older segments atomically.

For prefix-only FSTs (term -> row list), GC removes dead `record_uuid`s from row lists
and drops empty term entries during merge.

See `INDEX_GC_PROTOCOL.md` for the GC contract.

## Record Identity Requirements

Posting lists must store `record_uuid` with optional `SBRecordPtr` cache hints.
Legacy TID encodings are not permitted.

---

## Query Planner Integration

- Prefix predicates map to FST prefix scan
- Exact match predicates can use FST lookup
- FST can be chosen as a dictionary for inverted index terms

---

## DDL and Catalog

### Syntax

```sql
CREATE INDEX idx_title_fst
ON docs(title)
USING fst
WITH (analyzer = 'standard', min_term_len = 2, max_terms = 1000000);
```

### Catalog Additions

- `sys.indexes.index_type = 'FST'`
- `sys.indexes.index_options` stores analyzer and term limits

---

## Implementation Steps

1. Implement FST builder (sorted term insertion)
2. Add segment writer and reader
3. Wire prefix scan operator
4. Add analyzer integration and catalog options
5. Implement segment merge

---

## Testing Requirements

- Exact lookup and prefix scan correctness
- Segment merge preserves results
- MGA visibility across segments
- Analyzer tokenization compatibility

---

## Performance Targets

- 5M term lookups/sec in-memory
- Prefix scan latency under 2 ms for 10k results
- 5-10x reduction in dictionary size vs hash table

---

## Reserved Enhancements (Not Supported in V3)

- Fuzzy search via Levenshtein automata
- Weighted outputs for suggestion ranking
- Shared FST dictionary across indexes

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.

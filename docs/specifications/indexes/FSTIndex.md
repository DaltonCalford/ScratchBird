# FST (Finite State Transducer) Index Specification for ScratchBird

**Version:** 1.0
**Date:** January 22, 2026
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Beta (Optional Index Type)
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
15. [Future Enhancements](#future-enhancements)

---

## Overview

### Purpose

FST (Finite State Transducer) indexes provide compact dictionaries with fast prefix, exact, and fuzzy lookup. They are commonly used for autocomplete, prefix search, and term dictionaries for full-text search.

### Primary Use Cases

- `LIKE 'prefix%'` and `STARTS WITH` acceleration
- Autocomplete and typeahead suggestions
- Term dictionary for inverted index segments

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

### Meta Page

```cpp
#pragma pack(push, 1)

struct SBFSTIndexMetaPage {
    PageHeader fst_header;

    uint8_t fst_index_uuid[16];
    uint8_t fst_table_uuid[16];

    uint16_t fst_column_id;        // Indexed column
    uint16_t fst_segment_count;    // Number of FST segments

    uint32_t fst_root_page;        // Root of current segment
    uint32_t fst_terms_count;      // Total terms

    uint64_t fst_total_lookups;
    uint64_t fst_total_prefix_scans;

    uint32_t fst_reserved1;
    uint32_t fst_reserved2;

    uint8_t fst_padding[];
} __attribute__((packed));

#pragma pack(pop)
```

### FST Node and Arc

```cpp
#pragma pack(push, 1)

struct SBFSTArc {
    uint8_t label;                 // UTF-8 byte
    uint8_t flags;                 // FINAL, LAST, HAS_OUTPUT
    uint16_t output_len;           // Output length (bytes)
    uint32_t target_node;          // Node ID or page offset
    // output bytes follow (varlen)
};

struct SBFSTNode {
    uint32_t node_id;
    uint16_t arc_count;
    uint16_t reserved;
    // arc list follows
};

#pragma pack(pop)
```

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
- During merge, posting lists are filtered against dead TIDs.
- Terms whose posting lists become empty are dropped from the merged FST.
- The merged FST replaces older segments atomically.

For prefix-only FSTs (term -> row list), GC removes dead TIDs from row lists
and drops empty term entries during merge.

See `INDEX_GC_PROTOCOL.md` for the GC contract.

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

## Future Enhancements

- Fuzzy search via Levenshtein automata
- Weighted outputs for suggestion ranking
- Shared FST dictionary across indexes

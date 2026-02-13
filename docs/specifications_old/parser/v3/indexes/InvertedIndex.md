# Inverted Index (Full-Text Search) Specification for ScratchBird


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)



Status: Authoritative (V3)
Last Updated: 2026-02-08
Features: Page-size agnostic (supports 8K, 16K, 32K, 64K, 128K pages)

**Naming Note:** Catalog/SBLR use `IndexType::FULLTEXT`. This spec uses `USING inverted(...)` as the DDL alias; the parser MUST expose FULLTEXT/INVERTED.

**Authoritative Note:** Any schedule text or time estimates in this document are historical and
non-normative. All features described here are REQUIRED for V3 unless explicitly marked
"Reserved (Not Supported in V3)".

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Decision](#architecture-decision)
3. [External Dependencies](#external-dependencies)
4. [On-Disk Structures](#on-disk-structures)
5. [Page Size Considerations](#page-size-considerations)
6. [Term Dictionary](#term-dictionary)
7. [Posting Lists](#posting-lists)
8. [Tokenization Pipeline](#tokenization-pipeline)
9. [Compression](#compression)
10. [MGA Compliance](#mga-compliance)
11. [Core API](#core-api)
12. [DML Integration](#dml-integration)
13. [Query Processing](#query-processing)
14. [Scoring](#scoring)
15. [Segment Management](#segment-management)
16. [Query Planner Integration](#query-planner-integration)
17. [Bytecode Integration](#bytecode-integration)
18. [Implementation Steps](#implementation-steps)
19. [Testing Requirements](#testing-requirements)
20. [Performance Targets](#performance-targets)
21. [Reserved Enhancements (Not Supported in V3)](#reserved-enhancements-not-supported-in-v3)

---

## Overview

### Purpose

Inverted indexes enable **full-text search** on text columns by mapping terms (words) to the documents (rows) containing them. This allows sub-100ms search queries across millions of documents.

### Key Characteristics

- **Type:** Auxiliary index structure (standalone)
- **Granularity:** Term-level indexing with document references
- **Space efficiency:** 20-40% of original text size (with compression)
- **Query acceleration:** 100-10,000x speedup vs LIKE queries
- **Best for:** Search engines, document retrieval, log analysis
- **Page-size aware:** Adapts to database page size (8K/16K/32K/64K/128K)

---

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Index Construction (Single-Pass / Blocked)

1. **Tokenize** each document (row):
   - Case fold, normalize Unicode, remove punctuation.  
   - optional stopword removal and stemming/lemmatization.  
2. For each term occurrence, emit `(term, doc_id, position)` triples.  
3. Sort triples by `(term, doc_id, position)` in blocks.  
4. For each term, build a **posting list**:
   - `(doc_id, term_frequency, positions[])`  
5. Compress postings:
   - Delta‑encode doc_ids and positions.  
   - Variable‑byte or bit‑packed integers.  
6. Write segments to disk; merge segments into a single term dictionary + postings store.

### Term Dictionary

1. Store lexicographically sorted terms.  
2. Map each term → postings offset + length.  
3. Use FST or B‑tree for dictionary lookup (optional).

### Query Processing

1. Parse query into boolean/phrase/proximity operators.  
2. Fetch postings for each term.  
3. Evaluate boolean operators with document‑at‑a‑time or term‑at‑a‑time merging.  
4. For phrase queries, intersect positional postings with offset constraints.  
5. Apply scoring (BM25 or TF‑IDF) over candidate docs.

### Updates / Deletes

1. Inserts append to a **delta segment** (like LSM).  
2. Deletes emit tombstones for doc_id; filtered during merges.  
3. Periodic merge compacts delta segments into base segments.

### MGA / Versioning

- Postings store version metadata for each doc_id.  
- Queries return only versions visible to MGA epoch.

### Complexity Targets

- Indexing: `O(N log N)` dominated by sort/merge.  
- Query: `O(sum(postings))` with skipping and compression.

### References (for algorithmic definitions)

- Manning, Raghavan, Schütze, *Introduction to Information Retrieval*, 2008 (inverted index construction and query processing).  

---

### Classic Use Case

```sql
-- Document table
CREATE TABLE documents (
    doc_id BIGINT PRIMARY KEY,
    title VARCHAR(255),
    content TEXT,
    created_at TIMESTAMP
);

-- Create inverted index on content
CREATE INDEX idx_content_fts ON documents USING inverted(content);

-- Full-text search query
SELECT doc_id, title, ts_rank(content, 'database search') AS score
FROM documents
WHERE content @@ 'database & search'
ORDER BY score DESC
LIMIT 10;

-- Without inverted index: Full table scan with LIKE → 10-60 seconds
-- With inverted index: Term lookup + intersection → 10-100 milliseconds
```

### Integration Strategy

**Design Decision:** Inverted indexes are **standalone index types** similar to B-Tree indexes, but optimized for text search.

```
Table: documents
├── Heap Pages (row data)
└── Inverted Index
    ├── Segment 0 (immutable)
    │   ├── Term Dictionary
    │   ├── Posting Lists
    │   └── Document Stats
    ├── Segment 1 (immutable)
    └── Segment N (active, mutable)
```

---

## Architecture Decision

### Segment-Based Design

Following Lucene's proven architecture, ScratchBird uses **immutable segments** for inverted indexes.

**Rationale:**
- **Write-once, read-many (WORM):** Immutable segments simplify concurrency
- **Incremental indexing:** New documents go into new segments
- **Background merging:** Merge small segments into larger ones
- **Delete via tombstones:** Mark deleted documents, clean up during merge

#### Segment Structure


**Logical Fields:**

- `segment_id` (uint32_t): Unique segment ID
- `num_documents` (uint64_t): Documents in this segment
- `num_terms` (uint64_t): Unique terms
- `meta_page` (uint32_t): Metadata page ID
- `dict_first_page` (uint32_t): First dictionary page
- `posting_first_page` (uint32_t): First posting list page
- `created_at` (uint64_t): Unix timestamp
- `flags` (uint8_t): Status flags (active, merged, etc.)
- `deleted_docs` (std::vector<bool>): In-memory


#### Multi-Segment Query Processing

```
Query: "database search"

1. Tokenize → ["database", "search"]
2. For each segment:
   a. Lookup "database" → DocList1
   b. Lookup "search" → DocList2
   c. Intersect → MatchingDocs
3. Merge results from all segments
4. Score and rank
5. Return top K
```

**Merge Policy:** TieredMergePolicy (similar to Lucene)
- Segments grouped into tiers by size
- Merge when tier has ≥ 10 segments
- Target merged segment: 5GB max

---

## External Dependencies

### 1. Snowball Stemmer (BSD License)

**Purpose:** Language-specific stemming for search normalization

**Integration:**
- **Library:** libstemmer (C library)
- **License:** BSD 3-Clause (permissive, compatible)
- **Languages:** English, Spanish, French, German, etc. (40+ languages)
- **Size:** ~500 KB compiled library

**Installation:**
```bash
# Download Snowball stemmer
git clone https://github.com/snowballstem/snowball.git
cd snowball
make
sudo make install
```

**API Usage:**

**Logical Fields:**

- (see authoritative spec for on-disk layout)


**Examples:**
```
running → run
databases → databas
searching → search
```

### 2. ICU (International Components for Unicode) - optional

**Purpose:** Advanced Unicode tokenization and normalization

**Baseline:** Simple ASCII tokenization (built-in, no dependencies)
**ICU (optional):** If ICU is available, use it; otherwise fall back to built-in UCA tables.

**License:** ICU License (permissive, similar to MIT/X)

---

## On-Disk Structures

**Storage Layout Authority:** On-disk page headers, slot arrays, free-space rules, and page-type layouts are authoritative in `../storage/PAGE_TYPES_AND_LAYOUTS.md`. Any structs here are logical field groupings; do not infer byte-accurate layout from this file.

### Page Layout Overview

```
Inverted Index
├── Meta Pages (per segment)
│   └── Segment metadata, statistics
├── Term Dictionary Pages
│   ├── Term entries (sorted)
│   └── B-Tree index for term lookup
├── Posting List Pages
│   ├── Compressed document IDs
│   ├── Term frequencies
│   └── (optional) Word positions
└── Document Stats Pages
    └── Document lengths, field counts
```

### 1. Inverted Index Meta Page


**Logical Fields:**

- `ii_header` (PageHeader): Page header (see `../storage/PAGE_TYPES_AND_LAYOUTS.md`)
- `ii_index_uuid[16]` (uint8_t): Index UUID (16 bytes)
- `ii_table_id` (uint32_t): Parent table ID (4 bytes)
- `ii_column_id` (uint16_t): Indexed column (2 bytes)
- `ii_language` (uint16_t): Language code (2 bytes, 0=English)
- `ii_num_segments` (uint32_t): Total segments (4 bytes)
- `ii_active_segment` (uint32_t): Current active segment (4 bytes)
- `ii_total_documents` (uint64_t): Total documents indexed (8 bytes)
- `ii_total_terms` (uint64_t): Total unique terms (8 bytes)
- `ii_total_tokens` (uint64_t): Total tokens indexed (8 bytes)
- `ii_avg_doc_length` (uint32_t): Average document length (4 bytes)
- `ii_features` (uint8_t): Feature flags (1 byte)
- `ii_compression_type` (uint8_t): 0=None, 1=VByte, 2=PForDelta (1 byte)
- `ii_reserved1[30]` (uint8_t): Reserved (30 bytes)
- `ii_segment_pages[256]` (uint32_t): Meta page for each segment (1024 bytes)
- `ii_total_queries` (uint64_t): Total queries (8 bytes)
- `ii_avg_query_time_us` (uint64_t): Average query time (8 bytes)
- `ii_last_merge_time` (uint64_t): Last merge timestamp (8 bytes)
- `ii_reserved2` (uint64_t): Reserved (8 bytes)
- `ii_padding[]` (uint8_t): Flexible - size varies by page_size


### 2. Segment Meta Page


**Logical Fields:**

- `seg_header` (PageHeader): Page header (see `../storage/PAGE_TYPES_AND_LAYOUTS.md`)
- `seg_id` (uint32_t): Segment ID (4 bytes)
- `seg_num_documents` (uint64_t): Documents in segment (8 bytes)
- `seg_num_terms` (uint64_t): Unique terms (8 bytes)
- `seg_num_tokens` (uint64_t): Total tokens (8 bytes)
- `seg_avg_doc_length` (uint32_t): Average document length (4 bytes)
- `seg_created_at` (uint64_t): Creation timestamp (8 bytes)
- `seg_merged_at` (uint64_t): Merge timestamp (8 bytes, 0 if not merged)
- `seg_flags` (uint8_t): Status flags (1 byte)
- `seg_reserved1[23]` (uint8_t): Reserved (23 bytes)
- `seg_dict_first_page` (uint32_t): First dictionary page (4 bytes)
- `seg_dict_num_pages` (uint32_t): Dictionary page count (4 bytes)
- `seg_posting_first_page` (uint32_t): First posting list page (4 bytes)
- `seg_posting_num_pages` (uint32_t): Posting list page count (4 bytes)
- `seg_docstats_page` (uint32_t): Document statistics page (4 bytes)
- `seg_delete_bitmap_page` (uint32_t): Deletion bitmap page (4 bytes, 0 if none)
- `seg_total_posting_bytes` (uint64_t): Total posting list bytes (8 bytes)
- `seg_reserved2` (uint64_t): Reserved (8 bytes)
- `seg_padding[]` (uint8_t): Flexible array member


### 3. Term Dictionary Entry


**Logical Fields:**

- `term[64]` (char): Term text (null-terminated, 64 bytes)
- `term_hash` (uint32_t): Hash for quick comparison (4 bytes)
- `doc_frequency` (uint32_t): Documents containing term (4 bytes)
- `total_frequency` (uint64_t): Total occurrences (8 bytes)
- `posting_offset` (uint64_t): Byte offset in posting list pages (8 bytes)
- `posting_length` (uint32_t): Bytes in posting list (4 bytes)
- `reserved` (uint32_t): Reserved (4 bytes)
- `reserved2[4]` (uint64_t): Reserved (32 bytes)


### 4. Term Dictionary Page


**Logical Fields:**

- `dict_header` (PageHeader): Page header (see `../storage/PAGE_TYPES_AND_LAYOUTS.md`)
- `dict_next_page` (uint32_t): Next dictionary page (0 if last) (4 bytes)
- `dict_num_entries` (uint16_t): Number of entries on page (2 bytes)
- `dict_reserved` (uint16_t): Reserved (2 bytes)
- `dict_first_term_hash` (uint64_t): First term hash (for binary search) (8 bytes)
- `dict_entries[]` (uint8_t): Variable-length entries


### 5. Posting List Page


**Logical Fields:**

- `post_header` (PageHeader): Page header (see `../storage/PAGE_TYPES_AND_LAYOUTS.md`)
- `post_next_page` (uint32_t): Next posting page (0 if last) (4 bytes)
- `post_data_length` (uint32_t): Bytes of compressed data (4 bytes)
- `post_compression_type` (uint8_t): Compression type (1 byte)
- `post_reserved[7]` (uint8_t): Reserved (7 bytes)
- `post_data[]` (uint8_t): Flexible array member


### 6. Document Statistics Page


**Logical Fields:**

- `doc_id` (uint32_t): Document ID (local to segment) (4 bytes)
- `doc_length` (uint32_t): Number of tokens (4 bytes)
- `num_unique_terms` (uint32_t): Distinct terms (4 bytes)
- `reserved` (uint32_t): Reserved (4 bytes)
- `docstats_header` (PageHeader): Page header (see `../storage/PAGE_TYPES_AND_LAYOUTS.md`)
- `docstats_next_page` (uint32_t): Next stats page (0 if last) (4 bytes)
- `docstats_num_entries` (uint32_t): Number of entries (4 bytes)
- `docstats_reserved` (uint64_t): Reserved (8 bytes)
- `docstats_data[]` (uint8_t): Flexible array member


---

## Page Size Considerations

Inverted indexes in ScratchBird adapt to the database page size configured at creation time.

### Supported Page Sizes

- 8 KB (8,192 bytes)
- 16 KB (16,384 bytes)
- 32 KB (32,768 bytes)
- 64 KB (65,536 bytes)
- 128 KB (131,072 bytes)

### Terms Per Dictionary Page

| Page Size | Available Bytes | Terms Per Page |
|-----------|-----------------|----------------|
| 8 KB      | 8,112           | 63             |
| 16 KB     | 16,304          | 127            |
| 32 KB     | 32,688          | 255            |
| 64 KB     | 65,456          | 511            |
| 128 KB    | 130,992         | 1,023          |

### Documents Per Stats Page

| Page Size | Available Bytes | Docs Per Page |
|-----------|-----------------|---------------|
| 8 KB      | 8,112           | 507           |
| 16 KB     | 16,304          | 1,019         |
| 32 KB     | 32,688          | 2,043         |
| 64 KB     | 65,456          | 4,091         |
| 128 KB    | 130,992         | 8,187         |

### Posting List Capacity

| Page Size | Available Bytes | Approx Doc IDs (VByte) |
|-----------|-----------------|------------------------|
| 8 KB      | 8,112           | ~6,000-8,000          |
| 16 KB     | 16,304          | ~12,000-16,000        |
| 32 KB     | 32,688          | ~24,000-32,000        |
| 64 KB     | 65,456          | ~48,000-64,000        |
| 128 KB    | 130,992         | ~96,000-128,000       |

**Note:** VByte encoding compresses small integers efficiently. Larger page sizes allow more documents per posting list page, reducing fragmentation.

### Trade-offs by Page Size

#### Small Pages (8KB)
**Pros:**
- More granular I/O for small term lookups
- Lower memory footprint per page

**Cons:**
- More pages needed for large posting lists
- Higher page overhead percentage

**Best for:** Small to medium document collections (< 1M documents)

#### Large Pages (64KB-128KB)
**Pros:**
- Fewer pages for large posting lists
- Better sequential scan performance
- More terms cached per dictionary page

**Cons:**
- Higher memory usage per page load
- Potential waste for rare terms

**Best for:** Large document collections (> 10M documents), log analysis

---

## Term Dictionary

The term dictionary maps terms (words) to posting list metadata.

### Data Structure Choice

**Baseline:** Sorted array with binary search (simple, cache-friendly)
**Reserved (Not Supported in V3):** B-Tree for faster insertions

### Binary Search Implementation

```cpp
class TermDictionary {
public:
    // Lookup term in dictionary
    Status lookup(const std::string& term, TermDictionaryEntry* entry_out, ErrorContext* ctx) {
        uint32_t term_hash = hashTerm(term);

        // Binary search across dictionary pages
        uint32_t page_num = dict_first_page_;

        while (page_num != 0) {
            BufferFrame* frame = nullptr;
            auto status = db_->getBufferPool()->pinPage(page_num, &frame, ctx);
            if (status != Status::OK) return status;

            auto* dict_page = reinterpret_cast<SBTermDictionaryPage*>(frame->getData());

            // Binary search within page
            int idx = binarySearchTerms(dict_page, term, term_hash);

            if (idx >= 0) {
                // Found!
                memcpy(entry_out, getTermEntry(dict_page, idx), sizeof(TermDictionaryEntry));
                db_->getBufferPool()->unpinPage(page_num);
                return Status::OK;
            }

            // Check next page
            page_num = dict_page->dict_next_page;
            db_->getBufferPool()->unpinPage(page_num);
        }

        return Status::NOT_FOUND;
    }

private:
    int binarySearchTerms(SBTermDictionaryPage* page, const std::string& term, uint32_t hash) {
        int left = 0;
        int right = page->dict_num_entries - 1;

        while (left <= right) {
            int mid = (left + right) / 2;
            auto* entry = getTermEntry(page, mid);

            // Compare hash first (fast)
            if (entry->term_hash == hash) {
                // Hash match, verify full term (handle collisions)
                int cmp = strncmp(entry->term, term.c_str(), 63);
                if (cmp == 0) return mid;
                if (cmp < 0) left = mid + 1;
                else right = mid - 1;
            } else if (entry->term_hash < hash) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }

        return -1;  // Not found
    }

    TermDictionaryEntry* getTermEntry(SBTermDictionaryPage* page, int index) {
        return (TermDictionaryEntry*)(page->dict_entries + (index * sizeof(TermDictionaryEntry)));
    }

    uint32_t hashTerm(const std::string& term) {
        // Use xxHash3 for speed
        return XXH32(term.c_str(), term.length(), 0);
    }
};
```

---

## Posting Lists

Posting lists store the document IDs where each term appears.

### Structure


**Logical Fields:**

- `doc_ids` (std::vector<uint32_t>): Sorted document IDs (delta-encoded)
- `frequencies` (std::vector<uint32_t>): Term frequency in each doc
- `positions` (std::vector<std::vector<uint32_t>>): (optional) Word positions


### Example

```
Term: "database"
Documents: [5, 12, 15, 17, 25]
Frequencies: [2, 1, 3, 1, 5]
Positions (optional): [[3,10], [7], [2,8,15], [12], [1,5,9,13,18]]
```

### Storage Format

Posting lists are stored compressed in posting list pages:

```
[Header: 80 bytes]
[Compressed doc IDs]
[Compressed frequencies]
[(optional) Compressed positions]
```

---

## Tokenization Pipeline

### Pipeline Stages

```
Raw Text → Character Filters → Tokenization → Token Filters → Indexed Terms

Character Filters:
  • HTML strip (remove tags)
  • Unicode normalization

Tokenization:
  • Split on whitespace and punctuation
  • Handle contractions (don't → don't or do n't)

Token Filters:
  • Lowercase conversion
  • Stop word removal (optional)
  • Stemming (Snowball)
  • Length filter (min 2, max 40 characters)
```

### Tokenizer Implementation


**Logical Fields:**

- `tokens` (std::vector<std::string>)
- `token` (std::string)
- `tokens` (return)
- `result` (std::vector<std::string>)
- `result` (return)
- `result` (std::vector<std::string>)
- `result` (return)
- `stop_words_` (std::set<std::string>)
- `result` (std::vector<std::string>)
- `result` (return)


### Analyzer

```cpp
class Analyzer {
public:
    virtual ~Analyzer() = default;
    virtual std::vector<std::string> analyze(const std::string& text) = 0;
};

class StandardAnalyzer : public Analyzer {
public:
    StandardAnalyzer(bool use_stemming = true, bool filter_stop_words = true)
        : use_stemming_(use_stemming), filter_stop_words_(filter_stop_words) {

        // Initialize stop words
        stop_words_ = {"the", "a", "an", "and", "or", "but", "in", "on", "at",
                      "to", "for", "of", "with", "is", "are", "was", "were"};
    }

    std::vector<std::string> analyze(const std::string& text) override {
        // 1. Tokenize
        auto tokens = tokenizer_.tokenize(text);

        // 2. Lowercase
        LowercaseFilter lowercase;
        tokens = lowercase.filter(tokens);

        // 3. Filter stop words
        if (filter_stop_words_) {
            StopWordFilter stop_filter(stop_words_);
            tokens = stop_filter.filter(tokens);
        }

        // 4. Stem
        if (use_stemming_) {
            SnowballStemFilter stem_filter;
            tokens = stem_filter.filter(tokens);
        }

        return tokens;
    }

private:
    StandardTokenizer tokenizer_;
    std::set<std::string> stop_words_;
    bool use_stemming_;
    bool filter_stop_words_;
};
```

---

## Compression

Posting lists are compressed using **VByte encoding** (Baseline).

### VByte (Variable Byte) Encoding

**Principle:** Use 7 bits for data, 1 bit for continuation flag

```cpp
class VByteCodec {
public:
    // Encode delta-encoded integers
    static std::vector<uint8_t> encode(const std::vector<uint32_t>& integers) {
        std::vector<uint8_t> result;

        for (uint32_t num : integers) {
            while (num >= 128) {
                result.push_back((num & 0x7F) | 0x80);  // MSB=1: more bytes follow
                num >>= 7;
            }
            result.push_back(num & 0x7F);  // MSB=0: final byte
        }

        return result;
    }

    // Decode back to integers
    static std::vector<uint32_t> decode(const uint8_t* data, size_t length) {
        std::vector<uint32_t> result;
        uint32_t num = 0;
        int shift = 0;

        for (size_t i = 0; i < length; i++) {
            uint8_t byte = data[i];
            num |= ((uint32_t)(byte & 0x7F) << shift);

            if ((byte & 0x80) == 0) {
                // Final byte
                result.push_back(num);
                num = 0;
                shift = 0;
            } else {
                shift += 7;
            }
        }

        return result;
    }
};
```

### Delta Encoding

Before compression, convert document IDs to deltas:

```cpp
std::vector<uint32_t> deltaEncode(const std::vector<uint32_t>& doc_ids) {
    std::vector<uint32_t> deltas;
    uint32_t prev = 0;

    for (uint32_t doc_id : doc_ids) {
        deltas.push_back(doc_id - prev);
        prev = doc_id;
    }

    return deltas;
}

std::vector<uint32_t> deltaDecode(const std::vector<uint32_t>& deltas) {
    std::vector<uint32_t> doc_ids;
    uint32_t sum = 0;

    for (uint32_t delta : deltas) {
        sum += delta;
        doc_ids.push_back(sum);
    }

    return doc_ids;
}
```

### Compression Pipeline

```cpp
// Compress posting list
std::vector<uint8_t> compressPostingList(const PostingList& posting) {
    // 1. Delta encode doc IDs
    auto deltas = deltaEncode(posting.doc_ids);

    // 2. VByte encode deltas
    auto compressed_docs = VByteCodec::encode(deltas);

    // 3. VByte encode frequencies
    auto compressed_freqs = VByteCodec::encode(posting.frequencies);

    // 4. Combine
    std::vector<uint8_t> result;
    result.insert(result.end(), compressed_docs.begin(), compressed_docs.end());
    result.insert(result.end(), compressed_freqs.begin(), compressed_freqs.end());

    return result;
}
```

**Compression Ratio:** Typically 4-10x for document IDs

---

## MGA Compliance

### Challenge: Document Visibility

Inverted indexes map terms to document IDs, but in an MGA system:

**Problem:**
- Transaction T1 inserts document D1 with term "database"
- Inverted index maps "database" → D1
- Transaction T1 rolls back
- Index now contains invalid reference to D1

### Solution: Lazy Visibility Checking

**Approach:** Inverted index stores all document references (including uncommitted). Query processor filters using TIP (Transaction Inventory Page).

```cpp
// Query processing with MGA compliance
std::vector<UUID> InvertedIndex::search(const std::string& query,
                                       const SBTransactionSnapshot* snapshot,
                                       SBTransactionManager* tm,
                                       ErrorContext* ctx) {
    // 1. Parse query, extract terms
    auto terms = parseQuery(query);

    // 2. For each term, get posting list (may include uncommitted docs)
    std::vector<UUID> candidate_docs;
    for (const auto& term : terms) {
        auto posting = lookupTerm(term, ctx);
        candidate_docs = intersect(candidate_docs, posting.record_uuids);
    }

    // 3. Filter candidates by visibility (TIP checks)
    std::vector<UUID> visible_docs;
    for (const auto& record_uuid : candidate_docs) {
        const SBRecordHeader* rhd = resolve_record_header_by_uuid(record_uuid);
        if (!rhd) continue;

        const SBRecordHeader* visible = sb_find_visible_version(rhd, snapshot, tm);
        if (visible && (visible->rhd_flags & RHD_DELETED) == 0) {
            visible_docs.push_back(record_uuid);
        }
    }

    return visible_docs;
}
```

### Garbage Collection Integration

During sweep/GC, remove references to deleted documents:

```cpp
Status InvertedIndex::removeDeadEntries(const std::vector<UUID>& dead_record_uuids,
                                        ErrorContext* ctx) {
    // Mark segments for rebuilding if they contain dead docs
    for (auto& segment : segments_) {
        bool needs_rebuild = false;

        for (const auto& record_uuid : dead_record_uuids) {
            if (segment.contains(record_uuid)) {
                segment.deleted_records[record_uuid] = true;
                needs_rebuild = true;
            }
        }

        // Trigger merge if deletion ratio > 20%
        double deletion_ratio = segment.numDeleted() / (double)segment.num_documents;
        if (deletion_ratio > 0.20) {
            scheduleMerge(&segment);
        }
    }

    return Status::OK;
}
```

### MGA Compliance Summary

✅ **Correctness:** TIP checks ensure only visible documents are returned
✅ **Performance:** Temporary inclusion of uncommitted docs is acceptable
⚠️ **Maintenance:** Require periodic segment merging to remove deleted docs
✅ **Concurrency:** Read posting lists without locking

---

## Tablespace + Record UUID Requirements

- **Row identity:** Posting lists store `record_uuid` with optional `SBRecordPtr` cache hints (no legacy page IDs).
- **Document ID mapping:** Any doc-id indirection must round-trip through `record_uuid` without dropping `tablespace_id`.
- **Tablespace routing:** On-disk pages for the inverted index must allocate/pin via `root_gpid` and `tablespace_id`.
- **Visibility checks:** Record fetch uses `record_uuid` resolution; cached `SBRecordPtr` may be stale.
- **Migration:** `updateRecordLocatorsAfterMigration` must refresh cached pointers for migrated GPIDs, including compressed segments.

---

## Core API

**File:** `include/scratchbird/core/inverted_index.h`


**Logical Fields:**

- `Database` (class)
- `Table` (class)
- `Analyzer` (class)
- `record_uuid` (UUID)
- `score` (float)
- `store_positions` (bool): Store word positions (for phrase queries)
- `store_offsets` (bool): Store character offsets
- `enable_stemming` (bool): Use Snowball stemmer
- `filter_stop_words` (bool): Remove stop words
- `language` (std::string): Language for stemmer (default: "english")
- `min_term_length` (uint32_t): Minimum term length (default: 2)
- `max_term_length` (uint32_t): Maximum term length (default: 40)
- `compression_type` (uint8_t): 0=None, 1=VByte (default: 1)
- `merge_factor` (uint32_t): Segments to merge at once (default: 10)
- `max_segment_size` (uint64_t): Max segment size in bytes (default: 5GB)
- `db_` (Database*)
- `index_uuid_` (UuidV7Bytes)
- `meta_page_` (uint32_t)
- `column_id_` (uint16_t)
- `config_` (InvertedIndexConfig)
- `analyzer_` (std::unique_ptr<Analyzer>)
- `segments_` (std::vector<InvertedIndexSegment>)


---

## DML Integration

### INSERT Hook

When a new row is inserted with text data, index it in the active segment.

**File:** `src/core/storage_engine.cpp`

```cpp
Status StorageEngine::insertTuple(Table* table,
                                 const std::vector<Value>& values,
                                 UUID* record_uuid_out,
                                 ErrorContext* ctx) {
    // ... existing heap insert logic ...
    //

 Allocate page, insert record version, etc.

    // NEW: Update inverted indexes
    for (auto& idx_info : table->indexes) {
        if (idx_info.type == IndexType::INVERTED_INDEX) {
            auto* inv_idx = static_cast<InvertedIndex*>(idx_info.index.get());

            // Get text value
            uint16_t col_id = inv_idx->getColumnId();
            const auto& text_value = values[col_id];

            if (!text_value.isNull()) {
                // Index the document by record UUID
                auto status = inv_idx->indexDocument(*record_uuid_out,
                                                    text_value.asString(),
                                                    ctx);
                if (status != Status::OK) {
                    LOG_WARN(Category::INDEX, "Failed to index document: %d", status);
                    // Non-fatal
                }
            }
        }
    }

    return Status::OK;
}
```

### UPDATE Hook

When text is updated, mark old document as deleted and index new version.

```cpp
Status StorageEngine::updateTuple(Table* table,
                                 const UUID& record_uuid,
                                 const std::vector<Value>& new_values,
                                 ErrorContext* ctx) {
    // ... existing update logic ...

    // NEW: Update inverted indexes
    for (auto& idx_info : table->indexes) {
        if (idx_info.type == IndexType::INVERTED_INDEX) {
            auto* inv_idx = static_cast<InvertedIndex*>(idx_info.index.get());

            uint16_t col_id = inv_idx->getColumnId();

            // Check if indexed column changed
            if (old_values[col_id] != new_values[col_id]) {
                // Mark old document as deleted
                inv_idx->deleteDocument(record_uuid, ctx);

                // Index new version
                if (!new_values[col_id].isNull()) {
                    inv_idx->indexDocument(new_record_uuid,
                                          new_values[col_id].asString(),
                                          ctx);
                }
            }
        }
    }

    return Status::OK;
}
```

### DELETE Hook

Mark document as deleted in the inverted index.

```cpp
Status StorageEngine::deleteTuple(Table* table, const UUID& record_uuid, ErrorContext* ctx) {
    // ... existing delete logic (creates RHD_DELETED version) ...

    // NEW: Mark document deleted in inverted indexes
    for (auto& idx_info : table->indexes) {
        if (idx_info.type == IndexType::INVERTED_INDEX) {
            auto* inv_idx = static_cast<InvertedIndex*>(idx_info.index.get());

            inv_idx->deleteDocument(record_uuid, ctx);
        }
    }

    return Status::OK;
}
```

---

## Query Processing

### Boolean Query Parser


**Logical Fields:**

- `op` (QueryOperator)
- `term` (std::string): For leaf nodes
- `children` (std::vector<QueryNode*>): For operator nodes
- `tokens` (std::vector<std::string>)
- `tokens` (return)
- `nullptr` (return)


### Term Intersection (AND)

```cpp
std::vector<uint32_t> intersect(const std::vector<uint32_t>& list1,
                                const std::vector<uint32_t>& list2) {
    std::vector<uint32_t> result;
    size_t i = 0, j = 0;

    while (i < list1.size() && j < list2.size()) {
        if (list1[i] == list2[j]) {
            result.push_back(list1[i]);
            i++;
            j++;
        } else if (list1[i] < list2[j]) {
            i++;
        } else {
            j++;
        }
    }

    return result;
}
```

### Term Union (OR)

```cpp
std::vector<uint32_t> unionLists(const std::vector<uint32_t>& list1,
                                 const std::vector<uint32_t>& list2) {
    std::vector<uint32_t> result;
    size_t i = 0, j = 0;

    while (i < list1.size() && j < list2.size()) {
        if (list1[i] == list2[j]) {
            result.push_back(list1[i]);
            i++;
            j++;
        } else if (list1[i] < list2[j]) {
            result.push_back(list1[i]);
            i++;
        } else {
            result.push_back(list2[j]);
            j++;
        }
    }

    // Append remaining
    while (i < list1.size()) result.push_back(list1[i++]);
    while (j < list2.size()) result.push_back(list2[j++]);

    return result;
}
```

### Query Execution

```cpp
std::vector<SearchResult> InvertedIndex::search(const std::string& query,
                                               TransactionId current_xid,
                                               uint32_t limit,
                                               ErrorContext* ctx) {
    // 1. Analyze query (tokenize, stem)
    auto query_terms = analyzer_->analyze(query);

    // 2. For each term, lookup posting lists from all segments
    std::vector<std::vector<uint32_t>> term_postings;

    for (const auto& term : query_terms) {
        std::vector<uint32_t> combined_posting;

        // Lookup term in each segment
        for (const auto& segment : segments_) {
            PostingList posting;
            auto status = lookupTermInSegment(segment.segment_id, term, &posting, ctx);

            if (status == Status::OK) {
                // Add segment offset to doc IDs
                for (const auto& record_uuid : posting.record_uuids) {
                    combined_posting.push_back(record_uuid);
                }
            }
        }

        term_postings.push_back(combined_posting);
    }

    // 3. Intersect posting lists (AND query)
    std::vector<UUID> candidate_docs = term_postings[0];
    for (size_t i = 1; i < term_postings.size(); i++) {
        candidate_docs = intersect(candidate_docs, term_postings[i]);
    }

    // 4. Filter by visibility (MGA compliance)
    std::vector<UUID> visible_docs;
    for (const auto& record_uuid : candidate_docs) {
        const SBRecordHeader* rhd = resolve_record_header_by_uuid(record_uuid);
        if (!rhd) continue;

        const SBRecordHeader* visible = sb_find_visible_version(rhd, snapshot, tm);
        if (visible && (visible->rhd_flags & RHD_DELETED) == 0) {
            visible_docs.push_back(record_uuid);
        }
    }

    // 5. Score and rank
    auto results = scoreAndRank(visible_docs, query_terms, ctx);

    // 6. Sort by score descending
    std::sort(results.begin(), results.end(),
             [](const SearchResult& a, const SearchResult& b) {
                 return a.score > b.score;
             });

    // 7. Return top K
    if (results.size() > limit) {
        results.resize(limit);
    }

    return results;
}
```

---

## Scoring

### BM25 Algorithm

**Best Matches 25 (BM25)** is the industry-standard scoring function.

**Formula:**
```
BM25(D, Q) = Σ IDF(qi) × [f(qi,D) × (k1 + 1)] / [f(qi,D) + k1 × (1 - b + b × |D|/avgdl)]

Where:
  qi = query terms
  f(qi,D) = term frequency in document D
  |D| = document length (number of terms)
  avgdl = average document length across all documents
  k1 = 1.2 (controls TF saturation)
  b = 0.75 (controls length normalization)

IDF(qi) = log[(N - n(qi) + 0.5) / (n(qi) + 0.5)]

Where:
  N = total number of documents
  n(qi) = number of documents containing term qi
```

### Implementation

```cpp
class BM25Scorer {
public:
    BM25Scorer(uint64_t total_docs, uint32_t avg_doc_length,
              double k1 = 1.2, double b = 0.75)
        : total_docs_(total_docs),
          avg_doc_length_(avg_doc_length),
          k1_(k1),
          b_(b) {}

    float score(const std::vector<std::string>& query_terms,
               uint32_t doc_id,
               const DocumentStats& doc_stats,
               const std::map<std::string, uint32_t>& term_frequencies,
               const std::map<std::string, uint32_t>& doc_frequencies) {

        float total_score = 0.0f;

        for (const auto& term : query_terms) {
            // Term frequency in this document
            uint32_t tf = 0;
            auto it = term_frequencies.find(term);
            if (it != term_frequencies.end()) {
                tf = it->second;
            }

            if (tf == 0) continue;  // Term not in document

            // Document frequency (how many docs contain this term)
            uint32_t df = 0;
            auto df_it = doc_frequencies.find(term);
            if (df_it != doc_frequencies.end()) {
                df = df_it->second;
            }

            // IDF calculation
            double idf = log((total_docs_ - df + 0.5) / (df + 0.5));

            // BM25 calculation
            double numerator = tf * (k1_ + 1.0);
            double denominator = tf + k1_ * (1.0 - b_ + b_ * (doc_stats.doc_length / (double)avg_doc_length_));

            total_score += idf * (numerator / denominator);
        }

        return total_score;
    }

private:
    uint64_t total_docs_;
    uint32_t avg_doc_length_;
    double k1_;
    double b_;
};
```

### Scoring Integration

```cpp
std::vector<SearchResult> InvertedIndex::scoreAndRank(
    const std::vector<UUID>& docs,
    const std::vector<std::string>& query_terms,
    ErrorContext* ctx) {

    std::vector<SearchResult> results;

    // Initialize BM25 scorer
    BM25Scorer scorer(total_documents_, avg_doc_length_);

    for (const auto& record_uuid : docs) {
        // Get document statistics
        DocumentStats doc_stats;
        auto status = getDocumentStats(record_uuid, &doc_stats, ctx);
        if (status != Status::OK) continue;

        // Get term frequencies for this document
        std::map<std::string, uint32_t> term_freqs;
        for (const auto& term : query_terms) {
            uint32_t tf = getTermFrequency(record_uuid, term, ctx);
            term_freqs[term] = tf;
        }

        // Get document frequencies (global)
        std::map<std::string, uint32_t> doc_freqs;
        for (const auto& term : query_terms) {
            TermDictionaryEntry entry;
            auto status = lookupTerm(term, &entry, ctx);
            if (status == Status::OK) {
                doc_freqs[term] = entry.doc_frequency;
            }
        }

        // Calculate BM25 score
        float score = scorer.score(query_terms, record_uuid, doc_stats, term_freqs, doc_freqs);

        results.push_back({record_uuid, score});
    }

    return results;
}
```

---

## Segment Management

### Segment Creation

```cpp
Status InvertedIndex::createNewSegment(uint32_t* segment_id_out, ErrorContext* ctx) {
    // Allocate segment meta page
    uint32_t seg_meta_page = 0;
    auto status = db_->getBufferPool()->allocatePage(&seg_meta_page, ctx);
    if (status != Status::OK) return status;

    // Initialize segment
    InvertedIndexSegment segment;
    segment.segment_id = next_segment_id_++;
    segment.num_documents = 0;
    segment.num_terms = 0;
    segment.meta_page = seg_meta_page;
    segment.flags = SEG_FLAG_ACTIVE;

    segments_.push_back(segment);
    *segment_id_out = segment.segment_id;

    return Status::OK;
}
```

### Merge Policy

```cpp
class TieredMergePolicy {
public:
    TieredMergePolicy(uint32_t merge_factor = 10, uint64_t max_segment_size = 5ULL * 1024 * 1024 * 1024)
        : merge_factor_(merge_factor), max_segment_size_(max_segment_size) {}

    std::vector<uint32_t> selectSegmentsToMerge(const std::vector<InvertedIndexSegment>& segments) {
        // Group segments by size tier
        std::map<uint32_t, std::vector<uint32_t>> tiers;  // tier -> segment IDs

        for (const auto& seg : segments) {
            if (seg.flags & SEG_FLAG_MERGED) continue;  // Skip already merged

            uint64_t size = seg.totalSizeBytes();
            uint32_t tier = getTier(size);

            tiers[tier].push_back(seg.segment_id);
        }

        // Find tier with >= merge_factor segments
        for (auto& [tier, seg_ids] : tiers) {
            if (seg_ids.size() >= merge_factor_) {
                // Merge these segments
                return seg_ids;
            }
        }

        return {};  // No merge needed
    }

private:
    uint32_t getTier(uint64_t size) {
        // Tier 0: 0-10MB
        // Tier 1: 10MB-100MB
        // Tier 2: 100MB-1GB
        // Tier 3: 1GB+

        if (size < 10 * 1024 * 1024) return 0;
        if (size < 100 * 1024 * 1024) return 1;
        if (size < 1024 * 1024 * 1024) return 2;
        return 3;
    }

    uint32_t merge_factor_;
    uint64_t max_segment_size_;
};
```

### Merge Execution

```cpp
Status InvertedIndex::mergeSegments(const std::vector<uint32_t>& segment_ids,
                                   ErrorContext* ctx) {
    // 1. Create new merged segment
    uint32_t merged_segment_id = 0;
    auto status = createNewSegment(&merged_segment_id, ctx);
    if (status != Status::OK) return status;

    // 2. Merge term dictionaries (union of all terms)
    std::map<std::string, std::vector<PostingList>> term_postings;

    for (uint32_t seg_id : segment_ids) {
        // Read all terms from this segment
        auto terms = getAllTermsFromSegment(seg_id, ctx);

        for (const auto& [term, posting] : terms) {
            term_postings[term].push_back(posting);
        }
    }

    // 3. For each term, merge posting lists
    for (auto& [term, postings] : term_postings) {
        PostingList merged_posting = mergePostingLists(postings);

        // Add to new segment
        addTermToSegment(merged_segment_id, term, merged_posting, ctx);
    }

    // 4. Mark old segments as merged
    for (uint32_t seg_id : segment_ids) {
        auto* seg = getSegment(seg_id);
        seg->flags |= SEG_FLAG_MERGED;
        seg->flags &= ~SEG_FLAG_ACTIVE;
    }

    // 5. Update meta page
    updateMetaPage(ctx);

    return Status::OK;
}

PostingList mergePostingLists(const std::vector<PostingList>& postings) {
    PostingList merged;

    // Merge-sort doc IDs
    std::priority_queue<std::pair<uint32_t, size_t>,
                       std::vector<std::pair<uint32_t, size_t>>,
                       std::greater<>> pq;

    // Initialize heap with first doc from each posting list
    for (size_t i = 0; i < postings.size(); i++) {
        if (!postings[i].doc_ids.empty()) {
            pq.push({postings[i].doc_ids[0], i});
        }
    }

    std::vector<size_t> indices(postings.size(), 0);

    while (!pq.empty()) {
        auto [doc_id, list_idx] = pq.top();
        pq.pop();

        merged.doc_ids.push_back(doc_id);
        merged.frequencies.push_back(postings[list_idx].frequencies[indices[list_idx]]);

        indices[list_idx]++;

        if (indices[list_idx] < postings[list_idx].doc_ids.size()) {
            pq.push({postings[list_idx].doc_ids[indices[list_idx]], list_idx});
        }
    }

    return merged;
}
```

---

## Query Planner Integration

### Full-Text Search Queries

**SQL Syntax:**
```sql
SELECT * FROM documents
WHERE content @@ 'database search';
```

The `@@` operator triggers full-text search.

### Planner Integration

**File:** `src/sblr/query_planner.cpp`

```cpp
Status QueryPlanner::planFullTextSearch(Table* table,
                                       const std::string& query,
                                       uint16_t column_id,
                                       SearchPlan* plan_out,
                                       ErrorContext* ctx) {
    // Find inverted index on this column
    InvertedIndex* inv_idx = nullptr;

    for (auto& idx : table->indexes) {
        if (idx.type == IndexType::INVERTED_INDEX) {
            auto* ii = static_cast<InvertedIndex*>(idx.index.get());
            if (ii->getColumnId() == column_id) {
                inv_idx = ii;
                break;
            }
        }
    }

    if (!inv_idx) {
        // No inverted index - fallback to LIKE scan
        plan_out->type = ScanPlan::FULL_SCAN;
        plan_out->table = table;
        return Status::OK;
    }

    // Use inverted index
    plan_out->type = ScanPlan::INVERTED_INDEX_SCAN;
    plan_out->table = table;
    plan_out->inverted_index = inv_idx;
    plan_out->query = query;

    LOG_INFO(Category::PLANNER, "Using inverted index for full-text search");

    return Status::OK;
}
```

### Executor Integration

```cpp
Status Executor::executeInvertedIndexScan(const SearchPlan& plan,
                                         std::vector<UUID>* results_out,
                                         ErrorContext* ctx) {
    auto snapshot = db_->getTransactionManager()->getCurrentSnapshot();
    auto* tm = db_->getTransactionManager();

    // Execute full-text search
    auto search_results = plan.inverted_index->search(plan.query,
                                                     snapshot,
                                                     tm,
                                                     plan.limit,
                                                     ctx);

    // Extract record UUIDs (already filtered by visibility)
    for (const auto& result : search_results) {
        results_out->push_back(result.record_uuid);
    }

    return Status::OK;
}
```

---

## Bytecode Integration

### SQL Syntax

```sql
-- Create inverted index
CREATE INDEX idx_content_fts ON documents USING inverted(content);

-- Create with options
CREATE INDEX idx_content_fts ON documents USING inverted(content)
WITH (
    store_positions = true,
    enable_stemming = true,
    language = 'english'
);

-- Drop inverted index
DROP INDEX idx_content_fts;

-- Full-text search query
SELECT * FROM documents WHERE content @@ 'database search';
```

### AST Additions

**File:** `include/scratchbird/parser/ast_v2.h`


**Logical Fields:**

- `store_positions` (bool): Store word positions
- `enable_stemming` (bool): Use Snowball stemmer
- `filter_stop_words` (bool): Remove stop words
- `language` (std::string): Language for stemmer
- `inverted_options` (InvertedIndexOptions): NEW: For INVERTED indexes
- `column_id` (uint16_t)
- `query` (std::string)


### Bytecode Opcodes

Extend CREATE_INDEX (0x50) for inverted indexes:

```cpp
// Bytecode format for CREATE INDEX ... USING inverted
// [CREATE_INDEX opcode]
// [index type = INVERTED_INDEX]
// [table name]
// [index name]
// [column ID]
// [options flags (uint8)]
// [language string]
```

### Bytecode Generation

```cpp
void BytecodeGeneratorV2::generateCreateIndex(ResolvedCreateIndexStmt* stmt) {
    // ... existing encoding ...

    // NEW: For INVERTED indexes, encode options
    if (stmt->index_type == IndexType::INVERTED_INDEX) {
        uint8_t flags = 0;
        if (stmt->inverted_options.store_positions) flags |= 0x01;
        if (stmt->inverted_options.enable_stemming) flags |= 0x02;
        if (stmt->inverted_options.filter_stop_words) flags |= 0x04;

        encodeUint8(flags, &bytecode_);
        encodeString(stmt->inverted_options.language, &bytecode_);
    }
}
```

### Executor Integration

```cpp
Status Executor::executeCreateIndex(const uint8_t* bytecode, size_t* offset,
                                   ErrorContext* ctx) {
    // ... decode common fields ...

    if (index_type == IndexType::INVERTED_INDEX) {
        // Decode options
        uint8_t flags = decodeUint8(bytecode, offset);
        std::string language = decodeString(bytecode, offset);

        InvertedIndexConfig config;
        config.store_positions = (flags & 0x01) != 0;
        config.enable_stemming = (flags & 0x02) != 0;
        config.filter_stop_words = (flags & 0x04) != 0;
        config.language = language;

        // Create inverted index
        auto* catalog = db_->getCatalogManager();
        auto table = catalog->getTable(table_name, ctx);
        if (!table) {
            return Status::TABLE_NOT_FOUND;
        }

        uint32_t meta_page = 0;
        auto status = InvertedIndex::create(db_, table, column_id, config, &meta_page, ctx);
        if (status != Status::OK) {
            return status;
        }

        // Register in catalog
        catalog->registerIndex(table_name, index_name, IndexType::INVERTED_INDEX, meta_page, ctx);

        LOG_INFO(Category::EXECUTOR, "Created inverted index '%s' on %s.%s",
                index_name.c_str(), table_name.c_str(), column_name.c_str());
    }

    return Status::OK;
}
```

---

## Required Implementation Steps

### Baseline: Core Infrastructure

1. **Page structures and storage**
   - Define all page structures with flexible arrays
   - Implement page-size aware helpers
   - Meta page, segment meta, dictionary, posting pages
   - Document stats pages

2. **Term dictionary**
   - Binary search implementation
   - Term insertion and lookup
   - Page chaining for large dictionaries

3. **Posting list management**
   - VByte compression codec
   - Delta encoding/decoding
   - Posting list serialization

### Tokenization and Analysis (Required)

1. **Basic tokenizer**
   - ASCII tokenization
   - Whitespace and punctuation splitting

2. **Token filters**
   - Lowercase filter
   - Stop word filter
   - Length filter

3. **Snowball stemmer integration**
   - Library integration
   - Multi-language support
   - Error handling

### Indexing (Required)

1. **Document indexing**
   - Analyze text and extract terms
   - Build in-memory posting lists
   - Flush to disk (active segment)

2. **DML integration**
   - INSERT hook
   - UPDATE hook (delete + reindex)
   - DELETE hook (mark deleted)

3. **Batch indexing**
   - Bulk insert optimization
   - Progress tracking

### Query Processing (Required)

1. **Query parser**
   - Boolean query parsing (AND, OR, NOT)
   - Term extraction

2. **Posting list operations**
   - Intersection (AND)
   - Union (OR)
   - Multi-segment merging

3. **MGA visibility filtering**
   - TIP integration
   - Visible document filtering

4. **BM25 scoring**
   - Score calculation
   - Result ranking

### Segment Management (Required)

1. **Segment creation and management**
   - New segment allocation
   - Active segment switching

2. **Merge policy**
   - Tiered merge policy
   - Segment selection

3. **Merge execution**
   - Term dictionary merging
   - Posting list merging
   - Cleanup of old segments

### Query Planner Integration (Required)

1. **Full-text search operator**
   - `@@` operator support
   - Plan generation

2. **Executor integration**
   - Execute inverted index scan
   - Result materialization

### Bytecode Integration (Required)

1. **Parser**
   - CREATE INDEX USING inverted syntax
   - Options parsing

2. **Bytecode generation**
   - Encode/decode options

3. **Executor**
   - Create inverted index from SQL

### Testing and Optimization (Required)

1. **Unit tests**
2. **Integration tests**

**Total:** 124-180 hours (15-22 days full-time)

---

## Testing Requirements

### Unit Tests

**File:** `tests/unit/test_inverted_index.cpp`

```cpp
TEST(InvertedIndexTest, TokenizationBasic) {
    StandardTokenizer tokenizer;
    auto tokens = tokenizer.tokenize("The quick brown fox jumps");

    EXPECT_EQ(tokens.size(), 5);
    EXPECT_EQ(tokens[0], "The");
    EXPECT_EQ(tokens[1], "quick");
    EXPECT_EQ(tokens[2], "brown");
}

TEST(InvertedIndexTest, LowercaseFilter) {
    StandardTokenizer tokenizer;
    auto tokens = tokenizer.tokenize("The Quick BROWN Fox");

    LowercaseFilter lowercase;
    tokens = lowercase.filter(tokens);

    EXPECT_EQ(tokens[0], "the");
    EXPECT_EQ(tokens[1], "quick");
    EXPECT_EQ(tokens[2], "brown");
}

TEST(InvertedIndexTest, StopWordFilter) {
    std::set<std::string> stop_words = {"the", "a", "an"};
    StopWordFilter filter(stop_words);

    auto tokens = filter.filter({"the", "quick", "fox"});

    EXPECT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "quick");
    EXPECT_EQ(tokens[1], "fox");
}

TEST(InvertedIndexTest, SnowballStemming) {
    SnowballStemFilter stemmer("english");

    auto tokens = stemmer.filter({"running", "databases", "searching"});

    EXPECT_EQ(tokens[0], "run");
    EXPECT_EQ(tokens[1], "databas");
    EXPECT_EQ(tokens[2], "search");
}

TEST(InvertedIndexTest, VByteEncoding) {
    std::vector<uint32_t> values = {1, 127, 128, 255, 256, 16384};

    auto encoded = VByteCodec::encode(values);
    auto decoded = VByteCodec::decode(encoded.data(), encoded.size());

    EXPECT_EQ(decoded, values);
}

TEST(InvertedIndexTest, DeltaEncoding) {
    std::vector<uint32_t> doc_ids = {5, 12, 15, 17, 25};

    auto deltas = deltaEncode(doc_ids);
    EXPECT_EQ(deltas[0], 5);
    EXPECT_EQ(deltas[1], 7);  // 12 - 5
    EXPECT_EQ(deltas[2], 3);  // 15 - 12

    auto decoded = deltaDecode(deltas);
    EXPECT_EQ(decoded, doc_ids);
}

TEST(InvertedIndexTest, PostingListIntersection) {
    std::vector<uint32_t> list1 = {1, 3, 5, 7, 9};
    std::vector<uint32_t> list2 = {3, 5, 8, 9};

    auto result = intersect(list1, list2);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 3);
    EXPECT_EQ(result[1], 5);
    EXPECT_EQ(result[2], 9);
}

TEST(InvertedIndexTest, BM25Scoring) {
    BM25Scorer scorer(1000, 100);  // 1000 docs, avg length 100

    std::vector<std::string> query_terms = {"database"};
    DocumentStats doc_stats{1, 120, 80, 0};
    std::map<std::string, uint32_t> tf = {{"database", 3}};
    std::map<std::string, uint32_t> df = {{"database", 50}};

    float score = scorer.score(query_terms, 1, doc_stats, tf, df);

    EXPECT_GT(score, 0.0f);
}

TEST(InvertedIndexTest, PageSizeAdaptation) {
    std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 65536, 131072};

    for (uint32_t page_size : page_sizes) {
        uint32_t terms_per_page = maxTermsPerPage(page_size);
        uint32_t expected = (page_size - 80) / 128;
        EXPECT_EQ(terms_per_page, expected);
    }
}
```

### Integration Tests

**File:** `tests/integration/test_inverted_index_search.cpp`

```cpp
TEST(InvertedIndexSearchTest, BasicSearch) {
    auto db = createTestDatabase();

    executeSQL(db.get(), R"(
        CREATE TABLE documents (
            doc_id INT PRIMARY KEY,
            content TEXT
        )
    )");

    // Insert documents
    executeSQL(db.get(), "INSERT INTO documents VALUES (1, 'database systems')");
    executeSQL(db.get(), "INSERT INTO documents VALUES (2, 'search engines')");
    executeSQL(db.get(), "INSERT INTO documents VALUES (3, 'database search')");

    // Create inverted index
    executeSQL(db.get(), "CREATE INDEX idx_content ON documents USING inverted(content)");

    // Search for "database"
    auto results = executeSQL(db.get(), "SELECT * FROM documents WHERE content @@ 'database'");

    EXPECT_EQ(results.size(), 2);  // Documents 1 and 3
}

TEST(InvertedIndexSearchTest, BooleanAND) {
    auto db = createTestDatabase();

    executeSQL(db.get(), R"(
        CREATE TABLE documents (
            doc_id INT PRIMARY KEY,
            content TEXT
        )
    )");

    executeSQL(db.get(), "INSERT INTO documents VALUES (1, 'database systems')");
    executeSQL(db.get(), "INSERT INTO documents VALUES (2, 'search engines')");
    executeSQL(db.get(), "INSERT INTO documents VALUES (3, 'database search optimization')");

    executeSQL(db.get(), "CREATE INDEX idx_content ON documents USING inverted(content)");

    // Search for "database AND search"
    auto results = executeSQL(db.get(), "SELECT * FROM documents WHERE content @@ 'database search'");

    EXPECT_EQ(results.size(), 1);  // Only document 3
}

TEST(InvertedIndexSearchTest, RankingBM25) {
    auto db = createTestDatabase();

    executeSQL(db.get(), R"(
        CREATE TABLE documents (
            doc_id INT PRIMARY KEY,
            content TEXT
        )
    )");

    executeSQL(db.get(), "INSERT INTO documents VALUES (1, 'database')");  // Short, high relevance
    executeSQL(db.get(), "INSERT INTO documents VALUES (2, 'database database database')");  // Higher TF
    executeSQL(db.get(), "INSERT INTO documents VALUES (3, 'systems')");  // No match

    executeSQL(db.get(), "CREATE INDEX idx_content ON documents USING inverted(content)");

    auto results = executeSQL(db.get(),
                             "SELECT doc_id, ts_rank(content, 'database') AS score "
                             "FROM documents WHERE content @@ 'database' "
                             "ORDER BY score DESC");

    EXPECT_EQ(results.size(), 2);
    // Document 2 should rank higher (more occurrences)
    EXPECT_EQ(results[0].getInt(0), 2);
}
```

---

## Performance Targets

### Indexing Performance

- **Indexing speed:** 10-50 MB/s of text
- **Document insertion:** < 10ms per document (single)
- **Batch insertion:** 1,000-10,000 documents/second

### Query Performance

- **Single term query:** 1-10ms
- **Multi-term query (AND):** 5-50ms
- **Ranked search (top 10):** 10-100ms
- **Throughput:** 100-1,000 queries/second

### Space Efficiency

- **Index size:** 20-40% of original text
- **With compression:** 4-10x compression for posting lists
- **Memory overhead:** < 100MB for 1M documents

### Page Size Impact

| Page Size | Terms/Page | Impact |
|-----------|----------|--------|
| 8 KB      | 63       | More pages, more I/O|
| 16 KB     | 127      | Balanced |
| 32 KB     | 255      | Good for large indexes |
| 64 KB+    | 511+     | Best for very large indexes |

---

## Reserved Enhancements (Not Supported in V3)

### Required Features

1. **Phrase Queries**
   - Store word positions
   - Phrase matching ("database systems")

2. **Wildcard and Fuzzy Search**
   - Prefix queries (data*)
   - Edit distance matching

3. **Advanced Compression**
   - PForDelta (2-4x better than VByte)
   - SIMD-accelerated decompression

4. **Proximity Queries**
   - NEAR operator
   - Positional scoring

5. **Field Weighting**
   - Boost title vs content
   - Custom field weights

6. **Faceted Search**
   - Category filtering
   - Aggregations

7. **Highlighting**
   - Return matching snippets
   - Context extraction

8. **Real-time Updates**
   - NRT (Near Real-Time) search
   - Faster segment refresh

---

## Conclusion

This specification provides a complete, implementation-ready design for Inverted Indexes in ScratchBird.

**Key Features:**
- ✅ Segment-based architecture (immutable, mergeable)
- ✅ **Page-size agnostic** (supports 8KB, 16KB, 32KB, 64KB, 128KB pages)
- ✅ MGA-compliant (lazy visibility checking with TIP)
- ✅ Full tokenization pipeline (tokenizer, filters, stemming)
- ✅ Snowball stemmer integration (40+ languages)
- ✅ VByte compression (4-10x for posting lists)
- ✅ BM25 scoring (industry standard)
- ✅ Boolean queries (AND, OR, NOT)
- ✅ Tiered merge policy (automatic segment merging)
- ✅ SQL syntax: CREATE INDEX USING inverted(column)
- ✅ Full-text search operator (@@)
- ✅ Runtime capacity calculations

**Implementation Effort:** 124-180 hours (15-22 days full-time)

**Risk Level:** HIGH - Complex multi-component system, external dependencies

**Performance:** 100-10,000x speedup vs LIKE queries

**Page Size Flexibility:**
- Small pages (8KB): More I/O, suitable for small indexes
- Large pages (64KB+): Fewer pages, better for large indexes (10M+ documents)

**Next Steps:**
1. Review this specification
2. Implement Baseline (core infrastructure)
3. Implement tokenization and analysis
4. Implement indexing
5. Benchmark on real-world document collections

---

**Specification Status:** READY FOR IMPLEMENTATION (v1.0)
**Reviewer:** Please provide feedback on:
- Segment-based architecture vs monolithic
- Snowball stemmer dependency
- VByte vs PForDelta compression (Baseline)
- MGA compliance approach (lazy visibility)
- Missing considerations or edge cases

**Author:** ScratchBird Architecture Team
**Date:** November 20, 2025
**Version:** 1.0

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.

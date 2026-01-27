# ScratchBird Index Implementation Specification
## Comprehensive Technical Specification for Index Subsystem

---

## IMPLEMENTATION STATUS: 🟡 MOSTLY COMPLETE (11/12 core; 0/15 optional)

**Last Updated:** January 22, 2026

**Completed Core Index Types (11/12 core = 92%):**
- ✅ **B-Tree Index** - Production ready with prefix compression and GC
  - Location: `src/core/btree.cpp`
- ✅ **Hash Index** - Extendible hashing with GC
  - Location: `src/core/hash_index.cpp`
- ✅ **GIN Index** - Posting lists, GC, and DML integration
  - Location: `src/core/gin_index.cpp`
- ✅ **GiST Index** - Operator-class based search tree
  - Location: `src/core/gist_index.cpp`
- ✅ **SP-GiST Index** - Space-partitioned trees
  - Location: `src/core/spgist_index.cpp`
- ✅ **BRIN Index** - Block range summaries
  - Location: `src/core/brin_index.cpp`
- ✅ **R-Tree Index** - Spatial indexing
  - Location: `src/core/rtree_index.cpp`, `src/core/rtree.cpp`
- ✅ **Bitmap Index** - Versioned roaring bitmap
  - Location: `src/core/bitmap_index.cpp`
- ✅ **HNSW Index** - Vector similarity search
  - Location: `src/core/hnsw_index.cpp`
- ✅ **Columnstore Index** - Row-buffered columnar storage
  - Location: `src/core/columnstore_index.cpp`
- ✅ **LSM-Tree Index** - File-based LSM with compaction
  - Location: `src/core/lsm_tree_index.cpp`

**Partial Implementation (1/12 core):**
- ⚠️ **FULLTEXT (Inverted) Index** - DML and search implemented; GC stub
  - Location: `src/core/inverted_index.cpp`
  - Gap: `InvertedIndex::removeDeadEntries` returns NOT_IMPLEMENTED

**Integration Gaps (core):**
- ❌ **Index GC wiring** in `src/core/garbage_collector.cpp` is limited to BTREE/HASH/GIN/BRIN/HNSW.
- ⚠️ **GiST cache cleanup** in `src/sblr/index_cache.cpp` skips deletion due to incomplete type cleanup.
- ⚠️ **FULLTEXT vs GIN wrapper split**: `FullTextIndex` (GIN-based) exists but `IndexFactory` uses `InvertedIndex` for FULLTEXT.

### Remediation Items (Doc-Level)

1. **Implement FULLTEXT GC**
   - File: `src/core/inverted_index.cpp:3795`
   - Task: Implement `InvertedIndex::removeDeadEntries` to purge obsolete postings and update doc stats.

2. **Expand Index GC Coverage**
   - File: `src/core/garbage_collector.cpp:870-946`
   - Task: Add GC hooks for GIST, SPGIST, RTREE, BITMAP, COLUMNSTORE, FULLTEXT, LSM.

**Optional/Advanced Index Types (16/16 not implemented):**
- ❌ **IVF** - Vector similarity
- ❌ **ZONEMAP** - Min/max pruning
- ❌ **ZORDER** - Morton curve
- ❌ **GEOHASH** - Geo cell indexing
- ❌ **S2** - S2 cell indexing
- ❌ **QUADTREE** - 2D spatial partitioning
- ❌ **OCTREE** - 3D spatial partitioning
- ❌ **FST** - Prefix/autocomplete dictionary
- ❌ **SUFFIX_ARRAY** - Substring search
- ❌ **SUFFIX_TREE** - Substring search (tree mode)
- ❌ **COUNT_MIN_SKETCH** - Frequency estimation
- ❌ **HYPERLOGLOG** - Approx distinct counts
- ❌ **ART** - Adaptive radix tree
- ❌ **LEARNED** - RMI learned index
- ❌ **LSM_TTL** - Time-series retention
- ❌ **JSON_PATH** - JSON/JSONB path index

**Optional Specs:** See `docs/specifications/indexes/` for detailed implementation documents.

**Audit Report:** `/docs/status/ALPHA_003_AUDIT_FINDINGS.md`
**Progress Tracker:** `/docs/status/ALPHA_003_PROGRESS.md`

**Note:** Specification dated 2025-09-15 was outdated. Significant implementation work occurred after that date. This header updated 2025-10-13 based on comprehensive code audit.

---

**This specification reflects current implementation. Remaining work is FULLTEXT GC completion and GC wiring for non-covered index types.**

---

## Canonical Index Type Names (Parser/Catalog)

**Core (Alpha scope):** BTREE, HASH, FULLTEXT, GIN, GIST, SPGIST, BRIN, BITMAP, RTREE, HNSW, COLUMNSTORE, LSM

**Optional/Advanced (Beta scope):** IVF, ZONEMAP, ZORDER, GEOHASH, S2, QUADTREE, OCTREE, FST, SUFFIX_ARRAY, SUFFIX_TREE, COUNT_MIN_SKETCH, HYPERLOGLOG, ART, LEARNED, LSM_TTL, JSON_PATH

**DDL Note:** SQL uses lowercase keywords in `USING` clauses (e.g., `USING zorder`). Bloom filters are index options (`WITH (bloom_filter = true)`), not a standalone index type.

**Total Index Types:** 28 (12 core + 16 optional)

## Overview

ScratchBird's index implementation (PLANNED) will follow a hybrid approach combining Firebird's efficient B-Tree with prefix/suffix compression, PostgreSQL's multiple index types, and novel UUID-optimized structures. This specification aligns with Phase 11 (B-Tree Indexing) and Phase 13 (Query Optimization) of the implementation plan.

## 1. Core B-Tree Implementation

### 1.1 B-Tree Page Structure

```c
// ScratchBird B-Tree page structure (all page sizes supported)
typedef struct sb_btree_page {
    // Standard page header
    PageHeader      btr_header;         // Standard ScratchBird page header
    
    // Index identification
    UUID            btr_index_uuid;     // Index UUID v7 (not numeric ID)
    UUID            btr_table_uuid;     // Table this index belongs to
    
    // Tree structure
    uint16_t        btr_level;          // Level (0 = leaf, increases upward)
    uint16_t        btr_flags;          // Page flags (see below)
    uint16_t        btr_count;          // Number of entries in this page
    uint16_t        btr_free_space;     // Free space in bytes
    
    // Sibling navigation
    uint64_t        btr_left_sibling;   // Left sibling page number
    uint64_t        btr_right_sibling;  // Right sibling page number
    uint64_t        btr_parent_page;    // Parent page (for fast traversal)
    
    // Compression metadata
    uint16_t        btr_prefix_total;   // Total prefix compression bytes saved
    uint16_t        btr_suffix_total;   // Total suffix truncation bytes saved
    uint8_t         btr_compression;    // Compression type (see enum below)
    uint8_t         btr_min_prefix_len; // Minimum prefix length on page
    
    // Multi-version support for MGA
    TransactionId   btr_xmin;           // Page creation transaction
    TransactionId   btr_xmax;           // Page deletion transaction (0 if active)
    uint64_t        btr_lsn;            // Last LSN that modified this page
    
    // High water mark
    uint16_t        btr_high_water;     // Highest used offset in page
    
    // Variable-length node array starts here
    // Nodes are stored from end of page backward
    // Free space is between fixed header and nodes
} SBBTreePage;

// Page flags
#define BTR_FLAG_LEAF           0x0001  // Leaf page
#define BTR_FLAG_ROOT           0x0002  // Root page
#define BTR_FLAG_RIGHTMOST      0x0004  // Rightmost page at this level
#define BTR_FLAG_LEFTMOST       0x0008  // Leftmost page at this level
#define BTR_FLAG_COMPRESSED     0x0010  // Compression enabled
#define BTR_FLAG_ENCRYPTED      0x0020  // Page is encrypted
#define BTR_FLAG_HAS_GARBAGE    0x0040  // Page has deleted entries
#define BTR_FLAG_INCOMPLETE     0x0080  // Split in progress

// Compression types
enum BTreeCompressionType {
    BTR_COMPRESS_NONE = 0,
    BTR_COMPRESS_PREFIX = 1,        // Prefix compression only
    BTR_COMPRESS_SUFFIX = 2,        // Suffix truncation only
    BTR_COMPRESS_BOTH = 3,          // Both prefix and suffix
    BTR_COMPRESS_ZSTD = 4,          // Page-level zstd compression
    BTR_COMPRESS_ADAPTIVE = 5       // Choose best per page
};
```

### 1.2 B-Tree Node Structure

```c
// B-Tree node structure (variable length)
typedef struct sb_btree_node {
    // Node header (fixed part)
    uint16_t        btn_flags;          // Node flags
    uint16_t        btn_prefix_len;     // Prefix compression length
    uint16_t        btn_suffix_trunc;   // Suffix truncation length
    uint16_t        btn_key_len;        // Actual key length (after compression)
    
    // For leaf nodes
    uint32_t        btn_tuple_count;    // Number of tuples (for duplicates)
    
    // For internal nodes
    uint64_t        btn_child_page;     // Child page number (left of this key)
    
    // Multi-version support
    TransactionId   btn_xmin;           // Node creation transaction
    TransactionId   btn_xmax;           // Node deletion transaction
    
    // Variable length data follows
    // [key_data][tuple_ids or child_pointer]
} SBBTreeNode;

// Node flags
#define BTN_FLAG_DELETED        0x0001  // Logically deleted
#define BTN_FLAG_HAS_DUPLICATES 0x0002  // Multiple tuple IDs
#define BTN_FLAG_FIRST_ON_PAGE  0x0004  // First node on page (no prefix)
#define BTN_FLAG_LAST_ON_PAGE   0x0008  // Last node on page
#define BTN_FLAG_NULL_KEY       0x0010  // NULL key value
#define BTN_FLAG_INFINITY       0x0020  // Positive infinity (rightmost)
```

### 1.3 B-Tree Operations

```c
// B-Tree interface
typedef struct sb_btree_index {
    UUID            idx_uuid;           // Index UUID
    UUID            idx_table_uuid;     // Table UUID
    uint16_t        idx_column_count;   // Number of columns
    uint16_t*       idx_column_ids;     // Column IDs in index
    uint32_t        idx_flags;          // Index flags (unique, etc.)
    
    // Root page tracking
    uint64_t        idx_root_page;      // Current root page
    uint16_t        idx_height;         // Tree height
    
    // Statistics
    uint64_t        idx_tuple_count;    // Number of indexed tuples
    uint64_t        idx_page_count;     // Number of pages
    uint64_t        idx_deleted_count;  // Deleted but not cleaned
    
    // Methods
    BTreeMethods*   idx_methods;        // Function pointers
} SBBTreeIndex;

// B-Tree methods
typedef struct btree_methods {
    // Insert operation
    Status (*insert)(SBBTreeIndex* index, 
                    const Datum* key_values,
                    uint16_t n_keys,
                    TupleId tid,
                    TransactionId xid);
    
    // Delete operation
    Status (*delete)(SBBTreeIndex* index,
                    const Datum* key_values,
                    uint16_t n_keys,
                    TupleId tid,
                    TransactionId xid);
    
    // Search operations
    TupleId (*find_equal)(SBBTreeIndex* index,
                         const Datum* key_values,
                         uint16_t n_keys,
                         Snapshot* snapshot);
    
    IndexScanDesc (*begin_scan)(SBBTreeIndex* index,
                               ScanKey* keys,
                               uint16_t n_keys,
                               ScanDirection direction);
    
    bool (*get_next)(IndexScanDesc scan,
                    TupleId* tid);
    
    void (*end_scan)(IndexScanDesc scan);
    
    // Maintenance (sweep/GC)
    void (*vacuum)(SBBTreeIndex* index,
                  TransactionId oldest_xid);
    
    IndexStats* (*get_statistics)(SBBTreeIndex* index);
} BTreeMethods;
```

### 1.4 B-Tree Page Split Algorithm

```c
// Enhanced page split with optimal split point selection
SBBTreePage* btree_split_page(
    SBBTreeIndex* index,
    SBBTreePage* page,
    SBBTreeNode* new_node,
    BufferPool* buffer_pool)
{
    // Step 1: Determine optimal split point
    uint32_t total_size = btree_page_used_space(page);
    uint32_t target_size = (get_page_size() - sizeof(SBBTreePage)) / 2;
    
    // Find split point that best balances page sizes
    uint16_t split_index = 0;
    uint32_t left_size = 0;
    
    for (uint16_t i = 0; i < page->btr_count; i++) {
        SBBTreeNode* node = btree_get_node(page, i);
        uint32_t node_size = btree_node_size(node);
        
        if (left_size + node_size > target_size) {
            // Check if splitting here or at next position is better
            uint32_t diff_here = abs(left_size + node_size - target_size);
            uint32_t diff_next = abs(left_size - target_size);
            
            split_index = (diff_here < diff_next) ? i + 1 : i;
            break;
        }
        left_size += node_size;
    }
    
    // Step 2: Allocate new page
    uint64_t new_page_num = allocate_page(buffer_pool);
    SBBTreePage* new_page = (SBBTreePage*) get_page(buffer_pool, new_page_num);
    
    // Step 3: Initialize new page
    btree_init_page(new_page, page->btr_level, page->btr_index_uuid);
    
    // Step 4: Move nodes to new page
    btree_move_nodes(page, new_page, split_index, page->btr_count);
    
    // Step 5: Update sibling pointers
    new_page->btr_left_sibling = get_page_number(page);
    new_page->btr_right_sibling = page->btr_right_sibling;
    page->btr_right_sibling = new_page_num;
    
    if (new_page->btr_right_sibling != 0) {
        SBBTreePage* right_sibling = get_page(buffer_pool, 
                                              new_page->btr_right_sibling);
        right_sibling->btr_left_sibling = new_page_num;
        mark_dirty(buffer_pool, new_page->btr_right_sibling);
    }
    
    // Step 6: Recompress both pages
    btree_recompress_page(page);
    btree_recompress_page(new_page);
    
    // Step 7: Insert separator key in parent
    if (!(page->btr_flags & BTR_FLAG_ROOT)) {
        btree_insert_parent(index, page, new_page, 
                           btree_get_separator_key(page, new_page));
    } else {
        // Root split - create new root
        btree_split_root(index, page, new_page);
    }
    
    // Step 8: Mark pages dirty
    mark_dirty(buffer_pool, get_page_number(page));
    mark_dirty(buffer_pool, new_page_num);
    
    return new_page;
}
```

### 1.5 Prefix/Suffix Compression

```c
// Prefix compression for keys
typedef struct prefix_compressor {
    uint8_t*        last_key;           // Last key for comparison
    uint16_t        last_key_len;       // Last key length
    uint16_t        min_prefix_len;     // Minimum prefix to keep
} PrefixCompressor;

// Calculate prefix length between two keys
uint16_t calculate_prefix_length(
    const uint8_t* key1, uint16_t len1,
    const uint8_t* key2, uint16_t len2)
{
    uint16_t min_len = MIN(len1, len2);
    uint16_t prefix_len = 0;
    
    while (prefix_len < min_len && 
           key1[prefix_len] == key2[prefix_len]) {
        prefix_len++;
    }
    
    return prefix_len;
}

// Compress key using prefix compression
CompressedKey* compress_key(
    PrefixCompressor* compressor,
    const uint8_t* key,
    uint16_t key_len)
{
    uint16_t prefix_len = 0;
    
    if (compressor->last_key != NULL) {
        prefix_len = calculate_prefix_length(
            compressor->last_key, compressor->last_key_len,
            key, key_len);
        
        // Don't compress too much - keep minimum prefix
        prefix_len = MIN(prefix_len, 
                        key_len - compressor->min_prefix_len);
    }
    
    // Allocate compressed key
    uint16_t suffix_len = key_len - prefix_len;
    CompressedKey* compressed = allocate(
        sizeof(CompressedKey) + suffix_len);
    
    compressed->prefix_len = prefix_len;
    compressed->suffix_len = suffix_len;
    memcpy(compressed->suffix, key + prefix_len, suffix_len);
    
    // Update last key
    if (compressor->last_key_len < key_len) {
        compressor->last_key = realloc(compressor->last_key, key_len);
    }
    memcpy(compressor->last_key, key, key_len);
    compressor->last_key_len = key_len;
    
    return compressed;
}
```

## 2. UUID-Optimized B-Tree

### 2.1 UUID v7 Optimization

```c
// Special handling for UUID v7 (time-ordered) keys
typedef struct uuid_btree_config {
    bool            optimize_uuid_v7;   // Enable UUID v7 optimizations
    uint32_t        time_partition_ms;  // Time-based partitioning
    uint16_t        prefix_bytes;       // Bytes to use for prefix (timestamp)
} UUIDBTreeConfig;

// UUID v7 aware comparison
int compare_uuid_v7(const UUID* uuid1, const UUID* uuid2) {
    // First compare timestamp portions (first 48 bits)
    uint64_t time1 = extract_uuid_timestamp(uuid1);
    uint64_t time2 = extract_uuid_timestamp(uuid2);
    
    if (time1 != time2) {
        return (time1 < time2) ? -1 : 1;
    }
    
    // Then compare remaining bytes
    return memcmp(uuid1->bytes + 6, uuid2->bytes + 6, 10);
}

// UUID-optimized page structure
typedef struct uuid_btree_page {
    SBBTreePage     base;               // Standard B-Tree page
    
    // UUID-specific optimizations
    uint64_t        time_range_start;   // Minimum timestamp in page
    uint64_t        time_range_end;     // Maximum timestamp in page
    uint16_t        time_prefix_len;    // Common time prefix length
    uint8_t         time_prefix[6];     // Common time prefix
} UUIDBTreePage;
```

## 3. Additional Index Types

### 3.1 Hash Index

```c
// Hash index for equality searches
typedef struct sb_hash_index {
    UUID            idx_uuid;           // Index UUID
    uint32_t        idx_bucket_count;   // Number of hash buckets
    uint64_t*       idx_bucket_pages;   // Bucket page numbers
    HashFunction    idx_hash_func;      // Hash function
    
    // Adaptive features
    uint32_t        idx_split_threshold; // When to split bucket
    uint32_t        idx_merge_threshold; // When to merge buckets
    bool            idx_dynamic_resize;  // Auto-resize enabled
} SBHashIndex;

// Hash bucket page
typedef struct hash_bucket_page {
    PageHeader      hbp_header;         // Standard page header
    uint32_t        hbp_bucket_id;      // Bucket number
    uint16_t        hbp_entry_count;    // Number of entries
    uint64_t        hbp_overflow_page;  // Overflow page if full
    
    // Entries stored as array
    HashEntry       hbp_entries[];      // Variable length
} HashBucketPage;
```

### 3.2 Bitmap Index

```c
// Bitmap index for low-cardinality columns
typedef struct sb_bitmap_index {
    UUID            idx_uuid;           // Index UUID
    uint32_t        idx_distinct_values; // Number of distinct values
    
    // Bitmap segments per value
    struct {
        Datum       value;              // The indexed value
        uint64_t    bitmap_page;        // Start of bitmap pages
        uint64_t    tuple_count;        // Number of set bits
    } idx_bitmaps[];
} SBBitmapIndex;

// Bitmap page
typedef struct bitmap_page {
    PageHeader      bmp_header;         // Standard page header
    uint64_t        bmp_start_tid;      // Starting tuple ID
    uint32_t        bmp_word_count;     // Number of bitmap words
    
    // Compression
    uint8_t         bmp_compression;    // Compression type
    uint32_t        bmp_compressed_size; // If compressed
    
    // Bitmap data
    uint64_t        bmp_words[];        // Bitmap words (or compressed)
} BitmapPage;
```

### 3.3 GIN (Generalized Inverted Index)

```c
// GIN index for full-text search and arrays
typedef struct sb_gin_index {
    UUID            idx_uuid;           // Index UUID
    
    // Entry tree (B-Tree of unique values)
    uint64_t        idx_entry_root;     // Root of entry tree
    
    // Posting trees (B-Trees of tuple IDs per value)
    uint64_t        idx_posting_root;   // Root of posting tree
    
    // Pending list for fast inserts
    uint64_t        idx_pending_head;   // Head of pending list
    uint64_t        idx_pending_tail;   // Tail of pending list
    uint32_t        idx_pending_count;  // Number of pending entries
} SBGINIndex;
```

### 3.4 Optional Advanced Index Types (Beta)

These index types are fully specified and ready for implementation, but are optional/beta scope in the index roadmap:

- **IVF** (vector similarity) - `docs/specifications/indexes/IVFIndex.md`
- **ZONEMAP** (min/max pruning) - `docs/specifications/indexes/ZoneMapsIndex.md`
- **ZORDER** (Morton) - `docs/specifications/indexes/ZOrderIndex.md`
- **GEOHASH**, **S2** - `docs/specifications/indexes/GeohashS2Index.md`
- **QUADTREE**, **OCTREE** - `docs/specifications/indexes/QuadtreeOctreeIndex.md`
- **FST** - `docs/specifications/indexes/FSTIndex.md`
- **SUFFIX_ARRAY**, **SUFFIX_TREE** - `docs/specifications/indexes/SuffixIndex.md`
- **COUNT_MIN_SKETCH** - `docs/specifications/indexes/CountMinSketchIndex.md`
- **HYPERLOGLOG** - `docs/specifications/indexes/HyperLogLogIndex.md`
- **ART** - `docs/specifications/indexes/AdaptiveRadixTreeIndex.md`
- **LEARNED** - `docs/specifications/indexes/LearnedIndex.md`
- **LSM_TTL** - `docs/specifications/indexes/LSMTimeSeriesIndex.md`

## 4. Index Scan Implementation

### 4.1 Index Scan Descriptor

```c
// Index scan state
typedef struct index_scan_desc {
    // Index information
    SBBTreeIndex*   isd_index;          // Index being scanned
    
    // Scan keys
    ScanKey*        isd_keys;           // Array of scan keys
    uint16_t        isd_nkeys;          // Number of keys
    
    // Scan state
    ScanDirection   isd_direction;      // Forward or backward
    BufferDesc      isd_current_buf;    // Current buffer
    uint16_t        isd_current_pos;    // Position in current page
    ItemPointer     isd_current_tid;    // Current tuple ID
    
    // Scan boundaries
    bool            isd_have_lower;     // Have lower bound
    bool            isd_have_upper;     // Have upper bound
    Datum*          isd_lower_bound;    // Lower bound values
    Datum*          isd_upper_bound;    // Upper bound values
    
    // Statistics
    uint64_t        isd_tuples_fetched; // Tuples returned
    uint64_t        isd_pages_fetched;  // Pages accessed
    
    // Kill tuple support (mark dead tuples during scan)
    bool            isd_kill_prior_tuple;
    ItemPointer     isd_killed_tuples;  // List of killed tuples
} IndexScanDesc;

// Scan key for index conditions
typedef struct scan_key {
    uint16_t        sk_attno;           // Attribute number
    StrategyNumber  sk_strategy;        // Comparison strategy
    Datum           sk_argument;        // Comparison value
    FmgrInfo        sk_func;            // Comparison function
    uint32_t        sk_flags;           // Flags
} ScanKey;
```

### 4.2 Index Scan Execution

```c
// Begin index scan
IndexScanDesc* index_begin_scan(
    Relation rel,
    SBBTreeIndex* index,
    Snapshot* snapshot,
    uint16_t nkeys,
    ScanKey* keys)
{
    IndexScanDesc* scan = allocate(sizeof(IndexScanDesc));
    
    scan->isd_index = index;
    scan->isd_keys = keys;
    scan->isd_nkeys = nkeys;
    scan->isd_direction = ForwardScanDirection;
    
    // Find starting position
    btree_find_start_position(scan);
    
    // Set up scan boundaries
    btree_setup_scan_bounds(scan);
    
    return scan;
}

// Get next tuple from index scan
bool index_get_next(IndexScanDesc* scan, TupleId* tid) {
    while (true) {
        // Get next index entry
        if (!btree_get_next_entry(scan)) {
            return false;  // End of scan
        }
        
        // Check visibility
        if (tuple_satisfies_snapshot(scan->isd_current_tid, 
                                    scan->snapshot)) {
            *tid = scan->isd_current_tid;
            
            // Check additional conditions
            if (recheck_index_conditions(scan, tid)) {
                scan->isd_tuples_fetched++;
                return true;
            }
        }
        
        // Mark tuple as dead if invisible to all
        if (scan->isd_kill_prior_tuple && 
            tuple_dead_to_all(scan->isd_current_tid)) {
            btree_mark_tuple_dead(scan->isd_index, 
                                scan->isd_current_tid);
        }
    }
}
```

## 5. Index Maintenance

### 5.1 Vacuum and Cleanup

```c
// Index sweep/GC to remove dead tuples
void index_vacuum(
    SBBTreeIndex* index,
    TransactionId oldest_xid,
    TupleId* dead_tuples,
    uint64_t num_dead_tuples)
{
    // Phase 1: Mark dead tuples in index
    for (uint64_t i = 0; i < num_dead_tuples; i++) {
        btree_mark_dead(index, dead_tuples[i]);
    }
    
    // Phase 2: Cleanup empty pages
    btree_cleanup_empty_pages(index);
    
    // Phase 3: Try to merge underfull pages
    btree_merge_pages(index);
    
    // Phase 4: Update statistics
    index_update_statistics(index);
}

// Page merge for underfull pages
void btree_merge_pages(SBBTreeIndex* index) {
    // Scan for underfull pages
    PageList* underfull = btree_find_underfull_pages(index);
    
    for (Page* page : underfull) {
        // Try to merge with sibling
        if (page->btr_left_sibling != 0) {
            Page* left = get_page(page->btr_left_sibling);
            if (can_merge(left, page)) {
                btree_merge_right_into_left(left, page);
                free_page(page);
                continue;
            }
        }
        
        if (page->btr_right_sibling != 0) {
            Page* right = get_page(page->btr_right_sibling);
            if (can_merge(page, right)) {
                btree_merge_right_into_left(page, right);
                free_page(right);
            }
        }
    }
}
```

### 5.2 Index Statistics

```c
// Index statistics for optimizer
typedef struct index_statistics {
    UUID            stat_index_uuid;    // Index UUID
    
    // Size statistics
    uint64_t        stat_num_pages;     // Total pages
    uint64_t        stat_num_tuples;    // Total tuples
    uint64_t        stat_num_entries;   // Total entries (with duplicates)
    
    // Tree statistics
    uint16_t        stat_tree_height;   // Tree height
    double          stat_avg_fanout;    // Average fanout
    double          stat_fill_factor;   // Average page fill
    
    // Key statistics
    double          stat_distinct_keys; // Estimated distinct keys
    double          stat_avg_key_size;  // Average key size
    double          stat_null_fraction; // Fraction of NULL keys
    
    // Performance statistics
    double          stat_correlation;   // Correlation with table order
    uint64_t        stat_leaf_pages;    // Number of leaf pages
    uint64_t        stat_internal_pages; // Number of internal pages
    
    // Compression statistics
    uint64_t        stat_compressed_size; // Total compressed size
    uint64_t        stat_uncompressed_size; // Uncompressed size
    double          stat_compression_ratio; // Compression ratio
} IndexStatistics;

// Collect index statistics
IndexStatistics* index_analyze(SBBTreeIndex* index) {
    IndexStatistics* stats = allocate(sizeof(IndexStatistics));
    
    // Scan index to collect statistics
    IndexScanState scan;
    btree_start_stats_scan(index, &scan);
    
    while (btree_next_stats_page(&scan)) {
        SBBTreePage* page = scan.current_page;
        
        stats->stat_num_pages++;
        
        if (page->btr_flags & BTR_FLAG_LEAF) {
            stats->stat_leaf_pages++;
            stats->stat_num_entries += page->btr_count;
            
            // Sample keys for statistics
            sample_page_keys(page, stats);
        } else {
            stats->stat_internal_pages++;
        }
        
        // Calculate fill factor
        double fill = (double)page_used_space(page) / 
                     get_page_size();
        stats->stat_fill_factor += fill;
        
        // Track compression
        stats->stat_compressed_size += page->btr_high_water;
        stats->stat_uncompressed_size += 
            calculate_uncompressed_size(page);
    }
    
    // Finalize statistics
    stats->stat_fill_factor /= stats->stat_num_pages;
    stats->stat_compression_ratio = 
        (double)stats->stat_compressed_size / 
        stats->stat_uncompressed_size;
    
    return stats;
}
```

## 6. Index Creation and Management

### 6.1 CREATE INDEX Implementation

```c
// Create index on table
Status create_index(
    const char* index_name,
    UUID table_uuid,
    uint16_t* column_ids,
    uint16_t num_columns,
    uint32_t index_flags)
{
    // Allocate index structure
    SBBTreeIndex* index = allocate(sizeof(SBBTreeIndex));
    index->idx_uuid = generate_uuid_v7();
    index->idx_table_uuid = table_uuid;
    index->idx_column_count = num_columns;
    index->idx_column_ids = copy_array(column_ids, num_columns);
    index->idx_flags = index_flags;
    
    // Allocate root page
    index->idx_root_page = allocate_page();
    SBBTreePage* root = get_page(index->idx_root_page);
    btree_init_page(root, 0, index->idx_uuid);
    root->btr_flags |= BTR_FLAG_ROOT | BTR_FLAG_LEAF;
    
    // Register in system catalog
    register_index_in_catalog(index, index_name);
    
    // Build index from existing data
    if (table_has_data(table_uuid)) {
        build_index_from_table(index, table_uuid);
    }
    
    return STATUS_OK;
}

// Build index from existing table data
void build_index_from_table(
    SBBTreeIndex* index,
    UUID table_uuid)
{
    // Use sort-based build for efficiency
    SortState* sorter = create_sorter(index->idx_column_count);
    
    // Scan table and add to sorter
    TableScan* scan = begin_table_scan(table_uuid);
    Tuple* tuple;
    
    while ((tuple = get_next_tuple(scan)) != NULL) {
        Datum* key_values = extract_index_keys(tuple, 
                                              index->idx_column_ids,
                                              index->idx_column_count);
        add_to_sorter(sorter, key_values, tuple->tid);
    }
    
    // Sort all entries
    perform_sort(sorter);
    
    // Build index from sorted data
    btree_build_from_sorted(index, sorter);
    
    // Cleanup
    destroy_sorter(sorter);
    end_table_scan(scan);
}
```

## 7. Integration with Query Optimizer

### 7.1 Index Cost Estimation

```c
// Estimate cost of index scan
typedef struct index_scan_cost {
    double          startup_cost;       // Cost to start scan
    double          total_cost;         // Total scan cost
    double          index_selectivity;  // Selectivity estimate
    uint64_t        estimated_rows;     // Estimated rows returned
} IndexScanCost;

IndexScanCost* estimate_index_scan_cost(
    SBBTreeIndex* index,
    ScanKey* keys,
    uint16_t nkeys,
    IndexStatistics* stats)
{
    IndexScanCost* cost = allocate(sizeof(IndexScanCost));
    
    // Calculate selectivity based on keys
    double selectivity = 1.0;
    for (uint16_t i = 0; i < nkeys; i++) {
        selectivity *= estimate_key_selectivity(&keys[i], stats);
    }
    
    // Estimate pages to fetch
    uint64_t index_pages = ceil(stats->stat_leaf_pages * selectivity);
    uint64_t table_pages = ceil(stats->stat_num_pages * 
                               selectivity * stats->stat_correlation);
    
    // Calculate costs
    cost->startup_cost = random_page_cost;  // First page
    cost->total_cost = cost->startup_cost +
                      (index_pages - 1) * seq_page_cost +  // Index pages
                      table_pages * random_page_cost +      // Table pages
                      stats->stat_num_tuples * selectivity * 
                      cpu_tuple_cost;                       // CPU
    
    cost->index_selectivity = selectivity;
    cost->estimated_rows = stats->stat_num_tuples * selectivity;
    
    return cost;
}
```

## 8. Concurrency Control

### 8.1 Index Locking

```c
// Fine-grained index locking
typedef struct index_lock {
    LockTag         lock_tag;           // Lock identifier
    LockMode        lock_mode;          // Lock mode
    TransactionId   lock_holder;        // Holding transaction
    
    // For page-level locks
    uint64_t        lock_page_num;      // Page number
    
    // For key-range locks
    Datum*          lock_key_start;     // Range start
    Datum*          lock_key_end;       // Range end
    bool            lock_exclusive_end; // Exclusive end point
} IndexLock;

// Acquire index lock
Status index_acquire_lock(
    SBBTreeIndex* index,
    IndexLock* lock,
    TransactionId xid,
    bool wait)
{
    // Check for conflicts
    if (index_has_conflicting_lock(index, lock, xid)) {
        if (!wait) {
            return STATUS_LOCK_NOT_AVAILABLE;
        }
        
        // Wait for lock
        index_wait_for_lock(index, lock, xid);
    }
    
    // Grant lock
    index_grant_lock(index, lock, xid);
    
    return STATUS_OK;
}
```

## 9. Performance Optimizations

### 9.1 Adaptive Index Selection

```c
// Monitor index usage and suggest new indexes
typedef struct index_advisor {
    // Workload statistics
    QueryPattern*   patterns;           // Common query patterns
    uint32_t        pattern_count;      // Number of patterns
    
    // Missing index detection
    MissingIndex*   missing_indexes;    // Suggested indexes
    uint32_t        missing_count;      // Number of suggestions
    
    // Unused index detection
    UUID*           unused_indexes;     // Rarely used indexes
    uint32_t        unused_count;       // Number of unused
} IndexAdvisor;

// Analyze workload and suggest indexes
void index_advisor_analyze(IndexAdvisor* advisor) {
    // Analyze recent queries
    for (QueryPattern* pattern : advisor->patterns) {
        // Check if good index exists
        if (!has_suitable_index(pattern)) {
            // Suggest new index
            MissingIndex* suggestion = create_index_suggestion(pattern);
            add_missing_index(advisor, suggestion);
        }
    }
    
    // Find unused indexes
    for (Index* index : all_indexes) {
        if (index->usage_count < USAGE_THRESHOLD) {
            add_unused_index(advisor, index->uuid);
        }
    }
}
```

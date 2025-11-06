# Index Implementation Fix Plan

**Created**: November 6, 2025
**Purpose**: Roadmap to correct all index implementation discrepancies identified in documentation audit
**Goal**: Achieve true 11/11 index completion (100%)

---

## EXECUTIVE SUMMARY

**Current Status**: 6/11 indexes fully complete (55%)
**Target Status**: 11/11 indexes fully complete (100%)
**Total Work Remaining**: ~60-85 hours across 5 indexes
**Priority**: HIGH - Required for accurate "100% complete" claim

### Issues to Fix (by Priority)

| Issue | Index | Severity | Effort | Priority |
|-------|-------|----------|--------|----------|
| Range scan missing | LSM-Tree | CRITICAL | 15-20h | P0 |
| Dictionary compression missing | Columnstore | HIGH | 20-30h | P1 |
| Custom tablespace support | Hash | MEDIUM | 3-5h | P2 |
| Custom tablespace support | GIN | MEDIUM | 3-5h | P2 |
| Custom tablespace support | Bitmap | MEDIUM | 3-5h | P2 |
| Custom tablespace support | HNSW | MEDIUM | 3-5h | P2 |
| Distance computation TODOs | HNSW | MEDIUM | 5-8h | P3 |
| **TOTAL** | **5 indexes** | - | **60-85h** | - |

---

## PART 1: CRITICAL FIXES (P0)

### Issue #1: LSM-Tree Range Scan NOT IMPLEMENTED

**Priority**: P0 (CRITICAL)
**Effort**: 15-20 hours
**Impact**: Blocks all range queries (e.g., `WHERE col BETWEEN x AND y`)
**File**: `src/core/lsm_tree_index.cpp`
**Line**: 297-307

#### Current State

```cpp
Status LSMTreeIndex::scan(const Key* start_key, const Key* end_key,
                          TransactionId current_xid,
                          std::vector<TID>* tids, ErrorContext* ctx) {
    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                      "LSM-Tree range scan not yet implemented");
    return Status::NOT_IMPLEMENTED;
}
```

#### Required Implementation

**Phase 1: Design (2-3 hours)**
1. Design k-way merge algorithm for range scans
2. Define cursor structure for tracking position across levels
3. Plan visibility filtering integration with MGA

**Phase 2: Implementation (10-12 hours)**
1. **Memtable Scan** (2-3 hours)
   - Implement iterator over Red-Black tree for range [start_key, end_key]
   - Add MGA visibility check: `isVersionVisible(xmin, current_xid)`
   - Return visible TIDs sorted by key

2. **SSTable Level Scan** (3-4 hours)
   - For each level (L0, L1, L2, L3):
     - Open all SSTables that may contain keys in range
     - Use Bloom filters to skip irrelevant SSTables
     - Read index blocks to find data block offsets
   - Create sorted run of TIDs per SSTable

3. **K-way Merge** (4-5 hours)
   - Implement priority queue-based merge of all runs:
     - Memtable run
     - L0 SSTable runs (all overlap, need merge)
     - L1 SSTable runs (non-overlapping within level)
     - L2, L3 runs
   - Handle duplicates: Keep newest version (highest LSN)
   - Apply MGA visibility filtering
   - Return merged, deduplicated TIDs in key order

4. **Edge Cases** (1-2 hours)
   - Handle NULL start_key (scan from beginning)
   - Handle NULL end_key (scan to end)
   - Handle empty range
   - Handle deleted keys (skip if rhd_deleted flag set)

**Phase 3: Testing (3-5 hours)**
1. Unit tests:
   - Empty index scan
   - Single key scan
   - Full range scan
   - Partial range scan
   - Scan with deletes
2. Integration tests:
   - SQL: `SELECT * FROM t WHERE indexed_col BETWEEN 100 AND 200`
   - Verify correctness with multiple levels
3. Performance tests:
   - Benchmark scan speed vs B-Tree
   - Verify Bloom filter effectiveness

#### Implementation Steps

```cpp
// PHASE 1: Add cursor structure
struct LSMCursor {
    const Key* start_key;
    const Key* end_key;
    TransactionId xid;

    // Memtable iterator
    std::map<Key, LSMEntry>::iterator memtable_it;

    // SSTable iterators (one per SSTable in range)
    struct SSTableCursor {
        int level;
        std::string sstable_path;
        std::ifstream file;
        Key current_key;
        TID current_tid;
        bool exhausted;
    };
    std::vector<SSTableCursor> sstable_cursors;

    // Priority queue for k-way merge
    struct CursorEntry {
        Key key;
        TID tid;
        uint64_t xmin;
        int source_id;  // Which cursor
        bool operator>(const CursorEntry& other) const {
            return key > other.key;
        }
    };
    std::priority_queue<CursorEntry, std::vector<CursorEntry>, std::greater<>> pq;
};

// PHASE 2: Implement scan()
Status LSMTreeIndex::scan(const Key* start_key, const Key* end_key,
                          TransactionId current_xid,
                          std::vector<TID>* tids, ErrorContext* ctx) {
    if (!tids) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "tids cannot be null");
        return Status::INVALID_ARGUMENT;
    }
    tids->clear();

    LSMCursor cursor;
    cursor.start_key = start_key;
    cursor.end_key = end_key;
    cursor.xid = current_xid;

    // Step 1: Initialize memtable iterator
    std::lock_guard<std::mutex> lock(memtable_mutex_);
    if (start_key) {
        cursor.memtable_it = memtable_.lower_bound(*start_key);
    } else {
        cursor.memtable_it = memtable_.begin();
    }

    // Step 2: Initialize SSTable cursors for all levels
    for (int level = 0; level < 4; level++) {
        for (const auto& sstable_path : level_sstables_[level]) {
            // Check Bloom filter
            if (start_key && !bloom_filter_may_contain(sstable_path, *start_key)) {
                continue;  // Skip this SSTable
            }

            // Open SSTable and seek to start_key
            SSTableCursor sst_cursor;
            sst_cursor.level = level;
            sst_cursor.sstable_path = sstable_path;
            sst_cursor.file.open(sstable_path, std::ios::binary);

            // Seek to first key >= start_key using index block
            seek_sstable_to_key(&sst_cursor, start_key);

            if (!sst_cursor.exhausted) {
                cursor.sstable_cursors.push_back(std::move(sst_cursor));
            }
        }
    }

    // Step 3: K-way merge using priority queue
    // Add first entry from each cursor to priority queue
    add_cursor_entries_to_pq(&cursor);

    Key last_key;
    bool first_key = true;

    while (!cursor.pq.empty()) {
        CursorEntry entry = cursor.pq.top();
        cursor.pq.pop();

        // Check end key
        if (end_key && entry.key > *end_key) {
            break;
        }

        // Skip duplicates: Only process first occurrence of each key
        if (!first_key && entry.key == last_key) {
            // Advance cursor and add next entry
            advance_cursor(&cursor, entry.source_id);
            continue;
        }

        // Check MGA visibility
        if (isVersionVisible(entry.xmin, current_xid)) {
            tids->push_back(entry.tid);
        }

        last_key = entry.key;
        first_key = false;

        // Advance cursor and add next entry
        advance_cursor(&cursor, entry.source_id);
    }

    return Status::OK;
}
```

#### Success Criteria

- ✅ Range scan returns correct TIDs in key order
- ✅ Handles all edge cases (NULL boundaries, empty ranges)
- ✅ MGA visibility filtering works correctly
- ✅ Deduplication works (returns newest version only)
- ✅ Performance: Scan speed within 2x of B-Tree for same range
- ✅ All tests pass (unit + integration + performance)

---

## PART 2: HIGH PRIORITY FIXES (P1)

### Issue #2: Columnstore Dictionary Compression NOT IMPLEMENTED

**Priority**: P1 (HIGH)
**Effort**: 20-30 hours
**Impact**: Core feature claimed but non-functional
**Files**: `src/core/columnstore.cpp`
**Lines**: 473-490 (compress), 590-609 (decompress)

#### Current State

```cpp
Status ColumnstoreIndex::compressDictionary(const std::vector<Value>& values,
                                            ColumnSegment* segment,
                                            ErrorContext* ctx) {
    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                      "Dictionary compression not yet implemented");
    return Status::NOT_IMPLEMENTED;
}

Status ColumnstoreIndex::decompressDictionary(const ColumnSegment* segment,
                                              std::vector<Value>* values,
                                              ErrorContext* ctx) {
    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                      "Dictionary decompression not yet implemented");
    return Status::NOT_IMPLEMENTED;
}
```

#### Required Implementation

**Phase 1: Design (3-4 hours)**
1. Design dictionary encoding format:
   - Dictionary: sorted array of unique values
   - Codes: array of indices into dictionary (1, 2, or 4 bytes depending on cardinality)
2. Define compression heuristic:
   - Use dictionary encoding if: cardinality < 0.5 * row_count
   - Otherwise fall back to RLE or bitpack
3. Plan storage layout in ColumnSegment

**Phase 2: Compression Implementation (8-10 hours)**

1. **Build Dictionary** (3-4 hours)
   ```cpp
   Status ColumnstoreIndex::compressDictionary(const std::vector<Value>& values,
                                               ColumnSegment* segment,
                                               ErrorContext* ctx) {
       // Step 1: Extract unique values
       std::set<Value> unique_values;
       for (const auto& val : values) {
           unique_values.insert(val);
       }

       // Step 2: Check if dictionary encoding is worthwhile
       size_t cardinality = unique_values.size();
       size_t row_count = values.size();

       if (cardinality > row_count / 2) {
           // Too many unique values, use different compression
           SET_ERROR_CONTEXT(ctx, Status::INVALID_STATE,
                            "Dictionary encoding not beneficial for high cardinality");
           return Status::INVALID_STATE;
       }

       // Step 3: Build dictionary (sorted for binary search)
       std::vector<Value> dictionary(unique_values.begin(), unique_values.end());
       std::sort(dictionary.begin(), dictionary.end());

       // Step 4: Build value->code mapping
       std::map<Value, uint32_t> value_to_code;
       for (size_t i = 0; i < dictionary.size(); i++) {
           value_to_code[dictionary[i]] = i;
       }

       // Step 5: Encode values as codes
       std::vector<uint32_t> codes;
       codes.reserve(values.size());
       for (const auto& val : values) {
           codes.push_back(value_to_code[val]);
       }

       // Step 6: Choose code width (1, 2, or 4 bytes)
       uint8_t code_width;
       if (cardinality <= 256) {
           code_width = 1;
       } else if (cardinality <= 65536) {
           code_width = 2;
       } else {
           code_width = 4;
       }

       // Step 7: Pack codes into byte array
       size_t packed_size = codes.size() * code_width;
       std::vector<uint8_t> packed_codes(packed_size);

       for (size_t i = 0; i < codes.size(); i++) {
           uint32_t code = codes[i];
           size_t offset = i * code_width;

           if (code_width == 1) {
               packed_codes[offset] = (uint8_t)code;
           } else if (code_width == 2) {
               packed_codes[offset] = (uint8_t)(code & 0xFF);
               packed_codes[offset + 1] = (uint8_t)((code >> 8) & 0xFF);
           } else {
               memcpy(&packed_codes[offset], &code, 4);
           }
       }

       // Step 8: Serialize dictionary to byte array
       std::vector<uint8_t> dict_bytes = serializeDictionary(dictionary);

       // Step 9: Store in segment
       segment->compression_type = CompressionType::DICTIONARY;
       segment->dictionary_code_width = code_width;
       segment->dictionary_cardinality = cardinality;
       segment->compressed_data = packed_codes;
       segment->dictionary_data = dict_bytes;
       segment->row_count = values.size();

       return Status::OK;
   }
   ```

2. **Serialize Dictionary** (2-3 hours)
   - Handle different data types (INT, VARCHAR, DECIMAL, etc.)
   - Use variable-length encoding for strings
   - Add type metadata

3. **Compression Ratio Calculation** (1-2 hours)
   - Calculate: `original_size / (dict_size + codes_size)`
   - Update segment stats

**Phase 3: Decompression Implementation (6-8 hours)**

1. **Deserialize Dictionary** (3-4 hours)
   ```cpp
   Status ColumnstoreIndex::decompressDictionary(const ColumnSegment* segment,
                                                 std::vector<Value>* values,
                                                 ErrorContext* ctx) {
       if (segment->compression_type != CompressionType::DICTIONARY) {
           SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                            "Segment is not dictionary compressed");
           return Status::INVALID_ARGUMENT;
       }

       // Step 1: Deserialize dictionary
       std::vector<Value> dictionary;
       Status status = deserializeDictionary(segment->dictionary_data, &dictionary, ctx);
       if (status != Status::OK) {
           return status;
       }

       // Step 2: Unpack codes
       size_t row_count = segment->row_count;
       uint8_t code_width = segment->dictionary_code_width;
       const uint8_t* packed_codes = segment->compressed_data.data();

       values->clear();
       values->reserve(row_count);

       for (size_t i = 0; i < row_count; i++) {
           size_t offset = i * code_width;
           uint32_t code;

           if (code_width == 1) {
               code = packed_codes[offset];
           } else if (code_width == 2) {
               code = packed_codes[offset] | (packed_codes[offset + 1] << 8);
           } else {
               memcpy(&code, &packed_codes[offset], 4);
           }

           // Step 3: Lookup value in dictionary
           if (code >= dictionary.size()) {
               SET_ERROR_CONTEXT(ctx, Status::INDEX_CORRUPTED,
                                "Invalid dictionary code");
               return Status::INDEX_CORRUPTED;
           }

           values->push_back(dictionary[code]);
       }

       return Status::OK;
   }
   ```

2. **Predicate Pushdown with Dictionary** (2-3 hours)
   - For predicates like `WHERE col = 'value'`:
     - Look up 'value' in dictionary → get code
     - Scan codes array for matches (faster than scanning values)
   - For range predicates:
     - Find range of codes in sorted dictionary
     - Scan for codes in range

3. **SIMD Optimization** (1-2 hours)
   - Vectorize code scanning for equality predicates
   - Use AVX2 to compare 8 codes at once

**Phase 4: Testing (3-5 hours)**
1. Unit tests:
   - Low cardinality (< 256 unique values, 1-byte codes)
   - Medium cardinality (< 65536 unique values, 2-byte codes)
   - High cardinality (> 65536 unique values, 4-byte codes)
   - Different data types (INT, VARCHAR, DECIMAL)
2. Compression ratio tests:
   - Verify ratio is correct
   - Ensure dictionary encoding outperforms RLE for categorical data
3. Predicate pushdown tests:
   - Verify equality predicates work
   - Verify range predicates work
4. Integration tests:
   - SQL: `SELECT * FROM t WHERE category = 'Electronics'`
   - Verify correctness with real data

#### Implementation Steps

1. Add dictionary structures to `ColumnSegment`:
   ```cpp
   struct ColumnSegment {
       // Existing fields...

       // Dictionary compression specific
       std::vector<uint8_t> dictionary_data;  // Serialized dictionary
       uint8_t dictionary_code_width;         // 1, 2, or 4 bytes
       uint32_t dictionary_cardinality;       // Number of unique values
   };
   ```

2. Implement `serializeDictionary()` and `deserializeDictionary()` helpers

3. Implement compression logic in `compressDictionary()`

4. Implement decompression logic in `decompressDictionary()`

5. Update `applyPredicate()` to use dictionary for faster filtering

6. Add comprehensive tests

#### Success Criteria

- ✅ Dictionary compression works for all data types
- ✅ 1-byte, 2-byte, and 4-byte code widths work correctly
- ✅ Compression ratio calculation is accurate
- ✅ Decompression restores original values exactly
- ✅ Predicate pushdown works with dictionary encoding
- ✅ Performance: 3-5x compression ratio for categorical data
- ✅ All tests pass

---

## PART 3: MEDIUM PRIORITY FIXES (P2)

### Issue #3-6: Custom Tablespace Support (4 Indexes)

**Priority**: P2 (MEDIUM)
**Effort**: 3-5 hours per index (12-20 hours total)
**Impact**: Blocks use of indexes on custom tablespaces
**Indexes Affected**: Hash, GIN, Bitmap, HNSW

#### Current State (Same for all 4 indexes)

```cpp
// Example from GIN (similar in Hash, Bitmap, HNSW)
Status GINIndex::create(const std::string& index_name, uint32_t tablespace_id,
                       const std::vector<uint32_t>& column_ids,
                       ErrorContext* ctx) {
    if (tablespace_id != 0) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                         "Custom tablespace support not yet implemented for GIN indexes");
        return Status::NOT_IMPLEMENTED;
    }
    // ... rest of implementation
}
```

#### Root Cause Analysis

All 4 indexes hard-code the database path and don't use the `tablespace_id` parameter to look up the correct tablespace directory.

**Current (WRONG)**:
```cpp
std::string index_path = database_path_ + "/" + index_name + ".gin";
```

**Should be**:
```cpp
std::string tablespace_path = catalog_->getTablespacePath(tablespace_id);
std::string index_path = tablespace_path + "/" + index_name + ".gin";
```

#### Required Implementation (Per Index)

**Phase 1: Add Tablespace Lookup** (1-2 hours)

1. Store `CatalogManager*` pointer in index class:
   ```cpp
   class GINIndex {
   private:
       CatalogManager* catalog_;  // Add this
       std::string database_path_;
       uint32_t tablespace_id_;   // Add this
   };
   ```

2. Update constructor to accept `CatalogManager*`:
   ```cpp
   GINIndex::GINIndex(Database* db, CatalogManager* catalog)
       : database_(db), catalog_(catalog), tablespace_id_(0) {
   }
   ```

3. Implement `getTablespacePath()` helper:
   ```cpp
   std::string GINIndex::getTablespacePath(uint32_t tablespace_id, ErrorContext* ctx) {
       if (tablespace_id == 0) {
           // Default tablespace = database directory
           return database_->path();
       }

       // Look up tablespace in catalog
       TablespaceInfo ts_info;
       Status status = catalog_->getTablespace(tablespace_id, &ts_info, ctx);
       if (status != Status::OK) {
           return "";
       }

       return ts_info.directory;
   }
   ```

**Phase 2: Update Index File Path Logic** (1-2 hours)

1. Update `create()` method:
   ```cpp
   Status GINIndex::create(const std::string& index_name, uint32_t tablespace_id,
                          const std::vector<uint32_t>& column_ids,
                          ErrorContext* ctx) {
       // REMOVE this check:
       // if (tablespace_id != 0) { return NOT_IMPLEMENTED; }

       // ADD tablespace path lookup
       std::string tablespace_path = getTablespacePath(tablespace_id, ctx);
       if (tablespace_path.empty()) {
           SET_ERROR_CONTEXT(ctx, Status::TABLESPACE_NOT_FOUND,
                            "Tablespace not found");
           return Status::TABLESPACE_NOT_FOUND;
       }

       // Store tablespace ID
       tablespace_id_ = tablespace_id;

       // Use tablespace path instead of database path
       std::string index_path = tablespace_path + "/" + index_name + ".gin";

       // ... rest of implementation
   }
   ```

2. Update `open()` method:
   ```cpp
   Status GINIndex::open(const std::string& index_name, uint32_t tablespace_id,
                        ErrorContext* ctx) {
       std::string tablespace_path = getTablespacePath(tablespace_id, ctx);
       if (tablespace_path.empty()) {
           return Status::TABLESPACE_NOT_FOUND;
       }

       tablespace_id_ = tablespace_id;
       std::string index_path = tablespace_path + "/" + index_name + ".gin";

       // ... rest of implementation
   }
   ```

3. Update all file I/O operations to use `tablespace_path`

**Phase 3: Testing** (1 hour per index)

1. Create custom tablespace:
   ```sql
   CREATE TABLESPACE ts_custom LOCATION '/var/lib/scratchbird/ts_custom';
   ```

2. Create index on custom tablespace:
   ```sql
   CREATE INDEX idx_users_email ON users USING GIN (email) TABLESPACE ts_custom;
   ```

3. Verify index file created in correct directory:
   ```bash
   ls /var/lib/scratchbird/ts_custom/idx_users_email.gin
   ```

4. Verify index operations work:
   ```sql
   INSERT INTO users (email) VALUES ('test@example.com');
   SELECT * FROM users WHERE email @> ARRAY['test@example.com'];
   ```

#### Implementation Order

1. **Hash Index** (easiest, template for others) - 3 hours
2. **GIN Index** - 4 hours
3. **Bitmap Index** - 4 hours
4. **HNSW Index** - 5 hours (most complex due to multi-page logic)

#### Success Criteria (Per Index)

- ✅ Custom tablespace check removed (no more NOT_IMPLEMENTED error)
- ✅ Index files created in correct tablespace directory
- ✅ Index operations work on custom tablespaces
- ✅ Existing default tablespace functionality still works
- ✅ Tests pass for both default and custom tablespaces

---

## PART 4: LOW PRIORITY FIXES (P3)

### Issue #7: HNSW Distance Computation TODOs

**Priority**: P3 (MEDIUM-LOW)
**Effort**: 5-8 hours
**Impact**: Incomplete distance metrics (cosine, Manhattan, dot product)
**File**: `src/core/hnsw_index.cpp`
**Lines**: 660-673

#### Current State

```cpp
double HNSWIndex::computeDistance(const std::vector<float>& a,
                                  const std::vector<float>& b,
                                  DistanceMetric metric) {
    switch (metric) {
        case DistanceMetric::EUCLIDEAN:
            // Fully implemented
            return computeEuclideanDistance(a, b);

        case DistanceMetric::COSINE:
            // TODO: Implement cosine distance
            return 0.0;

        case DistanceMetric::MANHATTAN:
            // TODO: Implement Manhattan distance
            return 0.0;

        case DistanceMetric::DOT_PRODUCT:
            // TODO: Implement dot product distance
            return 0.0;
    }
}
```

#### Required Implementation

**Phase 1: Cosine Distance** (2-3 hours)

```cpp
double HNSWIndex::computeCosineDistance(const std::vector<float>& a,
                                        const std::vector<float>& b) {
    if (a.size() != b.size()) {
        return std::numeric_limits<double>::max();
    }

    double dot_product = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;

    for (size_t i = 0; i < a.size(); i++) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    norm_a = std::sqrt(norm_a);
    norm_b = std::sqrt(norm_b);

    if (norm_a < 1e-10 || norm_b < 1e-10) {
        // Avoid division by zero
        return 1.0;  // Maximum distance
    }

    double cosine_similarity = dot_product / (norm_a * norm_b);

    // Cosine distance = 1 - cosine similarity
    // Clamp to [0, 2] to handle floating point errors
    return std::max(0.0, std::min(2.0, 1.0 - cosine_similarity));
}
```

**Phase 2: Manhattan Distance (L1)** (1-2 hours)

```cpp
double HNSWIndex::computeManhattanDistance(const std::vector<float>& a,
                                           const std::vector<float>& b) {
    if (a.size() != b.size()) {
        return std::numeric_limits<double>::max();
    }

    double sum = 0.0;
    for (size_t i = 0; i < a.size(); i++) {
        sum += std::abs(a[i] - b[i]);
    }

    return sum;
}
```

**Phase 3: Dot Product Distance** (1-2 hours)

```cpp
double HNSWIndex::computeDotProductDistance(const std::vector<float>& a,
                                            const std::vector<float>& b) {
    if (a.size() != b.size()) {
        return std::numeric_limits<double>::max();
    }

    double dot_product = 0.0;
    for (size_t i = 0; i < a.size(); i++) {
        dot_product += a[i] * b[i];
    }

    // Negate because HNSW uses distance (smaller = closer)
    // but dot product is similarity (larger = closer)
    return -dot_product;
}
```

**Phase 4: Update Main Function** (30 minutes)

```cpp
double HNSWIndex::computeDistance(const std::vector<float>& a,
                                  const std::vector<float>& b,
                                  DistanceMetric metric) {
    switch (metric) {
        case DistanceMetric::EUCLIDEAN:
            return computeEuclideanDistance(a, b);

        case DistanceMetric::COSINE:
            return computeCosineDistance(a, b);  // IMPLEMENTED

        case DistanceMetric::MANHATTAN:
            return computeManhattanDistance(a, b);  // IMPLEMENTED

        case DistanceMetric::DOT_PRODUCT:
            return computeDotProductDistance(a, b);  // IMPLEMENTED

        default:
            return std::numeric_limits<double>::max();
    }
}
```

**Phase 5: SIMD Optimization (Optional)** (2-3 hours)

Use AVX2/AVX-512 to vectorize distance computations:
```cpp
#ifdef __AVX2__
double HNSWIndex::computeEuclideanDistanceSIMD(const std::vector<float>& a,
                                               const std::vector<float>& b) {
    // Use _mm256_sub_ps, _mm256_mul_ps, _mm256_hadd_ps for 8-wide SIMD
    // 4-8x faster than scalar code
}
#endif
```

**Phase 6: Testing** (1-2 hours)

1. Unit tests for each distance metric:
   ```cpp
   TEST(HNSWDistanceTest, CosineDistance) {
       std::vector<float> a = {1.0, 0.0, 0.0};
       std::vector<float> b = {0.0, 1.0, 0.0};
       double dist = computeCosineDistance(a, b);
       EXPECT_NEAR(dist, 1.0, 1e-6);  // Orthogonal vectors
   }
   ```

2. Integration tests:
   ```sql
   CREATE INDEX idx_vectors ON embeddings USING HNSW (vec)
       WITH (distance_metric = 'cosine');

   SELECT * FROM embeddings ORDER BY vec <=> '[1.0, 0.0, 0.0]' LIMIT 10;
   ```

#### Success Criteria

- ✅ All 4 distance metrics implemented (Euclidean, Cosine, Manhattan, Dot Product)
- ✅ No TODO comments in computeDistance()
- ✅ Unit tests verify correctness
- ✅ Integration tests verify SQL integration
- ✅ (Optional) SIMD optimizations provide 4-8x speedup

---

## PART 5: IMPLEMENTATION TIMELINE

### Single Developer (Sequential)

**Week 1** (40 hours):
- Days 1-2 (15-20h): LSM-Tree range scan (P0)
- Days 3-5 (20-30h): Columnstore dictionary compression (P1)

**Week 2** (40 hours):
- Days 1-3 (12-20h): Custom tablespace support (Hash, GIN, Bitmap, HNSW) (P2)
- Days 4-5 (5-8h): HNSW distance computation (P3)
- Remaining: Buffer/testing

**Total**: 2 weeks (60-85 hours)

### Two Developers (Parallel)

**Week 1**:
- **Developer A**: LSM-Tree range scan (15-20h) → Hash/GIN tablespace (7-9h)
- **Developer B**: Columnstore dictionary compression (20-30h)

**Week 2**:
- **Developer A**: Bitmap/HNSW tablespace (8-11h) → Testing (5h)
- **Developer B**: HNSW distance computation (5-8h) → Testing (5h)

**Total**: 1.5 weeks (with parallel work)

### Recommended Approach

**Parallel with 2 developers** to complete in ~1.5 weeks:
1. Assign P0/P1 to both devs (critical path items)
2. Assign P2/P3 to Dev A after P0 completes
3. Comprehensive testing at end

---

## PART 6: TESTING STRATEGY

### Unit Tests (Per Issue)

1. **LSM-Tree Range Scan**:
   - Empty range
   - Full range
   - Partial range
   - Range with deletes
   - Range with multiple levels
   - Edge cases (NULL boundaries)

2. **Columnstore Dictionary**:
   - Low/medium/high cardinality
   - Different data types
   - Compression ratio validation
   - Predicate pushdown

3. **Custom Tablespace**:
   - Create on default tablespace
   - Create on custom tablespace
   - Switch tablespaces
   - Drop tablespace with indexes

4. **HNSW Distance**:
   - Each metric (Euclidean, Cosine, Manhattan, Dot Product)
   - Edge cases (zero vectors, identical vectors)

### Integration Tests

1. **SQL End-to-End**:
   ```sql
   -- LSM-Tree range scan
   SELECT * FROM logs WHERE timestamp BETWEEN '2025-01-01' AND '2025-01-31';

   -- Columnstore dictionary
   SELECT * FROM products WHERE category = 'Electronics';

   -- Custom tablespace
   CREATE TABLESPACE ts1 LOCATION '/data1';
   CREATE INDEX idx1 ON t1 USING GIN (col) TABLESPACE ts1;

   -- HNSW distance metrics
   SELECT * FROM vectors ORDER BY vec <=> '[1,0,0]' LIMIT 10;
   ```

2. **Performance Tests**:
   - LSM-Tree scan vs B-Tree scan (same range)
   - Columnstore dictionary vs RLE (categorical data)
   - HNSW distance metrics (compare speeds)

### Regression Tests

Ensure fixes don't break existing functionality:
- All existing index tests still pass
- Default tablespace behavior unchanged
- Euclidean distance still works in HNSW

---

## PART 7: SUCCESS CRITERIA

### Final Validation

Before marking "11/11 indexes complete (100%)":

**Per-Index Checklist**:
- [ ] LSM-Tree
  - [x] Point lookup works
  - [ ] Range scan works (currently NOT_IMPLEMENTED)
  - [x] Insert works
  - [x] Delete works
  - [x] Compaction works
  - [x] Bloom filters work
  - [x] MGA compliance
  - [x] Tests pass

- [ ] Columnstore
  - [x] RLE compression works
  - [ ] Dictionary compression works (currently NOT_IMPLEMENTED)
  - [x] Bitpack compression works
  - [x] Predicate pushdown works
  - [x] SIMD batch scans work
  - [x] MGA compliance
  - [x] Tests pass

- [ ] Hash Index
  - [x] Core operations work
  - [ ] Custom tablespace support (currently NOT_IMPLEMENTED)
  - [x] Tests pass

- [ ] GIN Index
  - [x] Core operations work
  - [ ] Custom tablespace support (currently NOT_IMPLEMENTED)
  - [x] Tests pass

- [ ] Bitmap Index
  - [x] Core operations work
  - [ ] Custom tablespace support (currently NOT_IMPLEMENTED)
  - [x] Tests pass

- [ ] HNSW Index
  - [x] Core operations work
  - [ ] Custom tablespace support (currently NOT_IMPLEMENTED)
  - [ ] All distance metrics work (cosine, Manhattan, dot product currently TODO)
  - [x] Tests pass

**Global Checklist**:
- [ ] All NOT_IMPLEMENTED blocks removed from index code
- [ ] All TODO comments addressed
- [ ] Line counts in documentation match actual implementation
- [ ] All tests pass (unit + integration + performance)
- [ ] Documentation updated to reflect completion
- [ ] No celebratory emojis until actually complete

---

## PART 8: DOCUMENTATION UPDATES

After completing all fixes, update:

### README.md

**Change**:
```markdown
**Indexes** (11/11 types complete, 100%): 🎉
- ✅ LSM-Tree - Complete (Memtable + SSTable + Compaction + Bloom filters, 117K write ops/sec) ✨
```

**To**:
```markdown
**Indexes** (11/11 types complete, 100%): 🎉
- ✅ LSM-Tree - Complete (Memtable + SSTable + Compaction + Bloom filters + Range Scan) ✨
```

### PROJECT_CONTEXT.md

**Add**:
```markdown
**Indexes** (11/11 complete, 100%):
- ✅ **LSM-Tree**: Complete (~2,880 lines) - Full CRUD, range scan, compaction ✨ Nov 6
- ✅ **Columnstore**: Complete (~2,366 lines) - RLE/Dictionary/Bitpack compression ✨ Nov 6
- ✅ **Hash/GIN/Bitmap/HNSW**: Complete with custom tablespace support ✨ Nov 6
```

### ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md

**Update completion percentages**:
- Index implementations: 100% (was 91%)
- Overall completion: 71% (was 69%)

---

## APPENDIX A: CODE REVIEW CHECKLIST

Before merging each fix:

**Code Quality**:
- [ ] Follows MGA_RULES.md (no PostgreSQL MVCC contamination)
- [ ] Proper error handling (ErrorContext)
- [ ] Memory safety (RAII, no leaks)
- [ ] Thread safety (proper locking)
- [ ] Clear comments

**Testing**:
- [ ] Unit tests added
- [ ] Integration tests added
- [ ] Performance tests added
- [ ] All tests pass
- [ ] No regressions

**Documentation**:
- [ ] Code comments updated
- [ ] Function documentation updated
- [ ] README.md updated (if applicable)
- [ ] CHANGELOG updated

---

## APPENDIX B: RISK MITIGATION

**Risk 1: LSM-Tree K-way Merge Complexity**
- Mitigation: Start with simple 2-way merge, expand to k-way
- Fallback: Materialize all runs to temp buffer, then sort

**Risk 2: Dictionary Compression Performance**
- Mitigation: Implement basic version first, optimize later
- Fallback: Use RLE if dictionary encoding fails

**Risk 3: Custom Tablespace Path Errors**
- Mitigation: Add extensive path validation
- Fallback: Fail gracefully with clear error messages

**Risk 4: Timeline Overrun**
- Mitigation: Prioritize P0/P1 first, defer P3 if needed
- Fallback: Mark as "90% complete with known limitations"

---

**Plan Created**: November 6, 2025
**Status**: READY FOR IMPLEMENTATION
**Next Step**: Begin with P0 (LSM-Tree range scan)

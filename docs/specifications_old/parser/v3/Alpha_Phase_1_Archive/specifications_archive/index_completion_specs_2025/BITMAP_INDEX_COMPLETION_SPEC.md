# Bitmap Index - Completion Specification

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Project**: ScratchBird Database Engine
**Component**: Bitmap Index - Complete Remaining Features
**Current Status**: 85% Complete (Core functional, missing features)
**Remaining Effort**: 20-30 hours
**Priority**: HIGH (Scalability blocker)

---

## ⚠️ CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- All bitmap operations must respect xmin/xmax visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- NO PostgreSQL MVCC contamination

**If context is lost during compaction, re-read `/MGA_RULES.md` immediately.**

---

## Table of Contents

1. [Current Status](#1-current-status)
2. [Missing Feature 1: Multi-Page Dictionary](#2-missing-feature-1-multi-page-dictionary)
3. [Missing Feature 2: Compression Ratio Calculation](#3-missing-feature-2-compression-ratio-calculation)
4. [Missing Feature 3: Mixed Type Handling](#4-missing-feature-3-mixed-type-handling)
5. [Testing Requirements](#5-testing-requirements)
6. [Implementation Breakdown](#6-implementation-breakdown)

---

## 1. Current Status

### What Works (85% Complete)

**File**: `src/core/bitmap_index.cpp` (1,378 lines)

**Implemented Features**:
- ✅ Roaring bitmap storage and operations
- ✅ Insert with bitmap creation
- ✅ Scan with bitmap AND/OR/NOT operations
- ✅ Dictionary for value mapping (single page)
- ✅ Compression (Roaring bitmap format)
- ✅ MGA compliance (xmin/xmax visibility)
- ✅ Bitmap operations: intersection, union, difference, NOT
- ✅ Cardinality estimation

### What's Missing (15% = 20-30 hours)

**Missing Feature 1**: Multi-page dictionary support
- **Location**: Line 282
- **Current**: Dictionary limited to single page (~250-500 entries)
- **Required**: Spill to multiple pages for large value sets
- **Impact**: Cannot index columns with >500 unique values
- **Effort**: 10-15 hours

**Missing Feature 2**: Compression ratio calculation
- **Location**: Line 659
- **Current**: Always returns 1.0 (no compression)
- **Required**: Track compressed vs uncompressed size
- **Impact**: Cannot measure storage efficiency
- **Effort**: 5-10 hours

**Missing Feature 3**: Mixed type handling
- **Location**: Line 1102
- **Current**: Only homogeneous types per index
- **Required**: Support multiple data types in composite index
- **Impact**: Cannot create bitmap index on (col1 INT, col2 VARCHAR)
- **Effort**: 5-10 hours

---

## 2. Missing Feature 1: Multi-Page Dictionary

### 2.1 Problem Statement

**Current Limitation**:
```cpp
// Line 282 in bitmap_index.cpp
if (dictionary_page_->num_entries >= MAX_DICTIONARY_ENTRIES_PER_PAGE) {
    // TODO: Implement multi-page dictionary
    return Status::ResourceExhausted("Dictionary full", ctx);
}
```

**Impact**:
- Single dictionary page can hold ~250-500 unique values (depends on value size)
- Indexes on high-cardinality columns fail after dictionary fills
- Example: Cannot index `country` column with 195+ countries

### 2.2 Solution: B-Tree Dictionary

**Architecture**:
```
┌─────────────────────────────────────────────────────┐
│            Bitmap Index Root Page                    │
├─────────────────────────────────────────────────────┤
│  - Dictionary root page pointer                      │
│  - Bitmap pages array                                │
│  - Statistics                                        │
└─────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────┐
│         Dictionary B-Tree (Root Page)                │
├─────────────────────────────────────────────────────┤
│  Entry 1: value="Alice" → bitmap_id=0                │
│  Entry 2: value="Bob"   → bitmap_id=1                │
│  ...                                                 │
│  Entry N: value="Zoe"   → bitmap_id=N                │
│  [Child pointers for leaf pages]                     │
└─────────────────────────────────────────────────────┘
                    │
        ┌───────────┼───────────┐
        ▼           ▼           ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│ Dictionary  │ │ Dictionary  │ │ Dictionary  │
│ Leaf Page 1 │ │ Leaf Page 2 │ │ Leaf Page 3 │
└─────────────┘ └─────────────┘ └─────────────┘
```

**Why B-Tree instead of Hash?**
- Need ordered traversal for range queries
- Need to find "closest match" for fuzzy lookups
- Need efficient bulk loading (sorted insert)

### 2.3 Implementation Details

**Step 1: Define Multi-Page Dictionary Structure** (2-3 hours)

```cpp
// Add to include/scratchbird/core/bitmap_index.h

struct SBBitmapDictionaryNode {
    PageHeader header;                    // Standard page header
    uint16_t node_type;                   // 0 = internal, 1 = leaf
    uint16_t num_entries;                 // Number of entries in this node
    uint32_t parent_page;                 // Parent node page (0 if root)
    uint32_t prev_page;                   // Previous sibling (for leaf pages)
    uint32_t next_page;                   // Next sibling (for leaf pages)

    // MGA compliance
    uint64_t xmin;
    uint64_t xmax;
    uint64_t lsn;

    uint8_t padding[64];

    // Entries follow immediately after header
    // Format depends on node_type:
    //   Internal: [key_len][key][child_page][bitmap_id]
    //   Leaf:     [key_len][key][bitmap_id][tid_count]
};

struct DictionaryEntry {
    std::vector<uint8_t> key;             // Value to look up
    uint32_t bitmap_id;                   // Bitmap page index
    uint32_t child_page;                  // For internal nodes only
};
```

**Step 2: Implement Dictionary Insert** (4-5 hours)

```cpp
// Add to src/core/bitmap_index.cpp

Status BitmapIndex::insertDictionaryEntry(const std::vector<uint8_t>& value,
                                          uint32_t bitmap_id,
                                          ErrorContext* ctx) {
    // 1. Search B-Tree for insertion point
    uint32_t leaf_page = 0;
    Status status = findDictionaryLeaf(value, &leaf_page, ctx);
    if (!status.ok()) return status;

    // 2. Pin leaf page
    SBBitmapDictionaryNode* leaf = nullptr;
    status = buffer_pool_->pinPage(leaf_page, (void**)&leaf, ctx);
    if (!status.ok()) return status;

    // 3. Check if leaf has space
    size_t entry_size = sizeof(uint16_t) + value.size() + sizeof(uint32_t);
    size_t available = PAGE_SIZE - sizeof(SBBitmapDictionaryNode)
                       - (leaf->num_entries * AVG_ENTRY_SIZE);

    if (entry_size > available) {
        // 4. Split leaf page
        uint32_t new_leaf_page = 0;
        status = splitDictionaryLeaf(leaf_page, &new_leaf_page, ctx);
        if (!status.ok()) {
            buffer_pool_->unpinPage(leaf_page, false, ctx);
            return status;
        }

        // 5. Re-find correct leaf after split
        buffer_pool_->unpinPage(leaf_page, false, ctx);
        status = findDictionaryLeaf(value, &leaf_page, ctx);
        if (!status.ok()) return status;

        status = buffer_pool_->pinPage(leaf_page, (void**)&leaf, ctx);
        if (!status.ok()) return status;
    }

    // 6. Insert entry into leaf (sorted position)
    status = insertIntoLeaf(leaf, value, bitmap_id, ctx);

    // 7. Unpin and mark dirty
    buffer_pool_->unpinPage(leaf_page, true, ctx);

    return status;
}
```

**Step 3: Implement Dictionary Search** (3-4 hours)

```cpp
Status BitmapIndex::searchDictionary(const std::vector<uint8_t>& value,
                                     uint32_t* bitmap_id_out,
                                     bool* found,
                                     ErrorContext* ctx) {
    *found = false;

    if (dict_root_page_ == 0) {
        return Status::OK;  // Empty dictionary
    }

    uint32_t current_page = dict_root_page_;

    // Traverse B-Tree from root to leaf
    while (true) {
        SBBitmapDictionaryNode* node = nullptr;
        Status status = buffer_pool_->pinPage(current_page, (void**)&node, ctx);
        if (!status.ok()) return status;

        if (node->node_type == 1) {  // Leaf node
            // Binary search within leaf
            status = binarySearchLeaf(node, value, bitmap_id_out, found, ctx);
            buffer_pool_->unpinPage(current_page, false, ctx);
            return status;
        } else {  // Internal node
            // Find child to descend into
            uint32_t child_page = 0;
            status = findChildPage(node, value, &child_page, ctx);
            buffer_pool_->unpinPage(current_page, false, ctx);

            if (!status.ok()) return status;
            current_page = child_page;
        }
    }
}
```

**Step 4: Implement Dictionary Split** (4-5 hours)

```cpp
Status BitmapIndex::splitDictionaryLeaf(uint32_t old_leaf_page,
                                        uint32_t* new_leaf_page_out,
                                        ErrorContext* ctx) {
    // 1. Load old leaf
    SBBitmapDictionaryNode* old_leaf = nullptr;
    Status status = buffer_pool_->pinPage(old_leaf_page, (void**)&old_leaf, ctx);
    if (!status.ok()) return status;

    // 2. Allocate new leaf
    uint32_t new_leaf_page = 0;
    status = page_manager_->allocatePage(new_leaf_page, ctx);
    if (!status.ok()) {
        buffer_pool_->unpinPage(old_leaf_page, false, ctx);
        return status;
    }

    SBBitmapDictionaryNode* new_leaf = nullptr;
    status = buffer_pool_->pinPage(new_leaf_page, (void**)&new_leaf, ctx);
    if (!status.ok()) {
        buffer_pool_->unpinPage(old_leaf_page, false, ctx);
        return status;
    }

    // 3. Initialize new leaf
    std::memset(new_leaf, 0, sizeof(SBBitmapDictionaryNode));
    new_leaf->header.page_type = static_cast<uint16_t>(PageType::BITMAP_DICTIONARY);
    new_leaf->node_type = 1;  // Leaf
    new_leaf->parent_page = old_leaf->parent_page;
    new_leaf->xmin = txn_mgr_->getCurrentXid();
    new_leaf->xmax = 0;

    // 4. Distribute entries (move half to new leaf)
    uint16_t split_point = old_leaf->num_entries / 2;
    std::vector<DictionaryEntry> entries;
    parseLeafEntries(old_leaf, &entries);

    // Keep first half in old leaf
    std::vector<DictionaryEntry> left_entries(entries.begin(),
                                              entries.begin() + split_point);
    // Move second half to new leaf
    std::vector<DictionaryEntry> right_entries(entries.begin() + split_point,
                                               entries.end());

    writeLeafEntries(old_leaf, left_entries);
    writeLeafEntries(new_leaf, right_entries);

    // 5. Update sibling pointers
    new_leaf->prev_page = old_leaf_page;
    new_leaf->next_page = old_leaf->next_page;
    old_leaf->next_page = new_leaf_page;

    // 6. Get split key (first key of new leaf)
    std::vector<uint8_t> split_key = right_entries[0].key;

    // 7. Insert split key into parent (may cause recursive split)
    status = insertIntoParent(old_leaf->parent_page, split_key,
                             new_leaf_page, ctx);

    // 8. Unpin pages
    buffer_pool_->unpinPage(old_leaf_page, true, ctx);
    buffer_pool_->unpinPage(new_leaf_page, true, ctx);

    *new_leaf_page_out = new_leaf_page;
    return status;
}
```

### 2.4 Testing Requirements

**Unit Tests** (`tests/unit/test_bitmap_multipage_dict.cpp`):
1. [ ] Insert 1,000 unique values (force multiple splits)
2. [ ] Search all 1,000 values (verify correctness)
3. [ ] Insert in sorted order (no splits needed)
4. [ ] Insert in reverse order (worst case splits)
5. [ ] Insert in random order (typical case)
6. [ ] Verify leaf sibling pointers after splits
7. [ ] Verify parent pointers after splits
8. [ ] Range scan across multiple leaf pages

**Acceptance Criteria**:
- All 8 tests pass
- Can insert 10,000+ unique values without error
- Search time remains O(log n) as dictionary grows

---

## 3. Missing Feature 2: Compression Ratio Calculation

### 3.1 Problem Statement

**Current Code** (Line 659):
```cpp
stats.compression_ratio = 1.0; // TODO: Calculate actual compression
```

**Impact**:
- Cannot measure storage efficiency
- Cannot justify using bitmap index vs B-Tree
- No feedback on whether Roaring compression is working

### 3.2 Solution: Track Compressed and Uncompressed Sizes

**Architecture**:
- Track **uncompressed size** = number of TIDs × 8 bytes (uint64_t)
- Track **compressed size** = actual bytes stored in Roaring bitmap
- Compression ratio = uncompressed / compressed

### 3.3 Implementation Details

**Step 1: Add Size Tracking to Bitmap Structure** (1-2 hours)

```cpp
// Modify struct in include/scratchbird/core/bitmap_index.h

struct SBBitmapIndexPage {
    // ... existing fields ...

    // Add compression tracking
    uint64_t total_uncompressed_bytes;  // Sum of (bitmap_cardinality * 8)
    uint64_t total_compressed_bytes;    // Sum of bitmap storage sizes

    uint8_t padding[48];  // Adjust padding
};
```

**Step 2: Track Sizes During Insert** (2-3 hours)

```cpp
// Modify insert method in src/core/bitmap_index.cpp

Status BitmapIndex::insert(const std::vector<uint8_t>& key,
                          const TID& tid,
                          uint64_t current_xid,
                          ErrorContext* ctx) {
    // ... existing lookup code ...

    // Get bitmap for this value
    RoaringBitmap* bitmap = getBitmapForValue(key, ctx);

    // Track size before insert
    size_t size_before = bitmap->getSizeInBytes();
    uint32_t cardinality_before = bitmap->getCardinality();

    // Insert TID into bitmap
    bitmap->add(tid.toUint64());

    // Track size after insert
    size_t size_after = bitmap->getSizeInBytes();
    uint32_t cardinality_after = bitmap->getCardinality();

    // Update statistics
    meta_page_->total_uncompressed_bytes += (cardinality_after - cardinality_before) * 8;
    meta_page_->total_compressed_bytes += (size_after - size_before);

    return Status::OK;
}
```

**Step 3: Calculate Ratio in getStats** (1-2 hours)

```cpp
Status BitmapIndex::getStats(BitmapIndexStats* stats_out, ErrorContext* ctx) {
    // ... existing code ...

    // Calculate compression ratio
    if (meta_page_->total_compressed_bytes > 0) {
        stats_out->compression_ratio =
            static_cast<double>(meta_page_->total_uncompressed_bytes) /
            static_cast<double>(meta_page_->total_compressed_bytes);
    } else {
        stats_out->compression_ratio = 1.0;
    }

    stats_out->uncompressed_bytes = meta_page_->total_uncompressed_bytes;
    stats_out->compressed_bytes = meta_page_->total_compressed_bytes;

    return Status::OK;
}
```

### 3.4 Testing Requirements

**Unit Tests** (`tests/unit/test_bitmap_compression_ratio.cpp`):
1. [ ] Insert 1,000 sequential TIDs (best case compression ~100x)
2. [ ] Insert 1,000 random TIDs (typical compression ~2-5x)
3. [ ] Insert all even TIDs 0-10000 (good compression ~10x)
4. [ ] Verify ratio increases with sequential inserts
5. [ ] Verify ratio after deletes (should recalculate)

**Acceptance Criteria**:
- All 5 tests pass
- Compression ratio matches expected values (±10%)
- Ratio updates correctly on insert/delete

---

## 4. Missing Feature 3: Mixed Type Handling

### 4.1 Problem Statement

**Current Code** (Line 1102):
```cpp
// In RoaringBitmap::combine()
if (lhs.value_type_ != rhs.value_type_) {
    // TODO: Handle mixed types
    return Status::InvalidArgument("Type mismatch", ctx);
}
```

**Impact**:
- Cannot create bitmap index on composite key like (INT, VARCHAR)
- Example: `CREATE INDEX ON orders (customer_id INT, status VARCHAR)`
- Limits usefulness for multi-column queries

### 4.2 Solution: Type-Aware Key Encoding

**Architecture**:
- Encode composite keys as byte arrays with type tags
- Format: `[type1][value1][type2][value2]...`
- Dictionary stores encoded keys, bitmaps map to TIDs

**Example**:
```
Key: (customer_id=123, status="shipped")
Encoded: [0x01][00 00 00 7B][0x02][00 07]shipped
          ^^^^  ^^^^^^^^^^^  ^^^^  ^^^^^^^^^^^
          INT32 123          TEXT  len=7, "shipped"
```

### 4.3 Implementation Details

**Step 1: Define Key Encoding Format** (1-2 hours)

```cpp
// Add to include/scratchbird/core/bitmap_index.h

enum class BitmapKeyType : uint8_t {
    INT32 = 0x01,
    INT64 = 0x02,
    TEXT = 0x03,
    TIMESTAMP = 0x04,
    DECIMAL = 0x05,
    // ... other types
};

struct EncodedKey {
    std::vector<uint8_t> data;  // Encoded composite key

    // Encode composite key from multiple values
    static Status encode(const std::vector<Value>& values,
                        EncodedKey* out,
                        ErrorContext* ctx);

    // Decode back to individual values
    static Status decode(const EncodedKey& key,
                        std::vector<Value>* values_out,
                        ErrorContext* ctx);

    // Compare two encoded keys (for dictionary sorting)
    int compare(const EncodedKey& other) const;
};
```

**Step 2: Implement Key Encoding** (2-3 hours)

```cpp
// Add to src/core/bitmap_index.cpp

Status EncodedKey::encode(const std::vector<Value>& values,
                         EncodedKey* out,
                         ErrorContext* ctx) {
    out->data.clear();

    for (const Value& val : values) {
        // 1. Write type tag
        BitmapKeyType type = mapDataTypeToBitmapKeyType(val.type);
        out->data.push_back(static_cast<uint8_t>(type));

        // 2. Write value based on type
        switch (type) {
            case BitmapKeyType::INT32: {
                int32_t int_val = val.asInt32();
                out->data.insert(out->data.end(),
                               reinterpret_cast<uint8_t*>(&int_val),
                               reinterpret_cast<uint8_t*>(&int_val) + sizeof(int32_t));
                break;
            }

            case BitmapKeyType::INT64: {
                int64_t int_val = val.asInt64();
                out->data.insert(out->data.end(),
                               reinterpret_cast<uint8_t*>(&int_val),
                               reinterpret_cast<uint8_t*>(&int_val) + sizeof(int64_t));
                break;
            }

            case BitmapKeyType::TEXT: {
                std::string str_val = val.asString();
                uint16_t len = str_val.size();
                out->data.insert(out->data.end(),
                               reinterpret_cast<uint8_t*>(&len),
                               reinterpret_cast<uint8_t*>(&len) + sizeof(uint16_t));
                out->data.insert(out->data.end(), str_val.begin(), str_val.end());
                break;
            }

            // ... other types
        }
    }

    return Status::OK;
}
```

**Step 3: Update Insert to Use Encoded Keys** (2-3 hours)

```cpp
Status BitmapIndex::insert(const std::vector<Value>& key_values,
                          const TID& tid,
                          uint64_t current_xid,
                          ErrorContext* ctx) {
    // 1. Encode composite key
    EncodedKey encoded;
    Status status = EncodedKey::encode(key_values, &encoded, ctx);
    if (!status.ok()) return status;

    // 2. Lookup or create bitmap for this encoded key
    uint32_t bitmap_id = 0;
    bool found = false;
    status = searchDictionary(encoded.data, &bitmap_id, &found, ctx);
    if (!status.ok()) return status;

    if (!found) {
        // Create new bitmap
        bitmap_id = next_bitmap_id_++;
        status = insertDictionaryEntry(encoded.data, bitmap_id, ctx);
        if (!status.ok()) return status;
    }

    // 3. Add TID to bitmap
    RoaringBitmap* bitmap = getBitmap(bitmap_id, ctx);
    bitmap->add(tid.toUint64());

    return Status::OK;
}
```

### 4.4 Testing Requirements

**Unit Tests** (`tests/unit/test_bitmap_mixed_types.cpp`):
1. [ ] Create index on (INT, VARCHAR)
2. [ ] Insert 100 rows with composite keys
3. [ ] Query with both columns specified
4. [ ] Query with only first column (should work)
5. [ ] Verify key encoding is deterministic (same input → same bytes)
6. [ ] Verify key comparison works correctly (sorted order)
7. [ ] Test with 3+ column composite key

**Acceptance Criteria**:
- All 7 tests pass
- Composite key queries return correct results
- Performance comparable to single-column bitmap index

---

## 5. Testing Requirements

### 5.1 Unit Tests

**New Test Files**:
1. `tests/unit/test_bitmap_multipage_dict.cpp` (8 tests)
2. `tests/unit/test_bitmap_compression_ratio.cpp` (5 tests)
3. `tests/unit/test_bitmap_mixed_types.cpp` (7 tests)

**Total**: 20 new tests

### 5.2 Integration Tests

**Existing Integration Tests** (`tests/integration/test_bitmap_index.cpp`):
- [ ] Verify all existing tests still pass after changes
- [ ] Add test for 10,000 unique values (multi-page dictionary stress test)
- [ ] Add test for composite key queries

### 5.3 Performance Benchmarks

**Benchmarks** (`tests/benchmark/benchmark_bitmap_index.cpp`):
- [ ] Measure insert throughput with 10,000 unique values
- [ ] Measure search time with multi-page dictionary (should be O(log n))
- [ ] Measure compression ratio on real-world data
- [ ] Compare composite key performance vs single key

---

## 6. Implementation Breakdown

### 6.1 Task Breakdown

| Task | Effort (hours) | Dependency |
|------|----------------|------------|
| **Multi-Page Dictionary** | **10-15** | - |
| 1.1 Define B-Tree dictionary structure | 2-3 | - |
| 1.2 Implement dictionary insert with split | 4-5 | 1.1 |
| 1.3 Implement dictionary search | 3-4 | 1.1 |
| 1.4 Implement dictionary split logic | 4-5 | 1.2 |
| 1.5 Unit tests for multi-page dictionary | 2-3 | 1.2, 1.3, 1.4 |
| **Compression Ratio** | **5-10** | - |
| 2.1 Add size tracking to bitmap structure | 1-2 | - |
| 2.2 Track sizes during insert/delete | 2-3 | 2.1 |
| 2.3 Calculate ratio in getStats | 1-2 | 2.2 |
| 2.4 Unit tests for compression ratio | 1-2 | 2.3 |
| **Mixed Type Handling** | **5-10** | - |
| 3.1 Define key encoding format | 1-2 | - |
| 3.2 Implement key encoding/decoding | 2-3 | 3.1 |
| 3.3 Update insert to use encoded keys | 2-3 | 3.2 |
| 3.4 Unit tests for mixed types | 1-2 | 3.3 |
| **Integration & Performance** | **2-3** | All |
| 4.1 Integration test updates | 1-2 | All |
| 4.2 Performance benchmarks | 1-2 | All |
| **TOTAL** | **22-38** | - |

### 6.2 Estimated Total Effort

**Realistic Estimate**: 20-30 hours (includes buffer time for debugging)

**Timeline**:
- Single developer: 1 week (full-time)
- Part-time: 2-3 weeks

---

## 7. MGA Compliance Checklist

**All dictionary operations must respect MGA rules:**

- [ ] Dictionary B-Tree nodes have xmin/xmax
- [ ] Dictionary inserts use `TransactionManager::getCurrentXid()`
- [ ] Dictionary searches filter invisible entries
- [ ] NO use of `Snapshot` structures (use `TransactionId` uint64_t)
- [ ] All visibility checks use `TransactionManager::isVersionVisible()`

**Reference**: See `/MGA_RULES.md` Section 4 (Visibility Rules)

---

## 8. Conclusion

This specification provides complete implementation details for the 3 missing features in the Bitmap index.

**Key Takeaways**:
- Multi-page dictionary is most complex (10-15 hours), enables scalability
- Compression ratio tracking is straightforward (5-10 hours), enables metrics
- Mixed type handling enables composite keys (5-10 hours), improves usability

**Completion Criteria**:
- All 20 unit tests pass
- Can insert 10,000+ unique values without error
- Compression ratio accurately reflects Roaring bitmap efficiency
- Composite key indexes work correctly

**Next Steps**:
1. Create detailed implementation plan (`/docs/Alpha_Phase_1_Archive/Index_Implementation_Archive/BITMAP_INDEX_COMPLETION_PLAN.md`)
2. Begin with multi-page dictionary (highest priority for scalability)
3. Add compression tracking (quick win for metrics)
4. Add mixed type support (enables composite keys)

**Status**: SPECIFICATION COMPLETE ✅
**Implementation**: PENDING (20-30 hours)

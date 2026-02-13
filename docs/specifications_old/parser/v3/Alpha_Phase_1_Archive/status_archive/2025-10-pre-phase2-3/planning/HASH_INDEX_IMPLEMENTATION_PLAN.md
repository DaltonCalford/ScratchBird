# Hash Index Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date:** 2025-10-01
**Status:** In Progress
**Estimated Effort:** 3-4 days

---

## Specification References

### Primary Specifications:
1. **LOW_LEVEL_SPECIFICATION_HASH_INDEX.md**
   - Location: `/docs/specifications/parser/v3/LOW_LEVEL_SPECIFICATION_HASH_INDEX.md`
   - Contains: Complete data structures, algorithms, and implementation details
   - Version: 1.0 (September 15, 2025)

2. **INDEX_IMPLEMENTATION_SPEC.md**
   - Location: `/docs/specifications/parser/v3/INDEX_IMPLEMENTATION_SPEC.md`
   - Contains: High-level hash index overview, integration patterns
   - Section: 3.1 Hash Index (lines 369-395)

### Related Code:
- B-tree implementation: `src/core/btree.cpp`, `include/scratchbird/core/btree.h`
- Page structures: `include/scratchbird/core/ondisk.h`
- Buffer pool: `src/core/buffer_pool.cpp`
- Catalog manager: `src/core/catalog_manager.cpp`

---

## Implementation Steps

### Phase 1: Core Data Structures (Day 1, Morning)

**Step 1: Create hash_index.h header**
- Location: `include/scratchbird/core/hash_index.h`
- Contents:
  - `SBHashIndexMetaPage` structure
  - `SBHashDirectoryPage` structure
  - `SBHashBucketPage` structure
  - `HashEntry` structure
  - `HashIndex` class declaration

**Step 2: Implement MurmurHash3**
- Location: `src/core/hash_functions.cpp` (new file)
- Header: `include/scratchbird/core/hash_functions.h` (new file)
- Function: `uint64_t MurmurHash64(const void* key, int len, uint64_t seed)`
- Public domain implementation

**Step 3: Create hash_index.cpp skeleton**
- Location: `src/core/hash_index.cpp`
- Constructor/destructor
- Basic initialization

### Phase 2: Basic Operations (Day 1, Afternoon - Day 2, Morning)

**Step 4: Implement index creation**
- `HashIndex::create()`
- Allocate meta page
- Initialize with global_depth = 4 (16 initial buckets)
- Allocate initial directory page
- Allocate initial bucket pages

**Step 5: Implement bucket lookup**
- `get_directory_index()` - calculate directory slot from hash
- `find_bucket_page_for_key()` - locate bucket page for key
- Handle multi-page directories

**Step 6: Implement basic insert**
- `HashIndex::insert(key, tuple_id)`
- Hash the key
- Find bucket
- Add entry if space available
- NO split logic yet (just overflow pages)

### Phase 3: Split Logic (Day 2, Afternoon)

**Step 7: Implement bucket split**
- `split_bucket()` - split when full
- Case 1: local_depth < global_depth (simple split)
  - Allocate new bucket
  - Increment local_depth for both
  - Redistribute entries
  - Update directory pointers
- Case 2: local_depth == global_depth (requires directory expansion)

**Step 8: Implement directory expansion**
- `expand_directory()` - double directory size
- Allocate new directory pages as needed
- Copy existing pointers (each pointer duplicated)
- Increment global_depth
- Call split_bucket()

**Step 9: Implement overflow pages**
- `allocate_overflow_page()` - create overflow page
- Link to bucket via hbp_overflow_page
- Chain multiple overflow pages

### Phase 4: Lookup Operation (Day 3, Morning)

**Step 10: Implement find operation**
- `HashIndex::find(key)` - return vector<TupleId>
- Hash the key
- Find bucket page
- Scan bucket and overflow chain
- Compare full hash values
- Return matching tuple IDs
- Note: Heap recheck done by caller

**Step 11: Implement scan support**
- `HashIndex::begin_scan()` - for full index scan
- `HashIndex::get_next()` - get next entry
- `HashIndex::end_scan()` - cleanup
- Used for vacuum, index rebuild

### Phase 5: Deletion & Maintenance (Day 3, Afternoon)

**Step 12: Implement delete operation**
- `HashIndex::remove(key, tuple_id)` - remove specific entry
- Find bucket
- Mark entry as deleted (use special flag)
- Track deleted count

**Step 13: Implement vacuum**
- `HashIndex::vacuum()` - compact and cleanup
- Remove deleted entries
- Consolidate overflow pages
- Free empty overflow pages
- Update statistics

**Step 14: Implement statistics**
- `HashIndex::get_statistics()` - collect stats
- Track: bucket count, entry count, deleted count
- Track: avg entries per bucket, overflow page count
- Used by query optimizer

### Phase 6: Integration (Day 4, Morning)

**Step 15: Catalog manager integration**
- Add hash index type to catalog
- Store meta page location
- Store index metadata
- Handle index creation/drop

**Step 16: Storage engine integration**
- Add `createHashIndex()` method
- Add `openHashIndex()` method
- Handle index scans
- Coordinate with B-tree indexes

### Phase 7: Testing & Documentation (Day 4, Afternoon)

**Step 17: Create test suite**
- Location: `tests/test_hash_index.cpp`
- Tests:
  - Basic insert/lookup
  - Bucket splits
  - Directory expansion
  - Overflow pages
  - Delete/vacuum
  - Large dataset (10K+ entries)
  - Hash collisions

**Step 18: Error handling**
- OOM conditions
- Page allocation failures
- Corrupt page detection
- Invalid hash values

**Step 19: Update documentation**
- Update INDEX_IMPLEMENTATION_SPEC.md status
- Update LOW_LEVEL_SPECIFICATION_HASH_INDEX.md
- Add implementation notes
- Document limitations

---

## Data Structure Details

### Meta Page (Page 0 of hash index)
```cpp
struct SBHashIndexMetaPage {
    PageHeader      hip_header;           // 64 bytes
    UUID            hip_index_uuid;       // 16 bytes
    uint32_t        hip_hash_func_id;     // 4 bytes (always 1 = MurmurHash3)
    uint32_t        hip_global_depth;     // 4 bytes (max 32)
    uint64_t        hip_directory_page;   // 8 bytes
    uint64_t        hip_num_tuples;       // 8 bytes
    uint64_t        hip_num_deleted;      // 8 bytes (for vacuum)
    // Total: ~120 bytes
};
```

### Directory Page
```cpp
struct SBHashDirectoryPage {
    PageHeader      hdp_header;           // 64 bytes
    uint64_t        hdp_next_page;        // 8 bytes (0 if last)
    uint64_t        hdp_bucket_pointers[]; // Fill rest of page
    // For 8KB page: (8192 - 72) / 8 = 1015 pointers per page
};
```

### Bucket Page
```cpp
struct SBHashBucketPage {
    PageHeader      hbp_header;           // 64 bytes
    uint16_t        hbp_entry_count;      // 2 bytes
    uint16_t        hbp_local_depth;      // 2 bytes
    uint32_t        hbp_deleted_count;    // 4 bytes (track deletions)
    uint64_t        hbp_overflow_page;    // 8 bytes
    // Reserved space                     // 12 bytes (alignment)
    HashEntry       hbp_entries[];        // Fill rest of page
    // For 8KB page: (8192 - 92) / 16 = 506 entries per page
};
```

### Hash Entry
```cpp
struct HashEntry {
    uint64_t        he_key_hash;          // 8 bytes (full hash)
    uint64_t        he_tuple_id;          // 8 bytes (page_id << 32 | item_id)
    // Total: 16 bytes per entry
    // Special value: he_tuple_id = 0 means deleted
};
```

---

## Key Algorithms

### Directory Index Calculation
```cpp
uint32_t get_directory_index(uint64_t hash, uint32_t global_depth) {
    return hash & ((1ULL << global_depth) - 1);
}
```

### Bucket Split Decision
```cpp
if (bucket_full) {
    if (local_depth < global_depth) {
        split_bucket();  // Simple split
    } else {
        expand_directory();  // Double directory, then split
    }
}
```

### Entry Redistribution on Split
```cpp
void redistribute_entries(bucket_old, bucket_new, local_depth) {
    uint64_t bit_mask = (1ULL << (local_depth - 1));
    for (entry : bucket_old->entries) {
        if (entry.he_key_hash & bit_mask) {
            move_to(bucket_new, entry);
        } else {
            keep_in(bucket_old, entry);
        }
    }
}
```

---

## Configuration Parameters

```cpp
// Constants
constexpr uint32_t INITIAL_GLOBAL_DEPTH = 4;     // 16 initial buckets
constexpr uint32_t MAX_GLOBAL_DEPTH = 20;        // 1M max buckets
constexpr uint32_t BUCKET_FILL_THRESHOLD = 90;   // Split at 90% full
constexpr uint32_t MAX_OVERFLOW_CHAIN = 5;       // Force split after 5 overflow pages
constexpr uint64_t MURMURHASH_SEED = 0x9747b28c;
```

---

## Performance Characteristics

### Time Complexity:
- **Insert:** O(1) average, O(n) worst case during split
- **Lookup:** O(1) average, O(k) where k = overflow chain length
- **Delete:** O(1) average
- **Split:** O(n) where n = entries in bucket

### Space Complexity:
- **Meta page:** 1 page (fixed)
- **Directory:** 2^global_depth * 8 bytes
  - Depth 4: 128 bytes (fits in 1 page)
  - Depth 10: 8 KB (1 page)
  - Depth 16: 512 KB (64 pages @ 8KB)
- **Buckets:** ~500 entries per bucket page (8KB)

### Capacity Estimates (8KB pages):
- Depth 10: 1024 buckets × 500 entries = ~500K entries
- Depth 16: 65K buckets × 500 entries = ~32M entries
- Depth 20: 1M buckets × 500 entries = ~500M entries

---

## Error Handling Strategy

```cpp
// Error conditions to handle:
1. OOM during page allocation
   -> Return Status::OOM, rollback partial work

2. Page corruption detected
   -> Return Status::CORRUPTED, mark index for rebuild

3. Directory too large (global_depth > MAX)
   -> Return Status::LIMIT_EXCEEDED

4. Hash collision overflow (too many overflow pages)
   -> Issue warning, allow but track for rebuild
```

---

## Testing Strategy

### Unit Tests:
1. Hash function consistency
2. Directory index calculation
3. Bucket split logic
4. Directory expansion
5. Overflow page chains

### Integration Tests:
1. Insert 1K sequential keys
2. Insert 1K random keys
3. Insert with duplicates
4. Lookup existing/non-existing keys
5. Delete and vacuum
6. Concurrent operations (future)

### Stress Tests:
1. Insert 100K entries
2. Force multiple directory expansions
3. Create long overflow chains
4. Random insert/delete/lookup mix

---

## Implementation Progress

- [x] Phase 0: Planning and specification review
- [ ] Phase 1: Core data structures
- [ ] Phase 2: Basic operations
- [ ] Phase 3: Split logic
- [ ] Phase 4: Lookup operation
- [ ] Phase 5: Deletion & maintenance
- [ ] Phase 6: Integration
- [ ] Phase 7: Testing & documentation

---

## Notes

- Hash index does NOT store actual key data (saves space)
- Heap recheck required after hash match (caller's responsibility)
- Only useful for equality searches (WHERE col = value)
- Cannot be used for range queries or ORDER BY
- Very fast for exact-match lookups
- Good for join conditions, unique constraint checking

---

## Future Enhancements (Post-Initial Implementation)

1. Concurrent access (latch bucket pages)
2. Bulk loading optimization
3. Dynamic resize based on load factor
4. Compression for overflow chains
5. Statistics-based optimization hints
6. Index advisor integration

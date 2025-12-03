# Work Package 8: Storage & Indexes

**Status:** NOT STARTED
**Priority:** P2-P3 Mixed
**Estimated Hours:** 12-16
**Files:** Various in src/core/

---

## Overview

Storage and index implementations have optimization gaps and placeholder code. These are lower priority but must be completed for Alpha 1 completeness.

---

## Tasks

### STOR-M1: Columnstore Row-Level OLTP (MEDIUM)
**File:** src/core/storage_engine.cpp
**Lines:** 429-440, 1522-1531
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Phase 2 Enhancement: Columnstore row-level OLTP integration
// ColumnstoreIndex currently only supports batch insertColumn() operations.
// For now, columnstore indexes are updated via explicit batch loads (REFRESH/ANALYZE)
```

**Required Changes:**
1. Add row buffer to columnstore index
2. Buffer individual row inserts
3. When buffer reaches threshold, batch insert to column segments
4. Handle updates and deletes similarly

**Implementation:**
```cpp
class ColumnstoreRowBuffer {
    std::vector<std::vector<TypedValue>> buffered_rows_;
    size_t threshold_ = 1000;  // Configurable

    void addRow(const std::vector<TypedValue>& row) {
        buffered_rows_.push_back(row);
        if (buffered_rows_.size() >= threshold_) {
            flushToColumnstore();
        }
    }

    void flushToColumnstore() {
        // Convert row-major to column-major
        // Call insertColumn() for each column
    }
};
```

**Verification:**
- [ ] INSERT INTO table with columnstore index updates index
- [ ] Query using columnstore returns recent inserts

---

### STOR-M2: GiST Page Allocation (MEDIUM)
**File:** src/core/gist_index.cpp
**Lines:** 1264-1284
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// For now, use a simple counter (in production, use free page list)
static uint64_t next_page = 1;
*page_num = next_page++;
```

**Required Changes:**
1. Maintain free page list in index metadata
2. Allocate from free list when available
3. Add to free list on page deletion
4. Fall back to extending file if no free pages

**Verification:**
- [ ] GiST index grows correctly
- [ ] Deleted pages are reused

---

### STOR-M3: GIN Fuzzy Matching (MEDIUM)
**File:** src/core/gin_index.cpp
**Lines:** 3917, 4136
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Optimized fuzzy matching (placeholder - full BK-tree implementation would go here)
// This is a placeholder for the full implementation
```

**Required Changes:**
Implement BK-tree for efficient fuzzy/similarity matching:
1. Build BK-tree structure for terms
2. Search within edit distance threshold
3. Return matching terms for fuzzy queries

**Verification:**
- [ ] GIN similarity search returns approximate matches
- [ ] Performance acceptable for large term sets

---

### STOR-L1: Hash Index Overflow Cleanup (LOW)
**File:** src/core/hash_index.cpp
**Lines:** 1158-1159
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// If this overflow page is now empty, we could free it
// For now, just mark it dirty
buffer_pool_->unpinPage(current_page, true, ctx);
```

**Required Changes:**
1. Detect when overflow page becomes empty
2. Remove from overflow chain
3. Add page to free list

**Verification:**
- [ ] Empty overflow pages are freed
- [ ] Hash index doesn't grow unboundedly with deletes

---

### STOR-L2: Hash Index Overflow Stats (LOW)
**File:** src/core/hash_index.cpp
**Line:** 1220
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
stats.num_overflow_pages = 0; // Phase 3 Enhancement: Count overflow pages
```

**Required Changes:**
1. Track overflow page count in index metadata
2. Update on page allocation/deallocation
3. Return in statistics

**Verification:**
- [ ] getStatistics() returns accurate overflow count

---

### STOR-L3: B-tree Parent Merge (LOW)
**File:** src/core/btree.cpp
**Line:** 2293
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Phase 3 Enhancement: Consider implementing parent merge if parent becomes underutilized
```

**Required Changes:**
1. After delete, check parent node fill factor
2. If below threshold and sibling can accommodate, merge
3. Recursively check up the tree

**Verification:**
- [ ] B-tree height decreases after heavy deletion
- [ ] No unnecessary internal nodes

---

## Dependencies

- STOR-M1 depends on columnstore batch insert working
- STOR-M2 should use existing page manager infrastructure
- STOR-M3 is independent
- STOR-L1/L2/L3 are independent

---

## Testing Plan

1. Columnstore OLTP tests (insert, query, verify)
2. GiST page allocation stress test
3. GIN fuzzy search accuracy tests
4. Hash index vacuum tests
5. B-tree compaction tests

---

## Completion Checklist

- [ ] All 6 tasks implemented
- [ ] All 1020 existing tests pass
- [ ] Index stress tests pass
- [ ] Code compiles without warnings

---

**Last Updated:** December 2, 2025

# Work Package 8: Storage & Indexes

**Status:** 1/6 COMPLETE (17%)
**Priority:** P2-P3 Mixed
**Estimated Hours:** 12-16
**Files:** Various in src/core/

---

## Overview

Storage and index implementations have optimization gaps and placeholder code. These are lower priority but must be completed for Alpha 1 completeness.

---

## Tasks

### STOR-M1: Columnstore Row-Level OLTP (MEDIUM)
**File:** src/core/storage_engine.cpp, include/scratchbird/core/columnstore_index.h, src/core/columnstore_index.cpp
**Lines:** storage_engine.cpp:429-447, 1530-1548; columnstore_index.h:48-55, 105-115; columnstore_index.cpp:147-258
**Status:** [x] COMPLETE

**Implementation:**
Added row-level OLTP support to `ColumnstoreIndexSimple`:

1. **BufferedRow struct** - Holds tid, data vector, and is_null flag
2. **Row buffer** - Per-column buffer map with mutex protection
3. **insertRow() method** - Buffers individual rows with auto-flush at threshold (1000 rows)
4. **flushRowBuffer() method** - Flushes all column buffers to segments
5. **flushColumnBuffer() helper** - Converts row-major to column-major format and calls insertColumn()

Updated `storage_engine.cpp`:
1. INSERT path (line 429-447): Calls `columnstore->insertRow()` for each indexed column
2. UPDATE path (line 1530-1548): Appends new values to columnstore buffer (append-only model)

**Key Features:**
- Thread-safe with mutex protection
- Auto-flush at configurable threshold (1000 rows)
- Converts row-major buffered data to column-major format
- Uses gpid (64-bit global page id) as TID representation

**Verification:**
- [x] INSERT INTO table with columnstore index updates index
- [x] Query using columnstore returns recent inserts
- [x] All 9 columnstore tests pass

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

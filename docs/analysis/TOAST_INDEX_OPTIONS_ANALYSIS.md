# TOAST Records in Indexes: Options and Architecture Analysis

**Date**: November 3, 2025
**Purpose**: Analyze different options for how TOAST records are pointed to/used in indexes within Firebird MGA
**Status**: Comprehensive Analysis

---

## Executive Summary

This document analyzes all possible architectural options for how indexes can handle TOAST (out-of-line) values in Firebird's Multi-Generational Architecture (MGA). The analysis evaluates three primary approaches and determines that **Option 3 (Detoast Before Index Insert)** is the only architecturally sound approach for MGA.

---

## Part 1: The Three Options for TOAST in Indexes

### Option 1: Store TOAST Pointer Bytes in Index

**Description**: Index stores the 18-byte ToastPointer structure as the key value

**Implementation**:
```cpp
// Insert path
ToastPointer pointer;
toastValue(large_data, &pointer, xid);

// Store heap tuple with pointer
heap_tuple.column = pointer;  // 18 bytes

// Index stores pointer bytes as key
std::vector<uint8_t> key(18);
memcpy(key.data(), &pointer, 18);
index->insert(key, tid, xid);
```

**Index Entry Structure**:
```
┌────────────────────────────────────────┐
│ Key: [0x01, 0x03, 0x88, 0x13, ...]     │ ← 18 bytes of pointer
│      (va_header, va_tag, va_valueid,   │
│       va_rawsize, va_extsize...)        │
│ TID: (5, 3)                             │
│ xmin: 100                               │
│ xmax: 0                                 │
└────────────────────────────────────────┘
```

**Search Behavior**:
```sql
SELECT * FROM users WHERE profile = 'some text';

-- Query executor builds search key
search_key = "some text"  -- actual text

-- Index search
index->search(search_key, current_xid, &tids)
  ❌ FAIL: Index has pointer bytes, not "some text"
  ❌ No match found (comparing text to pointer bytes)

-- Result: Query fails to find existing rows
```

**Evaluation**:

| Criterion | Assessment | Notes |
|-----------|------------|-------|
| **Correctness** | ❌ FAIL | Cannot find rows by actual value |
| **Range Queries** | ❌ FAIL | Ordering by pointer bytes, not values |
| **Uniqueness** | ❌ FAIL | Multiple rows with different values but same pointer structure could "collide" |
| **Sorting** | ❌ FAIL | ORDER BY sorts by pointer bytes, not actual data |
| **MGA Compliance** | ⚠️ PARTIAL | TID stable, visibility works, but index is useless |
| **Performance** | ✅ GOOD | Fast insert (no detoasting), but searches never work |

**Critical Flaws**:
1. **Search failure**: `WHERE name = 'Alice'` never finds rows (comparing text to pointer bytes)
2. **Range nonsense**: `WHERE name > 'A' AND name < 'Z'` orders by pointer bytes (meaningless)
3. **Uniqueness broken**: UNIQUE index allows duplicate actual values (pointer bytes differ)
4. **Sorting broken**: `ORDER BY name` sorts by pointer bytes, not alphabetically
5. **Index scans useless**: Full table scan faster than index scan (index provides no filtering)

**Conclusion**: ❌ **ARCHITECTURALLY INVALID** - Index becomes useless for queries

---

### Option 2: Store Direct Pointer to TOAST Chunks in Index

**Description**: Index stores TIDs pointing directly to TOAST chunk locations, not heap tuple

**Implementation**:
```cpp
// Insert path
ToastPointer pointer;
toastValue(large_data, &pointer, xid);
// Assume first chunk at (Page 100, Slot 1)

// Store heap tuple with pointer
heap_tuple.column = pointer;  // Points to chunks
heap_tid = (5, 3)

// Index points to FIRST TOAST CHUNK, not heap tuple
TID chunk_tid(100, 1);  // First chunk location
index->insert(actual_value, chunk_tid, xid);  // ❌ Points to chunk!
```

**Index Entry Structure**:
```
┌────────────────────────────────────────┐
│ Key: "This is the actual large text"   │
│ TID: (100, 1)  ← Points to TOAST chunk │ ❌ WRONG
│ xmin: 100                               │
│ xmax: 0                                 │
└────────────────────────────────────────┘
```

**Search Behavior**:
```sql
SELECT * FROM users WHERE profile = 'some text';

-- Index search
index->search('some text', current_xid, &tids)
  ✅ Returns TID: (100, 1)  ← TOAST chunk TID

-- Heap fetch
tuple = heapFetch((100, 1), current_xid)
  ❌ FAIL: Fetching from TOAST table, not main table
  ❌ TOAST chunk structure != main table tuple structure
  ❌ Result is chunk data, not full row
```

**MGA Violation Analysis**:

**Violation 1: TID Instability**
```
Update non-indexed column:
UPDATE users SET email = 'new' WHERE id = 1;

MGA Principle: TID should remain stable if indexed column unchanged

BEFORE UPDATE:
  Heap tuple: (5, 3)
  Index points to: (100, 1) ← TOAST chunk

UPDATE creates new back version at (7, 12)
But indexed column (profile) unchanged
TOAST chunks remain at (100, 1)

Index should NOT change (TID stable)
But index points to (100, 1), not (5, 3)

Problem: How does executor find heap tuple?
Index returns (100, 1) ← chunk location
Need (5, 3) ← heap tuple location
```

**Violation 2: Multi-Value Chunks**
```
TOAST value split into 25 chunks at:
  (100, 1), (100, 2), (100, 3), ..., (100, 25)

Which TID does index store?
- First chunk (100, 1)?
- Last chunk (100, 25)?
- All chunks? (duplicate index entries for same value)

If first chunk (100, 1):
  - What if first chunk deleted by GC but later chunks remain?
  - Index points to non-existent chunk
```

**Violation 3: Back Version Navigation**
```
MGA traversal: Current version → Back version → Older back version

BEFORE:
  Heap: (5,3) → back_ptr → (7,12) → back_ptr → (9,5)
  Each heap version has its own TOAST chunks

Version at (5,3): TOAST chunks at (100,1), (100,2), ...
Version at (7,12): TOAST chunks at (95,10), (95,11), ...
Version at (9,5): No TOAST (inline)

Index points to: (100,1) ← current version chunks

Transaction needs to see old version (7,12):
  - Traverses heap back version chain: (5,3) → (7,12)
  - Finds TOAST pointer in (7,12): va_valueid=5000
  - Needs TOAST chunks at (95,10), (95,11), ...

  But index points to (100,1) - wrong version!

Problem: Index points to chunk for current version only
Cannot navigate to back version chunks via index
```

**Violation 4: Garbage Collection Breaks Index**
```
Sweep garbage collects old TOAST chunks:

BEFORE GC:
  Index: Key="text", TID=(100,1)
  TOAST chunk at (100,1): xmin=100, xmax=200, data=...

AFTER GC (sweep removes chunks with xmax < OIT):
  TOAST chunk at (100,1): REMOVED (garbage collected)
  Index: Key="text", TID=(100,1) ← DANGLING POINTER

Index now points to non-existent chunk!
```

**Evaluation**:

| Criterion | Assessment | Notes |
|-----------|------------|-------|
| **Correctness** | ❌ FAIL | Returns chunk, not heap tuple |
| **TID Stability** | ❌ FAIL | Violates MGA TID stability principle |
| **Back Versioning** | ❌ FAIL | Cannot navigate version chains |
| **Multi-Chunk** | ❌ FAIL | No clear TID to store (25 chunks = 25 TIDs?) |
| **Garbage Collection** | ❌ FAIL | GC creates dangling index pointers |
| **MGA Compliance** | ❌ FAIL | Violates multiple MGA core principles |
| **Executor Compatibility** | ❌ FAIL | Executor expects heap tuple TIDs, not chunk TIDs |

**Critical MGA Violations**:
1. **TID Stability**: TID should point to primary tuple location (stable), not chunk location (changes)
2. **Back Version Chain**: Cannot traverse heap back versions via chunk TIDs
3. **Heap/Index Separation**: Indexes must point to heap, not secondary storage (TOAST table)
4. **Garbage Collection**: Chunk removal breaks index pointers
5. **Tuple Structure**: Chunk structure ≠ heap tuple structure (executor expects heap tuples)

**Conclusion**: ❌ **FUNDAMENTALLY INCOMPATIBLE WITH MGA** - Violates core architectural principles

---

### Option 3: Detoast Value Before Index Insert (Store Actual Value + Heap TID)

**Description**: Storage layer detoasts TOAST values before indexing; index stores actual detoasted value as key and heap tuple TID

**Implementation**:
```cpp
// Insert path
// 1. TOAST large value
ToastPointer pointer;
toastValue(large_data, &pointer, xid);
// Chunks stored at (100,1), (100,2), ..., (100,25)

// 2. Store heap tuple with pointer
heap_tuple.column = pointer;  // 18-byte pointer
heap_tid = insertHeapTuple(heap_tuple);  // Returns (5, 3)

// 3. For index insert, detoast value
std::vector<uint8_t> actual_value;
detoastValue(&pointer, &actual_value, xid);
// actual_value = "This is the actual large text..."

// 4. Index stores actual value + heap TID
index->insert(actual_value, heap_tid, xid);
```

**Index Entry Structure**:
```
┌────────────────────────────────────────┐
│ Key: "This is the actual large text"   │ ← Actual detoasted value
│ TID: (5, 3)                             │ ← Heap tuple location ✓
│ xmin: 100                               │
│ xmax: 0                                 │
└────────────────────────────────────────┘
```

**Search Behavior**:
```sql
SELECT * FROM users WHERE profile = 'some text';

-- Index search
index->search('some text', current_xid, &tids)
  ✅ Compares search key to actual detoasted values in index
  ✅ Finds match
  ✅ Returns heap TID: (5, 3)

-- Heap fetch
tuple = heapFetch((5, 3), current_xid)
  ✅ Fetches heap tuple at (5, 3)
  ✅ Tuple contains all columns (including ToastPointer for profile)

-- Detoast for result (if needed)
if (query_needs_profile_column) {
    detoastValue(tuple.profile, &result_value, current_xid);
}
```

**MGA Compliance Analysis**:

**✅ TID Stability**:
```
Update non-indexed column:
UPDATE users SET email = 'new' WHERE id = 1;

BEFORE:
  Heap: (5, 3) - profile = ToastPointer(5678)
  Index: Key="text", TID=(5,3)

UPDATE:
  Create back version at (7, 12)
  Modify heap at (5, 3) in-place
  Indexed column (profile) UNCHANGED

Index update:
  ✅ NO INDEX UPDATE NEEDED
  ✅ TID (5,3) remains stable
  ✅ Index still points to correct heap location
```

**✅ Back Version Navigation**:
```
Version chain:
  (5,3): profile=ToastPointer(9999), xmin=200
    ↓ back_ptr
  (7,12): profile=ToastPointer(5678), xmin=100
    ↓ back_ptr
  (9,5): profile="inline", xmin=50

Index entries:
  [Key="new text", TID=(5,3), xmin=200]  ← Current version
  [Key="old text", TID=(5,3), xmin=100, xmax=200]  ← Old version (soft deleted)

Transaction 150 (before both updates):
  1. Index search finds both entries
  2. Visibility check: xmin=100 visible, xmin=200 not visible
  3. Returns TID=(5,3) from old index entry
  4. Heap fetch at (5,3) finds xmin=200 not visible
  5. Traverse back version chain to (7,12)
  6. Find TOAST pointer to chunks 5678
  7. Detoast old value
  ✅ Correct old version returned
```

**✅ Multi-Chunk Handling**:
```
Large value split into 25 chunks:
  (100,1), (100,2), ..., (100,25)
  All chunks have same va_valueid=5678

Index stores:
  Key: <full detoasted value from all 25 chunks>
  TID: (5, 3)  ← Heap tuple, not chunk

✅ Single index entry regardless of chunk count
✅ Chunks reassembled before indexing
✅ Index unaware of chunking
```

**✅ Garbage Collection**:
```
Old TOAST chunks deleted:
  Chunks (95,10), (95,11), ... have xmax=200

Sweep (OIT advances past 200):
  - Remove chunks (95,10), (95,11), ... (garbage)
  - Index entries for old version also garbage (xmax=200)
  - Cooperative GC or background GC removes old index entries

Index remains valid:
  ✅ Current index entry points to (5,3) - heap tuple
  ✅ Old index entry can be GC'd (marked with xmax=200)
  ✅ No dangling pointers
```

**✅ Update Indexed Column**:
```
UPDATE users SET profile = <new large value> WHERE id = 1;

Process:
  1. TOAST new value → chunks (200,1), (200,2), ... (va_valueid=9999)
  2. Create back version at (7,12) with old pointer (va_valueid=5678)
  3. Update heap at (5,3) with new pointer (va_valueid=9999)
  4. Mark old chunks as deleted (xmax=200)
  5. Index update:
     A. Detoast OLD value from back version (7,12)
        - Read ToastPointer(5678)
        - Fetch chunks with va_valueid=5678
        - Reassemble old text
     B. Soft delete old index entry
        index->softDelete(old_text, (5,3), 200)
     C. Detoast NEW value from primary tuple (5,3)
        - Read ToastPointer(9999)
        - Fetch chunks with va_valueid=9999
        - Reassemble new text
     D. Insert new index entry
        index->insert(new_text, (5,3), 200)

Result:
  Index has two entries for same TID:
    [Key="old text", TID=(5,3), xmin=100, xmax=200]
    [Key="new text", TID=(5,3), xmin=200, xmax=0]

  ✅ TID stable (both point to 5,3)
  ✅ Visibility determines which entry visible to each transaction
```

**Evaluation**:

| Criterion | Assessment | Notes |
|-----------|------------|-------|
| **Correctness** | ✅ PASS | Searches find rows by actual value |
| **Range Queries** | ✅ PASS | Ordering by actual values |
| **Uniqueness** | ✅ PASS | UNIQUE constraint on actual values |
| **Sorting** | ✅ PASS | ORDER BY sorts by actual values |
| **TID Stability** | ✅ PASS | TID points to stable heap location |
| **Back Versioning** | ✅ PASS | Version chain navigation works |
| **Multi-Chunk** | ✅ PASS | Chunks reassembled before indexing |
| **Garbage Collection** | ✅ PASS | GC cleans both chunks and index entries |
| **MGA Compliance** | ✅ PASS | Follows all MGA principles |
| **Executor Compatibility** | ✅ PASS | Returns heap tuple TIDs as expected |

**Trade-offs**:

**Pros**:
- ✅ Architecturally sound (MGA compliant)
- ✅ Indexes work correctly (searches, ranges, sorting)
- ✅ TID stability maintained
- ✅ Back version navigation works
- ✅ Garbage collection safe
- ✅ Executor compatibility (returns heap TIDs)
- ✅ Indexes unaware of TOAST (separation of concerns)

**Cons**:
- ⚠️ Detoasting overhead on insert (must reassemble chunks)
- ⚠️ Detoasting overhead on update (must reassemble old & new values)
- ⚠️ Index size larger (stores full detoasted values, not 18-byte pointers)
- ⚠️ Repeated detoasting if multiple indexes on same TOAST column

**Conclusion**: ✅ **RECOMMENDED - Only MGA-Compliant Option**

---

## Part 2: Performance Comparison

### 2.1 Insert Performance

**Operation**: `INSERT INTO users (name, profile) VALUES ('Alice', <50KB text>);`

| Option | Steps | Detoast Count | Index Size |
|--------|-------|---------------|------------|
| **Option 1** | 1. TOAST value<br>2. Insert heap tuple<br>3. Insert pointer bytes to index | 0 | 18 bytes |
| **Option 2** | 1. TOAST value<br>2. Insert heap tuple<br>3. Insert chunk TID to index | 0 | 8 bytes |
| **Option 3** | 1. TOAST value<br>2. Insert heap tuple<br>3. **Detoast value**<br>4. Insert actual value to index | **1** | 50KB |

**Analysis**:
- Option 3 has **highest insert cost** (1 detoast per insert)
- Option 3 has **largest index size** (50KB vs 18 bytes)
- But Options 1 & 2 are architecturally invalid

**Optimization for Option 3**:
```cpp
// Cache detoasted value if inserting into multiple indexes
std::vector<uint8_t> detoasted_value;
bool detoasted = false;

for (Index* index : indexes_on_profile) {
    if (!detoasted) {
        detoastValue(pointer, &detoasted_value, xid);
        detoasted = true;  // Cache for subsequent indexes
    }
    index->insert(detoasted_value, tid, xid);
}
```
**Result**: 1 detoast per N indexes (instead of N detoasts)

### 2.2 Search Performance

**Operation**: `SELECT * FROM users WHERE profile = 'keyword';`

| Option | Search Steps | Detoast Count | Result |
|--------|--------------|---------------|--------|
| **Option 1** | 1. Search for "keyword"<br>2. ❌ Compare to pointer bytes<br>3. ❌ No match | 0 | ❌ FAIL |
| **Option 2** | 1. Search for "keyword"<br>2. ✅ Find match<br>3. ❌ Fetch chunk, not heap tuple | 0 | ❌ FAIL |
| **Option 3** | 1. Search for "keyword"<br>2. ✅ Compare to actual values<br>3. ✅ Find match<br>4. ✅ Fetch heap tuple | 0 | ✅ PASS |

**Analysis**:
- Option 3 has **no detoasting overhead on search** (index already has actual values)
- Option 3 is **only option where search works correctly**
- Options 1 & 2 fail to find matching rows

### 2.3 Update Performance

**Operation**: `UPDATE users SET profile = <new 50KB text> WHERE id = 1;` (indexed column)

| Option | Update Steps | Detoast Count |
|--------|--------------|---------------|
| **Option 1** | 1. TOAST new value<br>2. Update heap<br>3. Delete old pointer from index<br>4. Insert new pointer to index | 0 |
| **Option 2** | 1. TOAST new value<br>2. Update heap<br>3. Delete old chunk TID from index<br>4. Insert new chunk TID to index | 0 |
| **Option 3** | 1. TOAST new value<br>2. Update heap<br>3. **Detoast old value** (from back version)<br>4. Delete old actual value from index<br>5. **Detoast new value**<br>6. Insert new actual value to index | **2** |

**Analysis**:
- Option 3 has **highest update cost** (2 detoasts per update)
- But Options 1 & 2 don't maintain correct index state
- Detoasting old value required to find index entry to delete

**Optimization for Option 3**:
```cpp
// If updating multiple TOAST columns with multiple indexes
// Cache detoasted values per column
std::unordered_map<uint16_t, std::vector<uint8_t>> old_values_cache;
std::unordered_map<uint16_t, std::vector<uint8_t>> new_values_cache;

for (Index* index : table.indexes) {
    for (uint16_t col : index.columns) {
        if (!old_values_cache.contains(col)) {
            detoastIfNeeded(old_tuple.column[col], &old_values_cache[col], xid);
        }
        if (!new_values_cache.contains(col)) {
            detoastIfNeeded(new_tuple.column[col], &new_values_cache[col], xid);
        }
    }

    index->delete(old_values_cache, tid, xid);
    index->insert(new_values_cache, tid, xid);
}
```
**Result**: 1 detoast per column across all indexes (instead of 2N detoasts for N indexes)

### 2.4 Range Scan Performance

**Operation**: `SELECT * FROM users WHERE profile > 'A' AND profile < 'Z' ORDER BY profile;`

| Option | Scan Steps | Detoast Count | Result |
|--------|------------|---------------|--------|
| **Option 1** | 1. Range scan on pointer bytes<br>2. ❌ Wrong ordering | 0 | ❌ FAIL |
| **Option 2** | 1. Range scan finds chunk TIDs<br>2. ❌ Fetch chunks, not tuples | 0 | ❌ FAIL |
| **Option 3** | 1. ✅ Range scan on actual values<br>2. ✅ Correct ordering<br>3. ✅ Fetch heap tuples<br>4. Detoast if needed for result | 0-M | ✅ PASS |

**Analysis**:
- Option 3: No detoasting during scan (index has actual values)
- Detoasting only if application needs column value in result
- Lazy detoasting: skip if `SELECT id` (doesn't need profile)

---

## Part 3: Space Overhead Comparison

### 3.1 Index Size

**Scenario**: 1 million rows, average TOAST value size 50KB

| Option | Index Key Size | Index Size (1M rows) |
|--------|----------------|----------------------|
| **Option 1** | 18 bytes (pointer) | 18 MB |
| **Option 2** | 8 bytes (chunk TID) | 8 MB |
| **Option 3** | 50 KB (actual value) | 50 GB |

**Analysis**:
- Option 3 has **massive index size** (50GB vs 18MB)
- But Options 1 & 2 produce **unusable indexes**
- Trade-off: Correctness vs space

**Mitigation**:
1. **Prefix/Suffix Compression**: B-tree can compress common prefixes
   - Example: Indexing URLs with common prefix "https://example.com/"
   - Compression saves 80-90% for text data
   - Reduces 50GB to 5-10GB

2. **Partial Indexing**: Index first N bytes only (for large text)
   ```sql
   CREATE INDEX idx_profile_prefix ON users (LEFT(profile, 1000));
   ```
   - Index stores first 1000 bytes, not full 50KB
   - Reduces index size by 98% (1KB vs 50KB)
   - Loses exact uniqueness/ordering, but useful for prefix searches

3. **Hash Indexing**: Index hash of value instead of value itself
   ```sql
   CREATE INDEX idx_profile_hash ON users (HASH(profile));
   ```
   - Index stores 8-byte hash, not 50KB value
   - Fast equality searches, no range scans
   - Loses ordering, but small index size

### 3.2 Total Storage Cost

**Scenario**: 1 million rows, 50KB average TOAST value, 2 indexes on TOAST column

| Component | Option 1 | Option 2 | Option 3 |
|-----------|----------|----------|----------|
| Heap (with pointers) | 100 MB | 100 MB | 100 MB |
| TOAST chunks | 50 GB | 50 GB | 50 GB |
| Index 1 | 18 MB | 8 MB | 50 GB |
| Index 2 | 18 MB | 8 MB | 50 GB |
| **Total** | **50.14 GB** | **50.12 GB** | **150 GB** |

**Analysis**:
- Option 3: **3x total storage** (150GB vs 50GB)
- Most cost is in indexes (100GB) vs TOAST chunks (50GB)
- Options 1 & 2 have lower storage but don't work

**Recommendation**: Use Option 3 with optimizations:
- Prefix compression: Reduces index to 5-10GB each
- Partial indexing: Reduces index to 1-2GB each
- Result: 50GB (TOAST) + 10-20GB (indexes) = 60-70GB (comparable to Options 1 & 2)

---

## Part 4: Firebird MGA Specific Considerations

### 4.1 TIP-Based Visibility

All options must use **TIP (Transaction Inventory Pages)** for visibility, not snapshots.

**Option 3 Implementation**:
```cpp
// Index search with MGA visibility
Status BTree::search(const std::vector<uint8_t> &key,
                     uint64_t current_xid,
                     std::vector<TID> *tids_out,
                     ErrorContext *ctx)
{
    // ... traverse tree to find matching entries ...

    for (each matching entry) {
        // Check entry visibility via TIP
        TxState xmin_state = txn_mgr->getTransactionState(entry.xmin);

        // Own changes always visible
        if (entry.xmin == current_xid) {
            tids_out->push_back(entry.tid);
            continue;
        }

        // Check if creating transaction committed and older
        if (xmin_state == TX_COMMITTED && entry.xmin < current_xid) {
            // Check if deleted
            if (entry.xmax != 0) {
                TxState xmax_state = txn_mgr->getTransactionState(entry.xmax);
                if (xmax_state == TX_COMMITTED && entry.xmax < current_xid) {
                    continue;  // Deleted before our transaction
                }
            }
            tids_out->push_back(entry.tid);  // Visible
        }
    }

    return Status::OK;
}
```

**Key Points**:
- Uses `getTransactionState()` which looks up TIP bitmap
- No `Snapshot` structure
- Simple integer comparison: `entry.xmin < current_xid`
- Works identically for regular values and detoasted TOAST values

### 4.2 Back Versioning

**TOAST Value Update Example**:

```
Initial state (txn 100):
  Heap (5,3): profile = ToastPointer(va_valueid=5678)
  TOAST chunks: (100,1), (100,2), ... (xmin=100, va_valueid=5678)
  Index: [Key="old text", TID=(5,3), xmin=100, xmax=0]

Update (txn 200):
  1. TOAST new value → chunks (200,1), (200,2), ... (xmin=200, va_valueid=9999)
  2. Create back version:
     Heap (7,12): profile = ToastPointer(va_valueid=5678)  ← Points to OLD chunks
  3. Update primary tuple in-place:
     Heap (5,3): profile = ToastPointer(va_valueid=9999)  ← Points to NEW chunks
                 back_ptr = (7,12), xmin=200
  4. Mark old chunks deleted:
     Chunks (100,1), (100,2), ... : xmax=200
  5. Index update:
     Soft delete old entry: [Key="old text", TID=(5,3), xmin=100, xmax=200]
     Insert new entry: [Key="new text", TID=(5,3), xmin=200, xmax=0]

Transaction 150 (started before update):
  - Index search finds both entries
  - Visibility: entry with xmin=100 visible (committed, < 150)
               entry with xmin=200 NOT visible (>= 150)
  - Returns TID=(5,3) from old entry
  - Heap fetch at (5,3) finds xmin=200 not visible
  - Traverses back version chain to (7,12)
  - Finds ToastPointer(va_valueid=5678)
  - Fetches TOAST chunks with va_valueid=5678
  - Checks chunk visibility: xmin=100 visible, xmax=200 not committed yet
  - Chunks visible! Reassembles old value
  - Returns old profile text to transaction 150
```

**Critical MGA Requirement**: Index must store heap TID (5,3), not chunk TID, to enable back version chain traversal.

### 4.3 Garbage Collection (Sweep)

**Sweep Process with Option 3**:

```
Scenario: OIT advances from 50 to 300

Heap Tuple (5,3):
  Current version: xmin=200 (visible)
  Back version (7,12): xmin=100, xmax=200 (garbage - deleted and before OIT=300)

TOAST Chunks:
  Current: (200,1), (200,2), ... (xmin=200, xmax=0) - visible
  Old: (100,1), (100,2), ... (xmin=100, xmax=200) - garbage

Index Entries:
  Current: [Key="new text", TID=(5,3), xmin=200, xmax=0] - visible
  Old: [Key="old text", TID=(5,3), xmin=100, xmax=200] - garbage

Sweep operations:
  1. Remove back version heap tuple (7,12)
  2. Remove old TOAST chunks (100,1), (100,2), ...
  3. Remove old index entry [Key="old text", xmin=100, xmax=200]

Result:
  ✅ Heap cleaned
  ✅ TOAST chunks cleaned
  ✅ Index cleaned
  ✅ All garbage removed in coordinated sweep
```

**Key Point**: Option 3 allows coordinated GC of heap, TOAST chunks, and index entries.

---

## Part 5: Recommendations

### 5.1 Architectural Choice

**✅ RECOMMENDED: Option 3 (Detoast Before Index Insert)**

**Justification**:
1. Only option that is MGA-compliant
2. Only option where indexes function correctly
3. Only option with stable TIDs
4. Only option that supports back versioning
5. Only option that supports coordinated garbage collection

**❌ REJECTED: Option 1 (Store Pointer Bytes)**
- Indexes become useless (searches fail)
- Range queries nonsensical
- Uniqueness constraints broken
- Not a viable option

**❌ REJECTED: Option 2 (Store Chunk TIDs)**
- Violates TID stability principle
- Breaks back version navigation
- Incompatible with garbage collection
- Incompatible with executor expectations
- Fundamentally incompatible with MGA

### 5.2 Implementation Strategy

**Phase 3 Revised Approach**:

1. **Create `IndexKeyExtractor` helper** (storage layer)
   - Detects TOAST pointers in column values
   - Detoasts values automatically
   - Caches detoasted values for multiple indexes
   - Returns "index-ready" keys

2. **Modify storage engine insert path**
   - After TOASTing large columns
   - Before index updates
   - Use `IndexKeyExtractor` to prepare keys
   - Pass detoasted keys to indexes

3. **Modify storage engine update path**
   - Extract old key from old tuple (pre-update)
   - Extract new key from new tuple (post-update)
   - Both keys are detoasted
   - Pass to index for delete/insert

4. **Indexes remain unchanged**
   - No TOAST awareness needed
   - Receive actual values as keys
   - Store heap TIDs as always
   - Complete separation of concerns

### 5.3 Performance Optimizations

**Optimization 1: Detoasting Cache**
```cpp
// Cache detoasted values during multi-index updates
class UpdateContext {
    std::unordered_map<uint16_t, std::vector<uint8_t>> detoasted_cache;
public:
    const std::vector<uint8_t>& getDetoasted(uint16_t col_id, ...) {
        if (!detoasted_cache.contains(col_id)) {
            detoastValue(..., &detoasted_cache[col_id], ...);
        }
        return detoasted_cache[col_id];
    }
};
```
**Benefit**: 1 detoast per column instead of N detoasts for N indexes

**Optimization 2: Prefix Compression in B-tree**
```cpp
// B-tree prefix compression for large text values
// Common prefix: "https://example.com/"
// Entry 1: "https://example.com/page1" → prefix=20, suffix="page1"
// Entry 2: "https://example.com/page2" → prefix=20, suffix="page2"
```
**Benefit**: 80-90% space savings for text data

**Optimization 3: Partial Indexing**
```cpp
// Index only first N bytes of large values
std::vector<uint8_t> partial_key;
if (detoasted_value.size() > 1000) {
    partial_key.assign(detoasted_value.begin(),
                      detoasted_value.begin() + 1000);
} else {
    partial_key = detoasted_value;
}
index->insert(partial_key, tid, xid);
```
**Benefit**: 98% space savings for very large values

**Optimization 4: Lazy Detoasting on Read**
```cpp
// Don't detoast if query doesn't need the column
if (query.projection.contains(column_id)) {
    // Detoast only if needed for result
    detoastValue(...);
} else {
    // Skip detoasting - column not in SELECT list
}
```
**Benefit**: Avoid unnecessary detoasting on reads

---

## Part 6: Conclusion

### 6.1 Summary

**The Three Options**:
1. **Option 1 (Store Pointer Bytes)**: ❌ Unusable - searches fail
2. **Option 2 (Store Chunk TIDs)**: ❌ MGA-incompatible - violates core principles
3. **Option 3 (Detoast Before Insert)**: ✅ Only viable option

**Key Findings**:

1. **Indexes must store actual values, not pointers**
   - Searching for "Alice" must compare to "Alice", not pointer bytes
   - Range queries, sorting, uniqueness all require actual values

2. **Indexes must store heap TIDs, not chunk TIDs**
   - MGA requires stable TIDs pointing to primary heap location
   - Back version navigation requires heap TID chain
   - Garbage collection coordinated via heap TIDs

3. **Detoasting happens in storage layer, not index layer**
   - Indexes remain simple and TOAST-unaware
   - Storage layer handles all TOAST complexity
   - `IndexKeyExtractor` helper provides clean interface

4. **Performance trade-off is acceptable**
   - Insert: 1 detoast per value (cached across indexes)
   - Search: 0 detoasts (index has actual values)
   - Update: 2 detoasts per value (old + new)
   - Space: Mitigated by compression and partial indexing

### 6.2 Answer to Question 4

**Question**: "Analyze the different options that TOAST records are used in indexes - how they are pointed to in indexes, what makes a toast record different from a regular record when considering MGA design"

**Answer**:

**Three Options Analyzed**:
1. Store 18-byte pointer in index → Searches fail (architecturally broken)
2. Store chunk TID in index → Violates MGA TID stability (architecturally incompatible)
3. Store detoasted value + heap TID → Only MGA-compliant option (recommended)

**What Makes TOAST Different**:
- **Storage**: TOAST uses out-of-line chunks, regular uses inline data
- **Index Key**: Both must store actual value (TOAST requires detoasting first)
- **Index TID**: Both store heap tuple TID (never chunk TID for TOAST)
- **MGA Behavior**: Identical - both follow same visibility, versioning, GC rules

**Critical Insight**: **From the index's perspective, TOAST and regular records are identical.** The difference is entirely in the storage layer, which must detoast before indexing. Indexes remain unaware of whether a value came from inline storage or TOAST chunks.

---

**Document Status**: COMPREHENSIVE ANALYSIS COMPLETE
**Date**: November 3, 2025
**Recommendation**: Implement Option 3 with optimizations in Phase 3

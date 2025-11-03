# TOAST Records in Indexes: Differences from Regular Records in Firebird MGA

**Date**: November 3, 2025
**Purpose**: Analyze how TOAST records differ from regular records when considering indexes in Firebird MGA architecture
**Status**: Analysis Document

---

## Executive Summary

This document analyzes the fundamental differences between TOAST (TOASTed/out-of-line) records and regular (inline) records in the context of index operations within Firebird's Multi-Generational Architecture (MGA). The analysis reveals that **TOAST records and regular records are treated identically by indexes in MGA** - the key architectural insight is that indexes always point to the **primary heap tuple location**, never directly to TOAST chunks.

---

## Part 1: What Makes a TOAST Record Different from a Regular Record?

### 1.1 Storage Location

**Regular Record (Inline)**:
```
Heap Tuple at (Page 5, Slot 3):
┌────────────────────────────────────────┐
│ Tuple Header (rhd)                     │
│   xmin: 100                             │
│   back_ptr: (7,12)                      │
│   flags: rhd_chain                      │
├────────────────────────────────────────┤
│ Column 1: id = 1234 (4 bytes)          │
│ Column 2: name = "Alice" (5 bytes)     │
│ Column 3: data = <500 bytes of data>   │ ← Inline data
└────────────────────────────────────────┘
```

**TOAST Record (Out-of-Line)**:
```
Main Heap Tuple at (Page 5, Slot 3):
┌────────────────────────────────────────┐
│ Tuple Header (rhd)                     │
│   xmin: 100                             │
│   back_ptr: (7,12)                      │
│   flags: rhd_chain                      │
├────────────────────────────────────────┤
│ Column 1: id = 1234 (4 bytes)          │
│ Column 2: name = "Alice" (5 bytes)     │
│ Column 3: ToastPointer (18 bytes)      │ ← TOAST pointer, not actual data
│   va_header: 0x01 (TOAST marker)       │
│   va_valueid: 5678                      │
│   va_rawsize: 50000                     │
│   va_extsize: 25000                     │
└────────────────────────────────────────┘
           │
           │ Points to TOAST chunks in separate TOAST table
           ↓
TOAST Table (pg_toast_<table_id>):
┌────────────────────────────────────────┐
│ Chunk 0 at (Page 100, Slot 1):         │
│   xmin: 100                             │
│   xmax: 0                               │
│   chunk_id: 5678                        │
│   chunk_seq: 0                          │
│   chunk_data: [2000 bytes]              │
├────────────────────────────────────────┤
│ Chunk 1 at (Page 100, Slot 2):         │
│   xmin: 100                             │
│   xmax: 0                               │
│   chunk_id: 5678                        │
│   chunk_seq: 1                          │
│   chunk_data: [2000 bytes]              │
├────────────────────────────────────────┤
│ ... (more chunks) ...                   │
└────────────────────────────────────────┘
```

**Key Difference**:
- Regular record: Actual data stored in heap tuple
- TOAST record: Heap tuple contains **18-byte ToastPointer**, actual data stored in separate TOAST table chunks

### 1.2 Size Characteristics

**Regular Record**:
- Column data size: Variable, but typically small (< 2KB per column)
- Total tuple size: Sum of all column sizes + tuple header
- Storage: Single heap page (if tuple fits)

**TOAST Record**:
- Main tuple column size: Fixed 18 bytes (ToastPointer) regardless of actual data size
- TOAST data size: Can be gigabytes (stored in chunks of ~2KB each)
- Storage: Main tuple on heap page + N chunks in TOAST table
- Chunk threshold: Values > 2KB are candidates for TOASTing

### 1.3 Transaction Visibility

**Regular Record** (MGA):
```cpp
// Heap tuple visibility
bool isVisible(HeapTuple* tuple, TransactionId reader_xid) {
    // Check tuple's xmin via TIP
    TxState xmin_state = getTransactionState(tuple->xmin);

    // Own changes always visible
    if (tuple->xmin == reader_xid) return true;

    // Committed and older than reader
    if (xmin_state == TX_COMMITTED && tuple->xmin < reader_xid) {
        // Check if deleted
        if (tuple->xmax != 0) {
            TxState xmax_state = getTransactionState(tuple->xmax);
            if (xmax_state == TX_COMMITTED && tuple->xmax < reader_xid) {
                return false;  // Deleted
            }
        }
        return true;  // Visible
    }

    return false;  // Not visible
}
```

**TOAST Record** (MGA):
```cpp
// TOAST chunk visibility (SAME LOGIC as heap tuples)
bool isChunkVisible(ToastChunk* chunk, TransactionId reader_xid) {
    // Check chunk's xmin via TIP
    TxState xmin_state = getTransactionState(chunk->xmin);

    // Own changes always visible
    if (chunk->xmin == reader_xid) return true;

    // Committed and older than reader
    if (xmin_state == TX_COMMITTED && chunk->xmin < reader_xid) {
        // Check if deleted
        if (chunk->xmax != 0) {
            TxState xmax_state = getTransactionState(chunk->xmax);
            if (xmax_state == TX_COMMITTED && chunk->xmax < reader_xid) {
                return false;  // Deleted
            }
        }
        return true;  // Visible
    }

    return false;  // Not visible
}
```

**Key Point**: TOAST chunks follow **identical MGA visibility rules** as heap tuples:
- Track xmin (creating transaction) and xmax (deleting transaction)
- Use TIP for transaction state lookups
- Apply same visibility logic: committed + older = visible

---

## Part 2: How Indexes Point to TOAST Records

### 2.1 The Critical Insight: Indexes Point to Heap Tuples, NOT TOAST Chunks

**Firebird MGA Design Principle**:
> Indexes always store **stable TIDs (Tuple Identifiers)** pointing to the primary heap tuple location. Indexes NEVER point directly to TOAST chunks or back versions.

**Why This Matters**:
```
Index Entry Structure:
┌──────────────────────────────────────┐
│ Index Key: "Alice"                   │
│ TID: (Page=5, Slot=3)                │ ← Points to PRIMARY heap tuple
└──────────────────────────────────────┘
           │
           ↓
Heap Tuple at (Page 5, Slot 3):
┌──────────────────────────────────────┐
│ Column 2: name = "Alice"              │
│ Column 3: ToastPointer                │ ← Index doesn't care about this
│   va_valueid: 5678                    │
└──────────────────────────────────────┘
```

**Key Properties**:
1. Index knows NOTHING about TOAST pointers
2. Index just stores: `(key_value, tid)` pairs
3. TID always points to primary heap tuple (never to TOAST chunks or back versions)
4. TID remains **stable** across updates (unless indexed column changes)

### 2.2 What Gets Indexed: The Actual Value, Not the Pointer

**CRITICAL ARCHITECTURAL QUESTION**: When a column contains a TOAST pointer, what value does the index store?

**Answer**: The index stores the **actual detoasted value**, NOT the 18-byte ToastPointer bytes.

**Example**:

```
Heap Tuple:
  Column: large_text = ToastPointer(va_valueid=5678, va_rawsize=50000)

WRONG Index Entry (if we indexed pointer bytes):
  Key: [0x01, 0x00, 0x16, 0x2E, ...] (18 bytes of pointer)  ← USELESS
  TID: (5, 3)

CORRECT Index Entry (index actual value):
  Key: "This is a very long text that was TOASTed..." (actual text)
  TID: (5, 3)
```

**Why Indexing Pointer Bytes is Wrong**:
1. Searching for "Alice" would never match pointer bytes
2. Range queries would sort by pointer values, not actual data
3. Uniqueness constraints would fail
4. Full-text search would be impossible

**Why Indexing Actual Value is Correct**:
1. Index lookups work: `SELECT * WHERE name = 'Alice'` finds the row
2. Range queries work: `SELECT * WHERE name > 'A' AND name < 'B'`
3. Sorting works: `ORDER BY name`
4. Uniqueness works: `UNIQUE INDEX` prevents duplicate values

### 2.3 When Does Detoasting Happen for Indexes?

**Insert Path**:
```
1. Application inserts row with large column
   INSERT INTO users (name, profile) VALUES ('Alice', <50KB text>);

2. Storage engine determines column should be TOASTed
   - Column size > 2KB threshold
   - toastValue() called, returns ToastPointer

3. Heap tuple created with ToastPointer
   - Tuple at (Page 5, Slot 3)
   - Column: profile = ToastPointer(va_valueid=5678)

4. Index insert called
   - Index on profile column
   - Question: What gets indexed?

5. Two possible approaches:

   APPROACH A (WRONG - Index pointer bytes):
   index->insert(pointer_bytes, tid, xid)
   ❌ Index stores: Key=[0x01, 0x00, 0x16, ...], TID=(5,3)

   APPROACH B (CORRECT - Detoast before indexing):
   if (isToastPointer(value)) {
       detoastValue(value, &actual_data, xid);
       index->insert(actual_data, tid, xid);
   }
   ✓ Index stores: Key="<actual 50KB text>", TID=(5,3)
```

**Search Path**:
```
1. Application searches
   SELECT * FROM users WHERE profile LIKE '%keyword%';

2. Query executor builds search key
   search_key = '%keyword%'

3. Index search
   - Index already has actual detoasted values as keys
   - Standard index search: find matching keys
   - Returns TIDs: [(5,3), (7,2), ...]

4. Heap tuple fetch
   - For each TID, fetch heap tuple
   - Heap tuple contains ToastPointer
   - Detoast if application needs actual value
   - Return to application
```

---

## Part 3: Index Insert Operation with TOAST

### 3.1 Current Implementation Analysis

**File**: `src/core/btree.cpp` (B-tree insert)

```cpp
// Current implementation (Phase 1.5 Task 1.5.2a)
Status BTree::insert(const std::vector<uint8_t> &key, const TID &tid,
                     uint64_t xid, ErrorContext *ctx)
{
    // ... lock acquisition ...

    // Insert key directly - NO detoasting check
    Status status = insertInternal(key, tid, xid, ctx);

    return status;
}
```

**Problem**: The `key` parameter might contain ToastPointer bytes (18 bytes) instead of actual value.

**Question**: Where should detoasting happen?

### 3.2 Options for TOAST Integration with Indexes

#### Option 1: Detoast at Index Insert Time (Storage Layer)

**Approach**: Storage engine detoasts before calling index insert

```cpp
// In storage_engine.cpp or heap_page.cpp
Status insertTuple(table_id, tuple_data, ...) {
    // 1. TOAST large columns
    for (each column in tuple) {
        if (shouldToast(column)) {
            ToastPointer pointer;
            toastValue(column, &pointer, xid);
            column = pointer;  // Replace with pointer
        }
    }

    // 2. Insert heap tuple (with pointers)
    insertHeapTuple(tuple_data, &tid);

    // 3. Update indexes
    for (each index on table) {
        std::vector<uint8_t> key;

        // Extract indexed column(s) from tuple
        for (each indexed_column) {
            if (isToastPointer(column_value)) {
                // DETOAST before indexing
                std::vector<uint8_t> actual_value;
                detoastValue(column_value, &actual_value, xid);
                key.append(actual_value);
            } else {
                // Regular inline value
                key.append(column_value);
            }
        }

        // Insert actual value into index, NOT pointer
        index->insert(key, tid, xid);
    }
}
```

**Pros**:
- Index code remains unchanged
- Indexes always receive actual values
- Separation of concerns: storage layer handles TOAST

**Cons**:
- Detoasting overhead on every insert
- Repeated detoasting if multiple indexes on same column
- Performance cost even if data already detoasted

#### Option 2: Detoast Inside Index Insert (Index Layer)

**Approach**: Index detects TOAST pointer and detoasts

```cpp
// In btree.cpp
Status BTree::insert(const std::vector<uint8_t> &key, const TID &tid,
                     uint64_t xid, ErrorContext *ctx)
{
    // Check if key is a TOAST pointer
    std::vector<uint8_t> actual_key;
    if (ToastManager::isToastPointer(key.data(), key.size())) {
        // Detoast value
        ToastManager *toast_mgr = getToastManager();
        Status status = toast_mgr->detoastValue(
            (ToastPointer*)key.data(), &actual_key, xid, ctx);
        if (status != Status::OK) {
            return status;
        }
    } else {
        actual_key = key;
    }

    // Insert actual value, not pointer
    Status status = insertInternal(actual_key, tid, xid, ctx);
    return status;
}
```

**Pros**:
- Index is self-contained
- Storage layer doesn't need TOAST knowledge
- Each index can optimize detoasting

**Cons**:
- Every index type must implement detoasting
- Code duplication across 7 index types
- Index needs ToastManager reference
- Violates separation of concerns

#### Option 3: Hybrid Approach (Recommended)

**Approach**: Storage layer provides "index-ready" key extractor

```cpp
// In storage_engine.cpp or new index_helper.cpp
class IndexKeyExtractor {
public:
    // Extract index key with automatic detoasting
    Status extractKey(
        const HeapTuple* tuple,
        const std::vector<uint16_t>& column_indices,
        ToastManager* toast_mgr,
        TransactionId xid,
        std::vector<uint8_t>* key_out,
        ErrorContext* ctx)
    {
        key_out->clear();

        for (uint16_t col_idx : column_indices) {
            const uint8_t* col_data = getColumnData(tuple, col_idx);
            size_t col_size = getColumnSize(tuple, col_idx);

            if (ToastManager::isToastPointer(col_data, col_size)) {
                // Column is TOASTed, detoast it
                std::vector<uint8_t> detoasted;
                Status status = toast_mgr->detoastValue(
                    (ToastPointer*)col_data, &detoasted, xid, ctx);
                if (status != Status::OK) {
                    return status;
                }
                key_out->insert(key_out->end(),
                               detoasted.begin(), detoasted.end());
            } else {
                // Regular inline value
                key_out->insert(key_out->end(),
                               col_data, col_data + col_size);
            }
        }

        return Status::OK;
    }
};

// In storage_engine.cpp
Status insertTuple(table_id, tuple_data, ...) {
    // ... TOAST large columns ...
    // ... insert heap tuple ...

    // Update indexes
    IndexKeyExtractor extractor;
    for (each index on table) {
        std::vector<uint8_t> key;
        Status status = extractor.extractKey(
            tuple, index.column_indices, toast_mgr, xid, &key, ctx);
        if (status != Status::OK) {
            return status;
        }

        // Index receives actual value
        index->insert(key, tid, xid);
    }
}
```

**Pros**:
- Clean separation of concerns
- Indexes remain simple (no TOAST knowledge)
- Centralized detoasting logic
- Easy to optimize (caching, batching)
- Storage layer controls when/how detoasting happens

**Cons**:
- Additional layer of abstraction
- Slight complexity in storage layer

---

## Part 4: Index Search Operation with TOAST

### 4.1 Search Path Analysis

**Query**: `SELECT * FROM users WHERE profile = 'some_text';`

**Execution Steps**:
```
1. Query planner decides to use index on 'profile'

2. Executor builds search key
   search_key = "some_text"  ← Actual value, not TOAST pointer

3. Index search
   index->search(search_key, current_xid, &tids)

   - Index already has actual values as keys (detoasted at insert time)
   - Standard B-tree search finds matching keys
   - Returns TIDs: [(5, 3)]

4. Heap tuple fetch
   for each TID:
       tuple = heapFetch(tid, current_xid)

       // Tuple might contain TOAST pointers
       // Question: When to detoast for result?

5. Result assembly
   - If query SELECT * : Must detoast all TOASTed columns
   - If query SELECT id : No need to detoast profile column
   - If query SELECT profile : Must detoast profile column
```

**Key Insight**: Index search doesn't need to handle TOAST at all!
- Index already has actual values (detoasted at insert)
- Search operates on actual values
- Returns TIDs to heap tuples

### 4.2 Detoasting on Read: When and Where?

**Scenarios**:

**Scenario 1: Query doesn't need TOASTed column**
```sql
SELECT id, name FROM users WHERE profile = 'keyword';
```
- Index search on profile (actual values)
- Heap fetch retrieves tuple
- Projection only needs id, name columns
- **No detoasting needed** for profile

**Scenario 2: Query needs TOASTed column**
```sql
SELECT id, name, profile FROM users WHERE profile = 'keyword';
```
- Index search on profile
- Heap fetch retrieves tuple
- Projection needs profile column
- **Detoasting required**:
  ```cpp
  if (isToastPointer(profile_column)) {
      detoastValue(profile_column, &actual_value, xid);
      result.profile = actual_value;
  }
  ```

**Scenario 3: Range scan on TOASTed column**
```sql
SELECT * FROM users WHERE profile > 'A' AND profile < 'Z' ORDER BY profile;
```
- Index range scan (on actual values)
- Index returns multiple TIDs
- Heap fetch for each TID
- Detoast each profile value for result

**Optimization**: Lazy detoasting
```cpp
// Only detoast if application actually accesses the value
class LazyToastValue {
    ToastPointer pointer;
    mutable std::optional<std::vector<uint8_t>> cached_value;

    const std::vector<uint8_t>& getValue() const {
        if (!cached_value) {
            cached_value = detoastValue(pointer);
        }
        return *cached_value;
    }
};
```

---

## Part 5: Index Update Operation with TOAST

### 5.1 Update Scenarios

**Firebird MGA Update Model**: In-place modification + back version

**Scenario 1: Update non-indexed column**
```sql
UPDATE users SET email = 'new@email.com' WHERE id = 1;
-- Indexed columns: id (primary key)
-- Updated column: email (not indexed)
```

**MGA Behavior**:
```
BEFORE:
Heap Tuple at (5, 3):
  id: 1
  email: "old@email.com"

Index on id:
  Key: 1, TID: (5, 3)

UPDATE executes:
1. Create back version
   Back Version at (7, 12):
     id: 1
     email: "old@email.com"

2. Modify primary tuple in-place
   Heap Tuple at (5, 3):
     id: 1
     email: "new@email.com"
     back_ptr: (7, 12)
     xmin: 200

3. Index update?
   ✓ NO INDEX UPDATE NEEDED
   - Index still points to (5, 3)
   - TID is stable
   - Indexed column (id) unchanged
```

**Scenario 2: Update indexed column (no TOAST)**
```sql
UPDATE users SET name = 'Bob' WHERE id = 1;
-- Indexed columns: id (primary key), name (indexed)
```

**MGA Behavior**:
```
BEFORE:
Heap Tuple at (5, 3):
  id: 1
  name: "Alice"

Index on name:
  Key: "Alice", TID: (5, 3)

UPDATE executes:
1. Create back version
   Back Version at (7, 12):
     id: 1
     name: "Alice"

2. Modify primary tuple in-place
   Heap Tuple at (5, 3):
     id: 1
     name: "Bob"
     back_ptr: (7, 12)
     xmin: 200

3. Index update
   ✓ INDEX UPDATE REQUIRED (indexed column changed)

   A. Delete old index entry (soft delete)
      index->softDelete("Alice", (5,3), 200)
      - Mark entry with xmax=200
      - Entry becomes invisible to txns >= 200

   B. Insert new index entry
      index->insert("Bob", (5,3), 200)
      - New entry with xmin=200
      - Points to SAME TID (5,3)
```

**Scenario 3: Update indexed TOASTed column**
```sql
UPDATE users SET profile = <new 50KB text> WHERE id = 1;
-- Indexed columns: profile (indexed, TOASTed)
```

**MGA Behavior**:
```
BEFORE:
Heap Tuple at (5, 3):
  id: 1
  profile: ToastPointer(va_valueid=5678, rawsize=50000)

Index on profile:
  Key: "<old 50KB text>", TID: (5, 3)

TOAST Table:
  Chunks for va_valueid=5678 (old profile)

UPDATE executes:
1. TOAST new value
   - Creates new TOAST chunks (va_valueid=9999)
   - New TOAST chunks have xmin=200

2. Create back version
   Back Version at (7, 12):
     id: 1
     profile: ToastPointer(va_valueid=5678)  ← Points to OLD chunks

3. Modify primary tuple in-place
   Heap Tuple at (5, 3):
     id: 1
     profile: ToastPointer(va_valueid=9999)  ← Points to NEW chunks
     back_ptr: (7, 12)
     xmin: 200

4. Mark old TOAST chunks as deleted
   - Set xmax=200 on all chunks with va_valueid=5678
   - Chunks become invisible to txns >= 200 (after commit)

5. Index update
   ✓ INDEX UPDATE REQUIRED (indexed column changed)

   A. Detoast OLD value for index delete
      detoastValue(va_valueid=5678, &old_text, 200)

   B. Delete old index entry
      index->softDelete(old_text, (5,3), 200)

   C. Detoast NEW value for index insert
      detoastValue(va_valueid=9999, &new_text, 200)

   D. Insert new index entry
      index->insert(new_text, (5,3), 200)
```

**Key Observations**:
1. TID remains stable (5,3) - no change
2. TOAST pointer in tuple changes (va_valueid: 5678 → 9999)
3. Old TOAST chunks marked deleted (xmax=200)
4. New TOAST chunks created (xmin=200)
5. Index needs BOTH old and new detoasted values
6. Index delete requires old value (not available in new tuple!)

### 5.2 The Challenge: Getting Old Value for Index Delete

**Problem**: When updating indexed column, index needs to delete old entry

```cpp
// Pseudocode for update
Status updateTuple(table_id, tid, new_values, xid) {
    // 1. Fetch current tuple
    HeapTuple* old_tuple = heapFetch(tid, xid);

    // 2. For each index
    for (Index* index : table.indexes) {
        if (indexed_column_changed(index, old_tuple, new_values)) {
            // Need to delete old index entry
            // Question: What key to use?

            // CHALLENGE: If column is TOASTed, old_tuple contains ToastPointer
            // Must detoast old value to get key
            std::vector<uint8_t> old_key;
            extractIndexKey(old_tuple, index.columns, &old_key, xid);
            // ^ This might need to detoast old value

            // Delete old entry
            index->softDelete(old_key, tid, xid);

            // Insert new entry
            std::vector<uint8_t> new_key;
            extractIndexKey(new_values, index.columns, &new_key, xid);
            // ^ This might need to detoast new value

            index->insert(new_key, tid, xid);
        }
    }
}
```

**Solution Approaches**:

**Approach A: Detoast old value before update**
```cpp
// Before creating back version, detoast any TOASTed indexed columns
for (each indexed column that changed) {
    if (isToastPointer(old_column_value)) {
        detoasted_old_value = detoastValue(old_column_value, xid);
        cache_old_value(column_id, detoasted_old_value);
    }
}

// Later, when deleting index entry
std::vector<uint8_t> old_key = get_cached_old_value(column_id);
index->softDelete(old_key, tid, xid);
```

**Approach B: Keep old tuple accessible**
```cpp
// After creating back version, back version is still accessible
// Can detoast from back version if needed
HeapTuple* back_version = fetchBackVersion(old_tuple->back_ptr);
if (isToastPointer(back_version->column)) {
    detoasted_old_value = detoastValue(back_version->column, xid);
}
index->softDelete(detoasted_old_value, tid, xid);
```

**Approach C: Store detoasted value in index initially**
```cpp
// During initial insert, index stores actual value (already detoasted)
// During update, old index entry still exists with old actual value
// Just need to mark it deleted (xmax), don't need to detoast
index->markDeleted(tid, xid);  // Find entry by TID, set xmax
```

**Recommended**: Approach A with caching
- Detoast old values once before update
- Cache in update operation context
- Use cached values for index deletes
- Avoids repeated detoasting

---

## Part 6: Summary of Differences Between TOAST and Regular Records in Indexes

### 6.1 Key Differences

| Aspect | Regular Record | TOAST Record |
|--------|---------------|--------------|
| **Storage in Heap** | Column data inline | 18-byte ToastPointer |
| **Index Key Value** | Actual data | Actual data (detoasted) |
| **Index Insert** | Direct insert | Must detoast first |
| **Index Search** | Standard search | Standard search (index has actual values) |
| **Index Update** | Extract old/new values directly | Must detoast old/new values |
| **TID Stability** | Stable (unless indexed col changes) | Stable (same as regular) |
| **Back Versioning** | Back version has old data | Back version has old ToastPointer |
| **Garbage Collection** | Sweep removes back versions | Sweep removes back versions + old TOAST chunks |

### 6.2 Architectural Principles

**Principle 1: Indexes are Unaware of TOAST**
- Indexes store actual detoasted values as keys
- Indexes never see or store ToastPointer bytes
- Detoasting happens before index insert, outside index code

**Principle 2: TIDs Always Point to Primary Heap Tuple**
- Index TIDs point to heap tuple location (Page, Slot)
- Heap tuple contains ToastPointer (if column is TOASTed)
- Indexes never point to TOAST chunks directly

**Principle 3: TOAST Chunks Follow MGA Rules**
- TOAST chunks have xmin/xmax like heap tuples
- TOAST chunks use TIP-based visibility
- TOAST chunks participate in back-versioning
- TOAST chunks cleaned by garbage collection (sweep)

**Principle 4: Detoasting is Lazy Where Possible**
- Index insert: Must detoast (index needs actual value)
- Index search: No detoasting (index already has actual values)
- Heap fetch: Detoast only if application needs value
- Update: Detoast old/new values for index update

### 6.3 The Answer to Phase 3 "Impossibility"

**Original Phase 3 Plan Statement**:
> "Fix all 7 index types to detoast values before indexing"

**Why This Seemed Impossible**:
- Each index type would need ToastManager reference
- Code duplication across 7 index implementations
- Indexes would need to know about TOAST internals
- Violates separation of concerns

**The Actual Solution**:
- **Indexes should NOT detoast values themselves**
- **Storage layer detoasts before calling index insert**
- Use `IndexKeyExtractor` helper class (hybrid approach)
- Indexes remain simple: just store (key, TID) pairs
- Storage layer handles all TOAST complexity

**Correct Phase 3 Approach**:
1. Create `IndexKeyExtractor` class in storage layer
2. Modify storage engine's index update path:
   - After TOASTing large columns
   - Before calling `index->insert()`
   - Extract index key with automatic detoasting
3. Indexes receive "index-ready" keys (already detoasted)
4. No changes to index implementations

---

## Part 7: Recommendations for Implementation

### 7.1 Phase 3 Revised Plan

**Phase 3: Storage Layer TOAST-to-Index Integration** (not "Index TOAST Integration")

**Duration**: 20-30 hours (not 40-60 hours)

**Tasks**:

#### Task 3.1: Create IndexKeyExtractor Helper Class
**File**: `src/core/index_key_extractor.cpp` (NEW)
**Duration**: 5-7 hours

```cpp
class IndexKeyExtractor {
public:
    Status extractKey(
        const HeapTuple* tuple,
        const std::vector<uint16_t>& column_indices,
        ToastManager* toast_mgr,
        TransactionId xid,
        std::vector<uint8_t>* key_out,
        ErrorContext* ctx);

    Status extractKeyForUpdate(
        const HeapTuple* old_tuple,
        const HeapTuple* new_tuple,
        const std::vector<uint16_t>& column_indices,
        ToastManager* toast_mgr,
        TransactionId xid,
        std::vector<uint8_t>* old_key_out,
        std::vector<uint8_t>* new_key_out,
        ErrorContext* ctx);
};
```

#### Task 3.2: Integrate with Storage Engine Insert Path
**File**: `src/core/storage_engine.cpp`
**Duration**: 6-8 hours

- Modify `insertTuple()` to use `IndexKeyExtractor`
- Extract detoasted keys before calling `index->insert()`
- Handle errors from detoasting

#### Task 3.3: Integrate with Storage Engine Update Path
**File**: `src/core/storage_engine.cpp` or `src/core/heap_page.cpp`
**Duration**: 8-12 hours

- Modify `updateTuple()` to use `IndexKeyExtractor`
- Extract old key (from old tuple, pre-update)
- Extract new key (from new tuple, post-update)
- Handle index deletes and inserts

#### Task 3.4: Add Detoasting Performance Optimizations
**Duration**: 3-5 hours

- Cache detoasted values during update operation
- Avoid repeated detoasting for multiple indexes on same column
- Add metrics/logging for detoasting overhead

### 7.2 Phase 5 Removal

**Action**: Remove Phase 5 entirely from plan
**Reason**: MGA does not use WAL for core operations

**Affected Sections in Plan**:
- Lines 1317-1423 in TOAST_MGA_COMPLIANCE_FIX_PLAN.md
- Remove Phase 5 from progress tracking table
- Update total estimated hours

### 7.3 Updated Project Roadmap

**Phase 0**: ✅ COMPLETE (7 hours)
**Phase 1**: ✅ COMPLETE (18 hours) - Chunk format redesign
**Phase 2**: ✅ COMPLETE (15 hours) - TIP visibility
**Phase 3**: ⏳ IN PROGRESS (20-30 hours) - Storage layer integration
**Phase 4**: ⏳ PENDING (25-35 hours) - Garbage collection
**Phase 5**: ❌ REMOVED (was WAL integration)
**Phase 6**: ⏳ PENDING (20-30 hours) - Testing
**Phase 7**: ⏳ PENDING (15-20 hours) - Documentation

**Total Estimated Effort**: 120-165 hours (revised from 160-238 hours)

---

## Part 8: Conclusions

### 8.1 Core Findings

1. **TOAST records and regular records are nearly identical from index perspective**
   - Indexes always store actual values (detoasted for TOAST)
   - Indexes always point to primary heap tuple TID
   - TID stability principle applies equally to both

2. **The "difference" is in the storage layer, not index layer**
   - Heap tuples contain either inline data or ToastPointer
   - Storage layer must detoast before indexing
   - Indexes are completely unaware of TOAST

3. **Phase 3 is about storage layer integration, not index changes**
   - Indexes don't need modification
   - Storage layer needs `IndexKeyExtractor` helper
   - Much simpler than originally thought

4. **MGA principles apply uniformly**
   - TOAST chunks follow same MGA rules as heap tuples
   - xmin/xmax tracked on chunks
   - TIP-based visibility for chunks
   - Garbage collection via sweep

### 8.2 Answer to the Core Question

**Question**: "What makes a TOAST record different from a regular record when considering MGA design in indexes?"

**Answer**: **Nothing, from the index's perspective.**

Indexes treat TOAST records identically to regular records:
- Both have stable TIDs pointing to primary heap tuple
- Both require actual values as index keys
- Both follow same MGA visibility rules
- Both trigger index updates only when indexed columns change

The only difference is in the **storage layer**: it must detoast TOAST values before passing them to indexes. This is a storage layer responsibility, not an index responsibility.

**Therefore**: Phase 3 should focus on storage layer integration, not index changes. The original plan's focus on "fixing all 7 index types" was architecturally incorrect.

---

**Document Status**: ANALYSIS COMPLETE
**Date**: November 3, 2025
**Next Steps**: Revise Phase 3 plan based on these findings

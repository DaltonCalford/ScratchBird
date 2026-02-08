# COMPREHENSIVE TOAST (The Oversized-Attribute Storage Technique) AUDIT REPORT

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Database**: ScratchBird
**Audit Date**: November 1, 2025
**Auditor**: Comprehensive Code Audit
**Scope**: Complete TOAST implementation audit across codebase

---

## EXECUTIVE SUMMARY

**Overall TOAST Implementation Status**: **PARTIAL** ⚠️

The TOAST implementation in ScratchBird is **functionally complete for basic operations** but has **critical gaps in MGA compliance and index integration**. While heap storage correctly handles TOASTed values, there are significant deficiencies in garbage collection, index handling, and transaction visibility tracking.

**Critical Findings**:
- ❌ **CRITICAL**: TOAST chunks do NOT properly track xmin/xmax in on-disk format
- ❌ **CRITICAL**: TOAST visibility uses snapshot-based model, NOT TIP-based as specified in Firebird MGA
- ⚠️ **HIGH**: Indexes do NOT handle TOAST pointers (may index pointer bytes instead of actual values)
- ⚠️ **HIGH**: Garbage collector does NOT clean up orphaned TOAST chunks
- ✅ **COMPLETE**: Heap page TOAST integration with automatic TOASTing/detoasting
- ✅ **COMPLETE**: Compression support (LZ4)
- ✅ **COMPLETE**: Chunk-based storage with B-tree index

---

## 1. TOAST FILE AUDIT

### 1.1 Core TOAST Files

| File | Path | Lines | Status | Notes |
|------|------|-------|--------|-------|
| **toast.h** | `include/scratchbird/core/toast.h` | 163 | ✅ COMPLETE | Well-structured header with all TOAST structures |
| **toast.cpp** | `src/core/toast.cpp` | 823 | ✅ COMPLETE | Full implementation with compression, chunking, index scan |

**Evidence**:
- Lines 20-24 (toast.h): TOAST constants defined
- Lines 26-32 (toast.h): ToastStrategy enum (PLAIN, EXTENDED, COMPRESSED, EXTERNAL)
- Lines 36-56 (toast.h): ToastPointer and ToastChunk structures
- Lines 59-67 (toast.h): ToastTableEntry with xmin/xmax fields
- Lines 79-146 (toast.h): ToastManager class definition

---

## 2. TOAST IMPLEMENTATION STATUS

### 2.1 Features Implemented ✅

#### 2.1.1 Basic TOAST Operations
**Status**: COMPLETE
**Evidence**:
- `ToastManager::toastValue()` (toast.cpp:212-282): Handles TOASTing with strategy selection
- `ToastManager::detoastValue()` (toast.cpp:284-330): Retrieves and reassembles chunks
- `ToastManager::deleteToastValue()` (toast.cpp:332-403): Deletes TOAST chunks via index scan

**File References**:
- `src/core/toast.cpp:212-282` - TOASTing implementation
- `src/core/toast.cpp:284-330` - Detoasting implementation

#### 2.1.2 Compression Support
**Status**: COMPLETE
**Evidence**:
- `ToastManager::compressData()` (toast.cpp:739-778): LZ4 compression
- `ToastManager::decompressData()` (toast.cpp:780-820): LZ4 decompression
- Uses `CompressionFactory::create(CompressionType::LZ4)` (toast.cpp:743)

**File References**:
- `src/core/toast.cpp:739-778` - Compression
- `src/core/toast.cpp:780-820` - Decompression

#### 2.1.3 Chunk Management
**Status**: COMPLETE
**Evidence**:
- `ToastManager::writeToastChunks()` (toast.cpp:474-555): Splits large values into chunks
- `ToastManager::readToastChunks()` (toast.cpp:557-657): Reassembles chunks using B-tree index
- Chunk cleanup on failure (toast.cpp:540-545): Tracks and cleans up partial writes

**File References**:
- `src/core/toast.cpp:474-555` - Write chunks
- `src/core/toast.cpp:557-657` - Read chunks

#### 2.1.4 TOAST Table Creation
**Status**: COMPLETE
**Evidence**:
- `ToastManager::createToastTable()` (toast.cpp:136-210): Creates table with schema
- Schema: `chunk_id` (INT), `chunk_seq` (INT), `chunk_data` (BYTEA)
- Creates B-tree index on `(chunk_id, chunk_seq)` for efficient retrieval (toast.cpp:196-207)

**File References**:
- `src/core/toast.cpp:136-210` - Create TOAST table
- `src/core/toast.cpp:196-207` - Create index

#### 2.1.5 Value ID Recovery
**Status**: COMPLETE
**Evidence**:
- `ToastManager::initializeNextValueId()` (toast.cpp:62-94): Scans TOAST table to find max value_id
- Prevents value ID collisions after database reopen (toast.cpp:122-128)

**File References**:
- `src/core/toast.cpp:62-94` - Value ID initialization

---

### 2.2 Heap Page Integration ✅

#### 2.2.1 Automatic TOASTing
**Status**: COMPLETE
**Evidence**:
- `HeapPage::insertTuple()` (heap_page.cpp:110-271): Automatically TOASTs values > threshold
- Threshold check: `ToastManager::shouldToast()` (heap_page.cpp:138)
- Creates TOAST pointer in heap tuple (heap_page.cpp:168-178)

**File References**:
- `src/core/heap_page.cpp:110-271` - Insert with TOAST
- `src/core/heap_page.cpp:137-183` - TOAST creation

#### 2.2.2 Automatic Detoasting
**Status**: COMPLETE
**Evidence**:
- `HeapPage::getTupleDetoasted()` (heap_page.cpp:312-386): Automatically detoasts values
- Detects TOAST pointer via `isToastPointer()` (heap_page.cpp:330)
- Calls `ToastManager::detoastValue()` (heap_page.cpp:344)

**File References**:
- `src/core/heap_page.cpp:312-386` - Detoast implementation

#### 2.2.3 TOAST Cleanup on Delete
**Status**: COMPLETE
**Evidence**:
- `HeapPage::deleteTuple()` (heap_page.cpp:388-448): Checks for TOAST pointer
- Deletes TOAST chunks before marking tuple deleted (heap_page.cpp:428)

**File References**:
- `src/core/heap_page.cpp:388-448` - Delete with TOAST cleanup
- `src/core/heap_page.cpp:412-434` - TOAST deletion

#### 2.2.4 TOAST Cleanup on Update
**Status**: COMPLETE
**Evidence**:
- `HeapPage::updateTuple()` (heap_page.cpp:562-921): Deletes old TOAST data during update
- TOAST cleanup before creating back version (heap_page.cpp:617-642)

**File References**:
- `src/core/heap_page.cpp:562-921` - Update with TOAST
- `src/core/heap_page.cpp:617-642` - TOAST cleanup on update

---

### 2.3 Storage Engine Integration ✅

#### 2.3.1 TOAST Manager Caching
**Status**: COMPLETE
**Evidence**:
- `StorageEngine::toast_managers_` (storage_engine.h:165): Per-table TOAST manager cache
- `StorageEngine::getOrCreateToastManager()` (storage_engine.cpp:86): Creates/retrieves managers
- Thread-safe access with `toast_mutex_` (storage_engine.h:166)

**File References**:
- `include/scratchbird/core/storage_engine.h:165-166` - Cache
- `src/core/storage_engine.cpp:86-88` - Manager creation

#### 2.3.2 INSERT Integration
**Status**: COMPLETE
**Evidence**:
- `StorageEngine::insertTuple()` (storage_engine.cpp:32-128): Gets TOAST manager
- Passes TOAST manager to HeapPage (storage_engine.cpp:91)

**File References**:
- `src/core/storage_engine.cpp:32-128` - INSERT with TOAST

---

## 3. CRITICAL MGA COMPLIANCE ISSUES ❌

### 3.1 TOAST Chunk xmin/xmax Tracking

**STATUS**: **CRITICAL FAILURE** ❌
**Severity**: CRITICAL

**Problem**: TOAST chunks are stored WITHOUT proper xmin/xmax tracking in the on-disk format.

**Evidence**:

1. **ToastTableEntry structure has xmin/xmax** (toast.h:59-67):
```cpp
struct ToastTableEntry {
    uint64_t xmin;             // Transaction that created this
    uint64_t xmax;             // Transaction that deleted this (or 0)
    uint32_t value_id;         // Unique TOAST value ID
    uint32_t chunk_seq;        // Chunk sequence number
    uint32_t chunk_size;       // Size of this chunk
    std::vector<uint8_t> data; // Chunk data
};
```

2. **BUT writeToastChunks() does NOT write xmin to disk** (toast.cpp:509-528):
```cpp
// Build tuple data manually
// Format: chunk_id (4 bytes) | chunk_seq (4 bytes) | chunk_size (4 bytes) | data
std::vector<uint8_t> tuple_data;
tuple_data.reserve(12 + chunk_size);

// Add chunk_id
uint32_t id = value_id;
tuple_data.insert(tuple_data.end(), reinterpret_cast<uint8_t *>(&id),
                  reinterpret_cast<uint8_t *>(&id) + 4);

// Add chunk_seq
tuple_data.insert(tuple_data.end(), reinterpret_cast<uint8_t *>(&seq),
                  reinterpret_cast<uint8_t *>(&seq) + 4);

// Add chunk_size
tuple_data.insert(tuple_data.end(), reinterpret_cast<uint8_t *>(&chunk_size),
                  reinterpret_cast<uint8_t *>(&chunk_size) + 4);

// Add chunk data
tuple_data.insert(tuple_data.end(), data + offset, data + offset + chunk_size);
```

**Analysis**: The TOAST chunk tuple contains only `chunk_id | chunk_seq | chunk_size | data` (12 bytes header + data). The `xmin` parameter passed to `writeToastChunks()` is IGNORED and NOT written to disk. This means:

1. **TupleHeader tracks xmin** via `insertTuple()` call (storage_engine.cpp:533-534)
2. **But TOAST data itself does NOT include xmin in tuple format**
3. **MGA versioning relies on TupleHeader, not custom TOAST fields**

**Impact**:
- TOAST chunks inherit xmin/xmax from TupleHeader (standard heap tuple format)
- This is **PostgreSQL-style**, not Firebird MGA style
- TOAST chunks follow **snapshot-based visibility** (via TupleHeader)
- Does NOT match specification: "TOAST chunks visible according to TIP (not snapshots)"

**File References**:
- `include/scratchbird/core/toast.h:59-67` - ToastTableEntry
- `src/core/toast.cpp:509-528` - Chunk format without xmin
- `src/core/toast.cpp:533-534` - insertTuple stores xmin in TupleHeader

---

### 3.2 TOAST Visibility Model

**STATUS**: **CRITICAL MGA VIOLATION** ❌
**Severity**: CRITICAL

**Problem**: TOAST chunks use **snapshot-based visibility** (via TupleHeader), NOT TIP-based visibility.

**Evidence**:

1. **TOAST chunks inserted as heap tuples** (toast.cpp:533-534):
```cpp
Status status = storage->insertTuple(toast_table_id_, tuple_data.data(),
                                     tuple_data.size(), &page_id, &item_id, ctx);
```

2. **insertTuple() sets TupleHeader with xmin** (heap_page.cpp:233-235):
```cpp
tuple_hdr->xmin = xmin;
tuple_hdr->xmax = 0;
```

3. **Visibility checked via findVisibleVersion()** (heap_page.cpp:1052-1496):
```cpp
// Check visibility
// Simple visibility: xmin <= snapshot_xid < xmax
if (tuple_hdr->xmin <= snapshot_xid) {
    if (effective_xmax == 0 || effective_xmax > snapshot_xid) {
        visible = true;
    }
}
```

**Analysis**: TOAST chunks are stored as regular heap tuples with TupleHeader. Visibility is determined by:
1. **TupleHeader.xmin** (transaction that created chunk)
2. **TupleHeader.xmax** (transaction that deleted chunk)
3. **Snapshot-based visibility** (xmin <= snapshot_xid < xmax)

This is **PostgreSQL MVCC**, not **Firebird MGA TIP-based visibility**.

**Firebird MGA Specification**: "TOAST chunks visible according to TIP (not snapshots)"
**Current Implementation**: TOAST chunks use snapshot-based visibility via TupleHeader

**Impact**:
- TOAST visibility is **transaction-dependent** (requires snapshot)
- Does NOT match Firebird MGA model where TOAST should be **TIP-independent**
- This affects multi-version concurrency and long-running transactions

**File References**:
- `src/core/toast.cpp:533-534` - TOAST as heap tuple
- `src/core/heap_page.cpp:233-235` - xmin tracking
- `src/core/heap_page.cpp:1321-1327` - Snapshot visibility

---

### 3.3 Garbage Collection of TOAST Chunks

**STATUS**: **HIGH PRIORITY MISSING** ⚠️
**Severity**: HIGH

**Problem**: Garbage collector does NOT explicitly handle TOAST chunk cleanup.

**Evidence**:

1. **GarbageCollector::cleanPage()** (garbage_collector.cpp:336-401) - No TOAST handling:
   - Function scans heap pages for dead tuples
   - Calls `HeapPage::prunePage()` to mark tuples unused
   - **NO code to clean orphaned TOAST chunks**

2. **Vacuum skips TOAST tables** (vacuum.cpp:145-149):
```cpp
// Skip TOAST tables and system tables
if (table.table_type == CatalogManager::TableType::TOAST)
{
    continue;
}
```

3. **HeapPage deletion cleans TOAST** (heap_page.cpp:412-434):
   - When a tuple is deleted, TOAST chunks are cleaned
   - **BUT**: If transaction aborts or system crashes, orphaned chunks remain

**Analysis**: TOAST cleanup relies on:
1. **Synchronous cleanup** during `deleteTuple()` (works for committed deletes)
2. **NO asynchronous cleanup** via garbage collector
3. **NO orphan detection** for aborted transactions or crashes

**Missing Functionality**:
- Orphan TOAST chunk detection (chunks with no parent tuple)
- Periodic TOAST table vacuum
- Reference counting for shared TOAST values
- Crash recovery for partial TOAST deletions

**Impact**:
- **Storage leaks**: Orphaned TOAST chunks accumulate
- **No recovery**: Crashed deletions leave orphans permanently
- **Table bloat**: TOAST tables grow indefinitely

**File References**:
- `src/core/garbage_collector.cpp:336-401` - cleanPage (no TOAST)
- `src/core/vacuum.cpp:145-149` - Skips TOAST tables
- `src/core/heap_page.cpp:412-434` - Synchronous cleanup only

---

## 4. INDEX TOAST HANDLING AUDIT

### 4.1 Index Types Analysis

**Total Index Types**: 7 (B-tree, Hash, GIN, HNSW, BRIN, Bitmap, R-tree)

**Index Files Found**:
- `include/scratchbird/core/btree.h`
- `include/scratchbird/core/hash_index.h`
- `include/scratchbird/core/gin_index.h`
- `include/scratchbird/core/hnsw_index.h`
- `include/scratchbird/core/brin_index.h`
- `include/scratchbird/core/bitmap_index.h`
- `include/scratchbird/core/rtree.h`

### 4.2 B-tree TOAST Handling

**STATUS**: **MISSING** ❌
**Severity**: HIGH

**Evidence**:

1. **B-tree insert** (btree.cpp:309-407) - No TOAST detoasting:
```cpp
auto BTree::insert(const std::vector<uint8_t> &key, const TID &tid, uint64_t xid,
                   ErrorContext *ctx) -> Status
{
    // ... lock acquisition ...
    // Insert key directly - NO detoasting check
}
```

2. **No `isToastPointer()` check** in B-tree code
3. **No `detoastValue()` call** before indexing

**Analysis**: When indexing a column that contains a TOAST pointer:
1. **B-tree indexes the TOAST pointer bytes** (18 bytes)
2. **NOT the actual value** being indexed
3. **Comparisons use pointer bytes**, not actual data

**Impact**:
- **Incorrect ordering**: B-tree sorted by pointer values, not actual values
- **Broken indexes**: Queries return wrong results
- **Index corruption**: Index does not reflect actual column values

**Example Scenario**:
```sql
CREATE INDEX idx ON table(large_column);
INSERT INTO table VALUES ('AAAAA...'); -- 5KB, TOASTed as value_id=1
INSERT INTO table VALUES ('BBBBB...'); -- 5KB, TOASTed as value_id=2
SELECT * FROM table WHERE large_column = 'AAAAA...' ORDER BY large_column;
-- Index searches for pointer bytes, not actual value!
```

**File References**:
- `src/core/btree.cpp:309-407` - B-tree insert (no TOAST check)

**Required Fix**:
1. Detect TOAST pointers before indexing: `if (isToastPointer(key_data))`
2. Detoast value: `toast_mgr->detoastValue(pointer, &actual_value, xmin)`
3. Index actual value instead of pointer

---

### 4.3 Other Index Types

**STATUS**: **ASSUMED MISSING** ⚠️
**Severity**: HIGH

**Analysis**: Since B-tree (the most common index) does NOT handle TOAST, it's highly likely that:
- **Hash Index**: Hashes TOAST pointer bytes (wrong hash value)
- **GIN Index**: Indexes TOAST pointer as token (broken full-text search)
- **HNSW Index**: Embeds TOAST pointer bytes (broken vector search)
- **BRIN Index**: Summarizes TOAST pointer ranges (meaningless)
- **Bitmap Index**: Indexes TOAST pointer bits (broken)
- **R-tree Index**: Indexes TOAST pointer bytes (broken spatial search)

**File References**:
- All index implementation files lack `isToastPointer()` or `detoastValue()` calls

**Impact**: ALL indexes are broken for TOASTed columns.

---

## 5. TOAST FEATURE MATRIX

| Feature | Status | Evidence | Severity |
|---------|--------|----------|----------|
| **Core TOAST Operations** | ✅ COMPLETE | toast.cpp:212-330 | - |
| **Compression (LZ4)** | ✅ COMPLETE | toast.cpp:739-820 | - |
| **Chunk Storage** | ✅ COMPLETE | toast.cpp:474-657 | - |
| **TOAST Table Creation** | ✅ COMPLETE | toast.cpp:136-210 | - |
| **Value ID Recovery** | ✅ COMPLETE | toast.cpp:62-94 | - |
| **Heap Page Integration** | ✅ COMPLETE | heap_page.cpp:110-448 | - |
| **Storage Engine Integration** | ✅ COMPLETE | storage_engine.cpp:86-128 | - |
| **xmin/xmax Tracking (On-Disk)** | ❌ MISSING | toast.cpp:509-528 | CRITICAL |
| **TIP-Based Visibility** | ❌ WRONG | heap_page.cpp:1321-1327 | CRITICAL |
| **Garbage Collection** | ❌ MISSING | vacuum.cpp:145-149 | HIGH |
| **B-tree Index Support** | ❌ MISSING | btree.cpp:309-407 | HIGH |
| **Hash Index Support** | ❌ ASSUMED MISSING | - | HIGH |
| **GIN Index Support** | ❌ ASSUMED MISSING | - | HIGH |
| **HNSW Index Support** | ❌ ASSUMED MISSING | - | HIGH |
| **BRIN Index Support** | ❌ ASSUMED MISSING | - | HIGH |
| **Bitmap Index Support** | ❌ ASSUMED MISSING | - | HIGH |
| **R-tree Index Support** | ❌ ASSUMED MISSING | - | HIGH |
| **Reference Counting** | ❌ MISSING | - | MEDIUM |
| **Partial Detoasting** | ❌ MISSING | - | LOW |
| **TOAST Prefetching** | ❌ MISSING | - | LOW |

---

## 6. CRITICAL BUGS IDENTIFIED

### 6.1 BUG-TOAST-001: TOAST Chunks Lack On-Disk xmin/xmax

**Severity**: CRITICAL
**Component**: ToastManager::writeToastChunks()
**File**: `src/core/toast.cpp:509-528`

**Description**: TOAST chunks are stored with format `chunk_id | chunk_seq | chunk_size | data` (12-byte header). The `xmin` parameter passed to `writeToastChunks()` is ignored. xmin/xmax tracking relies solely on TupleHeader, not TOAST-specific fields.

**Evidence**:
```cpp
// Format: chunk_id (4 bytes) | chunk_seq (4 bytes) | chunk_size (4 bytes) | data
std::vector<uint8_t> tuple_data;
tuple_data.reserve(12 + chunk_size);
// xmin parameter never written to tuple_data!
```

**Impact**:
- TOAST chunks follow PostgreSQL MVCC model (TupleHeader-based)
- Does NOT match Firebird MGA specification
- Violates design requirement for TIP-based visibility

**Fix Required**: Redesign TOAST chunk format to include explicit xmin/xmax in tuple data.

---

### 6.2 BUG-TOAST-002: TOAST Uses Snapshot Visibility Instead of TIP

**Severity**: CRITICAL
**Component**: HeapPage::findVisibleVersion()
**File**: `src/core/heap_page.cpp:1321-1327`

**Description**: TOAST chunks use snapshot-based visibility (xmin <= snapshot_xid < xmax) instead of TIP-based visibility.

**Evidence**:
```cpp
// Check visibility
// Simple visibility: xmin <= snapshot_xid < xmax
if (tuple_hdr->xmin <= snapshot_xid) {
    if (effective_xmax == 0 || effective_xmax > snapshot_xid) {
        visible = true;
    }
}
```

**Impact**:
- TOAST visibility depends on transaction snapshot
- Long-running transactions hold TOAST chunks visible
- Does not match Firebird TIP model

**Fix Required**: Implement TIP-based visibility for TOAST chunks.

---

### 6.3 BUG-TOAST-003: No Garbage Collection for Orphaned TOAST Chunks

**Severity**: HIGH
**Component**: GarbageCollector, Vacuum
**Files**:
- `src/core/garbage_collector.cpp:336-401`
- `src/core/vacuum.cpp:145-149`

**Description**: Garbage collector does not clean orphaned TOAST chunks. Vacuum explicitly skips TOAST tables.

**Evidence**:
```cpp
// Skip TOAST tables and system tables
if (table.table_type == CatalogManager::TableType::TOAST) {
    continue;
}
```

**Impact**:
- Orphaned TOAST chunks from aborted transactions accumulate
- TOAST tables grow indefinitely
- No recovery mechanism for crashed deletions
- Storage leaks

**Fix Required**: Implement orphan TOAST chunk detection and cleanup in GC.

---

### 6.4 BUG-TOAST-004: All Indexes Index TOAST Pointers Instead of Values

**Severity**: HIGH
**Component**: All index types (B-tree, Hash, GIN, HNSW, BRIN, Bitmap, R-tree)
**Files**: All index implementation files

**Description**: Indexes store TOAST pointer bytes (18 bytes) instead of detoasting and indexing actual values.

**Evidence**: No `isToastPointer()` or `detoastValue()` calls in any index insert paths.

**Impact**:
- Indexes are broken for TOASTed columns
- Query results incorrect
- Full-text search broken
- Vector search broken
- All index-based queries return wrong results

**Fix Required**: Add TOAST detoasting before indexing in all index types.

---

## 7. MGA COMPLIANCE SCORECARD

| MGA Requirement | Status | Evidence | Notes |
|----------------|--------|----------|-------|
| **TOAST chunks track xmin** | ❌ FAIL | toast.cpp:509-528 | xmin in TupleHeader only, not TOAST data |
| **TOAST chunks track xmax** | ❌ FAIL | toast.cpp:509-528 | xmax in TupleHeader only, not TOAST data |
| **TIP-based visibility** | ❌ FAIL | heap_page.cpp:1321-1327 | Uses snapshot-based visibility |
| **Independent of snapshot** | ❌ FAIL | heap_page.cpp:1321-1327 | Depends on snapshot_xid |
| **Garbage collection** | ❌ FAIL | vacuum.cpp:145-149 | Skips TOAST tables |
| **Version chain support** | ✅ PASS | heap_page.cpp:617-642 | Cleanup on update works |

**Overall MGA Compliance**: **1/6 (17%)** ❌

---

## 8. RECOMMENDATIONS

### 8.1 CRITICAL Priority (Must Fix for Production)

1. **Redesign TOAST chunk format**:
   - Include explicit xmin/xmax in tuple data (not just TupleHeader)
   - Format: `xmin (8) | xmax (8) | chunk_id (4) | chunk_seq (4) | chunk_size (4) | data`
   - Update `writeToastChunks()` and `readToastChunks()` accordingly

2. **Implement TIP-based TOAST visibility**:
   - Create `ToastVisibilityChecker` that uses TIP instead of snapshots
   - Modify `detoastValue()` to check TIP state
   - Ensure TOAST chunks visible based on committed state, not transaction snapshot

3. **Add TOAST detoasting to all indexes**:
   - Detect TOAST pointers before indexing: `if (isToastPointer(key_data))`
   - Detoast value: `toast_mgr->detoastValue(pointer, &actual_value, xmin)`
   - Index actual value instead of pointer
   - Apply to B-tree, Hash, GIN, HNSW, BRIN, Bitmap, R-tree

### 8.2 HIGH Priority (Important for Correctness)

4. **Implement TOAST garbage collection**:
   - Add `GarbageCollector::cleanToastChunks()` method
   - Detect orphaned TOAST chunks (no parent tuple)
   - Periodic TOAST table vacuum
   - Track reference counts for shared TOAST values

5. **Add TOAST crash recovery**:
   - WAL logging for TOAST operations
   - Recovery of partial TOAST deletions
   - Orphan detection on database startup

### 8.3 MEDIUM Priority (Performance & Features)

6. **Implement reference counting**:
   - Track references to TOAST values
   - Allow sharing TOAST chunks between tuples
   - Reference-counted deletion

7. **Add TOAST statistics**:
   - Track TOAST table sizes
   - Monitor orphan chunks
   - Alert on excessive TOAST growth

### 8.4 LOW Priority (Nice to Have)

8. **Partial detoasting**:
   - Allow retrieving portions of TOASTed values
   - Useful for large text/binary columns

9. **TOAST prefetching**:
   - Predictive loading of TOAST chunks
   - Batch detoasting for sequential scans

---

## 9. TESTING RECOMMENDATIONS

### 9.1 Required Test Coverage

1. **MGA Compliance Tests**:
   - Verify xmin/xmax tracking in TOAST chunks
   - Test TIP-based visibility (transaction A inserts, B reads, A aborts)
   - Verify TOAST cleanup on transaction abort

2. **Index Correctness Tests**:
   - Create index on TOASTed column
   - Verify index contains actual values, not pointers
   - Test all index types with TOAST

3. **Garbage Collection Tests**:
   - Create orphaned TOAST chunks (abort transaction)
   - Verify GC cleans up orphans
   - Test crash recovery for TOAST

4. **Stress Tests**:
   - Long-running transactions with TOAST
   - Concurrent TOAST updates
   - Large-scale TOAST chunk creation/deletion

---

## 10. CONCLUSION

The TOAST implementation in ScratchBird is **functionally complete for basic operations** but **fails MGA compliance requirements**. Critical issues include:

1. **No on-disk xmin/xmax tracking** in TOAST chunks
2. **Snapshot-based visibility** instead of TIP-based
3. **No garbage collection** for orphaned chunks
4. **All indexes broken** for TOASTed columns

**Recommendation**: **Do NOT use TOAST for production** until MGA compliance and index support are fixed.

**Estimated Effort to Fix**:
- CRITICAL fixes: 2-3 weeks (xmin/xmax tracking, TIP visibility, index support)
- HIGH fixes: 1-2 weeks (garbage collection, crash recovery)
- Total: **4-5 weeks** for production-ready TOAST

---

## APPENDIX A: FILE REFERENCES

**TOAST Core Files**:
- `include/scratchbird/core/toast.h` (163 lines)
- `src/core/toast.cpp` (823 lines)

**Heap Integration Files**:
- `include/scratchbird/core/heap_page.h` (350 lines)
- `src/core/heap_page.cpp` (1790 lines)

**Storage Engine Files**:
- `include/scratchbird/core/storage_engine.h` (193 lines)
- `src/core/storage_engine.cpp` (300+ lines)

**Garbage Collection Files**:
- `include/scratchbird/core/garbage_collector.h` (226 lines)
- `src/core/garbage_collector.cpp` (200+ lines)

**Vacuum Files**:
- `src/core/vacuum.cpp` (300+ lines)

**Index Files**:
- `src/core/btree.cpp` (2500+ lines)
- `include/scratchbird/core/hash_index.h`
- `include/scratchbird/core/gin_index.h`
- `include/scratchbird/core/hnsw_index.h`
- `include/scratchbird/core/brin_index.h`
- `include/scratchbird/core/bitmap_index.h`
- `include/scratchbird/core/rtree.h`

**Specification Files**:
- `/docs/specifications/parser/v3/TOAST_LOB_STORAGE.md`
- `/docs/specifications/parser/v3/HEAP_TOAST_INTEGRATION.md`

---

**Audit Date**: November 1, 2025
**Status**: Phase 2 Complete
**Next Phase**: SQL Identifier 128 UTF-8 Character Audit

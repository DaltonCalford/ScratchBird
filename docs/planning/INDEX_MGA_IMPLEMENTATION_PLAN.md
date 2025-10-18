# ScratchBird Index MGA Compliance Implementation Plan

**Created**: October 18, 2025
**Status**: 🔴 CRITICAL PRIORITY - Production Blocker
**Related**: `/docs/audit/INDEX_MGA_COMPLIANCE_ANALYSIS.md`
**Target**: Alpha 1.4 (Phase 1), Alpha 1.5 (Phase 2), Beta (Phase 3), Post-Beta (Phase 4)

---

## OVERVIEW

This document provides a detailed, actionable implementation plan to achieve full MGA compliance for all ScratchBird index types and implement missing index types specified in the grammar.

**Critical Finding**: Current indexes bypass MVCC, causing isolation violations and potential data corruption.

**Total Work Estimate**:
- **Phase 1 (Critical)**: 38-54 hours (1.5-2 weeks) - MVCC visibility checks
- **Phase 2 (High)**: 48-64 hours (2-2.5 weeks) - Dead entry pruning / GC integration
- **Phase 3 (Medium)**: 50-72 hours (2-3 weeks) - Full MGA optimization
- **Phase 4 (Future)**: 300-470 hours (7.5-12 weeks) - New index types

---

## PHASE 1: CRITICAL MGA COMPLIANCE (ALPHA 1.4)

**Goal**: Make existing indexes MVCC-safe
**Priority**: 🔴 CRITICAL - MUST COMPLETE BEFORE ANY NEW FEATURES
**Timeline**: 1.5-2 weeks
**Estimated Effort**: 38-54 hours

### TASK 1.1: Add Snapshot Parameter to All Index APIs

**Priority**: 🔴 CRITICAL
**Estimated Time**: 8-12 hours
**Dependencies**: None
**Breaking Change**: YES - API change

#### Subtasks

- [ ] **1.1.1**: Update B-Tree API signatures (2 hours)
  - File: `include/scratchbird/core/btree.h`
  - Change `search()` signature:
    ```cpp
    // OLD
    Status search(const std::vector<uint8_t> &key,
                 std::vector<uint64_t> *tuple_ids_out,
                 ErrorContext *ctx = nullptr);

    // NEW
    Status search(const std::vector<uint8_t> &key,
                 Snapshot *snapshot,
                 std::vector<uint64_t> *tuple_ids_out,
                 ErrorContext *ctx = nullptr);
    ```
  - Update `rangeScan()` to accept snapshot
  - Update BTreeIterator to store snapshot

- [ ] **1.1.2**: Update Hash Index API signatures (2 hours)
  - File: `include/scratchbird/core/hash_index.h`
  - Change `find()` signature to include `Snapshot *snapshot`

- [ ] **1.1.3**: Update GIN Index API signatures (2 hours)
  - File: `include/scratchbird/core/gin_index.h`
  - Change `scan()`, `scanPartial()` signatures

- [ ] **1.1.4**: Update Bitmap Index API signatures (1 hour)
  - File: `include/scratchbird/core/bitmap_index.h`
  - Change `find()`, `findAnd()`, `findOr()` signatures

- [ ] **1.1.5**: Update all call sites in storage_engine.cpp (2-3 hours)
  - File: `src/core/storage_engine.cpp`
  - Pass current snapshot to all index operations
  - Get snapshot from ConnectionContext

- [ ] **1.1.6**: Update all call sites in executor layer (1-2 hours)
  - Find all index scan operations
  - Pass executor's snapshot to index operations

**Acceptance Criteria**:
- All index APIs accept Snapshot parameter
- Code compiles without errors
- All tests pass (may need updates)

---

### TASK 1.2: Implement Visibility Checks in B-Tree

**Priority**: 🔴 CRITICAL
**Estimated Time**: 6-8 hours
**Dependencies**: TASK 1.1 (API changes)

#### Subtasks

- [ ] **1.2.1**: Add TIP visibility check helper (2 hours)
  - File: `src/core/btree.cpp`
  - Function: `bool isTupleVisible(uint64_t tuple_id, Snapshot* snapshot)`
  - Call `TransactionManager::getTransactionState()` to check TIP
  - Or call heap's `findVisibleVersion()` and check for NULL

- [ ] **1.2.2**: Update search() implementation (2 hours)
  - File: `src/core/btree.cpp`
  - For each TID found, call visibility check
  - Only add visible TIDs to results
  - Handle NULL snapshot (scan all - for VACUUM)

- [ ] **1.2.3**: Update rangeScan() / BTreeIterator (2 hours)
  - File: `src/core/btree_iterator.cpp`
  - Store snapshot in iterator
  - Filter TIDs during iteration
  - Skip non-visible tuples

- [ ] **1.2.4**: Add unit tests for visibility (2-3 hours)
  - Test file: `tests/unit/test_btree_mvcc.cpp`
  - Test: don't see uncommitted inserts
  - Test: see committed updates
  - Test: consistent snapshot in REPEATABLE READ

**Acceptance Criteria**:
- B-Tree search returns only visible tuples
- All isolation levels work correctly
- Tests validate MVCC behavior

---

### TASK 1.3: Add xmin Tracking and Visibility to Hash Index

**Priority**: 🔴 CRITICAL
**Estimated Time**: 10-14 hours
**Dependencies**: TASK 1.1 (API changes)
**Breaking Change**: YES - page format change

#### Subtasks

- [ ] **1.3.1**: Extend HashEntry structure (2 hours)
  - File: `include/scratchbird/core/hash_index.h`
  - Add `uint64_t he_xmin` field
  - Add `uint64_t he_xmax` field
  - Update `sizeof(HashEntry)` to 32 bytes
  - Bump page format version to v2

- [ ] **1.3.2**: Update insert() to set xmin (1 hour)
  - File: `src/core/hash_index.cpp`
  - Get current XID from ConnectionContext
  - Set `entry.he_xmin = current_xid`
  - Set `entry.he_xmax = 0` (active)

- [ ] **1.3.3**: Update delete() to set xmax (1 hour)
  - Instead of `entry.he_tuple_id = 0`
  - Set `entry.he_xmax = current_xid`
  - Keep entry in bucket (soft delete)

- [ ] **1.3.4**: Implement visibility check in find() (3-4 hours)
  - For each entry found, check:
    - `entry.he_xmin <= snapshot->xmax` (created before snapshot)
    - `entry.he_xmax == 0 || entry.he_xmax > snapshot->xmin` (not deleted)
    - Check TIP state for he_xmin (committed?)
  - Filter out non-visible entries

- [ ] **1.3.5**: Add migration path for old format (2-3 hours)
  - Detect v1 vs v2 page format
  - For v1: treat as xmin=0, xmax=0 (always visible)
  - Mark v1 indexes as "needs rebuild"
  - Provide REINDEX command

- [ ] **1.3.6**: Add unit tests (2-3 hours)
  - Test file: `tests/unit/test_hash_index_mvcc.cpp`
  - Test uncommitted inserts not visible
  - Test deleted entries not visible
  - Test rollback handling

**Acceptance Criteria**:
- Hash index tracks transaction IDs
- Visibility checks work correctly
- Migration path exists for old indexes

---

### TASK 1.4: Implement Visibility Checks in GIN Index

**Priority**: 🔴 CRITICAL
**Estimated Time**: 8-12 hours
**Dependencies**: TASK 1.1 (API changes)

#### Subtasks

- [ ] **1.4.1**: Add TIP visibility check helper (1 hour)
  - File: `src/core/gin_index.cpp`
  - Similar to B-Tree helper

- [ ] **1.4.2**: Filter pending list by transaction state (3-4 hours)
  - In `scanPendingList()`, check `entry.xmin` against snapshot
  - Only return entries from committed transactions
  - Skip entries with xmin > snapshot->xmax

- [ ] **1.4.3**: Add visibility filtering in scan() (2-3 hours)
  - For each TID from posting trees, check visibility
  - Call heap's visibility check
  - Return only visible TIDs

- [ ] **1.4.4**: Make pending list merge transaction-aware (3-4 hours)
  - In `mergePendingList()`, check TIP before merging
  - Only merge entries from committed transactions
  - Keep uncommitted entries in pending list

- [ ] **1.4.5**: Add unit tests (2-3 hours)
  - Test file: `tests/unit/test_gin_index_mvcc.cpp`
  - Test pending list visibility
  - Test merge during concurrent transactions
  - Test rollback of full-text insert

**Acceptance Criteria**:
- GIN returns only visible results
- Pending list respects transaction boundaries
- Merge preserves MVCC semantics

---

### TASK 1.5: Add Post-Filter Visibility to Bitmap Index

**Priority**: 🔴 CRITICAL
**Estimated Time**: 6-8 hours
**Dependencies**: TASK 1.1 (API changes)

#### Subtasks

- [ ] **1.5.1**: Implement post-filter in find() (2-3 hours)
  - File: `src/core/bitmap_index.cpp`
  - Get all TIDs from bitmap
  - For each TID, check visibility
  - Return only visible TIDs

- [ ] **1.5.2**: Implement post-filter in findAnd() / findOr() (2-3 hours)
  - Apply visibility filter AFTER bitmap operations
  - Filter final result set

- [ ] **1.5.3**: Document performance implications (1 hour)
  - Add comment: "Post-filtering degrades performance by 20-40%"
  - Document in INDEX_MGA_COMPLIANCE_ANALYSIS.md
  - Note: Full MVCC redesign deferred to Beta

- [ ] **1.5.4**: Benchmark overhead (1-2 hours)
  - Measure find() latency with/without filtering
  - Test with large result sets (10K, 100K, 1M TIDs)
  - Document results

**Acceptance Criteria**:
- Bitmap index returns only visible tuples
- Performance impact documented
- Tests validate correctness

---

### TASK 1.6: Integration Testing

**Priority**: 🔴 CRITICAL
**Estimated Time**: 6-8 hours
**Dependencies**: TASKS 1.2-1.5

#### Subtasks

- [ ] **1.6.1**: Create MVCC test suite for all indexes (3-4 hours)
  - Test file: `tests/integration/test_index_mvcc.cpp`
  - For each index type (B-Tree, Hash, GIN, Bitmap):
    - Test READ COMMITTED isolation
    - Test REPEATABLE READ isolation
    - Test SERIALIZABLE isolation
    - Test concurrent insert/scan
    - Test rollback scenarios

- [ ] **1.6.2**: Edge case tests (2-3 hours)
  - Test scan during concurrent UPDATE
  - Test scan sees rolled-back INSERT
  - Test scan doesn't see uncommitted DELETE
  - Test scan with savepoint rollback

- [ ] **1.6.3**: Performance regression tests (1-2 hours)
  - Measure overhead of visibility checks
  - Ensure overhead < 30% for typical workloads
  - Document results

**Acceptance Criteria**:
- All isolation levels work correctly
- Edge cases handled properly
- Performance acceptable

---

## PHASE 2: DEAD ENTRY PRUNING (ALPHA 1.5)

**Goal**: Integrate indexes with heap garbage collection
**Priority**: 🟠 HIGH
**Timeline**: 2-2.5 weeks
**Estimated Effort**: 48-64 hours

### TASK 2.1: Design Index-Heap GC Protocol

**Priority**: 🟠 HIGH
**Estimated Time**: 4-6 hours
**Dependencies**: Phase 1 complete

#### Subtasks

- [ ] **2.1.1**: Define GC interface (2 hours)
  - File: `include/scratchbird/core/index_gc.h` (new)
  - Interface: `markDeadEntries(const std::vector<uint64_t>& dead_tuple_ids)`
  - Document lifecycle: heap VACUUM identifies dead TIDs → calls index GC

- [ ] **2.1.2**: Coordinate with sweep process (2 hours)
  - How to get OIT/OAT from TransactionManager
  - When to trigger index GC (after heap sweep? during?)
  - Bulk vs incremental GC

- [ ] **2.1.3**: Write specification document (1-2 hours)
  - File: `/docs/specifications/INDEX_GC_PROTOCOL.md`
  - Document GC lifecycle
  - Document performance considerations
  - Document testing strategy

**Acceptance Criteria**:
- GC protocol defined
- Interface documented
- Specification complete

---

### TASK 2.2: Implement B-Tree Dead Entry Removal

**Priority**: 🟠 HIGH
**Estimated Time**: 12-16 hours
**Dependencies**: TASK 2.1

#### Subtasks

- [ ] **2.2.1**: Implement tree walk to identify dead entries (4-5 hours)
  - File: `src/core/btree_vacuum.cpp`
  - Walk tree depth-first
  - For each node, check if TID is in dead list
  - Mark nodes with `btn_xmax = OIT`

- [ ] **2.2.2**: Implement dead node removal (4-5 hours)
  - Remove nodes where `btn_xmax < OIT`
  - Update parent pointers
  - Handle underflow (may need merging)

- [ ] **2.2.3**: Implement tree rebalancing (3-4 hours)
  - After removal, check for underfull pages
  - Merge with siblings if needed
  - Update statistics (page count, tuple count)

- [ ] **2.2.4**: Add tests (2-3 hours)
  - Test file: `tests/unit/test_btree_gc.cpp`
  - Test removal of single dead entry
  - Test removal of large dead set
  - Test tree rebalancing after removal
  - Verify statistics updated

**Acceptance Criteria**:
- B-Tree shrinks after VACUUM
- Dead entries removed correctly
- Tree remains balanced

---

### TASK 2.3: Implement Hash Index Dead Entry Removal

**Priority**: 🟠 HIGH
**Estimated Time**: 10-14 hours
**Dependencies**: TASK 2.1

#### Subtasks

- [ ] **2.3.1**: Scan buckets for dead entries (3-4 hours)
  - File: `src/core/hash_index.cpp`
  - For each bucket, check entries against dead list
  - Mark with `he_xmax < OIT` or delete immediately

- [ ] **2.3.2**: Compact buckets after removal (3-4 hours)
  - Remove dead entries from buckets
  - Reclaim space
  - Update overflow chains

- [ ] **2.3.3**: Update directory statistics (2-3 hours)
  - Update entry counts per bucket
  - Recalculate load factor
  - Consider bucket merging if load drops

- [ ] **2.3.4**: Add tests (2-3 hours)
  - Test file: `tests/unit/test_hash_index_gc.cpp`
  - Test bucket compaction
  - Test overflow chain handling
  - Verify statistics

**Acceptance Criteria**:
- Hash buckets shrink after VACUUM
- Space reclaimed efficiently
- Statistics accurate

---

### TASK 2.4: Implement GIN Dead Entry Removal

**Priority**: 🟠 HIGH
**Estimated Time**: 16-20 hours
**Dependencies**: TASK 2.1

#### Subtasks

- [ ] **2.4.1**: Remove TIDs from posting trees (6-8 hours)
  - File: `src/core/gin_index.cpp`
  - For each dead TID, find in posting tree
  - Remove from posting list (compressed)
  - Recompress posting list after removal

- [ ] **2.4.2**: Update entry tree counts (3-4 hours)
  - Decrement counts for affected entries
  - Remove entries with 0 postings
  - Update entry tree structure

- [ ] **2.4.3**: Merge/compact posting trees (4-5 hours)
  - After removal, check for underfull posting pages
  - Merge small posting lists
  - Optimize posting tree structure

- [ ] **2.4.4**: Add tests (3-4 hours)
  - Test file: `tests/unit/test_gin_index_gc.cpp`
  - Test posting list removal
  - Test entry removal when no postings left
  - Test large posting list compaction
  - Benchmark compaction overhead

**Acceptance Criteria**:
- Posting trees shrink correctly
- Entry counts accurate
- Performance acceptable

---

### TASK 2.5: Implement Bitmap Dead Entry Removal

**Priority**: 🟠 HIGH
**Estimated Time**: 6-8 hours
**Dependencies**: TASK 2.1

#### Subtasks

- [ ] **2.5.1**: Clear bits for dead TIDs (2-3 hours)
  - File: `src/core/bitmap_index.cpp`
  - For each dead TID, clear bit in bitmap
  - Update Roaring containers

- [ ] **2.5.2**: Recompress containers (2-3 hours)
  - After clearing bits, recompress
  - Convert Bitset → Array if sparse
  - Convert Run → Array if fragmented

- [ ] **2.5.3**: Update cardinality (1 hour)
  - Recalculate set bit count
  - Update dictionary entry

- [ ] **2.5.4**: Add tests (1-2 hours)
  - Test file: `tests/unit/test_bitmap_index_gc.cpp`
  - Test bit clearing
  - Test compression efficiency
  - Verify cardinality

**Acceptance Criteria**:
- Bitmaps shrink after VACUUM
- Compression efficient
- Cardinality accurate

---

### TASK 2.6: Integration with Heap VACUUM

**Priority**: 🟠 HIGH
**Estimated Time**: 6-8 hours
**Dependencies**: TASKS 2.2-2.5

#### Subtasks

- [ ] **2.6.1**: Update heap VACUUM to call index GC (2-3 hours)
  - File: `src/core/storage_engine.cpp` (or heap_page.cpp)
  - After identifying dead tuples, call index GC for each index
  - Pass dead TID list to each index

- [ ] **2.6.2**: Add GC coordination tests (3-4 hours)
  - Test file: `tests/integration/test_vacuum_integration.cpp`
  - Test VACUUM removes dead entries from all indexes
  - Test concurrent VACUUM and queries
  - Test large dead set handling

- [ ] **2.6.3**: Benchmark GC performance (1-2 hours)
  - Time heap VACUUM with/without index GC
  - Measure space reclaimed per index type
  - Document overhead

**Acceptance Criteria**:
- VACUUM cleans all indexes
- No dangling TIDs
- Performance acceptable

---

## PHASE 3: FULL MGA INTEGRATION (BETA)

**Goal**: Complete Firebird-style MGA for indexes
**Priority**: 🟡 MEDIUM
**Timeline**: 2-3 weeks
**Estimated Effort**: 50-72 hours

### TASK 3.1: Add xmax Support Everywhere

**Priority**: 🟡 MEDIUM
**Estimated Time**: 12-16 hours
**Dependencies**: Phase 2 complete

#### Subtasks

- [ ] **3.1.1**: Add xmax to all index entry structures (4-6 hours)
  - B-Tree: already has btn_xmax ✅
  - Hash: already added in Phase 1 ✅
  - GIN: add to entry tree and posting tree nodes
  - Bitmap: add to dictionary entries (optional)

- [ ] **3.1.2**: Implement soft deletion (4-5 hours)
  - Set xmax on delete, don't remove immediately
  - Keep entries until transaction commits
  - Cleanup in VACUUM

- [ ] **3.1.3**: Handle rollback scenarios (3-4 hours)
  - If transaction rolls back, clear xmax
  - Entries become visible again

- [ ] **3.1.4**: Add tests (2-3 hours)
  - Test concurrent delete visibility
  - Test rollback makes entries visible
  - Test commit makes entries invisible

**Acceptance Criteria**:
- All indexes support soft deletion
- Rollback works correctly
- Commit properly hides entries

---

### TASK 3.2: Implement Index-Level MVCC Snapshots

**Priority**: 🟡 MEDIUM
**Estimated Time**: 20-30 hours
**Dependencies**: TASK 3.1

#### Subtasks

- [ ] **3.2.1**: Snapshot isolation for index scans (8-10 hours)
  - Ensure index scan sees consistent snapshot
  - Coordinate with heap snapshot
  - Handle concurrent modifications

- [ ] **3.2.2**: Prevent phantom reads in SERIALIZABLE (8-10 hours)
  - Implement predicate locking (key-range locks)
  - Detect conflicts with concurrent inserts
  - Abort conflicting transactions

- [ ] **3.2.3**: Add SERIALIZABLE tests (4-6 hours)
  - Test phantom prevention
  - Test predicate lock conflicts
  - Test write-write conflicts

**Acceptance Criteria**:
- SERIALIZABLE isolation fully enforced
- No phantom reads possible
- Conflicts detected correctly

---

### TASK 3.3: Optimize Visibility Checks

**Priority**: 🟡 MEDIUM
**Estimated Time**: 10-14 hours
**Dependencies**: Phase 1 complete

#### Subtasks

- [ ] **3.3.1**: Cache TIP results (4-5 hours)
  - Add TIP result cache in TransactionManager
  - Cache transaction states (committed/aborted/in-progress)
  - Invalidate on commit/abort

- [ ] **3.3.2**: Implement hint bits (3-4 hours)
  - Add hint bit to index entries (like heap tuples)
  - Set hint after first visibility check
  - Skip TIP lookup if hint set

- [ ] **3.3.3**: Batch visibility checks (3-4 hours)
  - Check multiple TIDs in single TIP lookup
  - Optimize for sequential TIDs
  - Reduce TIP page traffic

**Acceptance Criteria**:
- Visibility checks faster
- TIP cache hit rate > 80%
- Hint bits reduce TIP lookups

---

### TASK 3.4: Benchmark and Tune

**Priority**: 🟡 MEDIUM
**Estimated Time**: 8-12 hours
**Dependencies**: TASKS 3.1-3.3

#### Subtasks

- [ ] **3.4.1**: Measure overhead of visibility checks (2-3 hours)
  - Benchmark each index type
  - Measure latency per TID
  - Compare to heap-only scans

- [ ] **3.4.2**: Optimize hot paths (4-6 hours)
  - Profile index scans
  - Identify bottlenecks
  - Optimize critical functions

- [ ] **3.4.3**: Document performance characteristics (2-3 hours)
  - Document overhead by index type
  - Document optimization techniques
  - Provide tuning guidelines

**Acceptance Criteria**:
- Performance overhead < 15%
- Hot paths optimized
- Documentation complete

---

## PHASE 4: NEW INDEX TYPES (POST-BETA)

**Goal**: Implement all specified index types with MGA from the start
**Priority**: 🟢 FUTURE
**Timeline**: 7.5-12 weeks total
**Estimated Effort**: 300-470 hours

### TASK 4.1: Implement BRIN Index

**Priority**: 🟢 HIGH VALUE
**Estimated Time**: 20-30 hours
**Dependencies**: Phase 3 complete

#### Design Requirements

**MGA Compliance from Start**:
- Add xmin/xmax to block range structures
- Implement visibility checks for range summaries
- Integrate with heap GC

#### Subtasks

- [ ] **4.1.1**: Design block range data structure (4-6 hours)
  ```cpp
  struct BrinRange {
      uint32_t start_block;
      uint32_t end_block;
      uint64_t min_value;  // For numeric/date columns
      uint64_t max_value;
      uint64_t xmin;  // ← MGA compliance
      uint64_t xmax;  // ← MGA compliance
      // ... other summary data
  };
  ```

- [ ] **4.1.2**: Implement min/max summaries (4-6 hours)
  - Calculate min/max per block range
  - Update on INSERT/UPDATE/DELETE
  - Efficient range queries

- [ ] **4.1.3**: Implement range scan with pruning (6-8 hours)
  - Skip ranges where value < min or value > max
  - Filter by transaction visibility
  - Return TIDs from matching blocks

- [ ] **4.1.4**: Add MGA compliance (2-3 hours)
  - Track xmin/xmax per range
  - Visibility checks during scan
  - Dead range removal in VACUUM

- [ ] **4.1.5**: Test with time-series workload (3-4 hours)
  - Insert millions of time-ordered rows
  - Benchmark range queries
  - Compare to B-tree

- [ ] **4.1.6**: Benchmark vs B-tree (1-2 hours)
  - Measure space savings
  - Measure query performance
  - Document trade-offs

**Acceptance Criteria**:
- BRIN fully MGA-compliant
- Space savings > 90% vs B-tree
- Query performance acceptable for time-series

**Total Estimated Time**: 20-30 hours

---

### TASK 4.2: Implement VECTOR Index (HNSW)

**Priority**: 🟢 HIGH DEMAND
**Estimated Time**: 40-60 hours
**Dependencies**: VECTOR data type implemented

#### Design Requirements

**MGA Compliance**:
- Node versioning (graph structure changes)
- Transaction-aware link updates
- Visibility checks during graph traversal
- Dead node removal during VACUUM

#### Subtasks

- [ ] **4.2.1**: Implement HNSW graph structure (12-16 hours)
  - Multi-layer graph
  - Node connections (bi-directional links)
  - Distance metrics (L2, cosine, dot product)

- [ ] **4.2.2**: Implement graph insertion (8-10 hours)
  - Select layer for new node
  - Find neighbors using greedy search
  - Create bi-directional links

- [ ] **4.2.3**: Implement graph deletion (6-8 hours)
  - Mark nodes as deleted (xmax)
  - Reroute links around deleted nodes
  - Cleanup in VACUUM

- [ ] **4.2.4**: Implement KNN search (8-10 hours)
  - Greedy search from top layer
  - Beam search for accuracy
  - Return k nearest neighbors

- [ ] **4.2.5**: Add MGA compliance (4-6 hours)
  - Node xmin/xmax tracking
  - Visibility checks during traversal
  - Dead node pruning

- [ ] **4.2.6**: Test with embeddings (2-3 hours)
  - Load text embeddings (1536-dim)
  - Benchmark accuracy (recall@10)
  - Benchmark query latency

**Acceptance Criteria**:
- HNSW fully MGA-compliant
- Recall@10 > 95%
- Query latency < 10ms for 1M vectors

**Total Estimated Time**: 40-60 hours

---

### TASK 4.3: Implement LSM Tree Index

**Priority**: 🟢 MEDIUM
**Estimated Time**: 60-80 hours
**Dependencies**: WAL implementation (for crash recovery)

#### Design Requirements

**MGA Compliance**:
- Memtable MVCC (xmin/xmax per entry)
- SSTable tombstones for deletes
- Snapshot isolation across memtable + SSTables
- Compaction preserves visibility

#### Subtasks

- [ ] **4.3.1**: Design memtable structure (8-10 hours)
  - In-memory sorted buffer (skip list or red-black tree)
  - xmin/xmax per entry
  - Efficient insert/search

- [ ] **4.3.2**: Implement SSTable format (10-12 hours)
  - On-disk sorted string table
  - Block-based structure
  - Bloom filter for fast lookups
  - xmin/xmax in entries

- [ ] **4.3.3**: Implement compaction (16-20 hours)
  - Leveled compaction strategy
  - Merge SSTables
  - Remove tombstones (deleted entries)
  - Preserve MVCC visibility

- [ ] **4.3.4**: Implement reads (8-10 hours)
  - Check memtable first
  - Search SSTables (newest to oldest)
  - Merge results from multiple levels
  - Apply visibility filters

- [ ] **4.3.5**: Add MGA compliance (6-8 hours)
  - Snapshot isolation
  - Visibility checks across memtable + SSTables
  - Dead entry removal in compaction

- [ ] **4.3.6**: Integrate with WAL (6-8 hours)
  - Log memtable writes to WAL
  - Recover memtable on crash
  - Replay WAL entries

- [ ] **4.3.7**: Test write-heavy workload (3-4 hours)
  - Insert 10M rows at high rate
  - Benchmark write throughput
  - Compare to B-tree

**Acceptance Criteria**:
- LSM Tree fully MGA-compliant
- Write throughput > 2x B-tree
- Crash recovery works

**Total Estimated Time**: 60-80 hours

---

### TASK 4.4: Implement GIST Index

**Priority**: 🟢 EXTENSIBILITY
**Estimated Time**: 80-120 hours
**Dependencies**: Operator class system

#### Design Requirements

**MGA Compliance**:
- xmin/xmax per entry (same as B-tree)
- Visibility checks during tree traversal
- Dead entry pruning
- Transaction-aware page splits

#### Subtasks

- [ ] **4.4.1**: Design operator class system (20-30 hours)
  - Define operator class interface
  - Comparison functions (overlap, contains, adjacent, etc.)
  - Penalty/picksplit functions for insertion
  - Consistent function for searches

- [ ] **4.4.2**: Implement GIST tree structure (20-30 hours)
  - Similar to R-tree (bounding boxes)
  - Generic predicate support
  - Page split algorithms

- [ ] **4.4.3**: Implement range types (20-30 hours)
  - int4range, int8range, tsrange, daterange
  - Range operators (overlaps, contains, before, after)
  - Register operator classes

- [ ] **4.4.4**: Add MGA compliance (8-10 hours)
  - xmin/xmax tracking
  - Visibility checks
  - Dead entry removal

- [ ] **4.4.5**: Test with custom types (8-10 hours)
  - Test range queries
  - Test geometric types (if implemented)
  - Test extensibility

- [ ] **4.4.6**: Document extension API (4-6 hours)
  - How to add new operator classes
  - How to define comparison functions
  - Examples

**Acceptance Criteria**:
- GIST fully MGA-compliant
- Extensible for custom types
- Range types work correctly

**Total Estimated Time**: 80-120 hours

---

### TASK 4.5: Implement R-Tree Index

**Priority**: 🟢 GIS SUPPORT
**Estimated Time**: 40-60 hours
**Dependencies**: GIST implementation, geometric types

#### Design Requirements

**MGA Compliance**: Same as GIST

#### Subtasks

- [ ] **4.5.1**: Implement MBR (Minimum Bounding Rectangle) (6-8 hours)
  - Bounding box calculations
  - Overlap detection
  - Containment checks

- [ ] **4.5.2**: Implement quadratic split (10-12 hours)
  - Choose split algorithm (quadratic, R*, Hilbert)
  - Minimize overlap after split
  - Optimize for query performance

- [ ] **4.5.3**: Implement spatial operators (10-12 hours)
  - Overlap, contains, within, intersects, distance

- [ ] **4.5.4**: Add MGA compliance (4-6 hours)
  - Spatial + temporal visibility
  - Dead entry removal

- [ ] **4.5.5**: Integrate with PostGIS types (6-8 hours)
  - POINT, LINESTRING, POLYGON
  - Coordinate systems

- [ ] **4.5.6**: Test with GIS queries (4-6 hours)
  - Find all points within polygon
  - Nearest neighbor searches
  - Overlap queries

**Acceptance Criteria**:
- R-Tree fully MGA-compliant
- PostGIS compatibility
- Spatial queries work correctly

**Total Estimated Time**: 40-60 hours

---

### TASK 4.6: Implement SPGIST Index

**Priority**: 🟢 LOW (ADVANCED SPATIAL)
**Estimated Time**: 60-80 hours
**Dependencies**: GIST implementation

#### Design Requirements

**MGA Compliance**: Same as GIST + space partition versioning

#### Subtasks

- [ ] **4.6.1**: Implement space partitioning (16-20 hours)
  - Quad-tree for 2D points
  - K-d tree for multi-dimensional
  - Radix tree for strings

- [ ] **4.6.2**: Implement choose/picksplit functions (16-20 hours)
  - Different strategies per type
  - Optimize for space partitioning

- [ ] **4.6.3**: Implement IP address trees (12-16 hours)
  - CIDR range searches
  - Efficient for network addresses

- [ ] **4.6.4**: Add MGA compliance (6-8 hours)
  - Partition versioning
  - Visibility checks

- [ ] **4.6.5**: Test with various data types (8-10 hours)
  - Points, strings, IP addresses
  - Benchmark vs GIST

**Acceptance Criteria**:
- SPGIST fully MGA-compliant
- Efficient for specialized types
- Better than GIST for those types

**Total Estimated Time**: 60-80 hours

---

## SUMMARY CHECKLIST

### Phase 1: Critical MGA Compliance (Alpha 1.4)
- [ ] TASK 1.1: Add Snapshot parameter to all APIs (8-12h)
- [ ] TASK 1.2: B-Tree visibility checks (6-8h)
- [ ] TASK 1.3: Hash xmin tracking + visibility (10-14h)
- [ ] TASK 1.4: GIN visibility checks (8-12h)
- [ ] TASK 1.5: Bitmap post-filter (6-8h)
- [ ] TASK 1.6: Integration testing (6-8h)
**Total: 38-54 hours**

### Phase 2: Dead Entry Pruning (Alpha 1.5)
- [ ] TASK 2.1: Design GC protocol (4-6h)
- [ ] TASK 2.2: B-Tree dead entry removal (12-16h)
- [ ] TASK 2.3: Hash dead entry removal (10-14h)
- [ ] TASK 2.4: GIN dead entry removal (16-20h)
- [ ] TASK 2.5: Bitmap dead entry removal (6-8h)
- [ ] TASK 2.6: Integration with heap VACUUM (6-8h)
**Total: 48-64 hours**

### Phase 3: Full MGA Integration (Beta)
- [ ] TASK 3.1: Add xmax support (12-16h)
- [ ] TASK 3.2: Index-level MVCC snapshots (20-30h)
- [ ] TASK 3.3: Optimize visibility checks (10-14h)
- [ ] TASK 3.4: Benchmark and tune (8-12h)
**Total: 50-72 hours**

### Phase 4: New Index Types (Post-Beta)
- [ ] TASK 4.1: BRIN Index (20-30h)
- [ ] TASK 4.2: VECTOR Index (40-60h)
- [ ] TASK 4.3: LSM Tree Index (60-80h)
- [ ] TASK 4.4: GIST Index (80-120h)
- [ ] TASK 4.5: R-Tree Index (40-60h)
- [ ] TASK 4.6: SPGIST Index (60-80h)
**Total: 300-470 hours**

---

## GRAND TOTAL

**All Phases**: 436-660 hours (11-17 weeks for 1 developer)

**Critical Path to Production**:
- Phase 1 + Phase 2 + Phase 3 = **136-190 hours (3.5-5 weeks)**

**Critical Path to Alpha Release**:
- Phase 1 only = **38-54 hours (1-1.5 weeks)**

---

**Document Status**: ✅ Complete
**Next Review**: After Phase 1 completion
**Owner**: Database Team
**Created**: October 18, 2025

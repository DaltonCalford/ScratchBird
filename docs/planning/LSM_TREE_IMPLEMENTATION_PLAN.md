# LSM-Tree Index - Detailed Implementation Plan

**Project**: ScratchBird Database Engine
**Component**: LSM-Tree (Log-Structured Merge-Tree)
**Status**: PHASE 1 COMPLETE - PHASE 2 STARTING (November 5, 2025)
**Estimated Effort**: 80-110 hours remaining (20-30 hours completed)
**Specification**: `/docs/specifications/LSM_TREE_SPEC.md`

---

## ⚠️ CRITICAL: MGA COMPLIANCE REQUIRED

**BEFORE ANY WORK**: Read `/MGA_RULES.md` - Firebird MGA rules are ABSOLUTE.

**Key MGA Rules for LSM-Tree**:
- Use `TransactionId` (uint64_t), NOT `Snapshot` or `SnapshotData`
- Every entry (memtable + SSTable) has `xmin`/`xmax` for MGA visibility
- TIP-based visibility via `TransactionManager::isVersionVisible()`
- Soft delete: Set `xmax`, physical removal via garbage collection during compaction
- NO PostgreSQL MVCC contamination

**MGA References in Code**:
```cpp
// CORRECT: Firebird MGA
bool isEntryVisible(const MemtableEntry& entry, uint64_t current_xid,
                    TransactionManager* txn_mgr);

// WRONG: PostgreSQL MVCC (DO NOT USE)
bool isEntryVisible(const MemtableEntry& entry, Snapshot snapshot, ...);  // ❌ WRONG!
```

**If context is lost during compaction, re-read `/MGA_RULES.md` immediately.**

---

## Table of Contents

1. [Overview](#1-overview)
2. [Phase 1: Memtable (20-30 hours)](#2-phase-1-memtable-20-30-hours)
3. [Phase 2: SSTable Writer (20-30 hours)](#3-phase-2-sstable-writer-20-30-hours)
4. [Phase 3: SSTable Reader (20-30 hours)](#4-phase-3-sstable-reader-20-30-hours)
5. [Phase 4: Compaction (30-40 hours)](#5-phase-4-compaction-30-40-hours)
6. [Phase 5: WAL Integration (15-20 hours)](#6-phase-5-wal-integration-15-20-hours)
7. [Phase 6: Bloom Filter (10-15 hours)](#7-phase-6-bloom-filter-10-15-hours)
8. [Phase 7: LSMTreeIndex Integration (20-30 hours)](#8-phase-7-lsmtreeindex-integration-20-30-hours)
9. [Phase 8: Testing & Optimization (20-30 hours)](#9-phase-8-testing--optimization-20-30-hours)
10. [Progress Tracking](#10-progress-tracking)
11. [Risk Mitigation](#11-risk-mitigation)

---

## 1. Overview

### 1.1 Current Status

**Existing Infrastructure**:
- ❌ NO existing LSM-Tree code
- ✅ PageManager available (for SSTable pages)
- ✅ BufferPool available (for caching SSTables)
- ✅ TransactionManager available (for MGA visibility)
- ✅ Build system configured

**Completion Status**:
- ❌ NOT STARTED - No implementation exists
- ❌ Does NOT count toward project completion percentage
- ❌ Blocks Alpha Phase 1 completion

### 1.2 Implementation Strategy

**Approach**: Implement components bottom-up, with complete testing at each phase.

**Order**:
1. Memtable (in-memory sorted map, Red-Black Tree)
2. SSTable Writer (flush memtable to disk)
3. SSTable Reader (read SSTables, binary search)
4. Compaction (merge SSTables, garbage collection)
5. WAL (durability, crash recovery)
6. Bloom Filter (reduce read amplification)
7. LSMTreeIndex (orchestrate all components)
8. Testing & optimization

**Each phase must**:
- Complete implementation (NO stubs)
- Pass unit tests (100% coverage)
- Pass integration tests
- Document performance characteristics

---

## 2. Phase 1: Memtable (20-30 hours)

### 2.1 Overview

Implement in-memory sorted map using Red-Black Tree (std::map).

**Requirements**:
- O(log n) insert, search, delete
- Thread-safe (mutex)
- MGA compliance (xmin/xmax on entries)
- Size tracking (trigger flush at 4 MB)
- Range scan support

### 2.2 Tasks

#### Task 2.1: Memtable Entry Structure (2-3 hours)

**File**: `include/scratchbird/core/lsm_tree.h` (create new)

**Requirements**:
- Define `MemtableEntry` structure
- Variable-length key and value
- Sequence number (monotonic)
- Entry type (Insert or Delete)
- MGA fields (xmin, xmax)
- Comparison operator for std::map

**Acceptance Criteria**:
- [ ] Structure compiles cleanly
- [ ] Comparison operator works correctly (lexicographic key order, then sequence)
- [ ] MGA fields included (xmin, xmax)

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 3.2):
```cpp
struct MemtableEntry {
    std::vector<uint8_t> key;
    std::vector<uint8_t> value;
    uint64_t sequence_number;
    uint8_t entry_type;  // 0 = Insert, 1 = Delete
    uint64_t xmin;       // MGA: Transaction that created
    uint64_t xmax;       // MGA: Transaction that deleted (0 if active)

    bool operator<(const MemtableEntry& other) const;
};
```

---

#### Task 2.2: Memtable Class Definition (4-6 hours)

**File**: `include/scratchbird/core/lsm_tree.h`

**Requirements**:
- Define `Memtable` class
- Methods: `put()`, `remove()`, `get()`, `scan()`, `isFull()`, `getSize()`, `getAllEntries()`
- Private: `std::map<MemtableEntry, bool>`, mutex, size tracking

**Acceptance Criteria**:
- [ ] Class compiles cleanly
- [ ] All methods declared
- [ ] Thread-safe access (mutex)
- [ ] Size tracking

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 3.3).

---

#### Task 2.3: Memtable Put Implementation (4-6 hours)

**File**: `src/core/lsm_tree.cpp` (create new)

**Requirements**:
- Insert entry into Red-Black Tree (std::map)
- Increment sequence number
- Track size (trigger flush at 4 MB)
- Thread-safe (lock mutex)

**Acceptance Criteria**:
- [ ] Inserts entries correctly
- [ ] Size tracking works
- [ ] Returns `ResourceExhausted` when full
- [ ] Thread-safe

**MGA Notes**: `put()` sets `xmin` to current transaction ID, `xmax` to 0.

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 3.4).

---

#### Task 2.4: Memtable Get Implementation (4-6 hours)

**File**: `src/core/lsm_tree.cpp`

**Requirements**:
- Search Red-Black Tree for key
- Return latest visible version (MGA visibility check)
- Handle tombstones (entry_type = Delete)

**Acceptance Criteria**:
- [ ] Returns correct value for key
- [ ] MGA visibility filtering works (xmin/xmax)
- [ ] Handles tombstones (returns NOT FOUND)
- [ ] Returns NOT FOUND if key not present

**MGA Reference**: See `/MGA_RULES.md` Section 4 (Visibility Rules).

**Code Example**:
```cpp
Status Memtable::get(const std::vector<uint8_t>& key,
                     uint64_t current_xid,
                     TransactionManager* txn_mgr,
                     std::vector<uint8_t>* value_out,
                     bool* found,
                     ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    *found = false;

    // Search for key (iterate versions newest first)
    auto it = entries_.lower_bound(createSearchKey(key));
    while (it != entries_.end() && it->first.key == key) {
        const MemtableEntry& entry = it->first;

        // Check MGA visibility (Firebird rules)
        if (isEntryVisible(entry, current_xid, txn_mgr)) {
            if (entry.entry_type == 0) {  // Insert
                *value_out = entry.value;
                *found = true;
                return Status::OK;
            } else {  // Delete (tombstone)
                return Status::OK;
            }
        }
        ++it;
    }
    return Status::OK;
}
```

---

#### Task 2.5: Memtable Range Scan (4-6 hours)

**File**: `src/core/lsm_tree.cpp`

**Requirements**:
- Scan entries in key order (start_key to end_key)
- Apply MGA visibility filtering
- Return entries in sorted order

**Acceptance Criteria**:
- [ ] Returns entries in correct order
- [ ] Respects start/end keys
- [ ] MGA visibility filtering works
- [ ] Handles empty range

---

#### Task 2.6: Unit Tests for Memtable (4-6 hours)

**File**: `tests/unit/test_lsm_memtable.cpp` (create new)

**Test Cases**:
1. [ ] Insert and retrieve single key-value
2. [ ] Insert multiple keys, verify sorted order
3. [ ] Update existing key (replace value)
4. [ ] Delete key (tombstone)
5. [ ] Range scan with start/end keys
6. [ ] MGA visibility filtering (xmin/xmax)
7. [ ] Memtable full detection (4 MB)
8. [ ] Thread-safety (concurrent inserts from 10 threads)

**Command to run tests**:
```bash
make test_lsm_memtable
./tests/test_lsm_memtable
```

**Acceptance Criteria**: All 8 tests pass with 100% code coverage.

---

**Phase 1 Total**: 22-33 hours

**Milestone**: Memtable fully implemented and tested.

---

## 3. Phase 2: SSTable Writer (20-30 hours)

### 3.1 Overview

Implement SSTable writer to flush memtable to disk.

**Requirements**:
- Data blocks (4 KB each, sorted entries)
- Index block (binary search entry points)
- Bloom filter (probabilistic membership test)
- Footer (metadata, checksums)

### 3.2 Tasks

#### Task 3.1: SSTable Footer Structure (2-3 hours)

**File**: `include/scratchbird/core/lsm_tree.h`

**Requirements**:
- Define `SSTableFooter` structure
- Magic number (0x5353544142 "SSTAB")
- Version, index offset, bloom offset
- Num entries, min/max keys
- Checksum (CRC32)

**Acceptance Criteria**:
- [ ] Structure compiles cleanly
- [ ] Fixed size (for reading from end of file)

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 4.3).

---

#### Task 3.2: SSTableWriter Class (4-6 hours)

**File**: `include/scratchbird/core/lsm_tree.h`

**Requirements**:
- Define `SSTableWriter` class
- Methods: `open()`, `addEntry()`, `finish()`
- Private: file stream, current block, index entries, Bloom filter

**Acceptance Criteria**:
- [ ] Class compiles cleanly
- [ ] All methods declared

---

#### Task 3.3: SSTableWriter addEntry Implementation (8-10 hours)

**File**: `src/core/lsm_tree.cpp`

**Requirements**:
- Serialize entry (key_len, key, value_len, value, seq, type, xmin, xmax)
- Add to current block (4 KB)
- Flush block when full
- Update Bloom filter
- Track min/max keys

**Acceptance Criteria**:
- [ ] Entries serialized correctly
- [ ] Blocks flushed at 4 KB boundary
- [ ] Bloom filter updated
- [ ] Min/max keys tracked

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 4.5).

---

#### Task 3.4: SSTableWriter finish Implementation (6-8 hours)

**File**: `src/core/lsm_tree.cpp`

**Requirements**:
- Flush last block
- Write index block (binary search entry points)
- Write Bloom filter
- Write footer (metadata, checksum)

**Acceptance Criteria**:
- [ ] Index block written correctly
- [ ] Bloom filter serialized
- [ ] Footer written (with checksum)
- [ ] File closed cleanly

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 4.5).

---

#### Task 3.5: Unit Tests for SSTable Writer (4-6 hours)

**File**: `tests/unit/test_lsm_sstable_writer.cpp` (create new)

**Test Cases**:
1. [ ] Write SSTable with 100 entries
2. [ ] Write SSTable with 10,000 entries (multiple blocks)
3. [ ] Verify min/max keys
4. [ ] Verify Bloom filter includes all keys
5. [ ] Verify footer checksum
6. [ ] Verify entries written in sorted order
7. [ ] Verify MGA fields (xmin/xmax) preserved

**Acceptance Criteria**: All 7 tests pass.

---

**Phase 2 Total**: 24-33 hours

**Milestone**: SSTable Writer fully implemented and tested.

---

## 4. Phase 3: SSTable Reader (20-30 hours)

### 4.1 Overview

Implement SSTable reader to read SSTables from disk.

**Requirements**:
- Read footer (parse metadata)
- Deserialize Bloom filter
- Binary search index block
- Read data blocks
- MGA visibility filtering

### 4.2 Tasks

#### Task 4.1: SSTableReader Class (4-6 hours)

**File**: `include/scratchbird/core/lsm_tree.h`

**Requirements**:
- Define `SSTableReader` class
- Methods: `open()`, `get()`, `scan()`, `createIterator()`, `minKey()`, `maxKey()`
- Private: file stream, footer, index, Bloom filter

**Acceptance Criteria**:
- [ ] Class compiles cleanly
- [ ] All methods declared

---

#### Task 4.2: SSTableReader open Implementation (4-6 hours)

**File**: `src/core/lsm_tree.cpp`

**Requirements**:
- Read footer from end of file
- Verify magic number and checksum
- Deserialize Bloom filter
- Parse index block

**Acceptance Criteria**:
- [ ] Footer parsed correctly
- [ ] Bloom filter deserialized
- [ ] Index block loaded
- [ ] Handles corrupted file (return error)

---

#### Task 4.3: SSTableReader get Implementation (8-10 hours)

**File**: `src/core/lsm_tree.cpp`

**Requirements**:
- Check Bloom filter (skip if not present)
- Binary search index block to find data block
- Read data block
- Scan entries in block
- Apply MGA visibility filtering

**Acceptance Criteria**:
- [ ] Bloom filter check works (true negatives skip file)
- [ ] Binary search finds correct block
- [ ] Returns correct value for key
- [ ] MGA visibility filtering works
- [ ] Returns NOT FOUND if key not present

**MGA Reference**: See `/MGA_RULES.md` Section 4.

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 5.3).

---

#### Task 4.4: SSTableReader Iterator (6-8 hours)

**File**: `src/core/lsm_tree.cpp`

**Requirements**:
- Implement iterator for range scans
- Read blocks sequentially
- Deserialize entries
- Apply MGA visibility filtering

**Acceptance Criteria**:
- [ ] Iterator returns entries in sorted order
- [ ] Handles block boundaries
- [ ] MGA visibility filtering works
- [ ] Handles end of file

---

#### Task 4.5: Unit Tests for SSTable Reader (4-6 hours)

**File**: `tests/unit/test_lsm_sstable_reader.cpp` (create new)

**Test Cases**:
1. [ ] Open and read SSTable
2. [ ] Get single entry (point query)
3. [ ] Bloom filter true negative (key not in file)
4. [ ] Bloom filter false positive (check I/O still happens)
5. [ ] Range scan with iterator
6. [ ] MGA visibility filtering (xmin/xmax)
7. [ ] Handle corrupted footer (checksum mismatch)
8. [ ] Handle corrupted data block

**Acceptance Criteria**: All 8 tests pass.

---

**Phase 3 Total**: 26-36 hours

**Milestone**: SSTable Reader fully implemented and tested.

---

## 5. Phase 4: Compaction (30-40 hours)

### 5.1 Overview

Implement compaction to merge SSTables and remove garbage.

**Requirements**:
- K-way merge (priority queue)
- Level 0 → Level 1 compaction
- Level N → Level N+1 compaction (generic)
- Tombstone removal
- Garbage collection (MGA rules)
- Atomic SSTable replacement

### 5.2 Tasks

#### Task 5.1: Compaction Strategy (4-6 hours)

**File**: `include/scratchbird/core/lsm_tree.h`

**Requirements**:
- Define `CompactionStrategy` enum (LEVELED, TIERED)
- Define `LevelMetadata` structure (size limits, SSTables)
- Define `CompactionTask` structure (source SSTables, target level)

**Acceptance Criteria**:
- [ ] Structures compile cleanly
- [ ] Leveled compaction strategy defined

---

#### Task 5.2: K-Way Merge Algorithm (10-12 hours)

**File**: `src/core/lsm_tree_compaction.cpp` (create new)

**Requirements**:
- Use priority queue to merge sorted SSTables
- Handle duplicate keys (keep newest version)
- Remove tombstones (entry_type = Delete)
- Remove invisible entries (garbage collection)
- Write merged output to new SSTables

**Acceptance Criteria**:
- [ ] Merges SSTables correctly (sorted order)
- [ ] Removes duplicate keys (keeps newest)
- [ ] Removes tombstones
- [ ] Garbage collection works (Firebird MGA rules)

**MGA Reference**: See `/MGA_RULES.md` Section 6 (Garbage Collection).

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 7.2):
```cpp
Status compactLevel0ToLevel1(LSMTreeIndex* lsm, ErrorContext* ctx) {
    // 1. Select all Level 0 SSTables
    std::vector<SSTableReader*> level0_sstables = lsm->getLevel0SSTables();

    // 2. Find overlapping Level 1 SSTables
    std::vector<SSTableReader*> level1_sstables;
    for (auto* sst : level0_sstables) {
        auto overlapping = lsm->findOverlappingSSTables(1, sst->minKey(), sst->maxKey());
        level1_sstables.insert(level1_sstables.end(), overlapping.begin(), overlapping.end());
    }

    // 3. K-way merge using priority queue
    std::priority_queue<MergeEntry> pq;
    for (auto* sst : level0_sstables) {
        auto it = sst->createIterator();
        if (it->valid()) pq.push(createMergeEntry(it, sst));
    }
    for (auto* sst : level1_sstables) {
        auto it = sst->createIterator();
        if (it->valid()) pq.push(createMergeEntry(it, sst));
    }

    // 4. Create new Level 1 SSTables
    SSTableWriter writer(generateNewSSTablePath(1), 4096);
    writer.open(ctx);

    uint64_t oldest_active_xid = lsm->getOldestActiveXid();
    std::vector<uint8_t> last_key;

    while (!pq.empty()) {
        MergeEntry entry = pq.top();
        pq.pop();

        // Skip duplicate keys (keep only newest version)
        if (!last_key.empty() && entry.key == last_key) continue;

        // Garbage collection (Firebird MGA rules)
        if (canGarbageCollect(entry, oldest_active_xid, lsm->txn_mgr())) {
            continue;
        }

        // Write to new SSTable
        writer.addEntry(entry.key, entry.value, entry.sequence,
                       entry.type, entry.xmin, entry.xmax, ctx);
        last_key = entry.key;

        // Advance iterator
        advanceIterator(&pq, entry.iterator, entry.source_sstable);
    }

    writer.finish(ctx);

    // 5. Atomically replace old SSTables with new ones
    lsm->replaceLevel0And1SSTables(level0_sstables, level1_sstables, {writer.filePath()});

    // 6. Delete old SSTable files
    deleteOldSSTables(level0_sstables, level1_sstables);

    return Status::OK;
}
```

---

#### Task 5.3: Garbage Collection (MGA) (8-10 hours)

**File**: `src/core/lsm_tree_compaction.cpp`

**Requirements**:
- Identify invisible entries (xmin/xmax committed, no active transactions can see)
- Remove invisible entries during compaction
- Use `TransactionManager::getOldestActiveXid()` for safety

**Acceptance Criteria**:
- [ ] Correctly identifies invisible entries (Firebird MGA rules)
- [ ] NO PostgreSQL MVCC (verify no Snapshot usage)
- [ ] Safe: Only removes entries invisible to ALL active transactions

**MGA Reference**: See `/MGA_RULES.md` Section 6.

**Code Example**:
```cpp
bool canGarbageCollect(const SSTableEntry& entry,
                       uint64_t oldest_active_xid,
                       TransactionManager* txn_mgr) {
    // Entry must be deleted (xmax != 0)
    if (entry.xmax == 0) return false;

    // Both xmin and xmax must be committed
    if (!txn_mgr->isCommitted(entry.xmin)) return false;
    if (!txn_mgr->isCommitted(entry.xmax)) return false;

    // No active transaction can see this version
    if (entry.xmax < oldest_active_xid) {
        return true;
    }

    return false;
}
```

---

#### Task 5.4: Level Management (6-8 hours)

**File**: `src/core/lsm_tree.cpp`

**Requirements**:
- Track Level 0-3 SSTables
- Detect when compaction needed (Level 0 has 4+ files, Level N exceeds size limit)
- Trigger background compaction

**Acceptance Criteria**:
- [ ] Level metadata tracked correctly
- [ ] Compaction triggered at correct thresholds
- [ ] Background thread pool manages compactions

---

#### Task 5.5: Unit Tests for Compaction (6-8 hours)

**File**: `tests/unit/test_lsm_compaction.cpp` (create new)

**Test Cases**:
1. [ ] Compact Level 0 to Level 1 (k-way merge)
2. [ ] Remove tombstones during compaction
3. [ ] Remove duplicate keys (keep newest)
4. [ ] Garbage collection (remove old versions)
5. [ ] Verify sorted order after compaction
6. [ ] Atomic replacement of SSTables
7. [ ] Concurrent reads during compaction (thread safety)

**Acceptance Criteria**: All 7 tests pass.

---

**Phase 4 Total**: 34-44 hours

**Milestone**: Compaction fully implemented and tested.

---

## 6. Phase 5: WAL Integration (15-20 hours)

### 6.1 Overview

Implement Write-Ahead Log for durability.

**Requirements**:
- Append-only log file
- Record every write BEFORE updating memtable
- Recovery: Replay WAL on startup
- Truncation: Delete WAL after memtable flush

### 6.2 Tasks

#### Task 6.1: WAL Entry Format (2-3 hours)

**File**: `include/scratchbird/core/lsm_tree.h`

**Requirements**:
- Define `WALEntry` structure
- Entry size, sequence, type (Put/Delete)
- xmin, xmax (MGA)
- Variable-length key and value
- Checksum (CRC32)

**Acceptance Criteria**:
- [ ] Structure compiles cleanly
- [ ] Supports variable-length key/value

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 8.2).

---

#### Task 6.2: WALWriter Implementation (6-8 hours)

**File**: `src/core/lsm_tree_wal.cpp` (create new)

**Requirements**:
- Append entry to WAL file
- Sync to disk (fsync)
- Truncate after memtable flush

**Acceptance Criteria**:
- [ ] Entries appended correctly
- [ ] fsync ensures durability
- [ ] Truncation works

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 8.4).

---

#### Task 6.3: WAL Recovery (4-6 hours)

**File**: `src/core/lsm_tree_wal.cpp`

**Requirements**:
- Read WAL on startup
- Replay entries into memtable
- Handle corrupted entries (checksum mismatch)

**Acceptance Criteria**:
- [ ] Memtable reconstructed from WAL
- [ ] Handles corrupted WAL (stop at first error)
- [ ] WAL deleted after recovery

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 8.5).

---

#### Task 6.4: Unit Tests for WAL (3-4 hours)

**File**: `tests/unit/test_lsm_wal.cpp` (create new)

**Test Cases**:
1. [ ] Append entries to WAL
2. [ ] Sync WAL to disk (fsync)
3. [ ] Recover memtable from WAL
4. [ ] Truncate WAL after memtable flush
5. [ ] Handle corrupted WAL entries (checksum)
6. [ ] WAL entry order matches memtable order

**Acceptance Criteria**: All 6 tests pass.

---

**Phase 5 Total**: 15-21 hours

**Milestone**: WAL integration fully implemented and tested.

---

## 7. Phase 6: Bloom Filter (10-15 hours)

### 7.1 Overview

Implement Bloom filter to reduce read amplification.

**Requirements**:
- Probabilistic membership test
- False positive rate ~1% (10 bits/key)
- Serialization/deserialization

### 7.2 Tasks

#### Task 7.1: BloomFilter Class (4-6 hours)

**File**: `src/core/bloom_filter.cpp` (create new)

**Requirements**:
- Bit array implementation
- Hash functions (MurmurHash3 or FNV-1a)
- Methods: `add()`, `mightContain()`, `serialize()`, `deserialize()`

**Acceptance Criteria**:
- [ ] Bloom filter works correctly
- [ ] False positive rate ~1%
- [ ] Serialization/deserialization works

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 6.3).

---

#### Task 7.2: Unit Tests for Bloom Filter (4-6 hours)

**File**: `tests/unit/test_lsm_bloom_filter.cpp` (create new)

**Test Cases**:
1. [ ] Add 1000 keys, verify all present
2. [ ] Check 1000 absent keys, measure false positive rate (<2%)
3. [ ] Serialize and deserialize Bloom filter
4. [ ] Test with different bit sizes (5, 10, 20 bits/key)
5. [ ] Large dataset (100K keys)

**Acceptance Criteria**: All 5 tests pass, false positive rate <2%.

---

**Phase 6 Total**: 8-12 hours

**Milestone**: Bloom filter fully implemented and tested.

---

## 8. Phase 7: LSMTreeIndex Integration (20-30 hours)

### 8.1 Overview

Orchestrate all components: memtable, SSTables, compaction, WAL, Bloom filter.

### 8.2 Tasks

#### Task 8.1: LSMTreeIndex Class (6-8 hours)

**File**: `include/scratchbird/core/lsm_tree.h`

**Requirements**:
- Define `LSMTreeIndex` class
- Methods: `create()`, `open()`, `put()`, `get()`, `remove()`, `scan()`
- Private: memtable, immutable memtable, SSTables (Levels 0-3), WAL, compaction thread

**Acceptance Criteria**:
- [ ] Class compiles cleanly
- [ ] All methods declared

---

#### Task 8.2: Put/Get/Remove Implementation (8-10 hours)

**File**: `src/core/lsm_tree.cpp`

**Requirements**:
- `put()`: Append to WAL, insert into memtable, trigger flush if full
- `get()`: Check memtable → immutable memtable → Level 0-3 SSTables
- `remove()`: Append to WAL, insert tombstone into memtable

**Acceptance Criteria**:
- [ ] Put writes to WAL then memtable
- [ ] Get checks all sources (memtable first, then SSTables)
- [ ] Remove inserts tombstone

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Sections 8.4, 9.1).

---

#### Task 8.3: Range Scan Implementation (8-10 hours)

**File**: `src/core/lsm_tree.cpp`

**Requirements**:
- K-way merge across memtable + all SSTables
- Return entries in sorted order
- Skip duplicates (keep newest version)
- Apply MGA visibility filtering

**Acceptance Criteria**:
- [ ] Returns entries in correct order
- [ ] Handles overlapping SSTables
- [ ] MGA visibility filtering works
- [ ] Performance: O(log n + k) per level

**Code Reference** (see `/docs/specifications/LSM_TREE_SPEC.md` Section 9.2).

---

#### Task 8.4: Memtable Flush (4-6 hours)

**File**: `src/core/lsm_tree.cpp`

**Requirements**:
- Mark memtable as immutable
- Create new active memtable
- Background: Flush immutable memtable to Level 0 SSTable
- Truncate WAL after flush

**Acceptance Criteria**:
- [ ] Memtable flushed to SSTable
- [ ] WAL truncated
- [ ] New memtable created

---

#### Task 8.5: Integration Tests (4-6 hours)

**File**: `tests/integration/test_lsm_tree_integration.cpp` (create new)

**Test Cases**:
1. [ ] Insert 10K keys, verify all readable
2. [ ] Insert until memtable flushes (verify Level 0 SSTable created)
3. [ ] Insert until Level 0 compacts (verify Level 1 SSTable created)
4. [ ] Interleaved reads and writes
5. [ ] Range scan across memtable and SSTables
6. [ ] Delete keys, verify tombstone behavior
7. [ ] Crash recovery (kill process, restart, verify data)
8. [ ] MGA isolation (concurrent transactions see correct versions)

**Acceptance Criteria**: All 8 tests pass.

---

**Phase 7 Total**: 30-40 hours

**Milestone**: LSMTreeIndex fully integrated and tested.

---

## 9. Phase 8: Testing & Optimization (20-30 hours)

### 9.1 Tasks

#### Task 9.1: Performance Benchmarks (8-10 hours)

**File**: `tests/benchmark/benchmark_lsm_tree.cpp` (create new)

**Benchmarks**:
1. [ ] Sequential insert throughput (ops/sec)
2. [ ] Random insert throughput
3. [ ] Point query latency (ms)
4. [ ] Range scan throughput (rows/sec)
5. [ ] Write amplification measurement
6. [ ] Space amplification measurement
7. [ ] Compaction CPU usage

**Acceptance Criteria**:
- Sequential inserts: 100K-500K ops/sec
- Random inserts: 50K-200K ops/sec
- Point queries: 10K-50K ops/sec
- Range scans: 1K-10K ops/sec (1K rows)
- Write amplification: 5x-30x
- Space amplification: 10-30%

---

#### Task 9.2: Stress Testing (6-8 hours)

**File**: `tests/stress/stress_test_lsm_tree.cpp` (create new)

**Stress Tests**:
1. [ ] Insert 1M keys (verify no data loss)
2. [ ] Concurrent readers and writers (100 threads)
3. [ ] Simulate crashes during compaction
4. [ ] Bloom filter effectiveness (measure false positives)
5. [ ] WAL recovery with large dataset (100 MB WAL)

**Acceptance Criteria**: All stress tests pass without crashes or data loss.

---

#### Task 9.3: Memory Profiling (4-6 hours)

**Requirements**:
- Detect memory leaks (use valgrind)
- Optimize memory usage (reduce allocations)
- Verify no leaks in compaction

**Acceptance Criteria**:
- [ ] No memory leaks detected
- [ ] Memory usage within budget (<100 MB for 1M keys)

---

#### Task 9.4: Documentation (2-4 hours)

**Files**:
- Create `/docs/status/LSM_TREE_COMPLETION_REPORT_2025-11-XX.md`
- Update `/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md` (mark complete)

**Acceptance Criteria**:
- [ ] Completion report created
- [ ] Master plan updated (100% completion - ALL 12 indexes complete!)

---

**Phase 8 Total**: 20-28 hours

**Milestone**: LSM-Tree fully tested, optimized, and documented.

---

## 10. Progress Tracking

### 10.1 Completion Checklist

**Phase 1: Memtable** (20-30 hours) ✅ **COMPLETE** - November 5, 2025
- [x] Task 2.1: Memtable Entry Structure (2-3 hours) ✅
- [x] Task 2.2: Memtable Class Definition (4-6 hours) ✅
- [x] Task 2.3: Memtable Put Implementation (4-6 hours) ✅
- [x] Task 2.4: Memtable Get Implementation (4-6 hours) ✅
- [x] Task 2.5: Memtable Range Scan (4-6 hours) ✅
- [x] Task 2.6: Unit Tests for Memtable (4-6 hours) ✅ - 8 tests passing

**Phase 2: SSTable Writer** (20-30 hours)
- [ ] Task 3.1: SSTable Footer Structure (2-3 hours)
- [ ] Task 3.2: SSTableWriter Class (4-6 hours)
- [ ] Task 3.3: SSTableWriter addEntry Implementation (8-10 hours)
- [ ] Task 3.4: SSTableWriter finish Implementation (6-8 hours)
- [ ] Task 3.5: Unit Tests for SSTable Writer (4-6 hours)

**Phase 3: SSTable Reader** (20-30 hours)
- [ ] Task 4.1: SSTableReader Class (4-6 hours)
- [ ] Task 4.2: SSTableReader open Implementation (4-6 hours)
- [ ] Task 4.3: SSTableReader get Implementation (8-10 hours)
- [ ] Task 4.4: SSTableReader Iterator (6-8 hours)
- [ ] Task 4.5: Unit Tests for SSTable Reader (4-6 hours)

**Phase 4: Compaction** (30-40 hours)
- [ ] Task 5.1: Compaction Strategy (4-6 hours)
- [ ] Task 5.2: K-Way Merge Algorithm (10-12 hours)
- [ ] Task 5.3: Garbage Collection (MGA) (8-10 hours)
- [ ] Task 5.4: Level Management (6-8 hours)
- [ ] Task 5.5: Unit Tests for Compaction (6-8 hours)

**Phase 5: WAL Integration** (15-20 hours)
- [ ] Task 6.1: WAL Entry Format (2-3 hours)
- [ ] Task 6.2: WALWriter Implementation (6-8 hours)
- [ ] Task 6.3: WAL Recovery (4-6 hours)
- [ ] Task 6.4: Unit Tests for WAL (3-4 hours)

**Phase 6: Bloom Filter** (10-15 hours)
- [ ] Task 7.1: BloomFilter Class (4-6 hours)
- [ ] Task 7.2: Unit Tests for Bloom Filter (4-6 hours)

**Phase 7: LSMTreeIndex Integration** (20-30 hours)
- [ ] Task 8.1: LSMTreeIndex Class (6-8 hours)
- [ ] Task 8.2: Put/Get/Remove Implementation (8-10 hours)
- [ ] Task 8.3: Range Scan Implementation (8-10 hours)
- [ ] Task 8.4: Memtable Flush (4-6 hours)
- [ ] Task 8.5: Integration Tests (4-6 hours)

**Phase 8: Testing & Optimization** (20-30 hours)
- [ ] Task 9.1: Performance Benchmarks (8-10 hours)
- [ ] Task 9.2: Stress Testing (6-8 hours)
- [ ] Task 9.3: Memory Profiling (4-6 hours)
- [ ] Task 9.4: Documentation (2-4 hours)

### 10.2 Estimated Total Effort

| Phase | Minimum | Maximum |
|-------|---------|---------|
| Phase 1: Memtable | 20 hours | 30 hours |
| Phase 2: SSTable Writer | 20 hours | 30 hours |
| Phase 3: SSTable Reader | 20 hours | 30 hours |
| Phase 4: Compaction | 30 hours | 40 hours |
| Phase 5: WAL Integration | 15 hours | 20 hours |
| Phase 6: Bloom Filter | 10 hours | 15 hours |
| Phase 7: LSMTreeIndex Integration | 20 hours | 30 hours |
| Phase 8: Testing & Optimization | 20 hours | 30 hours |
| **TOTAL** | **155 hours** | **225 hours** |

**Realistic Estimate**: 100-140 hours (using existing infrastructure like BufferPool, PageManager, TransactionManager).

---

## 11. Risk Mitigation

### 11.1 Technical Risks

**Risk 1: MGA Contamination (PostgreSQL MVCC creep)**
- **Mitigation**: Re-read `/MGA_RULES.md` before ANY visibility code
- **Detection**: Grep for `Snapshot` in code (should be ZERO occurrences)
- **Fix**: Replace `Snapshot` with `TransactionId` (uint64_t)

**Risk 2: Write Amplification Higher Than Expected**
- **Mitigation**: Benchmark compaction on real-world workloads
- **Detection**: Measure write amplification in benchmarks
- **Fix**: Tune compaction strategy (leveled vs tiered)

**Risk 3: Bloom Filter False Positive Rate Too High**
- **Mitigation**: Use 10 bits/key (1% false positive rate)
- **Detection**: Measure false positive rate in unit tests
- **Fix**: Increase bits/key (trade space for accuracy)

**Risk 4: Compaction Slows Down Writes**
- **Mitigation**: Background thread pool for compaction
- **Detection**: Monitor write latency during compaction
- **Fix**: Throttle compaction or increase memtable size

**Risk 5: Context Loss During Compaction**
- **Mitigation**: Prominent references to `/MGA_RULES.md` in code comments
- **Detection**: Check for Snapshot usage after compaction
- **Fix**: Re-read `/MGA_RULES.md` and correct violations

### 11.2 Schedule Risks

**Risk 1: Underestimated Effort**
- **Mitigation**: Track actual hours per task, adjust estimates
- **Detection**: Compare actual vs estimated hours weekly
- **Fix**: Reduce scope or extend timeline

**Risk 2: Blocked on Dependencies**
- **Mitigation**: Verify TransactionManager, PageManager APIs work as expected
- **Detection**: Build fails or tests fail due to missing functionality
- **Fix**: Implement missing dependencies or stub temporarily

---

## 12. References

**Specifications**:
- `/docs/specifications/LSM_TREE_SPEC.md` (complete technical specification)

**MGA Compliance** (CRITICAL - read first!):
- `/MGA_RULES.md` (Firebird MGA rules)

**Master Plan**:
- `/docs/planning/ALPHA_PHASE1_COMPLETE_IMPLEMENTATION_PLAN.md`

**External References**:
- [LevelDB Documentation](https://github.com/google/leveldb/blob/main/doc/index.md)
- [RocksDB Wiki](https://github.com/facebook/rocksdb/wiki)
- [The Log-Structured Merge-Tree (LSM-Tree)](https://www.cs.umb.edu/~poneil/lsmtree.pdf)

---

## 13. Conclusion

This plan provides a **complete task-by-task roadmap** for implementing LSM-Tree with full Firebird MGA compliance.

**Key Takeaways**:
- 8 phases, 155-225 hours total effort (100-140 hours realistic)
- Each phase has clear tasks, acceptance criteria, and test requirements
- MGA compliance checked at every phase (NO PostgreSQL contamination)
- References to `/MGA_RULES.md` to prevent context loss
- 100% implementation (NO stubs, NO deferrals)

**Next Steps**:
1. Start Phase 1: Memtable implementation (Task 2.1)
2. Continuously reference `/MGA_RULES.md` for visibility rules
3. Track progress in this document (check off tasks as completed)
4. Update master plan when complete (100% - ALL 12 INDEXES COMPLETE!)

**Status**: PLANNING COMPLETE ✅
**Implementation**: PENDING (100-140 hours)

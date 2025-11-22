# LSM-Tree Index Completion Plan

**Date:** November 22, 2025
**Branch:** claude/fix-index-architecture-016kV6ymGNbWtJdWE4v7iicd
**Status:** DRAFT - Ready for Review

---

## Executive Summary

This document provides a comprehensive plan to complete the LSM-Tree index implementation for ScratchBird, ensuring:
1. Full LSM-Tree functionality per specification
2. API compatibility with existing executor index calls
3. MGA compliance (Firebird architecture)
4. Production readiness with all necessary optimizations

### Current Status

**Two implementations exist:**
1. **LSMTree** (`lsm_tree.cpp`, `lsm_tree.h`) - Simple in-memory only (70% complete)
   - Used by executor via `getOrOpenIndex<LSMTree>()`
   - No SSTable persistence
   - No WAL durability
   - Manual compaction only

2. **LSMTreeIndex** (`lsm_tree_index.cpp`, `lsm_tree_components.cpp`) - Full implementation (85% complete)
   - Has Memtable, SSTableWriter/Reader, CompactionManager
   - 4-level tiering (Level 0-3)
   - Background compaction thread
   - NOT used by executor currently

### Specification Requirements

Per `/docs/specifications/LSM_TREE_SPEC.md` and `/docs/specifications/LSM_TREE_ARCHITECTURE.md`:
- ✅ Memtable (Red-Black Tree / std::map)
- ✅ SSTable format with index blocks
- ⚠️ Bloom filters (partially implemented)
- ❌ WAL (Write-Ahead Log) for durability
- ✅ Leveled compaction (4 levels)
- ✅ MGA compliance (xmin/xmax, TIP-based visibility)
- ✅ K-way merge for range scans
- ❌ Compression (Snappy/Zstd)
- ❌ Parallel compaction

---

## Part 1: Implementation Gaps Analysis

### 1.1 Missing Components (Critical)

| Component | Status | Impact | Priority |
|-----------|--------|--------|----------|
| **WAL (Write-Ahead Log)** | ❌ Not implemented | NO crash recovery, data loss | **P0 - CRITICAL** |
| **Bloom Filters** | ⚠️ Stubbed but not functional | Poor read performance, excessive I/O | **P1 - HIGH** |
| **Compression** | ❌ Not implemented | Large disk usage, slow I/O | **P2 - MEDIUM** |
| **LSMCompactionManager** | ⚠️ Partially implemented | Basic compaction works, no optimization | **P2 - MEDIUM** |

### 1.2 API Compatibility Gaps

**Current executor signatures (from executor.cpp:20473-20484):**
```cpp
// INSERT operation
auto lsm = getOrOpenIndex<core::LSMTreeIndex>(index_uuid, type, index_info.root_page, ctx);
lsm->put(key, value, current_xid, ctx);  // value = serialized TID

// SEARCH operation
lsm->get(key, current_xid, &value, &found, ctx);  // Returns value (TID)

// DELETE operation
lsm->remove(key, current_xid, ctx);  // Inserts tombstone
```

**LSMTreeIndex current signatures (from lsm_tree_index.h):**
```cpp
Status put(const std::vector<uint8_t> &key,
           const std::vector<uint8_t> &value,
           uint64_t xid,
           ErrorContext *ctx = nullptr);  ✅ COMPATIBLE

Status get(const std::vector<uint8_t> &key,
           uint64_t xid,
           std::vector<uint8_t> *value_out,
           bool *found,
           ErrorContext *ctx = nullptr);  ✅ COMPATIBLE

Status remove(const std::vector<uint8_t> &key,
              uint64_t xid,
              ErrorContext *ctx = nullptr);  ✅ COMPATIBLE
```

**✅ RESULT:** LSMTreeIndex API is ALREADY COMPATIBLE with executor! No changes needed.

### 1.3 Simple LSMTree vs Full LSMTreeIndex

**LSMTree (simple):**
- ✅ In-memory only (std::map)
- ✅ MGA compliant (xmin/xmax)
- ✅ TIP-based visibility
- ✅ Range scans with iterator
- ❌ No persistence
- ❌ No crash recovery
- ❌ Limited scalability

**LSMTreeIndex (full):**
- ✅ All features of LSMTree +
- ✅ SSTable persistence
- ✅ 4-level tiering
- ✅ Background compaction
- ✅ K-way merge for scans
- ⚠️ Missing WAL
- ⚠️ Missing Bloom filters
- ✅ API compatible with executor

**Recommendation:** Complete LSMTreeIndex and deprecate simple LSMTree.

---

## Part 2: Implementation Plan

### Phase 1: Complete LSMTreeIndex Core (Priority 0-1)

**Goal:** Make LSMTreeIndex production-ready with durability and performance optimizations.

#### Task 1.1: Implement WAL (Write-Ahead Log) [P0 - CRITICAL]

**Estimated effort:** 12-16 hours

**Files to create/modify:**
- `include/scratchbird/core/lsm_wal.h` (new)
- `src/core/lsm_wal.cpp` (new)
- `src/core/lsm_tree_index.cpp` (modify)

**Implementation:**

1. **WAL Entry Format** (per spec LSM_TREE_SPEC.md:1048-1061):
```cpp
struct WALEntry {
    uint32_t entry_size;           // Total size
    uint64_t sequence_number;      // Monotonic sequence
    uint8_t entry_type;            // 0 = Put, 1 = Delete
    uint64_t xmin;                 // Transaction ID
    uint64_t xmax;                 // For deletes
    uint16_t key_len;              // Key length
    uint8_t key[key_len];          // Variable-length key
    uint32_t value_len;            // Value length
    uint8_t value[value_len];      // Variable-length value
    uint32_t checksum;             // CRC32
};
```

2. **WAL Writer Class:**
```cpp
class WALWriter {
public:
    WALWriter(const std::string &wal_path);
    Status open(ErrorContext *ctx);
    Status append(uint8_t entry_type, const std::vector<uint8_t> &key,
                  const std::vector<uint8_t> &value, uint64_t xmin,
                  uint64_t xmax, uint64_t seq, ErrorContext *ctx);
    Status sync(ErrorContext *ctx);  // fsync to disk
    Status truncate(ErrorContext *ctx);  // After flush
    Status close(ErrorContext *ctx);
};
```

3. **WAL Reader for Recovery:**
```cpp
class WALReader {
public:
    Status open(const std::string &wal_path, ErrorContext *ctx);
    Status readEntry(WALEntry *entry_out, ErrorContext *ctx);
    bool isEndOfFile();
};
```

4. **Integration with LSMTreeIndex:**
   - Add `std::unique_ptr<WALWriter> wal_writer_` member
   - Modify `put()` to append to WAL BEFORE memtable
   - Add `recoverFromWAL()` method called from `open()`
   - Truncate WAL after memtable flush

**Acceptance criteria:**
- ✅ All writes logged to WAL before memtable
- ✅ WAL fsync'd to disk
- ✅ Recovery from WAL on startup
- ✅ WAL truncated after successful flush
- ✅ Crash recovery test passes (kill process, restart, verify data)

#### Task 1.2: Implement Bloom Filters [P1 - HIGH]

**Estimated effort:** 8-12 hours

**Files to create/modify:**
- `include/scratchbird/core/lsm_bloom_filter.h` (new)
- `src/core/lsm_bloom_filter.cpp` (new)
- `src/core/lsm_tree_components.cpp` (modify SSTableWriter/Reader)

**Implementation:**

1. **Bloom Filter Class** (per spec LSM_TREE_SPEC.md:800-839):
```cpp
class BloomFilter {
public:
    BloomFilter(size_t estimated_num_keys, double false_positive_rate = 0.01);

    void add(const std::vector<uint8_t> &key);
    bool mightContain(const std::vector<uint8_t> &key) const;

    void serialize(std::vector<uint8_t> *out) const;
    static std::unique_ptr<BloomFilter> deserialize(const std::vector<uint8_t> &data);

private:
    size_t num_keys_;
    size_t num_bits_;
    size_t num_hashes_;
    std::vector<uint8_t> bits_;  // Bit array

    uint64_t hash(const std::vector<uint8_t> &key, size_t seed) const;
};
```

2. **Hash Function:**
   - Use MurmurHash3 or FNV-1a (spec shows FNV-1a example)
   - Multiple hash functions via seed variation

3. **SSTable Integration:**
   - Add `BloomFilter bloom_filter_` to SSTableWriter
   - Call `bloom_filter_.add(key)` for each entry
   - Serialize Bloom filter to SSTable footer
   - Add `BloomFilter bloom_filter_` to SSTableReader
   - Check `bloom_filter_.mightContain(key)` before disk I/O in `get()`

4. **Configuration:**
   - Default: 10 bits per key, ~1% false positive rate
   - Configurable via LSMTreeIndex constructor parameter

**Acceptance criteria:**
- ✅ Bloom filters created during SSTable write
- ✅ Bloom filters loaded during SSTable open
- ✅ `get()` checks Bloom filter before disk read
- ✅ False positive rate measured at ~1%
- ✅ Read performance improved by 90%+ for non-existent keys

#### Task 1.3: Complete LSMCompactionManager [P2 - MEDIUM]

**Estimated effort:** 10-15 hours

**Files to modify:**
- `src/core/lsm_tree_index.cpp`
- `include/scratchbird/core/lsm_tree_index.h`

**Current gaps:**
```cpp
// MISSING from lsm_tree_components.cpp:
Status LSMCompactionManager::executeCompaction(const CompactionTask &task, ErrorContext *ctx)
{
    // TODO: Implement k-way merge
    // TODO: Implement garbage collection based on OIT
    // TODO: Implement atomic SSTable replacement
}
```

**Implementation:**

1. **K-way Merge Algorithm** (per spec LSM_TREE_SPEC.md:927-1010):
```cpp
Status LSMCompactionManager::kWayMerge(
    const std::vector<std::string> &input_sstables,
    const std::string &output_sstable,
    uint64_t oit,
    ErrorContext *ctx)
{
    // 1. Open all input SSTables
    std::vector<std::unique_ptr<SSTableReader>> readers;
    for (const auto &path : input_sstables) {
        auto reader = std::make_unique<SSTableReader>(path);
        reader->open(ctx);
        readers.push_back(std::move(reader));
    }

    // 2. Create output SSTable writer
    SSTableWriter writer(output_sstable, 4096);
    writer.open(ctx);

    // 3. Priority queue for k-way merge
    std::priority_queue<MergeEntry, std::vector<MergeEntry>, MergeEntryComparator> pq;

    // Initialize queue with first entry from each SSTable
    // ...

    // 4. Merge loop
    std::vector<uint8_t> last_key;
    while (!pq.empty()) {
        MergeEntry entry = pq.top();
        pq.pop();

        // Skip duplicates (keep newest version)
        if (!last_key.empty() && entry.key == last_key) {
            continue;
        }

        // Garbage collection: remove entries invisible to all transactions
        if (canGarbageCollect(entry, oit, txn_mgr_)) {
            continue;
        }

        // Write to output SSTable
        writer.addEntry(entry.key, entry.value, entry.sequence,
                       entry.type, entry.xmin, entry.xmax, ctx);
        last_key = entry.key;

        // Advance iterator and add next entry to queue
        // ...
    }

    writer.finish(ctx);
    return Status::OK;
}
```

2. **Garbage Collection Logic:**
```cpp
bool canGarbageCollect(const SSTableEntry &entry,
                       uint64_t oldest_active_xid,
                       TransactionManager *txn_mgr)
{
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

3. **Atomic SSTable Replacement:**
   - Write new SSTables to temp names
   - Update metadata atomically
   - Delete old SSTables after successful replacement

4. **Compaction Strategy Tuning:**
   - Level 0: Compact when 8+ SSTables
   - Level 1: Compact when size > 40MB
   - Level 2: Compact when size > 400MB
   - Level 3: No upper limit

**Acceptance criteria:**
- ✅ K-way merge produces sorted output
- ✅ Duplicate keys removed (newest kept)
- ✅ Garbage collection removes old versions
- ✅ Compaction runs automatically in background
- ✅ No data loss during compaction
- ✅ Space amplification < 30%

---

### Phase 2: Optional Enhancements (Priority 3)

#### Task 2.1: Compression Support [P3 - LOW]

**Estimated effort:** 6-8 hours

**Options:**
- Snappy (fast, moderate compression)
- Zstd (slower, better compression)
- LZ4 (fastest, least compression)

**Implementation:**
- Add compression parameter to SSTableWriter
- Compress data blocks before writing
- Decompress blocks during read
- Store compression type in SSTable footer

#### Task 2.2: Parallel Compaction [P3 - LOW]

**Estimated effort:** 8-12 hours

**Implementation:**
- Use thread pool for compaction tasks
- Allow multiple levels to compact simultaneously
- Coordinate to avoid resource conflicts
- Monitor compaction backlog

#### Task 2.3: Block Cache [P3 - LOW]

**Estimated effort:** 6-10 hours

**Implementation:**
- LRU cache for SSTable data blocks
- Reduce repeated disk reads
- Configurable cache size
- Integration with buffer pool

---

### Phase 3: Executor Integration & Testing

#### Task 3.1: Update Executor to Use LSMTreeIndex [P1 - HIGH]

**Estimated effort:** 2-4 hours

**Current code (executor.cpp:20471-20486):**
```cpp
case IndexType::LSM:
{
    auto lsm = getOrOpenIndex<core::LSMTreeIndex>(index_uuid, type, index_info.root_page, ctx);
    if (lsm)
    {
        // Serialize TID to byte vector (page_num + slot_num)
        std::vector<uint8_t> value(sizeof(uint32_t) * 2);
        uint32_t page = static_cast<uint32_t>(core::getPageNumber(tid.gpid));
        uint32_t slot = tid.slot_num;
        std::memcpy(value.data(), &page, sizeof(page));
        std::memcpy(value.data() + sizeof(page), &slot, sizeof(slot));
        return lsm->put(key, value, xmin, ctx);
    }
    return core::Status::INTERNAL_ERROR;
}
```

**Changes needed:**
- ✅ None - already using LSMTreeIndex!
- Verify `LSMTreeIndex::open()` static factory works correctly
- Test with actual workload

#### Task 3.2: Deprecate Simple LSMTree [P2 - MEDIUM]

**Estimated effort:** 1-2 hours

**Steps:**
1. Add deprecation notice to `lsm_tree.h`
2. Update documentation to recommend LSMTreeIndex
3. Keep simple LSMTree for backward compatibility
4. Remove in future release

#### Task 3.3: Comprehensive Testing [P0 - CRITICAL]

**Estimated effort:** 12-16 hours

**Test suites to create:**

1. **Unit Tests:**
   - `test_lsm_memtable.cpp` - Memtable operations
   - `test_lsm_sstable.cpp` - SSTable read/write
   - `test_lsm_bloom_filter.cpp` - Bloom filter accuracy
   - `test_lsm_wal.cpp` - WAL write/recovery
   - `test_lsm_compaction.cpp` - Compaction correctness

2. **Integration Tests:**
   - `test_lsm_tree_integration.cpp` - End-to-end operations
   - Insert 100K entries, verify all readable
   - Range scans across memtable and SSTables
   - MGA isolation (concurrent transactions)
   - Crash recovery (kill process, restart, verify data)

3. **Performance Tests:**
   - `benchmark_lsm_tree.cpp` - Throughput and latency
   - Sequential inserts: target 100K ops/sec
   - Random inserts: target 50K ops/sec
   - Point queries: target 10K ops/sec (with Bloom filters)
   - Range scans: target 1K scans/sec (1K rows each)

4. **Stress Tests:**
   - `stress_test_lsm_tree.cpp` - Reliability
   - Insert 1M entries
   - 100 concurrent threads (readers + writers)
   - Simulate crashes during compaction
   - Verify no data loss or corruption

**Acceptance criteria:**
- ✅ All unit tests pass
- ✅ All integration tests pass
- ✅ Performance targets met
- ✅ Stress tests show no data loss
- ✅ Crash recovery works correctly

---

## Part 3: API Compatibility Matrix

### 3.1 Index Interface Comparison

**Standard index signatures (from other index types):**

```cpp
// B-Tree
Status insert(const std::vector<uint8_t> &key, const TID &tid,
              uint64_t xmin, ErrorContext *ctx);
Status search(const std::vector<uint8_t> &key, uint64_t current_xid,
              std::vector<TID> *results_out, ErrorContext *ctx);
Status markDeleted(const std::vector<uint8_t> &key, const TID &tid,
                   uint64_t xmax, ErrorContext *ctx);

// Hash Index
Status insert(const void *key_data, size_t key_size, const TID &tid,
              uint64_t xmin, ErrorContext *ctx);
Status find(const void *key_data, size_t key_size, uint64_t current_xid,
            std::vector<TID> *results_out, ErrorContext *ctx);

// LSMTreeIndex (different pattern - stores value, not just TID)
Status put(const std::vector<uint8_t> &key,
           const std::vector<uint8_t> &value,  // ← Value is serialized TID
           uint64_t xid, ErrorContext *ctx);
Status get(const std::vector<uint8_t> &key, uint64_t xid,
           std::vector<uint8_t> *value_out, bool *found, ErrorContext *ctx);
Status remove(const std::vector<uint8_t> &key, uint64_t xid, ErrorContext *ctx);
```

**Why LSMTreeIndex is different:**
- LSM-Tree is a key-value store, not just an index
- Stores arbitrary values (in this case, serialized TIDs)
- Follows RocksDB/LevelDB semantics (put/get/remove)
- Executor handles serialization/deserialization

**✅ This is correct! No changes needed.**

### 3.2 Range Scan Interface

**B-Tree range scan:**
```cpp
std::unique_ptr<Iterator> rangeScan(const std::vector<uint8_t> *start_key,
                                    const std::vector<uint8_t> *end_key,
                                    uint64_t current_xid,
                                    bool start_inclusive, bool end_inclusive,
                                    ErrorContext *ctx);
```

**LSMTreeIndex range scan:**
```cpp
Status scan(const std::vector<uint8_t> &start_key,
            const std::vector<uint8_t> &end_key,
            uint64_t xid,
            std::vector<MemtableEntry> *entries_out,
            ErrorContext *ctx);
```

**Difference:** LSMTreeIndex returns all results at once, B-Tree uses iterator.

**Recommendation:** Add iterator-based scan to LSMTreeIndex for large result sets.

---

## Part 4: MGA Compliance Verification

### 4.1 MGA Requirements Checklist

Per `/MGA_RULES.md`:

- ✅ **Rule 1:** No snapshots (uses TransactionId, not Snapshot)
- ✅ **Rule 2:** TIP-based visibility (`txn_mgr->isVersionVisible(xmin, current_xid)`)
- ✅ **Rule 5:** Back-versioning NOT forward-versioning (LSM doesn't update in place)
- ✅ **Rule 11:** Correct signatures (`TransactionId current_xid`, not `Snapshot*`)
- ✅ **Section 5:** xmin/xmax on all entries (MemtableEntry, SSTableEntry)

**Potential Issue:** LSM-Tree doesn't use back-versioning (it's append-only), but this is **acceptable** because:
- LSM-Tree is inherently multi-versioned (keeps all versions until compaction)
- Uses TIP for visibility (MGA compliant)
- Garbage collection based on OIT (MGA compliant)
- No forward pointers (entries are immutable)

**Verdict:** ✅ **MGA COMPLIANT**

### 4.2 Visibility Check Implementation

**Current code (lsm_tree_components.cpp:123-144):**
```cpp
Status Memtable::get(const std::vector<uint8_t> &key,
                     uint64_t current_xid,
                     TransactionManager *txn_mgr,
                     std::vector<uint8_t> *value_out,
                     bool *found,
                     ErrorContext *ctx)
{
    // ...search through versions (newest first - reverse iteration)
    for (auto ver_it = versions.rbegin(); ver_it != versions.rend(); ++ver_it)
    {
        const MemtableEntry &entry = *ver_it;

        // MGA visibility check (Firebird TIP-based)
        bool visible = txn_mgr->isVersionVisible(entry.xmin, current_xid);

        if (visible)
        {
            if (entry.entry_type == ENTRY_TYPE_INSERT) {
                *value_out = entry.value;
                *found = true;
                return Status::OK;
            }
            else {  // ENTRY_TYPE_DELETE (tombstone)
                *found = false;
                return Status::OK;
            }
        }
    }
    return Status::OK;
}
```

**✅ CORRECT:** Uses TIP-based visibility, no snapshots.

---

## Part 5: Work Breakdown & Timeline

### Summary Table

| Phase | Task | Priority | Effort | Dependencies |
|-------|------|----------|--------|--------------|
| **Phase 1** | | | | |
| 1.1 | Implement WAL | P0 | 12-16h | None |
| 1.2 | Implement Bloom Filters | P1 | 8-12h | None |
| 1.3 | Complete Compaction Manager | P2 | 10-15h | None |
| **Phase 2** | | | | |
| 2.1 | Compression Support | P3 | 6-8h | None |
| 2.2 | Parallel Compaction | P3 | 8-12h | Task 1.3 |
| 2.3 | Block Cache | P3 | 6-10h | None |
| **Phase 3** | | | | |
| 3.1 | Update Executor (verify) | P1 | 2-4h | Phase 1 complete |
| 3.2 | Deprecate LSMTree | P2 | 1-2h | Task 3.1 |
| 3.3 | Comprehensive Testing | P0 | 12-16h | Phase 1 complete |

### Total Effort Estimates

- **Minimum (P0-P1 only):** 42-56 hours
- **Recommended (P0-P2):** 52-71 hours
- **Maximum (All features):** 72-103 hours

### Recommended Implementation Order

**Week 1 (Critical Path):**
1. Implement WAL (12-16h) - **CRITICAL for durability**
2. Implement Bloom Filters (8-12h) - **HIGH impact on performance**
3. Complete Compaction Manager (10-15h)

**Week 2 (Testing & Integration):**
4. Comprehensive Testing (12-16h)
5. Verify Executor Integration (2-4h)
6. Deprecate Simple LSMTree (1-2h)

**Week 3 (Optional Enhancements):**
7. Compression Support (6-8h)
8. Parallel Compaction (8-12h)
9. Block Cache (6-10h)

---

## Part 6: Testing Strategy

### 6.1 Test Coverage Goals

**Target:** 90%+ code coverage for LSM-Tree components

**Critical paths to test:**
- ✅ Memtable put/get/remove
- ✅ SSTable write/read
- ✅ WAL write/recovery
- ✅ Bloom filter add/query
- ✅ Compaction k-way merge
- ✅ Garbage collection
- ✅ Crash recovery
- ✅ MGA visibility filtering
- ✅ Range scans
- ✅ Concurrent access

### 6.2 Performance Benchmarks

**Target performance (per spec LSM_TREE_SPEC.md:1365-1375):**

| Workload | Target | Notes |
|----------|--------|-------|
| Sequential inserts | 100K-500K ops/sec | Batch writes, WAL buffered |
| Random inserts | 50K-200K ops/sec | Memtable Red-Black Tree |
| Point queries | 10K-50K ops/sec | With Bloom filters |
| Range scans (1K rows) | 1K-10K ops/sec | K-way merge overhead |
| Mixed (50% read, 50% write) | 50K-100K ops/sec | Depends on compaction |

### 6.3 Stress Test Scenarios

1. **High Write Volume:**
   - Insert 1M entries as fast as possible
   - Verify no data loss
   - Measure write amplification

2. **Concurrent Access:**
   - 100 threads (50 readers, 50 writers)
   - Run for 60 seconds
   - Verify isolation and correctness

3. **Crash Recovery:**
   - Insert 100K entries
   - Kill process randomly during writes
   - Restart and verify all committed writes present

4. **Compaction Under Load:**
   - Continuous writes while compaction runs
   - Verify no deadlocks
   - Verify data integrity after compaction

---

## Part 7: Documentation Updates

### 7.1 Files to Update

1. **`/docs/specifications/LSM_TREE_ARCHITECTURE.md`**
   - Update status from "85% complete" to "100% complete"
   - Update known issues section
   - Add WAL section
   - Add Bloom filter section

2. **`/docs/specifications/LSM_TREE_SPEC.md`**
   - Mark all components as implemented
   - Update implementation breakdown section

3. **`/docs/specifications/INDEX_IMPLEMENTATION_SPEC.md`**
   - Change LSM-Tree status from "MEDIUM PRIORITY" to "COMPLETE"
   - Update implementation status

4. **`/PROJECT_CONTEXT.md`**
   - Update index count from "4/12" to "5/12" (with LSM-Tree)
   - Update completion percentage

5. **`README.md` or `CHANGELOG.md`**
   - Add release notes for LSM-Tree completion

### 7.2 Code Documentation

**Add comprehensive comments to:**
- LSMTreeIndex class (architecture overview)
- WAL implementation (recovery procedure)
- Bloom filter (hash functions, false positive rate)
- Compaction manager (strategy, tuning parameters)

---

## Part 8: Risks & Mitigation

### 8.1 Technical Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| WAL implementation bugs cause data loss | HIGH | MEDIUM | Comprehensive crash recovery tests |
| Bloom filter false positive rate too high | MEDIUM | LOW | Benchmark and tune bits-per-key parameter |
| Compaction falls behind on writes | HIGH | MEDIUM | Monitor compaction backlog, tune thresholds |
| API incompatibility with executor | HIGH | LOW | Already verified compatible |
| MGA compliance violations | CRITICAL | LOW | Code review against MGA_RULES.md |

### 8.2 Performance Risks

| Risk | Mitigation |
|------|------------|
| Write amplification too high (>30x) | Tune compaction strategy, consider tiered compaction |
| Read amplification too high (>20x) | Add Bloom filters (reduces by 90%+) |
| Space amplification too high (>50%) | More aggressive compaction, better GC |
| Memtable flush blocks writes | Implement double-buffering (two memtables) |

---

## Part 9: Success Criteria

### Minimum Viable Product (MVP)

- ✅ WAL implemented and tested
- ✅ Bloom filters implemented and tested
- ✅ Compaction manager complete
- ✅ All unit tests pass
- ✅ Integration tests pass
- ✅ Crash recovery test passes
- ✅ MGA compliance verified
- ✅ Executor integration verified
- ✅ Documentation updated

### Production Ready

- ✅ All MVP criteria +
- ✅ Performance benchmarks meet targets
- ✅ Stress tests pass (1M entries, 100 threads)
- ✅ 90%+ code coverage
- ✅ No known critical bugs
- ✅ Compression support (optional but recommended)

---

## Part 10: Next Steps

### Immediate Actions

1. **Review this plan** with stakeholders
2. **Prioritize features** based on project needs
3. **Assign tasks** to developers
4. **Set up test infrastructure** (benchmark harness, stress test framework)
5. **Create tracking issues** for each task

### Open Questions

1. **Should we implement compression immediately or defer to Phase 2?**
   - Recommendation: Defer to Phase 2 unless disk space is a concern

2. **Should we keep simple LSMTree for backward compatibility?**
   - Recommendation: Keep for 1-2 releases, then deprecate

3. **What Bloom filter false positive rate should we target?**
   - Recommendation: Start with 1% (10 bits/key), tune based on workload

4. **Should parallel compaction be P2 or P3?**
   - Recommendation: P3 unless write-heavy workload shows compaction backlog

---

## Appendix A: Reference Implementation Links

**LevelDB:**
- https://github.com/google/leveldb
- Memtable: `db/memtable.cc`
- SSTable: `table/table_builder.cc`, `table/table.cc`
- Compaction: `db/db_impl.cc` (CompactMemTable, BackgroundCompaction)

**RocksDB:**
- https://github.com/facebook/rocksdb
- LSM implementation: `db/memtable.cc`, `table/block_based/block_based_table_builder.cc`
- Compaction: `db/compaction/`

**Firebird MGA:**
- `/MGA_RULES.md` (this project)
- Transaction Inventory Pages (TIP): Firebird codebase `src/jrd/tra.cpp`

---

## Appendix B: File Checklist

**Files to create:**
- [ ] `include/scratchbird/core/lsm_wal.h`
- [ ] `src/core/lsm_wal.cpp`
- [ ] `include/scratchbird/core/lsm_bloom_filter.h`
- [ ] `src/core/lsm_bloom_filter.cpp`
- [ ] `tests/unit/test_lsm_memtable.cpp`
- [ ] `tests/unit/test_lsm_sstable.cpp`
- [ ] `tests/unit/test_lsm_bloom_filter.cpp`
- [ ] `tests/unit/test_lsm_wal.cpp`
- [ ] `tests/unit/test_lsm_compaction.cpp`
- [ ] `tests/integration/test_lsm_tree_integration.cpp`
- [ ] `tests/performance/benchmark_lsm_tree.cpp`
- [ ] `tests/stress/stress_test_lsm_tree.cpp`

**Files to modify:**
- [ ] `src/core/lsm_tree_index.cpp` (add WAL, Bloom filters)
- [ ] `src/core/lsm_tree_components.cpp` (add Bloom to SSTable)
- [ ] `include/scratchbird/core/lsm_tree_index.h` (add WAL/Bloom members)
- [ ] `src/sblr/executor.cpp` (verify LSMTreeIndex usage)
- [ ] `docs/specifications/LSM_TREE_ARCHITECTURE.md` (update status)
- [ ] `docs/specifications/LSM_TREE_SPEC.md` (mark complete)
- [ ] `PROJECT_CONTEXT.md` (update progress)

---

**End of Plan**

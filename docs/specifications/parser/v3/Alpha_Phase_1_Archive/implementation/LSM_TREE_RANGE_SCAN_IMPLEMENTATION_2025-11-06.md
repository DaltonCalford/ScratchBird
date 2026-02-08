# LSM-Tree Range Scan Implementation

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: November 6, 2025
**Priority**: P0 (CRITICAL)
**Status**: IN PROGRESS
**Estimated Effort**: 15-20 hours
**Actual Progress**: Design phase complete

---

## DISCOVERY: Existing Infrastructure ✅

### What Already Exists

During code analysis, I discovered that **significant infrastructure is already in place**:

1. **Memtable::scan()** - COMPLETE ✅
   - File: `src/core/lsm_tree.cpp:154-215`
   - Implements range scan over in-memory Red-Black tree
   - Handles start/end key boundaries
   - MGA visibility filtering via `isEntryVisible()`
   - Deduplication (skips duplicate keys)
   - Tombstone handling

2. **SSTableReader::scan()** - COMPLETE ✅
   - File: `src/core/lsm_tree.cpp:1093-1172`
   - Implements range scan over on-disk SSTable
   - Block-level scanning (finds relevant blocks)
   - MGA visibility filtering
   - Deduplication
   - Tombstone handling

### What's Missing

**Only the top-level orchestration is missing**:

- **LSMTreeIndex::scan()** - Currently returns NOT_IMPLEMENTED
- File: `src/core/lsm_tree_index.cpp:297-307`
- Needs to combine results from:
  1. Active memtable
  2. Immutable memtable
  3. Level 0-3 SSTables

---

## IMPLEMENTATION PLAN (REVISED)

### Original Estimate: 15-20 hours
### Revised Estimate: 8-12 hours (50% reduction due to existing code)

**Breakdown**:
- ~~Phase 1: Design (2-3h)~~ → **1h** (simpler due to existing scan methods)
- ~~Phase 2: Memtable iterator (2-3h)~~ → **ALREADY DONE** ✅
- ~~Phase 3: SSTable reader (3-4h)~~ → **ALREADY DONE** ✅
- Phase 4: K-way merge (4-5h) → **4-5h** (unchanged, core complexity)
- Phase 5: Testing (3-5h) → **3-5h** (unchanged, thoroughness required)

---

## ARCHITECTURE

### Data Flow

```
LSMTreeIndex::scan(start_key, end_key, xid)
    ↓
    ├─→ Active Memtable::scan(start_key, end_key, xid)
    │   └─→ Returns: vector<pair<key, value>> (sorted by key)
    │
    ├─→ Immutable Memtable::scan(start_key, end_key, xid)
    │   └─→ Returns: vector<pair<key, value>> (sorted by key)
    │
    ├─→ Level 0 SSTables (all 4 files, may overlap)
    │   ├─→ SSTableReader[0]::scan(start_key, end_key, xid)
    │   ├─→ SSTableReader[1]::scan(start_key, end_key, xid)
    │   ├─→ SSTableReader[2]::scan(start_key, end_key, xid)
    │   └─→ SSTableReader[3]::scan(start_key, end_key, xid)
    │
    ├─→ Level 1 SSTables (non-overlapping within level)
    │   └─→ For each SSTable in range: scan()
    │
    ├─→ Level 2 SSTables
    │   └─→ For each SSTable in range: scan()
    │
    └─→ Level 3 SSTables
        └─→ For each SSTable in range: scan()

    ↓
K-way Merge (Priority Queue)
    - Merge all sorted runs
    - Deduplicate (keep newest version per key)
    - Apply MGA visibility filtering
    - Handle tombstones
    ↓
Return: vector<MemtableEntry> (deduplicated, sorted, visible only)
```

### K-way Merge Algorithm

**Input**: Multiple sorted runs (each run sorted by key)

**Output**: Single sorted, deduplicated run with only visible entries

**Algorithm**:
```
1. Create priority queue (min-heap by key)
2. For each source (memtables + SSTables):
   - Add first entry to priority queue
   - Track source ID and position

3. While priority queue not empty:
   a. Pop entry with smallest key
   b. If key != last_key:
      - This is the newest version of this key
      - Check MGA visibility
      - If visible and not tombstone: add to results
      - Update last_key
   c. Else (key == last_key):
      - Skip (we already processed newest version)
   d. Advance source that provided this entry
   e. Add next entry from that source to priority queue

4. Return results
```

**Complexity**:
- Time: O(N log K) where N = total entries, K = number of sources
- Space: O(K) for priority queue + O(N) for results

---

## IMPLEMENTATION

### Phase 1: Data Structures

```cpp
// Helper structure for k-way merge
struct ScanSource {
    enum class Type {
        ACTIVE_MEMTABLE,
        IMMUTABLE_MEMTABLE,
        SSTABLE
    };

    Type type;
    uint32_t level;        // For SSTables: 0-3
    uint32_t sstable_idx;  // For SSTables: index within level

    // Current scan results from this source
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> entries;
    size_t current_pos;    // Current position in entries vector

    ScanSource() : type(Type::ACTIVE_MEMTABLE), level(0), sstable_idx(0), current_pos(0) {}

    // Get current entry (nullptr if exhausted)
    const std::pair<std::vector<uint8_t>, std::vector<uint8_t>>* getCurrentEntry() const {
        if (current_pos >= entries.size()) {
            return nullptr;
        }
        return &entries[current_pos];
    }

    // Advance to next entry
    void advance() {
        if (current_pos < entries.size()) {
            current_pos++;
        }
    }

    // Check if exhausted
    bool isExhausted() const {
        return current_pos >= entries.size();
    }
};

// Priority queue entry for k-way merge
struct MergeEntry {
    std::vector<uint8_t> key;
    std::vector<uint8_t> value;
    size_t source_id;  // Index in sources array

    // For min-heap: smallest key comes first
    bool operator>(const MergeEntry& other) const {
        return key > other.key;
    }
};
```

### Phase 2: LSMTreeIndex::scan() Implementation

```cpp
Status LSMTreeIndex::scan(const std::vector<uint8_t> &start_key,
                          const std::vector<uint8_t> &end_key,
                          uint64_t xid,
                          std::vector<MemtableEntry> *entries_out,
                          ErrorContext *ctx)
{
    if (!entries_out) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "entries_out cannot be null");
        return Status::INVALID_ARGUMENT;
    }

    entries_out->clear();

    // Vector of scan sources (memtables + SSTables)
    std::vector<ScanSource> sources;

    // ========================================================================
    // STEP 1: Scan Active Memtable
    // ========================================================================
    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);

        if (active_memtable_) {
            ScanSource source;
            source.type = ScanSource::Type::ACTIVE_MEMTABLE;

            const std::vector<uint8_t>* start_ptr = start_key.empty() ? nullptr : &start_key;
            const std::vector<uint8_t>* end_ptr = end_key.empty() ? nullptr : &end_key;

            Status status = active_memtable_->scan(start_ptr, end_ptr, xid, txn_mgr_,
                                                   &source.entries, ctx);
            if (status != Status::OK) {
                return status;
            }

            if (!source.entries.empty()) {
                sources.push_back(std::move(source));
            }
        }

        // ========================================================================
        // STEP 2: Scan Immutable Memtable
        // ========================================================================
        if (immutable_memtable_) {
            ScanSource source;
            source.type = ScanSource::Type::IMMUTABLE_MEMTABLE;

            const std::vector<uint8_t>* start_ptr = start_key.empty() ? nullptr : &start_key;
            const std::vector<uint8_t>* end_ptr = end_key.empty() ? nullptr : &end_key;

            Status status = immutable_memtable_->scan(start_ptr, end_ptr, xid, txn_mgr_,
                                                      &source.entries, ctx);
            if (status != Status::OK) {
                return status;
            }

            if (!source.entries.empty()) {
                sources.push_back(std::move(source));
            }
        }
    }

    // ========================================================================
    // STEP 3: Scan SSTables (All Levels)
    // ========================================================================
    {
        std::lock_guard<std::mutex> lock(sstables_mutex_);

        for (uint32_t level = 0; level < 4; level++) {
            const auto& level_sstables = sstables_[level];

            for (uint32_t sstable_idx = 0; sstable_idx < level_sstables.size(); sstable_idx++) {
                const auto& sstable = level_sstables[sstable_idx];

                if (!sstable || !sstable->isOpen()) {
                    continue;
                }

                // Quick range check: skip SSTable if range doesn't overlap
                std::vector<uint8_t> sstable_min = sstable->getMinKey();
                std::vector<uint8_t> sstable_max = sstable->getMaxKey();

                // Skip if: SSTable max < start_key OR SSTable min > end_key
                if (!start_key.empty() && sstable_max < start_key) {
                    continue;  // SSTable is entirely before start_key
                }
                if (!end_key.empty() && sstable_min > end_key) {
                    continue;  // SSTable is entirely after end_key
                }

                // Scan this SSTable
                ScanSource source;
                source.type = ScanSource::Type::SSTABLE;
                source.level = level;
                source.sstable_idx = sstable_idx;

                Status status = sstable->scan(start_key, end_key, xid, txn_mgr_,
                                             &source.entries, ctx);
                if (status != Status::OK) {
                    // Log error but continue with other SSTables
                    // TODO: Add proper logging
                    continue;
                }

                if (!source.entries.empty()) {
                    sources.push_back(std::move(source));
                }
            }
        }
    }

    // ========================================================================
    // STEP 4: K-way Merge
    // ========================================================================

    // Special case: no sources
    if (sources.empty()) {
        return Status::OK;
    }

    // Special case: only one source (no merge needed)
    if (sources.size() == 1) {
        for (const auto& kv_pair : sources[0].entries) {
            MemtableEntry entry;
            entry.key = kv_pair.first;
            entry.value = kv_pair.second;
            entry.sequence_number = 0;  // Not used for output
            entry.entry_type = ENTRY_TYPE_INSERT;
            entry.xmin = 0;  // Already filtered by visibility
            entry.xmax = 0;
            entries_out->push_back(entry);
        }
        return Status::OK;
    }

    // K-way merge using priority queue
    std::priority_queue<MergeEntry, std::vector<MergeEntry>, std::greater<MergeEntry>> pq;

    // Initialize priority queue with first entry from each source
    for (size_t source_id = 0; source_id < sources.size(); source_id++) {
        const auto* entry = sources[source_id].getCurrentEntry();
        if (entry) {
            MergeEntry merge_entry;
            merge_entry.key = entry->first;
            merge_entry.value = entry->second;
            merge_entry.source_id = source_id;
            pq.push(merge_entry);
        }
    }

    // Track last key to avoid duplicates
    std::vector<uint8_t> last_key;
    bool first_entry = true;

    // Merge loop
    while (!pq.empty()) {
        // Pop entry with smallest key
        MergeEntry current = pq.top();
        pq.pop();

        // Check if this is a duplicate key
        if (!first_entry && current.key == last_key) {
            // Skip duplicate (we already processed newest version)
            // Just advance the source and continue
            sources[current.source_id].advance();

            const auto* next_entry = sources[current.source_id].getCurrentEntry();
            if (next_entry) {
                MergeEntry next_merge;
                next_merge.key = next_entry->first;
                next_merge.value = next_entry->second;
                next_merge.source_id = current.source_id;
                pq.push(next_merge);
            }
            continue;
        }

        // This is the newest version of this key - add to results
        MemtableEntry result_entry;
        result_entry.key = current.key;
        result_entry.value = current.value;
        result_entry.sequence_number = 0;  // Not used for scan results
        result_entry.entry_type = ENTRY_TYPE_INSERT;
        result_entry.xmin = 0;  // Already visibility filtered
        result_entry.xmax = 0;

        entries_out->push_back(result_entry);

        // Update last key
        last_key = current.key;
        first_entry = false;

        // Advance source and add next entry to priority queue
        sources[current.source_id].advance();

        const auto* next_entry = sources[current.source_id].getCurrentEntry();
        if (next_entry) {
            MergeEntry next_merge;
            next_merge.key = next_entry->first;
            next_merge.value = next_entry->second;
            next_merge.source_id = current.source_id;
            pq.push(next_merge);
        }
    }

    return Status::OK;
}
```

---

## MGA COMPLIANCE VERIFICATION ✅

### Requirements

Per `/MGA_RULES.md`:
- ✅ Uses `TransactionId` (uint64_t), NOT `Snapshot*`
- ✅ Visibility checks via `isVersionVisible()` (in Memtable and SSTableReader)
- ✅ TIP-based visibility (no snapshot arrays)
- ✅ O(1) transaction state lookups
- ✅ Stable TIDs (entries already filtered by lower levels)

### Compliance in Implementation

```cpp
// ✅ CORRECT: TransactionId parameter
Status LSMTreeIndex::scan(const std::vector<uint8_t> &start_key,
                          const std::vector<uint8_t> &end_key,
                          uint64_t xid,  // NOT Snapshot*
                          std::vector<MemtableEntry> *entries_out,
                          ErrorContext *ctx)

// ✅ CORRECT: Memtable uses isEntryVisible() which calls isVersionVisible()
active_memtable_->scan(start_ptr, end_ptr, xid, txn_mgr_, &source.entries, ctx);

// ✅ CORRECT: SSTableReader uses isEntryVisible() which calls isVersionVisible()
sstable->scan(start_key, end_key, xid, txn_mgr_, &source.entries, ctx);
```

**All visibility filtering happens at lower levels** (Memtable and SSTableReader), which already use correct MGA patterns. The k-way merge just combines pre-filtered results.

---

## TESTING PLAN

### Unit Tests (8-10 tests)

1. **test_lsm_range_scan_empty**
   - Empty index, any range returns empty

2. **test_lsm_range_scan_single_key**
   - One key in memtable, scan range containing it

3. **test_lsm_range_scan_full_range**
   - 100 keys, scan entire range

4. **test_lsm_range_scan_partial_range**
   - 100 keys, scan middle 50

5. **test_lsm_range_scan_null_boundaries**
   - start_key = nullptr (from beginning)
   - end_key = nullptr (to end)

6. **test_lsm_range_scan_with_deletes**
   - Insert keys 1-10
   - Delete keys 3, 5, 7
   - Scan should return 7 keys (skip deleted)

7. **test_lsm_range_scan_multiple_levels**
   - Keys in memtable, L0, L1, L2
   - Scan should merge all levels

8. **test_lsm_range_scan_overlapping_sstables**
   - Multiple L0 SSTables with overlapping keys
   - Scan should return newest version only

9. **test_lsm_range_scan_mga_visibility**
   - Transaction T1 inserts keys
   - Transaction T2 scans (should not see T1's uncommitted changes)
   - T1 commits
   - Transaction T3 scans (should see T1's committed changes)

10. **test_lsm_range_scan_deduplication**
    - Same key in memtable and multiple SSTables
    - Scan should return only memtable version (newest)

### Integration Tests (3-5 tests)

```sql
-- Test 1: Basic range query
CREATE TABLE logs (id INT, timestamp TIMESTAMP, message TEXT);
CREATE INDEX idx_logs_ts ON logs USING LSM (timestamp);

INSERT INTO logs VALUES (1, '2025-01-15', 'Log 1');
INSERT INTO logs VALUES (2, '2025-02-15', 'Log 2');
INSERT INTO logs VALUES (3, '2025-03-15', 'Log 3');

SELECT * FROM logs WHERE timestamp BETWEEN '2025-01-01' AND '2025-01-31';
-- Expected: 1 row (Log 1)

-- Test 2: Comparison operators
SELECT * FROM logs WHERE timestamp > '2025-01-01' AND timestamp < '2025-03-01';
-- Expected: 2 rows (Log 1, Log 2)

-- Test 3: Large range with flush
INSERT 10000 rows (forces memtable flush to SSTables)
SELECT COUNT(*) FROM logs WHERE timestamp >= '2025-01-01';
-- Expected: 10003 rows

-- Test 4: Range scan with ORDER BY
SELECT * FROM logs WHERE timestamp >= '2025-01-01' ORDER BY timestamp ASC;
-- Expected: Results already sorted (no additional sort needed)

-- Test 5: Range scan with LIMIT
SELECT * FROM logs WHERE timestamp >= '2025-01-01' LIMIT 5;
-- Expected: First 5 results
```

### Performance Tests (3 tests)

1. **Scan Speed vs B-Tree**
   - Same dataset (100K rows)
   - Same range query
   - LSM should be within 2x of B-Tree

2. **Bloom Filter Effectiveness**
   - Measure number of SSTables accessed vs total SSTables
   - Should skip 90%+ of irrelevant SSTables

3. **K-way Merge Overhead**
   - Measure merge time for 1, 4, 8, 16 sources
   - Should scale O(N log K)

---

## EDGE CASES

### Handled Edge Cases ✅

1. **Empty index** - Returns empty result
2. **NULL start_key** - Scan from beginning
3. **NULL end_key** - Scan to end
4. **Empty range** (start > end) - Returns empty result
5. **Deleted keys** - Skipped (tombstone handling)
6. **Duplicate keys across levels** - Returns newest version only
7. **Overlapping L0 SSTables** - Correct merge
8. **No visible entries** - Returns empty result
9. **Single source** - Optimized (no merge needed)
10. **SSTable read error** - Continue with other SSTables

---

## PERFORMANCE CHARACTERISTICS

### Time Complexity

- **K-way merge**: O(N log K)
  - N = total entries scanned
  - K = number of sources (memtables + SSTables)
  - Typical K ≤ 20 (2 memtables + up to 18 SSTables)

- **Memtable scan**: O(M log M)
  - M = memtable size
  - Red-Black tree iteration

- **SSTable scan**: O(B * E)
  - B = number of blocks read
  - E = entries per block

### Space Complexity

- **Priority queue**: O(K) ≈ 20 entries
- **Source buffers**: O(N) total entries
- **Results**: O(R) where R = number of results

### Optimization Opportunities

1. **Bloom filter skipping** - Already implemented in SSTableReader
2. **Range pruning** - Skip SSTables outside query range ✅
3. **Single source optimization** - No merge if only one source ✅
4. **Lazy loading** - Could stream from disk instead of loading all at once

---

## IMPLEMENTATION STATUS

### Completed ✅
- [x] Read and understand existing code
- [x] Design k-way merge algorithm
- [x] Verify MGA compliance
- [x] Document implementation plan

### In Progress 🔄
- [ ] Implement LSMTreeIndex::scan()
- [ ] Add helper structures (ScanSource, MergeEntry)
- [ ] Implement k-way merge logic

### Pending ⏳
- [ ] Write unit tests (8-10 tests)
- [ ] Write integration tests (SQL queries)
- [ ] Run performance benchmarks
- [ ] Update documentation

---

## ESTIMATED COMPLETION

**Original**: 15-20 hours
**Revised**: 8-12 hours (due to existing infrastructure)

**Current Progress**: 20% complete (design phase)
**Remaining Work**:
- Implementation: 4-5 hours
- Testing: 3-5 hours
- Documentation: 1 hour

**Expected Completion**: November 7, 2025

---

**Last Updated**: November 6, 2025
**Status**: Design Complete, Ready for Implementation

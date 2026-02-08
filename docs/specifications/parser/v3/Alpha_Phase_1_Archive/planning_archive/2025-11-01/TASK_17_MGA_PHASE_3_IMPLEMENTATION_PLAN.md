# Task 17 MGA Phase 3: B-tree MGA Enhancements Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: October 31, 2025
**Status**: 🚧 IN PLANNING
**Priority**: 🔴 CRITICAL
**Estimated Effort**: 10-15 hours

---

## Overview

Phase 3 adds index-level visibility filtering to optimize query performance. Currently, visibility is checked at the heap level (correct but slower). Phase 3 moves visibility filtering to the index level to reduce heap accesses.

**Current behavior** (correct but slower):
```cpp
// B-tree returns ALL matching TIDs (including invisible ones)
std::vector<TID> all_tids = btree->search(key, snapshot);

// Executor filters at heap level
for (TID tid : all_tids) {
    if (storage_engine->isVisible(tid, xid)) {
        // Use this tuple
    }
}
```

**Enhanced behavior** (faster):
```cpp
// B-tree filters at index level (only returns visible TIDs)
std::vector<TID> visible_tids = btree->search(key, snapshot);

// Executor doesn't need heap visibility check
for (TID tid : visible_tids) {
    // Use this tuple (already visible)
}
```

**Benefits**:
- Fewer heap page accesses
- Better cache utilization
- Lower I/O
- Faster queries with many invisible tuples

**Trade-offs**:
- Index API changes (breaking change)
- More complex implementation
- Slightly larger on-disk format (already have btn_xmin/btn_xmax)

---

## Current State Analysis

### What Already Exists ✅

1. **On-disk structure supports MGA** (`include/scratchbird/core/btree.h` lines 122-123):
   ```cpp
   struct SBBTreeNode {
       // ... other fields ...
       uint64_t btn_xmin;  // Node creation transaction
       uint64_t btn_xmax;  // Node deletion transaction
   };
   ```

2. **Transaction context available** (Phase 1 complete):
   - `buildExpressionIndex(uint64_t xid, ...)` has xid
   - `updateIndexesOnInsert/Update/Delete` have xid
   - All index maintenance methods have transaction context

3. **Visibility infrastructure exists** (Phase 1 complete):
   - `storage_engine->isVisible(xmin, xmax, xid)` works
   - `Snapshot` structure defined
   - `TransactionManager::getTxnState(xid)` works

### What's Missing ❌

1. **btn_xmin/btn_xmax not populated**:
   - Set to 0 in `BTreePage::add_node()` (line 76: `// TODO: Integrate with transaction manager`)
   - Not updated during insert/delete

2. **No xid parameter in B-tree API**:
   - `BTree::insert(key, tid, ctx)` - no xid parameter
   - `BTree::remove(key, tid, ctx)` - no xid parameter
   - `BTreePage::add_node(key, value, ctx)` - no xid parameter

3. **search() doesn't use btn_xmin/btn_xmax**:
   - Returns all TIDs regardless of visibility
   - Snapshot parameter exists but not used for index-level filtering

4. **No markDeleted() method**:
   - Only physical removal via `remove()`
   - No soft deletion support

---

## Implementation Plan

### Phase 3.1: Populate btn_xmin/btn_xmax (4-6 hours)

#### Step 1: Add xid parameter to B-tree API (1-2 hours)

**File**: `include/scratchbird/core/btree.h`

**Before**:
```cpp
Status insert(const std::vector<uint8_t> &key, const TID &tid,
              ErrorContext *ctx = nullptr);

Status remove(const std::vector<uint8_t> &key, const TID &tid,
              ErrorContext *ctx = nullptr);
```

**After**:
```cpp
// Add xid parameter for MGA visibility tracking
Status insert(const std::vector<uint8_t> &key, const TID &tid,
              uint64_t xid,  // NEW: Transaction ID for btn_xmin
              ErrorContext *ctx = nullptr);

Status remove(const std::vector<uint8_t> &key, const TID &tid,
              uint64_t xid,  // NEW: Transaction ID for btn_xmax
              ErrorContext *ctx = nullptr);
```

**Impact**: Breaking change - all callers must be updated

**Files to update**:
- `src/sblr/executor.cpp` - 3 call sites in buildExpressionIndex, updateIndexesOnInsert, updateIndexesOnUpdate
- `src/core/btree.cpp` - insert/remove implementations
- Any other index users (search codebase)

#### Step 2: Pass xid to BTreePage::add_node() (30 minutes)

**File**: `include/scratchbird/core/btree_page.h`

**Before**:
```cpp
Status add_node(const std::vector<uint8_t> &key, const Tuple &value,
                ErrorContext *ctx = nullptr);
```

**After**:
```cpp
Status add_node(const std::vector<uint8_t> &key, const Tuple &value,
                uint64_t xmin,  // NEW: Transaction creating this entry
                ErrorContext *ctx = nullptr);
```

**File**: `src/core/btree_page.cpp` line 76-77

**Before**:
```cpp
new_node->btn_xmin = 0; // TODO: Integrate with transaction manager
new_node->btn_xmax = 0;
```

**After**:
```cpp
new_node->btn_xmin = xmin;  // Set by caller (transaction creating entry)
new_node->btn_xmax = 0;      // 0 = entry is active (not deleted)
```

#### Step 3: Update all insert() call sites (1-2 hours)

**Files to update**:

1. **src/sblr/executor.cpp** - buildExpressionIndex (line ~1539)
   ```cpp
   // Before
   status = btree->insert(key_bytes, tuple.tid, nullptr);

   // After
   status = btree->insert(key_bytes, tuple.tid, xid, nullptr);
   ```

2. **src/sblr/executor.cpp** - updateIndexesOnInsert (line ~1709)
   ```cpp
   // Before
   btree->insert(key_bytes, tid, nullptr);

   // After
   btree->insert(key_bytes, tid, xid, nullptr);
   ```

3. **src/sblr/executor.cpp** - updateIndexesOnUpdate (line ~1888)
   ```cpp
   // Before
   btree->insert(new_key, new_tid, nullptr);

   // After
   btree->insert(new_key, new_tid, xid, nullptr);
   ```

4. **src/core/btree.cpp** - split_leaf_page (recursive insert call)
   - Need to pass xid through split operations
   - Check for any other internal insert calls

#### Step 4: Update all split/internal operations (1-2 hours)

**Challenge**: Page splits create new internal nodes. What xid should they use?

**Solution**: Internal nodes (non-leaf) don't need accurate xmin/xmax because:
- Visibility is checked at leaf level (where TIDs are stored)
- Internal nodes just guide navigation
- Can set to 0 or special value (INVALID_XID)

**Files**: `src/core/btree.cpp`
- `split_leaf_page()` - pass xid to new leaf node creation
- `split_internal_page()` - set xmin = 0 for internal nodes
- `insert_into_parent()` - set xmin = 0 for internal nodes
- `create_new_root()` - set xmin = 0 for root

#### Step 5: Testing (1 hour)

**Tests**:
1. Insert entries with different xids
2. Read back btn_xmin values
3. Verify entries have correct transaction IDs
4. Test page splits preserve xmin

**Expected results**:
- Leaf entries have btn_xmin = inserting transaction XID
- Leaf entries have btn_xmax = 0 (active)
- Internal nodes have btn_xmin = 0 (don't care)

---

### Phase 3.2: Implement markDeleted() (3-4 hours)

#### Purpose

Support soft deletion (set btn_xmax instead of physical removal). This is more efficient than physical removal because:
- No page reorganization needed
- Faster DELETE operations
- Deferred cleanup to GC (removeDeadEntries)
- Supports MVCC (deleted entries still visible to older snapshots)

#### Implementation

**File**: `include/scratchbird/core/btree.h`

**Add new method**:
```cpp
/**
 * Mark index entry as deleted by setting btn_xmax
 *
 * More efficient than physical removal - entry remains in index
 * but becomes invisible to transactions >= xmax (if committed).
 * Physical removal happens later during GC/vacuum.
 *
 * @param key Index key
 * @param tid Tuple ID to mark deleted
 * @param xmax Transaction ID deleting this entry
 * @param ctx Error context
 * @return Status::OK on success
 */
Status markDeleted(const std::vector<uint8_t> &key,
                   const TID &tid,
                   uint64_t xmax,
                   ErrorContext *ctx = nullptr);
```

**File**: `src/core/btree.cpp`

**Implementation** (similar to search, but sets xmax):
```cpp
Status BTree::markDeleted(const std::vector<uint8_t> &key,
                          const TID &tid,
                          uint64_t xmax,
                          ErrorContext *ctx)
{
    // 1. Navigate to leaf page containing key
    uint64_t leaf_page_num;
    Status status = find_leaf_page(key, &leaf_page_num, true, ctx);
    if (status != Status::OK) {
        return status;
    }

    // 2. Pin page
    BufferPool *bp = db_->buffer_pool();
    void *page_buffer;
    status = bp->pinPage(leaf_page_num, &page_buffer, ctx);
    if (status != Status::OK) {
        return status;
    }

    auto *page = reinterpret_cast<SBBTreePage*>(page_buffer);
    uint8_t *page_data = reinterpret_cast<uint8_t*>(page_buffer);

    // 3. Scan entries on page to find matching key + TID
    auto *offsets = reinterpret_cast<uint16_t*>(page_data + sizeof(SBBTreePage));
    bool found = false;

    for (uint16_t i = 0; i < page->btr_count; i++) {
        auto *node = reinterpret_cast<SBBTreeNode*>(page_data + offsets[i]);

        // Extract key
        uint8_t *node_key_data = reinterpret_cast<uint8_t*>(node) + sizeof(SBBTreeNode);
        std::vector<uint8_t> node_key(node_key_data, node_key_data + node->btn_key_len);

        // Check if key matches
        if (compare_keys(key, node_key) == 0) {
            // Key matches - check if TID matches
            uint8_t *tid_data = node_key_data + node->btn_key_len;
            auto *tids = reinterpret_cast<uint64_t*>(tid_data);

            for (uint32_t j = 0; j < node->btn_tuple_count; j++) {
                TID node_tid = convertLegacyToTID(tids[j]);
                if (node_tid.value() == tid.value()) {
                    // Found it! Set xmax
                    node->btn_xmax = xmax;
                    found = true;
                    break;
                }
            }
        }

        if (found) break;
    }

    // 4. Unpin page (mark dirty if modified)
    bp->unpinPage(leaf_page_num, found, ctx);

    return found ? Status::OK : Status::NOT_FOUND;
}
```

#### Integration with Executor

**File**: `src/sblr/executor.cpp`

**Update DELETE operations to use markDeleted()** instead of remove():

```cpp
// In updateIndexesOnDelete()
// Before:
btree->remove(key_bytes, tid, nullptr);

// After (preferred):
btree->markDeleted(key_bytes, tid, xid, nullptr);

// OR keep remove() for immediate removal (user choice)
```

**Benefits of markDeleted()**:
- ✅ Faster DELETEs (no page reorganization)
- ✅ MVCC-correct (old snapshots see entry)
- ✅ Deferred cleanup to GC

**When to use remove()**:
- When immediate space reclamation needed
- When no concurrent transactions expected

#### Testing (1 hour)

**Tests**:
1. Insert entry with xid = 100
2. Delete entry with xid = 200 (using markDeleted)
3. Verify btn_xmin = 100, btn_xmax = 200
4. Test visibility: xid < 200 sees entry, xid >= 200 doesn't

---

### Phase 3.3: Visibility-Aware Search (3-5 hours)

#### Purpose

Filter invisible entries at the index level instead of heap level.

#### Current search() behavior

**File**: `src/core/btree.cpp`

Currently returns all TIDs:
```cpp
Status BTree::search(const std::vector<uint8_t> &key,
                     struct Snapshot *snapshot,
                     std::vector<TID> *tids_out,
                     ErrorContext *ctx)
{
    // Navigate to leaf page
    // Find matching key
    // Return ALL TIDs (no visibility check)
    for (each matching entry) {
        tids_out->push_back(entry.tid);
    }
}
```

#### Enhanced search() with visibility

**Add visibility filtering**:
```cpp
Status BTree::search(const std::vector<uint8_t> &key,
                     struct Snapshot *snapshot,
                     std::vector<TID> *tids_out,
                     ErrorContext *ctx)
{
    // Navigate to leaf page
    uint64_t leaf_page_num;
    Status status = find_leaf_page(key, &leaf_page_num, false, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Pin page and search
    BufferPool *bp = db_->buffer_pool();
    void *page_buffer;
    status = bp->pinPage(leaf_page_num, &page_buffer, ctx);
    if (status != Status::OK) {
        return status;
    }

    auto *page = reinterpret_cast<SBBTreePage*>(page_buffer);
    uint8_t *page_data = reinterpret_cast<uint8_t*>(page_buffer);
    auto *offsets = reinterpret_cast<uint16_t*>(page_data + sizeof(SBBTreePage));

    for (uint16_t i = 0; i < page->btr_count; i++) {
        auto *node = reinterpret_cast<SBBTreeNode*>(page_data + offsets[i]);

        // Extract and compare key
        uint8_t *node_key_data = reinterpret_cast<uint8_t*>(node) + sizeof(SBBTreeNode);
        std::vector<uint8_t> node_key(node_key_data, node_key_data + node->btn_key_len);

        if (compare_keys(key, node_key) == 0) {
            // Key matches - check visibility if snapshot provided

            // NEW: Visibility check using btn_xmin and btn_xmax
            if (snapshot != nullptr) {
                uint64_t xmin = node->btn_xmin;
                uint64_t xmax = node->btn_xmax;

                // Check if entry is visible to this snapshot
                if (!isEntryVisible(xmin, xmax, snapshot)) {
                    continue;  // Skip invisible entry
                }
            }

            // Entry is visible - return TIDs
            uint8_t *tid_data = node_key_data + node->btn_key_len;
            auto *tids = reinterpret_cast<uint64_t*>(tid_data);

            for (uint32_t j = 0; j < node->btn_tuple_count; j++) {
                TID tid = convertLegacyToTID(tids[j]);
                tids_out->push_back(tid);
            }
        }
    }

    bp->unpinPage(leaf_page_num, false, ctx);
    return Status::OK;
}
```

#### Add visibility helper method

**File**: `include/scratchbird/core/btree.h` (private section)

```cpp
private:
    /**
     * Check if index entry is visible to snapshot
     *
     * @param xmin Transaction that created entry
     * @param xmax Transaction that deleted entry (0 if active)
     * @param snapshot Snapshot for visibility check
     * @return true if visible, false otherwise
     */
    bool isEntryVisible(uint64_t xmin, uint64_t xmax, struct Snapshot *snapshot) const;
```

**File**: `src/core/btree.cpp`

```cpp
bool BTree::isEntryVisible(uint64_t xmin, uint64_t xmax, struct Snapshot *snapshot) const
{
    // Get transaction manager
    TransactionManager *txn_mgr = db_->transaction_manager();
    if (txn_mgr == nullptr) {
        // No transaction manager - assume visible
        return true;
    }

    // Check if creating transaction (xmin) is visible
    TxnState xmin_state = txn_mgr->getTxnState(xmin);
    if (xmin_state == TXN_STATE_ABORTED) {
        return false;  // Entry from aborted transaction - invisible
    }
    if (xmin_state == TXN_STATE_IN_PROGRESS) {
        // Entry from in-progress transaction
        // Visible only if it's our own transaction
        return (xmin == snapshot->xid);
    }
    // xmin_state == TXN_STATE_COMMITTED - entry was committed

    // Check if deleting transaction (xmax) committed
    if (xmax != 0) {
        TxnState xmax_state = txn_mgr->getTxnState(xmax);
        if (xmax_state == TXN_STATE_COMMITTED) {
            // Entry was deleted and delete committed
            // Check if delete happened before our snapshot
            if (xmax < snapshot->xid) {
                return false;  // Deleted before our snapshot - invisible
            }
        }
        else if (xmax_state == TXN_STATE_IN_PROGRESS) {
            // Delete in progress
            if (xmax != snapshot->xid) {
                return true;  // Other transaction's delete - we see old version
            }
            else {
                return false;  // Our own delete - we don't see it
            }
        }
        // xmax_state == TXN_STATE_ABORTED - delete aborted, entry still visible
    }

    return true;  // Entry is visible
}
```

#### Update rangeScan() similarly (2 hours)

**File**: `src/core/btree.cpp`

Apply same visibility filtering to `BTreeIterator::next()`:
- Check btn_xmin/btn_xmax before returning TID
- Skip invisible entries
- Maintain correct ordering

#### Testing (1 hour)

**Tests**:
1. Insert with xid = 100
2. Search with snapshot xid = 99 (should not see)
3. Search with snapshot xid = 101 (should see)
4. Delete with xid = 200
5. Search with snapshot xid = 199 (should see)
6. Search with snapshot xid = 201 (should not see)

---

## Testing Strategy

### Unit Tests

**File**: `tests/unit/test_btree_mga.cpp` (new file)

**Tests**:
1. `test_insert_populates_xmin` - Verify btn_xmin set correctly
2. `test_markDeleted_sets_xmax` - Verify btn_xmax set correctly
3. `test_search_visibility_aborted` - Skip entries from aborted txns
4. `test_search_visibility_committed` - Return entries from committed txns
5. `test_search_visibility_deleted` - Skip entries with xmax < snapshot
6. `test_rangeScan_visibility` - Verify range scans filter correctly
7. `test_page_split_preserves_xmin` - Page splits maintain xmin values

### Integration Tests

**File**: `tests/integration/test_expression_index_mga.cpp`

**Tests**:
1. Expression index with concurrent inserts (different xids)
2. Filtered index with DELETE and visibility
3. Complex queries with visibility filtering
4. Performance comparison (with/without index-level filtering)

---

## Performance Impact

### Expected Improvements

**Scenario**: Table with 1M rows, 500K deleted (xmax set), query returns 10 rows

**Current** (heap-level filtering):
- Index scan returns ~510 TIDs (10 visible + 500 invisible from same key)
- 510 heap page accesses
- 510 visibility checks

**Enhanced** (index-level filtering):
- Index scan returns ~10 TIDs (only visible)
- 10 heap page accesses
- 10 visibility checks (already done at index level)

**Speedup**: ~50x fewer heap accesses!

### Overhead

**Insert overhead**:
- Additional 8 bytes per entry (btn_xmin)
- Negligible CPU cost (one parameter pass)

**Search overhead**:
- Visibility check at index level (~50-100 ns per entry)
- Saves heap access (~1-10 µs per entry)
- **Net gain**: 10-100x faster

---

## Migration Notes

### Breaking Changes

1. **BTree::insert() API change**:
   - Old: `insert(key, tid, ctx)`
   - New: `insert(key, tid, xid, ctx)`
   - **Action**: Update all callers

2. **BTree::remove() API change**:
   - Old: `remove(key, tid, ctx)`
   - New: `remove(key, tid, xid, ctx)`
   - **Action**: Update all callers OR use markDeleted()

3. **BTreePage::add_node() API change**:
   - Old: `add_node(key, value, ctx)`
   - New: `add_node(key, value, xmin, ctx)`
   - **Action**: Internal change, no external callers

### Backward Compatibility

**On-disk format**: Compatible!
- SBBTreeNode already has btn_xmin/btn_xmax fields
- Existing indexes have btn_xmin = 0, btn_xmax = 0
- New behavior: entry with xmin = 0 treated as "always visible" (legacy)

**Upgrade path**:
- No index rebuild required
- Old entries (xmin = 0) remain visible
- New entries use proper xmin
- GC will eventually clean up old entries

---

## Summary

Phase 3 adds index-level visibility filtering by:
1. Populating btn_xmin during insert (4-6h)
2. Implementing markDeleted() for soft deletion (3-4h)
3. Adding visibility checks to search() and rangeScan() (3-5h)

**Total effort**: 10-15 hours
**Impact**: 10-100x performance improvement for queries with many deleted tuples
**Risk**: Low (incremental changes, backward compatible on-disk format)

**Next**: Phase 4 (Comprehensive Testing) to validate all MGA behavior.

---

**Document Date**: October 31, 2025
**Phase**: 3 - B-tree MGA Enhancements
**Status**: IN PLANNING
**Estimated Effort**: 10-15 hours

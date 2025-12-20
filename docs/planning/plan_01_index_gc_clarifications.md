# Plan 01 - Index GC Clarifications (Per Index Type)

This appendix gives exact, per-index guidance for `removeDeadEntries()` so a low-capability agent can implement GC without guessing internal traversal.

## Global Rules (Applies to All Indexes)
- **Source of truth**: `dead_tids` passed to `removeDeadEntries()` are already confirmed dead by heap sweep (OIT-based). Do **not** re-check heap visibility.
- **TID format**: Use the on-disk format used by that index:
  - If the index stores legacy `uint64_t` TIDs, use `convertTIDtoLegacy()`.
  - If the index stores `TID`/`GPID` + slot, compare those fields directly.
- **Thread safety**: Use the index’s existing lock (e.g., `std::shared_mutex` in GiST/RTree, `mutex_` in SPGiST). If no lock exists, do not invent one—match existing conventions.
- **Stats**: Always update `entries_removed_out` and `pages_modified_out`.

---

## ✅ Already Implemented (Do Not Change)
- **B-Tree**: `src/core/btree.cpp` `BTree::removeDeadEntries()`
  - Scans leaf chain and marks nodes `DELETED`.
- **Hash**: `src/core/hash_index.cpp` `HashIndex::removeDeadEntries()`
  - Scans buckets + overflow, marks `INVALID_TID`, updates meta counts.
- **Bitmap**: `src/core/bitmap_index.cpp` `BitmapIndex::removeDeadEntries()`
  - Removes dead TIDs from Roaring bitmaps, updates cardinality.
- **SP-GiST**: `src/core/spgist_index.cpp` `SPGiSTIndex::removeDeadEntries()`
  - Recursively rewrites leaf pages excluding dead TIDs.
- **LSM**: `src/core/lsm_tree.cpp` `LSMTree::removeDeadEntries()`
  - Removes dead versions from memtable (OK for alpha).

---

## ❗Needs Work (Exact Guidance)

### 1) GIN (and FULLTEXT via GIN)
**Files**:
- `src/core/gin_index.cpp` `GinIndex::removeDeadEntries()`
- `src/core/fulltext_index.cpp` (should delegate to `GinIndex`)

**What’s missing**: Step 2 “posting list/tree pruning” is stubbed.

**Exact algorithm**:
1. **Pending list cleanup**: already implemented; keep as-is.
2. **Collect all posting pages from entry tree**:
   - Reuse `updateTIDsAfterMigration()` pattern (`collectPostingPages` lambda). It already traverses the entry tree and collects `posting_list_page` for all keys.
3. **For each posting page**:
   - Pin page and read `SBGinPostingListPage`.
   - If `gpl_is_tree != 0`:
     - Traverse posting tree leaves (reuse `updatePostingTree` traversal from `updateTIDsAfterMigration`).
     - In each `SBGinPostingTreeLeaf`, **compact** `gpt_tids[]` by removing entries whose TID is in `dead_set`.
     - Update `gpt_entry_count` and mark page dirty.
     - If leaf becomes empty, keep it (no rebalance in alpha).
   - Else (simple list):
     - If `gpl_is_compressed != 0`: decompress list, remove dead TIDs, recompress (using existing `compress_posting_list`), update `gpl_entry_count` and `gpl_compressed_bytes`.
     - If uncompressed: compact `GinPostingEntry[]`, update `gpl_entry_count`, mark page dirty.
4. **Empty posting list**:
   - Leave the key in the entry tree for alpha (no key deletion required). Optional future optimization.

**FullText**:
- Implement `FullTextIndex::removeDeadEntries()` to call `gin_index_->removeDeadEntries(...)`.

---

### 2) GiST
**Files**:
- `src/core/gist_index.cpp`
- `include/scratchbird/core/gist_index.h`

**Issue**: Current `removeDeadEntries(uint64_t oldest_active_xid)` signature is incompatible with `IndexGCInterface`.

**Exact fix**:
- Add `Status removeDeadEntries(const std::vector<TID>& dead_tids, ...)` that:
  1. Builds `dead_set` from `dead_tids` (use `TID` directly; leaf entries store `entry_row_id`).
  2. Gets `uint64_t oit = TransactionManager::getOldestXid()`.
  3. Calls a modified recursive function that:
     - For **leaf entries**: remove entry if `entry_row_id` in `dead_set` **and** `entry_xmax != 0 && entry_xmax < oit`.
     - For **internal entries**: recurse into children that are not deleted.
     - Rebuild page with only live entries; update `gist_count`, `gist_free_space`, and `gist_deleted_entries`.

---

### 3) BRIN
**Files**:
- `src/core/brin_index.cpp`
- `include/scratchbird/core/brin_index.h`

**Issue**: `removeDeadEntries()` is stubbed; must resummarize ranges.

**Exact algorithm**:
1. Convert `dead_tids` into **block numbers** using `getPageNumber(tid)`.
2. For each BRIN range covering any dead block:
   - Rescan heap blocks `[brn_start_block, brn_end_block]`.
   - Recompute min/max using `BrinMinmaxOps` (same as insert/update path).
   - If **no live tuples** remain, set:
     - `brn_xmax = current_xid` and `brn_flags |= DELETED`.
   - Otherwise, update `min/max`, `brn_flags` (NULL handling), leave `brn_xmax = 0`.
3. Mark page dirty and increment `pages_modified_out`.

---

### 4) HNSW
**Files**:
- `src/core/hnsw_index.cpp`
- `include/scratchbird/core/hnsw_index.h`

**Issue**: `removeDeadEntries()` is stubbed.

**Exact algorithm**:
1. Build `dead_set` using `TID` (compare `node_gpid` + `node_slot`).
2. Traverse all pages using `hnsw_right_sibling` starting at `idx_root_page`.
3. For each node:
   - If node TID is in `dead_set`:
     - Set `node_xmax = TransactionManager::getCurrentXid()`.
     - Set `node_flags |= DELETED`.
     - Increment `hnsw_deleted_nodes` in page header.
4. Optional (alpha safe): do **not** remove neighbors. Search already checks `is_node_visible()`.

---

### 5) R-Tree
**Files**:
- `src/core/rtree.cpp`
- `include/scratchbird/core/rtree.h`

**Issue**: Current GC only scans root; must traverse all leaves.

**Exact algorithm**:
1. Build `dead_set` from `dead_tids` (`TID` directly).
2. Get `oit = TransactionManager::getOldestXid()`.
3. Recursively traverse from root to all leaf pages:
   - If leaf entry’s `entry_row_id` is in `dead_set` **and** `entry_xmax != 0 && entry_xmax < oit`, remove it.
   - Update node bounding boxes and stats.
4. No full condense/merge required in alpha; leave empty nodes.

---

### 6) ColumnstoreIndexSimple
**Files**:
- `src/core/columnstore_index.cpp`
- `include/scratchbird/core/columnstore_index.h`
- Spec: `docs/specifications/COLUMNSTORE_SPEC.md` (row_xmin/xmax + first_tid model)

**Issue**: No per-row TID mapping exists; GC cannot map dead_tids to segment rows.

**Required changes**:
1. Extend `ColumnSegment` to track:
   - `first_tid` (or per-row `row_tids[]`),
   - `row_xmin[]`, `row_xmax[]` (per spec).
2. On `insertRow` → `flushColumnBuffer`, persist these vectors to segment metadata.
3. `removeDeadEntries()`:
   - For each segment, map `dead_tids` to row indexes using `first_tid` (or row_tids array).
   - Mark `row_xmax` for dead rows.
   - If >50% dead, **rebuild** the segment (recompress without dead rows, update `ColumnSegment` metadata).

---

## Reminder: FULLTEXT
`FullTextIndex` must **delegate GC to GIN**. Fulltext itself does not store posting lists.


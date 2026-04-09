# MongoDB 2dsphere Index Specification

## Purpose
Define spherical geospatial index using hierarchical geohash cells.

## Dependencies
- 18_Index_Framework/INDEX_ARCHITECTURE.md
- 18_Index_Framework/BTREE_SPEC.md

## Page Types
- Uses standard B-tree pages (`PAGE_TYPE_BTREE_*`).

## On-Disk Layout
This index uses the B-tree page layout defined in `BTREE_SPEC.md`.

Leaf entries store `(s2_cell_id, tid)` where `s2_cell_id` is encoded as an unsigned 64-bit integer.
Internal entries store `(s2_cell_id, child_page_id)` separators.

## Canonical Options
- `min_level` (u8): minimum geohash level (default 4).
- `max_level` (u8): maximum geohash level (default 23).
- `max_cells` (u32): max number of cells per geometry (default 20).
- `coarsest_level` (u8): if omitted, use `min_level`.
- `finest_level` (u8): if omitted, use `max_level`.

## Cell ID Encoding
- Convert lat/lon to normalized range:
  - `lat_norm = (lat + 90) / 180`
  - `lon_norm = (lon + 180) / 360`
- At level `L`, compute `bits = L` per axis.
- `ix = floor(lon_norm * (2^bits - 1))`
- `iy = floor(lat_norm * (2^bits - 1))`
- `cell_id = interleave_bits(ix, iy)`.
- Store `cell_id` with `level` in high 6 bits: `cell_key = (level << 58) | cell_id`.

## Cell Intersection Rules (Deterministic)
1. Represent each cell as a lat/lon rectangle `[lat_min, lat_max] x [lon_min, lon_max]`.
2. For point geometry: cell contains point if lat/lon within bounds.
3. For line geometry: cell intersects if any segment crosses the cell rectangle or any endpoint is inside.
4. For polygon geometry: cell fully inside if all polygon vertices are inside and no edge crosses boundary.
5. Cell intersects if any polygon edge crosses the cell rectangle or any vertex is inside.

## Embedded B-Tree Implementation (Required)
1. Page types: `PAGE_TYPE_BTREE_META`, `PAGE_TYPE_BTREE_INTERNAL`, `PAGE_TYPE_BTREE_LEAF`.
2. Meta layout: `root_page_id`, `first_leaf_page_id`, `tree_height`, `fillfactor`, `page_count`, `leaf_count`.
3. Page header: `left_sibling`, `right_sibling`, `leftmost_child_page_id`, `node_count`, `level`, `flags`, `free_offset`.
4. Slot array at page end stores 2-byte offsets to node records in sorted key order.
5. Node record format: `prefix_len`, `suffix_len`, `flags`, `tid_count`, `child_page_id`, `key_suffix`, `tid_list`.
6. Search: binary search slot array, reconstruct keys, follow child pointers until leaf.
7. Insert: traverse root to leaf with latch coupling, insert into leaf if space, else split.
8. Split: allocate new right page, move upper keys until `fillfactor` achieved, update high keys and parent separator, cascade splits upward.
9. Delete: GC removes dead TIDs; if entry empty remove it; rebalance or merge when below min fill.

## Geometry Covering Algorithm
1. Start with root cell at `coarsest_level`.
2. Maintain a queue of candidate cells.
3. For each cell:
   - If cell fully inside geometry, emit it.
   - Else if cell intersects geometry and `level < finest_level` and `cell_count < max_cells`, subdivide into 4 children and enqueue.
   - Else emit cell as covering cell.
4. Result is a set of `cell_key` values.

## Index Key
- For each `cell_key` emitted, insert `(cell_key, tid)` into B-tree.

## Build Algorithm
1. Capture build snapshot `S1` with MGA rules.
2. Initialize meta and root pages and set index state to `building`.
3. If this index requires training, dictionary build, or model materialization, run that phase first and persist its outputs.
4. Scan base table in physical order. For each row version visible in `S1`, derive all index entries and call `Insert` for each entry.
5. Populate `index_stats` with row_count, distinct_count, null_frac, min_value, max_value, and histogram buckets as applicable.
6. Validate structural invariants and set index state to `active`.

## Insert
1. Extract geometry and compute covering cell_keys.
2. For each `cell_key`, insert `(cell_key, tid)` into the B-tree.

## Delete
1. Recompute covering cell_keys for the geometry.
2. Remove `(cell_key, tid)` entries from the B-tree.

## Search
- Use same covering algorithm on query geometry to generate cell_keys.
- Scan B-tree for all matching cell_keys.
- Post-filter by exact geometry predicates (`within`, `intersects`, `near`).

## Error Handling
- Lat/lon outside valid ranges are rejected.

## Test Contract
- Covering algorithm respects `max_cells`.
- Query results are filtered to exact geometry matches.

## Update
1. If indexed value is unchanged, do nothing.
2. Otherwise perform Delete for the old value and Insert for the new value.

## Online Rebalance and Relocate (Per-Index Procedure)
1. Begin online maintenance and capture snapshot `S1`.
2. Create a shadow index in the target filespace if relocating; otherwise use the current filespace.
3. Build the shadow index from all row versions visible in `S1` using this spec's build and insert rules.
4. Apply deltas in commit order:
5. For `delta_op=insert`, re-fetch the row version by TID and re-check MGA visibility.
6. If visible, apply the Insert rules defined in this spec to the shadow index.
7. If the row is not visible or has rolled back, skip the delta.
8. For `delta_op=delete`, apply the Delete rules defined in this spec to the shadow index.
9. If this index type uses tombstones, set the tombstone flag instead of removing the entry in place.
10. If this index type is unique, enforce uniqueness in the shadow index during delta apply.
11. Validate structural invariants and set `state=validating` then `state=active`.
12. Acquire `LOCK_MAINTENANCE_EXCLUSIVE`, swap root/meta pointers to the shadow index, and update `index_storage.filespace_uuid` if relocating.
13. Mark old index pages for GC after OIT advances.

## Online Rebalance Notes
- Rebalance uses the same steps as online relocate but keeps filespace unchanged.
- Use the `target_fillfactor` option during shadow build.

## Online Relocate Notes
- All newly allocated pages must be in the target filespace.
- If any shadow build step fails, abort and discard shadow pages.

## Implementation Appendix (Self-Contained, Required)
This appendix is authoritative for implementing this index; do not require external documents.

### Standard Page Header (All Index Pages)
Each on-disk page begins with the standard header. All multi-byte fields are little-endian and 8-byte aligned.
- `magic` (4 bytes) = `SBRD`
- `version` (u16)
- `page_type` (u16)
- `page_size` (u32)
- `checksum` (u32) CRC32C of page bytes excluding this field
- `page_id` (u32)
- `flags` (u32)
- `database_uuid` (UUID, 16 bytes)
- `table_uuid` (UUID, 16 bytes)
- `generation` (u32)
- `free_space` (u16)
- `item_count` (u16)
- `free_offset` (u16)
- `special_size` (u16)
- `lsn_reserved` (u64, must be 0 in ScratchBird)

### TID Layout (16 bytes)
- `page_id` (u32)
- `slot_id` (u16)
- `version_id` (u16)
- `table_uuid_hash` (u32)
- `reserved` (u32, zero)

### Key Encoding (For All Ordered Indexes)
Key bytes are concatenated segments. Each segment encodes one column or expression value:
1. `null_flag` (u8): 0 = non-null, 1 = null.
2. `type_tag` (u8): identifies the base type (see list below).
3. `len` (u16): length of value bytes.
4. `value_bytes` (len bytes).
5. `collation_id` (u16) if collation-specific ordering is required; otherwise 0.

Type tags (u8):
- 0x01 BOOL
- 0x02 INT8
- 0x03 INT16
- 0x04 INT32
- 0x05 INT64
- 0x06 UINT8
- 0x07 UINT16
- 0x08 UINT32
- 0x09 UINT64
- 0x0A FLOAT32
- 0x0B FLOAT64
- 0x0C DECIMAL128
- 0x0D DATE
- 0x0E TIME
- 0x0F TIMESTAMP
- 0x10 UUID
- 0x11 BINARY
- 0x12 STRING

Encoding rules:
- BOOL: `0x00` for false, `0x01` for true.
- INT8/16/32/64: convert to unsigned by XOR with sign bit, then store big-endian.
  - Example INT32: `u = v ^ 0x80000000`, encode `u` big-endian.
- UINT8/16/32/64: store big-endian.
- FLOAT32/64: IEEE 754; to make sortable:
  - If sign bit = 1 (negative), invert all bits.
  - Else flip sign bit to 1.
  - Store big-endian of the transformed bits.
- DECIMAL128: scale to integer using column scale, store as signed 128-bit two's complement.
  - Apply sign-bias by XOR with `0x8000...` (top bit) then store big-endian 16 bytes.
- DATE: days since 1970-01-01 as INT32 with sign-bias and big-endian.
- TIME: microseconds since midnight as UINT64 big-endian.
- TIMESTAMP: microseconds since 1970-01-01 UTC as INT64 with sign-bias and big-endian.
- UUID: 16 bytes big-endian (network order). Comparison is bytewise.
- BINARY: raw bytes; comparison is lexicographic by bytes.
- STRING: UTF-8 bytes; if collation is binary, compare bytes.
  - If collation is non-binary, obtain collation sort key from collation module and store that as `value_bytes`.

NULL ordering:
- If `null_flag=1`, `value_bytes` is empty and `len=0`.
- For `nulls_first`, treat NULL as lowest possible segment.
- For `nulls_last`, treat NULL as highest possible segment.

Descending order:
- If `desc_sort_mode=invert_compare`, comparator result is inverted for this segment.
- If `desc_sort_mode=bytewise_complement`, each `value_bytes` is bitwise complemented before storage; comparison uses ascending order.

### MGA Visibility Algorithm (Firebird-Compatible)
Given a row version with `create_txid` and `delete_txid` and snapshot `S`:
- `S.active` = set of active txids at snapshot creation.
- `S.low` = oldest active txid (OAT).
- `S.high` = next transaction id at snapshot creation.

Visibility rules:
1. If `create_txid == current_txid`, the version is visible unless `delete_txid == current_txid`.
2. If `create_txid` is in `S.active` or `create_txid >= S.high`, the version is not visible.
3. If TIP state of `create_txid` is not COMMITTED, the version is not visible.
4. If `delete_txid == 0`, the version is visible.
5. If `delete_txid == current_txid`, the version is not visible.
6. If `delete_txid` is in `S.active` or `delete_txid >= S.high`, the version is visible.
7. If TIP state of `delete_txid` is COMMITTED, the version is not visible.
8. Otherwise, the version is visible.

### Security and Policy Enforcement
1. Apply row, column, and domain security after candidate TIDs are produced by the index.
2. If policy evaluation requires non-indexed columns, fetch the base row before returning a result.
3. Index-only scans are permitted only when policy evaluation can be completed from indexed columns and system metadata.
### Latching and Locking
- Page latches: shared for read, exclusive for write.
- Latch order: root to leaf, hold at most two latches.
- Unique key locks: resource id = `index_uuid || hash(key_bytes)`.
- Online maintenance uses `LOCK_MAINTENANCE_SHARED` (DML) and `LOCK_MAINTENANCE_EXCLUSIVE` (swap).

### Error Codes (All Indexes)
- `SB_ERR_INDEX_PARAM`: invalid option or parameter.
- `SB_ERR_DUPLICATE_KEY`: unique constraint violation.
- `SB_ERR_CORRUPT_INDEX`: checksum or structural error.
- `SB_ERR_LOCK_TIMEOUT`: lock wait timeout.
- `SB_ERR_DEADLOCK`: deadlock detected.
- `SB_ERR_SCHEMA_MISMATCH`: table schema does not match index requirements.

### Metrics Updates (Required)
Every index operation must update metrics:
- `index_usage`: `scan_count`, `tuple_read`, `tuple_returned`, `blocks_read`, `blocks_hit`, `total_time_ns`.
- `index_contention`: wait counts and durations when locks/latches block.
- `index_storage`: `page_count`, `bytes_used`, `bytes_allocated`, `fragmentation_ratio` after maintenance.

### Crash Recovery
- No WAL is used. If an index is detected corrupt or incomplete at startup, mark it `invalid` and require `IDX_REBUILD`.

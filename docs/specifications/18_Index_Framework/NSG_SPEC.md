# NSG (Navigating Spreading-out Graph) Specification

## Purpose
Define NSG graph-based ANN index with fixed out-degree and navigational entry point.

## Dependencies
- 18_Index_Framework/INDEX_ARCHITECTURE.md
- 18_Index_Framework/VECTOR_FLAT_SPEC.md

## Page Types
- `PAGE_TYPE_NSG_META`
- `PAGE_TYPE_NSG_NODE`

## Canonical Options
- `R` (u16): max out-degree per node (default 32).
- `L` (u16): search candidate pool (default 200).
- `C` (u16): build candidate pool size (default 200).
- `search_L` (u16): query candidate pool (default 100).
- `metric`: `l2`, `cosine`, `inner_product`.
- `vector_dim` (u16).
- `seed` (u64).

## Meta Page Layout
- `index_uuid` (UUID)
- `index_type` (enum)
- `vector_dim` (u16)
- `metric` (u8)
- `R` (u16)
- `L` (u16)
- `C` (u16)
- `search_L` (u16)
- `seed` (u64)
- `entry_point` (u32)
- `node_count` (u32)

## Node Layout
- `node_id` (u32)
- `flags` (u8) bit0 = deleted
- `reserved` (u8)
- `vector` (`vector_dim` * 4 bytes)
- `neighbor_count` (u16)
- `neighbors[]` (u32 list, size <= R)

## Build Algorithm
1. Normalize vectors if `metric=cosine`.
2. Build an initial kNN graph using NN-Descent:
   - Initialize each node's neighbor list with random nodes size `C`.
   - Repeat until convergence (no neighbor updates in an iteration):
     - For each node, compare current neighbors with neighbors of neighbors.
     - Keep the closest `C` candidates.
3. Select entry point:
   - Choose the medoid (vector minimizing average distance to others) using a sample of 1024 vectors; exact search if row_count < 1024.
4. Prune to NSG:
   - For each node, run a greedy search from entry point to find candidates.
   - Apply monotonic pruning: keep candidate `c` only if for all kept neighbors `s`, `dist(c,s) > dist(c,node)`.
   - Keep at most `R` neighbors.
5. Ensure connectivity:
   - BFS from entry point; if any node unreachable, connect it to the closest reachable node.

## Search Algorithm
1. Normalize query if `metric=cosine`.
2. Greedy search from entry point with candidate pool size `search_L`:
   - Maintain a candidate min-heap and a visited set.
   - Expand the closest unexpanded node, insert its neighbors if not visited.
3. Return top K closest nodes after visibility checks.

## Insert
1. Normalize vector if `metric=cosine`.
2. Run greedy search from `entry_point` to collect `L` candidates.
3. Apply monotonic pruning and keep up to `R` neighbors.
4. Insert edges from the new node to selected neighbors.
5. For each neighbor, insert reciprocal edge if it improves neighbor list; prune to `R`.
6. If the node is disconnected, connect it to the closest reachable node from `entry_point`.

## Delete
1. Mark node `deleted` in its page flags.
2. Search must ignore deleted nodes.
3. Deleted nodes are removed during rebuild or compaction.

## Update/Delete
- Updates are Delete followed by Insert.
- Deleted nodes are reclaimed during rebuild or compaction.

## Error Handling
- Invalid `R` or `L` values yield `SB_ERR_INDEX_PARAM`.
- Corrupt node pages mark index invalid.

## Test Contract
- Connectivity: all nodes reachable from entry point.
- Degree bound: neighbor_count <= R.
- Recall increases with `search_L`.

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


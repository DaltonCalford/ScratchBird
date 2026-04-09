# Neo4j Point Index Specification

## Purpose
Define point index for spatial properties using space-filling curves.

## Dependencies
- 18_Index_Framework/BTREE_SPEC.md

## Canonical Options
- `srid` (u32): spatial reference id.
- `dims` (u8): 2 or 3.
- `space_filling_curve` (enum: zorder, hilbert) (alias of `cell_curve`).
 - `cell_semantics` (enum: space_filling_curve) (default space_filling_curve).
 - `cell_curve` (enum: zorder, hilbert) (default hilbert).
 - `bits_per_dim` (u8): bits per dimension (default floor(64 / dims); max 31 for 2D, max 21 for 3D).
 - `hilbert_variant` (enum):
   - 2D: `skilling_xy`, `skilling_yx`
   - 3D: `skilling_xyz`, `skilling_xzy`, `skilling_yxz`, `skilling_yzx`, `skilling_zxy`, `skilling_zyx`
- `axis_invert_mask` (u8, 0..7): bit0=x, bit1=y, bit2=z.

If `cell_semantics != space_filling_curve`, index creation must fail with `SB_ERR_INDEX_PARAM`.

## Key Encoding
1. Normalize coordinates to unsigned integers per dimension using SRID bounds.
2. Quantize each dimension to `bits_per_dim`.
3. Compute `curve_key` using the selected `cell_curve`:
   - `zorder`: bitwise interleave.
   - `hilbert`: use the Skilling algorithm defined below with the selected variant.
4. Key is `(curve_key, tid)`.
5. `curve_key` is stored as unsigned big-endian bytes; if `dims * bits_per_dim < 64`, left-pad with zeros to 64 bits.

## On-Disk Layout
This index uses the B-tree page layout defined in `BTREE_SPEC.md` and the embedded B-tree section below.

Leaf entries store `(curve_key, tid)` and internal entries store `(curve_key, child_page_id)` separators.

## Z-Order Interleaving (Morton)
1. For `dims=2`, interleave bits `x` and `y` MSB-first:
2. `curve_key = concat( x[bits-1], y[bits-1], x[bits-2], y[bits-2], ... )`.
3. For `dims=3`, interleave bits `x`, `y`, `z` MSB-first:
4. `curve_key = concat( x[bits-1], y[bits-1], z[bits-1], x[bits-2], y[bits-2], z[bits-2], ... )`.

## Hilbert Variant Definitions
Each variant is the Skilling algorithm with an explicit axis permutation.
2D permutation table:
- `skilling_xy`: axes = [x, y]
- `skilling_yx`: axes = [y, x]
3D permutation table:
- `skilling_xyz`: axes = [x, y, z]
- `skilling_xzy`: axes = [x, z, y]
- `skilling_yxz`: axes = [y, x, z]
- `skilling_yzx`: axes = [y, z, x]
- `skilling_zxy`: axes = [z, x, y]
- `skilling_zyx`: axes = [z, y, x]
After permutation, apply axis inversion:
- If `axis_invert_mask & 0x1 != 0`, set `x = (2^bits_per_dim - 1) - x`.
- If `axis_invert_mask & 0x2 != 0`, set `y = (2^bits_per_dim - 1) - y`.
- If `dims=3` and `axis_invert_mask & 0x4 != 0`, set `z = (2^bits_per_dim - 1) - z`.

## Hilbert Mapping Algorithm (Skilling, Deterministic)
Input: integer axes `X[0..n-1]`, `n=dims`, `bits=bits_per_dim`.
Output: `curve_key` as `n*bits` interleaved bits (MSB-first).
1. Let `M = 1 << (bits - 1)`.
2. Let `Q = M`.
3. While `Q > 1`:
4. `P = Q - 1`.
5. Let `x0 = X[0]`.
6. If `(x0 & Q) != 0`, then `x0 = x0 XOR P`.
7. For `i = 1..n-1`:
8. Let `xi = X[i]`.
9. If `(xi & Q) != 0` then `x0 = x0 XOR P`.
10. Else:
11. `T = (x0 XOR xi) & P`.
12. `x0 = x0 XOR T`.
13. `X[i] = xi XOR T`.
14. Set `X[0] = x0`.
15. `Q = Q >> 1`.
16. Gray encode: for `i = 1..n-1`, set `X[i] = X[i] XOR X[i-1]`.
17. `T = 0`.
18. `Q = M`.
19. While `Q > 1`:
20. If `(X[n-1] & Q) != 0`, set `T = T XOR (Q - 1)`.
21. `Q = Q >> 1`.
22. For `i = 0..n-1`, set `X[i] = X[i] XOR T`.
23. Interleave `X[0..n-1]` MSB-first to form `curve_key`:
24. `curve_key = 0`.
25. For `b = bits-1 .. 0`:
26. For `i = 0..n-1`:
27. `curve_key = (curve_key << 1) | ((X[i] >> b) & 1)`.

## Dialect Constraints
- Neo4j emulation: `cell_curve` and `hilbert_variant` are fixed and not user-configurable:
  - `cell_curve = hilbert`
  - `hilbert_variant = skilling_xy` for 2D, `skilling_xyz` for 3D
  - `axis_invert_mask = 0`
- Native ScratchBird: all `cell_curve` and `hilbert_variant` options are allowed.

## Coordinate Normalization (Deterministic)
1. Each SRID provides `min` and `max` bounds per dimension from the SRID catalog.
2. For each coordinate `x`, compute `u = clamp((x - min) / (max - min), 0, 1)`.
3. Quantize to integer `q = floor(u * (2^bits_per_dim - 1))`.
4. Use `q` for bit interleaving.

## Z-Order Interleaving (Morton)
1. For `dims=2`, interleave bits: `z = interleave_bits(x, y)`.
2. For `dims=3`, interleave bits: `z = interleave_bits(x, y, z)`.
3. `interleave_bits` takes the highest bit first and writes bits in round-robin order.

## Hilbert Mapping (2D)
1. For `dims=2`, use the same Skilling algorithm with `n=2`.
2. `hilbert_variant` permutations reduce to `xy` or `yx` ordering and apply `axis_invert_mask` (default 0).

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

## Build Algorithm
1. Capture build snapshot `S1` with MGA rules.
2. Initialize meta and root pages and set index state to `building`.
3. If this index requires training, dictionary build, or model materialization, run that phase first and persist its outputs.
4. Scan base table in physical order. For each row version visible in `S1`, derive all index entries and call `Insert` for each entry.
5. Populate `index_stats` with row_count, distinct_count, null_frac, min_value, max_value, and histogram buckets as applicable.
6. Validate structural invariants and set index state to `active`.

## Insert
1. Encode point value to `curve_key`.
2. Insert `(curve_key, tid)` into the B-tree.

## Delete
1. Encode point value to `curve_key`.
2. Remove `(curve_key, tid)` from the B-tree.

## Search
- Range and distance queries are decomposed into curve key ranges.
- Scan ranges and post-filter by exact geometry.

## Test Contract
- Point queries return correct matches.
- Distance predicates are filtered correctly.

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

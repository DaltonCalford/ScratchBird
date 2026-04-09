# IVF Variant Specifications

## Purpose
Define storage formats and distance computation for IVF_FLAT, BIN_IVF_FLAT, IVF_PQ, IVF_SQ8, and IVF_SQ8_HYBRID.

## Dependencies
- 18_Index_Framework/IVF_SPEC.md
- 18_Index_Framework/VECTOR_FLAT_SPEC.md

## Common Rules
- Variant is chosen by `index_type` at creation.
- Each list entry begins with `tid` (16 bytes) and `flags` (u8), followed by variant payload.
- Payload length is fixed per variant.

## On-Disk Layout
IVF variants use the same page types as `IVF_SPEC.md`:
- `PAGE_TYPE_IVF_META`
- `PAGE_TYPE_IVF_CENTROID`
- `PAGE_TYPE_IVF_LIST`

IVF list entry format:
- `tid` (16 bytes)
- `flags` (u8)
- `payload_bytes` (variant-specific)

IVF_PQ centroid page extension:
Append after centroid array in `PAGE_TYPE_IVF_CENTROID` as a fixed block with fields:
`pq_m` u16, `pq_bits` u8, `pq_ksub` u16, `pq_subdim` u16, `pq_codebook` float32 array of length `pq_m * pq_ksub * pq_subdim` in row-major order.

IVF_SQ8 centroid page extension:
Append after centroid array in `PAGE_TYPE_IVF_CENTROID` as a fixed block with fields:
`sq_valid` u8, `sq_dim` u16, `sq_min` float32 array of length `sq_dim`, `sq_max` float32 array of length `sq_dim`.

## Build Algorithm
1. Capture build snapshot `S1` with MGA rules.
2. Initialize meta and root pages and set index state to `building`.
3. If this index requires training, dictionary build, or model materialization, run that phase first and persist its outputs.
4. Scan base table in physical order. For each row version visible in `S1`, derive all index entries and call `Insert` for each entry.
5. Populate `index_stats` with row_count, distinct_count, null_frac, min_value, max_value, and histogram buckets as applicable.
6. Validate structural invariants and set index state to `active`.

## Insert
1. This spec defines the payload encoding used by IVF insert operations.
2. During IVF insert, encode the vector into the variant payload defined below and store it in the list entry.

## Search
1. Validate query dimension and metric compatibility.
2. Select candidate IVF lists using the IVF coarse centroids.
3. For each candidate list entry:
4. IVF_FLAT: compute exact distance from query to stored float vector.
5. BIN_IVF_FLAT: compute hamming, jaccard, or tanimoto distance on bit vectors.
6. IVF_PQ: precompute distance table for each subquantizer, then sum table entries for the code bytes.
7. IVF_SQ8: dequantize using `sq_min` and `sq_max` and compute exact distance.
8. IVF_SQ8_HYBRID: use the same scoring as IVF_SQ8; centroid selection follows the deterministic GPU rule:
   - If `query_count >= gpu_search_threshold` and GPU available, use GPU for centroid distance evaluation.
   - Otherwise use CPU centroid distance evaluation.
9. Maintain a max-heap of size K and return the top K closest entries after MGA and security checks.

## Delete
1. Deletion uses IVF list entry delete semantics; no variant-specific deletion logic is required.

## IVF_FLAT
- Payload: raw float32 vector (`vector_dim * 4` bytes).
- Distance: same as VECTOR_FLAT.
- Insert: store normalized vectors if `metric=cosine`.

## BIN_IVF_FLAT
- Payload: raw binary vector (`vector_dim_bits / 8` bytes).
- Distance: hamming/jaccard/tanimoto as in VECTOR_FLAT.

## IVF_PQ (Product Quantization)
### Parameters
- `m`: number of subquantizers (1..64).
- `bits_per_code`: 4 or 8.
- `ksub = 2^bits_per_code`.
- `vector_dim` must be divisible by `m`.
- `subdim = vector_dim / m`.

### Codebook Training
1. From the training sample set, extract `subdim`-sized subvectors for each of the `m` positions.
2. For each subquantizer `j` in `0..m-1`:
   - Run K-means with `ksub` centroids on the subvectors.
   - Store centroids as `pq_codebook[j][ksub][subdim]`.
3. Store codebooks in `PAGE_TYPE_IVF_CENTROID` after the IVF centroids:
   - `pq_m` (u16), `pq_bits` (u8), `pq_ksub` (u16), `pq_subdim` (u16)
   - Codebook array `pq_codebook` in row-major order.

### Encoding
- For vector `v`, split into `m` subvectors.
- For each subvector, choose nearest codebook centroid by L2.
- Store code indices as bytes (1 byte each if `bits_per_code=8`, packed nibbles if `bits_per_code=4`).
- Payload size:
  - `m` bytes for 8-bit codes.
  - `ceil(m/2)` bytes for 4-bit codes.

### Distance Computation
- Precompute distance table for the query:
  - For each subquantizer `j`, compute distance from query subvector to all `ksub` centroids.
- For each entry, sum lookup distances for its codes.

## IVF_SQ8 (Scalar Quantization)
### Parameters
- `vector_dim` fixed.
- `sq_bits = 8`.
- Quantization uses per-dimension min/max computed from training set.

### Training
1. For each dimension `d`, compute `min_d` and `max_d` from training vectors.
2. Store arrays `sq_min[d]` and `sq_max[d]` in `PAGE_TYPE_IVF_CENTROID` after centroids:
   - `sq_valid` (u8), `sq_dim` (u16)
   - `sq_min` (float32 * dim)
   - `sq_max` (float32 * dim)

### Encoding
- For each dimension `d`:
  - If `max_d == min_d`, store 0.
  - Else `q = round((v[d] - min_d) * 255 / (max_d - min_d))`, clamp to [0,255].
- Payload size = `vector_dim` bytes.

### Distance Computation
- Dequantize on the fly:
  - `v[d] = min_d + q * (max_d - min_d) / 255`.
- Compute metric distance to query vector.

## IVF_SQ8_HYBRID
- Same storage format as IVF_SQ8.
- Search path selection:
  - If `query_count >= gpu_search_threshold` and GPU available, use GPU for centroid selection.
  - Else CPU path identical to IVF_SQ8.
- GPU path only accelerates centroid distance evaluation; list scanning remains CPU in Alpha.

## Error Handling
- IVF_PQ rejects `vector_dim % m != 0`.
- IVF_PQ rejects `bits_per_code` not in {4,8}.
- IVF_SQ8 rejects missing `sq_min/sq_max` tables.

## Test Contract
- IVF_PQ: round-trip encoding/decoding error under tolerance.
- IVF_SQ8: dequantized vector within expected error bounds.
- IVF_SQ8_HYBRID: CPU/GPU selection rule works and yields identical results on same inputs.

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

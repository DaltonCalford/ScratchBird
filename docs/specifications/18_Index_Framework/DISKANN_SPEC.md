# DiskANN Specification

## Purpose
Define a disk-backed ANN graph index optimized for large datasets with limited RAM.

## Dependencies
- 18_Index_Framework/INDEX_ARCHITECTURE.md
- 18_Index_Framework/VECTOR_FLAT_SPEC.md

## Page Types
- `PAGE_TYPE_DISKANN_META`
- `PAGE_TYPE_DISKANN_GRAPH`
- `PAGE_TYPE_DISKANN_VECTOR_BLOCK`

## Canonical Options
- `max_degree` (u16): max out-degree per node (default 64).
- `build_L` (u16): candidate pool during build (default 128).
- `search_L` (u16): beam width during search (default 64).
- `beam_width` (u16): max concurrent disk reads (default 8).
- `pq_bytes` (u16): bytes per vector in PQ store (0 = store raw vectors).
- `alpha` (float32): robust prune factor (default 1.2).
- `training_sample_size` (u32): max training vectors for PQ (default 100000).
- `entrypoint_strategy` (enum: `medoid_sample`, `first_vector`) (default `medoid_sample`).
- `metric`: `l2`, `cosine`, `inner_product`.
- `vector_dim` (u16).
- `seed` (u64).
- `use_disk_storage` (bool, default true).

## Meta Page Layout
- `index_uuid` (UUID)
- `index_type` (enum)
- `vector_dim` (u16)
- `metric` (u8)
- `max_degree` (u16)
- `build_L` (u16)
- `search_L` (u16)
- `beam_width` (u16)
- `pq_bytes` (u16)
- `alpha` (float32)
- `training_sample_size` (u32)
- `entrypoint_strategy` (u8)
- `entry_point` (u32)
- `node_count` (u32)
- `vector_block_head` (u32)

### Meta Extension Layout (PQ)
If `pq_bytes > 0`, append this structure immediately after the fixed meta fields:
- `pq_magic` (u32) = 0x44535150 (`DSPQ`)
- `pq_m` (u16) = `pq_bytes`
- `pq_bits` (u8) = 8
- `pq_subdim` (u16) = `vector_dim / pq_m`
- `pq_ksub` (u16) = 256
- `pq_reserved` (u16) = 0
- `pq_codebook` (float32 array) size = `pq_m * pq_ksub * pq_subdim`
  - Stored in row-major order: `pq_codebook[j][c][d]` for `j in [0,pq_m)`, `c in [0,255]`, `d in [0,pq_subdim)`.

## Graph Page Layout (`PAGE_TYPE_DISKANN_GRAPH`)
- `node_id` (u32)
- `flags` (u8) bit0 = deleted
- `neighbor_count` (u16)
- `neighbors[]` (u32 list, size <= max_degree)
- `vector_ref` (u64) (block_id << 32 | offset)

Graph page serialization:
- Page uses a slotted layout with an item directory in the page header.
- Each item points to a `NodeRecord`:
  - `record_len` (u16)
  - `node_id` (u32)
  - `flags` (u8)
  - `neighbor_count` (u16)
  - `vector_ref` (u64)
  - `neighbors[neighbor_count]` (u32 array)
- Neighbor arrays are stored sorted by increasing distance to `node_id`.
- Ties are broken by `neighbor_id` ascending.

## Vector Block Layout (`PAGE_TYPE_DISKANN_VECTOR_BLOCK`)
- `block_id` (u32)
- `entry_count` (u16)
- `capacity` (u16)
- `next_block_page_id` (u32)
- Entries:
  - `vector_id` (u32)
  - `payload`:
    - raw vector bytes if `pq_bytes=0` (dim * 4 bytes)
    - PQ code bytes if `pq_bytes>0` (exact `pq_bytes` length)

Vector block capacity formula:
- `entry_size = 4 + (pq_bytes == 0 ? vector_dim * 4 : pq_bytes)`
- `capacity = floor((page_payload_bytes - block_header_bytes) / entry_size)`
- `block_header_bytes` = fixed header bytes listed above.

## Deterministic RNG
All randomized steps use the following generator, seeded by `seed`:
```
state = seed; if state == 0: state = 0x9E3779B97F4A7C15
next_u64():
  state ^= state >> 12
  state ^= state << 25
  state ^= state >> 27
  return state * 0x2545F4914F6CDD1D
rand_u32(): return (next_u64() >> 32)
rand_float01(): return rand_u32() / 4294967296.0
```
Sampling `k` unique vectors uses Fisher-Yates shuffle with `rand_u32()`.

## Distance Functions
- `l2`: `dist(a,b) = sum_i (a[i]-b[i])^2`
- `inner_product`: `dist(a,b) = -sum_i (a[i]*b[i])`
- `cosine`: normalize both vectors to unit length, then `dist(a,b) = 1 - dot(a,b)`

## Build Algorithm (Vamana Deterministic)
1. Normalize vectors if `metric=cosine`.
2. Assign `node_id` sequentially in physical row scan order.
3. Determine `entry_point`:
   - If `entrypoint_strategy=first_vector`, use `node_id=0`.
   - If `entrypoint_strategy=medoid_sample`, sample `s = min(4096, node_count)` vectors using RNG, compute average distance per sample, choose the sample with minimum average distance.
4. Initialize each node with an empty neighbor list.
5. For each node `v` in `node_id` order:
   - Compute `candidate_set = GreedySearch(v, entry_point, build_L)`.
   - `neighbors_v = RobustPrune(v, candidate_set, max_degree, alpha)`.
   - Write `neighbors_v` into `v`'s NodeRecord.
   - For each neighbor `u` in `neighbors_v`:
     - Read `neighbors_u`, append `v`, then re-run `RobustPrune(u, neighbors_u, max_degree, alpha)`.
     - Write updated `neighbors_u` back to `u`'s NodeRecord.
6. Persist all graph pages and vector blocks.

### GreedySearch(v, entry_point, L)
Input: query vector = `v`, starting node `entry_point`, candidate size `L`.
1. Initialize `visited` bitset.
2. `candidate_minheap` (distance asc), `result_maxheap` (distance desc, size L).
3. Insert `entry_point` into both heaps with its distance to `v`.
4. While `candidate_minheap` not empty:
   - Pop `(d,u)` from `candidate_minheap`.
   - If `visited[u]` continue; mark visited.
   - If `result_maxheap` size < L, push `(d,u)`.
   - Else if `d < worst(result_maxheap)`, replace worst with `(d,u)`.
   - If `d > worst(result_maxheap)` and `result_maxheap` size == L, continue (no closer node can be found via `u`).
   - For each neighbor `n` of `u`:
     - If not visited, compute distance `dn` and push `(dn,n)` into `candidate_minheap`.
5. Return the node ids in `result_maxheap`.

### RobustPrune(v, candidates, R, alpha)
1. Sort candidates by increasing distance to `v` (tie by node_id).
2. Initialize `result = []`.
3. For each candidate `c` in order:
   - `keep = true`.
   - For each `s` in `result`:
     - If `dist(c,s) <= alpha * dist(c,v)`, set `keep=false` and break.
   - If `keep`, append `c` to `result`.
   - If `len(result) == R`, break.
4. Return `result`.

## Search Algorithm (Disk-Aware Beam)
1. Normalize query if `metric=cosine`.
2. Initialize `visited` bitset and `candidate_minheap` with `entry_point`.
3. Initialize `result_maxheap` (size K).
4. Initialize `io_queue` and `inflight = 0`.
5. While `candidate_minheap` not empty and visited_count < search_L:
   - Pop `(d,u)` from `candidate_minheap`.
   - If visited, continue; mark visited.
   - If `result_maxheap` size < K, push `(d,u)`; else if `d < worst(result_maxheap)` replace worst.
   - Enqueue `u` for expansion in `io_queue`.
   - While `inflight < beam_width` and `io_queue` not empty:
     - Dequeue `x` and issue disk read for its neighbor list and vectors.
     - `inflight += 1`.
   - For each completed IO for node `x`:
     - `inflight -= 1`.
     - For each neighbor `n` of `x`:
       - If visited, continue.
       - Compute distance using PQ or raw vectors.
       - Push `(dn,n)` into `candidate_minheap`.
6. Return top K nodes from `result_maxheap` after MGA and security checks.

## PQ Encoding (Optional)
If `pq_bytes > 0`:
1. Set `pq_m = pq_bytes`, `pq_bits = 8`, `ksub = 256`.
2. Require `vector_dim % pq_m == 0`; `subdim = vector_dim / pq_m`.
3. Training:
   - Sample `min(training_sample_size, node_count)` vectors using RNG.
   - For each subquantizer `j` in `0..pq_m-1`:
     - Extract subvector `j` from each training vector.
     - Run K-means with `ksub` centroids, max 25 iterations, initialization = kmeans++ using RNG.
     - Store centroids as `pq_codebook[j][ksub][subdim]`.
4. Encoding:
   - For each vector, for each subvector `j`, pick nearest centroid and store its id as one byte.
5. Distance (ADC):
   - Precompute `dist_table[j][c] = dist(query_subvec_j, pq_codebook[j][c])`.
   - For each code, sum `dist_table[j][code[j]]`.
6. If `use_disk_storage=false`, also keep raw vectors in memory for reranking.

## Insert
1. Normalize vector if `metric=cosine`.
2. Store vector payload in the next available `PAGE_TYPE_DISKANN_VECTOR_BLOCK` entry and record `vector_ref`.
3. Run greedy search from `entry_point` to collect `build_L` candidates.
4. Prune candidates using alpha pruning with `alpha=1.2` and keep up to `max_degree` neighbors.
5. Insert edges from the new node to selected neighbors.
6. For each neighbor, insert a reciprocal edge if it improves the neighbor list; prune to `max_degree`.
7. If `entry_point` is deleted or unset, update it to the inserted node.

## Delete
1. Mark node `deleted` in its graph entry.
2. Leave edges in neighbor lists; search must ignore deleted nodes.
3. Deleted nodes are removed during rebuild or compaction.

## Compaction and Rebuild Rules
- If `deleted_nodes / node_count >= 0.2`, schedule `IDX_REBUILD`.
- `IDX_REBUILD` replays Build Algorithm with only visible, non-deleted rows.

## Update/Delete
- Updates are Delete followed by Insert.
- Deleted nodes are reclaimed during rebuild or compaction.

## Error Handling
- Invalid `pq_bytes` or mismatch with `vector_dim` yields `SB_ERR_INDEX_PARAM`.
- Corrupt vector blocks mark index invalid.

## Test Contract
- Graph degree <= max_degree.
- Disk search results within recall threshold vs brute-force.

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

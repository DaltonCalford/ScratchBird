# Cassandra SAI Index Specification

## Purpose
Define Storage-Attached Index (SAI) behavior with per-SSTable segments.

## Dependencies
- 18_Index_Framework/INDEX_ARCHITECTURE.md
- 18_Index_Framework/TRIE_SPEC.md
- 18_Index_Framework/STL_SORT_SPEC.md
- 18_Index_Framework/HNSW_SPEC.md

## Canonical Options
- `index_kind` (enum: string, numeric, vector).
- `segment_bytes` (u32): target size of index segment files.
- `posting_block_rows` (u16): postings block size.

## Storage Model
- Each SSTable/run has one SAI segment per indexed column.
- Segments are immutable and compacted alongside SSTables.

## Page Types
- `PAGE_TYPE_SAI_META`
- `PAGE_TYPE_SAI_TERM_DICT`
- `PAGE_TYPE_SAI_POSTINGS`
- `PAGE_TYPE_SAI_RANGE`
- `PAGE_TYPE_SAI_VECTOR`

## On-Disk Layout
All SAI pages use slotted heap layout with the `IndexPageHeader` defined in `05_Page_Taxonomy_and_Binary_Layouts/INDEX_PAGE_BASE_LAYOUT.md`.

SAI meta record fields:
- `segment_id` u32
- `index_kind` u8
- `min_key_len` u16
- `min_key_bytes`
- `max_key_len` u16
- `max_key_bytes`
- `row_count` u32
- `term_dict_root_page` u32
- `postings_root_page` u32
- `range_root_page` u32
- `vector_root_page` u32

SAI term dictionary entry fields:
- `term_len` u16
- `term_bytes`
- `term_id` u64
- `doc_freq` u32
- `postings_head_page_id` u32

SAI postings record fields:
- `term_id` u64
- `doc_id_base` u64
- `doc_id_count` u16
- `doc_id_delta[doc_id_count]` u32
- For each doc in order, `pos_count` u16 then `pos_delta[pos_count]` u16

SAI range record fields:
- `key_len` u16
- `key_bytes`
- `tid` (16 bytes)

SAI range block index (stored in special area):
- `block_count` u16
- For each block, `block_first_key_len` u16, `block_first_key_bytes`, `block_last_key_len` u16, `block_last_key_bytes`, `block_page_offset` u32

SAI vector pages:
- `PAGE_TYPE_SAI_VECTOR` uses the HNSW node record layout from `HNSW_SPEC.md` with vectors stored as float32 arrays.

## Segment Layout
- Meta:
  - `segment_id` (u32)
  - `index_kind` (u8)
  - `min_key`, `max_key` (encoded)
  - `row_count` (u32)

## String Index
- Tokenize values using `keyword` analyzer (full value).
- Build term dictionary and postings lists.

## Embedded String Segment (Required)
1. Use a trie keyed by UTF-8 bytes of the term.
2. Each terminal node stores a postings list of TIDs for that term.
3. Posting list format: `doc_id_count`, then `(doc_id, pos_count, positions)` with delta encoding.

## Numeric Index
- Build sorted run of `(key, tid)` with block min/max.

## Embedded Numeric Segment (Required)
1. Store entries in sorted order by `key_bytes`.
2. Build a block index with `block_min`, `block_max`, and `block_offset` every `posting_block_rows`.
3. Range queries scan only blocks whose min/max overlap the predicate.

## Vector Index
- Build HNSW graph per segment using segment vectors only.

## Embedded Vector Segment (Required)
1. Each segment stores vectors and HNSW graph with `M` and `ef_construction` options.
2. Search uses HNSW best-first at level 0 and returns top K for the segment.

## Query
- For each SSTable segment:
  - Apply segment-level min/max pruning.
  - Use segment index to retrieve candidate TIDs.
- Merge candidate TIDs across segments and apply visibility checks.

## Search Algorithm
1. Identify relevant segments using min/max pruning.
2. For string predicates, use the trie to resolve term postings for each segment.
3. For numeric predicates, scan block index and then data blocks for matching keys.
4. For vector predicates, run HNSW search within each segment and merge top K across segments.
5. Union or intersect candidate sets for conjunctive predicates.
6. Apply MGA visibility and security policies before returning rows.

## Build Algorithm
1. Capture build snapshot `S1` with MGA rules.
2. Initialize meta and root pages and set index state to `building`.
3. If this index requires training, dictionary build, or model materialization, run that phase first and persist its outputs.
4. Scan base table in physical order. For each row version visible in `S1`, derive all index entries and call `Insert` for each entry.
5. Populate `index_stats` with row_count, distinct_count, null_frac, min_value, max_value, and histogram buckets as applicable.
6. Validate structural invariants and set index state to `active`.

## Insert
1. Insert updates a mutable in-memory segment for the active memtable.
2. For `index_kind=string`, append token entries to the term dictionary and postings.
3. For `index_kind=numeric`, append `(key, tid)` to the mutable range run.
4. For `index_kind=vector`, add the vector to the mutable HNSW graph segment.
5. If the mutable segment exceeds `segment_bytes`, flush it to an immutable SAI segment.

## Delete
1. Append a tombstone `(tid, deleted=true)` to the mutable segment for all index kinds.
2. During query, tombstones remove matching TIDs from candidate lists.
3. Tombstones are purged during segment compaction.

## Update/Delete
- Updates append to new SSTable segments; old segments removed on compaction.

## Error Handling
- Reject unsupported `index_kind` for column type.

## Test Contract
- Segment pruning works.
- Queries merge across segments correctly.

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

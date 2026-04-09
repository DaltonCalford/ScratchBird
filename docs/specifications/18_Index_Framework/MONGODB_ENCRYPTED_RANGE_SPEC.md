# MongoDB Encrypted Range Index Specification

## Purpose
Define encrypted range index using deterministic prefix tokens with post-filtering.

## Dependencies
- 18_Index_Framework/INDEX_ARCHITECTURE.md
- 19_Security_Model (HMAC key management)
- 18_Index_Framework/INVERTED_SPEC.md

## Canonical Options
- `range_bits` (u8): bit width of normalized values (default 32).
- `token_levels` (u8): prefix step; store prefixes at lengths 1, 1+token_levels, ... (default 1).
- `token_hmac` (enum: hmac_sha256) (default hmac_sha256).
- `range_bucket_policy` (enum: exact, coarse) (default exact).

## On-Disk Layout
This index uses the inverted index page types and layouts from `INVERTED_SPEC.md`.

Dictionary entries store `token` bytes as the term key. Postings entries store TIDs using the standard postings record format.

## Normalization
- Supported input types: signed/unsigned integer, fixed-precision DECIMAL. FLOAT types are rejected.
- Convert values to unsigned integer `v` with width `range_bits`:
  - For signed integers: `v = value - MIN_INT_TYPE` (e.g. INT32 uses -2^31).
  - For unsigned integers: `v = value`.
  - For DECIMAL(p,s): `v = (value * 10^s) - MIN_DECIMAL`, where `MIN_DECIMAL = -10^p`.
- Values outside `[0, 2^range_bits - 1]` are rejected.

## Token Generation
- Represent `v` in binary with `range_bits` bits.
- For each prefix length `p` in {1, 1+token_levels, ... , range_bits}:
  - `prefix = first p bits of v`.
  - `token = HMAC(key_range, prefix || p)`.
- Insert `(token, tid)` into inverted postings.

## Embedded Inverted Index Implementation (Required)
1. Dictionary maps `token` to `token_id` and `doc_freq`.
2. Posting list stores `(doc_id, positions)` where `doc_id` is the TID (16 bytes).
3. Posting lists are stored in pages with `(doc_id_count, doc_id, pos_count, positions)` format.

## Build Algorithm
1. Capture build snapshot `S1` with MGA rules.
2. Initialize meta and root pages and set index state to `building`.
3. If this index requires training, dictionary build, or model materialization, run that phase first and persist its outputs.
4. Scan base table in physical order. For each row version visible in `S1`, derive all index entries and call `Insert` for each entry.
5. Populate `index_stats` with row_count, distinct_count, null_frac, min_value, max_value, and histogram buckets as applicable.
6. Validate structural invariants and set index state to `active`.

## Insert
1. Normalize value to integer `v` using rules above.
2. Generate prefix tokens.
3. Insert `(token, tid)` into the inverted postings list for each token.

## Search
1. For equality, compute tokens for the full `range_bits` prefix and fetch postings.
2. For range predicates, use the range tokenization algorithm below to get a set of prefix tokens.
3. Union postings for all tokens and de-duplicate TIDs.
4. Post-filter by decrypting the value and verifying the range predicate.
5. Apply MGA visibility and security policies before returning rows.

## Delete
1. Normalize value to integer `v`.
2. Generate prefix tokens.
3. Remove `(token, tid)` from each postings list.

## Range Query Tokenization
1. Normalize range `[l,r]` to integers `[vl, vr]`.
2. Compute minimal prefix cover of `[vl, vr]` using binary interval decomposition:
   - While `vl <= vr`:
     - Let `max_size = lowest_set_bit(vl)`.
     - Let `max_len = floor(log2(max_size))`.
     - Let `remaining = vr - vl + 1`.
     - Let `len = min(max_len, floor(log2(remaining)))`.
     - Emit prefix for interval `[vl, vl + 2^len - 1]`.
     - `vl += 2^len`.
3. For each prefix, if its length is not in stored `token_levels`, shorten to nearest stored length.
4. Compute tokens for each prefix and union posting lists.
5. Post-filter by decrypting actual value and checking `l <= value <= r`.

## Insert/Update/Delete
- Insert: generate tokens and append to postings.
- Update: delete old tokens and insert new ones.
- Delete: remove tokens.

## Error Handling
- Reject if `range_bits` > 64.
- Reject if HMAC key missing.

## Test Contract
- Range query returns correct results after post-filter.
- Token generation is deterministic for same value and key.

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

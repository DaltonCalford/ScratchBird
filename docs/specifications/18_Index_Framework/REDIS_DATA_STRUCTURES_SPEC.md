# Redis Data Structures Specification

## Purpose
Define Redis-like data structures implemented as specialized index types and page layouts. Persistence behavior is controlled by `redis.persistence` and `redis.flush_policy`.

## Dependencies
- 18_Index_Framework/INDEX_ARCHITECTURE.md
- 05_Page_Taxonomy_and_Binary_Layouts
- 10_GC_and_Sweep

## Canonical Options
- `redis.persistence` (enum: always, snapshot, never) default `always`.
- `redis.flush_policy` (enum: immediate, periodic) default `immediate`.
- `redis.flush_interval_ms` (u32) default `1000` (used when `redis.flush_policy=periodic`).
- `ttl_policy` (enum: lazy, eager) default `lazy`.

## Persistence Rules (Deterministic)
- `redis.persistence=always`:
  - Every write operation appends its changes to on-disk pages before returning success.
- `redis.persistence=snapshot`:
  - Writes update in-memory pages and are flushed to disk by the periodic flusher.
  - The flusher runs every `redis.flush_interval_ms` (default 1000) and persists all dirty Redis pages.
- `redis.persistence=never`:
  - Writes update only in-memory pages; no disk persistence is performed.
  - On restart, Redis structures are empty.
- `redis.flush_policy=immediate`:
  - Force flush of Redis pages at the end of each transaction when `redis.persistence=always` or `snapshot`.
- `redis.flush_policy=periodic`:
  - Defer flush to the periodic flusher even when `redis.persistence=always`.

## Required Table Shapes
A Redis structure is defined by creating a native index of type `REDIS_*` on a table with the following required columns:

- REDIS_STRING:
  - `key` (STRING, not null)
  - `value` (BLOB or STRING)
  - `ttl_at` (TIMESTAMP, nullable)

- REDIS_HASH:
  - `key` (STRING)
  - `field` (STRING)
  - `value` (BLOB or STRING)
  - `ttl_at` (TIMESTAMP, nullable)

- REDIS_LIST:
  - `key` (STRING)
  - `seq` (INT64)
  - `value` (BLOB or STRING)
  - `ttl_at` (TIMESTAMP, nullable)

- REDIS_SET:
  - `key` (STRING)
  - `member` (BLOB or STRING)
  - `ttl_at` (TIMESTAMP, nullable)

- REDIS_ZSET:
  - `key` (STRING)
  - `score` (FLOAT64)
  - `member` (BLOB or STRING)
  - `ttl_at` (TIMESTAMP, nullable)

- REDIS_STREAM:
  - `key` (STRING)
  - `entry_id` (INT64)
  - `field` (STRING)
  - `value` (BLOB or STRING)
  - `ttl_at` (TIMESTAMP, nullable)

- REDIS_BITMAP:
  - `key` (STRING)
  - `bit_offset` (INT64)
  - `bit_value` (BOOL)
  - `ttl_at` (TIMESTAMP, nullable)

- REDIS_HLL:
  - `key` (STRING)
  - `hll_registers` (BLOB)
  - `ttl_at` (TIMESTAMP, nullable)

- REDIS_GEO:
  - `key` (STRING)
  - `member` (STRING)
  - `lon` (FLOAT64)
  - `lat` (FLOAT64)
  - `ttl_at` (TIMESTAMP, nullable)

If required columns are missing or of incompatible types, index creation must fail with `SB_ERR_SCHEMA_MISMATCH`.

## Page Types
- `PAGE_TYPE_REDIS_META`
- `PAGE_TYPE_REDIS_HASH`
- `PAGE_TYPE_REDIS_LIST`
- `PAGE_TYPE_REDIS_SET`
- `PAGE_TYPE_REDIS_ZSET`
- `PAGE_TYPE_REDIS_STREAM`
- `PAGE_TYPE_REDIS_BITMAP`
- `PAGE_TYPE_REDIS_HLL`
- `PAGE_TYPE_REDIS_GEO`

## Meta Page Layout
1. `redis_meta_version` (u16)
2. `string_root_page_id` (u32)
3. `hash_root_page_id` (u32)
4. `list_root_page_id` (u32)
5. `set_root_page_id` (u32)
6. `zset_root_page_id` (u32)
7. `stream_root_page_id` (u32)
8. `bitmap_root_page_id` (u32)
9. `hll_root_page_id` (u32)
10. `geo_root_page_id` (u32)

## Hash Table Layout (REDIS_STRING, REDIS_HASH, REDIS_SET)
1. Hash pages use fixed-size bucket arrays with separate chaining.
2. Bucket array: `bucket_count` (u32) followed by `bucket_head[]` of `(page_id, offset)` pairs.
3. Entry record format:
4. `key_len` (u16), `key_bytes`
5. `field_len` (u16), `field_bytes` (only for REDIS_HASH)
6. `member_len` (u16), `member_bytes` (only for REDIS_SET)
7. `value_len` (u32), `value_bytes` (STRING/HASH)
8. `ttl_at` (u64, 0 if no TTL)
9. `next_page_id` (u32), `next_offset` (u16)

## List Layout (REDIS_LIST)
1. Lists use a doubly linked list of list blocks.
2. Block header: `prev_block_id`, `next_block_id`, `count`, `capacity`, `min_seq`, `max_seq`.
3. Block entries are stored in a packed array of `(seq, value_len, value_bytes, ttl_at)`.

## ZSet Layout (REDIS_ZSET)
1. Each zset uses a skiplist for score order and a hash table for member lookup.
2. Skiplist node: `score` (float64), `member_len`, `member_bytes`, `next[level]`, `prev`, `tid`.
3. Hash entry maps `member` to `score` and skiplist node pointer.

## Stream Layout (REDIS_STREAM)
1. Streams use a radix tree on `entry_id`.
2. Radix node stores `prefix_len`, `prefix_bytes`, `child_count`, and child pointers.
3. Leaf stores `entry_id`, `field_count`, and repeated `(field_len, field_bytes, value_len, value_bytes)` pairs.

## Bitmap Layout (REDIS_BITMAP)
1. Bitmaps are stored as fixed-size blocks of 1024 bits (128 bytes).
2. Block key is `block_id = floor(bit_offset / 1024)`.
3. Block payload is a 128-byte bitset.

## HyperLogLog Layout (REDIS_HLL)
1. Register count `m = 16384` with 6-bit registers.
2. Registers are packed in a byte array of length 12288 bytes.
3. Register index uses the first 14 bits of the hash; register value is `rho` of the remaining bits.

## Geo Layout (REDIS_GEO)
1. Geo uses a zset keyed by geohash string with member->(lon,lat).
2. `geohash = interleave_bits(lon_norm, lat_norm)` encoded in base32.
3. Range queries scan geohash prefixes and post-filter by exact distance.

## Memory and Persistence Model
- If `redis.persistence=always`:
  - All mutations are persisted on commit.
- If `redis.persistence=snapshot`:
  - Mutations are kept in memory; persisted by explicit snapshot operation.
- If `redis.persistence=never`:
  - Mutations are memory-only and lost on restart.

## MGA and Security Rules
- Redis structures are stored as regular rows with TIDs; MGA visibility is enforced for all operations.
- Security policies (row/column/domain) are applied after candidate retrieval and before results are returned.
- Index-only behavior is disallowed if policy evaluation requires non-indexed columns.

## Structure Implementations

### REDIS_STRING
- Storage: hash table keyed by `key`, value stored inline or as TOAST.
- GET: lookup key, validate TTL, return value.
- SET: insert/replace value; update TTL if provided.

### REDIS_HASH
- Storage: per-key hash table of field->value, stored in `PAGE_TYPE_REDIS_HASH`.
- HGET: lookup field.
- HSET: upsert field.

### REDIS_LIST
- Storage: quicklist-style linked list of blocks.
- LPUSH/RPUSH: add to head/tail block; split block if full.
- LPOP/RPOP: remove from head/tail.
- LRANGE: iterate blocks by sequence.

### REDIS_SET
- Storage: hash set of members.
- SADD/SREM: add/remove member.
- SISMEMBER: lookup member.

### REDIS_ZSET
- Storage: dual structure per key:
  - hash map member->score
  - skiplist ordered by score
- ZADD: update both structures.
- ZRANGE: scan skiplist by score range.

### REDIS_STREAM
- Storage: radix-tree of stream IDs; each entry holds a list of field/value pairs.
- XADD: append entry; auto-increment IDs if not provided.
- XRANGE: range scan by entry_id.

### REDIS_BITMAP
- Storage: bitset blocks (1024 bits per block).
- SETBIT/GETBIT: set or read a bit.

### REDIS_HLL
- Storage: HyperLogLog registers (default 16384 registers, 6 bits each).
- PFADD: update registers.
- PFCOUNT: estimate cardinality.

### REDIS_GEO
- Storage: geohash of lon/lat plus member mapping, backed by REDIS_ZSET.
- GEOADD: convert to geohash, store in ZSET.
- GEORADIUS: range scan on geohash prefix and filter by exact distance.

## Operation Algorithms (Deterministic)
1. HASH_LOOKUP: compute `bucket = hash(key) % bucket_count` and traverse the chain to find matching entry.
2. HASH_INSERT: if key exists, replace value and update TTL; else allocate a new entry and link it at the bucket head.
3. HASH_DELETE: unlink entry from bucket chain and free record.
4. LIST_PUSH: choose head or tail block, append value, update `seq`, split block if capacity exceeded.
5. LIST_POP: remove from head or tail block, delete empty blocks and relink neighbors.
6. SET_ADD: use hash table with key `(key, member)`; ignore if present.
7. SET_REMOVE: delete hash entry; if last member removed, delete key.
8. ZSET_ADD: update hash entry for member and insert or update skiplist node by score.
9. ZSET_RANGE: traverse skiplist from first node with score >= min.
10. STREAM_ADD: generate `entry_id` if absent, insert into radix tree, append field list.
11. BITMAP_SET: compute `block_id` and `bit_index`, set or clear bit in block.
12. HLL_ADD: compute 64-bit hash, `idx = hash >> 50`, `rho = leading_zeros(hash << 14) + 1`, update register max.
13. GEO_ADD: compute geohash and insert into zset keyed by geohash, store lon/lat in value payload.

## TTL Handling
- Each key has a `ttl_at` timestamp.
  - `ttl_at = NULL` means no expiration.
  - `ttl_at <= now()` means expired.
- TTL check on read; expired keys are treated as absent and scheduled for GC.
- `ttl_policy=lazy`: GC on read or periodic sweep.
- `ttl_policy=eager`: background thread evicts keys at expiration.

## Build Algorithm
1. Capture build snapshot `S1` with MGA rules.
2. Initialize meta and root pages and set index state to `building`.
3. If this index requires training, dictionary build, or model materialization, run that phase first and persist its outputs.
4. Scan base table in physical order. For each row version visible in `S1`, derive all index entries and call `Insert` for each entry.
5. Populate `index_stats` with row_count, distinct_count, null_frac, min_value, max_value, and histogram buckets as applicable.
6. Validate structural invariants and set index state to `active`.

## Insert
1. For `REDIS_STRING`, insert or replace `(key, value, ttl_at)`.
2. For `REDIS_HASH`, insert or replace `(key, field, value, ttl_at)`.
3. For `REDIS_LIST`, append `(key, seq, value, ttl_at)` at head or tail as directed by the operation.
4. For `REDIS_SET`, insert `(key, member, ttl_at)` if not present.
5. For `REDIS_ZSET`, insert `(key, score, member, ttl_at)` and update both map and skiplist.
6. For `REDIS_STREAM`, append `(key, entry_id, field, value, ttl_at)`; auto-generate `entry_id` if absent.
7. For `REDIS_BITMAP`, set or clear `(key, bit_offset, bit_value, ttl_at)` in the bitset block.
8. For `REDIS_HLL`, merge the new element into `hll_registers`.
9. For `REDIS_GEO`, insert `(key, member, lon, lat, ttl_at)` and update geohash ZSET.

## Search
1. REDIS_STRING GET: hash lookup by `key`, validate TTL, return value.
2. REDIS_HASH HGET: hash lookup by `(key, field)`, validate TTL, return value.
3. REDIS_LIST LRANGE: locate first block, iterate by `seq` range, return values.
4. REDIS_SET SISMEMBER: hash lookup by `(key, member)`.
5. REDIS_ZSET ZRANGE: traverse skiplist between score bounds.
6. REDIS_STREAM XRANGE: radix-tree range scan by `entry_id`.
7. REDIS_BITMAP GETBIT: locate block and read bit.
8. REDIS_HLL PFCOUNT: compute cardinality estimate from registers.
9. REDIS_GEO GEORADIUS: find geohash prefixes, scan candidates, post-filter by distance.

## Delete
1. For `REDIS_STRING`, remove the key entry.
2. For `REDIS_HASH`, remove the `(key, field)` entry; delete the key if no fields remain.
3. For `REDIS_LIST`, remove the element at the specified position or trim by range.
4. For `REDIS_SET`, remove `(key, member)`; delete the key if the set becomes empty.
5. For `REDIS_ZSET`, remove `(key, member)` from both map and skiplist.
6. For `REDIS_STREAM`, remove the specified `entry_id` and its fields.
7. For `REDIS_BITMAP`, clear the bit at `bit_offset`.
8. For `REDIS_HLL`, deletion is not supported; `PFMERGE` into a new key and delete the old key.
9. For `REDIS_GEO`, remove `(key, member)` and its geohash entry.

## SBLR Operations
- Define canonical operations:
  - `REDIS_GET`, `REDIS_SET`, `REDIS_HGET`, `REDIS_HSET`, `REDIS_LPUSH`, `REDIS_RPUSH`, `REDIS_SADD`, `REDIS_ZADD`, `REDIS_XADD`, `REDIS_SETBIT`, `REDIS_PFADD`, `REDIS_GEOADD`.
  - Additional Redis operations must be explicitly listed in the command matrix before implementation.
- Each maps to the corresponding structure implementation above.

## Error Handling
- Accessing expired keys returns NULL/empty result.
- Invalid TTL values rejected.

## Test Contract
- Each structure operation matches Redis semantics for edge cases (empty, missing key, TTL expired).
- Persistence modes behave as specified.

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

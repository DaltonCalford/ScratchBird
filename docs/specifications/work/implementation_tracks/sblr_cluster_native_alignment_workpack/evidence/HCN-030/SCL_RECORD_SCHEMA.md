# SCL Record Schema

Per-shard durable file:
- path: `<root>/<shard_id>.scl`
- encoding: UTF-8 TSV, one entry per line

Record columns:
1. `local_txn_id` (u64)
2. `commit_timestamp_ns` (u64)
3. `payload_format` (string)
4. `payload_hex` (hex-encoded payload bytes)

Ordering contract:
- append requires `local_txn_id == last_local_txn_id + 1` per shard.
- out-of-order inserts are rejected before write.

Durability contract:
- append success is returned only after write + flush + fsync/commit.

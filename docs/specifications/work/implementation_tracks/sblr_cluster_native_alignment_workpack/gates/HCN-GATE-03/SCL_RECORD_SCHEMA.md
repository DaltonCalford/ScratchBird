# SCL Record Schema

Scope: HCN-030 gate contract snapshot.

## Durable record format
Each line in `<root>/<shard_id>.scl` is tab-separated:
1. `local_txn_id` (uint64)
2. `commit_timestamp_ns` (uint64)
3. `payload_format` (string, no tabs/newlines)
4. `payload_hex` (hex-encoded payload bytes)

## Integrity and ordering guarantees
- Append accepts only `local_txn_id == expected_next_local_txn_id` per shard.
- Writer flushes and fsyncs every committed line before success return.
- Reader validates monotonically increasing local_txn_id sequence and hex payload decode.
- Malformed lines or sequence violations return `Status::DATA_CORRUPTED`.

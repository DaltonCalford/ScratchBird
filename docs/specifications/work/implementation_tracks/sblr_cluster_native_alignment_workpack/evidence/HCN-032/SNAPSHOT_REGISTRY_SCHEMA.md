# Snapshot Registry Schema

Registry key:
- `(shard_id, session_id)`

Stored fields:
- `snapshot_boundary`
- `start_time_ns`
- `last_heartbeat_ns`

Derived values:
- `OST_shard` (oldest snapshot boundary) is computed as `min(snapshot_boundary)` over active entries for a shard.
- if no entries exist for shard, `OST_shard = 0`.

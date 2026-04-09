# GTXID Schema (Gate Snapshot)

From HCN-024 closure:
- GTXID contract: (`shard_id`, `local_txn_id`).
- per-shard allocation and commit/apply ordering are monotonic and checked.
- gaps/duplicates reject with deterministic reason codes.

Validated by:
- `GtxidOrderingTest.*`

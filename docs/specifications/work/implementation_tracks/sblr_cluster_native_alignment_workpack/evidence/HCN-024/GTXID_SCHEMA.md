# GTXID Schema

Canonical contract:
- GTXID = (`shard_id`, `local_txn_id`).
- `local_txn_id` is monotonic and scoped to a shard.

Ordering contracts:
- commit path accepts next monotonic or current expected ID; rejects stale/duplicates.
- follower apply must be exactly previous applied + 1; gaps and duplicates reject.

Reason taxonomy:
- `INVALID_GTXID`
- `STALE_OR_DUPLICATE`
- `OUT_OF_ORDER`

# Implementation Notes - HCN-024

Code paths:
- `include/scratchbird/core/cluster_write_safety.h`
  - introduced `GTXID`, `TxnOrderingReason`, `TxnOrderingResult`, and `ShardTxnOrderBook` APIs.
- `src/core/cluster_write_safety.cpp`
  - implemented `allocateNext`, `recordCommitted`, `recordFollowerApply`, `lastCommitted`, and `lastApplied`.

Safety properties:
- per-shard counters are isolated and monotonic.
- follower apply path rejects stale/duplicate and out-of-order GTXIDs.

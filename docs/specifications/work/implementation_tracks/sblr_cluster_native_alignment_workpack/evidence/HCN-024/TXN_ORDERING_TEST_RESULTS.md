# Txn Ordering Test Results

Validated paths:
- `GtxidOrderingTest.AllocationAndCommitOrderingIsMonotonicPerShard`
- `GtxidOrderingTest.FollowerApplyRejectsGapsAndDuplicates`

Observed:
- monotonic per-shard commit ordering holds.
- follower apply rejects both gaps and duplicate IDs with explicit reasons.

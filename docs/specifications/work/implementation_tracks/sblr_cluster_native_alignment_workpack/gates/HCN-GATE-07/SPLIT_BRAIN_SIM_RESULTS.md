# Split Brain Simulation Results - HCN-060

Validated tests:
- `ClusterWriteFencingTest.StaleLeaderWritesAreRejectedByTokenAndLeaderIdentity`
- `ClusterWriteFencingTest.RoutingEpochMustMatchPinnedWritePathEpoch`
- `DeterministicShardRouterTest.RejectsStaleRoutingEpoch`

Outcome:
- Write admission rejects stale leader/fencing combinations.
- Stale routing epochs are rejected deterministically.
- No split-brain acceptance observed in test suite.

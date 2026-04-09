# Stale Leader Rejection Results

`ClusterWriteFencingTest.StaleLeaderWritesAreRejectedByTokenAndLeaderIdentity` proves:
- mismatched leader node is rejected with `NOT_CURRENT_LEADER`.
- stale term token is rejected with `STALE_FENCING_TOKEN`.

`ClusterWriteFencingTest.RoutingEpochMustMatchPinnedWritePathEpoch` proves:
- mismatched routing epoch is rejected with `ROUTING_EPOCH_MISMATCH`.

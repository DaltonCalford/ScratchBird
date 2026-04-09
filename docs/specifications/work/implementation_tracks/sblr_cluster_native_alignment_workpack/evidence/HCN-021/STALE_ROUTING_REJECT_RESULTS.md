# Stale Routing Reject Results

`DeterministicShardRouterTest.RejectsStaleRoutingEpoch` verifies mismatched routing epoch requests are rejected with:
- `status = INVALID_TRANSACTION_STATE`
- `reason = STALE_ROUTING_EPOCH`

# Beta 2 Shard Failure Rebalance And Ownership Recovery Classification Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 failure taxonomy for shard split, merge, move, rebalance, and
read-routing recovery.

## Governing rules

1. Routing epoch, placement epoch, and cutover fence are independent guards.
2. Ambiguous ownership must fail closed.
3. Recovery restores one authoritative shard owner set before admitting writes.

## Failure classes

- `STALE_ROUTING_EPOCH`
- `PLACEMENT_EPOCH_CONFLICT`
- `CUTOVER_FENCE_MISSING`
- `SPLIT_PARTIAL_PUBLICATION`
- `MERGE_PARTIAL_PUBLICATION`
- `MOVE_CATCHUP_INCOMPLETE`
- `FOLLOWER_READ_STALENESS_EXCEEDED`
- `REBALANCE_DESTINATION_UNAVAILABLE`
- `SHARD_SPLIT_BRAIN_REFUSED`

## Recovery workflows

### Split recovery

1. Verify whether child shards were fully published.
2. If not, quarantine the split and keep the parent authoritative.
3. If yes, fence the parent and resume child publication.

### Merge recovery

1. Verify successor shard identity and predecessor retirement state.
2. If successor is incomplete, keep predecessors authoritative.
3. If successor is complete, fence predecessor write admission and complete
   retirement.

### Move or rebalance recovery

1. Compare source and destination placement epochs.
2. Resume catch-up if one destination is valid and incomplete.
3. Refuse both placements if conflicting primaries remain visible.
4. Re-admit one owner set only after cutover fence verification.

## Client-visible outcome classes

- `REFUSED`
- `RETRY_REQUIRED`
- `AMBIGUOUS_PLACEMENT`
- `READ_STALE_REFUSED`
- `RECOVERED_AFTER_FENCE`

## Metrics

- split or merge recovery count
- ambiguous-placement count
- cutover-fence verification latency
- follower-read refusal count
- shard recovery quarantine duration

## Cross-section requirements

- section `42` owns failure classes and recovery rules
- section `25` owns runtime workflows that emit these failures
- section `24` owns the epochs and fence rows consumed here

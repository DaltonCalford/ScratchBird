# Beta 2 Replicated Topology Read Scale And Geo Failover Model

## Purpose

Define native replicated-topology roles, read-routing policy, and geo failover
behavior over the Beta 2 cluster and sharding substrate.

## Governing rules

1. Exactly one write-authoritative primary owns each protected scope at a time.
2. Secondary replicas are derivative until promoted.
3. Read routing must publish staleness posture.
4. Geo failover must be operator-visible and fence old primaries.

## Canonical metadata

- `sb_replication_group`
  - `group_uuid`
  - `scope_uuid`
  - `primary_member_uuid`
  - `failover_policy`
  - `read_route_policy`
- `sb_replica_member`
  - `member_uuid`
  - `group_uuid`
  - `region_id`
  - `role`
  - `apply_lag_class`
  - `status`
- `sb_read_route_policy`
  - `policy_uuid`
  - `staleness_class`
  - `read_intent`
  - `preferred_regions`

## Roles

- `PRIMARY`
- `SYNC_SECONDARY`
- `ASYNC_SECONDARY`
- `READ_SCALE_MEMBER`
- `PROMOTION_CANDIDATE`

## Failover flow

1. Health and fencing rules declare the primary unavailable.
2. Promotion candidate is selected.
3. Old primary is fenced.
4. Promotion state is published.
5. Read routes are recalculated.

## Refusal rules

- `REPLICA_GROUP_INVALID`
- `READ_ROUTE_STALENESS_REFUSED`
- `FAILOVER_FENCE_INCOMPLETE`

## Metrics

- apply lag
- read-route distribution
- failover count
- promotion time

## Cross-section requirements

- section `25` owns roles, routing, and failover
- section `42` owns failure and fencing classification
- section `39` owns PITR and retention interaction

# Beta 2 Cluster HA DR And Failover Model

## Purpose

Define the Beta 2 clustered availability model for synchronous HA,
asynchronous DR, and failover while preserving MGA authority and explicit
publication fences.

## Governing rules

1. One database group has at most one write-primary lease holder at a time.
2. Lease and epoch state are durable catalog truth, not transient gossip-only
   state.
3. Replication capsules are derivative evidence of committed MGA state; they do
   not outrank the primary or promoted replica page image they certify.
4. HA, DR, and PITR are distinct modes with different admission and completion
   rules.

## Cluster roles

- `PRIMARY`
- `SYNC_SECONDARY`
- `ASYNC_SECONDARY`
- `CATCHUP`
- `PROMOTION_CANDIDATE`
- `FENCED`
- `QUARANTINED`

## Durable metadata

The cluster model shall persist:

- `cluster_uuid`
- `database_group_uuid`
- `membership_epoch`
- `primary_lease_holder`
- `lease_expiry`
- `replica_uuid`
- `replica_role`
- `replication_generation`
- `applied_capsule_sequence`
- `promotion_state`
- `fence_state`

## Replication capsule contract

Each replication capsule shall contain:

- database group uuid
- source epoch
- sequence number
- committed transaction lineage range
- changed-page or changed-object payload
- checksum and manifest hash
- policy class: `SYNC_HA`, `ASYNC_DR`, or `PITR_EXPORT`

Capsules are published only after local commit becomes durable.

## Synchronous commit path

1. Primary executes and durably publishes the commit locally.
2. Primary emits the `SYNC_HA` capsule.
3. Required synchronous secondaries durably apply the capsule.
4. Primary acknowledges success only after the synchronous durability rule is
   satisfied.

## Asynchronous DR path

1. Primary publishes commit locally.
2. Primary emits the `ASYNC_DR` capsule.
3. Async replicas apply without blocking client acknowledgement.
4. Lag and divergence are operator-visible metrics.

## Failover workflow

1. Failure detector marks the primary unreachable or fenced.
2. Manager validates lease expiry or explicit operator fence.
3. Candidate scoring compares:
   - membership epoch compatibility
   - last applied sync capsule
   - async lag
   - local health and corruption state
4. One candidate is promoted under a new epoch.
5. All older primaries are fenced before re-admission.
6. New primary begins serving writes only after durable epoch publication.

## Non-authority rule

No node may rebuild truth from speculative transport state alone. Promotion
requires a replica with durably applied MGA state plus certified derivative
completeness for the admitted policy.

## Refusal rules

- no promotion without durable fence:
  `CLUSTER_FAILOVER_FENCE_REQUIRED`
- no promotion from stale epoch:
  `CLUSTER_FAILOVER_EPOCH_STALE`
- no service from split-brain candidate:
  `CLUSTER_SPLIT_BRAIN_REFUSED`
- no sync-HA acknowledgement when sync targets are below quorum:
  `CLUSTER_SYNC_DURABILITY_UNSATISFIED`

## Metrics

- replica lag by policy class
- sync acknowledgement wait time
- membership epoch changes
- failover attempts and promoted candidate id
- fence duration and unresolved fenced nodes

## Cross-section requirements

- section 25 owns membership, placement, and failover orchestration
- section 39 owns PITR and restore-rehearsal artifact rules
- section 42 owns client-visible failure classification after failover

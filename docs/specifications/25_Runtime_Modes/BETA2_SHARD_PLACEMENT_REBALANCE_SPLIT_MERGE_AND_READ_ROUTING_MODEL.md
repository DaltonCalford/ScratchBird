# Beta 2 Shard Placement Rebalance Split Merge And Read Routing Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 runtime model for shard placement, split, merge, move,
rebalance, and read routing.

## Governing rules

1. Shard placement decisions are driven by catalog-owned policy and current
   bounded runtime health data.
2. Split, merge, move, and rebalance are different workflows with different
   guards and recovery paths.
3. Read routing must declare its semantic contract explicitly.

## Placement scoring

Shard placement and rebalance scoring shall consider:

- locality preference
- tenant and service-class policy
- storage and memory headroom
- observed load
- failure-domain diversity
- current shard density per node

## Split workflow

1. Detect range or shard hotspot or size-pressure candidate.
2. Publish split proposal with one cut key or range boundary.
3. Create child shard identities under a new routing epoch.
4. Copy or redirect reads under the declared split mode.
5. Publish cutover fence.
6. Retire the parent shard only after cutover verification succeeds.

## Merge workflow

1. Verify adjacent compatibility and low-load eligibility.
2. Publish merge proposal.
3. Create merged successor identity.
4. Redirect reads to the successor under one epoch change.
5. Retire predecessor shards after lineage verification.

## Move and rebalance workflow

1. Select source and destination by placement policy.
2. Start one bounded migration job.
3. Synchronize trailing changes.
4. Publish cutover fence and new placement epoch.
5. Drain old placement and confirm new primary or replica role.

## Read-routing modes

- `PRIMARY_STRICT`
- `LOCAL_REPLICA`
- `FOLLOWER_BOUNDED_STALENESS`
- `ANALYTIC_STALE_OK`

The chosen mode must be visible in diagnostics and explain output.

## Refusal rules

- `SHARD_PLACEMENT_POLICY_MISSING`
- `SHARD_SPLIT_PROPOSAL_UNSAFE`
- `SHARD_MERGE_COMPATIBILITY_REFUSED`
- `SHARD_REBALANCE_DESTINATION_UNQUALIFIED`
- `SHARD_READ_ROUTE_STALENESS_REFUSED`

## Metrics

- split and merge proposal count
- rebalance bytes moved
- routing-epoch changes
- follower-read lag
- per-node shard density

## Cross-section requirements

- section `25` owns runtime workflows and read-routing modes
- section `24` owns shard policy and migration catalog rows
- section `42` owns failure and recovery classification

# Cluster Routing, Fencing, and Shard Commit Log Observability

## Scope

This file defines the operator-visible observability contract for the cluster write-safety and shard commit-log lanes that already exist in code.

## Required metric families

The current code-backed registry already exposes:

- `sb_cluster_leader_term`
- `sb_cluster_lease_seconds_remaining`
- `sb_cluster_fencing_rejections_total`
- `sb_cluster_routing_requests_total`
- `sb_cluster_routing_epoch`
- `sb_cluster_replication_lag_txn`
- `sb_cluster_replication_lag_seconds`
- `sb_cluster_replication_apply_total`

These are canonical required outputs, not optional debug counters.

## Fencing observability

### Required dimensions

`sb_cluster_fencing_rejections_total` must remain dimensioned by:

- `db`
- `shard`
- `reason`

The reason dimension must remain aligned with write-admission refusal classes such as:

- shard not registered
- writes disabled
- not current leader
- fencing shard mismatch
- stale fencing token
- routing epoch mismatch

Canonical rule:

- leader-term and routing-epoch guard failures must remain externally
  distinguishable from generic write refusal
- write-fencing observability is not optional debug output

## Routing observability

### Required dimensions

`sb_cluster_routing_requests_total` must remain dimensioned by:

- `db`
- `protocol`
- `result`

### Required meanings

The `result` dimension must distinguish successful routing from stale-epoch or plan-not-found failures.

`sb_cluster_routing_epoch` must expose the currently active routing epoch for operator inspection.

The operator-visible routing surface must allow correlation between:

- routing epoch
- leader term
- fencing rejections
- routing request result

## Replication and follower-apply observability

The current code-backed metric contract already exposes:

- replication lag in transactions
- replication lag in seconds
- follower apply attempts by result

Canonical rule:

- any shard-commit or follower-apply implementation change must preserve operator visibility into apply health and lag

## Shard commit-log observability requirements

The current code already has a durable per-shard commit log with corruption refusal. The observability contract therefore requires surfaces that let operators diagnose:

- append refusal due to out-of-order local transaction id
- durability write failures
- malformed commit log content on read
- replay or follower-apply mismatches

The observability contract also requires a first-class way to distinguish:

- multi-shard guard refusal
- routing-epoch refusal
- leader-term or leader-identity refusal

If these remain inferred through generic error returns rather than dedicated
metrics or SQL rows, that is implementation drift against canon.

If a current runtime surface does not yet expose all of those counters directly, this remains an implementation gap against the canonical contract, not a reason to weaken the spec.

## Current code-backed versus reconstructed-required behavior

### Current code-backed

The code already proves:

- routing epoch gauge
- leader term gauge
- fencing rejection counter
- routing request counter
- replication lag gauges
- replication apply counter

### Required reconstructed behavior

The rebuild requires the observability layer to continue expanding until shard commit-log specific failures and multi-shard guard refusals are first-class operator outputs as well.

The rebuilt operator lane must also preserve the MGA-versus-derivative split:

- local MGA durability health
- shard commit-log durability health
- follower-apply health

These are related, but they are not the same health class and must not be
collapsed into one generic cluster-status bit.

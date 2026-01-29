# SBCLUSTER-12: Autoscaling and Elastic Node Lifecycle (Beta)

## 1. Purpose
Define the autoscaling model for ScratchBird clusters, including **elastic
node lifecycle**, **horizontal/vertical scaling**, and **safe rebalancing**
using cluster membership, shard migration, and observability metrics.

This spec ties into:
- **Membership/Identity** (SBCLUSTER-02)
- **Shard migration and rebalancing** (SBCLUSTER-11)
- **Observability** (SBCLUSTER-10)

## 2. Scope

### In Scope
- Autoscaling policies and decision flow
- Node lifecycle state machine
- Integration with membership and shard migration
- Metrics and thresholds used for scaling decisions
- Safety gates and invariants

### Out of Scope
- Cloud provider implementation details (Kubernetes, ASG, etc.)
- Physical hardware procurement
- Vendor-specific load balancer configuration

## 3. Goals
- Scale **horizontally** by adding/removing nodes without downtime.
- Support **vertical scaling** via planned node upgrades.
- Preserve quorum, security bundle integrity, and shard replication factor.
- Avoid disruptive oscillation with cooldown and stability guards.

## 4. Terminology
- **Autoscaler**: Control-plane service that evaluates metrics and issues
  scale decisions.
- **Node Pool**: Group of nodes with the same role and resource profile.
- **Drain**: Safe migration of shards off a node prior to removal.
- **Rebalance**: Redistribution of shard ownership across the cluster.

## 5. Architecture Overview

```
 Metrics (SBCLUSTER-10) ----> Autoscaler ----> Membership (SBCLUSTER-02)
                                     |                 |
                                     v                 v
                            Shard Migration (SBCLUSTER-11)
                                     |
                                     v
                               Data Plane Rebalance
```

The autoscaler is a **control-plane component** that does not execute queries.
It only:
1) Evaluates metrics against policies.
2) Proposes membership changes.
3) Triggers shard migration and rebalancing.

## 6. Node Lifecycle States

| State | Description | Allowed Transitions |
| --- | --- | --- |
| PROVISIONING | Node is being created or bootstrapped | JOINING |
| JOINING | Node requests membership | SYNCING, QUARANTINED |
| SYNCING | Node bootstraps catalogs, bundles, and shard map | ACTIVE |
| ACTIVE | Node is serving traffic | DRAINING, QUARANTINED |
| DRAINING | Node is evacuating shards | EXITING |
| EXITING | Membership removal in progress | TERMINATED |
| TERMINATED | Node removed from cluster | - |
| QUARANTINED | Node blocked due to security/health | EXITING |

Transitions must be committed via CCE updates (SBCLUSTER-01).

## 7. Scaling Models

### 7.1 Horizontal Scaling (Add/Remove Nodes)

**Scale-Out (Add nodes)**:
1. Autoscaler provisions new nodes (PROVISIONING).
2. Nodes join via membership (JOINING -> SYNCING).
3. Security bundle sync and CCE verification.
4. Shard map update allocates vnodes to new nodes.
5. Rebalance migrates shards (SBCLUSTER-11).
6. Node becomes ACTIVE.

**Scale-In (Remove nodes)**:
1. Autoscaler selects candidate nodes (ACTIVE -> DRAINING).
2. Shards are migrated off node (SBCLUSTER-11).
3. Membership removal committed (EXITING -> TERMINATED).

### 7.2 Vertical Scaling (Upgrade Nodes)

Vertical scaling is **rolling**:
1. Select node, migrate or drain shards if needed.
2. Stop node, resize resources.
3. Rejoin and resync (JOINING -> SYNCING -> ACTIVE).

This avoids downtime while maintaining quorum and replication targets.

## 8. Autoscaling Policy Model

### 8.1 Policy Structure

```
[cluster.autoscale]
enabled = true
mode = "horizontal"  # horizontal|vertical|hybrid
min_nodes = 3
max_nodes = 30
cooldown_seconds = 900
evaluation_window_seconds = 300
stability_required = true

[cluster.autoscale.policy.cpu]
scale_out_threshold = 0.70
scale_in_threshold = 0.30
min_duration_seconds = 600

[cluster.autoscale.policy.lag]
replication_lag_seconds = 5
max_duration_seconds = 300

[cluster.autoscale.policy.skew]
shard_skew_percent = 20
```

### 8.2 Decision Inputs (Observability)
Metrics are sourced from SBCLUSTER-10:
- `scratchbird_cluster_cpu_utilization`
- `scratchbird_cluster_memory_utilization`
- `scratchbird_cluster_disk_utilization`
- `scratchbird_cluster_replication_lag_seconds`
- `scratchbird_cluster_query_latency_ms`
- `scratchbird_cluster_shard_skew_percent`
- `scratchbird_cluster_churn_score`

### 8.3 Safety Gates
Scale decisions MUST be rejected if:
- Quorum would be violated.
- Replication factor would drop below target.
- Security bundle is not converged.
- Cluster churn is above threshold.
- Shard migration backlog exceeds limits.

## 9. Shard Migration Integration
Autoscaler interacts with SBCLUSTER-11:
- **Scale-Out**: add nodes, then rebalance vnodes to distribute shards.
- **Scale-In**: drain shards to remaining nodes before removal.

Shard migration progress and ETA are required inputs for scaling decisions.

## 10. Membership Integration
Membership updates are performed via:
- SBCLUSTER-02 membership API
- Raft-based CCE updates (SBCLUSTER-01)

Autoscaler MUST not bypass membership verification.

## 11. Operational Modes

### 11.1 Manual Override
Operators can disable autoscaling or force scale actions:

```
sb_admin cluster autoscale disable
sb_admin cluster autoscale scale_out --count 2
sb_admin cluster autoscale scale_in --node node-12
```

### 11.2 Scheduled Scaling
Policies may include time windows:
- Example: scale out during business hours, scale in overnight.

## 12. Failure Handling
- Autoscaler retries failed scale actions after cooldown.
- Nodes that fail to join are quarantined and removed.
- Rolling scale-in must abort if replication lag exceeds threshold.

## 13. Observability and Audit
All autoscaling decisions are recorded:
- Decision inputs
- Policy thresholds
- Membership and shard map changes
- Migration progress

Audit records are written to:
- sys.cluster_events
- sys.autoscale_decisions

## 14. Acceptance Criteria (Beta)
- Horizontal scale-out and scale-in automated without downtime.
- Vertical scaling supported with rolling upgrades.
- Membership and shard migration remain consistent under churn.
- Autoscaler decisions observable and auditable.


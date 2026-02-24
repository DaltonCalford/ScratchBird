# SB-CLUSTER-IMPLEMENTATION-WORKPLAN
## Derived From: SB-CLUSTER-SWS-MGA-01
### Model: Single-Writer-Per-Shard (MGA)
### Status: Beta Implementation Plan

---

# 1. Overview

This workplan converts SB-CLUSTER-SWS-MGA-01 into a phased, implementation-ready task structure.

Each task ID corresponds to a concrete engineering deliverable.

---

# 2. Phase 0 – Foundations

## SB-CLUSTER-001 — Persist Cluster Identity
- Add `cluster_id`, `node_id`, `cluster_config_epoch` to DB header.
- Ensure persistence across restart.

## SB-CLUSTER-002 — TimeSource Abstraction
- Implement injectable time source.
- Replace direct system clock usage in UUIDv7.

## SB-CLUSTER-003 — StorageLockProvider
- Replace direct flock usage with abstraction.

## SB-CLUSTER-004 — Shard Commit Log (SCL) Base
- Append-only per shard.
- Durable.
- Ordered by `local_txn_id`.

---

# 3. Phase 1 – Control Plane

## SB-CLUSTER-010 — Control Plane Log
- Replicated configuration log.
- Stores membership, shard definitions, leader assignments, epochs.

## SB-CLUSTER-011 — Node Membership Protocol
- Join workflow.
- Rejoin workflow.

## SB-CLUSTER-012 — Shard Metadata Model
- Implement shard map tables.
- Implement cluster_nodes table.

## SB-CLUSTER-013 — Leader Election
- Implement leader_term.
- Implement lease expiration.

## SB-CLUSTER-014 — Fencing Enforcement
- Enforce `(shard_id, leader_term)` token in engine write path.

---

# 4. Phase 2 – Routing & Epoch Pinning

## SB-CLUSTER-020 — Router Engine
- Deterministic shard routing.

## SB-CLUSTER-021 — Epoch Validation
- Reject stale routing_epoch writes.

## SB-CLUSTER-022 — Multi-Shard Write Guard
- Reject cross-shard write attempts.

---

# 5. Phase 3 – MGA Transaction Integration

## SB-CLUSTER-030 — Implement GTXID
- `(shard_id, local_txn_id)` representation.

## SB-CLUSTER-031 — Snapshot Registry
- Publish snapshot boundaries per session.

## SB-CLUSTER-032 — Committed Watermark Publication
- Implement `CWM_shard`.

---

# 6. Phase 4 – Replication

## SB-CLUSTER-050 — SCL Replication
- Ordered replication.

## SB-CLUSTER-051 — Follower Apply Pipeline
- Idempotent apply.

## SB-CLUSTER-052 — Replication Watermark
- Maintain `RWM_shard`.

---

# 7. Phase 5 – GC Safety

## SB-CLUSTER-060 — Compute OST_shard
- From snapshot registry.

## SB-CLUSTER-061 — GC Safe Horizon
- `GC_safe_shard = min(OST_shard, RWM_shard)`.

## SB-CLUSTER-062 — Sweep Enforcement
- Prevent reclaim beyond GC safe horizon.

---

# 8. Phase 6 – Domain Replication

## SB-CLUSTER-070 — Domain Control-Plane Log Entries
- DOMAIN_CREATE
- DOMAIN_ALTER
- DOMAIN_DROP

## SB-CLUSTER-071 — DomainSync Join Validation
- Validate identical UUID + hash.

## SB-CLUSTER-072 — Domain Epoch Enforcement
- Bump schema_epoch on change.

---

# 9. Phase 7 – Observability

## SB-CLUSTER-090 — SHOW CLUSTER Surfaces
## SB-CLUSTER-091 — SHOW SHARD STATUS
## SB-CLUSTER-092 — SHOW GC HORIZONS

---

# 10. Phase 8 – Test Suite

## SB-CLUSTER-100 — Split Brain Test
## SB-CLUSTER-101 — Failover Test
## SB-CLUSTER-102 — Replication Integrity Test
## SB-CLUSTER-103 — Snapshot Consistency Test
## SB-CLUSTER-104 — GC Safety Test

---

# 11. Beta Completion Criteria

Cluster ready when:
- Fencing blocks stale leaders.
- Replication stable.
- GC safe.
- Snapshot vectors consistent.
- Domain replication deterministic.

---

This document is authoritative for cluster implementation sequencing.


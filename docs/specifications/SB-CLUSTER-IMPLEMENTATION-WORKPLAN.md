# SB-CLUSTER-IMPLEMENTATION-WORKPLAN
## Derived From: SB-CLUSTER-SWS-MGA-01
### Scope: Single-Writer-Per-Shard Cluster (MGA Model)
### Status: Beta Target

---

# 1. Overview

This document converts SB-CLUSTER-SWS-MGA-01 into a structured implementation workplan.

The goal is to deliver a **beta-ready, deterministic, MGA-consistent cluster** using:

- Shared-nothing shards
- Single writer per shard
- Replicated control-plane log
- Shard Commit Log (SCL) replication
- Cluster-safe GC horizon
- Fencing and lease enforcement
- Snapshot-vector based cross-shard reads

---

# 2. Phased Implementation Plan

---

## Phase 0 – Foundational Infrastructure

### SB-CLUSTER-001 — Persist Cluster Identity
**Deliverables:**
- Add `cluster_id`, `node_id`, `cluster_config_epoch` to DB header.
- Ensure persistence across restart.

**Acceptance Criteria:**
- Restart does not change node identity.
- Cluster ID remains stable.

---

### SB-CLUSTER-002 — TimeSource Abstraction
**Deliverables:**
- `TimeSource` interface.
- Replace direct system clock usage in UUIDv7 + epoch stamping.

**Acceptance Criteria:**
- Unit tests confirm injected time source behavior.

---

### SB-CLUSTER-003 — StorageLockProvider Abstraction
**Deliverables:**
- Interface for file locking.
- Replace direct `flock` calls.

**Acceptance Criteria:**
- Lock provider swappable without code changes elsewhere.

---

### SB-CLUSTER-004 — Shard Commit Log (SCL) Base
**Deliverables:**
- Append-only commit log per shard.
- Persisted to disk.
- Monotonic `local_txn_id` ordering.

**Acceptance Criteria:**
- Log replay reconstructs committed state.

---

## Phase 1 – Control Plane

### SB-CLUSTER-010 — Control Plane Log
**Deliverables:**
- Replicated configuration log.
- Stores:
  - Node membership
  - Shard definitions
  - Leader assignments
  - Epoch updates

**Acceptance Criteria:**
- All nodes converge to identical cluster_config_epoch.

---

### SB-CLUSTER-011 — Node Membership Protocol
**Deliverables:**
- Join/rejoin workflow.
- Node state tracking.

---

### SB-CLUSTER-012 — Shard Metadata Model
**Deliverables:**
- `shards` table
- `shard_mapping`
- `cluster_nodes`

---

### SB-CLUSTER-013 — Leader Election
**Deliverables:**
- Leader term counter
- Lease expiration tracking
- Election protocol

**Acceptance Criteria:**
- Single leader active per shard at all times.

---

### SB-CLUSTER-014 — Fencing Token Enforcement
**Deliverables:**
- `(shard_id, leader_term)` token
- Engine-level write validation

**Acceptance Criteria:**
- Old leader cannot commit after demotion.

---

## Phase 2 – Routing & Epoch Pinning

### SB-CLUSTER-020 — Router Engine
**Deliverables:**
- Deterministic shard routing.
- Epoch pinning per request.

---

### SB-CLUSTER-021 — Epoch Validation
**Deliverables:**
- Reject writes if epoch mismatch.
- Replan reads if schema_epoch changed.

---

### SB-CLUSTER-022 — Multi-Shard Write Detection
**Deliverables:**
- Write spanning >1 shard rejected.

---

## Phase 3 – MGA Transaction Integration

### SB-CLUSTER-030 — GlobalTxnId (GTXID)
**Deliverables:**
- Implement `(shard_id, local_txn_id)` identity.

---

### SB-CLUSTER-031 — Snapshot Registry
**Deliverables:**
- Active snapshot publication.
- Heartbeat system.

---

### SB-CLUSTER-032 — Committed Watermark
**Deliverables:**
- `CWM_shard` publication.

---

## Phase 4 – Replication

### SB-CLUSTER-050 — SCL Replication
**Deliverables:**
- Follower apply pipeline.
- Ordered replay by `local_txn_id`.

---

### SB-CLUSTER-051 — Replication Watermark
**Deliverables:**
- `RWM_shard` tracking.

---

## Phase 5 – GC Safety

### SB-CLUSTER-060 — Compute OST_shard
**Deliverables:**
- Compute from snapshot registry.

---

### SB-CLUSTER-061 — GC Safe Horizon
**Deliverables:**
- `GC_safe_shard = min(OST_shard, RWM_shard)`

---

### SB-CLUSTER-062 — Sweep Enforcement
**Deliverables:**
- Block reclamation of versions ≥ GC_safe_shard.

---

## Phase 6 – Observability

### SB-CLUSTER-090 — Admin Surfaces
- SHOW CLUSTER
- SHOW NODES
- SHOW SHARDS
- SHOW LEADERS
- SHOW GC HORIZONS
- SHOW REPLICATION LAG

---

## Phase 7 – Test Suite

### SB-CLUSTER-100 — Split Brain Test
### SB-CLUSTER-101 — Failover Test
### SB-CLUSTER-102 — GC Safety Test
### SB-CLUSTER-103 — Snapshot Vector Consistency Test
### SB-CLUSTER-104 — Rejoin Catch-up Test

---

# 3. Beta Completion Criteria

Cluster considered beta-ready when:

- Fencing prevents stale leader writes.
- Leader failover completes without data loss.
- Replication log replays correctly.
- GC horizon protects long-running snapshots.
- Scatter-gather read snapshot is consistent.
- Split-brain attempts fail safely.

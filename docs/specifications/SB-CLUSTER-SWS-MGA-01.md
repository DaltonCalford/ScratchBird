# SB-CLUSTER-SWS-MGA-01
## ScratchBird Cluster Specification
### Single-Writer-Per-Shard Model (MGA / Firebird-Style Transaction Semantics)

**Status:** Draft – Beta Cluster Target  
**Applies To:** ScratchBird Native v3 Engine  
**Transaction Model:** Firebird-style MGA (Multi-Generational Architecture)  
**Cluster Model:** Shared-nothing, single writer per shard  

---

# 1. Purpose

This document defines the authoritative specification for ScratchBird’s distributed cluster architecture under a **Single-Writer-Per-Shard** model using **MGA (Firebird-style) versioning semantics**.

This specification establishes:

- Cluster membership model
- Shard ownership and routing rules
- Transaction identity semantics
- Snapshot consistency rules across shards
- Replication primitives
- GC / sweep safety rules under MGA
- Fencing and leader enforcement
- Epoch enforcement and correctness invariants
- Cluster-managed domain behavior

This document is normative and directly convertible into an implementation work plan.

---

# 2. Design Goals

1. Preserve ScratchBird’s MGA transaction semantics.
2. Avoid WAL-centric assumptions.
3. Maintain deterministic correctness.
4. Enforce single-writer guarantee per shard.
5. Provide cluster-safe garbage collection.
6. Enable shard-level replication.
7. Provide cross-shard read consistency.
8. Support phased future extension to multi-writer or 2PC.
9. Ensure cluster-managed domains are identical across all nodes.

---

# 3. Definitions

| Term | Definition |
|------|------------|
| Cluster | A set of nodes participating in a shared configuration domain |
| Node | A running ScratchBird instance with `node_id` |
| Shard | A disjoint partition of a database |
| Leader | The sole write-authorized node for a shard |
| Follower | Replica node for a shard |
| GTXID | Global Transaction Identifier `(shard_id, local_txn_id)` |
| CWM | Committed Watermark per shard |
| OST | Oldest Snapshot Transaction per shard |
| RWM | Replication Watermark per shard |
| GC Safe Horizon | Minimum safe transaction boundary for version reclamation |
| Control Plane Log | Replicated configuration log governing cluster state |
| Cluster-Managed Domain | Domain object replicated through control plane and identical across cluster |

---

# 4. Cluster Invariants

The following invariants MUST hold at all times:

## 4.1 Single Writer Invariant
For any given shard, at most one node may accept write transactions.

## 4.2 Fencing Invariant
Every write must include a valid fencing token derived from the shard’s current leader term.

## 4.3 Epoch Invariant
All routing and execution decisions are evaluated against a pinned `cluster_config_epoch`.

## 4.4 MGA Visibility Invariant
Record version visibility must follow Firebird-style transaction semantics.

## 4.5 GC Safety Invariant
No record version may be reclaimed if it may be visible to:
- Any active snapshot in the cluster
- Any follower requiring catch-up
- Any committed snapshot boundary still in use

## 4.6 Domain Identity Invariant
Cluster-managed domains must:
- Have identical UUIDs across all nodes
- Have identical definition hashes across all nodes
- Be replicated via control-plane log

---

# 5. Epoch Model

ScratchBird Cluster SHALL maintain:

- `cluster_config_epoch`
- `security_epoch`
- `schema_epoch`

Each session must carry these epochs and validate them at execution time.

If mismatch occurs:
- Replan OR
- Reject (based on policy)

Domain modifications MUST bump at least `schema_epoch`, and SHOULD bump `cluster_config_epoch` if domains are control-plane managed.

---

# 6. Transaction Identity

## 6.1 Local Transaction ID

Each shard leader maintains a monotonic `local_txn_id`.

## 6.2 Global Transaction ID (GTXID)

```
GTXID := (shard_id, local_txn_id)
```

Properties:
- Globally unique
- Deterministic ordering within shard
- Used for replication, auditing, GC boundaries

UUIDv7 ordering MUST NOT be used for transaction ordering.

---

# 7. Snapshot Semantics Across Shards

## 7.1 Per-Shard Committed Watermark (CWM)

Each shard leader maintains:

```
CWM_shard := highest local_txn_id fully committed and stable
```

## 7.2 Cross-Shard Snapshot Vector

For cross-shard reads:

```
snapshot_vector[shard] = CWM_shard at routing time
```

Each shard executes under its own snapshot boundary.

This yields deterministic per-shard snapshot isolation without requiring global ordering.

---

# 8. Shard Routing

## 8.1 Routing Inputs

- `db_uuid`
- `table_id`
- `shard_key`
- `cluster_config_epoch`

## 8.2 Routing Output

- `shard_id`
- `leader_endpoint`
- `routing_epoch`

## 8.3 Write Enforcement

A write must be rejected if:
- Routing epoch is stale
- Fencing token does not match current leader term
- Node is not current shard leader

---

# 9. Replication Model

## 9.1 Replication Primitive

Each shard leader emits a **Shard Commit Log (SCL)** entry per committed transaction.

Entry includes:
- `local_txn_id`
- `GTXID`
- change payload (logical or physical)
- commit timestamp

## 9.2 Follower Application

Followers apply entries in strictly increasing `local_txn_id` order.

## 9.3 Replication Watermark

Each shard maintains:

```
RWM_shard := highest local_txn_id applied on follower
```

---

# 10. Garbage Collection (MGA Cluster Semantics)

## 10.1 Snapshot Registry

Each node publishes active snapshots:
- session_id
- shard_id
- snapshot_boundary
- start_time
- last_heartbeat

## 10.2 Per-Shard OST

Leader computes:

```
OST_shard := minimum snapshot_boundary of active snapshots
```

## 10.3 GC Safe Horizon

```
GC_safe_shard = min(OST_shard, RWM_shard)
```

A record version may be reclaimed only if:

```
creator_txn_id < GC_safe_shard
```

---

# 11. Leadership and Fencing

## 11.1 Leader Term

Each shard leader has:
- `leader_term`
- `lease_expires_at`

## 11.2 Fencing Token

```
fencing_token = (shard_id, leader_term)
```

Every write request must include the current fencing token.

Engine must validate before commit.

---

# 12. Cluster-Managed Domains

## 12.1 Domain Replication

Domains are replicated through the control-plane log.

Control-plane log entries MUST exist for:
- `DOMAIN_CREATE`
- `DOMAIN_ALTER`
- `DOMAIN_DROP`

## 12.2 Domain Identity

Each domain has:
- `domain_id` (UUIDv7)
- `definition_hash`
- `version_counter`

Domains MUST be identical across cluster nodes.

## 12.3 Join/Rejoin DomainSync

When a node joins or rejoins cluster:
1. Fetch authoritative domain catalog snapshot.
2. Validate `definition_hash` and UUID match.
3. If mismatch: refuse join OR enter explicit repair mode.

---

# 13. Failure Handling

## 13.1 Leader Failure

- Control plane elects new leader.
- `leader_term` increments.
- New fencing token generated.
- Old leader writes rejected.

## 13.2 Split Brain Prevention

Old leader without valid lease MUST reject writes.

## 13.3 Network Partition

Majority partition retains control-plane authority.
Minority partition leaders lose lease and cannot write.

---

# 14. Observability Requirements

Minimum required cluster introspection:

- `SHOW CLUSTER`
- `SHOW NODES`
- `SHOW SHARDS`
- `SHOW SHARD LEADERS`
- `SHOW REPLICATION LAG`
- `SHOW GC HORIZONS`
- `SHOW SNAPSHOT REGISTRY`

Metrics must expose:
- replication lag
- leader term
- active snapshot count
- GC safe horizon
- fencing rejection counts

---

# 15. Non-Goals (Beta Scope)

- Transparent cross-shard ACID multi-write transactions
- Shared-storage multi-writer concurrency
- Global WAL ordering
- Byzantine fault tolerance

---

# 16. Acceptance Criteria

Cluster implementation SHALL be considered beta-ready when:

1. Single-writer-per-shard enforcement is proven via fencing tests.
2. Leader failover works without data loss.
3. Followers replicate commit log correctly.
4. GC safe horizon prevents premature version reclamation.
5. Cross-shard read snapshots are deterministic.
6. Domain replication remains identical across cluster.
7. Split-brain attempts fail safely.

---

# 17. Conclusion

This specification defines a deterministic, MGA-consistent, single-writer-per-shard cluster architecture that:

- Preserves ScratchBird’s Firebird-style semantics
- Avoids WAL coupling
- Maintains correctness through epoch enforcement
- Provides safe replication
- Enforces cluster-managed domain identity
- Enables horizontal scaling by shard

This is the authoritative cluster baseline for ScratchBird Beta.


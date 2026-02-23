# SB-CLUSTER-SWS-MGA-01

**Title:** Single-Writer-Per-Shard Cluster Specification (MGA / Firebird-style)  
**Scope:** ScratchBird cluster mode with shared-nothing shards, single writer per shard, MGA versioning.  
**Non-goals:** Cross-shard ACID with transparent 2PC (may be added later), shared-storage multi-writer.

---

## A. Architecture

### A.1 Entities

- **Cluster**: a set of nodes participating in a configuration epoch domain.
  
- **Node**: a process/host with a `node_id` and credentials.
  
- **Shard**: a disjoint partition of a database’s data. Each shard has:
  
  - a **Leader** (single writer)
    
  - optional **Followers** (read replicas / warm standbys)
    
- **Shard Group**: all replicas of a shard.
  
- **Router**: component (can be embedded in listener/parser) that maps requests to a shard leader.
  

### A.2 Strong invariants (must hold)

1. **Single-writer invariant:** At any time, for each shard, at most one node is the **writer leader**.
  
2. **Config epoch invariant:** Every routing decision is made against a specific `cluster_config_epoch` snapshot.
  
3. **MGA integrity invariant:** Record-version metadata remains correct without relying on WAL ordering.
  
4. **GC safety invariant:** No node may physically reclaim record versions that could still be required by:
  
  - any active snapshot on any node,
    
  - replication catch-up,
    
  - delayed follower.
    

### A.3 Core “epochs”

Maintain three epochs (monotonic):

- `cluster_config_epoch`: membership, shard maps, leadership terms
  
- `security_epoch`: auth policy, CA/cert epochs, cluster trust changes
  
- `schema_epoch`: DDL affecting object definitions / plan caches
  

Every session carries these and every execution checks them (policy determines replan vs fail).

---

## B. Control Plane

### B.1 Control plane log

Use a replicated control-plane log as the **source of truth** for:

- node membership
  
- shard definitions and placements
  
- leader assignments (with term/lease)
  
- routing tables
  
- policy epochs
  

**Minimal requirement:** leader election must be safe (fencing), deterministic, and auditable.

### B.2 Shard leadership: lease + fencing token

For each shard, store:

- `leader_node_id`
  
- `leader_term` (monotonic integer)
  
- `lease_expires_at` (logical time)
  
- `fencing_token = (shard_id, leader_term)` (presented by leader on every write)
  

**Rule:** The engine must reject any write that does not present the current fencing token.

This prevents split-brain writes.

---

## C. Data Plane Overview

### C.1 Routing contract

Router maps each request to:

- `target_shard_id`
  
- `target_leader_endpoint`
  
- `routing_epoch` used
  

Requests that touch multiple shards are executed as:

- scatter-gather reads (initially allowed)
  
- multi-shard writes: either forbidden or require explicit 2PC extension (out of scope here)
  

### C.2 “Single DB per listener” vs “shard-aware listener”

You can implement either:

- **Shard-aware listener**: accepts client connections and routes per statement.
  
- **Shard-per-listener**: each listener instance is bound to a shard leader only.
  

Spec supports both, as long as routing is epoch-pinned.

---

## D. MGA Transaction Model in a Sharded Cluster

### D.1 Transaction identity

Define transaction identity as:

- **LocalTxnId**: monotonically increasing integer per shard leader.
  
- **GlobalTxnId (GTXID)**: `(shard_id, local_txn_id)`
  

This is not “Postgres XID”; it’s a globally unique identifier for auditing, replication, and status queries.

### D.2 TIP semantics per shard

Each shard maintains its own TIP-like structure:

- status: Active | Committed | RolledBack
  
- commit timestamp (optional)
  
- monotonic assignment of local_txn_id
  

**Visibility on a shard** uses MGA rules:

- a snapshot sees committed txns up to its snapshot boundary, plus consistent rules for active txns.

### D.3 Cross-shard read snapshots

When a query spans multiple shards, you need a consistent “read point”.

**Minimal consistent snapshot mechanism (recommended):**

- Each shard exports a **Committed Watermark**:
  
  - `CWM_shard`: highest local_txn_id guaranteed committed and stable
- Router chooses a **read snapshot** as:
  
  - `snapshot_vector[shard] = CWM_shard` at the time routing epoch is pinned

Then each shard executes reads with:

- a snapshot boundary = that shard’s `snapshot_vector[shard]`

This yields a consistent *per-shard* snapshot without global ordering, which is MGA-friendly.

**Note:** This is “vector snapshot” semantics (like per-partition stable points), not a single global timestamp.

### D.4 Cross-shard writes

Out of scope for transparent ACID.  
Baseline rule for this spec:

- Any statement that would perform writes across >1 shard must be rejected **unless** in explicit “multi-shard transaction mode” (future extension).

---

## E. Replication Model

### E.1 What is replicated

Per shard, replicate:

- **transaction outcomes** (commit/rollback)
  
- **page/record deltas** or **logical changes** (choose one; spec supports either)
  
- **metadata deltas** (schema changes that impact shard)
  

### E.2 Replication ordering primitive

Because MGA isn’t WAL-based, you still need ordering for replication.

Two acceptable primitives:

1. **Shard Commit Log (SCL):** append-only log of committed changesets keyed by `(local_txn_id)`
  
2. **Page LSN stream:** monotonic per-page / per-shard LSN (harder)
  

**Recommended for beta:** Shard Commit Log:

- each committed txn emits a changeset event
  
- followers apply in `local_txn_id` order
  

### E.3 Follower read policy

A follower may serve reads only if:

- it has applied up to at least the requested `snapshot_vector[shard]`
  
- otherwise it must forward to leader or wait.
  

---

## F. GC / Sweep in MGA Cluster

### F.1 Horizons (MGA equivalents)

Define these per shard:

- `OAT_shard` (Oldest Active Txn) — min local_txn_id still active
  
- `OST_shard` (Oldest Snapshot Txn) — min snapshot boundary in use
  
- `RWM_shard` (Replication Watermark) — min applied point across followers, if replication is required for durability policy
  

Compute **GC Safe Horizon** per shard:

- `GC_safe_shard = min(OST_shard, RWM_shard)`  
  (and sometimes OAT_shard depending on your exact MGA definitions; OST usually dominates)

Compute **Cluster GC Horizon**:

- `GC_safe_cluster = min_over_all_shards(GC_safe_shard)` only for global resources.
  
- For per-shard storage, you can GC independently per shard *as long as you include follower lag and remote snapshots*.
  

### F.2 GC rules

A shard leader MAY reclaim old record versions only if:

- creator txn id < `GC_safe_shard`
  
- and versions are not required by any pinned snapshot or follower catch-up
  

Followers must also respect `GC_safe_shard` and cannot GC beyond their applied point.

### F.3 Snapshot registry

To compute OST safely, each node must publish active snapshots:

- `(session_id, shard_id, snapshot_boundary, started_at, last_heartbeat)`

The leader for each shard aggregates this to compute OST_shard.

---

## G. DDL / Schema in Cluster

### G.1 Schema propagation

DDL affecting shard layout or storage must be:

- recorded in control-plane log
  
- applied to shard leaders
  
- replicated/applied to followers
  

### G.2 DDL execution policy

Baseline policy:

- DDL is executed through the control plane and applied to leaders first.
  
- Shard leaders reject writes using stale `schema_epoch`.
  

---

## H. Security / Identity

### H.1 Node identity

Each node has:

- `node_id`
  
- cert identity / key (your CA/epoch model)
  
- roles: leader-eligible, follower-only, observer
  

### H.2 Fencing enforcement in engine

On any write, the engine checks:

- routing epoch
  
- shard fencing token
  
- node identity authorization
  

If token mismatch → reject.

---

## I. Failure Handling

### I.1 Leader failure

When leader is suspected dead:

- control plane runs election
  
- assigns new leader with incremented `leader_term`
  
- new leader must:
  
  - prove it has the highest applied SCL position (or otherwise catch up)
    
  - acquire lease
    
  - begin serving writes
    

### I.2 Split brain prevention

- Lease + fencing token ensures only the active leader can commit writes.
  
- Old leader may still run but cannot commit writes due to fencing token mismatch.
  

### I.3 Network partitions

- Majority partition retains control-plane leadership
  
- Minority partition leaders lose lease and cannot write
  

---

## J. Observability and Admin Surface

Minimum admin queries:

- `SHOW CLUSTER`
  
- `SHOW NODES`
  
- `SHOW SHARDS`
  
- `SHOW SHARD LEADERS`
  
- `SHOW REPLICATION LAG`
  
- `SHOW SNAPSHOT REGISTRY`
  
- `SHOW GC HORIZONS`
  

Export metrics:

- leader elections count
  
- replication lag per shard
  
- active snapshot counts
  
- GC safe horizon per shard
  
- rejected writes due to fencing mismatch
  

---

# Workplan Checklist (SB-CLUSTER-XXX)

## Phase 0 — Foundations

- **SB-CLUSTER-001** Add persistent `cluster_id`, `node_id`, `cluster_config_epoch` to DB header or catalog root.
  
- **SB-CLUSTER-002** Implement `TimeSource` abstraction for UUIDv7 and epoch timestamps.
  
- **SB-CLUSTER-003** Implement `StorageLockProvider` abstraction (local-only impl first).
  
- **SB-CLUSTER-004** Implement monotonic per-shard write order primitive (choose: Shard Commit Log recommended).
  

## Phase 1 — Control Plane MVP

- **SB-CLUSTER-010** Implement replicated control-plane log (membership + shard map + leadership).
  
- **SB-CLUSTER-011** Implement node membership states + join/rejoin protocol.
  
- **SB-CLUSTER-012** Implement shard definition model (shard_id, key spec, placements).
  
- **SB-CLUSTER-013** Implement leader election per shard with `leader_term` + `lease_expires_at`.
  
- **SB-CLUSTER-014** Implement fencing token generation and propagation `(shard_id, leader_term)`.
  

## Phase 2 — Router + Epoch Pinning

- **SB-CLUSTER-020** Implement router API: `route(table_id, shard_key) -> shard_id, endpoint, epoch`.
  
- **SB-CLUSTER-021** Implement query epoch pinning: attach `cluster_config_epoch` to query execution.
  
- **SB-CLUSTER-022** Implement write rejection when routing epoch or fencing token is stale.
  
- **SB-CLUSTER-023** Implement “multi-shard write detection” and reject by default.
  

## Phase 3 — MGA Transaction Identity + Status

- **SB-CLUSTER-030** Implement `GlobalTxnId = (shard_id, local_txn_id)` formatting + storage.
  
- **SB-CLUSTER-031** Persist origin metadata where needed (audit, replication, debugging).
  
- **SB-CLUSTER-032** Implement per-shard TIP status query API (local).
  
- **SB-CLUSTER-033** Publish `Committed Watermark (CWM_shard)` from leaders.
  

## Phase 4 — Cross-shard Read Consistency

- **SB-CLUSTER-040** Implement snapshot vector acquisition for scatter-gather reads.
  
- **SB-CLUSTER-041** Implement per-shard read execution under `snapshot_vector[shard]`.
  
- **SB-CLUSTER-042** Implement “read from follower if applied >= snapshot boundary” rule.
  

## Phase 5 — Replication MVP

- **SB-CLUSTER-050** Implement Shard Commit Log (SCL) emission on commit (leader).
  
- **SB-CLUSTER-051** Implement follower apply pipeline (ordered by local_txn_id).
  
- **SB-CLUSTER-052** Implement replication lag tracking and `RWM_shard` publishing.
  
- **SB-CLUSTER-053** Implement follower promotion eligibility checks (highest applied point).
  

## Phase 6 — GC / Sweep Safety

- **SB-CLUSTER-060** Implement snapshot registry publication from all nodes.
  
- **SB-CLUSTER-061** Compute `OST_shard` at leader from snapshot registry.
  
- **SB-CLUSTER-062** Compute `GC_safe_shard = min(OST_shard, RWM_shard)` and enforce it in sweep/GC.
  
- **SB-CLUSTER-063** Add admin visibility for horizons and blocked GC reasons.
  

## Phase 7 — Schema/DDL in Cluster

- **SB-CLUSTER-070** Add `schema_epoch` tracking and attach to plans/sessions.
  
- **SB-CLUSTER-071** Control-plane log entries for DDL changes affecting shards.
  
- **SB-CLUSTER-072** Enforce schema epoch mismatch behavior (replan or reject based on policy).
  

## Phase 8 — Security & Hardening

- **SB-CLUSTER-080** Node identity verification for control-plane participation.
  
- **SB-CLUSTER-081** Enforce leader-only write authorization and fencing checks in engine.
  
- **SB-CLUSTER-082** Audit: elections, leadership changes, shard map edits, replication state changes.
  

## Phase 9 — Observability + Tooling

- **SB-CLUSTER-090** Implement `SHOW CLUSTER/NODES/SHARDS/LEADERS` surfaces.
  
- **SB-CLUSTER-091** Implement `SHOW REPLICATION LAG` + metrics export.
  
- **SB-CLUSTER-092** Implement `SHOW SNAPSHOT REGISTRY` + `SHOW GC HORIZONS`.
  

## Phase 10 — Test Harness (non-negotiable)

- **SB-CLUSTER-100** Deterministic split-brain test: ensure stale leader cannot commit.
  
- **SB-CLUSTER-101** Election/failover test: leader crash → new leader → writes resume.
  
- **SB-CLUSTER-102** Replica lag test: follower behind cannot serve reads beyond its applied point.
  
- **SB-CLUSTER-103** GC safety test: long-running snapshot prevents version reclamation.
  
- **SB-CLUSTER-104** Scatter-gather consistency test: snapshot vector returns consistent results.
  
- **SB-CLUSTER-105** Rejoin test: node rejoins, catches up via SCL, resumes follower role.
  

---

## Beta “credible cluster” milestone (if you want a sharp target)

If you want a clear beta deliverable, I’d define **Beta Cluster MVP** as completion of:

- SB-CLUSTER-010–014 (control plane + fencing)
  
- SB-CLUSTER-020–023 (routing + write gating)
  
- SB-CLUSTER-050–052 (replication basics)
  
- SB-CLUSTER-060–062 (GC safety)
  
- SB-CLUSTER-100–103 (core correctness tests)
  

Everything else can stage in.

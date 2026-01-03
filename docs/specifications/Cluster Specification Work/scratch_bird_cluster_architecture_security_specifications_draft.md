# ScratchBird Cluster Architecture & Security Specifications (Draft)

> **Status:** Draft for architectural review
> **Scope:** Clustered deployment of ScratchBird engine with shard‑local Firebird MVCC, distributed query execution, enterprise‑grade security, and a temporal configuration stack.
> **Authoritative Context:** SBSEC handoff summary, Firebird transactional model, prior design discussions.

---

## 0. Guiding Principles (Non‑Negotiable)

1. **Engine Authority**: The ScratchBird engine is the sole authority for security enforcement, visibility, and execution. Parsers, coordinators, and remote plans are untrusted.
2. **Shard‑Local MVCC**: Firebird MGA/MVCC semantics apply fully and independently within each shard.
3. **No General Cross‑Shard Transactions**: Multi‑shard writes are forbidden. Multi‑shard reads are allowed via distributed execution.
4. **Push Compute to Data**: Remote execution happens on shard owners; results returned are already redacted/masked.
5. **Identical Security Configuration Cluster‑Wide**: Domains, RLS/CLS rules, masking logic, and grants are identical on all enabled members.
6. **One‑Way Upgrades**: Configuration and capability changes are monotonic; rollback is not supported except via explicit destructive operator action.

---

## 1. Temporal Cluster Configuration Stack

### 1.1 Cluster Configuration Epoch (CCE)
A **Cluster Configuration Epoch (CCE)** is a monotonically increasing identifier representing an immutable, cryptographically sealed cluster configuration state.

Each epoch record includes hashes (or references) to:
- CA Policy Bundle
- Security Bundle (domains, RLS/CLS, masking, grants)
- Shard Map
- Scheduler Policy
- Replication Policy
- Backup Policy
- Protocol / Capability Matrix (optional)

Epoch records are:
- Append‑only
- Hash‑chained to the previous epoch
- Signed by authorized cluster administrators

### 1.2 Published vs Effective Epoch
- **Published Epoch**: Written to the replicated control plane log.
- **Effective Epoch**: All enabled members have converged and peer‑observed compliance is recorded.

Distributed operations requiring strong guarantees (distributed query execution, scheduler control, CA cutover) MUST require the epoch to be *effective*.

### 1.3 Replicated Control Plane Log
- Strongly consistent (single leader, fenced writes)
- Stores epoch records, convergence evidence, membership state
- Serves as the authoritative audit spine

---

## 2. Cluster Membership & Identity

### 2.1 Node Identity
Each node has:
- `node_uuid` (stable)
- `cluster_uuid`
- Cryptographic identity (X.509 certificate by default)

Certificates MUST cryptographically bind:
```
sb://cluster/<cluster_uuid>/node/<node_uuid>
```

### 2.2 Authentication
- **Mandatory mTLS** for all cluster RPC
- TLS 1.3 recommended
- Ed25519 or ECDSA keys preferred

Kerberos/GSSAPI MAY be supported as an additional enterprise integration, but MUST NOT replace mutual authentication.

### 2.3 Authorization
- Valid certificate is **necessary but not sufficient**
- Node MUST be present and enabled in the cluster membership list
- Roles (voter, shard owner, learner, agent‑only) are centrally authorized

### 2.4 Enabled vs Disabled Members
- Only **enabled members** count for quorum, convergence, and cutover gates
- Disabling a member is an explicit, audited administrative action
- Re‑enable requires full re‑validation

---

## 3. CA Policy & Rotation

### 3.1 CA Policy Bundle
Defines trusted roots, key requirements, validity constraints, and acceptance rules.

- Versioned and bound to a CCE
- No implicit trust from backups or restored images

### 3.2 Rotation Model (Overlap + Cutover)
1. Publish overlap policy (CA_A + CA_B)
2. Require all enabled members to activate overlap epoch
3. Issue CA_B certificates
4. Peer‑observed convergence (all enabled members)
5. Stability window
6. Publish cutover policy (CA_B only)

### 3.3 Peer Observation (Mandatory)
- Nodes are validated by independent peers
- Observations include cert chain, issuer, node_uuid, cluster_uuid
- Self‑report is insufficient

### 3.4 No Rollback Rule
- CA policy epochs are monotonic
- Operators MAY force cutover (destructive, audited, explicit acknowledgement required)

---

## 4. Security Bundle

### 4.1 Contents
- Domains
- Row‑Level Security (RLS)
- Column‑Level Security (CLS)
- Domain‑level masking rules
- Grants and role mappings

### 4.2 Distribution & Enforcement
- Identical bundle on all enabled members
- Canonical representation and hash
- Bound to CCE

### 4.3 Execution Gate
Distributed query execution and scheduler control require:
- Matching security bundle hash/epoch
- Otherwise, operation degrades to local‑only or is rejected

---

## 5. Sharding & Data Placement

### 5.1 Shard Model
- Shared‑nothing shards
- Each shard is a full Firebird‑style MVCC universe

### 5.2 Shard Map
- Versioned
- Bound to CCE
- Defines routing rules, ownership, replicas

### 5.3 Resharding (Future‑Capable)
- Versioned shard maps
- Defined migration phases (read‑only, dual‑read, cutover)
- No cross‑shard writes

---

## 6. Distributed Query Execution

### 6.1 Coordinator Role
- Parse and plan
- Determine shard routing
- Generate subplans
- Merge results

### 6.2 Shard Execution
Shard nodes MUST:
- Treat subplans as untrusted
- Bind immutable transaction Security Context
- Resolve objects by UUID
- Enforce RLS/CLS and domain masking before tuple visibility
- Emit audit events

### 6.3 Consistency Modes
- Read committed (cluster)
- Best‑effort snapshot
- Bounded staleness

No global snapshot guarantee.

---

## 7. Replication Modes

### 7.1 Logical Commit Log Replication
- Ships committed transaction changes
- Target is a separate database
- Suitable for read replicas, analytics, migration

### 7.2 Physical Shadow Replication
- Ships modified on‑disk pages
- Target is a failover clone of the same database
- Suitable for HA and rapid promotion

### 7.3 Fencing & Promotion
- Term/epoch based fencing
- Old leaders cannot write after loss of authority

---

## 8. Backup, Restore & PITR

### 8.1 Per‑Shard Backups
- Independent backup chains
- Local retention policies

### 8.2 Cluster‑Consistent Backup Sets (Optional)
- Coordinator records per‑shard checkpoints
- Enables coherent cluster restore

### 8.3 Restore Rules
- Restore does NOT restore trust
- Nodes must re‑enroll and be re‑enabled

---

## 9. Scheduler (Cluster‑Controlled)

### 9.1 Architecture
- Scheduler Control Plane (SCP)
- Distributed agents on each node

### 9.2 Job Classes
- LOCAL_SAFE
- LEADER_ONLY
- QUORUM_REQUIRED

### 9.3 Partition Rules
- QUORUM_REQUIRED jobs MUST NOT run under partition
- LEADER_ONLY requires proven leadership
- LOCAL_SAFE may continue with bounded behavior

---

## 10. Observability & Audit

- Mandatory audit for:
  - membership changes
  - epoch changes
  - CA operations
  - distributed execution
- Distributed query tracing
- Connection churn metrics
- Peer observation records

---

## 11. Killer Feature Enablement (By Design)

This architecture enables future implementation of:
- Online resharding
- Rolling upgrades
- Self‑healing replication
- Causal reads
- Time‑travel queries
- Certified, auditable deployments for government and regulated environments

---

**End of Draft**


# SBCLUSTER-00: Guiding Principles

## Core Architectural Principles

The ScratchBird cluster architecture is built on these foundational principles that inform all design decisions across the specification suite:

### 1. Engine Authority
**The ScratchBird engine is the authoritative source of truth.**
- Cluster orchestration serves the engine, not the other way around
- Schema, transactions, and data semantics are engine-defined
- Cluster components must respect engine invariants (e.g., Firebird MGA)

### 2. Shard-Local MVCC
**Each shard maintains independent MVCC state.**
- No cross-shard transactions
- Each shard has its own transaction ID space
- Distributed queries coordinate at query level, not transaction level
- Guarantees: Firebird MGA semantics within each shard

### 3. No Cross-Shard Transactions
**Transactions are strictly shard-local.**
- Two-phase commit is explicitly rejected (complexity, latency, failure modes)
- Applications must use application-level sagas for multi-shard consistency
- Query coordinator can read from multiple shards (snapshot isolation per shard)

### 4. Push Compute to Data
**Minimize data movement; execute where data resides.**
- Filter/aggregate on shard nodes before returning results
- Use shard-local indexes and query plans
- Coordinator merges results, does not re-filter

### 5. Identical Security Configuration
**All nodes in a cluster share the same security policy.**
- Cluster Configuration Epoch (CCE) distributes uniform security bundle
- No per-node security variance (simplifies reasoning, prevents drift)
- Certificate Authority (CA) is cluster-wide

### 6. One-Way Upgrades
**Cluster configuration epochs only move forward.**
- CCE epoch numbers are monotonically increasing
- Nodes cannot downgrade to older CCEs
- Ensures security policies only strengthen over time

### 7. Trust Boundary Enforcement
**Restore does not restore trust.**
- Certificates, private keys, CA bundles are NEVER backed up
- Restored clusters must re-establish trust from scratch
- Data is backed up; cryptographic material is not

### 8. Immutable Audit Chain
**Audit events are cryptographically linked and tamper-evident.**
- Each audit event hashes previous event (Merkle chain)
- Digital signatures prevent forgery
- Audit log is append-only and permanent

### 9. Consensus Over Configuration
**Use Raft consensus for critical cluster state.**
- CCE distribution via Raft
- Membership changes via Raft
- Ensures cluster-wide agreement on critical state

### 10. Observability by Design
**Metrics, tracing, and audit are first-class citizens.**
- OpenTelemetry-native instrumentation
- Cryptographic audit chain for compliance
- Real-time cluster health visibility

---

## Design Implications

These principles cascade through the specification suite:

| Principle | Specification Impact |
|-----------|---------------------|
| **Engine Authority** | Parser, transaction model, storage format all defer to ScratchBird engine semantics |
| **Shard-Local MVCC** | SBCLUSTER-05 (Sharding), SBCLUSTER-06 (Distributed Query) |
| **No Cross-Shard Txns** | SBCLUSTER-06 (query coordination, not transaction coordination) |
| **Push Compute to Data** | SBCLUSTER-06 (distributed execution), SBCLUSTER-09 (scheduler partition rules) |
| **Identical Security Config** | SBCLUSTER-01 (CCE), SBCLUSTER-04 (Security Bundle) |
| **One-Way Upgrades** | SBCLUSTER-01 (CCE epoch monotonicity) |
| **Trust Boundary** | SBCLUSTER-08 (Backup/Restore), SBCLUSTER-02 (Membership) |
| **Immutable Audit** | SBCLUSTER-10 (Observability), Security Spec 08 (Audit) |
| **Consensus Over Config** | SBCLUSTER-01 (CCE distribution), SBCLUSTER-02 (Membership via Raft) |
| **Observability** | SBCLUSTER-10 (metrics, tracing, audit) |

---

## What This Means for Implementation

**Simplicity through Constraints**:
- Rejecting cross-shard transactions eliminates 2PC complexity
- Shard-local MVCC means no distributed snapshot coordination
- Identical security config means no per-node policy management

**Operational Clarity**:
- One-way upgrades prevent configuration drift
- Immutable audit provides compliance evidence
- Trust boundary enforcement clarifies backup/restore semantics

**Performance Characteristics**:
- Pushing compute to data minimizes network transfer
- Shard-local transactions avoid distributed coordination overhead
- Raft consensus used only for critical cluster state (not every transaction)

---

## Trade-Offs Accepted

These principles intentionally trade certain capabilities for simplicity and reliability:

| Trade-Off | What We Give Up | What We Gain |
|-----------|----------------|--------------|
| No cross-shard transactions | ACID across shards | No 2PC complexity, better availability |
| Shard-local MVCC | Global snapshot isolation | Independent shard scaling, no distributed clock |
| Identical security config | Per-node customization | Simplified reasoning, no policy drift |
| One-way upgrades | Rollback flexibility | Strong forward security guarantees |
| Trust boundary | Backup portability | Clear security model, no key escrow risk |

---

## Relationship to Other Documents

- **SBCLUSTER-01 to SBCLUSTER-10**: Detailed specifications implementing these principles
- **Security Design Specification**: Security principles aligned with cluster architecture
- **MGA_RULES.md**: Engine-level transaction semantics that cluster must respect
- **EMULATED_DATABASE_PARSER_SPECIFICATION.md**: Parser design respecting engine authority

---

**Document Status**: DRAFT (Beta Specification Phase)
**Version**: 1.0
**Date**: 2026-01-02
**Author**: D. Calford

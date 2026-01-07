
# ScratchBird Database Engine

Firebird-style MGA database engine with multi-dialect wire compatibility (Firebird, MySQL, PostgreSQL)
and the ScratchBird SBLR execution layer.

**Current Phase:** Alpha (completing) – Pre-Beta preparation

---

## Status Overview

- **Alpha 1:** Core MGA storage and transaction visibility – ✅ Complete
- **Alpha 2:** Parser V2 and multi-dialect SQL support – ✅ Complete
- **Alpha 3:** Security context, authentication, auditing – ✅ Complete
- **Alpha (Current):** Domain DDL (Plan 04) – 🚧 60% complete

> Note: Physical sweep reclamation and some isolation variants are scheduled for Beta.

---

## Architecture Highlights

- **Transaction Model:** Firebird-style MGA (no WAL)
  - Record versioning via xmin/xmax
  - Transaction markers: NEXT, OIT, OAT, OST
  - Snapshot-based isolation
- **Durability:** Ordered page writes and transaction markers (non-WAL model)
- **Execution Engine:** ScratchBird Bytecode Language Runtime (SBLR)

---

## Index Support

### Implemented
- B-Tree
- BRIN
- SP-GiST
- Bitmap
- R-Tree
- HNSW (vector similarity)
- LSM Tree

### Planned / Partial
- GIN
- GiST
- Column-oriented storage

---

## SQL & Compatibility

- Native ScratchBird SQL (Parser V2)
- Firebird, PostgreSQL, MySQL dialect parsers
- Extensive compatibility test coverage for supported features

> Compatibility tests validate syntax and supported semantics; full behavioral equivalence is a Beta/GA goal.

---

## Garbage Collection & Sweep

- MGA visibility rules fully implemented
- Sweep trigger logic implemented (OST − OIT)
- Physical reclamation of old record versions and index cleanup: **Planned (Beta)**

---

## Security

- Authentication: password, SCRAM, cert, MFA, OAuth, Kerberos
- Auditing and security context binding
- TLS infrastructure

---

## Cluster (Beta – Specification Complete)

Cluster features are **specified, not yet implemented**:

- Raft-based configuration consensus
- Sharding and distributed query execution
- Asynchronous logical replication (cluster-level WAL distinct from core engine)
- mTLS and PKI infrastructure
- Backup/restore and observability

---

## Development Status

ScratchBird is a late-Alpha system with a strong architectural foundation.
Focus areas before Beta:
- Complete Plan 04 (Domain DDL)
- Finalize GC and sweep reclamation
- Harden isolation-level edge cases

---

**Project Goal:** Deliver a secure, Firebird-inspired MGA engine with modern execution and a fully specified distributed future.

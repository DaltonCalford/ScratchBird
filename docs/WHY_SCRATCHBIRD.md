# Why ScratchBird (Alpha)

## Purpose

This document describes **why ScratchBird exists** and the **current Alpha
benefits** compared to MySQL, PostgreSQL, and FirebirdSQL.

ScratchBird is a **Firebird/InterBase modernization successor** that keeps the
single-file, embedded-first strengths but rebuilds the engine to be more secure,
more extensible, and easier to evolve. Alpha focuses on a solid core engine that
is ready for embedded use, local IPC, and network use.

## Alpha Benefits

Alpha specifications are implemented and will be part of the initial public release. The benefits below reflect the **Alpha scope** as it exists
today and as it will ship.

### 1) MGA transaction engine (fast commit, non-blocking reads)

- ScratchBird uses **MGA (Multi-Generational Architecture)**: updates create new
  versions while older versions remain visible to active snapshots.
- **Commit is fast** (flag flip), **readers do not block writers**, and **writers
  do not block readers** in the common case.
- **No mandatory WAL** in the core engine (optional write-after log only).

**Comparison**

- **PostgreSQL/MySQL**: WAL-based durability and background vacuum/undo.
- **FirebirdSQL**: MGA exists, but ScratchBird extends this model with a
  redesigned bytecode pipeline and stricter parser isolation.

### 2) SBLR bytecode VM with untrusted parser model

- ScratchBird executes **SBLR** (ScratchBird Bytecode), not raw SQL.
- Parsers are treated as **untrusted clients**: SQL is compiled to SBLR, then
  **verified and audited** by the engine before execution.
- SBLR enables **shared bytecode caching** and avoids name resolution at runtime
  by using UUID-based object IDs.

**Comparison**

- **PostgreSQL/MySQL**: SQL is interpreted/planned directly in the engine.
- **FirebirdSQL**: BLR exists, but ScratchBird's SBLR is a redesign with
  explicit verification and VM-driven execution.

### 3) Unified catalog with UUID-based object IDs

- All objects are identified by **UUID** internally.
- Names are resolved once, then execution is ID-based.
- This design makes **schema evolution, auditing, and multi-dialect emulation**
  more deterministic.

**Comparison**

- Most engines rely on name/oid resolution repeatedly in executor paths.

### 4) Core storage engine ready for Alpha workloads

- DML heap operations (insert/select/update/delete) are implemented.
- TOAST plumbing exists for large/varlen data.
- Multiple index families are implemented in core.
- Catalog persistence is implemented for schemas/tables/columns/indexes/permissions.

**Comparison**

- ScratchBird keeps Firebird's embedded strengths while extending index and
  catalog coverage beyond traditional Firebird/InterBase defaults.

### 5) Security baseline with engine-side enforcement

- Auth providers and SCRAM support exist.
- Executor-level permission checks are implemented for DML/DDL.
- Row-level security (RLS) hooks are present and expanding.

**Comparison**

- ScratchBird treats parser and clients as **untrusted** by default, enforcing
  permissions at the engine level for every statement path.

### 6) Listener/pool/parser architecture for emulation

- Native engine runs independently of dialect parsers.
- Emulated protocol parsers (Firebird/MySQL/PostgreSQL) connect to the engine
  through a strict conversion layer.
- This architecture supports **drop-in replacement** targets without polluting
  core engine semantics.

### 7) Integrated Git support (engine-first)

- Native Git integration for schema and metadata versioning.
- Repository-backed workflows and auditability without external tooling glue.

**Comparison**

- Most engines rely on external migration tooling or CI pipelines for versioning.

## Alpha Deployment Models (Implemented)

### Embedded (single-user)

- The engine runs **in-process** as a library.
- No external listener required.
- Best for local tools, embedded apps, and test harnesses.

### Local shared server (IPC)

- Engine runs as a **local server** with IPC connections.
- Multiple local clients share a single engine instance.
- Useful for workstation teams or multi-tool local setups.

### Network server (INET)

- Engine runs as a **network server** with listeners on configured ports.
- Clients connect over network protocols (native or emulated).
- Suitable for shared or remote environments.

## Single-Database Security Configuration (Alpha)

Alpha ships a **single-database security model** that applies consistently
across embedded, IPC, and network deployments:

- **Authentication**: SCRAM-capable authentication and local auth providers.
- **Authorization**: GRANT/REVOKE at object and column scope.
- **Row-Level Security (RLS)**: policy hooks defined at the engine level.
- **Domain-level safeguards**: constraints, masking, and validation are tied to
  the data type itself (domains are enforced by the engine).

**Configuration entry points**

- Server configuration: `docs/user-documentation/configuration/sb_server.conf.md`
- Access control and HBA rules: `docs/user-documentation/configuration/hba.conf.md`
- TLS configuration: `docs/user-documentation/configuration/ssl-setup.md`

## Practical Alpha Differentiators (Summary)

- **MGA without mandatory WAL**: fast commit + non-blocking reads.
- **Bytecode VM with validation**: smaller injection surface, predictable
  execution.
- **UUID object IDs**: faster resolution, cleaner auditing, stable mapping.
- **Embedded-first, single-file friendly**: Firebird-style manageability.
- **Strict parser isolation**: emulation without trusting external clients.
- **Integrated Git workflows**: engine-native versioning and audit trails.

## Alpha Scope Notes

Alpha is focused on a **fully correct core engine**. Dialect parity and
ecosystem integrations continue to expand, but the engine foundation is the
priority. For exact status, see:

- `docs/planning/ALPHA_COMPLETION_MASTER_PLAN.md`
- `docs/findings/ENGINE_CORE_IMPLEMENTATION_AUDIT.md`
  For Beta roadmap items (autoscaling, streaming, NoSQL), see:
- `docs/WHERE_WE_ARE_GOING_BETA.md`

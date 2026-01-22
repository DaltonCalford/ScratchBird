# ScratchBird Core Engine Unified Specification (Alpha)

## 1. Purpose
Define a single, authoritative core-engine specification for Alpha that
consolidates storage, catalog, transactions, execution, and persistence
requirements. This document explicitly excludes parser, network, cluster,
driver, and tooling concerns.

## 2. Scope

### In Scope (Core Engine)
- Storage engine and on-disk format
- Tablespaces/filespaces and GPID addressing
- Catalog and schema system (metadata persistence)
- Transaction system (MGA), locking, GC/sweep
- SBLR execution pipeline (bytecode, executor)
- DDL/DML execution semantics (not parser syntax)
- Index types, persistence, and maintenance
- Query optimizer and execution (Alpha core behaviors)
- Security core (identity/auth, authorization, RLS, audit)
- Monitoring and observability (system views + metrics)
- Backup/restore
- Scheduler/job system (engine maintenance jobs)
- UDR runtime and catalog persistence

### Out of Scope
- SQL parsers and dialect emulation
- Wire protocols and listener/pool networking
- Drivers and client libraries
- Cluster/sharding/replication (Beta)
- Remote DB UDR connectors
- CLI tooling and deployment

## 3. Source Specifications
This unified spec is derived from the following core documents:
- Storage: `docs/specifications/storage/*`, `docs/specifications/compression/*`
- Catalog: `docs/specifications/catalog/*`
- Transactions: `docs/specifications/transaction/*`
- Indexes: `docs/specifications/indexes/*`
- Types: `docs/specifications/types/*`
- DDL/DML: `docs/specifications/ddl/*`, `docs/specifications/dml/*`, `docs/specifications/triggers/*`
- SBLR: `docs/specifications/sblr/*`
- Security: `docs/specifications/Security Design Specification/*` (engine-core sections only)
- Operations/Monitoring: `docs/specifications/operations/*`
- Backup/Restore: `docs/specifications/BACKUP_AND_RESTORE.md`
- Scheduler: `docs/specifications/scheduler/*`
- Memory/Cache: `docs/specifications/MEMORY_MANAGEMENT.md`, `docs/specifications/core/CACHE_AND_BUFFER_ARCHITECTURE.md`
- Temporary tables: `docs/specifications/TEMPORARY_TABLES_SPECIFICATION.md`

## 4. Core Principles

### CORE-PRINCIPLE-001: MGA Transactions (No WAL in Alpha)
ScratchBird uses Multi-Generational Architecture (MGA) for concurrency.
Write-after log (WAL) is optional post-gold only (replication/PITR).
Source: `docs/specifications/transaction/TRANSACTION_MGA_CORE.md`,
`docs/specifications/storage/MGA_IMPLEMENTATION.md`.

### CORE-PRINCIPLE-002: UUID-First Metadata
All catalog objects use UUID IDs internally; names are user-facing only.
Source: `docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`.

### CORE-PRINCIPLE-003: SBLR Execution Boundary
SQL is converted to SBLR before execution; engine validates SBLR.
Source: `docs/specifications/sblr/*`.

### CORE-PRINCIPLE-004: Schema Hierarchy
Canonical schema roots: `root`, `sys`, `app`, `users`, `remote/emulation`.
Public schema is `/users/public` (not root). No `/emulated` root.
Source: `docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`,
`docs/specifications/catalog/SCHEMA_PATH_RESOLUTION.md`.

## 5. Storage Engine

### CORE-STORAGE-001: On-Disk Format and Page Types
- Page header and catalog root are fixed and versioned.
- Page types include heap, index, FSM, catalog, and specialized pages.
Source: `docs/specifications/storage/ON_DISK_FORMAT.md`,
`docs/specifications/storage/STORAGE_ENGINE_PAGE_MANAGEMENT.md`.

### CORE-STORAGE-002: Page Sizes
- Engine supports configured page sizes (8K–128K).
Source: `docs/specifications/storage/EXTENDED_PAGE_SIZES.md`.

### CORE-STORAGE-003: Buffer Pool and Cache
- Shared buffer pool with pin/unpin, eviction policy, and dirty page flush.
- Cache statistics and observability are required.
Source: `docs/specifications/storage/STORAGE_ENGINE_BUFFER_POOL.md`,
`docs/specifications/core/CACHE_AND_BUFFER_ARCHITECTURE.md`.

### CORE-STORAGE-004: Tablespaces and GPID
- Objects are placed by tablespace; GPID addresses are required.
- Multi-file tablespaces are supported in Alpha.
Source: `docs/specifications/storage/TABLESPACE_SPECIFICATION.md`,
`docs/specifications/storage/TABLESPACE_ONLINE_MIGRATION.md`.

### CORE-STORAGE-005: Heap + TOAST/LOB
- Heap tuples store version chains per MGA.
- Large values stored in TOAST/LOB; TOAST uses tablespace of parent table.
Source: `docs/specifications/storage/TOAST_LOB_STORAGE.md`,
`docs/specifications/storage/HEAP_TOAST_INTEGRATION.md`.

### CORE-STORAGE-006: Compression
- Optional page compression and compressed page I/O.
Source: `docs/specifications/compression/COMPRESSION_FRAMEWORK.md`.

## 6. Catalog and Schema System

### CORE-CATALOG-001: Catalog Root + Tables
- Catalog root page (id 3) stores pointers to system tables.
- Catalog tables persist schemas, tables, columns, indexes, constraints,
  sequences, views, triggers, permissions, statistics, dependencies.
Source: `docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`.

### CORE-CATALOG-002: Schema Path Resolution
- Search path resolution is deterministic; `public` resolves to `users.public`.
- Emulated sessions are scoped under `/remote/emulation`.
Source: `docs/specifications/catalog/SCHEMA_PATH_RESOLUTION.md`,
`docs/specifications/catalog/SCHEMA_PATH_SECURITY_DEFAULTS.md`.

### CORE-CATALOG-003: Object Persistence and Dependency Tracking
- All SQL objects must be persistent in catalog tables.
- Dependency graph is maintained for CASCADE and validation.
Source: `docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`,
`docs/specifications/ddl/CASCADE_DROP_SPECIFICATION.md`.

**Optional Alpha:** Materialized object-name registry for fast lookup
(`docs/specifications/alpha_requirements/optional/OBJECT_NAME_REGISTRY_MATERIALIZED_VIEW.md`).

## 7. Transactions, Locking, and GC

### CORE-TXN-001: Transaction Lifecycle
- Begin/commit/rollback and snapshot visibility semantics per MGA.
Source: `docs/specifications/transaction/TRANSACTION_MAIN.md`,
`docs/specifications/transaction/TRANSACTION_MGA_CORE.md`.

### CORE-TXN-002: Lock Manager
- Table/row locks, deadlock detection, and configurable timeouts.
Source: `docs/specifications/transaction/TRANSACTION_LOCK_MANAGER.md`.

### CORE-TXN-003: GC and Sweep
- Background GC and sweep behaviors per Firebird model.
- Sweeping is primary maintenance (VACUUM is alias only).
Source: `docs/specifications/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md`.

## 8. DDL and DML Execution Semantics

### CORE-DDL-001: Object Lifecycle (Execution)
- CREATE/ALTER/DROP for schemas, tables, indexes, sequences, views,
  triggers, procedures, functions, roles, groups, users, UDRs.
Source: `docs/specifications/ddl/*`.

### CORE-DDL-002: Table Features
- Temporary tables, partitioning, row-level security, temporal tables,
  foreign data objects (as catalog entries even if runtime is deferred).
Source: `docs/specifications/ddl/DDL_TABLES.md`,
`docs/specifications/TEMPORARY_TABLES_SPECIFICATION.md`,
`docs/specifications/ddl/DDL_ROW_LEVEL_SECURITY.md`,
`docs/specifications/ddl/DDL_TABLE_PARTITIONING.md`,
`docs/specifications/ddl/DDL_TEMPORAL_TABLES.md`.

### CORE-DML-001: DML Operations
- SELECT/INSERT/UPDATE/DELETE/MERGE semantics, RETURNING, conflict handling.
Source: `docs/specifications/dml/*`.

### CORE-TRG-001: Trigger Semantics
- Trigger context variables and execution points (BEFORE/AFTER/INSTEAD).
Source: `docs/specifications/triggers/*`, `docs/specifications/ddl/DDL_TRIGGERS.md`.

## 9. Indexes

### CORE-IDX-001: Index Types
- B-tree, Hash, Bitmap, Columnstore, LSM, R-tree, GIN, GiST, SP-GiST, BRIN,
  Vector/HNSW, Full-text (GIN-based).
Source: `docs/specifications/indexes/*`, `docs/specifications/ddl/DDL_INDEXES.md`.

### CORE-IDX-002: Index Versioning and GC
- Index rebuild/version metadata and GC protocol are required.
Source: `docs/specifications/indexes/INDEX_GC_PROTOCOL.md`,
`docs/specifications/indexes/INDEX_IMPLEMENTATION_SPEC.md`.

### CORE-IDX-003: Migration-Safe Index Maintenance
- TID updates and index revalidation must exist for all index types.
Source: `docs/specifications/storage/TABLESPACE_SPECIFICATION.md`.

## 10. Query Optimizer and Execution

### CORE-QRY-001: Statistics and Cost Model
- Stats collection, per-table/index counts, and cost-based planning.
Source: `docs/specifications/query/QUERY_OPTIMIZER_SPEC.md`,
`docs/specifications/operations/MONITORING_SQL_VIEWS.md`.

### CORE-QRY-002: Execution Pipeline
- Plan caching and SBLR execution path.
Source: `docs/specifications/sblr/*`, `docs/specifications/query/QUERY_OPTIMIZER_SPEC.md`.

### CORE-QRY-003: Parallel Execution (Beta)
- Parallel query execution is Beta-only (not required for Alpha).
Source: `docs/specifications/query/PARALLEL_EXECUTION_ARCHITECTURE.md`.

## 11. Types and Serialization

### CORE-TYPE-001: Primitive and Complex Types
- Numeric, text, binary, datetime, UUID, arrays, geometry types.
Source: `docs/specifications/types/*`.

### CORE-TYPE-002: On-Disk Encoding
- TypedValue serialization and casting rules.
Source: `docs/specifications/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md`.

## 12. Security (Engine Core)

### CORE-SEC-001: Identity and Authentication
- Pluggable auth methods, password storage, external auth integration.
Source: `docs/specifications/Security Design Specification/02_IDENTITY_AUTHENTICATION.md`,
`docs/specifications/Security Design Specification/AUTH_*`.

### CORE-SEC-002: Authorization and Roles
- Users, roles, groups, memberships, privilege enforcement.
Source: `docs/specifications/Security Design Specification/03_AUTHORIZATION_MODEL.md`,
`docs/specifications/Security Design Specification/ROLE_COMPOSITION_AND_HIERARCHIES.md`.

### CORE-SEC-003: Audit and Compliance (Engine-Side)
- Audit events, chains, and storage requirements.
Source: `docs/specifications/Security Design Specification/08_AUDIT_COMPLIANCE.md`.

## 13. Monitoring and Observability

### CORE-MON-001: System Views
- `sys.sessions`, `sys.locks`, `sys.performance`, `sys.statements`, etc.
Source: `docs/specifications/operations/MONITORING_SQL_VIEWS.md`.

### CORE-MON-002: Metrics
- Prometheus metrics with core engine counters and IO stats.
Source: `docs/specifications/operations/PROMETHEUS_METRICS_REFERENCE.md`.

## 14. Backup and Restore

### CORE-BACKUP-001: Full Coverage
- Include all tablespace files and catalog objects.
Source: `docs/specifications/BACKUP_AND_RESTORE.md`.

## 15. Scheduler and Jobs

### CORE-SCHED-001: Job System
- Secure job runner for maintenance tasks (GC, sweep, stats, index rebuild).
Source: `docs/specifications/scheduler/*`.

## 16. UDR Runtime (Engine Core)

### CORE-UDR-001: Catalog and Lifecycle
- UDRs are catalog-persisted objects with permissions and dependencies.
Source: `docs/specifications/udr/10-UDR-System-Specification.md`,
`docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`.

## 17. Non-Core References (Explicitly Excluded)
Parsers, wire protocols, drivers, network listeners, cluster/sharding,
and remote-DB connectors are tracked in their own specs and are not part
of this unified core engine specification.

## 18. Alpha Gap Sequencing (Implementation Order)
1. Catalog bootstrap schema roots (remove `/emulated`, move `/public` to `/users/public`).
2. Tablespace routing + GPID wiring for DDL/DML (including multi-file tablespaces).
3. Index TID updates + migration-safe rebuilds for all index types.
4. Scheduler/job system (maintenance jobs: sweep/GC/stats/index rebuild).
5. Constraint enforcement (PK/FK/UNIQUE/CHECK/NOT NULL + dependency validation).

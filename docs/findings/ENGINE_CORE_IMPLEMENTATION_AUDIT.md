# Engine Core Implementation Audit (Alpha)

## Purpose
Compare the unified core-engine specification against code-truth and list
gaps that block a fully implemented Alpha engine.

## Inputs
- Unified spec: `docs/specifications/core/ENGINE_CORE_UNIFIED_SPEC.md`
- Prior audits:
  - `docs/findings/SPECIFICATIONS_ALPHA_IMPLEMENTATION_AUDIT_DEEP.md`
  - `docs/findings/CACHE_AND_BUFFER_IMPLEMENTATION_REVIEW.md`
  - `docs/findings/TABLESPACE_IMPLEMENTATION_AUDIT.md`
  - `docs/findings/INDEX_IMPLEMENTATION_GAPS-completed.md`
  - `docs/findings/MGA_GC_THREAD_AUDIT.md`
  - `docs/findings/SCHEDULER_JOB_RUNNER_AUDIT.md`

## Summary
- **Implemented (evidence)**: core MGA transaction lifecycle, sweep/GC loop, basic
  storage engine DML, catalog persistence, optimizer runtime, and TOAST plumbing.
- **Partial**: tablespace routing, security
  enforcement (view definer/RLS SELECT), monitoring view parity, statistics
  coverage, and some catalog behaviors (schema roots).
- **Missing**: full tablespace DDL wiring in parsers.

## Findings by Core Area

### 1) Storage Engine

**Implemented (evidence)**
- DML heap operations (insert/select/update/delete) and TOAST support:
  `ScratchBird/src/core/storage_engine.cpp:432`
- Compressed page manager exists:
  `ScratchBird/src/core/compressed_page_manager.cpp:10`
- Index TID updates implemented across index types for tablespace migration:
  `ScratchBird/src/core/catalog_manager.cpp:11895-12443`

**Partial / Missing**
- Tablespace-aware DML paths are incomplete (GPID routing not fully wired).
  See `docs/findings/TABLESPACE_IMPLEMENTATION_AUDIT.md` (F-TS-003).
- Multi-file tablespace support is defined but unused.
  See `docs/findings/TABLESPACE_IMPLEMENTATION_AUDIT.md` (F-TS-010).
- Tablespace header transaction fields never updated.
  See `docs/findings/TABLESPACE_IMPLEMENTATION_AUDIT.md` (F-TS-007).

### 2) Catalog and Schema System

**Implemented (evidence)**
- Catalog root and extended system tables exist:
  `ScratchBird/src/core/catalog_manager.cpp:290`
- Object persistence (schemas/tables/columns/indexes/permissions):
  `ScratchBird/src/core/catalog_manager.cpp:7212`

**Missing / Spec Mismatch**
- Bootstrap still creates `/emulated` root and `/public` under root rather than
  `/remote/emulation` and `/users/public`:
  `ScratchBird/src/core/catalog_manager.cpp:1963-2035`

### 3) Transactions, Locking, and GC

**Implemented (evidence)**
- Begin/commit/rollback:
  `ScratchBird/src/core/transaction_manager.cpp:269`,
  `ScratchBird/src/core/transaction_manager.cpp:366`,
  `ScratchBird/src/core/transaction_manager.cpp:850`
- Sweep manager:
  `ScratchBird/src/core/sweep_manager.cpp:26`
- Garbage collector:
  `ScratchBird/src/core/garbage_collector.cpp:52`

**Partial / Spec Cleanup**
- Sweep logging uses VACUUM tag; spec prefers SWEEP terminology.
  See `docs/findings/SPECIFICATIONS_ALPHA_IMPLEMENTATION_AUDIT_DEEP.md`.

### 4) DDL/DML Execution Semantics

**Implemented (evidence)**
- Core DDL/DML dispatch exists in executor and catalog manager.
  See `docs/findings/SPECIFICATIONS_ALPHA_IMPLEMENTATION_AUDIT_DEEP.md`.

**Missing / Partial**
- Constraint enforcement remains a critical gap; executor does not fully enforce
  primary/foreign/unique/check constraints.
  See `docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md` (Summary section).

### 5) Indexes

**Implemented (evidence)**
- Multiple index types exist in core and factory:
  `ScratchBird/src/core/index_factory.cpp:254`

**Missing / Partial**
- Index TID updates for migrations not implemented for Vector/HNSW, Full-text,
  GIN, GiST, BRIN, R-tree:
  `ScratchBird/src/core/catalog_manager.cpp:11125-11227`
- GiST cache cleanup intentionally leaks (type integration incomplete):
  `ScratchBird/src/sblr/index_cache.cpp:286`
  See `docs/findings/INDEX_IMPLEMENTATION_GAPS-completed.md`.

### 6) Query Optimizer and Execution

**Implemented (evidence)**
- Optimizer runtime components exist:
  `ScratchBird/src/optimizer/statistics_manager.cpp:1`,
  `ScratchBird/src/optimizer/cost_model.cpp:1`,
  `ScratchBird/src/optimizer/query_planner.cpp:6`

**Partial / Needs Verification**
- Planner integration with executor and stats population needs explicit
  end-to-end validation (not fully audited yet).

### 7) Types and Serialization

**Implemented (evidence)**
- TypedValue serialization exists:
  `ScratchBird/src/core/typed_value.cpp:3371`
- UUIDv7 support:
  `ScratchBird/src/core/uuidv7.cpp:1`

**Partial**
- Type coverage vs specs requires a dedicated verification pass.

### 8) Security (Engine Core)

**Implemented (evidence)**
- Auth providers exist:
  `ScratchBird/src/security/auth_manager.cpp:1`,
  `ScratchBird/src/security/scram_auth.cpp:1`
- Executor permission checks for DML/DDL:
  - CREATE TABLE on schema:
    `ScratchBird/src/sblr/executor.cpp:5628`
  - SELECT (table/column):
    `ScratchBird/src/sblr/executor.cpp:20748`
  - UPDATE (table/column):
    `ScratchBird/src/sblr/executor.cpp:13613`
  - DELETE:
    `ScratchBird/src/sblr/executor.cpp:15200`
- RLS policy evaluation helper exists:
  `ScratchBird/src/sblr/executor.cpp:41348`

**Partial / Missing**
- RLS enforcement is wired for INSERT/UPDATE/DELETE but not for SELECT:
  - INSERT WITH CHECK:
    `ScratchBird/src/sblr/executor.cpp:12401`
  - UPDATE USING/WITH CHECK:
    `ScratchBird/src/sblr/executor.cpp:14452`,
    `ScratchBird/src/sblr/executor.cpp:14576`
  - DELETE USING:
    `ScratchBird/src/sblr/executor.cpp:15896`
  - PolicyType::SELECT only appears in policy DDL creation:
    `ScratchBird/src/sblr/executor.cpp:36586`
- View security enforcement is stubbed (definer/column checks always OK):
  `ScratchBird/src/security/view_security.cpp:176`

### 9) Monitoring and Observability

**Implemented (evidence)**
- Monitoring query handler implements MON_* and sys.table_stats/sys.io_stats:
  `ScratchBird/src/sblr/executor.cpp:22854`
- sys.table_stats/sys.io_stats are recognized by semantic analyzer:
  `ScratchBird/src/sblr/semantic_analyzer_v2.cpp:2151`
- Firebird virtual MON$ tables are scaffolded:
  `ScratchBird/src/catalog/firebird_catalog.cpp:1726`

**Partial / Missing**
- sys.sessions/sys.transactions/sys.locks/sys.statements/sys.performance are not
  recognized or implemented (only sys.table_stats/sys.io_stats exist):
  `ScratchBird/src/sblr/semantic_analyzer_v2.cpp:2151`
- Firebird MON$ tables include hardcoded placeholders (MON$DATABASE, MON$ATTACHMENTS,
  MON$TRANSACTIONS), not full parity:
  `ScratchBird/src/catalog/firebird_catalog.cpp:1726`,
  `ScratchBird/src/catalog/firebird_catalog.cpp:1776`,
  `ScratchBird/src/catalog/firebird_catalog.cpp:1838`

### 10) Backup and Restore

**Partial**
- Spec exists but implementation parity not yet audited.
  See `docs/specifications/BACKUP_AND_RESTORE.md`.

### 11) Scheduler and Jobs

**Implemented (code-truth)**
- Persistent job catalog, scheduler loop, cron parsing, DDL surface, security,
  and audit hooks are implemented.
  See `docs/findings/SCHEDULER_JOB_RUNNER_AUDIT.md`.

### 12) UDR Runtime

**Implemented (evidence)**
- UDR catalog tables and CRUD exist in catalog manager:
  `ScratchBird/src/core/catalog_manager.cpp:30902`

**Needs Verification**
- Runtime execution and module loading not fully audited.

## Consolidated Gap List (Alpha Priority)
1. Fix catalog bootstrap schema roots (/emulated + /public root).
2. Complete tablespace routing + multi-file support.
3. Implement index TID updates for all index types.
4. (Removed) Scheduler/job system now implemented; see scheduler audit.
5. Enforce constraints (PK/FK/UNIQUE/CHECK/NOT NULL) in executor.
6. Implement security enforcement gaps (view definer checks + RLS SELECT) and
   monitoring view parity (sys.* + MON$ data sources).
7. Validate backup/restore coverage for all tablespaces and catalogs.

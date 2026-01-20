# Alpha Specification Implementation Audit (Deep Pass)

Date: 2026-01-20  
Scope: Alpha-only. ScratchBird source is read-only; audit is evidence-based (file/line).

## Purpose

Provide a deeper, evidence-backed mapping of alpha specifications to current
implementation and identify missing or stale areas that must become the final
alpha TODO list.

## Evidence Legend

- **Implemented (evidence)**: matching code exists (file/line cited).
- **Partial**: some code exists or explicit gaps are listed in specs.
- **Missing**: no implementation evidence found in source tree.
- **Needs spec update**: spec conflicts with current alpha scope/architecture.

## Core Engine (core/, storage/, transaction/)

**Implemented (evidence)**
- Database header and MGA defaults (no WAL for alpha): `ScratchBird/src/core/database.cpp:677-700`.
- Transaction lifecycle (begin/commit, TIP + CLOG interaction):  
  `ScratchBird/src/core/transaction_manager.cpp:269-403`.
- Sweep manager (OIT/OST gap trigger + sweep execution):  
  `ScratchBird/src/core/sweep_manager.cpp:48-140`.
- Garbage collector (cooperative + background GC):  
  `ScratchBird/src/core/garbage_collector.cpp:52-130`.
- Storage engine DML operations with TOAST support:  
  `ScratchBird/src/core/storage_engine.cpp:432-520`.
- Page compression manager (LZ4 fallback + compressed page I/O):  
  `ScratchBird/src/core/compressed_page_manager.cpp:10-112`.
- Buffer pool implementation: `ScratchBird/src/core/buffer_pool.cpp` (module exists).

**Partial**
- Tablespace routing: insert path explicitly notes single tablespace support:  
  `ScratchBird/src/core/storage_engine.cpp:465-487` ("findFreePage only supports tablespace 0").
- Vacuum naming in code (uses VACUUM tag in logging) still appears in GC/Sweep paths; spec
  now treats VACUUM as alias for SWEEP/GC.

**Missing / Needs update**
- WAL is not implemented (by design for alpha): confirm specs do not imply WAL usage for recovery.
- Tablespace multi-file support is not in code (see tablespace TODOs in specs).

## Catalog and Metadata (catalog/)

**Implemented (evidence)**
- Catalog root page structure includes schema/table/index/permissions and extended system tables:  
  `ScratchBird/src/core/catalog_manager.cpp:290-360`.
- Temporary table metadata fields:  
  `ScratchBird/src/core/catalog_manager.cpp:402-423`.

**Partial**
- Async TRUNCATE job exists but is ad-hoc and not a generic job scheduler:  
  `ScratchBird/src/core/catalog_manager.cpp:17577-17688`.

**Missing / Needs update**
- Full job scheduler is not present (no scheduler module; see Scheduler section).

## DDL/DML Parser (parser/, ddl/, dml/)

**Implemented (evidence)**
- Statement dispatch and core DDL/DML entry points:  
  `ScratchBird/src/parser/parser_v2.cpp:159-208`.
- CREATE dispatch for schema/table/index/view/sequence/proc/function/trigger/user/role:  
  `ScratchBird/src/parser/parser_v2.cpp:216-320`.
- Dialect parsers exist:  
  `ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp`,  
  `ScratchBird/src/parser/mysql/mysql_parser.cpp`,  
  `ScratchBird/src/parser/firebird/firebird_parser.cpp`.

**Partial**
- MySQL/PostgreSQL gaps are explicitly documented:  
  `ScratchBird/docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md`,  
  `ScratchBird/docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md`.
- Firebird parity specs still require feature-by-feature validation.

**Missing / Needs update**
- "SHOW DATABASE vs SHOW SCHEMA" parity needs final validation against parser+catalog.
- Any DDL in specs tagged as "planned" but not evidenced in parser should be listed as TODO.

## Temporary Tables (TEMPORARY_TABLES_SPECIFICATION.md)

**Implemented (evidence)**
- Parser support for TEMP/TEMPORARY and Firebird GLOBAL TEMPORARY:  
  `ScratchBird/src/parser/parser_v2.cpp:234-250`,  
  `ScratchBird/src/parser/firebird/firebird_parser.cpp:1762-1773`,  
  `ScratchBird/src/parser/mysql/mysql_parser.cpp:3374-3466`.
- Session cleanup hooks:  
  `ScratchBird/src/core/connection_context.cpp:641-713`.
- Storage engine applies session IDs for temp tuples:  
  `ScratchBird/src/core/storage_engine.cpp:449-460`.

**Partial**
- Need verification of catalog persistence rules and isolation semantics vs spec.

## SBLR (sblr/)

**Implemented (evidence)**
- SBLR executor present, including triggers and parameter handling:  
  `ScratchBird/src/sblr/executor.cpp:1432-1510`.
- Bytecode generator and semantic analyzer exist:  
  `ScratchBird/src/sblr/bytecode_generator_v2.cpp`,  
  `ScratchBird/src/sblr/semantic_analyzer_v2.cpp`.

**Partial**
- Performance optimization tiers are specified but not fully validated vs code.
- BLR→SBLR mapping work is ongoing in specs; parity requires audit harness.

## Types and Serialization (types/)

**Implemented (evidence)**
- TypedValue serialization and length-prefixed encoding:  
  `ScratchBird/src/core/typed_value.cpp:3371-3440`.
- UUIDv7 support exists: `ScratchBird/src/core/uuidv7.cpp`.
- Timezone/charset loaders present:  
  `ScratchBird/src/core/timezone_loader.cpp`, `ScratchBird/src/core/charset_loader.cpp`.

**Partial**
- Disabled legacy type modules indicate migration in progress:  
  `ScratchBird/src/core/type_serialization.cpp.disabled`,  
  `ScratchBird/src/core/type_conversions.cpp.disabled`.

## Indexes (indexes/)

**Implemented (evidence)**
- Index factory supports BTREE, LSM, HASH, GIN, BITMAP, RTREE, COLUMNSTORE, HNSW:  
  `ScratchBird/src/core/index_factory.cpp:130-214`.
- Individual index implementations exist in `src/core/`: `btree.cpp`, `hash_index.cpp`,
  `gin_index.cpp`, `gist_index.cpp`, `spgist_index.cpp`, `rtree.cpp`, `hnsw_index.cpp`,
  `columnstore.cpp`, `bitmap_index.cpp`.

**Partial**
- LSM and columnstore durability rules remain tied to optional WAL (post-gold);
  ensure specs keep WAL references scoped as optional.

## Security (Security Design Specification)

**Implemented (evidence)**
- Auth manager with HBA, rate limiting, SCRAM, certs:  
  `ScratchBird/src/security/auth_manager.cpp:760-860`.
- TLS/SAML/OAuth/Kerberos modules present:  
  `ScratchBird/src/security/tls_context.cpp`, `saml_auth.cpp`, `oauth_auth.cpp`, `kerberos_auth.cpp`.
- Password policy and audit logging modules exist:  
  `ScratchBird/src/security/password_policy.cpp`, `ScratchBird/src/core/audit_logger.cpp`.

**Partial**
- Role/group catalog policy enforcement not fully validated against specs.

## Network + Wire Protocols (network/, protocol/, wire_protocols/)

**Implemented (evidence)**
- Listener with parser pool and per-protocol port selection:  
  `ScratchBird/src/network/sb_listener_main.cpp:57-215`.
- Protocol adapters for native/PG/MySQL/Firebird:  
  `ScratchBird/src/protocol/adapters/native_adapter.cpp:121-140`,  
  `ScratchBird/src/protocol/adapters/postgresql_adapter.cpp`,  
  `ScratchBird/src/protocol/adapters/mysql_adapter.cpp`,  
  `ScratchBird/src/protocol/adapters/firebird_adapter.cpp:1-120`.

**Partial**
- Listener depends on engine endpoint; current readiness is pending (per project status).

**Needs update**
- Y-Valve specs should be reconciled with listener/pool architecture.

## Remote Database UDR (remote_database_udr/, udr_connectors/)

**Implemented (evidence)**
- FDW-style adapters for MySQL/PostgreSQL/Firebird:  
  `ScratchBird/src/fdw/mysql_adapter.cpp:1-120`,  
  `ScratchBird/src/fdw/postgresql_adapter.cpp`,  
  `ScratchBird/src/fdw/firebird_adapter.cpp`.
- Remote connection pool present: `ScratchBird/src/fdw/remote_connection_pool.cpp`.

**Missing**
- Core UDR runtime module not found (`src/udr` absent).

## Operations & Monitoring (operations/)

**Implemented (evidence)**
- Table stats manager captures scans and DML deltas:  
  `ScratchBird/src/core/table_stats_manager.cpp:33-159`.

**Partial**
- System views for `sys.*` monitoring are not verified against specs.

## Tools & Admin (tools/, admin/, cli/)

**Implemented (evidence)**
- sb_isql variants (native/pg/mysql/firebird) exist:  
  `ScratchBird/src/cli/sb_isql.cpp`, `sb_pg_isql.cpp`, `sb_my_isql.cpp`, `sb_fb_isql.cpp`.
- sb_backup tool exists: `ScratchBird/src/cli/sb_backup.cpp:1-96`.
- sb_security and sb_verify exist:  
  `ScratchBird/src/cli/sb_security.cpp`, `ScratchBird/src/cli/sb_verify.cpp`.

**Partial**
- sb_admin coverage vs specs is not fully validated.

## Compression (compression/)

**Implemented (evidence)**
- LZ4 compression support + compressed page manager:  
  `ScratchBird/src/core/compression_lz4.cpp`,  
  `ScratchBird/src/core/compressed_page_manager.cpp:10-112`.

**Partial**
- Spec coverage includes more compression modes; verify if only LZ4 is implemented.

## Scheduler / Job Runner (scheduler/)

**Missing**
- No dedicated scheduler/job-runner module found under `ScratchBird/src/`.
- Only ad-hoc job patterns (TRUNCATE) and network thread pool scheduling exist:  
  `ScratchBird/src/network/thread_pool.cpp:137-458`,  
  `ScratchBird/src/core/catalog_manager.cpp:17577-17688`.

## Specs Needing Update (Alpha Scope)

- **Y-Valve docs** vs **listener/pool** implementation (update or re-scope):
  `ScratchBird/docs/specifications/core/Y_VALVE_ARCHITECTURE.md`,  
  `ScratchBird/docs/specifications/network/Y_VALVE_DESIGN_PRINCIPLES.md`.
- **Tablespaces**: spec allows multi-file and DML routing; code still single tablespace
  (see storage_engine.cpp note above).
- **WAL**: ensure all alpha specs treat WAL as optional, post-gold.

## Final TODO Candidates (Alpha)

1. **Scheduler/Job Runner**: implement or re-scope; specs exist but runtime missing.
2. **UDR Core Runtime**: adapter specs exist but UDR execution engine module is absent.
3. **Tablespaces**: DML routing is partial; multi-file and GPID routing still pending.
4. **Parser Gap Lists**: implement missing MySQL/PG items per gap docs.
5. **Monitoring Views**: wire `sys.*` views to stats managers (table/statement/txn).
6. **Spec Hygiene**: reconcile Y-Valve vs listener and move post-gold material out of alpha scope.

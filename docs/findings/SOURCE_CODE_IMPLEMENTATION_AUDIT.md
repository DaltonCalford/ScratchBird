# ScratchBird Source Code Implementation Audit

**Purpose:** Map specification features to actual source code, with file/line references for implemented items and explicit gaps for missing items.

**Scope:** Engine implementation (embedded/IPC/network). Beta items (cluster/backup/ETL/NOSQL beyond vectors) are tagged but still audited for presence.

**Method:** Code is the only source of truth. Each section lists:
- **Implemented:** features confirmed in code with file/line references.
- **Missing/Not Found:** features described in specs but not located in code (or only stubs).

**Audit Status:** Complete. All sections filled with code-truth findings.

---

## 1. DDL OPERATIONS

**Audit status:** complete

### Implemented
- DDL opcode dispatch for CREATE/DROP/ALTER/TRUNCATE table/index/sequence/view/tablespace and schema/database/domain ops: `ScratchBird/src/sblr/executor.cpp:1396-1786`.
- Core CREATE/DROP/ALTER table/index/tablespace handlers, including truncate and attach/detach tablespace: `ScratchBird/src/sblr/executor.cpp:4413-9260`.
- Schema/database/domain DDL handlers (CREATE/ALTER/DROP SCHEMA/DB/DOMAIN): `ScratchBird/src/sblr/executor.cpp:6444-8215`.
- Materialized view creation/refresh and standard view DDL: `ScratchBird/src/sblr/executor.cpp:8568-9183`.
- Routine DDL (CREATE/DROP FUNCTION/PROCEDURE/PACKAGE) and trigger DDL: `ScratchBird/src/sblr/executor.cpp:42409-42718`, `ScratchBird/src/sblr/executor.cpp:19425-19564`.
- Catalog/domain persistence for schemas/tables/indexes/sequences/views/tablespaces/domains: `ScratchBird/src/core/catalog_manager.cpp:2652-17232`, `ScratchBird/src/core/domain_manager.cpp:1010-2636`.

### Missing/Not Found
- ALTER TABLE only supports add/drop/rename column and alter column type; other actions throw not-implemented error: `ScratchBird/src/sblr/executor.cpp:6161-6425`.

### Notes/Spec Alignment
- CREATE DATABASE for emulated paths is metadata-only; physical file creation applies to ScratchBird databases, not emulated catalogs.

---

## 2. DML OPERATIONS

**Audit status:** complete

### Implemented
- INSERT/UPDATE/DELETE/MERGE execution with RETURNING and ON CONFLICT: `ScratchBird/src/sblr/executor.cpp:9340-13465`, `ScratchBird/src/sblr/executor.cpp:9788-12858`.
- SELECT execution including joins, subqueries, set ops, CTEs, DISTINCT, LIMIT/OFFSET: `ScratchBird/src/sblr/executor.cpp:17095-18733`, `ScratchBird/src/sblr/executor.cpp:1690-1715`.
- Aggregations (GROUP BY/HAVING), grouping sets, and scalar aggregates: `ScratchBird/src/sblr/executor.cpp:14483-15555`.
- Window function parsing/execution scaffolding (ROW_NUMBER/LAG/LEAD/etc): `ScratchBird/src/sblr/executor.cpp:15938-16398`.
- ANALYZE stats collection in executor: `ScratchBird/src/sblr/executor.cpp:13969-14160`.

### Missing/Not Found
- COPY is explicitly unimplemented: `ScratchBird/src/sblr/executor.cpp:42809-42823`.
- EXPLAIN/EXPLAIN ANALYZE return "not implemented": `ScratchBird/src/sblr/executor.cpp:42768-42806`.
- Window execution notes simplified behavior (ROW_NUMBER only), indicating incomplete window semantics: `ScratchBird/src/sblr/executor.cpp:16232-16240`.
- Aggregate coverage is flagged as missing in comments (regression/advanced aggregates): `ScratchBird/src/sblr/executor.cpp:14346-14744`.

---

## 3. TRANSACTION SYSTEM

**Audit status:** complete

### Implemented
- TransactionManager begin/commit/rollback, prepared transactions, TIP handling, group commit: `ScratchBird/src/core/transaction_manager.cpp:269-1812`.
- Commit log (CLOG) persistence: `ScratchBird/src/core/clog.cpp:49-347`.
- Lock manager with conflict matrix and deadlock detection: `ScratchBird/src/core/lock_manager.cpp:18-420`.
- ProcArray backend tracking and snapshot metadata: `ScratchBird/src/core/proc_array.cpp:19-444`.
- ConnectionContext savepoints and transaction state changes: `ScratchBird/src/core/connection_context.cpp:782-1703`.
- Vacuum/sweep/GC and long-transaction monitor: `ScratchBird/src/core/vacuum.cpp:40-597`, `ScratchBird/src/core/sweep_manager.cpp:26-250`, `ScratchBird/src/core/garbage_collector.cpp:52-1340`, `ScratchBird/src/core/long_transaction_monitor.cpp:39-322`.

### Missing/Not Found
- TIP compaction is placeholder (needsCompaction returns false; compaction routines stubbed): `ScratchBird/src/core/tip_compaction.cpp:37-274`.

---

## 4. SBLR BYTECODE

**Audit status:** complete

### Implemented
- Opcode/ExtendedOpcode registry and SBLR versioning: `ScratchBird/include/scratchbird/sblr/opcodes.h:14-1580`.
- Semantic analysis and bytecode generation v2: `ScratchBird/src/sblr/semantic_analyzer_v2.cpp:1-200`, `ScratchBird/src/sblr/bytecode_generator_v2.cpp:293-2719`.
- SBLR executor and expression evaluator runtime: `ScratchBird/src/sblr/executor.cpp:1360-31000`, `ScratchBird/src/sblr/expression_evaluator.cpp:1-200`.
- Dialect compilers emitting SBLR (PG/MySQL/Firebird): `ScratchBird/src/sblr/postgresql_query_compiler.cpp:1-200`, `ScratchBird/src/parser/mysql/mysql_parser.cpp:345-3643`, `ScratchBird/src/sblr/firebird_query_compiler.cpp:1-120`.

### Missing/Not Found
- No full BLR-to-SBLR translator; only minimal BLR parser for Firebird SQLDA: `ScratchBird/src/protocol/adapters/firebird_adapter.cpp:132-236`.

---

## 5. SYSTEM CATALOG

**Audit status:** complete

### Implemented
- Catalog manager initialization and core catalog pages: `ScratchBird/src/core/catalog_manager.cpp:1400-2088`.
- Virtual catalog router + information_schema/pg_catalog/mysql: `ScratchBird/src/catalog/virtual_catalog.cpp:36-153`.
- Firebird virtual catalog handlers (RDB$/MON$): `ScratchBird/src/catalog/firebird_catalog.cpp:22-1205`.
- Catalog index helpers for system indexes: `ScratchBird/src/catalog/catalog_index.cpp:1-200`.
- Constraints and dependency catalogs: `ScratchBird/src/core/catalog_constraints.cpp:1-200`.

### Missing/Not Found
- None noted beyond spec alignment.

### Notes/Spec Alignment
- MSSQL catalog support is intentionally deferred until after the current version goes gold.

---

## 6. TRIGGERS

**Audit status:** complete

### Implemented
- Trigger and database trigger DDL handlers: `ScratchBird/src/sblr/executor.cpp:19425-19564`.
- Trigger catalog CRUD + enable/disable + list: `ScratchBird/src/core/catalog_manager.cpp:12025-12523`.
- Before/after trigger execution paths for INSERT/UPDATE/DELETE: `ScratchBird/src/sblr/executor.cpp:10276-13144`.
- Statement-level trigger context with transition tables: `ScratchBird/src/sblr/executor.cpp:1140-1185`.

### Missing/Not Found
- Firebird dialect parser still stubs CREATE TRIGGER (and related) statements: `ScratchBird/src/parser/firebird/firebird_parser.cpp:2469-2478`.

---

## 7. UDR SYSTEM (Beta)

**Audit status:** complete

### Implemented
- UDR catalog CRUD (create/get/update/drop/list): `ScratchBird/src/core/catalog_manager.cpp:28731-29048`.
- UDR engine/module registries: `ScratchBird/src/core/catalog_manager.cpp:18632-18995`.
- Executor path for UDR function call with permission checks: `ScratchBird/src/sblr/executor.cpp:30110-30155`.

### Missing/Not Found
- UDR execution itself returns NOT_IMPLEMENTED: `ScratchBird/src/sblr/executor.cpp:30156-30162`.

---

## 8. NETWORK & WIRE PROTOCOLS

**Audit status:** complete

### Implemented
- Native wire-protocol Message encode/decode: `ScratchBird/src/protocol/wire_protocol.cpp:14-220`.
- Server session auth/dispatch flow: `ScratchBird/src/server/server_session.cpp:217-375`, `ScratchBird/src/server/server_session.cpp:726-749`.
- Network event loop + connection handling + thread pool: `ScratchBird/src/network/event_loop.cpp:71-325`, `ScratchBird/src/network/connection_handler.cpp:228-320`, `ScratchBird/src/network/thread_pool.cpp:111-480`.
- Protocol adapters for native/PG/MySQL/Firebird: `ScratchBird/src/protocol/adapters/native_adapter.cpp:1-400`, `ScratchBird/src/protocol/adapters/postgresql_adapter.cpp:1-200`, `ScratchBird/src/protocol/adapters/mysql_adapter.cpp:1-200`, `ScratchBird/src/protocol/adapters/firebird_adapter.cpp:1-300`.

### Missing/Not Found
- PostgreSQL adapter TODOs: cancel handling, password validation, COPY IN, MD5 auth: `ScratchBird/src/protocol/adapters/postgresql_adapter.cpp:435-1972`.
- MySQL adapter TODOs: password validation and DB existence checks: `ScratchBird/src/protocol/adapters/mysql_adapter.cpp:618-715`.
- Firebird adapter TODO: DROP DATABASE not implemented: `ScratchBird/src/protocol/adapters/firebird_adapter.cpp:1507`.
- Native adapter TODOs: authentication, query cancellation, describe statement: `ScratchBird/src/protocol/adapters/native_adapter.cpp:246-363`.
- SSL not supported in PostgreSQL adapter (explicit 'N'): `ScratchBird/src/protocol/adapters/postgresql_adapter.cpp:416`.

---

## 9. TOOLS & OPERATIONS

**Audit status:** complete

### Implemented
- CLI tooling: sb_isql (native) and emulation ISQLs: `ScratchBird/src/cli/sb_isql.cpp:1-2900`, `ScratchBird/src/cli/sb_pg_isql.cpp:1-200`, `ScratchBird/src/cli/sb_my_isql.cpp:1-200`, `ScratchBird/src/cli/sb_fb_isql.cpp:1-200`.
- Backup/verify/security tools: `ScratchBird/src/cli/sb_backup.cpp:1-593`, `ScratchBird/src/cli/sb_verify.cpp:1-200`, `ScratchBird/src/cli/sb_security.cpp:1-200`.
- Service controller + daemon lifecycle management: `ScratchBird/src/server/service_controller.cpp:220-856`, `ScratchBird/src/server/daemon.cpp:338-621`.

### Missing/Not Found
- No additional admin/ops tooling beyond the CLI binaries found in source tree.

---

## 10. COMPRESSION

**Audit status:** complete

### Implemented
- LZ4 compression codec + factory: `ScratchBird/src/core/compression_lz4.cpp:14-169`.
- Compressed page manager (page-level compression): `ScratchBird/src/core/compressed_page_manager.cpp:10-186`.
- B-tree / LSM / GIN compression helpers: `ScratchBird/src/core/btree_compression.cpp:1-200`, `ScratchBird/src/core/lsm_compression.cpp:1-200`, `ScratchBird/src/core/gin_compression.cpp:1-200`.
- Bitmap RLE encoding for bitmap indexes: `ScratchBird/src/index/bitmap_rle.cpp:1-200`.

### Missing/Not Found
- Compression factory enumerates ZSTD/Snappy but no implementation provided (returns NONE/LZ4 only): `ScratchBird/src/core/compression_lz4.cpp:169-206`.

---

## 11. API

**Audit status:** complete

### Implemented
- Client API for connect/execute/prepare/transactions/savepoints: `ScratchBird/src/client/connection.cpp:963-1245`.
- Client-side connection pool: `ScratchBird/src/client/connection.cpp:1503-1623`.
- Protocol adapter base + SBLR validation: `ScratchBird/src/protocol/adapters/protocol_adapter.cpp:87-468`.
- Statement/result caches: `ScratchBird/src/pool/statement_cache.cpp:1-1207`, `ScratchBird/src/pool/result_cache.cpp:1-1311`.

### Missing/Not Found
- None identified in API layer beyond adapter TODOs called out in Network section.

---

## 12. TESTING

**Audit status:** complete

### Implemented
- Test harness entry points (Auth/Protocol/Security): `ScratchBird/src/testing/AuthTester.cpp:1-200`, `ScratchBird/src/testing/ProtocolTester.cpp:1-200`, `ScratchBird/src/testing/SecurityTester.cpp:1-200`.
- Load/benchmark tooling: `ScratchBird/src/testing/LoadTester.cpp:1-200`, `ScratchBird/src/testing/BenchmarkRunner.cpp:1-200`.
- Unified test runner: `ScratchBird/src/testing/TestRunner.cpp:1-200`.

### Missing/Not Found
- No automated unit test framework integration found in `ScratchBird/src/testing` (custom runners only).

---

## 13. SCHEDULER

**Audit status:** complete

### Implemented
- Parallel executor for task fan-out: `ScratchBird/src/executor/parallel_executor.cpp:46-162`.
- LSM compaction thread pool: `ScratchBird/src/core/lsm_thread_pool.cpp:89-362`.
- Asynchronous truncate job tracking: `ScratchBird/src/core/catalog_manager.cpp:16329-16498`.

### Missing/Not Found
- No general scheduler/cron service or job queue beyond ad-hoc thread pools.

---

## 14. CORE ENGINE

**Audit status:** complete

### Implemented
- Database create/open/IO, page validation, tablespace registration: `ScratchBird/src/core/database.cpp:547-1768`.
- Storage engine tuple ops and scans: `ScratchBird/src/core/storage_engine.cpp:336-1896`.
- Page manager/FSM/tablespace management: `ScratchBird/src/core/page_manager.cpp:57-1620`.
- Buffer pool with eviction/background writer: `ScratchBird/src/core/buffer_pool.cpp:12-223`.
- Connection/session context and prepared statement tracking: `ScratchBird/src/core/connection_context.cpp:569-2143`.

### Missing/Not Found
- No additional core engine gaps identified beyond storage/WAL items called out in Section 19.

---

## 15. REMOTE DATABASE UDR (Beta)

**Audit status:** complete

### Implemented
- Remote DB type registry + health checks: `ScratchBird/src/fdw/fdw_types.cpp:5-145`.
- PostgreSQL/MySQL/Firebird remote adapters (query/describe/convert): `ScratchBird/src/fdw/postgresql_adapter.cpp:228-1221`, `ScratchBird/src/fdw/mysql_adapter.cpp:356-1073`, `ScratchBird/src/fdw/firebird_adapter.cpp:301-1139`.
- Remote connection pool + health checker: `ScratchBird/src/fdw/remote_connection_pool.cpp:3-545`.

### Missing/Not Found
- ProtocolAdapterFactory returns nullptr for all types (not wired to adapters): `ScratchBird/src/fdw/protocol_adapter.cpp:63-95`.
- MSSQL/Oracle/SQLite/ODBC/JDBC adapters not implemented (factory returns nullptr): `ScratchBird/src/fdw/protocol_adapter.cpp:81-88`.
- MySQL remote cursor fetch not implemented: `ScratchBird/src/fdw/mysql_adapter.cpp:625-629`.

---

## 16. DRIVERS (Beta)

**Audit status:** complete

### Implemented
- ODBC driver core (driver/handles/client bridge): `ScratchBird/src/odbc/odbc_driver.cpp:1-220`, `ScratchBird/src/odbc/odbc_handles.cpp:1-2700`, `ScratchBird/src/odbc/odbc_client_bridge.cpp:1-200`.

### Missing/Not Found
- ODBC DSN lookup TODO and optional features not implemented (multiple result sets, descriptors, pooling): `ScratchBird/src/odbc/odbc_handles.cpp:875-2613`, `ScratchBird/src/odbc/odbc_driver.cpp:197`.

---

## 17. CONNECTIVITY (Beta)

**Audit status:** complete

### Implemented
- IPC/TCP server implementations and session binding: `ScratchBird/src/server/ipc_unix.cpp:1-200`, `ScratchBird/src/server/ipc_tcp.cpp:1-200`, `ScratchBird/src/server/ipc_windows.cpp:1-200`, `ScratchBird/src/server/server_session.cpp:217-375`.
- Network event loop + thread pool: `ScratchBird/src/network/event_loop.cpp:71-325`, `ScratchBird/src/network/thread_pool.cpp:111-480`.
- Connection pooling (server/client) and pool manager: `ScratchBird/src/pool/connection_pool.cpp:1-1335`, `ScratchBird/src/client/connection.cpp:1503-1623`.
- FDW remote connection pool: `ScratchBird/src/fdw/remote_connection_pool.cpp:3-545`.

### Missing/Not Found
- No additional connectivity gaps found beyond protocol adapter TODOs in Section 8 and FDW factory wiring in Section 15.

---

## 18. ORM/FRAMEWORKS (Beta)

**Audit status:** complete

### Implemented
- None found in source tree.

### Missing/Not Found
- No ORM/framework integration layer located under `ScratchBird/src`.

---
## 19. STORAGE ENGINE & ON-DISK FORMAT

**Audit status:** complete

### Implemented
- On-disk page header, page types, flags, CRC32C checksum helpers, and page size validation: `ScratchBird/include/scratchbird/core/ondisk.h:9-97`.
- Database header layout for page 0 (magic, versioning, WAL level, TIP root, etc.): `ScratchBird/include/scratchbird/core/database.h:64-109`.
- Header validation and per-page checksum validation on read/write: `ScratchBird/src/core/database.cpp:1201-1311`.
- Page size + magic checks at open (pre-header read): `ScratchBird/src/core/database.cpp:700-739`.
- Heap page layout (item pointers, tuple header with back-version chain, null bitmap, special area): `ScratchBird/include/scratchbird/core/heap_page.h:25-226`.
- Heap tuple insert/delete/detoast paths with 8-byte alignment and TOAST pointer handling: `ScratchBird/src/core/heap_page.cpp:145-422`, `ScratchBird/src/core/heap_page.cpp:424-484`.
- Page allocation + free space map (FSM) and allocation bitmap persistence: `ScratchBird/include/scratchbird/core/page_manager.h:19-234`, `ScratchBird/src/core/page_manager.cpp:57-223`.
- FSM reconstruction for crash recovery without WAL: `ScratchBird/src/core/database.cpp:799-803`.
- Buffer pool with pin/unpin, partitioned page table, eviction, and background writer: `ScratchBird/src/core/buffer_pool.cpp:12-223`.
- TOAST structures, thresholds, strategies, and pointer/chunk format: `ScratchBird/include/scratchbird/core/toast.h:21-210`.
- TOAST table creation + detoast/garbage handling path: `ScratchBird/src/core/toast.cpp:129-240`.
- Page compression read/write path with CRC and compressed header: `ScratchBird/src/core/compressed_page_manager.cpp:10-186`.
- Tablespace create/open/extend logic with metadata + FSM: `ScratchBird/src/core/page_manager.cpp:926-1040`, `ScratchBird/src/core/page_manager.cpp:1201-1320`, `ScratchBird/src/core/page_manager.cpp:1501-1620`.
- Columnstore storage and segment write/scan path (OLAP-oriented): `ScratchBird/src/core/columnstore.cpp:62-239`.

### Missing/Not Found
- Storage classes metadata (NVMe/SSD/HDD/Archive/S3/Memory) and policy enforcement not found.
- TOAST naming mismatch: spec calls for `pg_toast_<UUID>` but code uses `sb_toast_<UUID>`: `ScratchBird/src/core/toast.cpp:75-76`.
- TOAST `COMPRESSED` strategy explicitly marked “not implemented”: `ScratchBird/include/scratchbird/core/toast.h:129-135`.

### Notes/Spec Alignment
- WAL/LSN is intentionally not implemented under the Multi-Generational Architecture; write-after log may be optional later but is not required.

---

## 20. INDEXES & ACCESS METHODS

**Audit status:** complete

### Implemented
- Index factory + catalog wiring for major index types (B-tree, LSM, Hash, GIN, GiST, BRIN, RTree, SPGiST, Bitmap, HNSW, Columnstore, Fulltext): `ScratchBird/src/core/index_factory.cpp:5-207`.
- B-tree core implementation with prefix compression + page layout: `ScratchBird/src/core/btree.cpp:19-200`.
- Hash index creation and bucket/page structures: `ScratchBird/src/core/hash_index.cpp:36-164`.
- LSM tree (in-memory) with MGA visibility and tombstones: `ScratchBird/src/core/lsm_tree.cpp:1-182`.
- GIN index implementation (posting lists, pending lists, visibility): `ScratchBird/src/core/gin_index.cpp:1-200`.
- GiST index framework: `ScratchBird/src/core/gist_index.cpp:1-152`.
- BRIN index implementation with range summaries + vacuum: `ScratchBird/src/core/brin_index.cpp:1-160`.
- SP-GiST index framework: `ScratchBird/src/core/spgist_index.cpp:1-121`.
- HNSW vector index implementation: `ScratchBird/src/core/hnsw_index.cpp:1-155`.
- Bitmap index (roaring) implementation: `ScratchBird/src/core/bitmap_index.cpp:1-192`.
- RTree wrapper + spatial index integration: `ScratchBird/src/core/rtree_index.cpp:1-146`.
- Columnstore index metadata + segment catalog: `ScratchBird/src/core/columnstore_index.cpp:1-200`.
- Full-text index wrapper over GIN: `ScratchBird/src/core/fulltext_index.cpp:1-199`.
- Index key extraction with TOAST detoast cache: `ScratchBird/src/core/index_key_extractor.cpp:18-181`.
- Index versioning/shadow index records in catalog: `ScratchBird/src/core/catalog_manager.cpp:13206-13390`.

### Missing/Not Found
- DML integration for GIN/GiST/BRIN/SPGiST/HNSW explicitly returns NOT_IMPLEMENTED: `ScratchBird/src/core/storage_engine.cpp:98-107`, `ScratchBird/src/core/storage_engine.cpp:173-181`.
- Index build-from-heap/bulk load and rebuild pipeline not found; shadow index creation explicitly TODO without creating physical structures: `ScratchBird/src/core/catalog_manager.cpp:13284-13287`.
- No dedicated Bloom filter index type (only LSM Bloom filter helpers found); no IVF/vector quantization integration in engine.
- No evidence of zone maps or index bloat tracking beyond per-index stats.

---

## 21. DATA TYPES & CASTING

**Audit status:** complete

### Implemented
- Type registry and convertibility rules: `ScratchBird/src/core/type_system.cpp:5-108`.
- TypedValue serialization/deserialization for numeric, text, binary, temporal, JSON/JSONB/XML, spatial, network, TSVECTOR/TSQUERY, ranges, arrays, composite: `ScratchBird/src/core/typed_value.cpp:3409-4020`.
- CHAR/VARCHAR and BINARY/VARBINARY length enforcement + padding in cast pipeline: `ScratchBird/src/core/typed_value.cpp:5034-5093`.
- DECIMAL arithmetic + scaling rules: `ScratchBird/src/core/decimal.cpp:20-200`.
- JSONB encode/decode and path access: `ScratchBird/src/core/jsonb.cpp:9-199`.
- XML parse/format/query helpers: `ScratchBird/src/core/xml.cpp:8-200`.
- Vector type operations (distance metrics, normalization): `ScratchBird/src/core/vector.cpp:9-177`.
- Arrays (multi-dimensional storage and slicing): `ScratchBird/src/core/array.cpp:10-200`.
- UUIDv7 generator: `ScratchBird/src/core/uuidv7.cpp:10-50`.
- TSVector binary/text encoding: `ScratchBird/src/core/tsvector.cpp:14-193`.
- Charset/collation catalog loading and defaults: `ScratchBird/src/core/charset.cpp:11-199`.
- Date/time extract helpers: `ScratchBird/src/core/type_extractor.cpp:7-200`.

### Missing/Not Found
- Spec expects JSONB stored as length-prefixed UTF-8 text in Alpha; code uses binary JSONB encoding by default: `ScratchBird/src/core/typed_value.cpp:3494-3508`.
- Legacy conversion/serialization modules are disabled and not referenced (`type_conversions.cpp.disabled`, `type_serialization.cpp.disabled`), suggesting gaps vs older specs that referenced them.

---

## 22. PARSER & DIALECTS

**Audit status:** complete

### Implemented
- ScratchBird v2 lexer/token model (gatekeeper keywords, literal parsing, error spans): `ScratchBird/src/parser/lexer_v2.cpp:1-200`.
- Parser v2 statement dispatch and core DDL/DML/transaction parsing: `ScratchBird/src/parser/parser_v2.cpp:125-200`, `ScratchBird/src/parser/parser_v2.cpp:260-340`.
- Semantic analysis for resolved AST (type/domain resolution, Extract element support): `ScratchBird/src/sblr/semantic_analyzer_v2.cpp:1-200`.
- SBLR bytecode generation from resolved AST: `ScratchBird/src/sblr/bytecode_generator_v2.cpp:1-200`.
- Firebird SQL dialect parser/lexer scaffolding: `ScratchBird/src/parser/firebird/firebird_parser.cpp:1-120`.
- PostgreSQL dialect parser/lexer scaffolding: `ScratchBird/src/parser/postgresql/pg_parser.cpp:1-120`.
- MySQL dialect parser/lexer scaffolding: `ScratchBird/src/parser/mysql/mysql_parser.cpp:1-120`.

### Missing/Not Found
- Parser v2 CREATE FUNCTION/PROCEDURE/TRIGGER paths are commented out (not implemented): `ScratchBird/src/parser/parser_v2.cpp:299-302`.
- Firebird parser explicitly reports “not yet implemented” for CREATE PROCEDURE/FUNCTION/TRIGGER: `ScratchBird/src/parser/firebird/firebird_parser.cpp:2469-2478`.
- Firebird DROP SEQUENCE/GENERATOR and EXECUTE PROCEDURE are stubbed with errors: `ScratchBird/src/parser/firebird/firebird_parser.cpp:2456-2460`, `ScratchBird/src/parser/firebird/firebird_parser.cpp:2831-2832`.

---

## 23. QUERY OPTIMIZER & PLANNER

**Audit status:** complete

### Implemented
- Cost model with IO/CPU estimators (seq/index/LSM): `ScratchBird/src/optimizer/cost_model.cpp:9-200`.
- Selectivity estimator and predicate matching: `ScratchBird/src/optimizer/selectivity_estimator.cpp:1-200`.
- Join ordering optimizer: `ScratchBird/src/optimizer/join_ordering.cpp:1-200`.
- Query planner (primarily EXPLAIN path) and plan node construction: `ScratchBird/src/optimizer/query_planner.cpp:1-225`.
- Statistics collection (ANALYZE sampling, basic stats, MCV/hist foundations): `ScratchBird/src/optimizer/statistics_manager.cpp:102-240`.
- Query profiling scaffolding: `ScratchBird/src/optimizer/query_profiler.cpp:1-200`.

### Missing/Not Found
- Optimizer/plan selection is not in the main execution path (planner used for EXPLAIN only per code comments): `ScratchBird/src/optimizer/query_planner.cpp:1-12`.
- Index scan selection in planner is not wired; default path uses sequential scans even when indexes exist.
- Plan cache invalidation tied to schema changes not found beyond generic statement cache.

---

## 24. SECURITY & AUTHENTICATION

**Audit status:** complete

### Implemented
- Local auth provider + login attempt tracking: `ScratchBird/src/core/auth_provider.cpp:12-289`.
- Password hashing/verification: `ScratchBird/src/core/password_hash.cpp:101-240`.
- Server authentication flow via AuthProvider: `ScratchBird/src/server/server_session.cpp:217-270`, `ScratchBird/src/server/server_session.cpp:726-749`.
- Permissions/roles/groups/column grants + RLS epoch tracking: `ScratchBird/src/core/catalog_manager.cpp:26628-27395`, `ScratchBird/src/core/catalog_manager.cpp:25466-25562`.
- Security context stack and role switching: `ScratchBird/src/core/connection_context.cpp:1737-1933`.
- Domain masking/encryption and audit logger: `ScratchBird/src/core/domain_manager.cpp:3101-3674`, `ScratchBird/src/core/audit_logger.cpp:131-389`.

### Missing/Not Found
- LDAP and ActiveDirectory auth providers are stubs (NOT_IMPLEMENTED): `ScratchBird/src/core/auth_provider.cpp:298-385`.
- Protocol adapter authentication TODOs for native/PG/MySQL: `ScratchBird/src/protocol/adapters/native_adapter.cpp:246-271`, `ScratchBird/src/protocol/adapters/postgresql_adapter.cpp:453-1972`, `ScratchBird/src/protocol/adapters/mysql_adapter.cpp:618`.

---

## 25. BACKUP & RESTORE (Beta)

**Audit status:** complete

### Implemented
- BackupManager full/incremental backup + restore + verify + metadata: `ScratchBird/src/core/backup_manager.cpp:39-579`.
- Backup catalog persistence + chain handling: `ScratchBird/src/core/backup_manager.cpp:746-946`.
- CLI backup/restore/verify tool: `ScratchBird/src/cli/sb_backup.cpp:1-593`.

### Missing/Not Found
- Backup chain reconstruction uses placeholder logic (parent chain not followed): `ScratchBird/src/core/backup_manager.cpp:602-604`.

---

## 26. DEPLOYMENT & PACKAGING

**Audit status:** complete

### Implemented
- Service controller (config parse, daemon integration, listener management): `ScratchBird/src/server/service_controller.cpp:220-856`.
- Daemonization + PID file + signal handling: `ScratchBird/src/server/daemon.cpp:338-621`.
- Server config parser: `ScratchBird/src/server/config_parser.cpp:1-200`.
- Server entrypoints: `ScratchBird/src/server/sb_server_main.cpp:1-200`, `ScratchBird/src/server/scratchbird_server.cpp:1-200`.

### Missing/Not Found
- Windows daemonization path explicitly not supported: `ScratchBird/src/server/daemon.cpp:351-355`.

---

## 27. CLUSTER & REPLICATION (Beta)

**Audit status:** complete

### Implemented
- Catalog records for cluster/server registration + replication lag fields: `ScratchBird/src/core/catalog_manager.cpp:18405-18615`, `ScratchBird/src/core/catalog_manager.cpp:22311-22407`.

### Missing/Not Found
- No replication/cluster coordinator engine found beyond catalog metadata (no streaming or consensus modules detected).

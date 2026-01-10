# Alpha 3 Completion To-Do (2025-12-13)

Goal: bring the codebase in line with Alpha Phase 3 criteria and published docs.

## Wire Protocols / Firebird Bridge
- Finish Firebird adapter IPC bridge (docs/archive/2026-01-04/planning/old_Plans/firebird_adapter_ipc_bridge.md): native transaction mapping, BLR/SQLDA param/result handling, RDB$ catalog emulation (placeholders exist; flesh out real definitions), status vector mapping, streaming cursors, and end-to-end Firebird wire tests over Unix sockets in `build/`.
- Transaction mapping: Firebird adapter now tracks per-handle transactions against native IPC begin/commit/rollback; BLR is parsed for simple output fields/null indicators and input BLR drives basic param decoding (with message-length/null-indicator validation and result padding to declared message length); status vector carries isc_arg_gds + mapped code/sqlstate for common errors; catalog placeholders for RDB$* views are in place. Full SQLDA (charsets/type coercion), richer status mapping, real catalog projections, and streaming still pending.
- Catalog emulation progress: RDB$RELATIONS/FIELDS/RELATION_FIELDS are populated from live catalog tables/columns; RDB$INDICES/INDEX_SEGMENTS/RELATION_CONSTRAINTS/CHECK_CONSTRAINTS/REF_CONSTRAINTS reflect actual indexes + constraints; RDB$TRIGGERS and RDB$PROCEDURES/PROCEDURE_PARAMETERS surface catalog triggers/procs; RDB$VIEW_RELATIONS maps base relations using compiler-reported SBLR dependencies (with a minimal fallback when absent); adapter switches sessions to the emulated Firebird schema. Remaining: richer charset/type fidelity and removal of the fallback via fully parser-driven dependency capture.
- SQLDA synthesis: when clients omit an output BLR, the adapter now builds a BLR/field layout from native column metadata so null indicators and padding align with Firebird expectations; full charset/scale mapping still pending.
- Harden dialect compilers (no V2 fallbacks remain): expand MySQL/PostgreSQL/Firebird coverage so compilers emit runnable bytecode for supported syntax, and make adapters surface clear errors when a dialect feature is unsupported.
- Emulated catalog namespaces are now created per DB: `remote.emulated.firebird.<db>` and `remote.emulated.postgresql.<db>` with stub catalog views. Replace stubs with real catalog projections that match each engine’s system tables; add the equivalent for MySQL (`remote.emulated.mysql.<db>` INFORMATION_SCHEMA) and ensure adapters default to those schemas.
- Align docs with reality: update Alpha 3 status and ROADMAP to note which wire adapters are still partial.

## Wire Protocols / PostgreSQL Bridge
- Plan: see `docs/archive/2026-01-04/planning/old_Plans/postgresql_adapter_ipc_bridge.md`. Needs IPC-only execution, autocommit/transaction mapping, proper text/binary format handling, portal streaming, pg_catalog projections, SQLSTATE mapping, and end-to-end libpq tests over Unix sockets in `build/`.
- Current gaps: adapter now routes simple/extended execute over IPC with search_path to `remote.emulated.postgresql.<db>`, but catalog projections are stubbed (placeholders added for pg_database/pg_namespace/pg_class/pg_attribute/pg_type/pg_roles/pg_proc/pg_index); error/status mapping was broadened but remains thin; RowDescription reflects bound format codes and text encoding now matches declared formats with basic binary (bool/int/float/string/blob) emission and binary bind values are decoded to text for execution, but complex types still fall back to text and native binary parameter flow is not yet implemented; portal fetch counts/cursor lifecycle need enforcement beyond the current row_callback max_rows stop; placeholder substitution is still textual (parser-aware skipping quotes) rather than true bound execution. *Portals reuse buffered rows across Execute calls and ReadyForQuery now reports IDLE/IN_TRANSACTION/FAILED based on detected tx statements/errors, but cursor lifecycle, full binary encoders/decoders, and SQLSTATE coverage remain incomplete.*

## Wire Protocols / MySQL Bridge
- Plan: see `docs/archive/2026-01-04/planning/old_Plans/mysql_adapter_ipc_bridge.md`. Needs IPC-only execution, MySQL autocommit/status flag semantics, PS protocol (COM_STMT_*), NULL bitmap/type decoding, column metadata with charsets/type flags, INFORMATION_SCHEMA emulation under `remote.emulated.mysql.<db>`, error/status mapping, streaming rows, and end-to-end mysqlclient tests over Unix sockets in `build/`.
- Current gaps: COM_QUERY/COM_STMT_EXECUTE now bridge over IPC and attempt to switch to the emulated schema. Binary prepared-statement executes decode NULL bitmaps, parameter types/unsigned flags, and substitute bound literals; autocommit/BEGIN/COMMIT toggles update status flags; core statuses map to common MySQL error codes (now including FK/not-null/lock/deadlock/truncation/out-of-range/div-by-zero); emulated `information_schema` now includes schemata/tables/columns/table_constraints/key_column_usage/referential_constraints/statistics/routines copied from the engine catalog when available. PREPARE runs LIMIT 0 to emit column metadata and column definitions set basic length/charset/flag heuristics. Still missing: full PS metadata/charsets/flags, richer INFORMATION_SCHEMA completeness (routine/stats fidelity), streaming row delivery, and dialect-specific mysqlclient e2e tests.

## Pooling / Client Infrastructure
- Implement real connection pooling in `src/pool/connection_pool.cpp` (connect/execute/validate/close/reset logic, health checks, eviction, tag handling).
- Add integration tests for pooling and statement/result caches (under `tests/`), remove any stray test directories under `src/` once migrated.
- Stabilize IPC path handling: documentation should reflect new `build/ipc`/`build/run` defaults; unify PID file discovery between client/server utilities.

## Authentication / Security
- Implement LDAP/AD and OAuth/OIDC flows (network calls, JWKS/discovery caching, group mapping) instead of `NOT_IMPLEMENTED` stubs; add tests that hit real/mock endpoints.
- Verify SCRAM/TLS/HBA coverage with integration tests (auth success/fail, rate limiting, audit logging).

## Catalog / Engine Parity
- Provide RDB$* emulation views for Firebird clients and ensure the recursive schema model exposes per-engine catalogs.
- Audit storage/index/catalog code paths still returning `NOT_IMPLEMENTED` and either implement or downgrade documentation claims.
- Stored code DDL: executor can now create/drop functions/procedures/packages with dependency capture via QueryCompilerV2; parser/bytecode generation and true stored-code execution remain missing.

## Verification
- Maintain full build/test runs from `build/` (databases under `build/database/`, sockets under `build/ipc/`); track flakiness (e.g., `ConcurrentPageAccess`) and stabilize with retries or fixes.
- Keep `docs/audit/` up to date as features land; adjust IMPLEMENTATION_STATUS_DASHBOARD.md when milestones are actually met.
- 2025-12-13: `cmake --build build` succeeded; `ctest --output-on-failure` hit known crash in `TSAN_LockOrdering` (segfault in TransactionManager::writeTipEntry); all other tests, including new stored-code dependency coverage, passed.

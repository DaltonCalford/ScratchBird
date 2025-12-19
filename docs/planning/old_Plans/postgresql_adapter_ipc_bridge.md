# PostgreSQL Adapter → Engine IPC Bridge Plan

Goal: make the PostgreSQL wire adapter behave like a real PostgreSQL server for clients by translating v3 protocol messages into native ScratchBird IPC/SBLR calls (no in-process execution), with proper transaction mapping, parameter/result conversion, catalog emulation, and streaming behavior. Track work for multi-session execution.

## High-Level Steps
1) **Adapter → native IPC wiring (baseline bridge)**
   - Ensure all query/describe/execute paths go over IPC to the running engine (Unix socket under `build/`), not in-process.
   - Maintain a single IPC client per session; map backend PID/secret to native session IDs.
   - Replace any direct compiler/executor calls with native `QUERY/ROW_DESCRIPTION/ROW_DATA/END_OF_RESULTS/COMMAND_COMPLETE` messaging.

2) **Transaction mapping**
   - Map frontend `BEGIN/COMMIT/ROLLBACK/SAVEPOINT` to native transaction messages; track open transactions per session.
   - Support autocommit semantics expected by libpq: each simple query outside an explicit block runs in its own transaction.
   - Return correct error states on invalid transaction state (`25P01`, `25P02`).

3) **Parameter/result handling**
   - Honor text/binary format codes; decode bind parameter buffers into native values; encode results per requested format.
   - Describe phase should produce accurate OIDs/typmods/format codes from native column metadata; include schema/table/column ids where available.
   - Enforce max_rows/portal fetch counts instead of buffering everything.

4) **Catalog emulation**
   - Populate `remote.emulated.postgresql.<db>.pg_catalog` views that mirror pg_class/pg_attribute/pg_namespace/etc. from the ScratchBird catalog.
   - Expose pg_type, pg_proc, pg_roles minimally to satisfy common clients (`psql`, JDBC) without lying about unsupported features.
   - Ensure default search_path targets the emulated schema for the attached database.

5) **Error mapping**
   - Map native errors to PostgreSQL SQLSTATE/class/severity; include detail/hint/position where available.
   - Ensure protocol-level errors (bad message length/type) close the connection with `FATAL` severity.

6) **Streaming/cursors**
   - Support simple query and extended query flows: Parse/Bind/Describe/Execute/Sync with portals and named statements.
   - Implement portal-scoped fetch counts; stream rows instead of full buffering.

7) **Authentication**
   - Support cleartext/MD5 (as currently) plus SCRAM-SHA-256 if documented; ensure password checks go through server auth config.
   - Respect `sslmode=require` behavior if TLS is documented; otherwise surface clear errors.

8) **End-to-end tests**
   - Integration tests that spin up `sb_server` on a Unix socket in `build/`, then use libpq or a minimal client to:
     - Connect/auth; run simple and extended query flows.
     - Perform DML and verify row counts.
     - Exercise binary parameters/results (int4, text, bytea).
     - Test portal fetch counts and cursor close.
     - Validate pg_catalog projections (pg_class/pg_type/pg_roles minimal set).
     - Error cases: syntax error, constraint violation, invalid statement/portal, bad auth.

9) **Docs/status**
   - Update IMPLEMENTATION_STATUS_DASHBOARD and Alpha 3 tracking when the bridge reaches parity targets.

## Risks / Unknowns
- Binary format coverage (arrays, numerics) may need incremental support.
- TLS/SCRAM expectations from drivers may require additional crypto dependencies.
- Catalog projections must avoid claiming unsupported features while keeping clients happy.

## Tracking / Milestones
- [x] Step 1: All exec/describe paths routed over IPC (no in-process execution). *Simple and extended execute now call the IPC client; search_path is pushed to `remote.emulated.postgresql.<db>` on connect.*
- [ ] Step 2: Transaction mapping and autocommit semantics. *ReadyForQuery now reports IDLE/IN_TRANSACTION/FAILED based on detected BEGIN/COMMIT/ROLLBACK and errors; more precise server-side state mapping still needed.*
- [ ] Step 3: Parameter/result format handling (text/binary) with accurate OIDs/typmods. *RowDescription now propagates portal-bound format codes and row emission matches them for basic types (bool/int/float/string/blob) with a text fallback when binary is unsupported; binary bind values are decoded to text using the statement OIDs, but native binary parameter plumbing and complex-type encoders remain TODO; placeholder substitution now skips quoted/dollar-quoted text but still rewrites to text literals.*
- [ ] Step 4: pg_catalog emulation views under `remote.emulated.postgresql.<db>`.
- [ ] Step 5: Error mapping to SQLSTATE/severity. *SQLSTATE mapping now covers many core statuses (locks, serialization failure, not-null, truncation, etc.) but still needs completeness and severity/detail/hint propagation.*
- [ ] Step 6: Streaming portals/fetch counts. *Execute now streams rows via IPC row_callback, buffers per-portal rows, tracks fetch_pos, and can return PortalSuspended/CommandComplete over multiple Execute calls without re-running the query (portal reuse no longer clears buffers). Row callbacks stop when max_rows is reached and more_rows_available triggers PortalSuspended; still need full cursor lifecycle, dedicated Fetch handling, and binary format support.*
- [ ] Step 7: Auth parity (MD5/SCRAM) and config alignment.
- [ ] Step 8: End-to-end adapter tests over Unix sockets in `build/`.
- [ ] Step 9: Documentation/status updates.

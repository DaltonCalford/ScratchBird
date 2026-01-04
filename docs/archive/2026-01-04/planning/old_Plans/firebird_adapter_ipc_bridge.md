# Firebird Adapter → Engine IPC Bridge Plan

Goal: make the Firebird wire protocol adapter behave like a real Firebird server for clients by translating Firebird wire ops into the ScratchBird native protocol over IPC/SBLR (no in-process shortcuts), with proper BLR/SQLDA handling, transactions, and result streaming. This is a multi-session implementation plan to track progress.

## High-Level Steps
1) **Adapter → native IPC wiring (baseline bridge)**
   - Inside `FirebirdAdapter`, create an IPC client to the local server (Unix socket under `build/` by default).
   - Map `op_connect/attach/create` to native `CONNECT/CONNECT_RESPONSE` messages; keep Firebird handles mapped to native session IDs.
   - Map `op_auth` to native `AUTH_REQUEST/RESPONSE` (Firebird auth stays at adapter boundary; internal auth can be trust or mapped user).
   - Replace in-process `executeQuery` calls with native protocol `QUERY/ROW_DESCRIPTION/ROW_DATA/END_OF_RESULTS/COMMAND_COMPLETE` messages via `ProtocolSession`.
   - Keep adapter state machine intact but route execution through IPC.

2) **Transaction handle mapping**
   - Map Firebird `tr_handle` to native transaction IDs: on first execute/TPB, start a native transaction; store mapping per session.
   - Support `op_commit/op_rollback/op_commit_retaining/op_rollback_retaining` by sending native transaction messages; update adapter state accordingly.
   - Savepoints: if unsupported internally, return appropriate status (e.g., isc_wish_list) but don’t crash.

3) **BLR/SQLDA and parameter/result conversion**
   - Parse BLR from prepare/execute to build parameter metadata; map to native types.
   - Build BLR/SQLDA for result sets from native column info; include charset/length/scale.
   - Convert parameter buffers (XDR, length-prefixed) to native textual/binary params; convert native rows to Firebird wire row buffers (length + data, -1 for NULL).
   - Respect client-specified charset; default to UTF-8 mapping internally.

4) **Catalog emulation**
   - Ensure the Firebird emulation schema exists under `/remote/emulated/firebird/<host>/<db>` in the ScratchBird catalog.
   - Expose RDB$* system tables/views mapped to ScratchBird catalog data; at minimum RDB$RELATIONS, RDB$FIELDS, RDB$FORMATS, RDB$TYPES for basic tooling, plus RELATION_FIELDS/INDICES/TRIGGERS/VIEW_RELATIONS/CHECK_CONSTRAINTS/REF_CONSTRAINTS/PROCEDURES/PROCEDURE_PARAMETERS to satisfy client catalog introspection.
   - Handle `op_info_database` and related info items with Firebird-compatible responses (version string, capabilities).

5) **Result streaming and cursors**
   - Handle `op_prepare`, `op_execute`, `op_execute2`, `op_fetch` with real streaming:
     - Prepare caches native statement/column metadata.
     - Execute stores cursor rows or streams directly; `op_fetch` drains remaining rows, returning status 0 or 100 at end.
   - Support affected row counts for DML via `op_sql_response`.

6) **Error/status vectors**
   - Map native errors to Firebird status vectors (isc_arg_gds, isc_sqlerr, etc.) with sensible isc_* codes for protocol/auth/SQL errors.
   - Ensure adapter never crashes on malformed packets; respond with isc error codes.

7) **End-to-end tests**
   - Add tests that start `sb_server` on a Unix socket under `build/`.
   - Use a Firebird protocol harness (or a minimal client in tests) to:
     - Attach/auth/detach.
     - Prepare/execute/fetch `SELECT 1` and a simple table scan; verify rows and metadata.
     - DML (INSERT/UPDATE/DELETE) and check affected rows.
     - Transactions: begin/commit/rollback; visibility across fetches.
     - Catalog: query RDB$RELATIONS to see created tables.
     - Error paths: bad auth, invalid handles, syntax error.
   - Keep tests self-contained, cleaning up DB files under `build/database/`.

8) **Documentation/status updates**
   - Once functional, update status docs to reflect Firebird bridge readiness and note remaining gaps for other protocols/pooling/auth.

## Risks / Unknowns
- Precise BLR/SQLDA coverage: may need incremental support for complex types.
- Auth expectations: may need SRP support or fallback; decide mapping to internal users.
- Performance: initial implementation can buffer rows; streaming optimizations can follow.

## Tracking / Milestones
- [x] Step 1: IPC bridge in adapter (execute over native protocol, no in-process execution). *Implemented via `client::Connection` over Unix sockets; rows buffered, Firebird compiler used directly with stub `RDB$DATABASE` bootstrap (no V2 fallback).*
- [x] Step 2: Transaction handle mapping to native transactions. *Firebird tr_handles now tracked per session and bound to native begin/commit/rollback paths; retaining reopens within the same handle.*
- [ ] Step 3: BLR/SQLDA param/result conversion (basic scalar types). *BLR is now parsed for simple fields, result rows honor declared lengths and emit null indicators, and input BLR is parsed (with message-length/indicator validation) to feed text/numeric params to the native executor; output BLR is synthesized from native column metadata when the client does not supply one so fetch padding/null indicators stay aligned. Still need full SQLDA mapping (charsets, full type coercion, binary formats).*
- [~] Step 4: Catalog emulation (RDB$* views). *RDB$RELATIONS/FIELDS/RELATION_FIELDS now reflect real catalog tables/columns; RDB$INDICES/INDEX_SEGMENTS/RELATION_CONSTRAINTS/CHECK_CONSTRAINTS/REF_CONSTRAINTS materialize live index + constraint metadata; RDB$TRIGGERS and RDB$PROCEDURES/PROCEDURE_PARAMETERS are populated from catalog triggers/procedures; RDB$VIEW_RELATIONS now prefers compiler-reported SBLR dependencies to map base relations and falls back only when the compiler reports none; adapter switches search_path to `remote.emulated.firebird.<db>`. Remaining work: richer type/charset projections and eliminating the fallback by making dependency capture fully parser-driven.*
- [ ] Step 5: Streaming fetch/cursor handling.
- [ ] Step 6: Error/status vector mapping. *Status vector now carries isc_arg_gds + mapped Firebird code and optional string/sqlstate for common core statuses; mapping widened to cover constraint/type/unsupported cases. Still need fuller coverage of isc codes and warnings.*
- [~] Step 7: End-to-end Firebird tests passing (Unix socket). *Added IPC integration test that asserts RDB$ catalog views surface real indices/constraints; still need full wire-protocol coverage (auth, tx, fetch).*
- [ ] Step 8: Docs/status update.

## Implementation Notes
- Use Unix sockets under `build/ipc_firebird/` to avoid network sandbox limits.
- Reuse native `ProtocolSession` for IPC messaging; keep a single IPC client per Firebird session.
- Maintain handle maps: `db_handle -> IPC session`, `tr_handle -> native transaction`, `stmt_handle -> native statement/cursor`.
- For now, reuse existing Firebird parser/compiler to generate SBLR bytecode when preparing; send to server via native `QUERY` message.

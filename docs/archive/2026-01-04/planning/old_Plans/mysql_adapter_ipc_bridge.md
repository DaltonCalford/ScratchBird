# MySQL Adapter → Engine IPC Bridge Plan

Goal: make the MySQL wire adapter indistinguishable from a real MySQL server for clients by translating MySQL protocol messages into native ScratchBird IPC/SBLR calls (no in-process execution), with correct transaction semantics, result/metadata shaping, catalog emulation, and dialect limits matching MySQL behavior.

## High-Level Steps
1) **Adapter → native IPC wiring (baseline bridge)**
   - Route COM_QUERY/COM_STMT_* to the engine over IPC (Unix socket under `build/`); no direct executor calls.
   - Maintain one IPC client per session; map MySQL connection id to native session id.

2) **Transaction/autocommit mapping**
   - Mirror MySQL autocommit behavior: each statement commits unless autocommit=0 or an explicit transaction is open.
   - Support START TRANSACTION/COMMIT/ROLLBACK; return proper server status flags (SERVER_STATUS_AUTOCOMMIT, IN_TRANSACTION).
   - Handle savepoints minimally or return clear errors if unsupported.

3) **Prepared statements / PS protocol**
   - Implement COM_STMT_PREPARE/EXECUTE/SEND_LONG_DATA/CLOSE with parameter metadata derived from native column types.
   - Decode/encode text and binary parameter/result formats per MySQL PS protocol; respect NULL bitmap and type codes.
   - Enforce max packet size and sequence ids; close cursors cleanly.

4) **Result sets and metadata**
   - Populate column definitions with accurate schema/table/original_* fields drawn from the emulated schema.
   - Respect MySQL type ids/flags/charsets; clamp lengths to declared sizes.
   - Stream rows for large result sets instead of full buffering.

5) **Catalog emulation**
   - Ensure `remote.emulated.mysql.<db>` schema exists per database; project INFORMATION_SCHEMA tables/views that match MySQL expectations (tables, columns, constraints).
   - Default database handling: COM_INIT_DB and db in handshake must switch to the emulated schema.

6) **Error and status mapping**
   - Map native errors to MySQL error codes/SQLSTATE; include affected rows/last insert id and server status flags.
   - Protocol errors (bad sequence/length) should trigger connection close with appropriate errors.

7) **Authentication**
   - Native password (mysql_native_password) already; add caching_sha2_password support if required; ensure actual password checks use server auth config.
   - Enforce SSL/TLS negotiation behavior per documented capabilities (or clear error if unsupported).

8) **End-to-end tests**
   - Integration tests that start `sb_server` on a Unix socket in `build/`, then use mysqlclient or a minimal client to:
     - Connect/auth, run COM_QUERY (text) and COM_STMT_* (binary).
     - DML with affected rows/last insert id validation.
     - INFORMATION_SCHEMA queries (tables/columns) hitting the emulated schema.
     - Error cases: bad auth, syntax error, constraint violation, invalid statement handle, oversized packet.
     - Autocommit on/off behavior with transaction boundaries.

9) **Docs/status**
   - Update IMPLEMENTATION_STATUS_DASHBOARD and Alpha 3 tracking once parity targets are met.

## Risks / Unknowns
- Binary protocol coverage for complex types may need iterative work.
- TLS/caching_sha2 may require additional crypto dependencies or stubs with clear error surfaces.
- Catalog projections must stay aligned with MySQL’s limited feature set to maintain 1:1 parity.

## Tracking / Milestones
- [x] Step 1: All command paths routed over IPC. *COM_QUERY and COM_STMT_EXECUTE now run through the IPC client and switch to the emulated schema namespace when possible.*
- [ ] Step 2: Transaction/autocommit mapping with correct status flags. *Basic autocommit toggling/BEGIN/COMMIT/ROLLBACK detection now updates status flags; needs validation against server-side transaction state and savepoint coverage.*
- [ ] Step 3: Prepared statement protocol (text/binary params/results).
   - [ ] Parameter decoding/NULL bitmap/type codes. *Binary execute now decodes NULL bitmap, parameter types/unsigned flags, and substitutes literals into the rewritten SQL; still need true binary result encoding.*
   - [ ] Result metadata/type flags/charsets. *PREPARE now attempts a lightweight LIMIT 0 on SELECT/WITH to return column metadata and emits column definitions; charsets now mark BINARY for bytea and flags set NUM/BLOB/TIMESTAMP heuristics, but full charset/flag fidelity is still missing.*
- [ ] Step 4: INFORMATION_SCHEMA emulation under `remote.emulated.mysql.<db>`. *Adapter now bootstraps `schemata/tables/columns/table_constraints/key_column_usage/referential_constraints/statistics/routines` under the emulated schema and copies rows from the engine’s information_schema (excluding pg/iso schemas). Needs routines completeness/views, full referential constraint alignment, statistics accuracy, and adherence to MySQL limits.*
- [ ] Step 5: Error/status mapping to MySQL codes/SQLSTATE. *Core status codes are now mapped to ER codes/SQLSTATE (including FK/not-null/lock/deadlock/truncation/out-of-range/div-by-zero); needs real engine error translation and affected rows/last-insert-id wiring.*
- [ ] Step 6: Streaming row delivery (no full buffering).
- [ ] Step 7: Auth parity (native password + optional caching_sha2/TLS handling).
- [ ] Step 8: End-to-end MySQL adapter tests over Unix sockets in `build/`.
- [ ] Step 9: Documentation/status updates.

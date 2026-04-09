# ODBC Driver Baseline Specification

## Purpose
Define the ODBC driver implementation contract that must meet or exceed the JDBC baseline in `DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md`.

## Scope
- ODBC 3.8 compatible driver manager integration.
- Connection, authentication, transaction, execution, metadata, and type behavior.
- Recursive schema tree behavior for metadata-driven tooling.
- Conformance gates and evidence required for release-tier status.

## Normative Inputs
1. `DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md`
2. `CONNECTIVITY_PROFILES_AND_TOOL_RUNTIME.md`
3. `CLIENT_ERROR_AND_RESULT_MODEL.md`
4. `../21_V3_Dialect_Surface/*` and V3 authorized parser dialect guide.

## ODBC Surface Contract

### ODBCBL-CONN: Connection and Configuration
1. `ODBCBL-CONN-001`: DSN and connection-string forms MUST support host, port, database, user, password, protocol, ssl mode, front-door mode, and timeout controls.
2. `ODBCBL-CONN-002`: Attribute precedence MUST mirror JDBC baseline (`JDBCBL-CONN-002`).
3. `ODBCBL-CONN-003`: `SQLConnect` and `SQLDriverConnect` MUST produce equivalent session semantics.
4. `ODBCBL-CONN-004`: `SQLSetConnectAttr`/`SQLGetConnectAttr` MUST expose autocommit, access mode, network timeout, and current schema controls.
5. `ODBCBL-CONN-005`: Driver MUST negotiate password and SCRAM-SHA-256 authentication.
6. `ODBCBL-CONN-006`: Driver MUST support direct and manager-proxy front-door execution.

### ODBCBL-TXN: Transaction and Session
1. `ODBCBL-TXN-001`: `SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT)` MUST preserve JDBC-equivalent autocommit transitions.
2. `ODBCBL-TXN-002`: `SQLEndTran` commit/rollback semantics MUST match `JDBCBL-TXN-*` behavior.
3. `ODBCBL-TXN-003`: Savepoint API surface MUST be provided via SQL command contract with deterministic error mapping.
4. `ODBCBL-TXN-004`: Current schema set/get MUST be supported using metadata-safe session controls.

### ODBCBL-EXEC: Statement Execution
1. `ODBCBL-EXEC-001`: `SQLExecDirect`, `SQLPrepare` + `SQLBindParameter` + `SQLExecute` MUST both be supported.
2. `ODBCBL-EXEC-002`: Positional bind parameters MUST be supported with deterministic type coercion.
3. `ODBCBL-EXEC-003`: Batch execution MUST be supported using parameter arrays and/or multi-exec equivalents.
4. `ODBCBL-EXEC-004`: Multiple result sets MUST be supported through `SQLMoreResults`.
5. `ODBCBL-EXEC-005`: Generated key retrieval MUST be available and deterministic.
6. `ODBCBL-EXEC-006`: Timeout and cancellation MUST be supported via statement attributes and async cancel surface.
7. `ODBCBL-EXEC-007`: Streaming/fetch-size behavior MUST support large result traversal without full materialization.

### ODBCBL-META: Metadata and Recursive Schema Tree
1. `ODBCBL-META-001`: `SQLTables`, `SQLColumns`, `SQLPrimaryKeys`, `SQLForeignKeys`, `SQLStatistics`, `SQLProcedures`, `SQLProcedureColumns`, `SQLGetTypeInfo` MUST be implemented.
2. `ODBCBL-META-002`: Schema metadata MUST preserve recursive path ancestry.
3. `ODBCBL-META-003`: Metadata MUST support parent expansion mode equivalent to JDBC `metadataExpandSchemaParents`.
4. `ODBCBL-META-004`: Tree building for tooling MUST be metadata-only (`database -> schema branches -> objects`).
5. `ODBCBL-META-005`: Parent namespace uniqueness and cross-schema same-name allowance MUST be preserved.
6. `ODBCBL-META-006`: Metadata payload MUST include DDL-editor essential fields (object kind, parent path, key/index/column metadata).

### ODBCBL-TYPE: Type and Object Families
1. `ODBCBL-TYPE-001`: Scalar conversions MUST cover baseline scalar/time/numeric families.
2. `ODBCBL-TYPE-002`: Array, blob/clob, struct/ref, rowid, sqlxml-equivalent behavior MUST be mapped through ODBC-compatible type contracts.
3. `ODBCBL-TYPE-003`: JSONB, geometry, range, and raw passthrough families MUST be preserved where server surfaces them.
4. `ODBCBL-TYPE-004`: Parameter and result metadata MUST be type-safe and deterministic.

### ODBCBL-ERR: Errors, SQLSTATE, Diagnostics
1. `ODBCBL-ERR-001`: `SQLSTATE` mapping MUST remain deterministic and baseline-compatible.
2. `ODBCBL-ERR-002`: `SQLGetDiagRec` records MUST include stable native error codes and message classes.
3. `ODBCBL-ERR-003`: Timeout, cancel, auth, network, and metadata errors MUST map to documented SQLSTATE families.

### ODBCBL-RES: Pooling and Resilience
1. `ODBCBL-RES-001`: Pooling behavior MUST preserve acquisition timeout and validation semantics.
2. `ODBCBL-RES-002`: Keepalive and recoverable reconnect policy MUST be configurable.
3. `ODBCBL-RES-003`: Observability counters for connect/execute/error/timeout/cancel MUST be emitted.

## Default Compatibility Profile
The ODBC driver MUST preserve equivalent defaults from `JDBCBL-CFG-002` unless explicitly approved as stronger defaults with compatibility notes.

## Conformance Mapping
1. `ODBCBL-*` requirements are mandatory and trace to `JDBCBL-*` groups.
2. Passing `T30-I` and `T30-J` suites is required for release-tier promotion.
3. Any intentional deviation requires an approved exception entry with migration impact analysis.

## Evidence Required
1. ODBC conformance test logs and SQLSTATE matrices.
2. Recursive schema metadata snapshots proving metadata-only tree generation.
3. Type round-trip coverage report for scalar and advanced families.
4. Timeout/cancel/recovery reliability report.

## 2026-03-28 Audit Normalization Update

- Section `30` is normalized to the code-backed `partial` standard.
- Current authority is bounded to the shipped `ScratchBird-driver` surfaces, especially `tracks/p3/drivers/*`, shared connectivity docs, and the concrete CLI/runtime seams.
- Direct native and manager-proxy are the current portable client contract.
- Local runtime modes such as `embedded` and `local-ipc` are bounded tooling/runtime surfaces, not universal parity claims for every maintained language driver.
- The C/C++ lane in the current driver repo is intentionally IP-only; current CLI `embedded` mode is routed through local IPC in the present beta C++ runtime.
- Tool command truth is bounded to the shipped `sb_isql`, `sb_admin`, `sb_backup`, `sb_security`, `sb_verify`, and `sbdriver-conformance` surfaces.
- Recovery language follows MGA/session-repair rules and explicitly excludes WAL-style transaction replay.
- Forensic replay, migration/passthrough, and replication control narratives remain bounded, checklist-only, or target-state-only unless a shipped lane-local control surface is proven.
- Driver-lane claims must stay tied to the current maintained lane set and must not assume universal cross-language parity from section-outline text alone.

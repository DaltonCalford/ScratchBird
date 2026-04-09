# Go Driver Baseline Specification

## Purpose
Define the Go driver implementation contract (`database/sql` compatible) that must meet or exceed the JDBC baseline in `DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md`.

## Scope
- `database/sql/driver` integration and direct-native client support.
- Connection/config, execution, metadata, type mapping, and resilience behavior.
- Recursive schema tree behavior for metadata-driven tooling.

## Normative Inputs
1. `DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md`
2. `CONNECTIVITY_PROFILES_AND_TOOL_RUNTIME.md`
3. `CLIENT_ERROR_AND_RESULT_MODEL.md`
4. V3 parser/dialect contracts.

## Go Surface Contract

### GOBL-CONN: Connection and Config
1. `GOBL-CONN-001`: DSN MUST support JDBC-equivalent baseline fields and defaults.
2. `GOBL-CONN-002`: Config precedence MUST mirror `JDBCBL-CONN-002`.
3. `GOBL-CONN-003`: Driver MUST expose deterministic connection lifecycle and ping validation.
4. `GOBL-CONN-004`: Password and SCRAM-SHA-256 auth MUST be supported.
5. `GOBL-CONN-005`: Direct and manager-proxy front-door modes MUST be supported.

### GOBL-TXN: Transaction and Session
1. `GOBL-TXN-001`: `BeginTx`, `Commit`, `Rollback` MUST match baseline semantics.
2. `GOBL-TXN-002`: Autocommit-equivalent behavior MUST be deterministic under `database/sql` semantics.
3. `GOBL-TXN-003`: Savepoint operations MUST be supported via explicit API or deterministic SQL helper surface.
4. `GOBL-TXN-004`: Session schema controls MUST be supported.

### GOBL-EXEC: Query Execution
1. `GOBL-EXEC-001`: `ExecContext` and `QueryContext` MUST support simple and prepared execution.
2. `GOBL-EXEC-002`: Positional binds MUST be supported; named parameter aliasing equivalent to JDBC baseline MUST be supported.
3. `GOBL-EXEC-003`: Batch execution MUST be supported.
4. `GOBL-EXEC-004`: Multi-result traversal MUST be supported (`Rows.NextResultSet`).
5. `GOBL-EXEC-005`: Generated key retrieval MUST be deterministic.
6. `GOBL-EXEC-006`: Timeout and cancellation MUST map to context cancellation and deadline behavior deterministically.
7. `GOBL-EXEC-007`: Streaming behavior MUST avoid forced full materialization.
8. `GOBL-EXEC-008`: Routine/callable semantics equivalent to JDBC callable behavior MUST be supported.

### GOBL-META: Metadata and Recursive Schema Tree
1. `GOBL-META-001`: Metadata APIs MUST expose catalog/schema/table/column/index/key/routine/type families.
2. `GOBL-META-002`: Recursive schema ancestry MUST be preserved.
3. `GOBL-META-003`: Parent expansion mode equivalent to JDBC baseline MUST be supported.
4. `GOBL-META-004`: Metadata-only tree generation MUST support `database -> schema branches -> objects`.
5. `GOBL-META-005`: Parent uniqueness and cross-schema same-name semantics MUST be preserved.
6. `GOBL-META-006`: Metadata output MUST include DDL editor required fields.

### GOBL-TYPE: Type and Object Mapping
1. `GOBL-TYPE-001`: Scalar type families MUST meet baseline.
2. `GOBL-TYPE-002`: Array/blob/clob/struct/ref/rowid/sqlxml-equivalent families MUST be supported.
3. `GOBL-TYPE-003`: JSONB/geometry/range/raw passthrough families MUST be supported.
4. `GOBL-TYPE-004`: `Scan` and parameter conversion behavior MUST be deterministic and documented.
5. `GOBL-TYPE-005`: Parameter/result metadata MUST be exposed with stable typing metadata.

### GOBL-ERR: Error and SQLSTATE
1. `GOBL-ERR-001`: SQLSTATE and native code mapping MUST be deterministic.
2. `GOBL-ERR-002`: Timeout/cancel/auth/network errors MUST map to stable error classes.
3. `GOBL-ERR-003`: Recoverable vs fatal classification MUST be available for retry policy.

### GOBL-RES: Resilience and Pooling
1. `GOBL-RES-001`: Pooling via `database/sql` controls MUST preserve baseline semantics.
2. `GOBL-RES-002`: Validation/keepalive/reconnect controls MUST be exposed.
3. `GOBL-RES-003`: Telemetry counters and timing hooks MUST be exposed.

## Conformance and Evidence
1. `GOBL-*` requirements MUST trace to `JDBCBL-*` requirements.
2. Passing `T30-I` and `T30-J` suites is mandatory.
3. Evidence must include context timeout/cancel proofs, metadata tree snapshots, and type round-trip reports.

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

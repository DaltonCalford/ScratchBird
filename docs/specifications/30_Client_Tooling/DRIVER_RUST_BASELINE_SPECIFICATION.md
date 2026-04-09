# Rust Driver Baseline Specification

## Purpose
Define the Rust driver implementation contract (sync and async client surfaces) that must meet or exceed the JDBC baseline in `DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md`.

## Scope
- Rust driver API surface and optional `tokio` async surface.
- Connection, execution, metadata, type mapping, and resilience behavior.
- Recursive schema tree behavior for metadata-driven tooling.

## Normative Inputs
1. `DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md`
2. `CLIENT_ERROR_AND_RESULT_MODEL.md`
3. `CONNECTIVITY_PROFILES_AND_TOOL_RUNTIME.md`
4. V3 parser/dialect contracts.

## Rust Surface Contract

### RUSTBL-CONN: Connection and Config
1. `RUSTBL-CONN-001`: Connection options MUST support JDBC-equivalent baseline fields and defaults.
2. `RUSTBL-CONN-002`: Config precedence MUST mirror `JDBCBL-CONN-002`.
3. `RUSTBL-CONN-003`: Connection lifecycle MUST expose deterministic connect/close/is-valid semantics.
4. `RUSTBL-CONN-004`: Password and SCRAM-SHA-256 auth MUST be supported.
5. `RUSTBL-CONN-005`: Direct and manager-proxy front-door modes MUST be supported.

### RUSTBL-TXN: Transaction and Session
1. `RUSTBL-TXN-001`: Autocommit-equivalent behavior MUST be deterministic.
2. `RUSTBL-TXN-002`: Explicit transaction begin/commit/rollback MUST match baseline behavior.
3. `RUSTBL-TXN-003`: Savepoint create/release/rollback-to MUST be supported.
4. `RUSTBL-TXN-004`: Session schema controls MUST be supported.

### RUSTBL-EXEC: Execution
1. `RUSTBL-EXEC-001`: Simple and prepared query execution MUST be supported.
2. `RUSTBL-EXEC-002`: Positional bind parameters MUST be supported; named alias equivalence to JDBC baseline MUST be supported.
3. `RUSTBL-EXEC-003`: Batch execution MUST be supported.
4. `RUSTBL-EXEC-004`: Multi-result traversal MUST be supported.
5. `RUSTBL-EXEC-005`: Generated key retrieval MUST be deterministic.
6. `RUSTBL-EXEC-006`: Timeout and cancellation MUST be supported for sync and async surfaces.
7. `RUSTBL-EXEC-007`: Streaming row consumption MUST avoid forced full buffering.
8. `RUSTBL-EXEC-008`: Routine/callable semantics equivalent to JDBC callable behavior MUST be supported.

### RUSTBL-META: Metadata and Recursive Schema Tree
1. `RUSTBL-META-001`: Metadata APIs MUST cover catalog/schema/table/column/index/key/privilege/routine/type families.
2. `RUSTBL-META-002`: Recursive schema ancestry MUST be preserved.
3. `RUSTBL-META-003`: Parent expansion mode equivalent to JDBC baseline MUST be supported.
4. `RUSTBL-META-004`: Metadata-only tree generation MUST support `database -> schema branches -> objects`.
5. `RUSTBL-META-005`: Parent uniqueness and cross-schema same-name semantics MUST be preserved.
6. `RUSTBL-META-006`: Metadata payload MUST satisfy DDL editor needs.

### RUSTBL-TYPE: Type and Object Mapping
1. `RUSTBL-TYPE-001`: Scalar type families MUST meet baseline coverage.
2. `RUSTBL-TYPE-002`: Array/blob/clob/struct/ref/rowid/sqlxml-equivalent families MUST be supported.
3. `RUSTBL-TYPE-003`: JSONB/geometry/range/raw families MUST be preserved.
4. `RUSTBL-TYPE-004`: Typed decode/encode APIs MUST be deterministic.
5. `RUSTBL-TYPE-005`: Parameter/result metadata MUST include stable typing contracts.

### RUSTBL-ERR: Error and SQLSTATE
1. `RUSTBL-ERR-001`: Error types MUST include SQLSTATE/native code mapping.
2. `RUSTBL-ERR-002`: Timeout/cancel/auth/network families MUST map deterministically.
3. `RUSTBL-ERR-003`: Recoverable and non-recoverable classes MUST be distinguished.

### RUSTBL-RES: Resilience and Pooling
1. `RUSTBL-RES-001`: Pooling MUST support configurable bounds and acquisition timeout.
2. `RUSTBL-RES-002`: Validation/keepalive/reconnect controls MUST be available.
3. `RUSTBL-RES-003`: Observability counters and tracing hooks MUST be exposed.

## Async Parity Contract
1. Async and sync surfaces MUST preserve equivalent semantics and error mapping.
2. Cancellation and timeout behavior MUST be deterministic under async runtime scheduling.

## Conformance and Evidence
1. `RUSTBL-*` requirements MUST trace to `JDBCBL-*` requirements.
2. Passing `T30-I` and `T30-J` suites is mandatory.
3. Evidence must include sync/async parity proofs, metadata tree snapshots, and type round-trip reports.

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

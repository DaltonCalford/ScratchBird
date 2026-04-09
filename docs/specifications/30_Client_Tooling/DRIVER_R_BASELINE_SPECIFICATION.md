# R Driver Baseline Specification

## Purpose
Define the r driver implementation contract that must meet or exceed the JDBC baseline in DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md.

## Scope
- Driver API surface and runtime behavior for the r lane.
- Connection, transaction, execution, metadata, type, error, and resilience behavior.
- Metadata-only recursive schema tree behavior for tooling.

## Normative Inputs
1. DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md
2. CONNECTIVITY_PROFILES_AND_TOOL_RUNTIME.md
3. CLIENT_ERROR_AND_RESULT_MODEL.md
4. Section-21 V3 parser and dialect contracts.

## R Driver Surface Contract

### RBL-CONN: Connection and Configuration
1. RBL-CONN-001: Connection configuration MUST support JDBC-equivalent baseline fields and defaults.
2. RBL-CONN-002: Configuration precedence MUST mirror JDBCBL-CONN-002.
3. RBL-CONN-003: Driver MUST provide deterministic connection lifecycle and validation semantics.
4. RBL-CONN-004: Password and SCRAM-SHA-256 auth MUST be supported.
5. RBL-CONN-005: Direct and manager-proxy front-door modes MUST be supported.

### RBL-TXN: Transaction and Session
1. RBL-TXN-001: Autocommit-equivalent behavior MUST match JDBC baseline semantics.
2. RBL-TXN-002: Explicit transaction begin/commit/rollback behavior MUST be supported.
3. RBL-TXN-003: Savepoint create/release/rollback-to behavior MUST be supported.
4. RBL-TXN-004: Session schema controls MUST be supported.

### RBL-EXEC: Execution Behavior
1. RBL-EXEC-001: Simple and prepared execution surfaces MUST be supported.
2. RBL-EXEC-002: Positional bind behavior MUST be supported.
3. RBL-EXEC-003: Named parameter aliasing equivalent to JDBC baseline MUST be supported.
4. RBL-EXEC-004: Batch execution MUST be supported.
5. RBL-EXEC-005: Multi-result traversal MUST be supported.
6. RBL-EXEC-006: Generated key retrieval MUST be deterministic.
7. RBL-EXEC-007: Timeout and cancellation MUST be supported.
8. RBL-EXEC-008: Streaming behavior MUST avoid forced full buffering.
9. RBL-EXEC-009: Callable/routine invocation behavior equivalent to JDBC baseline MUST be supported.

### RBL-META: Metadata and Recursive Schema Tree
1. RBL-META-001: Metadata APIs MUST cover catalog/schema/table/column/index/key/privilege/routine/type families.
2. RBL-META-002: Metadata MUST preserve recursive schema ancestry.
3. RBL-META-003: Parent expansion mode equivalent to metadataExpandSchemaParents MUST be supported.
4. RBL-META-004: Schema tree generation for tooling MUST be metadata-only.
5. RBL-META-005: Parent uniqueness and cross-schema same-name behavior MUST be preserved.
6. RBL-META-006: Metadata payload MUST satisfy DDL editor support requirements.

### RBL-TYPE: Type and Object Mapping
1. RBL-TYPE-001: Scalar type coverage MUST meet JDBC baseline requirements.
2. RBL-TYPE-002: Array/blob/clob/struct/ref/rowid/sqlxml-equivalent families MUST be supported.
3. RBL-TYPE-003: JSONB/geometry/range/raw passthrough families MUST be preserved.
4. RBL-TYPE-004: Typed conversion APIs MUST be deterministic.
5. RBL-TYPE-005: Parameter/result metadata MUST be complete and stable.

### RBL-ERR: Error and SQLSTATE
1. RBL-ERR-001: SQLSTATE/native-code mapping MUST be deterministic.
2. RBL-ERR-002: Timeout/cancel/auth/network errors MUST map to stable classes.
3. RBL-ERR-003: Recoverable vs non-recoverable classification MUST be available.

### RBL-RES: Resilience and Pooling
1. RBL-RES-001: Pooling behavior (where applicable) MUST preserve baseline semantics.
2. RBL-RES-002: Keepalive/reconnect controls MUST be configurable.
3. RBL-RES-003: Telemetry and observability counters MUST be exposed.

## Lane Ownership
- Implementation path: tracks/beta/drivers/r

## Conformance and Evidence
1. RBL-* requirements MUST trace to JDBCBL-* requirements.
2. Passing T30-I and T30-J suites is mandatory.
3. Evidence must include metadata-tree proof, type round-trip report, and resilience/error mapping report.

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

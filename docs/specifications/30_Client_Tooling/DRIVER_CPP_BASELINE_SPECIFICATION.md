# C++ Driver Baseline Specification

## Purpose
Define the native C++ driver implementation contract that must meet or exceed the JDBC baseline in `DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md`.

## Scope
- Native C++ client library API and ABI contract.
- Sync and optional async execution behavior.
- Metadata-driven recursive schema navigation support.
- Type/object mapping and resilience behavior.
- Current maintained lane transport applicability.

## Normative Inputs
1. `DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md`
2. `EMBEDDED_AND_LINKED_LIBRARY_API.md`
3. `CONNECTIVITY_PROFILES_AND_TOOL_RUNTIME.md`
4. `CLIENT_ERROR_AND_RESULT_MODEL.md`

## C++ Surface Contract

### CPPBL-CONN: Connection and Configuration
1. `CPPBL-CONN-001`: Library MUST expose builder/config APIs for host, port, database, user, password, protocol, ssl mode, front-door mode, schema, and timeout controls.
2. `CPPBL-CONN-002`: Config precedence MUST mirror `JDBCBL-CONN-002`.
3. `CPPBL-CONN-003`: Connection lifecycle MUST include deterministic `open`, `close`, `is_valid` semantics.
4. `CPPBL-CONN-004`: Password and SCRAM-SHA-256 auth MUST be supported.
5. `CPPBL-CONN-005`: Direct and manager-proxy front-door modes MUST be supported.
6. `CPPBL-CONN-006`: The maintained ScratchBird-driver C++ lane transport scope is native INET direct and manager-proxy only; embedded and local IPC runtime profiles are non-normative for this lane.
7. `CPPBL-CONN-007`: Startup policy controls equivalent to `client_flags`, explicit auth method selection, auth payload/profile variants, required/forbidden methods, channel-binding requirement, workload identity, and proxy assertion inputs MUST be supported.

### CPPBL-TXN: Transaction and Session
1. `CPPBL-TXN-001`: Autocommit on/off behavior MUST match `JDBCBL-TXN-001`.
2. `CPPBL-TXN-002`: Explicit `begin`, `commit`, `rollback` API calls MUST be provided.
3. `CPPBL-TXN-003`: Savepoint create/release/rollback-to MUST be provided.
4. `CPPBL-TXN-004`: Current schema controls MUST be exposed.

### CPPBL-EXEC: Statements and Queries
1. `CPPBL-EXEC-001`: Simple statement execution MUST be supported.
2. `CPPBL-EXEC-002`: Prepared statement execution with positional binds MUST be supported.
3. `CPPBL-EXEC-003`: Named-parameter aliasing equivalent to JDBC behavior MUST be supported.
4. `CPPBL-EXEC-004`: Batch execution MUST be supported.
5. `CPPBL-EXEC-005`: Multi-result traversal MUST be supported.
6. `CPPBL-EXEC-006`: Generated key retrieval MUST be supported.
7. `CPPBL-EXEC-007`: Query timeout and cancellation MUST be supported.
8. `CPPBL-EXEC-008`: Streaming result consumption MUST avoid forced full buffering.
9. `CPPBL-EXEC-009`: Callable/routine invocation surface equivalent to JDBC callable behavior MUST be supported.

### CPPBL-META: Metadata and Recursive Schema Tree
1. `CPPBL-META-001`: Metadata APIs MUST cover catalog/schema/table/column/key/index/privilege/routine/type families.
2. `CPPBL-META-002`: Metadata MUST preserve recursive schema path ancestry.
3. `CPPBL-META-003`: Parent expansion mode equivalent to `metadataExpandSchemaParents` MUST be supported.
4. `CPPBL-META-004`: Metadata-only tree generation MUST be supported for tooling:
`database -> schema branches -> objects`.
5. `CPPBL-META-005`: Parent uniqueness and cross-schema same-name behavior MUST be preserved.
6. `CPPBL-META-006`: Metadata payload MUST support DDL editor requirements.

### CPPBL-TYPE: Type and Object Mapping
1. `CPPBL-TYPE-001`: Scalar bind/decode coverage MUST meet baseline.
2. `CPPBL-TYPE-002`: Array/blob/clob/struct/ref/rowid/sqlxml-equivalent object families MUST be supported.
3. `CPPBL-TYPE-003`: JSONB/geometry/range/raw families MUST be preserved.
4. `CPPBL-TYPE-004`: Typed get/set conversion APIs MUST be deterministic.
5. `CPPBL-TYPE-005`: Parameter and result metadata MUST be exposed for type-safe client behavior.

### CPPBL-ERR: Error Model and SQLSTATE
1. `CPPBL-ERR-001`: Driver MUST expose stable SQLSTATE and native code error metadata.
2. `CPPBL-ERR-002`: Timeout/cancel/auth/network errors MUST map deterministically.
3. `CPPBL-ERR-003`: Recoverable vs non-recoverable errors MUST be classifiable for retry policy.

### CPPBL-RES: Resilience and Pooling
1. `CPPBL-RES-001`: Connection pooling API MUST support bounds, acquisition timeout, and validation.
2. `CPPBL-RES-002`: Keepalive and reconnect policy controls MUST be available.
3. `CPPBL-RES-003`: Telemetry counters and timing hooks MUST be exposed.

## ABI and Packaging
1. C ABI shim for language bindings SHOULD be provided where needed.
2. ABI stability policy MUST be versioned and documented.
3. Linux and Windows artifacts MUST be produced for release-tier status.

## Conformance and Evidence
1. `CPPBL-*` requirements MUST trace to `JDBCBL-*` requirements.
2. Passing `T30-I` and `T30-J` suites is mandatory.
3. Evidence set MUST include metadata tree proofs, type round-trip matrices, and resilience/cancel/timeout logs.

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

## 2026-03-28 Hardening Promotion Update

- Section `30` now carries explicit bounded authority for current maintained `ScratchBird-driver` `p3` lanes.
- Embedded and linked-library language is bounded by the current IP-only C/C++ lane plus tool-local `embedded` or `local-ipc` seams.
- Direct native and manager-proxy remain the current portable client baseline.
- CLI authority is bounded to shipped `sb_isql`, `sb_admin`, `sb_backup`, `sb_security`, `sb_verify`, and `sbdriver-conformance`.
- Error and reconnect language is bounded to deterministic MGA/session repair and explicitly excludes whole-transaction replay.
- Installer, replay, migration, passthrough, and replication client-control claims remain bounded or `target_state_only` unless maintained lane-local proof is promoted.

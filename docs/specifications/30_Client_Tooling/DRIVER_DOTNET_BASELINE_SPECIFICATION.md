# .NET Driver Enterprise Baseline Implementation Specification

## Purpose
Define the implementation-backed .NET (ADO.NET) driver contract that must meet or exceed the JDBC baseline in `DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md`, including enterprise-readiness requirements and workplan-ready gap classification.

## Scope
- ADO.NET provider behavior for `DbConnection`, `DbCommand`, `DbDataReader`, `DbTransaction`, parameter, metadata, and factory surfaces.
- Protocol startup/authentication, front-door modes, and network runtime behavior.
- Sync/async parity, cancellation, timeout, pooling, and recovery behavior.
- Recursive schema metadata and DDL/editor compatibility behavior.
- Type encoding/decoding and LOB streaming behavior.
- Enterprise gate evidence and remediation backlog seeding.

## Baseline Inputs
1. `DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md`
2. `TEST_CONTRACT.md` (especially `T30-I*` and `T30-J04`)
3. `CLIENT_ERROR_AND_RESULT_MODEL.md`
4. `CONNECTIVITY_PROFILES_AND_TOOL_RUNTIME.md`
5. `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/dotnet/src/ScratchBird.Data/`
6. `/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/dotnet/tests/ScratchBird.Data.Tests/`
7. `/home/dcalford/CliWork/ScratchBird-driver/artifacts/enterprise-readiness/DOTNET-101/`
8. `/home/dcalford/CliWork/ScratchBird-driver/artifacts/enterprise-readiness/DOTNET-102/`
9. `/home/dcalford/CliWork/ScratchBird-driver/artifacts/enterprise-readiness/DOTNET-103/`
10. `/home/dcalford/CliWork/ScratchBird-driver/artifacts/enterprise-readiness/DOTNET-104/`
11. `/home/dcalford/CliWork/ScratchBird-driver/docs/planning/DRIVER_ENTERPRISE_READINESS_STRICT_IMPLEMENTATION_MATRIX_2026-02-22.md`

## Applicability Model
1. `DOTNETBL-*` requirements in this document are normative for the maintained .NET lane.
2. API shape may differ from JDBC, but semantics must remain equivalent.
3. The .NET lane MAY exceed JDBC baseline capabilities.
4. Known JDBC defects are not normative requirements to reproduce.
5. Workplan generation MUST classify each requirement as `MET`, `PARTIAL`, `MISSING`, or `EXCEEDS`.

## Invariants
1. Driver behavior MUST preserve parser/engine separation and MUST NOT implement engine semantics in the client.
2. Recursive schema navigation MUST be metadata-derived and MUST NOT depend on SQL text parsing heuristics.
3. Security defaults MUST remain TLS-on and deterministic.
4. Auth policy hints supplied by callers MUST remain transported end-to-end without silent mutation.

## Normative Requirements

### DOTNETBL-CONN: Connection, DSN, Runtime
1. `DOTNETBL-CONN-001`: Driver MUST accept ScratchBird URI DSN and key/value connection-string forms.
2. `DOTNETBL-CONN-002`: DSN aliases for host/port/database/user/password/schema/protocol/front-door mode MUST be deterministic and case-insensitive.
3. `DOTNETBL-CONN-003`: `protocol` MUST normalize to native wire semantics only; unsupported protocols MUST fail deterministically.
4. `DOTNETBL-CONN-004`: `front_door_mode` MUST support `direct` and `manager_proxy`.
5. `DOTNETBL-CONN-005`: `Open`/`OpenAsync` MUST provide deterministic state transitions and bounded reconnect attempts.
6. `DOTNETBL-CONN-006`: `Close`/dispose MUST release or return pooled resources without leaking active transaction state.
7. `DOTNETBL-CONN-007`: Schema configuration MUST apply deterministic `SET SCHEMA` or `SET SEARCH_PATH` behavior, including quoted and recursive paths.
8. `DOTNETBL-CONN-008`: Connection controls MUST expose host, port, database, SSL mode, connect timeout, socket timeout, pooling, and fetch-size controls.
9. `DOTNETBL-CONN-009`: `ChangeDatabase` semantics MUST be deterministic and reject empty database targets.
10. `DOTNETBL-CONN-010`: Runtime health checks MUST reconnect unhealthy transports before command execution where safe.

### DOTNETBL-AUTH: Authentication and Security
1. `DOTNETBL-AUTH-001`: TLS MUST be required by default; insecure disable MUST require explicit opt-in.
2. `DOTNETBL-AUTH-002`: Startup handshake MUST support password and SCRAM-SHA-256 authentication.
3. `DOTNETBL-AUTH-003`: Driver MUST support auth policy transport fields: `auth_method_id`, payload variants, provider profile, required/forbidden methods, and channel-binding requirement.
4. `DOTNETBL-AUTH-004`: `auth_method_id` MUST enforce ScratchBird namespace validation.
5. `DOTNETBL-AUTH-005`: Manager-proxy mode MUST support MCP hello/auth/connect sequence with token-based auth.
6. `DOTNETBL-AUTH-006`: Workload identity and proxy assertion payload fields MUST be transportable during startup.
7. `DOTNETBL-AUTH-007`: Unsupported auth modes/methods MUST map to deterministic SQLSTATE-aligned auth exceptions.
8. `DOTNETBL-AUTH-008`: Driver MUST preserve forward compatibility for plugin auth by allowing explicit caller-selected method/payload input.
9. `DOTNETBL-AUTH-009`: Driver MUST implement capability-based auth-method discovery handshake when section-27 server handshake extension is available.

### DOTNETBL-TXN: Transactions and Session State
1. `DOTNETBL-TXN-001`: Autocommit-equivalent behavior MUST match ADO.NET semantics when no explicit transaction exists.
2. `DOTNETBL-TXN-002`: `BeginTransaction`, `Commit`, `Rollback` MUST map to deterministic wire transaction operations.
3. `DOTNETBL-TXN-003`: Isolation levels MUST map deterministically to native protocol isolation constants.
4. `DOTNETBL-TXN-004`: Savepoint create, rollback-to, and release semantics MUST be supported with deterministic validation.
5. `DOTNETBL-TXN-005`: Commands executed while connection has active transaction MUST require explicit transaction association.
6. `DOTNETBL-TXN-006`: Transaction disposal MUST rollback unfinished transactions safely.
7. `DOTNETBL-TXN-007`: Invalid transaction lifecycle operations MUST fail deterministically.

### DOTNETBL-EXEC: Statements, Prepared, Callable, Multi-Result
1. `DOTNETBL-EXEC-001`: `CommandType.Text` and stored-procedure callable semantics MUST be supported.
2. `DOTNETBL-EXEC-002`: Positional parameter binding MUST be supported with deterministic SQL normalization.
3. `DOTNETBL-EXEC-003`: Named parameter aliases (for example `:name`, `@name`) MUST normalize to JDBC-equivalent positional behavior.
4. `DOTNETBL-EXEC-004`: Prepared statement cache MUST be deterministic, bounded, and invalidated on schema mutation paths.
5. `DOTNETBL-EXEC-005`: Driver MUST support execution variants: scalar, non-query, reader, batch, and generated-key retrieval.
6. `DOTNETBL-EXEC-006`: Query timeout and cancellation MUST map to deterministic cancel/timeout behavior.
7. `DOTNETBL-EXEC-007`: Fetch-size and streaming semantics MUST avoid mandatory full-buffer materialization.
8. `DOTNETBL-EXEC-008`: `NativeSql` and callable SQL normalization MUST be exposed.
9. `DOTNETBL-EXEC-009`: Driver MUST provide multi-result traversal semantics equivalent to JDBC `getMoreResults`.
10. `DOTNETBL-EXEC-010`: Async execution APIs MUST preserve sync semantics under cancellation and timeout.

### DOTNETBL-META: Metadata and Recursive Schema Navigation
1. `DOTNETBL-META-001`: `GetSchema` MUST support catalogs, schemas, tables, columns, indexes, index columns, constraints, PK/FK, privileges, routines, and type-info collections.
2. `DOTNETBL-META-002`: Metadata queries MUST rely on canonical `sys.*` and `information_schema` contracts only.
3. `DOTNETBL-META-003`: Metadata restriction filtering MUST support collection-scoped index mapping, wildcard matching, and explicit `NULL` matching semantics.
4. `DOTNETBL-META-004`: Recursive parent expansion mode MUST preserve schema ancestry without flattening collisions.
5. `DOTNETBL-META-005`: Parent uniqueness and same-name cross-schema object separation MUST be preserved.
6. `DOTNETBL-META-006`: Metadata column naming and shape MUST remain deterministic for tooling compatibility.
7. `DOTNETBL-META-007`: Metadata payload MUST be sufficient for DDL editors and schema explorers.

### DOTNETBL-TYPE: Type, LOB, and Object Families
1. `DOTNETBL-TYPE-001`: Scalar numeric, boolean, textual, temporal, UUID, and bytea families MUST encode/decode deterministically.
2. `DOTNETBL-TYPE-002`: JSON/JSONB, geometry, range, interval, vector, and raw object families MUST be supported.
3. `DOTNETBL-TYPE-003`: Composite/record decode behavior MUST preserve field OIDs and values.
4. `DOTNETBL-TYPE-004`: Array/list parameter handling MUST preserve deterministic literal conversion behavior.
5. `DOTNETBL-TYPE-005`: LOB stream paths (`GetBytes`, `GetChars`, `GetStream`, `GetTextReader`) MUST support large payload round trips.
6. `DOTNETBL-TYPE-006`: Reader type metadata (`GetDataTypeName`, `GetFieldType`) MUST be stable across supported OIDs.
7. `DOTNETBL-TYPE-007`: Unknown-type fallback heuristics MUST remain deterministic and non-lossy when typed decoding is unavailable.
8. `DOTNETBL-TYPE-008`: Callable OUT/inout parameter semantics MUST be provided or explicitly documented with approved compatibility deviation.

### DOTNETBL-ERR: Error Surface and SQLSTATE
1. `DOTNETBL-ERR-001`: Provider exceptions MUST include SQLSTATE plus optional detail/hint.
2. `DOTNETBL-ERR-002`: SQLSTATE exact mappings and class-prefix fallback mappings MUST be deterministic.
3. `DOTNETBL-ERR-003`: Error categories MUST cover auth, connection, data, integrity, transaction, syntax, resource, limit, operator-intervention, system, and internal classes.
4. `DOTNETBL-ERR-004`: Timeout/cancel/network/auth failures MUST preserve stable exception taxonomy for retry policy consumers.

### DOTNETBL-RES: Pooling, Recovery, Observability
1. `DOTNETBL-RES-001`: Pooling MUST support configurable min/max/lifetime with deterministic borrow/return/reject behavior.
2. `DOTNETBL-RES-002`: Pool stats MUST include at least active, idle, borrow-attempts, borrowed, returned, rejected, and evicted counters.
3. `DOTNETBL-RES-003`: Connection recovery MUST handle unhealthy transports with bounded retry/backoff.
4. `DOTNETBL-RES-004`: Runtime control operations (`Ping`, `SetOption`, notification plumbing) MUST be available for operational tooling pathways.
5. `DOTNETBL-RES-005`: Enterprise soak/fault harnesses MUST validate cancellation lifecycle, failover saturation, and isolation/deadlock contention behavior.

## JDBC Traceability
1. `JDBCBL-CONN-*` -> `DOTNETBL-CONN-*`, `DOTNETBL-AUTH-*`
2. `JDBCBL-TXN-*` -> `DOTNETBL-TXN-*`
3. `JDBCBL-EXEC-*` -> `DOTNETBL-EXEC-*`
4. `JDBCBL-META-*` -> `DOTNETBL-META-*`
5. `JDBCBL-TYPE-*` -> `DOTNETBL-TYPE-*`
6. `JDBCBL-ERR-*` -> `DOTNETBL-ERR-*`
7. `JDBCBL-RES-*` -> `DOTNETBL-RES-*`

## Enterprise Gate and Evidence Contract
1. Required section-30 suites: `T30-I*` and `T30-J04`.
2. Enterprise-readiness evidence set MUST include:
- `DOTNET-101`: cancellation/release soak harness evidence.
- `DOTNET-102`: failover/saturation soak harness evidence.
- `DOTNET-103`: isolation/deadlock matrix harness evidence.
- `DOTNET-104`: metadata/LOB/cache lifecycle evidence.
- `JDBC-203`: cross-runtime pooling/recovery contract evidence.
3. Evidence artifacts MUST be reproducible and linked from ticket folders.
4. Release-tier promotion is blocked by any unapproved `PARTIAL` or `MISSING` mandatory requirement.

## Capability Status Snapshot (2026-03-06)

| Requirement family | Status | Notes |
| --- | --- | --- |
| `DOTNETBL-CONN-*` | `MET` | URI + key/value DSN, native protocol normalization, direct/manager proxy, deterministic open/close/reconnect implemented. |
| `DOTNETBL-AUTH-*` | `PARTIAL` | Password/SCRAM, auth policy fields, manager proxy implemented; discovery handshake (`DOTNETBL-AUTH-009`) depends on section-27 server extension. |
| `DOTNETBL-TXN-*` | `MET` | Begin/commit/rollback/savepoint and transaction-state validation implemented. |
| `DOTNETBL-EXEC-*` | `PARTIAL` | Core execution, prepared cache, timeout/cancel and callable normalization implemented; ADO.NET `NextResult` parity requires dedicated completion for `DOTNETBL-EXEC-009`. |
| `DOTNETBL-META-*` | `MET` | Metadata families, restriction filtering, recursive parent expansion, and routine/type-info coverage implemented. |
| `DOTNETBL-TYPE-*` | `PARTIAL` | Broad scalar/object/LOB coverage implemented; explicit callable OUT/inout parity (`DOTNETBL-TYPE-008`) remains open. |
| `DOTNETBL-ERR-*` | `MET` | Spec-complete SQLSTATE mapping and deterministic exception taxonomy implemented. |
| `DOTNETBL-RES-*` | `MET` | Pooling/reconnect counters plus DOTNET-101..104 harness coverage implemented. |

## Workplan Seed (Actionable)
1. `DOTNET-WP-01`: Implement full `DbDataReader.NextResult/NextResultAsync` multi-result traversal parity (`DOTNETBL-EXEC-009`).
2. `DOTNET-WP-02`: Implement callable OUT/inout parameter contract or publish approved deviation model (`DOTNETBL-TYPE-008`).
3. `DOTNET-WP-03`: Add auth capability-discovery handshake support after section-27 server extension lands (`DOTNETBL-AUTH-009`).
4. `DOTNET-WP-04`: Publish externalized enterprise diagnostics surface (pool stats + structured event hooks) for operations tooling.

## Non-Normative Notes
1. This specification is the canonical source for .NET lane baseline and enterprise readiness in section 30.
2. Workplan generation should consume the `Capability Status Snapshot` and `Workplan Seed` sections directly.

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

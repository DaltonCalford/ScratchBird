# JDBC Driver Baseline Implementation Specification

## Purpose
Define the implementation-backed baseline from the current ScratchBird JDBC driver.
All maintained non-JDBC drivers (ODBC, C++, .NET, Go, Rust, and future maintained drivers) MUST meet or exceed this capability floor.

## Scope
- Driver connection/authentication/runtime behavior.
- Statement/prepared/callable execution behavior.
- Metadata and recursive schema navigation behavior.
- Type binding/decoding behavior, including advanced JDBC object families.
- Error, timeout, cancellation, pooling, and resilience behavior.
- Cross-driver conformance and workplan requirements.

## Baseline Inputs
1. Code baseline:
`/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/jdbc/src/main/java/com/scratchbird/jdbc/`
2. Test baseline:
`/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/jdbc/src/test/java/com/scratchbird/jdbc/`
3. V3 dialect alignment:
`/home/dcalford/CliWork/local_work/Update_Parser_Work/scratch_bird_v_3_authorized_dialect_guide.md`
4. Canonical V3 parser contracts:
`docs/specifications/21_V3_Dialect_Surface/`

## Applicability Model
1. Requirement IDs in this document are normative for maintained drivers unless explicitly marked advisory.
2. Non-JDBC drivers MAY expose different API shapes, but MUST preserve equivalent semantics.
3. Drivers MAY exceed this baseline.
4. Known JDBC implementation defects are not normative requirements to replicate.

## Invariants
1. Drivers MUST preserve parser/engine separation and MUST NOT introduce dialect-specific semantics into engine contracts.
2. Recursive schema navigation MUST be derivable from metadata without SQL text parsing or hidden non-metadata queries.
3. Schema tree roots MUST follow `catalog(database) -> schema path segments` and preserve full ancestry.
4. Within a parent namespace, duplicate names in the same object family are forbidden; the same object name in different schema paths is valid.
5. Domain/global object scoping and recursive schema path behavior MUST remain aligned with section-21 V3 dialect contracts.

## Normative Baseline Requirements

### JDBCBL-CONN: Connection, Config, Protocol
1. `JDBCBL-CONN-001`: Driver MUST accept a DSN/URL form equivalent to `jdbc:scratchbird:` with host, port, database/catalog, user, password, and property query support.
2. `JDBCBL-CONN-002`: Property precedence MUST preserve baseline semantics: defaults, explicit properties, endpoint/database path, query parameters; latest explicit override wins.
3. `JDBCBL-CONN-003`: Equivalent configuration fields MUST exist for host/port/protocol/front-door mode/ssl mode/connect timeout/socket timeout/login timeout/current schema/read-only/autocommit.
4. `JDBCBL-CONN-004`: Driver MUST support native-wire startup, authentication negotiation, and capability negotiation.
5. `JDBCBL-CONN-005`: Driver MUST support password auth and SCRAM-SHA-256 auth.
6. `JDBCBL-CONN-006`: Driver MUST support direct mode and manager-proxy front-door mode where configured.
7. `JDBCBL-CONN-007`: Driver MUST preserve connection validation (`isValid` equivalent) and network-timeout controls.
8. `JDBCBL-CONN-008`: Driver MUST transport compatibility/startup fields for `binary_transfer`, `compression`, startup client flags, explicit auth method selection, auth payload/profile variants, required/forbidden auth methods, channel-binding requirement, workload identity, and proxy assertion inputs, or publish an approved lane-specific deviation.
9. `JDBCBL-CONN-009`: When section-27 auth registry negotiation is available, driver MUST support capability-based auth discovery/selection with deterministic legacy fallback behavior.
10. `JDBCBL-CONN-010`: Invalid transport/front-door/auth-policy combinations MUST fail deterministically before session use.

### JDBCBL-TXN: Transaction and Session
1. `JDBCBL-TXN-001`: Driver MUST support autocommit on/off with deterministic transition behavior.
2. `JDBCBL-TXN-002`: Driver MUST support explicit `begin`, `commit`, `rollback`.
3. `JDBCBL-TXN-003`: Driver MUST support savepoint create/release/rollback-to semantics.
4. `JDBCBL-TXN-004`: Driver MUST support session schema mutation equivalent to `setSchema/getSchema`.
5. `JDBCBL-TXN-005`: Driver MUST reject invalid transaction operations deterministically (for example commit/rollback in invalid mode).

### JDBCBL-EXEC: Statement and Query Execution
1. `JDBCBL-EXEC-001`: Driver MUST support simple statements, prepared statements, and callable statements (or equivalent routine-call surface).
2. `JDBCBL-EXEC-002`: Driver MUST support positional bind parameters.
3. `JDBCBL-EXEC-003`: Driver MUST support named parameter aliasing semantics equivalent to JDBC `:name` and `@name` handling.
4. `JDBCBL-EXEC-004`: Driver MUST support batch execution semantics for statement and prepared flows.
5. `JDBCBL-EXEC-005`: Driver MUST support multi-result execution traversal.
6. `JDBCBL-EXEC-006`: Driver MUST support generated-key retrieval semantics.
7. `JDBCBL-EXEC-007`: Driver MUST support cancellation and query timeout enforcement.
8. `JDBCBL-EXEC-008`: Driver MUST support streaming/fetch-size behavior and large-result iteration.
9. `JDBCBL-EXEC-009`: Driver MUST support native SQL conversion entry point (`nativeSQL` equivalent).
10. `JDBCBL-EXEC-010`: Driver MUST support JDBC escape-call normalization behavior for callable flows or provide a semantically equivalent call normalization layer.

### JDBCBL-META: Metadata and Recursive Schema Navigation
1. `JDBCBL-META-001`: Driver MUST expose catalog, schema, table, column, index, key, privilege, routine, and type metadata surfaces equivalent to current JDBC baseline.
2. `JDBCBL-META-002`: Metadata MUST support recursive schema paths and preserve path ancestry.
3. `JDBCBL-META-003`: Metadata MUST support parent-expansion mode equivalent to `metadataExpandSchemaParents`.
4. `JDBCBL-META-004`: Schema tree generation for tooling MUST be metadata-only.
5. `JDBCBL-META-005`: Metadata MUST support default tree shape of `database -> top-level schema branches` (for example `sys`, `users`) when present.
6. `JDBCBL-META-006`: Metadata MUST preserve same-name objects in different schema paths as distinct nodes.
7. `JDBCBL-META-007`: Metadata MUST preserve parent namespace uniqueness rules and avoid flattening collisions.
8. `JDBCBL-META-008`: Metadata MUST provide sufficient DDL editor support fields for tables/views/routines/indexes/constraints/columns.
9. `JDBCBL-META-009`: Metadata capability flags and behavior claims MUST remain consistent with real behavior; any optimistic declarations require explicit compatibility notes.

### JDBCBL-TYPE: Type System and Object Families
1. `JDBCBL-TYPE-001`: Driver MUST support scalar bind/decode families currently covered by JDBC baseline and tests.
2. `JDBCBL-TYPE-002`: Driver MUST support array object handling and conversion.
3. `JDBCBL-TYPE-003`: Driver MUST support blob/clob/nclob object handling and conversion.
4. `JDBCBL-TYPE-004`: Driver MUST support struct/ref/rowid/sqlxml object handling and conversion.
5. `JDBCBL-TYPE-005`: Driver MUST support typed object conversion entry points equivalent to `getObject(Class<T>)`.
6. `JDBCBL-TYPE-006`: Driver MUST support callable OUT parameter registration/read semantics for scalar and object families.
7. `JDBCBL-TYPE-007`: Driver MUST preserve support for JSONB/geometry/range/raw passthrough families where exposed by current baseline.
8. `JDBCBL-TYPE-008`: Driver MUST expose parameter/result metadata sufficient for type-safe client behavior.

### JDBCBL-RES: Resilience, Pooling, Observability
1. `JDBCBL-RES-001`: Driver MUST support connection pooling with configurable bounds and acquire timeout.
2. `JDBCBL-RES-002`: Driver MUST support validation and maintenance behavior for pooled connections.
3. `JDBCBL-RES-003`: Driver MUST support keepalive and leak-detection controls or semantically equivalent lifecycle safeguards.
4. `JDBCBL-RES-004`: Driver MUST support deterministic connection-failure classification and reconnect/retry policy controls.
5. `JDBCBL-RES-005`: Driver MUST support telemetry/metrics collection hooks or equivalent observability interfaces.

### JDBCBL-ERR: Error and SQLSTATE Semantics
1. `JDBCBL-ERR-001`: Driver MUST preserve deterministic SQLSTATE mapping behavior for wire/protocol failures and query failures.
2. `JDBCBL-ERR-002`: Driver MUST preserve error-surface stability across statement, prepared, callable, metadata, and transaction APIs.
3. `JDBCBL-ERR-003`: Driver MUST preserve deterministic timeout and cancellation error mapping.

### JDBCBL-CFG: Baseline Defaults
1. `JDBCBL-CFG-001`: Equivalent defaults MUST be preserved unless the driver spec explicitly documents a stronger default with compatibility justification.
2. `JDBCBL-CFG-002`: The current JDBC defaults form the baseline reference set:
`host=localhost`, `port=3092`, `protocol=native`, `frontDoorMode=direct`, `ssl=require`, `connectTimeout=30`, `socketTimeout=0`, `loginTimeout=30`, `currentSchema=public`, `autoCommit=true`, `readOnly=false`, `metadataExpandSchemaParents=false`, `pooling=true`, `minPoolSize=0`, `maxPoolSize=10`, `acquireTimeout=30`, `prepareThreshold=5`, `binaryTransfer=true`, `compression=off`.

## Evidence Map (JDBC Baseline)
1. Connection/protocol/runtime:
`SBDriver`, `SBConnectionProperties`, `SBConnection`, `SBProtocolHandler`, `SBConnectionPool`, `CircuitBreaker`, `KeepaliveManager`, `LeakDetector`, `QueryPipeline`, `TelemetryCollector`.
2. Metadata and recursive schema behavior:
`SBDatabaseMetaData`, `SBResultSetMetaData`, `SBResultSet`, `SBStatement`, `SBPreparedStatement`, `SBCallableStatement`, `SBSQLParser`.
3. Type/object model:
`SBTypeCodec`, `SBArray`, `SBBlob`, `SBClob`, `SBNClob`, `SBStruct`, `SBRef`, `SBRowId`, `SBSQLXML`, `SBJsonb`, `SBGeometry`, `SBRange`, `SBRawValue`, `SBParameterMetaData`.
4. Test evidence set:
all classes under
`/home/dcalford/CliWork/ScratchBird-driver/tracks/alpha/drivers/jdbc/src/test/java/com/scratchbird/jdbc/`
with primary baseline anchors in `SBIntegrationTest`, `JDBC203PoolingAndRecoveryContractTest`, metadata suites, callable/type suites, protocol mapping suites, and resilience suites.

## Non-Normative JDBC Limitations (Do Not Replicate As Requirements)
1. Metadata and capability over-claims exist in some `supports*` flags.
2. `getProcedureColumns` row-shape inconsistency exists in current implementation.
3. Escape-processing behavior is uneven across statement/prepared/callable pathways.
4. SQL escape parser is brace-oriented and not fully quote/comment aware.
5. Some advanced type families have lighter direct test coverage than core scalar families.
6. Pool-key/session-reset behavior has known caveats.
7. Drivers SHOULD improve these areas while preserving baseline-compatible external semantics.

## Required Deliverables Per Non-JDBC Driver
1. Driver-specific spec mapping every `JDBCBL-*` requirement to concrete API semantics.
2. Driver-specific workplan with gap statuses: `MET`, `PARTIAL`, `MISSING`, `EXCEEDS`.
3. Conformance test matrix that traces each `JDBCBL-*` requirement to automated evidence.
4. Exception register for intentional deviations with compatibility rationale and migration notes.

## Stage-Gate Requirement
1. No maintained driver may be release-tier unless all mandatory `JDBCBL-*` requirements are `MET` or explicitly approved as `EXCEEDS`.
2. `PARTIAL` or `MISSING` items require an approved remediation workplan before release-tier promotion.

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

# V3 Functionality + Documentation Audit (Gate 06)
Last modified: 2026-02-19

## 1. Scope
- Objective:
  - identify missing or partial v3 functionality
  - identify functionality implemented in code but missing/inaccurate in user documentation
- Audit evidence is code-first and gate-artifact-backed.

## 2. Evidence Sources
- Native SQL gate artifacts:
  - `docs/planning/native_sql/gates/NSQL-GATE-06/SYN13_COVERAGE_SUMMARY.env`
  - `docs/planning/native_sql/gates/NSQL-GATE-06/AST_SBLR_BINDING_SUMMARY.env`
  - `docs/planning/native_sql/gates/NSQL-GATE-06/CAPABILITY_MATRIX_SUMMARY.env`
  - `docs/planning/native_sql/gates/NSQL-GATE-06/NATIVE_CAPABILITY_MATRIX.csv`
  - `docs/planning/native_sql/gates/NSQL-GATE-06/unmapped_ids.txt`
- Unmapped feature group catalog:
  - `docs/planning/native_sql/gates/NSQL-GATE-05/unmapped_feature_groups.md`
- Implementation code:
  - `src/parser/parser_v3.cpp`
  - `src/parser/v3_emitter.cpp`
  - `src/sblr/executor.cpp`
- Documentation tree:
  - `docs/user-documentation/language-guide/`
- Backlog checklist:
  - `docs/planning/BETA_0_2_0_SPEC_BACKLOG_2026-02-19.md`

## 3. Functionality Audit Result (Gate 06)
### 3.1 Hard counts
- Total capability rows: `178`
- Mandatory scope rows: `178`
- Mandatory closed: `37`
- Mandatory open: `141`
- Unmapped rows: `141`

Source: `docs/planning/native_sql/gates/NSQL-GATE-06/CAPABILITY_MATRIX_SUMMARY.env`

### 3.2 Severity distribution of mandatory-open rows
- `P0`: `84`
- `P1`: `74`
- `P2`: `20`

Source: `docs/planning/native_sql/gates/NSQL-GATE-06/NATIVE_CAPABILITY_MATRIX.csv`

### 3.3 Mandatory-open by engine (top contributors)
- `MySQL`: `32`
- `PostgreSQL`: `27`
- `FirebirdSQL`: `12`
- `MariaDB`: `9`
- `MongoDB`: `9`
- `Cassandra`: `7`
- `Milvus`: `7`
- `Neo4j`: `7`
- `Redis`: `7`
- `ClickHouse`: `6`
- `DuckDB`: `6`
- `InfluxDB`: `6`
- `OpenSearch`: `6`

Source: `docs/planning/native_sql/gates/NSQL-GATE-06/NATIVE_CAPABILITY_MATRIX.csv`

### 3.4 Mandatory-open by native domain
- `command_surface`: `83`
- `extensibility_surface`: `28`
- `security_surface`: `12`
- `index_vector_search_surface`: `9`
- `datatype_surface`: `3`
- `streaming_replication_surface`: `3`
- `runtime_surface`: `2`
- `connector_surface`: `1`

Source: `docs/planning/native_sql/gates/NSQL-GATE-06/NATIVE_CAPABILITY_MATRIX.csv`

### 3.5 Full gap list location
- Row IDs: `docs/planning/native_sql/gates/NSQL-GATE-06/unmapped_ids.txt`
- Grouped feature names by engine/domain: `docs/planning/native_sql/gates/NSQL-GATE-05/unmapped_feature_groups.md`

## 4. Runtime Partiality Audit (bridge status)
- Executor still rejects large vNext opcode families with deterministic `IRX_0406` semantic-bridge rejection.
- Rejection hub:
  - `src/sblr/executor.cpp:59331`
  - `src/sblr/executor.cpp:59433`
  - `src/sblr/executor.cpp:61383`
- Includes (not exhaustive): doc/ts/search/vector/hybrid opcodes, NoSQL bridge families, cluster/service/cube administrative families, and related extension/security bridge families.

## 5. Documentation Audit Result
### 5.1 Structure and metadata checks
- Markdown files under language guide: `571`
- Files with `Last modified:` metadata: `571`
- Structural population is complete.

### 5.2 Status-marked incompleteness
- Files with `Status: Partial`: `67`
- Files with `Status: Not available`: `96`
- Object READMEs marked `Lifecycle status: Partial command lifecycle`: `28`

### 5.3 Backlog checklist closure status
- `BKL-DOC-001` per-directory checklist remains unchecked (`[ ]` rows), including all scoped language-guide directories.
- Evidence:
  - `docs/planning/BETA_0_2_0_SPEC_BACKLOG_2026-02-19.md:274`

## 6. Implemented-But-Doc-Missing or Stale Items
### 6.1 Stale statements in consolidated language reference
- Docs currently claim non-specialized window emission and missing aggregate DISTINCT parse support:
  - `docs/user-documentation/language-guide/NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md:968`
  - `docs/user-documentation/language-guide/NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md:969`
- Code evidence contradicts this:
  - parser sets `expr->distinct` on function args:
    - `src/parser/parser_v3.cpp:12750`
  - emitter maps dedicated window opcodes for `LAG/LEAD/FIRST_VALUE/LAST_VALUE/NTH_VALUE`:
    - `src/parser/v3_emitter.cpp:4960`
  - executor handles these opcodes:
    - `src/sblr/executor.cpp:43012`
- Test evidence (passing):
  - `ParserV3GapContractsTest.CountDistinctParsesAndBuildsSelectSurface`
  - `ParserV3NoSqlEmitterContractTest.EmitsDedicatedWindowFunctionOpcodes`

### 6.2 DDL lifecycle docs stale for ALTER ROLE / ALTER GROUP
- Docs currently say ALTER is not available:
  - `docs/user-documentation/language-guide/ddl/security/role/README.md:26`
  - `docs/user-documentation/language-guide/ddl/security/group/README.md:26`
- Parser has explicit `ALTER ROLE` / `ALTER GROUP` generic rename/move paths:
  - `src/parser/parser_v3.cpp:7118`
  - `src/parser/parser_v3.cpp:7125`
- Classification: documentation stale (status text should reflect supported generic ALTER subset, not unavailable).

### 6.3 Parser-dispatched command surfaces lacking dedicated modular docs
- Commands with no coverage in modular language-guide files (outside consolidated reference and command-group index):
  - `SECURITY LABEL`
  - `REVOKE TOKEN`
  - `DECLARE EXTERNAL FUNCTION`
  - `INSTALL EXTENSION`
  - `LOAD EXTENSION`
  - `COMPILE UDR`
  - `VALIDATE EMBEDDED SQL`
  - `EVAL LUA`
  - `XGROUP`
  - `XREADGROUP`
  - `XCLAIM`
  - `DOC PATH FILTER`
  - `TS BUCKET AGG`
  - `SEARCH DSL`
  - `VECTOR ANN`
  - `HYBRID BRIDGE`
  - `GRAPH PATH MATCH`
  - `MATCH GRAPH PATH`
  - `CANCEL JOB RUN`
  - `EXECUTE JOB`
- Parser dispatch/function evidence for these surfaces:
  - `src/parser/parser_v3.cpp:655`
  - `src/parser/parser_v3.cpp:699`
  - `src/parser/parser_v3.cpp:708`
  - `src/parser/parser_v3.cpp:730`
  - `src/parser/parser_v3.cpp:813`
  - `src/parser/parser_v3.cpp:831`
  - `src/parser/parser_v3.cpp:930`
  - `src/parser/parser_v3.cpp:16646`
  - `src/parser/parser_v3.cpp:16899`
  - `src/parser/parser_v3.cpp:16941`
  - `src/parser/parser_v3.cpp:17937`
  - `src/parser/parser_v3.cpp:17950`

## 7. Verification Runs Executed During This Audit
- `ParserV3GapContractsTest.CountDistinctParsesAndBuildsSelectSurface` (pass)
- `ParserV3NativeExtensionSurfaceTest.ParsesInstallLoadExtensionSurfaces` (pass)
- `ParserV3NativeExtensionSurfaceTest.ParsesSecurityLabelAndIdentitySwitchSurfaces` (pass)
- `ParserV3NoSqlEmitterContractTest.EmitsDedicatedWindowFunctionOpcodes` (pass)
- `SBLRVNextExecutorDispatchContractTest.KnownVNextOpcodesRejectWithDeterministicBridgeCode` (pass)
- `SBLRVNextExecutorDispatchContractTest.VacuumAliasMapsToSweepGarbageCollection` (pass)

Command:
- `ctest --test-dir build -R "ParserV3GapContractsTest.CountDistinctParsesAndBuildsSelectSurface|ParserV3NoSqlEmitterContractTest.EmitsDedicatedWindowFunctionOpcodes|ParserV3NativeExtensionSurfaceTest.ParsesSecurityLabelAndIdentitySwitchSurfaces|ParserV3NativeExtensionSurfaceTest.ParsesInstallLoadExtensionSurfaces|SBLRVNextExecutorDispatchContractTest.KnownVNextOpcodesRejectWithDeterministicBridgeCode|SBLRVNextExecutorDispatchContractTest.VacuumAliasMapsToSweepGarbageCollection" --output-on-failure`

## 8. Audit Conclusion
- V3 is not yet at "all functions/functionality implemented":
  - `141` mandatory-open capability rows remain.
  - large bridge opcode families are still deterministic `IRX_0406` rejects.
- Documentation is not yet fully synchronized with implementation:
  - stale statements exist (window/distinct and ALTER ROLE/GROUP status).
  - multiple parser-dispatched commands are still missing dedicated modular language-guide documents.

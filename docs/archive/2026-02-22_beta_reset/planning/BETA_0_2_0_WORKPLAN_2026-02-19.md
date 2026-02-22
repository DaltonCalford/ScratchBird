# Beta 0.2.0 Workplan (V3 Full Functionality Closure)
Last modified: 2026-02-19

## 1. Objective

Move from early beta `0.1.0` to hardened beta `0.2.0` by closing all known native parser v3
functionality gaps across:

- parser/AST
- SBLR emission
- executor/runtime semantics
- tests (unit/integration/conformance)
- language documentation coverage labels

This plan is implementation-backed and uses only in-tree artifacts.

## 2. Source Inputs (Authoritative For This Plan)

- `docs/audit/BETA_0_1_0_IMPLEMENTATION_AUDIT_2026-02-19.md`
- `docs/audit/PARSER_V3_MISSING_PARTIAL_MATRIX_BETA_0_1_0.md`
- `docs/audit/V3_PARTIAL_ITEMS_WORKPLAN_INPUT_2026-02-19.md`
- `docs/audit/SYSTEM_DOMAIN_INDEX_CONTEXT_INVENTORY_BETA_0_1_0.md`
- `docs/planning/native_sql/NATIVE_GAP_FEATURE_REGISTRY.json`
- `docs/planning/native_sql/gates/NSQL-GATE-04/NATIVE_CAPABILITY_MATRIX.csv`
- `docs/planning/native_sql/gates/NSQL-GATE-04/ENGINE_SURFACE_PACK_COVERAGE.csv`
- `docs/planning/native_sql/gates/NSQL-GATE-05/V3_MASTER_PARSER_PARITY_BASELINE_2026-02-19.md`

## 3. Baseline Metrics (2026-02-19)

### 3.1 Native Gap Registry

- Total feature rows: `178`
- Priority split:
  - `P0`: `84`
  - `P1`: `74`
  - `P2`: `20`
- Domain split:
  - `command_surface`: `104`
  - `extensibility_surface`: `35`
  - `security_surface`: `15`
  - `index_vector_search_surface`: `10`
  - `datatype_surface`: `5`
  - `streaming_replication_surface`: `5`
  - `runtime_surface`: `3`
  - `connector_surface`: `1`

### 3.2 Gate-04 Coverage Snapshot

- Total rows: `178`
- Mapped rows: `37`
- Unmapped rows: `141`
- Mandatory scope rows: `37`
- Out-of-scope rows: `141`
- Mandatory misses: `0`
- Binding pass rows: `37`
- Binding fail rows: `0`

### 3.3 Gate-05 Mandatory-Scope Snapshot

- Total rows: `178`
- Mapped rows: `37`
- Unmapped rows: `141`
- Mandatory scope rows: `178`
- Mandatory closed rows: `37`
- Mandatory open rows: `141`
- Out-of-scope rows: `0`

Generated from:

- `tools/compliance/native_sql_gate05_scope_promotion.sh`
- `docs/planning/native_sql/gates/NSQL-GATE-05/SYN13_COVERAGE_SUMMARY.env`
- `docs/planning/native_sql/gates/NSQL-GATE-05/CAPABILITY_MATRIX_SUMMARY.env`

### 3.4 Engine Pack Unmapped Counts

- PostgreSQL: `27` unmapped (`38 total`, `11 mapped`)
- MySQL: `32` unmapped (`32 total`, `0 mapped`)
- FirebirdSQL: `12` unmapped (`26 total`, `14 mapped`)
- MongoDB: `9` unmapped (`9 total`, `0 mapped`)
- MariaDB: `9` unmapped (`9 total`, `0 mapped`)
- Cassandra: `7` unmapped (`9 total`, `2 mapped`)
- Redis: `7` unmapped (`9 total`, `2 mapped`)
- Milvus: `7` unmapped (`8 total`, `1 mapped`)
- Neo4j: `7` unmapped (`8 total`, `1 mapped`)
- ClickHouse: `6` unmapped (`8 total`, `2 mapped`)
- DuckDB: `6` unmapped (`8 total`, `2 mapped`)
- OpenSearch: `6` unmapped (`8 total`, `2 mapped`)
- InfluxDB: `6` unmapped (`6 total`, `0 mapped`)

## 4. Non-Negotiable Closure Rules

1. Every feature closure must include all five layers:
   - syntax
   - AST
   - SBLR opcode/payload
   - runtime semantics
   - tests
2. No shipping command family may rely on runtime `IRX_0406` semantic-bridge rejection.
3. No language-guide lifecycle may remain `Partial` for `0.2.0` in-scope objects.
4. Any partial/planned feature must have:
   - spec contract
   - implementation workpack
   - acceptance tests
5. Every merged workpack requires clean build + relevant focused tests; phase exits require
   full clean build + full test suite.
6. Master parser parity rule: no capability remains engine-only; every registry row must be in
   v3 scope (`mandatory_scope_rows == 178` and `out_of_scope_registry_row == 0`).
7. Every parser/emitter/executor change in `0.2.0` scope must land with matching updates under
   `docs/user-documentation/language-guide/` for the affected command/object lifecycle.

## 5. Workstreams

### WS-01: Immediate Parser/Emitter/Executor Hard Gaps

Close code-backed high-impact gaps already identified in audit matrices:

- `CREATE TYPE` runtime closure (`SBLR3_CREATE_TYPE` parser/emitter/runtime alignment)
- `CREATE DATABASE EMULATED` payload/opcode closure and full contract propagation
- `ALTER SEARCH INDEX` and `ALTER VECTOR INDEX` beyond `REBUILD`-only surface
- `REVOKE` privilege parity with `GRANT`
- `SET PARSER VERSION` explicit compatibility policy (implement or remove surface)
- `SET TERM` delimiter-switching runtime workflow integration (not just parser acceptance)
- subquery-membership closure for `IN (subquery)` and `NOT IN (subquery)`
- wide numeric operator closure (`DIV`, modulo, bitwise families for wide numerics)
- implicit comparison coercion matrix closure
- full value-window semantics (`LAG`, `LEAD`, `FIRST_VALUE`, `LAST_VALUE`, `NTH_VALUE`)

Exit criteria:

- deterministic parser/emitter contracts
- runtime semantic tests for each closed gap
- language guide updated from partial claims to closed behavior

### WS-02: DDL Lifecycle Closure Packs

Complete `CREATE + ALTER + SHOW/DESCRIBE + DROP` closure for partial families:

- management: `TABLESPACE`
- data storage: `TYPE`, `SEARCH INDEX`, `VECTOR INDEX`, `MEASUREMENT`,
  `ACCESS METHOD`, `STATISTICS`, `TRANSFORM`
- programmability: `EXCEPTION`, `UDR`
- security: `USER`, `ROLE`, `GROUP`, `POLICY`, `TOKEN`, `QUOTA PROFILE`,
  `CONNECTION RULE`
- integration: `REPLICATION CHANNEL`, `PUBLICATION`, `SUBSCRIPTION`, `CDC TABLE`,
  `DATABASE CONNECTION`, `FOREIGN DATA WRAPPER`, `FOREIGN SERVER`, `FOREIGN TABLE`,
  `USER MAPPING`, `SYNONYM`, `EXTENSION`

Exit criteria:

- lifecycle completeness matrix shows `Y` for all in-scope phases
- each family has positive integration tests for full lifecycle

### WS-03: Admin/Operations Runtime Closure

Remove bridge-partial runtime for admin operations:

- `BACKUP` / `RESTORE`
- `VALIDATE` / `ANALYZE`
- `SWEEP` / `VACUUM` extended maintenance semantics
- `RESYNC REPLICATION CHANNEL`
- cluster control commands
- service channel commands

Exit criteria:

- no admin family returns `IRX_0406` in normal runtime path
- operation success/failure contracts are deterministic and tested

### WS-04: NoSQL Family Runtime Closure

Implement runtime semantics for all currently emitted bridge families:

- CQL
- Mongo
- Cypher
- Redis
- Milvus

Exit criteria:

- each family has positive execution integration tests
- each family has deterministic rejection/error tests for unsupported variants

### WS-05: Cube/Cluster Semantic Bridge Closure

Close runtime for:

- `CREATE/ALTER/DROP CUBE`
- `REFRESH CUBE`
- `SHOW CUBE STATS`
- cluster workload/admission command families

Exit criteria:

- cube and cluster families execute via explicit handlers
- telemetry reflects handled opcode outcomes (not bridge rejection)

### WS-06: External Connection and Replication Canonicalization

Normalize external integration object model:

- dedicated lifecycle closure for `DATABASE CONNECTION`
- runtime closure for foreign server/table/user mapping command families
- one-way and bidirectional replication behavior validation
- CDC lifecycle closure with transaction-id + row-uuid tracking assertions

Exit criteria:

- dedicated canonical object paths across parser/emitter/runtime
- full integration tests for create/alter/show/drop and runtime use

### WS-07: Domain, Index, and Context Variable Closure

Use `SYSTEM_DOMAIN_INDEX_CONTEXT_INVENTORY_BETA_0_1_0.md` as audit baseline:

- system domain catalog coverage and lifecycle docs/tests
- index method surface normalization and runtime capability checks by type
- context variable semantics (`CURRENT_*`, `NOW`, transaction/session context)
- trigger context (`NEW.*`, `OLD.*`) contract coverage

Exit criteria:

- domain/index/context docs are complete and test-backed
- no unresolved catalog/domain/index mapping ambiguity remains

### WS-08: Engine Parity Workpacks (All Registry Rows)

Convert all `178` registry rows into implementation workpacks in dependency order:

- P0 pack: all `84` rows
- P1 pack: all `74` rows
- P2 pack: all `20` rows

Execution policy:

- row-level workpack ID: `gap_item_id`
- each row produces:
  - syntax contract test
  - AST/SBLR binding test
  - runtime semantic test or deterministic rejection contract
  - language-guide update for affected surface
- NSQL-GATE-05 inventory is the blocking closure list for all currently engine-only features.

Exit criteria:

- `mandatory_scope_rows = 178`
- `out_of_scope_registry_row = 0`
- `unmapped_rows = 0`
- row-level closure evidence in gate artifacts

### WS-09: Validation, Performance, and Release Gates

- full clean build and full test suite at each phase gate
- driver compatibility reruns after parser/catalog normalization work
- benchmark runs on identical hardware/OS against emulated engines
- go/no-go decision package with redesign trigger when thresholds fail

Exit criteria:

- release gate package includes test, performance, and compatibility evidence
- explicit go/no-go decision signed in planning docs

### WS-10: Language Reference Full Refresh

Scope: full tree under `docs/user-documentation/language-guide/`.

- update every new/modified SQL surface with command/object lifecycle docs
  (`CREATE`, `ALTER`, `SHOW`/`DESCRIBE`, `DROP` where applicable)
- close missing detail coverage for operators, functions, casts/coercions, domains, index methods,
  context variables, and admin/NoSQL command groups
- keep navigation integrity (`README.md` indices, parent/back links, next-step links)
- ensure each document has current `Last modified` metadata

Exit criteria:

- no in-scope parser/runtime change lacks language-guide coverage
- language-guide lifecycle status for in-scope objects is not `Partial`
- coverage audit confirms all changed SQL objects/commands link to updated docs

## 5.1 Current Execution Snapshot (2026-02-19)

- Validation gate automated and executed:
  - `tools/compliance/run_beta_gate_001.sh`
  - `docs/planning/gates/BETA-GATE-001/BETA_GATE_001_MANIFEST_20260219T160318Z.md`
  - `100% tests passed, 0 tests failed out of 3390`
- Release packaging automated and refreshed:
  - `tools/compliance/package_beta_release.sh`
  - `release/BETA_RELEASE_MANIFEST_20260219.md`
  - `release/scratchbird-beta-20260219-full.tar.gz`

## 6. Phase Plan

### Phase 1: Planning Freeze and P0 Workpack Carving

- freeze full workpack inventory from registry + partial matrices
- assign priority and dependencies
- define acceptance for every P0 gap

### Phase 2: Core Runtime and Lifecycle Closure

- execute WS-01, WS-02, WS-03
- run full clean build and full suite before phase close

### Phase 3: NoSQL/Cube/Cluster + Integration Closure

- execute WS-04, WS-05, WS-06
- eliminate runtime bridge-partial behavior for in-scope families

### Phase 4: Domain/Index/Context + Parity Expansion

- execute WS-07 and WS-08
- close remaining parity rows in registry scope

### Phase 5: Final Validation and Release Decision

- execute WS-09 and WS-10
- issue 0.2.0 go/no-go package

## 7. 0.2.0 Exit Gates

1. Parser/AST/SBLR/runtime closure is complete for all in-scope v3 command families.
2. No in-scope runtime path relies on deterministic bridge rejection (`IRX_0406`).
3. Language guide has no `Partial` for in-scope object lifecycles.
4. Feature registry workpack status is fully closed for `0.2.0` target scope.
5. Full clean build and full test suite pass.
6. Driver compatibility and benchmark gates pass, or redesign is explicitly approved.
7. `docs/user-documentation/language-guide/` is fully updated for all `0.2.0` new/modified surface.

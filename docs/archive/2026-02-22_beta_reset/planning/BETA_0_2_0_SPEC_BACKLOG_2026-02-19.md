# Beta 0.2.0 Spec and Implementation Backlog (V3)
Last modified: 2026-02-19

## 1. Backlog Policy

This backlog is the execution companion to:

- `docs/planning/BETA_0_2_0_WORKPLAN_2026-02-19.md`
- `docs/audit/PARSER_V3_MISSING_PARTIAL_MATRIX_BETA_0_1_0.md`
- `docs/audit/V3_PARTIAL_ITEMS_WORKPLAN_INPUT_2026-02-19.md`
- `docs/planning/native_sql/NATIVE_GAP_FEATURE_REGISTRY.json`
- `docs/planning/native_sql/gates/NSQL-GATE-05/V3_MASTER_PARSER_PARITY_BASELINE_2026-02-19.md`

Required for every backlog item:

1. specification contract update
2. parser/AST contract test
3. emitter/SBLR contract test
4. executor/runtime semantic test
5. language guide update (coverage label/lifecycle status; full file refresh where surface changed)

Status model:

- `OPEN`: not started
- `IN_PROGRESS`: implementation active
- `BLOCKED`: dependency not closed
- `DONE`: merged with full evidence

## 2. Immediate V3 Hard-Gap Backlog (Code-Backed)

### BKL-V3-001 (OPEN)

- Title: `CREATE TYPE` full runtime closure
- Source: parser/emitter/runtime mismatch in `PARSER_V3_MISSING_PARTIAL_MATRIX_BETA_0_1_0.md`
- Acceptance:
  - dedicated runtime execution path exists
  - lifecycle tests pass (`create/alter/show/drop`)

### BKL-V3-002 (OPEN)

- Title: `CREATE DATABASE EMULATED` payload and opcode canonicalization
- Acceptance:
  - parser contract fields propagated fully to emitted payload
  - runtime path consumes full contract deterministically

### BKL-V3-003 (OPEN)

- Title: Search/vector index ALTER closure beyond `REBUILD`
- Acceptance:
  - parser accepts full intended alter action set
  - runtime behavior implemented and tested

### BKL-V3-004 (OPEN)

- Title: `REVOKE` privilege parity with `GRANT`
- Acceptance:
  - privilege token matrix aligned
  - parity contract tests pass

### BKL-V3-005 (OPEN)

- Title: `SET PARSER VERSION` policy closure
- Acceptance:
  - command implemented or removed from accepted surface
  - deterministic contract behavior documented and tested

### BKL-V3-006 (OPEN)

- Title: `SET TERM` delimiter workflow closure
- Acceptance:
  - script delimiter behavior is integrated and testable end-to-end

### BKL-V3-007 (OPEN)

- Title: subquery-membership operator closure (`IN/NOT IN (subquery)`)
- Acceptance:
  - executor routing implemented
  - positive + negative tests pass

### BKL-V3-008 (OPEN)

- Title: wide numeric operator closure (`DIV`, modulo, bitwise families)
- Acceptance:
  - wide numeric operand matrix implemented/documented
  - deterministic overflow/error behavior tested

### BKL-V3-009 (OPEN)

- Title: implicit comparison coercion gap closure
- Acceptance:
  - unsupported type combinations resolved or explicitly rejected
  - strict-mode behavior remains deterministic

### BKL-V3-010 (OPEN)

- Title: full value-window semantics closure
- Acceptance:
  - `LAG/LEAD/FIRST_VALUE/LAST_VALUE/NTH_VALUE` semantics are frame/partition-correct
  - conformance tests added

## 3. Lifecycle Completion Packs (Language Guide Partial Families)

### BKL-LCY-001 (OPEN)

- Title: management/data-storage lifecycle closure pack
- Scope:
  - `TABLESPACE`, `TYPE`, `SEARCH INDEX`, `VECTOR INDEX`, `MEASUREMENT`,
    `ACCESS METHOD`, `STATISTICS`, `TRANSFORM`
- Acceptance:
  - no lifecycle phase remains `Partial` for in-scope objects

### BKL-LCY-002 (OPEN)

- Title: programmability/security lifecycle closure pack
- Scope:
  - `EXCEPTION`, `UDR`, `USER`, `ROLE`, `GROUP`, `POLICY`, `TOKEN`,
    `QUOTA PROFILE`, `CONNECTION RULE`
- Acceptance:
  - full create/alter/show/describe/drop closure and tests

### BKL-LCY-003 (OPEN)

- Title: integration lifecycle closure pack
- Scope:
  - `REPLICATION CHANNEL`, `PUBLICATION`, `SUBSCRIPTION`, `CDC TABLE`,
    `DATABASE CONNECTION`, `FOREIGN DATA WRAPPER`, `FOREIGN SERVER`,
    `FOREIGN TABLE`, `USER MAPPING`, `SYNONYM`, `EXTENSION`
- Acceptance:
  - full lifecycle closure with deterministic runtime behavior

## 4. Runtime Semantic Bridge Closure Packs

### BKL-RT-001 (OPEN)

- Title: admin operations runtime closure
- Scope:
  - `BACKUP`, `RESTORE`, `VALIDATE`, `ANALYZE`, `RESYNC REPLICATION CHANNEL`
- Acceptance:
  - no `IRX_0406` for in-scope admin operations

### BKL-RT-002 (OPEN)

- Title: cluster/service runtime closure
- Scope:
  - cluster workload/admission commands
  - service channel commands
- Acceptance:
  - explicit handlers + integration tests

### BKL-RT-003 (OPEN)

- Title: cube runtime closure
- Scope:
  - `CREATE/ALTER/DROP CUBE`, `REFRESH CUBE`, `SHOW CUBE STATS`
- Acceptance:
  - explicit runtime handlers and positive test suite

### BKL-RT-004 (OPEN)

- Title: NoSQL runtime closure pack
- Scope:
  - CQL, Mongo, Cypher, Redis, Milvus command families
- Acceptance:
  - positive execution contracts and deterministic reject contracts

## 5. Domain/Index/Context Backlog

### BKL-DIC-001 (OPEN)

- Title: system domain lifecycle and mapping closure
- Source: `SYSTEM_DOMAIN_INDEX_CONTEXT_INVENTORY_BETA_0_1_0.md`
- Acceptance:
  - domain-family docs + tests reflect runtime implementation exactly

### BKL-DIC-002 (OPEN)

- Title: index method capability closure
- Scope:
  - parser index aliases, canonical index names, runtime capability enforcement
- Acceptance:
  - unsupported method/type combinations are deterministic and tested

### BKL-DIC-003 (OPEN)

- Title: context variable + trigger context closure
- Scope:
  - `CURRENT_*`, `NOW`, transaction/session context, `NEW.*`, `OLD.*`
- Acceptance:
  - semantic contracts validated across statements and trigger runtime

## 6. Engine Parity Backlog Packs (Registry-Driven)

All rows in `NATIVE_GAP_FEATURE_REGISTRY.json` are backlog scope for v3 capability parity.

### BKL-ENG-000 (OPEN)

- Title: promote all registry rows into mandatory v3 scope
- Baseline:
  - historical (gate-04): mandatory scope `37`, out-of-scope `141`
  - current (gate-05 promoted): mandatory scope `178`, out-of-scope `0`, mandatory open `141`
- Acceptance:
  - `mandatory_scope_rows == 178`
  - `out_of_scope_registry_row == 0`
  - gate report updated with deterministic evidence

### BKL-ENG-P0 (OPEN)

- Title: priority P0 parity closure
- Scope: `84` rows
- Acceptance:
  - no P0 row remains unmapped/incomplete for `0.2.0` target scope

### BKL-ENG-P1 (OPEN)

- Title: priority P1 parity closure
- Scope: `74` rows
- Acceptance:
  - no P1 row remains unmapped/incomplete for `0.2.0` target scope

### BKL-ENG-P2 (OPEN)

- Title: priority P2 parity closure
- Scope: `20` rows
- Acceptance:
  - no P2 row remains unmapped/incomplete for `0.2.0`

### BKL-ENG-PACKS (OPEN)

- Title: engine-pack closure tracking
- Scope:
  - PostgreSQL (`27` unmapped)
  - MySQL (`32` unmapped)
  - FirebirdSQL (`12` unmapped)
  - MongoDB (`9` unmapped)
  - MariaDB (`9` unmapped)
  - Cassandra (`7` unmapped)
  - Redis (`7` unmapped)
  - Milvus (`7` unmapped)
  - Neo4j (`7` unmapped)
  - ClickHouse (`6` unmapped)
  - DuckDB (`6` unmapped)
  - OpenSearch (`6` unmapped)
  - InfluxDB (`6` unmapped)
- Acceptance:
  - per-engine closure report with tests and doc updates

### BKL-ENG-INV-NSQL05 (OPEN)

- Title: close NSQL-GATE-05 unmapped feature inventory
- Scope:
  - all `141` rows in
    `docs/planning/native_sql/gates/NSQL-GATE-05/V3_MASTER_PARSER_PARITY_BASELINE_2026-02-19.md`
- Acceptance:
  - inventory rows moved from unmapped to mapped/closed status
  - no engine retains non-zero unmapped count

## 7. Language Reference Backlog

### BKL-DOC-001 (OPEN)

- Title: full language-reference refresh for all changed v3 surface
- Scope:
  - `docs/user-documentation/language-guide/` full tree
  - all command/object docs touched by parser/emitter/executor changes
- Per-directory command/doc closure checklist (all folders under language-guide):

<!-- AUTO-GENERATED:BKL-DOC-001-CHECKLIST:START -->
Regeneration command:
`tools/compliance/generate_bkl_doc_001_checklist.sh`
Cycle scope source:
`docs/planning/BETA_0_2_0_DOC_CYCLE_SCOPE.tsv`
Cycle scope semantics: if a parent directory is listed in the scope file, all descendants are marked `YES`.

| Done | Parser/Emitter/Executor touched in this cycle | Directory | Command/doc files to close | README |
| --- | --- | --- | --- | --- |
| [ ] | `NO` | `docs/user-documentation/language-guide/` | `NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md, TODO_BETA_0_2_0.md, index.md, python-to-psql-migration.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/cluster-and-service/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/cluster-and-service/cluster-control/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/cluster-and-service/service-channel/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/connectivity-and-operations/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/connectivity-and-operations/backup-restore/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/connectivity-and-operations/connect-disconnect/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/connectivity-and-operations/resync-replication/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/connectivity-and-operations/sweep-vacuum/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/connectivity-and-operations/validate-analyze-maintenance/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/emulation/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/emulation/create-database-emulated/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/nosql-bridges/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/nosql-bridges/cql-family/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/nosql-bridges/cypher-family/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/nosql-bridges/milvus-family/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/nosql-bridges/mongo-family/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/nosql-bridges/redis-family/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/session-and-schema/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/session-and-schema/set-role-and-auth/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/session-and-schema/set-schema-and-path/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/session-and-schema/set-timezone-dialect/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/admin/session-and-schema/show-session-controls/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/command-groups/` | `alter.md, create.md, drop.md, select.md, set.md, show.md, utility-and-bridge.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/data-types/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/data-types/casts/` | `explicit-casts.md, implicit-casts.md, strict-mode.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/data-types/context/` | `context-variables.md, trigger-context.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/data-types/domains/` | `custom-domain-lifecycle.md, default-domain-bindings.md, system-domain-families.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/data-types/families/` | `advanced-container.md, json-vector-search.md, network-geo-range.md, numeric.md, temporal.md, text-and-binary.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/data-types/index-methods/` | `canonical-methods-and-aliases.md, parser-accepted-methods.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/data-types/operators/` | `arithmetic-comparison.md, logical-bitwise.md, pattern-json-array.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/cluster-and-service/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/cluster-and-service/cluster-admission-binding/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/cluster-and-service/cluster-admission-policy/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/cluster-and-service/cluster-workload-class/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/cluster-and-service/cluster-workload-route/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/cluster-and-service/cube/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/access-method/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/domain/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/index/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/measurement/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/search-index/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/sequence-generator/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/statistics-object/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/table/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/transform/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/type/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/vector-index/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/data-storage/view/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/cdc-table/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/database-connection/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/extension/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/foreign-data-wrapper/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/foreign-server/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/foreign-table/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/publication/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/replication-channel/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/subscription/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/synonym/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/integration/user-mapping/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/management/` | `object-naming-and-identifiers.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/management/database/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/management/schema/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/management/tablespace/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/programmability/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/programmability/exception/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/programmability/function/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/programmability/package/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/programmability/procedure/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/programmability/trigger/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/programmability/udr/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/security/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/security/connection-rule/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/security/group/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/security/policy/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/security/quota-profile/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/security/role/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/security/token/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/ddl/security/user/` | `alter.md, create.md, describe.md, drop.md, show.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/bulk/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/bulk/copy/` | `clauses.md, errors.md, examples.md, runtime.md, statement.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/mutation/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/mutation/delete/` | `clauses.md, errors.md, examples.md, runtime.md, statement.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/mutation/insert/` | `clauses.md, errors.md, examples.md, runtime.md, statement.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/mutation/update/` | `clauses.md, errors.md, examples.md, runtime.md, statement.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/mutation/update-or-insert/` | `clauses.md, errors.md, examples.md, runtime.md, statement.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/query/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/query/select/` | `clauses.md, errors.md, examples.md, runtime.md, statement.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/reconciliation/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/dml/reconciliation/merge/` | `clauses.md, errors.md, examples.md, runtime.md, statement.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/functions/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/functions/aggregate/` | `core-aggregates.md, statistical-and-regression.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/functions/scalar/` | `context-and-temporal.md, math-and-trig.md, string-json-conversion.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/functions/window/` | `ranking.md, value-window-functions.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/context/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/context/context-variables/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/context/trigger-old-new/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/control-flow/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/control-flow/case/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/control-flow/for-loop/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/control-flow/if-elsif-else/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/control-flow/loop-leave-continue/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/control-flow/while-loop/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/error-handling/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/error-handling/create-exception/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/error-handling/exception-block/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/error-handling/try-block/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/error-handling/when-block/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/routine-structure/` | `-` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/routine-structure/declare-section/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/routine-structure/dollar-quoted-bodies/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/routine-structure/procedure-block/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
| [ ] | `YES` | `docs/user-documentation/language-guide/psql/routine-structure/set-term/` | `errors.md, examples.md, runtime.md, semantics.md, syntax.md` | `README.md` |
<!-- AUTO-GENERATED:BKL-DOC-001-CHECKLIST:END -->

- Acceptance:
  - per-directory checklist rows are updated as command/doc closure progresses
  - no changed SQL object/command is missing a corresponding language-guide update
  - each updated file contains current `Last modified` metadata

### BKL-DOC-002 (OPEN)

- Title: lifecycle and navigation integrity in language guide
- Scope:
  - lifecycle documents (`create/alter/show-describe/drop`)
  - directory `README.md` parent/child/back links and next-step links
- Acceptance:
  - lifecycle chains are complete for in-scope objects
  - all navigation links resolve within repository tree

### BKL-DOC-003 (OPEN)

- Title: operators/functions/casts/domains/index/context completeness audit
- Scope:
  - operators and precedence groups
  - implicit/explicit casts and conversion rules
  - scalar/aggregate/window function families
  - system domains, index methods, context variables (`CURRENT_*`, `NOW`, `NEW.*`, `OLD.*`)
- Acceptance:
  - coverage matrix maps each implemented surface to documentation location
  - missing/partial items are tracked as explicit backlog entries

## 8. Validation and Gate Backlog

### BKL-GATE-001 (DONE)

- Title: clean build and full suite gate automation
- Evidence:
  - `tools/compliance/run_beta_gate_001.sh`
  - `docs/planning/gates/BETA-GATE-001/BETA_GATE_001_MANIFEST_20260219T160318Z.md`
  - `docs/planning/gates/BETA-GATE-001/BETA_GATE_001_SUMMARY_20260219T160318Z.env`
- Acceptance:
  - repeatable clean-build script + full test execution manifest

### BKL-GATE-002 (OPEN)

- Title: post-normalization driver revalidation
- Acceptance:
  - driver matrix rerun after parser/catalog changes

### BKL-GATE-003 (OPEN)

- Title: benchmark parity and go/no-go package
- Acceptance:
  - identical hardware/OS benchmark reports
  - documented go/no-go decision

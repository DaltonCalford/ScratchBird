# Parser Conformance Corpus Index

## Purpose
Define the canonical conformance corpus structure and required coverage so parser behavior is provably deterministic across native and all emulated targets.

## Parser Targets
- `native`
- `firebird`
- `postgresql`
- `mysql`
- `cassandra`
- `mongodb`
- `neo4j`
- `redis`
- `milvus`

## Corpus Root Contract
Corpus root is fixed at `tests/conformance/parser/` in the ScratchBird repository and must follow this layout:

```text
corpus/
  <target>/
    manifest.json
    feature_groups/
      <feature_key>.yaml
    negative/
      <feature_key>__negative.yaml
    wire/
      <wire_case_id>.bin
    expected/
      <case_id>.expected.json
```

## Manifest Schema (`manifest.json`)
Required keys:
- `target`
- `profile_id`
- `profile_version`
- `case_count_total`
- `feature_coverage`
- `negative_case_count`
- `wire_case_count`
- `result_shape_coverage`
- `checksum`

## Test Case Schema (`*.yaml`)
Required fields:
- `case_id`
- `feature_key`
- `family_key`
- `input_kind` (`sql`, `command`, `wire`)
- `input_payload`
- `expected_decision`
- `expected_transform_ids`
- `expected_feature_key`
- `expected_result_shape_id`
- `expected_error_code` (nullable for success)
- `expected_output_checksum`
- `seed_catalog_state`
- `seed_profile_state`

## Expected Output Schema (`*.expected.json`)
Required fields:
- `case_id`
- `decision`
- `feature_key`
- `family_key`
- `canonical_ast_checksum`
- `sblr_checksum` (nullable for reject)
- `result_shape_id`
- `error_code` (nullable)
- `dialect_error_code` (nullable)
- `sqlstate_or_equivalent` (nullable)

## Coverage Requirements

| target | minimum_total_cases | minimum_negative_cases | minimum_wire_cases | required_groups |
| --- | ---: | ---: | ---: | --- |
| native | 1200 | 250 | 120 | all family groups |
| firebird | 700 | 180 | 120 | txn, session, ddl_sql, dml_sql, prepared, notification, admin, service_channel |
| postgresql | 900 | 220 | 140 | txn, session, ddl_sql, dml_sql, prepared, notification, admin, fdw, service_channel |
| mysql | 850 | 210 | 130 | txn, session, ddl_sql, dml_sql, prepared, admin, service_channel |
| cassandra | 650 | 170 | 120 | session, ddl_sql, dml_sql, cql, admin, service_channel |
| mongodb | 700 | 180 | 130 | session, mongo, dml_sql(mapped), admin, service_channel |
| neo4j | 650 | 170 | 120 | session, cypher, admin, service_channel |
| redis | 700 | 180 | 130 | session, redis, notification, admin, service_channel |
| milvus | 700 | 180 | 130 | session, milvus, admin, service_channel |

## Required Feature Coverage
Each target corpus must include at least one success and one failure case for each applicable feature key from:
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_SUPERSET_COMPATIBILITY_MATRIX.md`

If a feature is not applicable to target, corpus must include a deterministic reject case with:
- `expected_decision: REJECT`
- `expected_error_code: UNSUPPORTED_IN_DIALECT`

## Determinism Rules
1. Same `input_payload`, same `seed_catalog_state`, and same `seed_profile_state` must yield byte-identical:
   - `canonical_ast_checksum`
   - `sblr_checksum` (for success)
   - `expected_output_checksum`
2. Any non-deterministic checksum change is gate failure.

## Negative Corpus Requirements
Negative corpus must include:
- malformed tokens
- malformed wire frames
- unknown feature key
- missing capability entry
- disabled dialect
- disabled feature
- name-binding failures
- result-shape mapping failures

## Wire Corpus Requirements
Wire corpus must include:
- startup and auth envelope cases
- prepared lifecycle frames
- streaming frames (copy/events/pubsub/vector streams when applicable)
- cancel and timeout scenarios
- malformed frame boundary cases

## Per-Target Required Feature Families

### `native`
- All feature groups.

### `firebird`
- SQL DDL/DML parity families, package/procedure/function/trigger/exception pathways, events and service operations.

### `postgresql`
- SQL DDL/DML parity families, copy/listen-notify/prepare-deallocate/fdw/publication-subscription pathways.

### `mysql`
- SQL DDL/DML parity families, event/procedure/function/trigger/user-grant pathways, prepared and XA mappings.

### `cassandra`
- keyspace/table/type/index/materialized view/batch/ttl/writetime pathways.

### `mongodb`
- find/aggregate/findAndModify/bulkWrite/user-role-session pathways.

### `neo4j`
- match/merge/unwind/call/admin privilege pathways.

### `redis`
- string/hash/list/set/zset/stream/pubsub/acl/function/script/cluster command pathways.

### `milvus`
- create/drop collection/index, insert/delete/search/query, load/release, rbac/database pathways.

## Gate Criteria
1. 100% feature-key coverage for applicable keys.
2. 100% expected decision match.
3. 100% result-shape ID match.
4. 100% deterministic checksum stability across repeated runs.
5. No unresolved case with `PROFILE_ENTRY_MISSING` in released profile versions.

## Reporting Output
Conformance run must generate:
- `summary.json`
- `per_feature_results.csv`
- `nondeterminism_failures.json`
- `dialect_error_mapping_coverage.csv`

## Change Control
Any update to:
- feature matrix
- capability profile schema
- parser profile version

requires corpus version bump and full rerun for affected targets.

## Audit normalization note (2026-03-28)
- Current code-backed parser authority is bounded to the native V3 stack (`parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`) plus dedicated shipped emulated SQL-family parser code for Firebird, PostgreSQL, and MySQL.
- Dedicated parser-agent and listener proof currently exists only for `sb_parser_fb`, `sb_parser_pg`, `sb_parser_mysql`, and the matching listener front doors; universal nine-family dedicated parser parity is not current implementation proof.
- Builtin emulation package scaffold proof is currently limited to `firebirdsql`, `postgresql`, and `mysql`.
- Cassandra, MongoDB, Neo4j, Redis, and Milvus are currently represented in this section by native-V3 feature vocabulary, catalog/runtime vocabulary, or checklist material rather than shipped dedicated parser implementations.
- Broad section-wide parity, corpus cardinality, and universal profile-generation claims are therefore bounded and must not be treated as present-day implementation proof without family-local source evidence.

## Hardening promotion note (2026-03-28)
- section `28` now carries explicit capability-state vocabulary for parser implementation proof lanes:
  - `supported_native_v3`
  - `supported_emulated_sql_family`
  - `supported_scaffold_or_udr_boundary`
  - `bounded_shipped_front_door`
  - `checklist_only`
  - `target_state_only`
  - `fail_closed`
- dedicated parser-family proof must be anchored to live parser code plus shipped parser-agent or listener/runtime seams, not to checklist presence alone
- native-V3 internal feature vocabulary must not be promoted into dedicated external parser-family parity without family-local source proof
- universal capability-profile generation, universal corpus cardinality, and universal wire parity claims remain non-authoritative unless backed by generated or runtime evidence

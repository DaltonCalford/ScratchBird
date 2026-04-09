# Capability Profile Build Algorithm (Canonical)

## Purpose
Define the exact deterministic algorithm for generating parser capability decisions for all feature keys and parser targets.

## Inputs
1. Feature matrix rows from:
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_SUPERSET_COMPATIBILITY_MATRIX.md`
2. Family mapping rules from:
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SBLR_OPCODE_FAMILIES_AND_SYMBOLS.md`
3. Explicit remap overrides from:
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_ENTRIES_CANONICAL.md`

## Output Cardinality
- Parser targets: `9`
- Current feature keys: `214`
- Required generated capability rows per profile version: `9 * 214 = 1926`
- Required decision projection rows (CSV): `214` plus header row.

This cardinality is mandatory for every active profile version.

## Required Output Artifacts
1. Catalog-entry generation output (normative for implementation):
- one row per `(profile_id, feature_key)` for each parser target profile.
2. Decision projection output (normative for review and drift checks):
- `CAPABILITY_PROFILE_DECISION_TABLE.csv`
- one row per `feature_key`, with one decision column per parser target.

## Target Set
- `native`
- `firebird`
- `postgresql`
- `mysql`
- `cassandra`
- `mongodb`
- `neo4j`
- `redis`
- `milvus`

## Required-Engines Normalization
The `required_engines` text in the section-21 matrix must be normalized exactly as follows.

### Preprocessing
1. Lowercase the source string.
2. Remove all parenthesized annotations, including `(mapped)`, `(events)`, `(pubsub)`.
3. Normalize separators:
- replace `/` with `,`
- collapse duplicate spaces
4. Split by comma and trim tokens.

### Group Expansion Tokens
- `all` -> all 9 targets.
- `all sql-like surfaces` -> all 9 targets.
- `all with indexing` -> all 9 targets.
- `all with long jobs` -> all 9 targets.
- `sql engines` -> `native, firebird, postgresql, mysql`.
- `postgresql-equivalent` -> `native, postgresql`.

### Alias Tokens
- `firebird service` -> `firebird`.
- `firebird-style mga` -> `firebird`.
- `redis-like expiry` -> `redis`.

### Direct Target Tokens
Direct tokens map 1:1 when present:
- `native`, `firebird`, `postgresql`, `mysql`, `cassandra`, `mongodb`, `neo4j`, `redis`, `milvus`.

### Unsupported Token Rule
If any token remains after normalization and is not in the group or alias tables above:
- fail profile build with `CAPABILITY_BUILD_UNKNOWN_REQUIRED_ENGINE_TOKEN`.

## Decision Generation Algorithm
For each `(parser_target, feature_key)` pair:
1. Set defaults:
- `decision = REJECT`
- `reject_code = UNSUPPORTED_IN_DIALECT`
- `transform_id = null`
2. If `parser_target = native`:
- set `decision = IMPLEMENT`
- clear `reject_code`
3. Else if `parser_target` is in normalized required-target set for `feature_key`:
- set `decision = IMPLEMENT`
- clear `reject_code`
4. Apply explicit remap overrides (if matched):
- set `decision = REMAP`
- set `transform_id`
- clear `reject_code`
5. Apply explicit reject overrides (if matched):
- set `decision = REJECT`
- set explicit `reject_code`
- clear `transform_id`
6. Set `family_key` and `result_shape_id` from section-21 and section-22 mappings.
7. Compute deterministic `precedence_rank` and `entry_checksum`.

## Explicit Remap Override Table
These overrides are applied after baseline `IMPLEMENT`/`REJECT` derivation.

### firebird
- `F_DDL_CREATE_DATABASE_EMULATED` -> `TR_REWRITE_EMULATED_CREATE_DATABASE`
- `F_SESSION_SET` -> `TR_RESOLVE_DIALECT_ALIASES`
- `F_SESSION_SHOW` -> `TR_RESOLVE_DIALECT_ALIASES`

### postgresql
- `F_ADMIN_VACUUM_ALIAS` -> `TR_RESOLVE_DIALECT_ALIASES`
- `F_DDL_CREATE_DATABASE_EMULATED` -> `TR_REWRITE_EMULATED_CREATE_DATABASE`

### mysql
- `F_DML_MERGE` -> `TR_REWRITE_DIALECT_UPSERT`
- `F_DDL_CREATE_DATABASE_EMULATED` -> `TR_REWRITE_EMULATED_CREATE_DATABASE`

### cassandra
- SQL upsert-like feature rewrites explicitly listed in profile entries -> `TR_REWRITE_DIALECT_UPSERT`

### redis
- `F_CQL_TTL` (when enabled by profile) -> redis expiry remap transform (must be registered in transform table)

### milvus
- Vector SQL alias remaps explicitly listed in profile entries -> corresponding registered transform.

## Explicit Reject Override Table
- none in alpha baseline beyond baseline decision rules.
- if a parser profile adds reject overrides, they must be declared in `CAPABILITY_PROFILE_ENTRIES_CANONICAL.md` and versioned.

## Precedence Rank Formula
For deterministic ordering:
- `precedence_rank = family_rank * 1000 + feature_rank`

Where:
1. `family_rank` is fixed by ordered family table in section 22.
2. `feature_rank` is 1-based lexicographic order of `feature_key` within the family.

No ties are allowed.

## Build-Failure Conditions
- Generated row count != `target_count * feature_count` (current baseline `1926`).
- Decision projection row count != `feature_key_count`.
- Missing `family_key`.
- Missing `result_shape_id`.
- `REMAP` row missing `transform_id`.
- `REJECT` row missing `reject_code`.
- Unknown required-engines token after normalization.
- Duplicate `(profile_id, feature_key)`.
- Any feature key in section-21 matrix missing from decision projection CSV.
- Any feature key in decision projection CSV missing from section-21 matrix.

## Determinism Requirement
Given identical inputs (matrix rows, transforms, overrides, profile version), generated capability rows must be byte-identical and checksums must match across runs.

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

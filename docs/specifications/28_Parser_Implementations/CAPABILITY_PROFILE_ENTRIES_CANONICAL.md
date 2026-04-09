# Capability Profile Entries (Canonical)

## Purpose
Define the authoritative schema and population algorithm for parser capability profiles so every parser target has explicit feature decisions with no implicit behavior.

## Canonical Build Reference
The required deterministic generation algorithm is defined in:
- `CAPABILITY_PROFILE_BUILD_ALGORITHM.md`

## Core Rule
No parser behavior is allowed unless backed by one capability profile entry.

## Required Catalog Entities

### `parser_profile`
Columns:
- `profile_id` (UUID, PK)
- `profile_name` (STRING, unique)
- `parser_target` (ENUM: native, firebird, postgresql, mysql, cassandra, mongodb, neo4j, redis, milvus)
- `profile_version` (UINT32)
- `is_active` (BOOL)
- `created_txid` (UINT64)
- `updated_txid` (UINT64)

### `parser_capability_entry`
Columns:
- `profile_id` (UUID, FK)
- `feature_key` (STRING)
- `family_key` (STRING)
- `decision` (ENUM: IMPLEMENT, REMAP, REJECT)
- `transform_id` (STRING, nullable)
- `reject_code` (STRING, nullable)
- `result_shape_id` (STRING)
- `precedence_rank` (UINT16)
- `entry_checksum` (BINARY(32))
- `created_txid` (UINT64)
- `updated_txid` (UINT64)

### `parser_transform_entry`
Columns:
- `transform_id` (STRING, PK)
- `transform_group` (STRING)
- `description` (STRING)
- `input_contract` (BLOB)
- `output_contract` (BLOB)
- `is_lossless` (BOOL)
- `created_txid` (UINT64)

### `parser_error_map_entry`
Columns:
- `profile_id` (UUID, FK)
- `reject_code` (STRING)
- `dialect_error_code` (STRING)
- `sqlstate_or_equivalent` (STRING, nullable)
- `message_template_id` (STRING)
- `created_txid` (UINT64)

### `parser_feature_precedence`
Columns:
- `family_key` (STRING)
- `feature_key` (STRING)
- `precedence_rank` (UINT16)
- `created_txid` (UINT64)

## Population Algorithm (No Guessing Allowed)
1. Load target list (`9` parser targets).
2. Load authoritative feature list from section 21 matrix.
3. Normalize `required_engines` text using the token and group-expansion rules in `CAPABILITY_PROFILE_BUILD_ALGORITHM.md`.
4. Create full cross-product rows: `target x feature_key`.
5. Generate baseline decisions from normalized required-target sets and native superset rules.
6. Apply explicit remap and reject overrides from this document.
7. Validate every row has:
   - non-null `result_shape_id`
   - non-null `family_key`
   - non-null `precedence_rank`
8. Persist profile entries.
9. Compute and store deterministic `entry_checksum`.

No row may be omitted.

Required row count per profile version:
- `9 * feature_key_count`
- with current section-21 matrix: `9 * 214 = 1926`.

## Global Defaults
- Missing capability entry at runtime: reject with `PROFILE_ENTRY_MISSING`.
- Missing transform for `REMAP`: reject with `TRANSFORM_NOT_DEFINED`.
- Missing error map row: reject with `ERROR_MAP_ENTRY_MISSING`.

## Allowed Transform IDs
- `TR_CASE_NORMALIZE_IDENTIFIERS`
- `TR_RESOLVE_DIALECT_ALIASES`
- `TR_NORMALIZE_LITERAL_TYPES`
- `TR_EXPAND_IMPLICIT_DEFAULTS`
- `TR_CANONICALIZE_OPTION_KEYS`
- `TR_REWRITE_EMULATED_CREATE_DATABASE`
- `TR_REWRITE_DIALECT_UPSERT`
- `TR_REWRITE_DIALECT_RETURNING`
- `TR_REWRITE_DIALECT_BULK_COPY`
- `TR_REWRITE_DIALECT_NOTIFICATION`

## Required Reject Codes
- `PROFILE_ENTRY_MISSING`
- `UNSUPPORTED_IN_DIALECT`
- `FEATURE_DISABLED`
- `DIALECT_DISABLED`
- `TRANSFORM_NOT_DEFINED`
- `REMAP_CONTRACT_FAILED`
- `RESULT_SHAPE_NOT_ALLOWED`

## Override Sets by Target

### `native`
- Set `decision=IMPLEMENT` for all feature keys in section 21 matrix unless explicitly marked native-reject.
- Native-reject list:
  - none in alpha baseline.

### `firebird`
- Set `IMPLEMENT`:
  - `F_TXN_BEGIN`, `F_TXN_COMMIT`, `F_TXN_ROLLBACK`, `F_TXN_SAVEPOINT`
  - `F_DDL_CREATE_SCHEMA`, `F_DDL_CREATE_TABLE`, `F_DDL_ALTER_TABLE`, `F_DDL_DROP_TABLE`
  - `F_DDL_CREATE_VIEW`, `F_DDL_CREATE_INDEX`, `F_DDL_DROP_INDEX`
  - `F_DDL_CREATE_SEQUENCE`, `F_DDL_CREATE_DOMAIN`, `F_DDL_CREATE_FUNCTION`, `F_DDL_CREATE_PROCEDURE`, `F_DDL_CREATE_PACKAGE`, `F_DDL_CREATE_TRIGGER`
  - `F_DML_SELECT`, `F_DML_INSERT`, `F_DML_UPDATE`, `F_DML_DELETE`, `F_DML_MERGE`, `F_DML_RETURNING`
  - `F_NOTIFY_PUBLISH`, `F_NOTIFY_SUBSCRIBE`
  - `F_ADMIN_BACKUP`, `F_ADMIN_RESTORE`, `F_ADMIN_VALIDATE`, `F_ADMIN_SWEEP`
- Set `REMAP`:
  - `F_DDL_CREATE_DATABASE_EMULATED` -> `TR_REWRITE_EMULATED_CREATE_DATABASE`
  - `F_SESSION_SET`, `F_SESSION_SHOW` -> `TR_RESOLVE_DIALECT_ALIASES`

### `postgresql`
- Set `IMPLEMENT`:
  - core txn/session/sql ddl/sql dml/prepared/notify/admin/fdw families
  - `F_DDL_CREATE_MATERIALIZED_VIEW`, `F_DDL_REFRESH_MATERIALIZED_VIEW`
  - `F_DML_COPY_BULK`
  - `F_PREPARE_STATEMENT`, `F_EXECUTE_PREPARED`, `F_DEALLOCATE_PREPARED`
- Set `REMAP`:
  - `F_ADMIN_VACUUM_ALIAS` -> `TR_RESOLVE_DIALECT_ALIASES`
  - `F_DDL_CREATE_DATABASE_EMULATED` -> `TR_REWRITE_EMULATED_CREATE_DATABASE`

### `mysql`
- Set `IMPLEMENT`:
  - core txn/session/sql ddl/sql dml/prepared/admin
  - mysql-specific event and user privilege paths (`F_DDL_CREATE_EVENT`, grant/revoke)
- Set `REMAP`:
  - `F_DML_MERGE` via upsert mapping -> `TR_REWRITE_DIALECT_UPSERT`
  - `F_DDL_CREATE_DATABASE_EMULATED` -> `TR_REWRITE_EMULATED_CREATE_DATABASE`

### `cassandra`
- Set `IMPLEMENT`:
  - `F_CQL_KEYSPACE`, `F_CQL_BATCH`, `F_CQL_TTL`, `F_CQL_WRITETIME`
  - mapped table/type/index/materialized-view family subset
- Set `REMAP`:
  - SQL families accepted by CQL adapter into canonical CQL-equivalent AST where defined.

### `mongodb`
- Set `IMPLEMENT`:
  - `F_MONGO_FIND`, `F_MONGO_AGGREGATE`, `F_MONGO_FIND_AND_MODIFY`, `F_MONGO_BULK_WRITE`
  - session/admin/security subset defined for mongodb profile.
- Set `REMAP`:
  - SQL mutation aliases to document command forms where profile allows.

### `neo4j`
- Set `IMPLEMENT`:
  - `F_CYPHER_MATCH`, `F_CYPHER_MERGE`, `F_CYPHER_UNWIND`, `F_CYPHER_CALL`
- Set `REMAP`:
  - selected SQL forms to graph forms where explicit mapping exists.

### `redis`
- Set `IMPLEMENT`:
  - `F_REDIS_STRING`, `F_REDIS_HASH`, `F_REDIS_LIST`, `F_REDIS_SET`, `F_REDIS_ZSET`, `F_REDIS_STREAM`, `F_REDIS_PUBSUB`
- Set `REMAP`:
  - `F_CQL_TTL` style expiry aliases -> redis expiry operations where enabled.

### `milvus`
- Set `IMPLEMENT`:
  - `F_MILVUS_CREATE_COLLECTION`, `F_MILVUS_DROP_COLLECTION`, `F_MILVUS_CREATE_INDEX`, `F_MILVUS_DROP_INDEX`
  - `F_MILVUS_INSERT`, `F_MILVUS_DELETE`, `F_MILVUS_SEARCH`, `F_MILVUS_QUERY`
- Set `REMAP`:
  - vector SQL aliases to API feature forms where enabled.

## Result Shape Enforcement
Each capability row must bind to one permitted `result_shape_id` from section 21 matrix. Parser must reject if runtime emitted shape differs.

## Precedence Rules
1. Higher `precedence_rank` wins when two feature entries could match.
2. Ties are invalid and must fail profile build validation.
3. Precedence must be deterministic per family.

## Profile Validation Checklist
1. Full cross-product entry count present.
2. No null `family_key`.
3. No null `result_shape_id`.
4. `REMAP` rows have non-null valid `transform_id`.
5. `REJECT` rows have non-null `reject_code`.
6. Every `reject_code` has error map entry.
7. No duplicate `(profile_id, feature_key)` rows.
8. No unknown normalized required-engine token remains after expansion.

## Mandatory Runtime Logging Fields
- `profile_id`
- `profile_version`
- `feature_key`
- `family_key`
- `decision`
- `transform_id`
- `reject_code`
- `result_shape_id`

## Test Requirements
- Build-time profile validator tests.
- Runtime capability lookup determinism tests.
- Transform contract tests for every `REMAP` row.
- Error-map coverage tests for every reject code per parser target.

## Audit normalization note (2026-03-28)
- Current code-backed parser authority is bounded to the native V3 stack (`parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`) plus dedicated shipped emulated SQL-family parser code for Firebird, PostgreSQL, and MySQL.
- Dedicated parser-agent and listener proof currently exists only for `sb_parser_fb`, `sb_parser_pg`, `sb_parser_mysql`, and the matching listener front doors; universal nine-family dedicated parser parity is not current implementation proof.
- Builtin emulation package scaffold proof is currently limited to `firebirdsql`, `postgresql`, and `mysql`.
- Cassandra, MongoDB, Neo4j, Redis, and Milvus are currently represented in this section by native-V3 feature vocabulary, catalog/runtime vocabulary, or checklist material rather than shipped dedicated parser implementations.
- Broad section-wide parity, corpus cardinality, and universal profile-generation claims are therefore bounded and must not be treated as present-day implementation proof without family-local source evidence.

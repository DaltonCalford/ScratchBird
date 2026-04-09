# Capability Profile Row Serialization Examples

## Purpose
Provide deterministic, implementation-ready examples of generated capability rows so low-capability agents can produce identical catalog data.

## Scope
- `parser_profile`
- `parser_capability_entry`
- `parser_feature_precedence`

This document is an executable reference for row-shape and value derivation, not an alternative algorithm. The canonical algorithm remains:
- `CAPABILITY_PROFILE_BUILD_ALGORITHM.md`

## Canonical Field Order
`parser_capability_entry` fields must be serialized in this exact order when computing checksums:
1. `profile_id`
2. `feature_key`
3. `family_key`
4. `decision`
5. `transform_id` (empty string when null)
6. `reject_code` (empty string when null)
7. `result_shape_id`
8. `precedence_rank`
9. `profile_version`

## Entry Checksum Formula
`entry_checksum = SHA256(utf8(join('|', field_values_in_order)))`

Normalization rules:
- UUID lowercase canonical textual form.
- Null values normalized to empty string.
- `decision`, `family_key`, `result_shape_id`, `feature_key` are uppercase as stored.

## Family Rank Table (for precedence)
1. `FG_TRANSACTION`
2. `FG_SESSION`
3. `FG_DDL_SQL`
4. `FG_DML_SQL`
5. `FG_PREPARED`
6. `FG_NOTIFICATION`
7. `FG_ADMIN`
8. `FG_FDW`
9. `FG_CQL`
10. `FG_MONGO`
11. `FG_CYPHER`
12. `FG_REDIS`
13. `FG_MILVUS`
14. `FG_TEXT_SEARCH`
15. `FG_CLUSTER_CONTROL`
16. `FG_JOB_CONTROL`
17. `FG_SECURITY_ADMIN`
18. `FG_SERVICE_CHANNEL`

`precedence_rank = family_rank * 1000 + feature_rank_within_family`

## Example Profile Rows

### Example A: Native Profile
Profile row:
```text
profile_id      = 00000000-0000-0000-0000-000000000021
profile_name    = native_alpha_v1
parser_target   = native
profile_version = 1
is_active       = true
```

Representative generated capability rows:

| feature_key | family_key | decision | transform_id | reject_code | result_shape_id | precedence_rank |
| --- | --- | --- | --- | --- | --- | ---: |
| `F_TXN_BEGIN` | `FG_TRANSACTION` | `IMPLEMENT` | `` | `` | `RS_COMMAND_STATUS` | `1001` |
| `F_CONFIG_SHOW` | `FG_SESSION` | `IMPLEMENT` | `` | `` | `RS_KEY_VALUE_SET` | `2004` |
| `F_CONFIG_RESOURCE_BUNDLE_VALIDATE` | `FG_SESSION` | `IMPLEMENT` | `` | `` | `RS_DIAGNOSTIC_REPORT` | `2007` |
| `F_DDL_CREATE_TABLE` | `FG_DDL_SQL` | `IMPLEMENT` | `` | `` | `RS_COMMAND_STATUS` | `3008` |
| `F_DML_SELECT` | `FG_DML_SQL` | `IMPLEMENT` | `` | `` | `RS_ROWSET` | `4005` |
| `F_PREPARE_STATEMENT` | `FG_PREPARED` | `IMPLEMENT` | `` | `` | `RS_COMMAND_STATUS` | `5001` |
| `F_NOTIFY_SUBSCRIBE` | `FG_NOTIFICATION` | `IMPLEMENT` | `` | `` | `RS_STREAM_STATUS` | `6002` |
| `F_ADMIN_VALIDATE` | `FG_ADMIN` | `IMPLEMENT` | `` | `` | `RS_DIAGNOSTIC_REPORT` | `7003` |
| `F_FDW_CREATE_SERVER` | `FG_FDW` | `IMPLEMENT` | `` | `` | `RS_COMMAND_STATUS` | `8002` |
| `F_TEXTSEARCH_DROP_CONFIGURATION` | `FG_TEXT_SEARCH` | `IMPLEMENT` | `` | `` | `RS_COMMAND_STATUS` | `14006` |
| `F_CLUSTER_SHOW_STATE` | `FG_CLUSTER_CONTROL` | `IMPLEMENT` | `` | `` | `RS_ROWSET` | `15006` |
| `F_SECURITY_SHOW_STATUS` | `FG_SECURITY_ADMIN` | `IMPLEMENT` | `` | `` | `RS_ROWSET` | `17007` |
| `F_SERVICE_CHANNEL_PROGRESS` | `FG_SERVICE_CHANNEL` | `IMPLEMENT` | `` | `` | `RS_JOB_STATUS` | `18003` |

### Example B: PostgreSQL Profile
Profile row:
```text
profile_id      = 00000000-0000-0000-0000-000000000028
profile_name    = postgresql_alpha_v1
parser_target   = postgresql
profile_version = 1
is_active       = true
```

Representative generated capability rows:

| feature_key | family_key | decision | transform_id | reject_code | result_shape_id | precedence_rank |
| --- | --- | --- | --- | --- | --- | ---: |
| `F_TXN_BEGIN` | `FG_TRANSACTION` | `IMPLEMENT` | `` | `` | `RS_COMMAND_STATUS` | `1001` |
| `F_SESSION_SHOW` | `FG_SESSION` | `IMPLEMENT` | `` | `` | `RS_KEY_VALUE_SET` | `2002` |
| `F_CONFIG_SET` | `FG_SESSION` | `REJECT` | `` | `UNSUPPORTED_IN_DIALECT` | `RS_COMMAND_STATUS` | `2005` |
| `F_CONFIG_RESOURCE_BUNDLE_ACTIVATE` | `FG_SESSION` | `REJECT` | `` | `UNSUPPORTED_IN_DIALECT` | `RS_COMMAND_STATUS` | `2008` |
| `F_DDL_CREATE_DATABASE_EMULATED` | `FG_DDL_SQL` | `REMAP` | `TR_REWRITE_EMULATED_CREATE_DATABASE` | `` | `RS_COMMAND_STATUS` | `3002` |
| `F_DML_COPY_BULK` | `FG_DML_SQL` | `IMPLEMENT` | `` | `` | `RS_STREAM_STATUS` | `4007` |
| `F_ADMIN_VACUUM_ALIAS` | `FG_ADMIN` | `REMAP` | `TR_RESOLVE_DIALECT_ALIASES` | `` | `RS_COMMAND_STATUS` | `7006` |
| `F_FDW_CREATE_WRAPPER` | `FG_FDW` | `IMPLEMENT` | `` | `` | `RS_COMMAND_STATUS` | `8001` |
| `F_CQL_KEYSPACE` | `FG_CQL` | `REJECT` | `` | `UNSUPPORTED_IN_DIALECT` | `RS_COMMAND_STATUS` | `9001` |
| `F_TEXTSEARCH_DROP_CONFIGURATION` | `FG_TEXT_SEARCH` | `IMPLEMENT` | `` | `` | `RS_COMMAND_STATUS` | `14006` |
| `F_CLUSTER_SET_STATE` | `FG_CLUSTER_CONTROL` | `REJECT` | `` | `UNSUPPORTED_IN_DIALECT` | `RS_COMMAND_STATUS` | `15005` |
| `F_SECURITY_SHOW_STATUS` | `FG_SECURITY_ADMIN` | `REJECT` | `` | `UNSUPPORTED_IN_DIALECT` | `RS_ROWSET` | `17007` |
| `F_SERVICE_CHANNEL_EVENTS` | `FG_SERVICE_CHANNEL` | `IMPLEMENT` | `` | `` | `RS_STREAM_STATUS` | `18002` |

## SQL Insert Shape (Normative)
```sql
INSERT INTO parser_capability_entry (
  profile_id,
  feature_key,
  family_key,
  decision,
  transform_id,
  reject_code,
  result_shape_id,
  precedence_rank,
  entry_checksum,
  created_txid,
  updated_txid
) VALUES (
  :profile_id,
  :feature_key,
  :family_key,
  :decision,
  :transform_id,
  :reject_code,
  :result_shape_id,
  :precedence_rank,
  :entry_checksum,
  :txid,
  :txid
);
```

## Build Assertions
1. Exactly one row per `(profile_id, feature_key)`.
2. Generated row count equals `target_count * feature_count`.
3. Entry checksums are deterministic for identical inputs.
4. `REMAP` rows always have non-null `transform_id`.
5. `REJECT` rows always have non-null `reject_code`.

## Audit normalization note (2026-03-28)
- Current code-backed parser authority is bounded to the native V3 stack (`parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`) plus dedicated shipped emulated SQL-family parser code for Firebird, PostgreSQL, and MySQL.
- Dedicated parser-agent and listener proof currently exists only for `sb_parser_fb`, `sb_parser_pg`, `sb_parser_mysql`, and the matching listener front doors; universal nine-family dedicated parser parity is not current implementation proof.
- Builtin emulation package scaffold proof is currently limited to `firebirdsql`, `postgresql`, and `mysql`.
- Cassandra, MongoDB, Neo4j, Redis, and Milvus are currently represented in this section by native-V3 feature vocabulary, catalog/runtime vocabulary, or checklist material rather than shipped dedicated parser implementations.
- Broad section-wide parity, corpus cardinality, and universal profile-generation claims are therefore bounded and must not be treated as present-day implementation proof without family-local source evidence.

# Catalog: Parser Capability Tables

## Purpose
Define canonical on-disk tables for parser capability profiles, feature decisions, transforms, error mappings, and precedence ordering.

## Scope
- Native parser capability registry.
- Emulated parser capability registry for Firebird, PostgreSQL, MySQL, Cassandra, MongoDB, Neo4j, Redis, and Milvus.
- Deterministic parser behavior contracts used by section 28.

## Conventions
- All columns use catalog domains from `CATALOG_SYSTEM_DOMAINS.md`.
- Enum columns use `[sb_dom]cat_enum_<enum_kind>` with labels defined below.
- `*_uuid` columns use domain-family rule `[sb_dom]cat_<name>_uuid`.
- All timestamps are UTC (`[sb_dom]cat_timestamp`).

## Enum Labels Used By This Document
For parser capability tables, these labels are fixed and case-sensitive.

### `capability_action`
- `IMPLEMENT`
- `REMAP`
- `REJECT`

### `transform_stage`
- `PRE_PARSE`
- `AST_REWRITE`
- `SBLR_REWRITE`
- `RESULT_REWRITE`

### `error_severity`
- `ERROR`
- `WARNING`
- `NOTICE`

### `precedence_tiebreak`
- `SPECIFICITY_FIRST`
- `PROFILE_ORDER`
- `FEATURE_KEY_ASC`

## Table: `parser_profile`
Columns:
- `parser_profile_uuid` `[sb_dom]cat_parser_profile_uuid` PK
- `profile_name` `[sb_dom]cat_identifier`
- `parser_engine` `[sb_dom]cat_enum_emulation_engine`
- `version_major` `[sb_dom]cat_uint16`
- `version_minor` `[sb_dom]cat_uint16`
- `is_native` `[sb_dom]cat_bool`
- `is_default` `[sb_dom]cat_bool`
- `is_enabled` `[sb_dom]cat_bool`
- `base_profile_uuid` `[sb_dom]cat_parser_profile_uuid` nullable
- `profile_hash` `[sb_dom]cat_hash32`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`profile_name`)
- UNIQUE(`parser_engine`, `version_major`, `version_minor`)
- `is_default=true` may appear at most once per `parser_engine`.
- `base_profile_uuid` must reference same `parser_engine`.

## Table: `parser_capability_entry`
Columns:
- `parser_capability_entry_uuid` `[sb_dom]cat_parser_capability_entry_uuid` PK
- `parser_profile_uuid` `[sb_dom]cat_parser_profile_uuid`
- `feature_family` `[sb_dom]cat_identifier`
- `feature_key` `[sb_dom]cat_identifier`
- `capability_action` `[sb_dom]cat_identifier`
- `transform_uuid` `[sb_dom]cat_parser_transform_entry_uuid` nullable
- `reject_code` `[sb_dom]cat_identifier` nullable
- `precedence_rank` `[sb_dom]cat_uint16`
- `notes` `[sb_dom]cat_text` nullable
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`parser_profile_uuid`, `feature_family`, `feature_key`)
- `capability_action` must be one of: `IMPLEMENT`, `REMAP`, `REJECT`.
- If `capability_action=REMAP`, `transform_uuid` must be non-null.
- If `capability_action=REJECT`, `reject_code` must be non-null.
- If `capability_action=IMPLEMENT`, both `transform_uuid` and `reject_code` must be null.

## Table: `parser_transform_entry`
Columns:
- `parser_transform_entry_uuid` `[sb_dom]cat_parser_transform_entry_uuid` PK
- `parser_profile_uuid` `[sb_dom]cat_parser_profile_uuid`
- `transform_name` `[sb_dom]cat_identifier`
- `feature_family` `[sb_dom]cat_identifier`
- `feature_key` `[sb_dom]cat_identifier`
- `transform_stage` `[sb_dom]cat_identifier`
- `input_contract_json` `[sb_dom]cat_json`
- `output_contract_json` `[sb_dom]cat_json`
- `implementation_ref` `[sb_dom]cat_identifier`
- `is_deterministic` `[sb_dom]cat_bool`
- `is_idempotent` `[sb_dom]cat_bool`
- `timeout_ms` `[sb_dom]cat_duration_ms`
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`parser_profile_uuid`, `transform_name`)
- `transform_stage` must be one of: `PRE_PARSE`, `AST_REWRITE`, `SBLR_REWRITE`, `RESULT_REWRITE`.
- `is_deterministic` must be `true` in Alpha.
- `timeout_ms` must be greater than `0`.

## Table: `parser_error_map_entry`
Columns:
- `parser_error_map_entry_uuid` `[sb_dom]cat_parser_error_map_entry_uuid` PK
- `parser_profile_uuid` `[sb_dom]cat_parser_profile_uuid`
- `reject_code` `[sb_dom]cat_identifier`
- `dialect_sqlstate` `[sb_dom]cat_identifier`
- `dialect_error_code` `[sb_dom]cat_identifier`
- `error_severity` `[sb_dom]cat_identifier`
- `message_template` `[sb_dom]cat_text`
- `hint_template` `[sb_dom]cat_text` nullable
- `is_retryable` `[sb_dom]cat_bool`
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`parser_profile_uuid`, `reject_code`)
- `error_severity` must be one of: `ERROR`, `WARNING`, `NOTICE`.

## Table: `parser_feature_precedence`
Columns:
- `parser_feature_precedence_uuid` `[sb_dom]cat_parser_feature_precedence_uuid` PK
- `parser_profile_uuid` `[sb_dom]cat_parser_profile_uuid`
- `feature_family` `[sb_dom]cat_identifier`
- `feature_key` `[sb_dom]cat_identifier`
- `precedence_rank` `[sb_dom]cat_uint16`
- `precedence_tiebreak` `[sb_dom]cat_identifier`
- `is_terminal` `[sb_dom]cat_bool`
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`parser_profile_uuid`, `feature_family`, `feature_key`)
- UNIQUE(`parser_profile_uuid`, `feature_family`, `precedence_rank`)
- `precedence_tiebreak` must be one of: `SPECIFICITY_FIRST`, `PROFILE_ORDER`, `FEATURE_KEY_ASC`.

## Deterministic Evaluation Algorithm (Normative)
1. Resolve active `parser_profile` for channel dialect and version.
2. Read enabled `parser_capability_entry` rows for target `feature_family`.
3. Apply `parser_feature_precedence` rank (ascending) for exact `feature_key`, then wildcard key.
4. First terminal row (`is_terminal=true`) decides action.
5. If action is `REMAP`, execute referenced `parser_transform_entry` at `transform_stage`.
6. If action is `REJECT`, map parser error with `parser_error_map_entry`.
7. If no row matches, reject with deterministic code `SB-PARSER-CAP-0001`.

## Required Integration Links
- Section 28 consumes these tables as runtime capability authority:
  - `28_Parser_Implementations/CAPABILITY_PROFILE_ENTRIES_CANONICAL.md`
  - `28_Parser_Implementations/CAPABILITY_PROFILE_BUILD_ALGORITHM.md`
  - `28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`
- Section 21 feature family and key names MUST match entries in this catalog.

## Test Contract Additions
- `T24-PARSER-CAP-01`: one and only one default profile per engine.
- `T24-PARSER-CAP-02`: conditional nullability for `capability_action` is enforced.
- `T24-PARSER-CAP-03`: non-deterministic transforms are rejected at DDL time.
- `T24-PARSER-CAP-04`: precedence ordering yields stable result for identical input.
- `T24-PARSER-CAP-05`: reject mapping emits deterministic dialect code and message.

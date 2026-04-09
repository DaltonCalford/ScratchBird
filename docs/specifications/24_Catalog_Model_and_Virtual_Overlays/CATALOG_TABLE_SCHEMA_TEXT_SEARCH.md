# Catalog: Text Search Tables

## Purpose
Define canonical catalog tables for PostgreSQL-compatible text search configuration.

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Enum values are defined in `CATALOG_ENUMS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.
- `*_toast_uuid` columns reference TOAST values.

## Table: `ts_parser`
Columns:
- `parser_uuid` `[sb_dom]cat_ts_parser_uuid` PK
- `parser_name` `[sb_dom]cat_identifier`
- `start_proc_uuid` `[sb_dom]cat_procedure_uuid`
- `gettoken_proc_uuid` `[sb_dom]cat_procedure_uuid`
- `end_proc_uuid` `[sb_dom]cat_procedure_uuid`
- `lextypes_proc_uuid` `[sb_dom]cat_procedure_uuid`
- `headline_proc_uuid` `[sb_dom]cat_procedure_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`parser_name`)

## Table: `ts_template`
Columns:
- `template_uuid` `[sb_dom]cat_ts_template_uuid` PK
- `template_name` `[sb_dom]cat_identifier`
- `init_proc_uuid` `[sb_dom]cat_procedure_uuid`
- `lexize_proc_uuid` `[sb_dom]cat_procedure_uuid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`template_name`)

## Table: `ts_dictionary`
Columns:
- `dict_uuid` `[sb_dom]cat_ts_dict_uuid` PK
- `dict_name` `[sb_dom]cat_identifier`
- `template_uuid` `[sb_dom]cat_ts_template_uuid`
- `init_options_uuid` `[sb_dom]cat_ts_init_options_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`dict_name`)

Notes:
- `init_options_uuid` stores a JSON payload for template initialization options.

## Table: `ts_config`
Columns:
- `config_uuid` `[sb_dom]cat_ts_config_uuid` PK
- `config_name` `[sb_dom]cat_identifier`
- `parser_uuid` `[sb_dom]cat_ts_parser_uuid`
- `default_dict_uuid` `[sb_dom]cat_ts_dict_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`config_name`)

## Table: `ts_config_map`
Columns:
- `map_uuid` `[sb_dom]cat_ts_config_map_uuid` PK
- `config_uuid` `[sb_dom]cat_ts_config_uuid`
- `token_type` `[sb_dom]cat_identifier`
- `dict_list_uuid` `[sb_dom]cat_ts_dict_list_uuid`
- `is_override` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`config_uuid`, `token_type`)

Notes:
- `dict_list_uuid` stores an ordered UUID list of dictionaries to try for the token type.

# Catalog: Type System Schema

## Purpose
Define catalog tables for all type metadata (base types, domains, composites, enums, ranges, arrays, maps, lists, vectors, geometry, BSON) and cast rules.

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Enum values are defined in `CATALOG_ENUMS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.
- `*_toast_uuid` columns reference TOAST values.

## Table: `type`
Columns:
- `type_uuid` `[sb_dom]cat_type_uuid` PK
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `type_name` `[sb_dom]cat_identifier`
- `type_kind` `[sb_dom]cat_enum_type_kind`
- `base_type_uuid` `[sb_dom]cat_type_uuid` nullable
- `element_type_uuid` `[sb_dom]cat_type_uuid` nullable
- `key_type_uuid` `[sb_dom]cat_type_uuid` nullable
- `value_type_uuid` `[sb_dom]cat_type_uuid` nullable
- `range_subtype_uuid` `[sb_dom]cat_type_uuid` nullable
- `is_system` `[sb_dom]cat_bool`
- `system_origin` `[sb_dom]cat_enum_system_origin`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`schema_uuid`, `type_name`)

## Table: `type_modifier`
Columns:
- `type_uuid` `[sb_dom]cat_type_uuid`
- `modifier_key` `[sb_dom]cat_enum_type_modifier_key`
- `val_u64` `[sb_dom]cat_uint64` nullable
- `val_i64` `[sb_dom]cat_int64` nullable
- `val_f64` `[sb_dom]cat_f64` nullable
- `val_bool` `[sb_dom]cat_bool` nullable
- `val_text` `[sb_dom]cat_text` nullable
- `val_uuid` `[sb_dom]cat_uuid` nullable
- `val_json` `[sb_dom]cat_json` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`type_uuid`, `modifier_key`)
- Exactly one of the `val_*` columns must be non-null and must match `modifier_key`.

## Table: `type_io`
Columns:
- `type_uuid` `[sb_dom]cat_type_uuid`
- `input_fn_uuid` `[sb_dom]cat_procedure_uuid`
- `output_fn_uuid` `[sb_dom]cat_procedure_uuid`
- `binary_input_fn_uuid` `[sb_dom]cat_procedure_uuid` nullable
- `binary_output_fn_uuid` `[sb_dom]cat_procedure_uuid` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`type_uuid`)

## Table: `type_cast`
Columns:
- `source_type_uuid` `[sb_dom]cat_type_uuid`
- `target_type_uuid` `[sb_dom]cat_type_uuid`
- `cast_kind` `[sb_dom]cat_enum_cast_kind`
- `cast_fn_uuid` `[sb_dom]cat_procedure_uuid` nullable
- `is_lossy` `[sb_dom]cat_bool`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`source_type_uuid`, `target_type_uuid`, `cast_kind`)

## Table: `type_transform`
Columns:
- `transform_uuid` `[sb_dom]cat_type_transform_uuid` PK
- `type_uuid` `[sb_dom]cat_type_uuid`
- `language_uuid` `[sb_dom]cat_language_uuid`
- `from_sql_proc_uuid` `[sb_dom]cat_procedure_uuid` nullable
- `to_sql_proc_uuid` `[sb_dom]cat_procedure_uuid` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`type_uuid`, `language_uuid`)
- At least one of `from_sql_proc_uuid` or `to_sql_proc_uuid` must be non-null.

## Table: `encoding_conversion`
Columns:
- `conversion_uuid` `[sb_dom]cat_encoding_conversion_uuid` PK
- `conversion_name` `[sb_dom]cat_identifier`
- `source_charset_uuid` `[sb_dom]cat_charset_uuid`
- `target_charset_uuid` `[sb_dom]cat_charset_uuid`
- `conversion_proc_uuid` `[sb_dom]cat_procedure_uuid`
- `is_default` `[sb_dom]cat_bool`
- `is_system` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`conversion_name`)
- UNIQUE(`source_charset_uuid`, `target_charset_uuid`, `is_default`) where `is_default=true`.

## Notes
- System types are hidden unless `SHOW SYSTEM` is enabled.
- Emulated parsers map dialect types to these catalog entries.

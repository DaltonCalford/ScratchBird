# Catalog: Rewrite Rules

## Purpose
Define canonical catalog tables for SQL rewrite rules (PostgreSQL-style rule system).

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Enum values are defined in `CATALOG_ENUMS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.
- `*_toast_uuid` columns reference TOAST values.

## Table: `rule`
Columns:
- `rule_uuid` `[sb_dom]cat_rule_uuid` PK
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `rule_name` `[sb_dom]cat_identifier`
- `table_uuid` `[sb_dom]cat_table_uuid` nullable
- `event_kind` `[sb_dom]cat_enum_rule_event`
- `action_kind` `[sb_dom]cat_enum_rule_action_kind`
- `action_sblr_uuid` `[sb_dom]cat_rule_action_uuid`
- `condition_sblr_uuid` `[sb_dom]cat_rule_condition_uuid` nullable
- `is_enabled` `[sb_dom]cat_bool`
- `is_system` `[sb_dom]cat_bool`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`schema_uuid`, `rule_name`)
- `table_uuid` MUST be non-null for `event_kind` in (INSERT, UPDATE, DELETE).

Notes:
- `event_kind=SELECT` applies to views and is valid when `table_uuid` references the view's backing table.
- Rules are applied by the parser prior to SBLR emission and must not inject dialect SQL into the engine.

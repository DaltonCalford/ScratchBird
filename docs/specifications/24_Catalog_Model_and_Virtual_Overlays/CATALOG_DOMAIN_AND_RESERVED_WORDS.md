# Catalog: Domains and Reserved Words

## Purpose
Define canonical catalog schema additions required for domain parameters and reserved word storage.

## Domain Catalog Tables
### `domain`
- `domain_uuid` (UUID, PK)
- `domain_name` (string)
- `domain_kind` (enum)
- `base_type_uuid` (UUID)
- `is_system` (bool)
- `system_origin` (enum: native, firebird, postgresql, mysql, cassandra, milvus, mongodb, neo4j, redis)

#### Catalog System Domains (Mandatory)
- Canonical catalog tables MUST use the catalog system domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Raw base types are forbidden for catalog columns; every catalog column must map to a catalog system domain or catalog domain family.
- Catalog system domains are created at database installation and recorded in `domain` with `is_system=true` and `system_origin=native`.

#### UUID Rules
- System domains MUST use fixed UUID values defined by the system domain registry.
- The system domain registry is defined in `15_Complex_Types/SYSTEM_DOMAIN_UUID_REGISTRY.md`.
- User domains in workgroup or cluster deployments MUST use shared UUIDs assigned by:
  - Workgroup master database (workgroup mode).
  - Cluster consensus service (cluster mode).
- Standalone embedded deployments may generate domain UUIDs locally.

### `domain_param_key`
- `param_key_id` (u16, PK)
- `param_name` (string)
- `param_type` (enum: u32, i32, u64, i64, u8, bool, string, uuid, enum, float32, float64)

### `domain_parameter`
- `domain_uuid` (UUID, FK)
- `param_key_id` (u16, FK)
- `param_type` (enum)
- `param_value` (typed storage columns)
- Typed storage columns:
  - `val_u32`, `val_i32`, `val_u64`, `val_i64`, `val_u8`, `val_bool`, `val_string`, `val_uuid`, `val_enum`, `val_f32`, `val_f64`

### `domain_constraint`
- `domain_uuid` (UUID, FK)
- `constraint_kind` (enum)
- `constraint_expr_sblr` (blob)

### `domain_security`
- `domain_uuid` (UUID, FK)
- `security_kind` (enum)
- `security_expr_sblr` (blob)

### `domain_validation`
- `domain_uuid` (UUID, FK)
- `validation_kind` (enum)
- `validation_expr_sblr` (blob)

### `domain_integrity`
- `domain_uuid` (UUID, FK)
- `integrity_kind` (enum)
- `integrity_expr_sblr` (blob)

## Reserved Words Catalog
### `reserved_words`
- `word` (string)
- `parser_scope` (enum: native, firebird, postgresql, mysql, cassandra, milvus, mongodb, neo4j, redis)
- `is_reserved` (bool)
- `is_keyword` (bool)
- `last_updated_txid` (u64)

## Constraints
- `(domain_name, system_origin)` is unique.
- `domain_parameter` must reference a key in `domain_param_key`.
- Each `param_key_id` must be unique.
- `reserved_words` has unique `(word, parser_scope)`.

## Notes
- Reserved word lists are data and may be updated without code changes.
- System domains are hidden by default unless `SHOW SYSTEM` is enabled.

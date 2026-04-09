# Specification: Catalog Table Layouts

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:4502`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:4806`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/catalog/sys_catalog.cpp:473`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:350`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_full_extract.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_database_bootstrap.cpp:30`

## Synopsis

This specification defines the on-disk and in-memory layouts of ScratchBird's catalog tables. It covers the core identity tables (database, schema, object, object_name), type system tables, and the sys.* virtual catalog views. These structures collectively form the RDB$*/pg_* equivalent metadata system.

## Scope

### In Scope

- Core catalog table schemas (database, schema, object, object_name)
- Type system tables (type, domain)
- Object metadata tables (table, column, index, constraint)
- Virtual catalog views (sys.*)
- Storage class assignments (on-disk vs virtual)
- Record layouts and field definitions

### Out of Scope

- Bootstrap sequence (see `bootstrap_sequence.md`)
- UUID generation rules (see `uuid_mapping.md`)
- Object identity resolution (see `object_identity_rules.md`)

## Background

ScratchBird's catalog system uses a hybrid storage model:
- **On-disk tables**: Persistent system catalog tables stored as heap pages
- **Virtual views**: Computed views over on-disk data (sys.*, information_schema)
- **In-memory tables**: Runtime-only structures

This differs from PostgreSQL's approach (all catalog tables are regular tables) and Firebird's approach (hard-coded system table structures).

## Specification

### Storage Classes

| Class | Description | Examples |
|-------|-------------|----------|
| `on-disk` | Persisted system catalog table | database, schema, table, column |
| `virtual` | View/overlay (no storage) | sys.sessions, sys.tables |
| `in-memory` | Runtime-only structure | prepared_statement cache |

### Core Identity Tables

#### Table: database

**Source:** `src/core/catalog_manager.cpp:4806`

Storage: `on-disk` | Primary Key: `database_id`

| Column | Type | Size | Nullable | Description |
|--------|------|------|----------|-------------|
| `database_id` | UUID | 16 | No | Database UUID (same as root schema) |
| `database_name` | STRING | 512 | No | Database name (UTF-8) |
| `owner_id` | UUID | 16 | No | Owner principal UUID |
| `created_time` | UINT64 | 8 | No | Creation timestamp |
| `last_modified_time` | UINT64 | 8 | No | Last modification timestamp |
| `is_valid` | UINT32 | 4 | No | Soft delete flag |
| `padding` | - | 4 | - | Alignment |

**Total record size:** 560 bytes

```cpp
struct DatabaseRecord {
    ID database_id;
    char database_name[512];
    ID owner_id;
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

#### Table: schema

**Source:** `src/core/catalog_manager.cpp:4851`

Storage: `on-disk` | Primary Key: `schema_id`

| Column | Type | Size | Nullable | Description |
|--------|------|------|----------|-------------|
| `schema_id` | UUID | 16 | No | Schema UUID |
| `parent_schema_id` | UUID | 16 | Yes | Parent schema (zero = root) |
| `schema_name` | STRING | 512 | No | Schema name |
| `owner_id` | UUID | 16 | No | Owner UUID |
| `default_tablespace_id` | UUID | 16 | Yes | Default tablespace |
| `permissions` | UINT32 | 4 | No | Permission bitmask |
| `default_charset_id` | UUID | 16 | Yes | Default charset |
| `name_is_delimited` | UINT8 | 1 | No | Quoted identifier flag |
| `reserved` | - | 7 | - | Padding |
| `default_collation_id` | UINT32 | 4 | Yes | Default collation |
| `acl_oid` | UUID | 16 | Yes | TOAST reference for ACL |
| `created_time` | UINT64 | 8 | No | Creation timestamp |
| `last_modified_time` | UINT64 | 8 | No | Modification timestamp |
| `is_valid` | UINT32 | 4 | No | Soft delete flag |
| `padding` | UINT32 | 4 | - | Alignment |

**Total record size:** 656 bytes

#### Table: object

**Source:** `src/core/catalog_manager.cpp:4818`

Storage: `on-disk` | Primary Key: `object_id`

| Column | Type | Size | Nullable | Description |
|--------|------|------|----------|-------------|
| `object_id` | UUID | 16 | No | Object UUID |
| `object_type` | UINT8 | 1 | No | ObjectType enum |
| `reserved_1` | - | 3 | - | Padding |
| `schema_id` | UUID | 16 | No | Containing schema |
| `parent_object_id` | UUID | 16 | Yes | Parent object (if hierarchical) |
| `owner_id` | UUID | 16 | No | Owner UUID |
| `created_time` | UINT64 | 8 | No | Creation timestamp |
| `last_modified_time` | UINT64 | 8 | No | Modification timestamp |
| `is_valid` | UINT32 | 4 | No | Soft delete flag |
| `padding` | UINT32 | 4 | - | Alignment |

**Total record size:** 112 bytes

#### Table: object_name

**Source:** `src/core/catalog_manager.cpp:4833`

Storage: `on-disk` | Primary Key: `name_id` | Unique Index: `(schema_path, object_type, canonical_name_text)`

| Column | Type | Size | Nullable | Description |
|--------|------|------|----------|-------------|
| `name_id` | UUID | 16 | No | Name entry UUID |
| `object_id` | UUID | 16 | No | Referenced object |
| `object_type` | UINT8 | 1 | No | ObjectType enum |
| `reserved_1` | - | 3 | - | Padding |
| `parent_object_id` | UUID | 16 | Yes | Parent for hierarchical naming |
| `schema_path` | STRING | 512 | No | Full schema path |
| `language_code` | STRING | 32 | No | Language code (i18n) |
| `name_text` | STRING | 512 | No | Original name |
| `canonical_name_text` | STRING | 512 | No | Uppercase/canonical form |
| `created_time` | UINT64 | 8 | No | Creation timestamp |
| `last_modified_time` | UINT64 | 8 | No | Modification timestamp |
| `is_valid` | UINT32 | 4 | No | Soft delete flag |
| `padding` | UINT32 | 4 | - | Alignment |

**Total record size:** 1648 bytes

### Table Metadata Tables

#### Table: table (sb_tables)

**Source:** `src/core/catalog_manager.cpp:4883`

Storage: `on-disk` | Primary Key: `table_id`

| Column | Type | Size | Nullable | Description |
|--------|------|------|----------|-------------|
| `table_id` | UUID | 16 | No | Table UUID |
| `schema_id` | UUID | 16 | No | Schema UUID |
| `table_name` | STRING | 512 | No | Table name |
| `owner_id` | UUID | 16 | No | Owner UUID |
| `root_gpid` | UINT64 | 8 | No | Root page GPID |
| `column_count` | UINT32 | 4 | No | Number of columns |
| `row_count` | UINT64 | 8 | No | Estimated row count |
| `table_type` | UINT8 | 1 | No | TableType enum |
| `has_toast` | UINT8 | 1 | No | Has TOAST table |
| `rls_enabled` | UINT8 | 1 | No | Row-level security enabled |
| `rls_forced` | UINT8 | 1 | No | Force RLS for owners |
| `temp_metadata_scope` | UINT8 | 1 | No | TempMetadataScope enum |
| `temp_data_scope` | UINT8 | 1 | No | TempDataScope enum |
| `temp_on_commit` | UINT8 | 1 | No | TempOnCommitAction enum |
| `temp_flags` | UINT8 | 1 | No | Reserved temp flags |
| `name_is_delimited` | UINT8 | 1 | No | Quoted identifier |
| `tablespace_id` | UUID | 16 | Yes | Tablespace UUID |
| `default_charset_id` | UUID | 16 | Yes | Default charset |
| `default_collation_id` | UINT32 | 4 | Yes | Default collation |
| `storage_params_oid` | UUID | 16 | Yes | TOAST for storage params |
| `creating_session_id` | UUID | 16 | Yes | Session for temp tables |
| `creating_transaction_id` | UINT64 | 8 | Yes | Transaction for temp |
| `temp_parent_table_id` | UUID | 16 | Yes | Parent for temp instance |
| `temp_schema_id` | UUID | 16 | Yes | Session temp schema |
| `created_time` | UINT64 | 8 | No | Creation timestamp |
| `last_modified_time` | UINT64 | 8 | No | Modification timestamp |
| `policy_epoch` | UINT64 | 8 | No | Security policy epoch |
| `is_valid` | UINT32 | 4 | No | Soft delete flag |
| `padding` | UINT32 | 4 | - | Alignment |

#### Table: column (sb_columns)

**Source:** `src/core/catalog_manager.cpp:4917`

Storage: `on-disk` | Primary Key: `(table_id, column_id)`

| Column | Type | Size | Nullable | Description |
|--------|------|------|----------|-------------|
| `table_id` | UUID | 16 | No | Parent table |
| `column_id` | UUID | 16 | No | Column UUID |
| `column_name` | STRING | 512 | No | Column name |
| `ordinal` | UINT16 | 2 | No | Position in table |
| `data_type` | UINT16 | 2 | No | DataType enum |
| `type_precision` | UINT32 | 4 | No | Precision/length |
| `type_scale` | UINT32 | 4 | No | Scale for decimal |
| `max_length` | UINT32 | 4 | No | Legacy field |
| `domain_id` | UUID | 16 | Yes | Domain UUID |
| `is_array` | UINT8 | 1 | No | Array column flag |
| `array_size` | UINT32 | 4 | No | Fixed array size |
| `nullable` | UINT8 | 1 | No | NULL allowed |
| `has_default` | UINT8 | 1 | No | Has default value |
| `is_primary_key` | UINT8 | 1 | No | PK column |
| `is_unique` | UINT8 | 1 | No | Unique column |
| `is_foreign_key` | UINT8 | 1 | No | FK column |
| `is_generated` | UINT8 | 1 | No | Generated column |
| `storage_type` | UINT8 | 1 | No | TOAST strategy |
| `with_timezone` | UINT8 | 1 | No | WITH TIME ZONE |
| `name_is_delimited` | UINT8 | 1 | No | Quoted identifier |
| `charset_id` | UUID | 16 | Yes | Charset UUID |
| `timezone_id` | UUID | 16 | Yes | Timezone UUID |
| `collation_id` | UINT32 | 4 | Yes | Collation ID |
| `default_value` | STRING | 128 | Yes | Default literal |
| `default_value_oid` | UUID | 16 | Yes | TOAST for large defaults |
| `check_expr_oid` | UUID | 16 | Yes | TOAST for check expr |
| `created_time` | UINT64 | 8 | No | Creation timestamp |
| `is_valid` | UINT32 | 4 | No | Soft delete flag |
| `padding` | UINT32 | 4 | - | Alignment |

### Index and Constraint Tables

#### Table: index (sb_indexes)

**Source:** `src/core/catalog_manager.cpp:4963`

Storage: `on-disk` | Primary Key: `index_id`

| Column | Type | Size | Nullable | Description |
|--------|------|------|----------|-------------|
| `index_id` | UUID | 16 | No | Index UUID |
| `table_id` | UUID | 16 | No | Table UUID |
| `index_name` | STRING | 512 | No | Index name |
| `owner_id` | UUID | 16 | No | Owner UUID |
| `root_gpid` | UINT64 | 8 | No | Root page GPID |
| `index_type` | UINT8 | 1 | No | IndexType enum |
| `is_unique` | UINT8 | 1 | No | Unique index |
| `column_count` | UINT16 | 2 | No | Key columns count |
| `column_ids` | UUID[] | 256 | No | Array of 16 UUIDs |
| `include_column_count` | UINT16 | 2 | No | Include columns |
| `include_column_ids` | UUID[] | 256 | No | Array of 16 UUIDs |
| `index_params_oid` | UUID | 16 | Yes | TOAST for params |
| `expression_oid` | UUID | 16 | Yes | TOAST for expression |
| `predicate_oid` | UUID | 16 | Yes | TOAST for predicate |
| `logical_index_id` | UUID | 16 | No | Stable logical ID |
| `state` | UINT8 | 1 | No | IndexState enum |
| `name_is_delimited` | UINT8 | 1 | No | Quoted identifier |
| `tablespace_id` | UUID | 16 | Yes | Tablespace UUID |
| `valid_from_xid` | UINT64 | 8 | No | Visibility XID |
| `retired_xid` | UINT64 | 8 | No | Retired XID |
| `build_started_time` | UINT64 | 8 | No | Build start |
| `build_completed_time` | UINT64 | 8 | No | Build end |
| `created_time` | UINT64 | 8 | No | Creation timestamp |
| `is_valid` | UINT32 | 4 | No | Soft delete flag |

#### Table: constraint (sb_constraints)

**Source:** `src/core/catalog_manager.cpp:5104`

Storage: `on-disk` | Primary Key: `constraint_id`

| Column | Type | Size | Nullable | Description |
|--------|------|------|----------|-------------|
| `constraint_id` | UUID | 16 | No | Constraint UUID |
| `table_id` | UUID | 16 | No | Table UUID |
| `constraint_name` | STRING | 512 | No | Constraint name |
| `owner_id` | UUID | 16 | No | Owner UUID |
| `constraint_type` | UINT8 | 1 | No | ConstraintType enum |
| `is_deferrable` | UINT8 | 1 | No | Can be deferred |
| `initially_deferred` | UINT8 | 1 | No | Deferred by default |
| `is_enabled` | UINT8 | 1 | No | Enabled flag |
| `is_validated` | UINT8 | 1 | No | Validated flag |
| `is_system_generated` | UINT8 | 1 | No | Auto-generated name |
| `on_delete` | UINT8 | 1 | No | FKAction enum |
| `on_update` | UINT8 | 1 | No | FKAction enum |
| `match_type` | UINT8 | 1 | No | FKMatchType enum |
| `column_count` | UINT16 | 2 | No | Column count |
| `column_ids` | UUID[] | 256 | No | Array of 16 UUIDs |
| `referenced_table_id` | UUID | 16 | Yes | FK parent table |
| `referenced_column_count` | UINT16 | 2 | Yes | Parent columns |
| `referenced_column_ids` | UUID[] | 256 | Yes | Array of 16 UUIDs |
| `check_expr_oid` | UUID | 16 | Yes | TOAST for check |
| `exclusion_operator_oid` | UUID | 16 | Yes | TOAST for operator |
| `index_method_oid` | UUID | 16 | Yes | TOAST for method |
| `created_time` | UINT64 | 8 | No | Creation timestamp |
| `validated_time` | UINT64 | 8 | No | Validation timestamp |
| `is_valid` | UINT32 | 4 | No | Soft delete flag |
| `padding` | UINT32 | 4 | - | Alignment |

### Virtual Catalog Views (sys.*)

**Source:** `src/catalog/sys_catalog.cpp:473`

Storage: `virtual` | Computed over on-disk tables

#### View: sys.schemas

| Column | Type | Nullable | Source |
|--------|------|----------|--------|
| `schema_id` | UUID | No | schema.schema_id |
| `schema_name` | TEXT | No | schema.schema_name |
| `owner_id` | UUID | Yes | schema.owner_id |
| `default_tablespace_id` | UUID | Yes | schema.default_tablespace_id |
| `default_charset` | UUID | Yes | schema.default_charset_id |
| `default_collation_id` | UINT32 | Yes | schema.default_collation_id |
| `is_valid` | BOOLEAN | Yes | schema.is_valid |

#### View: sys.tables

| Column | Type | Nullable | Source |
|--------|------|----------|--------|
| `table_id` | UUID | No | table.table_id |
| `schema_id` | UUID | No | table.schema_id |
| `table_name` | TEXT | No | table.table_name |
| `table_type` | TEXT | Yes | table.table_type enum |
| `owner_id` | UUID | Yes | table.owner_id |
| `tablespace_id` | UUID | Yes | table.tablespace_id |
| `row_count` | INT64 | Yes | table.row_count |
| `has_toast` | BOOLEAN | Yes | table.has_toast |
| `toast_table_id` | UUID | Yes | table.toast_table_id |
| `is_valid` | BOOLEAN | Yes | table.is_valid |
| `partition_strategy` | TEXT | Yes | From storage_params JSON |
| `partition_columns` | TEXT | Yes | From storage_params JSON |
| `partition_parent_name` | TEXT | Yes | From storage_params JSON |
| `is_partition_child` | BOOLEAN | Yes | Derived |

#### View: sys.columns

| Column | Type | Nullable | Source |
|--------|------|----------|--------|
| `column_id` | UUID | No | column.column_id |
| `table_id` | UUID | No | column.table_id |
| `column_name` | TEXT | No | column.column_name |
| `data_type_id` | UINT16 | Yes | column.data_type |
| `data_type_name` | TEXT | Yes | TypeSystem lookup |
| `ordinal_position` | INT32 | Yes | column.ordinal |
| `is_nullable` | BOOLEAN | Yes | column.nullable |
| `default_value` | TEXT | Yes | column.default_value or TOAST |
| `domain_id` | UUID | Yes | column.domain_id |
| `collation_id` | UINT32 | Yes | column.collation_id |
| `charset_id` | UUID | Yes | column.charset_id |
| `is_identity` | BOOLEAN | Yes | column.is_identity |
| `is_generated` | BOOLEAN | Yes | column.is_generated |
| `generation_expression` | TEXT | Yes | column.generation_expression or TOAST |
| `is_valid` | BOOLEAN | Yes | column.is_valid |

#### View: sys.indexes

| Column | Type | Nullable | Source |
|--------|------|----------|--------|
| `index_id` | UUID | No | index.index_id |
| `table_id` | UUID | No | index.table_id |
| `index_name` | TEXT | No | index.index_name |
| `index_type` | TEXT | Yes | index.index_type enum |
| `is_unique` | BOOLEAN | Yes | index.is_unique |
| `is_expression` | BOOLEAN | Yes | index.is_expression_index |
| `is_partial` | BOOLEAN | Yes | index.is_partial_index |
| `expression_sql` | TEXT | Yes | index.expression_strings |
| `predicate_sql` | TEXT | Yes | index.predicate_string |
| `state` | TEXT | Yes | index.state enum |
| `tablespace_id` | UUID | Yes | index.tablespace_id |
| `is_valid` | BOOLEAN | Yes | index.is_valid |

#### View: sys.domains

| Column | Type | Nullable | Source |
|--------|------|----------|--------|
| `domain_id` | UUID | No | domain.domain_id |
| `schema_id` | UUID | No | domain.schema_id |
| `domain_name` | TEXT | No | domain.domain_name |
| `domain_type` | TEXT | Yes | domain.domain_type enum |
| `base_type_id` | UINT16 | Yes | domain.base_type |
| `base_type_name` | TEXT | Yes | TypeSystem lookup |
| `precision` | UINT32 | Yes | domain.precision |
| `scale` | UINT32 | Yes | domain.scale |
| `is_nullable` | BOOLEAN | Yes | domain.nullable |
| `default_value` | TEXT | Yes | domain.default_value |
| `parent_domain_id` | UUID | Yes | domain.parent_domain_id |
| `is_enum` | BOOLEAN | Yes | domain.domain_type == ENUM |
| `enum_labels` | TEXT | Yes | domain.enum_values |
| `collation_name` | TEXT | Yes | domain.collation_name |

### Catalog Root Page Layout

**Source:** `src/core/catalog_manager.cpp:4502`

The `CatalogRootPage` structure contains page pointers to all catalog tables:

```cpp
struct CatalogRootPage {
    PageHeader header;
    uint32_t schema_count;
    uint32_t table_count;
    
    // Core catalog tables
    uint32_t schemas_page;        // Page containing schemas table
    uint32_t tables_page;         // Page containing tables table
    uint32_t columns_page;        // Page containing columns table
    uint32_t indexes_page;        // Page containing indexes table
    uint32_t constraints_page;    // Page containing constraints table
    uint32_t sequences_page;      // Page containing sequences table
    uint32_t views_page;          // Page containing views table
    uint32_t triggers_page;       // Page containing triggers table
    uint32_t permissions_page;    // Page containing permissions table
    // ... 150+ additional catalog page pointers
    
    uint8_t reserved[2920];       // Padding for 4KB page
};
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|--------------|
| `TABLE_INV_001` | All on-disk catalog tables use PAGE_TYPE_HEAP | Page header validation |
| `TABLE_INV_002` | Record sizes are 8-byte aligned | sizeof() verification |
| `TABLE_INV_003` | Virtual views expose only valid objects | is_valid filter |
| `TABLE_INV_004` | TOAST references are valid or zero | Foreign key check |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_catalog_full_extract.cpp` | Full catalog extraction |
| `tests/unit/test_catalog_database_bootstrap.cpp` | Record layout validation |
| `tests/unit/test_catalog_type_schema_contract.cpp` | Type system tables |
| `tests/unit/test_virtual_catalogs.cpp` | Virtual view correctness |

## Related Specifications

- `bootstrap_sequence.md` - Catalog table creation order
- `uuid_mapping.md` - UUID usage in catalog tables
- `object_identity_rules.md` - Name resolution using catalog tables

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| TOAST | The Oversized-Attribute Storage Technique for large values |
| GPID | Global Page ID (filespace + page number) |
| Soft Delete | is_valid flag for MGA (Multi-Generational Architecture) |
| Virtual View | Computed view over on-disk catalog tables |

### References

- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_CORE_OBJECTS.md`

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |

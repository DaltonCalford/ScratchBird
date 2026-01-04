# ScratchBird System Tables Reference

**Last Updated:** December 6, 2025

This document describes all system catalog tables in ScratchBird, their schemas, and on-disk storage formats.

---

## Catalog Architecture

### Page Layout

The catalog is stored on **Page 1** (CatalogRootPage) which contains references to all system table pages:

```
Page 0: Database Header
Page 1: Catalog Root (CatalogRootPage)
Page 2+: System table data pages
```

### Storage Format

- All records use **packed binary format** (`#pragma pack(push, 1)`)
- Fixed-size records for deterministic sizing
- **TOAST** for variable-length data (large text, JSON, expressions)
- **MGA semantics**: `is_valid=1` = current, `is_valid=0` = deleted
- **UUID references** throughout for secure cross-references

---

## Core Catalog Tables

### sb_schema (Table #1)
Stores database schema definitions.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| schema_id | ID (UUID) | 16 | Primary key |
| parent_schema_id | ID (UUID) | 16 | Parent schema (hierarchical) |
| schema_name | char[512] | 512 | Schema name |
| owner_id | ID (UUID) | 16 | Owner user UUID |
| default_tablespace_id | uint16 | 2 | Default tablespace |
| permissions | uint16 | 2 | Permission bitmask |
| default_charset | uint16 | 2 | CharacterSet enum |
| default_collation_id | uint32 | 4 | Default collation |
| acl_oid | uint32 | 4 | TOAST ref for ACL |
| created_time | uint64 | 8 | Creation timestamp |
| last_modified_time | uint64 | 8 | Last modified |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~640 bytes

---

### sb_tables (Table #2)
Stores table metadata.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| table_id | ID (UUID) | 16 | Primary key |
| schema_id | ID (UUID) | 16 | Parent schema |
| table_name | char[512] | 512 | Table name |
| owner_id | ID (UUID) | 16 | Owner user UUID |
| root_page | uint32 | 4 | Root data page |
| column_count | uint32 | 4 | Number of columns |
| row_count | uint64 | 8 | Estimated rows |
| table_type | uint8 | 1 | TableType enum |
| has_toast | uint8 | 1 | Has TOAST table |
| rls_enabled | uint8 | 1 | RLS enabled |
| rls_forced | uint8 | 1 | Force RLS |
| tablespace_id | uint16 | 2 | Tablespace ID |
| default_charset | uint16 | 2 | CharacterSet |
| default_collation_id | uint32 | 4 | Collation |
| storage_params_oid | uint32 | 4 | TOAST ref |
| created_time | uint64 | 8 | Creation time |
| last_modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Table Types:** HEAP=0, INDEX=1, TEMP=2, EXTERNAL=3, MATERIALIZED_VIEW=4, TOAST=5

**Record Size:** ~640 bytes

---

### sb_columns (Table #3)
Stores column definitions.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| table_id | ID (UUID) | 16 | Parent table |
| column_id | ID (UUID) | 16 | Primary key |
| column_name | char[512] | 512 | Column name |
| ordinal | uint16 | 2 | Position in table |
| data_type | uint16 | 2 | DataType enum |
| type_precision | uint32 | 4 | Precision |
| type_scale | uint32 | 4 | Scale |
| max_length | uint32 | 4 | Max length |
| nullable | uint8 | 1 | Nullable flag |
| has_default | uint8 | 1 | Has default |
| is_primary_key | uint8 | 1 | PK flag |
| is_unique | uint8 | 1 | Unique flag |
| is_foreign_key | uint8 | 1 | FK flag |
| is_generated | uint8 | 1 | Generated col |
| storage_type | uint8 | 1 | TOAST strategy |
| with_timezone | uint8 | 1 | TZ flag |
| charset | uint16 | 2 | CharacterSet |
| timezone_hint | uint16 | 2 | Timezone ID |
| collation_id | uint32 | 4 | Collation |
| default_value | char[128] | 128 | Inline default |
| default_value_oid | uint32 | 4 | TOAST ref |
| check_expr_oid | uint32 | 4 | TOAST ref |
| created_time | uint64 | 8 | Creation time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~760 bytes

---

### sb_indexes (Table #4)
Stores index metadata.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| index_id | ID (UUID) | 16 | Primary key |
| table_id | ID (UUID) | 16 | Parent table |
| index_name | char[512] | 512 | Index name |
| owner_id | ID (UUID) | 16 | Owner UUID |
| root_page | uint32 | 4 | Root index page |
| index_type | uint8 | 1 | IndexType enum |
| is_unique | uint8 | 1 | Unique flag |
| column_count | uint16 | 2 | Column count |
| column_ids | ID[16] | 256 | Column UUIDs |
| index_params_oid | uint32 | 4 | TOAST ref |
| created_time | uint64 | 8 | Creation time |
| is_valid | uint32 | 4 | MGA flag |

**Index Types:**
- BTREE=0, HASH=1, HNSW=2, FULLTEXT=3, GIN=4, GIST=5
- BRIN=6, RTREE=7, SPGIST=8, BITMAP=9, COLUMNSTORE=10, LSM=11

**Record Size:** ~896 bytes

---

### sb_constraints (Table #5)
Stores table constraints.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| constraint_id | ID (UUID) | 16 | Primary key |
| table_id | ID (UUID) | 16 | Parent table |
| constraint_name | char[512] | 512 | Name |
| owner_id | ID (UUID) | 16 | Owner UUID |
| constraint_type | uint8 | 1 | ConstraintType |
| is_deferrable | uint8 | 1 | Deferrable |
| initially_deferred | uint8 | 1 | Initially def |
| column_count | uint16 | 2 | Column count |
| column_ids | ID[16] | 256 | Columns |
| referenced_table_id | ID (UUID) | 16 | FK target |
| referenced_column_count | uint16 | 2 | FK col count |
| referenced_column_ids | ID[16] | 256 | FK columns |
| check_expr_oid | uint32 | 4 | TOAST ref |
| created_time | uint64 | 8 | Creation time |
| is_valid | uint32 | 4 | MGA flag |

**Constraint Types:** PK=0, FK=1, UNIQUE=2, CHECK=3, NOT_NULL=4, DEFAULT=5, EXCLUSION=6

**Record Size:** ~1024 bytes

---

### sb_sequences (Table #6)
Stores sequence definitions.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| sequence_id | ID (UUID) | 16 | Primary key |
| schema_id | ID (UUID) | 16 | Parent schema |
| sequence_name | char[512] | 512 | Name |
| owner_id | ID (UUID) | 16 | Owner UUID |
| current_value | int64 | 8 | Current value |
| increment_by | int64 | 8 | Increment |
| min_value | int64 | 8 | Minimum |
| max_value | int64 | 8 | Maximum |
| cache_size | int64 | 8 | Cache size |
| cycle | uint8 | 1 | Cycle flag |
| created_time | uint64 | 8 | Creation time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~624 bytes

---

### sb_views (Table #7)
Stores view definitions.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| view_id | ID (UUID) | 16 | Primary key |
| schema_id | ID (UUID) | 16 | Parent schema |
| view_name | char[512] | 512 | Name |
| owner_id | ID (UUID) | 16 | Owner UUID |
| definition_oid | uint32 | 4 | TOAST ref (SQL) |
| is_materialized | uint8 | 1 | MV flag |
| created_time | uint64 | 8 | Creation time |
| last_refreshed | uint64 | 8 | Last MV refresh |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~616 bytes

---

### sb_triggers (Table #8)
Stores trigger definitions.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| trigger_id | ID (UUID) | 16 | Primary key |
| table_id | ID (UUID) | 16 | Parent table |
| trigger_name | char[512] | 512 | Name |
| trigger_timing | uint8 | 1 | 0=BEFORE, 1=AFTER, 2=INSTEAD |
| trigger_events | uint8 | 1 | Bitmask: 1=INSERT, 2=UPDATE, 4=DELETE |
| for_each_row | uint8 | 1 | Row-level flag |
| enabled | uint8 | 1 | Enabled flag |
| condition_oid | uint32 | 4 | TOAST ref (WHEN) |
| action_oid | uint32 | 4 | TOAST ref (action) |
| created_time | uint64 | 8 | Creation time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~600 bytes

---

### sb_permissions (Table #9)
Stores object-level permissions.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| permission_id | ID (UUID) | 16 | Primary key |
| object_id | ID (UUID) | 16 | Target object |
| object_type | uint8 | 1 | ObjectType enum |
| grantee_id | ID (UUID) | 16 | User/Role/Group |
| grantee_type | uint8 | 1 | 0=USER, 1=ROLE, 2=GROUP, 3=PUBLIC |
| privileges | uint32 | 4 | Privilege bitmask |
| grant_option | uint8 | 1 | WITH GRANT OPTION |
| grantor_id | ID (UUID) | 16 | Grantor UUID |
| created_time | uint64 | 8 | Creation time |
| is_valid | uint32 | 4 | MGA flag |

**Privileges:** SELECT=1, INSERT=2, UPDATE=4, DELETE=8, TRUNCATE=16, REFERENCES=32, TRIGGER=64, CREATE=128, USAGE=256, EXECUTE=512, CONNECT=1024, ALL=65535

**Record Size:** ~120 bytes

---

### sb_statistics (Table #10)
Stores column statistics for query optimization.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| statistic_id | ID (UUID) | 16 | Primary key |
| table_id | ID (UUID) | 16 | Parent table |
| column_id | ID (UUID) | 16 | Column |
| data_type | uint16 | 2 | DataType enum |
| num_rows | uint64 | 8 | Row count |
| num_nulls | uint64 | 8 | NULL count |
| null_fraction | float | 4 | NULL fraction |
| num_distinct | uint64 | 8 | Distinct count |
| avg_width | float | 4 | Avg width bytes |
| mcv_oid | uint32 | 4 | TOAST (MCV JSON) |
| histogram_oid | uint32 | 4 | TOAST (histogram) |
| histogram_type | uint8 | 1 | 0=equal_height, 1=equal_width |
| histogram_bucket_count | uint32 | 4 | Bucket count |
| last_analyzed_time | uint64 | 8 | ANALYZE time |
| sample_size | uint64 | 8 | Sample rows |
| sample_rate | float | 4 | Sample fraction |
| created_time | uint64 | 8 | Creation time |
| last_modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~176 bytes

---

## Character Set & Collation Tables

### sb_charset (Table #13)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| charset_id | uint16 | 2 | CharacterSet enum |
| name | char[64] | 64 | e.g., "utf8" |
| description | char[128] | 128 | Description |
| min_bytes | uint8 | 1 | Min bytes/char |
| max_bytes | uint8 | 1 | Max bytes/char |
| variable_width | uint8 | 1 | Variable flag |
| default_collation_id | uint32 | 4 | Default collation |
| created_time | uint64 | 8 | Creation time |
| last_modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~216 bytes

---

### sb_collation (Table #14)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| collation_id | uint32 | 4 | Unique ID |
| name | char[128] | 128 | e.g., "utf8_general_ci" |
| charset_id | uint16 | 2 | Character set |
| collation_type | uint8 | 1 | CollationType |
| strength | uint8 | 1 | CollationStrength |
| pad_space | uint8 | 1 | PAD SPACE flag |
| is_default | uint8 | 1 | Default for charset |
| locale | char[32] | 32 | e.g., "en_US" |
| created_time | uint64 | 8 | Creation time |
| last_modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~200 bytes

---

### sb_timezone (Table #12)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| timezone_id | uint16 | 2 | Unique ID |
| name | char[64] | 64 | e.g., "America/New_York" |
| abbreviation | char[16] | 16 | e.g., "EST" |
| std_offset_minutes | int32 | 4 | GMT offset |
| observes_dst | uint8 | 1 | DST flag |
| dst_start_month | uint8 | 1 | DST start month |
| dst_start_week | uint8 | 1 | DST start week |
| dst_start_day | uint8 | 1 | DST start day |
| dst_start_hour | uint8 | 1 | DST start hour |
| dst_end_month | uint8 | 1 | DST end month |
| dst_end_week | uint8 | 1 | DST end week |
| dst_end_day | uint8 | 1 | DST end day |
| dst_end_hour | uint8 | 1 | DST end hour |
| dst_offset_minutes | int32 | 4 | DST offset |
| created_time | uint64 | 8 | Creation time |
| last_modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~140 bytes

---

## Security Tables

### sb_users (Table #17)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| user_id | ID (UUID) | 16 | Primary key |
| username | char[512] | 512 | Login name |
| password_hash_oid | uint32 | 4 | TOAST ref |
| user_metadata_oid | uint32 | 4 | TOAST (JSON) |
| default_schema_id | ID (UUID) | 16 | Default schema |
| is_active | uint8 | 1 | Active flag |
| is_superuser | uint8 | 1 | Superuser flag |
| created_time | uint64 | 8 | Creation time |
| last_login_time | uint64 | 8 | Last login |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~592 bytes

---

### sb_roles (Table #18)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| role_id | ID (UUID) | 16 | Primary key |
| role_name | char[512] | 512 | Role name |
| owner_id | ID (UUID) | 16 | Owner UUID |
| role_metadata_oid | uint32 | 4 | TOAST (JSON) |
| is_active | uint8 | 1 | Active flag |
| created_time | uint64 | 8 | Creation time |
| last_modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~592 bytes

---

### sb_groups (Table #19)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| group_id | ID (UUID) | 16 | Primary key |
| group_name | char[512] | 512 | Group name |
| external_id | char[512] | 512 | AD/LDAP ID |
| group_type | uint8 | 1 | 0=LOCAL, 1=AD, 2=LDAP |
| group_metadata_oid | uint32 | 4 | TOAST (JSON) |
| created_time | uint64 | 8 | Creation time |
| last_modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~1088 bytes

---

### sb_role_members (Table #20)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| membership_id | ID (UUID) | 16 | Primary key |
| user_id | ID (UUID) | 16 | Member user |
| role_id | ID (UUID) | 16 | Role |
| granted_by | ID (UUID) | 16 | Grantor |
| with_admin_option | uint8 | 1 | Admin flag |
| granted_time | uint64 | 8 | Grant time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~96 bytes

---

### sb_group_members (Table #21)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| membership_id | ID (UUID) | 16 | Primary key |
| user_id | ID (UUID) | 16 | Member (user/group) |
| member_type | uint8 | 1 | 0=USER, 1=GROUP |
| group_id | ID (UUID) | 16 | Parent group |
| granted_by | ID (UUID) | 16 | Grantor |
| granted_time | uint64 | 8 | Grant time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~96 bytes

---

### sb_policies (Table #40)
Row-Level Security policies.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| policy_id | ID (UUID) | 16 | Primary key |
| table_id | ID (UUID) | 16 | Target table |
| policy_name | char[64] | 64 | Policy name |
| policy_type | uint8 | 1 | 0=ALL, 1=SELECT, etc. |
| roles_oid | uint32 | 4 | TOAST (roles array) |
| using_expr_oid | uint32 | 4 | TOAST (USING expr) |
| with_check_expr_oid | uint32 | 4 | TOAST (WITH CHECK) |
| is_enabled | uint8 | 1 | Enabled flag |
| created_time | uint64 | 8 | Creation time |
| modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~120 bytes

---

### sb_column_permissions (Table #39)
Column-level permissions.

| Field | Type | Size | Description |
|-------|------|------|-------------|
| permission_id | ID (UUID) | 16 | Primary key |
| table_id | ID (UUID) | 16 | Target table |
| column_name | char[128] | 128 | Column name |
| grantee_id | ID (UUID) | 16 | Grantee |
| grantee_type | uint8 | 1 | 1=USER, 2=ROLE, 3=GROUP, 4=PUBLIC |
| privileges | uint32 | 4 | SELECT=1, UPDATE=2, INSERT=4, REFERENCES=8 |
| grant_option | uint8 | 1 | WITH GRANT OPTION |
| grantor_id | ID (UUID) | 16 | Grantor |
| created_time | uint64 | 8 | Creation time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~200 bytes

---

## Stored Code Tables

### sb_procedures (Table #24)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| procedure_id | ID (UUID) | 16 | Primary key |
| schema_id | ID (UUID) | 16 | Parent schema |
| procedure_name | char[512] | 512 | Name |
| owner_id | ID (UUID) | 16 | Owner |
| procedure_type | uint8 | 1 | 0=PROCEDURE, 1=FUNCTION |
| is_selectable | uint8 | 1 | Firebird selectable |
| language | uint8 | 1 | 0=PSQL, 1=SQL, 2=UDR, 3=PLPGSQL |
| sql_security | uint8 | 1 | 0=DEFINER, 1=INVOKER |
| parameter_count | uint32 | 4 | Param count |
| return_type_oid | uint32 | 4 | TOAST ref |
| body_oid | uint32 | 4 | TOAST ref |
| created_time | uint64 | 8 | Creation time |
| last_modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~600 bytes

---

### sb_procedure_params (Table #25)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| parameter_id | ID (UUID) | 16 | Primary key |
| procedure_id | ID (UUID) | 16 | Parent proc |
| parameter_name | char[512] | 512 | Param name |
| parameter_position | uint16 | 2 | Position (1-based) |
| parameter_mode | uint8 | 1 | 0=IN, 1=OUT, 2=INOUT |
| data_type_oid | uint32 | 4 | TOAST ref |
| default_value_oid | uint32 | 4 | TOAST ref |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~592 bytes

---

### sb_domains (Table #26)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| domain_id | ID (UUID) | 16 | Primary key |
| schema_id | ID (UUID) | 16 | Parent schema |
| domain_name | char[512] | 512 | Domain name |
| owner_id | ID (UUID) | 16 | Owner |
| base_type_oid | uint32 | 4 | TOAST ref |
| check_expr_oid | uint32 | 4 | TOAST ref |
| not_null | uint8 | 1 | NOT NULL flag |
| created_time | uint64 | 8 | Creation time |
| last_modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~600 bytes

---

## Metadata Tables

### sb_dependencies (Table #15)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| dependency_id | ID (UUID) | 16 | Primary key |
| dependent_object_id | ID (UUID) | 16 | Dependent object |
| dependent_type | uint8 | 1 | ObjectType |
| referenced_object_id | ID (UUID) | 16 | Referenced object |
| referenced_type | uint8 | 1 | ObjectType |
| dependency_type | uint8 | 1 | 0=NORMAL, 1=AUTO, 2=INTERNAL, 3=PIN |
| created_time | uint64 | 8 | Creation time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~120 bytes

---

### sb_comments (Table #16)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| comment_id | ID (UUID) | 16 | Primary key |
| object_id | ID (UUID) | 16 | Target object |
| object_type | uint8 | 1 | ObjectType |
| owner_id | ID (UUID) | 16 | Author |
| comment_text_oid | uint32 | 4 | TOAST ref |
| created_time | uint64 | 8 | Creation time |
| last_modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~96 bytes

---

### sb_foreign_keys (Table #34)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| fk_id | ID (UUID) | 16 | Primary key |
| fk_name | char[512] | 512 | FK name |
| child_table_id | ID (UUID) | 16 | Referencing table |
| parent_table_id | ID (UUID) | 16 | Referenced table |
| child_columns | char[1024] | 1024 | Child columns (CSV) |
| parent_columns | char[1024] | 1024 | Parent columns (CSV) |
| on_delete | uint8 | 1 | FKAction |
| on_update | uint8 | 1 | FKAction |
| match_type | uint8 | 1 | 0=SIMPLE, 1=FULL, 2=PARTIAL |
| is_enabled | uint8 | 1 | Enabled flag |
| created_time | uint64 | 8 | Creation time |
| is_valid | uint32 | 4 | MGA flag |

**FK Actions:** NO_ACTION=0, RESTRICT=1, CASCADE=2, SET_NULL=3, SET_DEFAULT=4

**Record Size:** ~2152 bytes

---

## Emulation Tables

### sb_emulation_types (Table #29)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| emulation_type_id | ID (UUID) | 16 | Primary key |
| emulation_name | char[64] | 64 | "mysql", "postgres", etc. |
| version_major | uint8 | 1 | Major version |
| version_minor | uint8 | 1 | Minor version |
| mapping_rules_oid | uint32 | 4 | TOAST (JSON) |
| created_time | uint64 | 8 | Creation time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~120 bytes

---

### sb_emulation_servers (Table #30)
| Field | Type | Size | Description |
|-------|------|------|-------------|
| server_id | ID (UUID) | 16 | Primary key |
| server_name | char[512] | 512 | Server name |
| emulation_type_id | ID (UUID) | 16 | Emulation type |
| owner_id | ID (UUID) | 16 | Owner |
| server_config_oid | uint32 | 4 | TOAST (JSON) |
| is_active | uint8 | 1 | Active flag |
| created_time | uint64 | 8 | Creation time |
| last_modified_time | uint64 | 8 | Modified time |
| is_valid | uint32 | 4 | MGA flag |

**Record Size:** ~616 bytes

---

## Complete Table List

| # | Table Name | Purpose |
|---|------------|---------|
| 1 | sb_schema | Schema definitions |
| 2 | sb_tables | Table metadata |
| 3 | sb_columns | Column definitions |
| 4 | sb_indexes | Index metadata |
| 5 | sb_constraints | Table constraints |
| 6 | sb_sequences | Sequence definitions |
| 7 | sb_views | View definitions |
| 8 | sb_triggers | Trigger definitions |
| 9 | sb_permissions | Object permissions |
| 10 | sb_statistics | Column statistics |
| 11 | sb_collation_legacy | Legacy collations |
| 12 | sb_timezone | Timezone data |
| 13 | sb_charset | Character sets |
| 14 | sb_collation | Collation definitions |
| 15 | sb_dependencies | Object dependencies |
| 16 | sb_comments | Object comments |
| 17 | sb_users | User accounts |
| 18 | sb_roles | Role definitions |
| 19 | sb_groups | Group definitions |
| 20 | sb_role_members | Role memberships |
| 21 | sb_group_members | Group memberships |
| 22 | sb_group_mappings | External group mappings |
| 23 | sb_column_permissions | Column-level permissions |
| 24 | sb_procedures | Stored procedures |
| 25 | sb_procedure_params | Procedure parameters |
| 26 | sb_domains | User-defined domains |
| 27 | sb_udr | User-defined resources |
| 28 | sb_packages | Firebird packages |
| 29 | sb_emulation_types | Emulation types |
| 30 | sb_emulation_servers | Emulation servers |
| 31 | sb_emulated_databases | Emulated databases |
| 32 | sb_tablespaces | Tablespace definitions |
| 33 | sb_extensions | Extensions |
| 34 | sb_foreign_keys | Foreign key details |
| 35 | sb_synonyms | Schema synonyms |
| 36 | sb_foreign_servers | FDW servers |
| 37 | sb_foreign_tables | FDW tables |
| 38 | sb_user_mappings | FDW user mappings |
| 39 | sb_column_permissions | Column permissions |
| 40 | sb_policies | RLS policies |
| 41 | sb_server_registry | Distributed MVCC |
| 42 | sb_migration_history | Migration history |

**Total: 42 system tables**

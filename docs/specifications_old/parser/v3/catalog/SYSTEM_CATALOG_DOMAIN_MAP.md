# System Catalog Domain Map (SBDB$ Domains)

Generated from `catalog/SYSTEM_CATALOG_DDL_SBDB.md`. Each column must use an `SBDB$` domain; raw types are forbidden.

Status: Authoritative (V3)

## Rules

- Identifier columns (schema/table/column/index names, roles, users, etc.) MUST use `SBDB$NAME`.
- All catalog identifiers are UUID v7 and use `SBDB$KEY_*` domains (backed by `SBDB$UUID_V7`).
- Any `SBDB$NAME_*` suffix is reserved for non-identifier strings with explicit length requirements
  (paths, hashes, diagnostic payloads). Do not use array syntax.
- If a column name implies a relationship, the domain MUST encode that relationship
  (e.g., `tablespace_id` -> `SBDB$KEY_TABLESPACE`, `default_tablespace_id` -> `SBDB$KEY_TABLESPACE`).

## Validation

- A catalog definition is invalid if any column uses a raw base datatype.
- Any column that violates the domain rule (wrong `SBDB$KEY_*` or name domain)
  MUST be treated as a schema error and rejected at boot.

## sys.catalogrootpage

| Column | Domain |
| --- | --- |
| header | SBDB$U32 |
| schema_count | SBDB$U32 |
| table_count | SBDB$U32 |
| schemas_page | SBDB$PAGE_ID |
| tables_page | SBDB$PAGE_ID |
| columns_page | SBDB$PAGE_ID |
| indexes_page | SBDB$PAGE_ID |
| index_versions_page | SBDB$PAGE_ID |
| constraints_page | SBDB$PAGE_ID |
| sequences_page | SBDB$PAGE_ID |
| views_page | SBDB$PAGE_ID |
| triggers_page | SBDB$PAGE_ID |
| permissions_page | SBDB$PAGE_ID |
| statistics_page | SBDB$PAGE_ID |
| collations_page | SBDB$PAGE_ID |
| timezones_page | SBDB$PAGE_ID |
| charsets_page | SBDB$PAGE_ID |
| collation_defs_page | SBDB$PAGE_ID |
| dependencies_page | SBDB$PAGE_ID |
| comments_page | SBDB$PAGE_ID |
| object_definitions_page | SBDB$PAGE_ID |
| jobs_page | SBDB$PAGE_ID |
| job_runs_page | SBDB$PAGE_ID |
| job_dependencies_page | SBDB$PAGE_ID |
| job_secrets_page | SBDB$PAGE_ID |
| users_page | SBDB$PAGE_ID |
| roles_page | SBDB$PAGE_ID |
| groups_page | SBDB$PAGE_ID |
| role_members_page | SBDB$PAGE_ID |
| group_members_page | SBDB$PAGE_ID |
| group_mappings_page | SBDB$PAGE_ID |
| procedures_page | SBDB$PAGE_ID |
| proc_params_page | SBDB$PAGE_ID |
| domains_page | SBDB$PAGE_ID |
| udr_page | SBDB$PAGE_ID |
| exceptions_page | SBDB$PAGE_ID |
| packages_page | SBDB$PAGE_ID |
| emulation_types_page | SBDB$PAGE_ID |
| emulation_servers_page | SBDB$PAGE_ID |
| emulated_dbs_page | SBDB$PAGE_ID |
| tablespaces_page | SBDB$PAGE_ID |
| tablespace_files_page | SBDB$PAGE_ID |
| extensions_page | SBDB$PAGE_ID |
| foreign_keys_page | SBDB$PAGE_ID |
| synonyms_page | SBDB$PAGE_ID |
| foreign_servers_page | SBDB$PAGE_ID |
| foreign_tables_page | SBDB$PAGE_ID |
| user_mappings_page | SBDB$PAGE_ID |
| server_registry_page | SBDB$PAGE_ID |
| udr_engines_page | SBDB$PAGE_ID |
| udr_modules_page | SBDB$PAGE_ID |
| migration_history_page | SBDB$PAGE_ID |
| dormant_transactions_page | SBDB$PAGE_ID |
| prepared_transactions_page | SBDB$PAGE_ID |
| encryption_keys_page | SBDB$PAGE_ID |
| authkeys_page | SBDB$PAGE_ID |
| sessions_page | SBDB$PAGE_ID |
| audit_log_page | SBDB$PAGE_ID |
| security_policy_epoch_page | SBDB$PAGE_ID |
| policy_toast_table_id | SBDB$KEY_TABLE |
| column_permissions_page | SBDB$PAGE_ID |
| object_permissions_page | SBDB$PAGE_ID |
| policies_page | SBDB$PAGE_ID |

## sys.schemarecord

| Column | Domain |
| --- | --- |
| schema_id | SBDB$KEY_SCHEMA |
| parent_schema_id | SBDB$KEY_SCHEMA |
| schema_name | SBDB$NAME |
| owner_id | SBDB$KEY_USER |
| default_tablespace_id | SBDB$KEY_TABLESPACE |
| permissions | SBDB$PERMISSIONS_MASK |
| default_charset | SBDB$KEY_CHARSET |
| name_is_delimited | SBDB$BOOL |
| default_collation_id | SBDB$U32 |
| acl_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.tableinfo

| Column | Domain |
| --- | --- |
| table_id | SBDB$KEY_TABLE |
| schema_id | SBDB$KEY_SCHEMA |
| table_name | SBDB$NAME |
| name_is_delimited | SBDB$BOOL |
| owner_id | SBDB$KEY_USER |
| root_page | SBDB$PAGE_ID |
| column_count | SBDB$U32 |
| row_count | SBDB$U64 |
| table_type | SBDB$U32 |
| temp_metadata_scope | SBDB$U32 |
| temp_data_scope | SBDB$U32 |
| temp_on_commit | SBDB$U32 |
| creating_session_id | SBDB$KEY_SESSION |
| creating_transaction_id | SBDB$U64 |
| temp_parent_table_id | SBDB$KEY_TABLE |
| temp_schema_id | SBDB$KEY_SCHEMA |
| has_toast | SBDB$U32 |
| toast_table_id | SBDB$UUID_V7 |
| tablespace_id | SBDB$KEY_TABLESPACE |
| default_charset | SBDB$KEY_CHARSET |
| default_collation_id | SBDB$U32 |
| storage_params_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| policy_epoch | SBDB$U64 |
| migration_in_progress | SBDB$U32 |
| migration_id | SBDB$KEY_MIGRATION |
| migration_xid | SBDB$KEY_TXN |
| migration_target_ts | SBDB$U16 |
| migration_phase | SBDB$U8 |
| rls_enabled | SBDB$U32 |
| rls_forced | SBDB$U32 |

## sys.columnrecord

| Column | Domain |
| --- | --- |
| table_id | SBDB$KEY_TABLE |
| column_id | SBDB$KEY_COLUMN |
| column_name | SBDB$NAME |
| ordinal | SBDB$U16 |
| data_type | SBDB$U16 |
| type_precision | SBDB$U32 |
| type_scale | SBDB$U32 |
| max_length | SBDB$U32 |
| domain_id | SBDB$KEY_DOMAIN |
| is_array | SBDB$BOOL |
| array_size | SBDB$U32 |
| nullable | SBDB$U8 |
| has_default | SBDB$U8 |
| is_primary_key | SBDB$BOOL |
| is_unique | SBDB$BOOL |
| is_foreign_key | SBDB$BOOL |
| is_generated | SBDB$BOOL |
| storage_type | SBDB$U8 |
| with_timezone | SBDB$U8 |
| name_is_delimited | SBDB$BOOL |
| charset | SBDB$KEY_CHARSET |
| timezone_hint | SBDB$KEY_TIMEZONE |
| collation_id | SBDB$U32 |
| default_value | SBDB$NAME |
| default_value_oid | SBDB$LOB_REF |
| check_expr_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.indexinfo

| Column | Domain |
| --- | --- |
| index_id | SBDB$KEY_INDEX |
| table_id | SBDB$KEY_TABLE |
| index_name | SBDB$NAME |
| name_is_delimited | SBDB$BOOL |
| owner_id | SBDB$KEY_USER |
| root_page | SBDB$PAGE_ID |
| tablespace_id | SBDB$KEY_TABLESPACE |
| index_type | SBDB$U32 |
| is_unique | SBDB$U32 |
| index_params_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| collation_id | SBDB$U32 |
| rtree_max_entries | SBDB$U32 |
| is_expression_index | SBDB$U32 |
| is_partial_index | SBDB$U32 |
| expression_oid | SBDB$LOB_REF |
| predicate_oid | SBDB$LOB_REF |
| predicate_string | SBDB$U32 |
| dependency_id | SBDB$KEY_DEPENDENCY |
| logical_index_id | SBDB$KEY_INDEX |
| state | SBDB$U8 |
| valid_from_xid | SBDB$KEY_TXN |
| retired_xid | SBDB$KEY_TXN |
| build_started_time | SBDB$TIME_US |
| build_completed_time | SBDB$TIME_US |

## sys.indexversioninfo

| Column | Domain |
| --- | --- |
| logical_index_id | SBDB$KEY_INDEX |
| index_id | SBDB$KEY_INDEX |
| state | SBDB$U32 |
| valid_from_xid | SBDB$KEY_TXN |
| retired_xid | SBDB$KEY_TXN |
| build_started_time | SBDB$TIME_US |
| build_completed_time | SBDB$TIME_US |
| created_time | SBDB$TIME_US |

## sys.constraintinfo

| Column | Domain |
| --- | --- |
| constraint_id | SBDB$KEY_CONSTRAINT |
| constraint_name | SBDB$NAME |
| name_is_delimited | SBDB$BOOL |
| table_id | SBDB$KEY_TABLE |
| constraint_type | SBDB$U32 |
| check_expression | SBDB$U32 |
| check_expr_oid | SBDB$LOB_REF |
| referenced_table_id | SBDB$KEY_TABLE |
| on_delete | SBDB$U32 |
| on_update | SBDB$U32 |
| match_type | SBDB$U32 |
| exclusion_operator | SBDB$U32 |
| index_method | SBDB$U32 |
| is_deferrable | SBDB$U32 |
| initially_deferred | SBDB$U32 |
| is_enabled | SBDB$U32 |
| is_validated | SBDB$BOOL |
| is_system_generated | SBDB$U32 |
| owner_id | SBDB$KEY_USER |
| created_time | SBDB$TIME_US |
| validated_time | SBDB$TIME_US |

## sys.sequencerecord

| Column | Domain |
| --- | --- |
| sequence_id | SBDB$KEY_SEQUENCE |
| schema_id | SBDB$KEY_SCHEMA |
| sequence_name | SBDB$NAME |
| owner_id | SBDB$KEY_USER |
| owned_by_table_id | SBDB$KEY_TABLE |
| owned_by_column_id | SBDB$KEY_COLUMN |
| current_value | SBDB$I64 |
| increment_by | SBDB$I64 |
| min_value | SBDB$I64 |
| max_value | SBDB$I64 |
| start_value | SBDB$I64 |
| cache_size | SBDB$I64 |
| cycle | SBDB$U8 |
| name_is_delimited | SBDB$BOOL |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.viewrecord

| Column | Domain |
| --- | --- |
| view_id | SBDB$KEY_VIEW |
| schema_id | SBDB$KEY_SCHEMA |
| view_name | SBDB$NAME |
| owner_id | SBDB$KEY_USER |
| materialized_table_id | SBDB$KEY_TABLE |
| change_log_table_id | SBDB$KEY_TABLE |
| definition_oid | SBDB$LOB_REF |
| columns_oid | SBDB$LOB_REF |
| base_table_ids_oid | SBDB$LOB_REF |
| name_is_delimited | SBDB$BOOL |
| check_option | SBDB$U8 |
| is_materialized | SBDB$BOOL |
| refresh_strategy | SBDB$U8 |
| refresh_on_commit | SBDB$U8 |
| supports_concurrent | SBDB$U8 |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| last_refreshed | SBDB$U64 |
| is_valid | SBDB$BOOL |

## sys.triggerrecord

| Column | Domain |
| --- | --- |
| trigger_id | SBDB$KEY_TRIGGER |
| table_id | SBDB$KEY_TABLE |
| trigger_name | SBDB$NAME |
| owner_id | SBDB$KEY_USER |
| scope | SBDB$U8 |
| name_is_delimited | SBDB$BOOL |
| trigger_timing | SBDB$U8 |
| trigger_event | SBDB$U8 |
| granularity | SBDB$U8 |
| enabled | SBDB$U8 |
| position | SBDB$I32 |
| condition_oid | SBDB$LOB_REF |
| action_oid | SBDB$LOB_REF |
| old_alias_oid | SBDB$LOB_REF |
| new_alias_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.permissionrecord

| Column | Domain |
| --- | --- |
| permission_id | SBDB$KEY_PERMISSION |
| object_id | SBDB$KEY_OBJECT |
| object_type | SBDB$OBJTYPE |
| grantee_id | SBDB$KEY_PRINCIPAL |
| grantee_type | SBDB$U8 |
| privileges | SBDB$U32 |
| grant_option | SBDB$U8 |
| grantor_id | SBDB$KEY_USER |
| created_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.statisticrecord

| Column | Domain |
| --- | --- |
| statistic_id | SBDB$KEY_STATISTIC |
| table_id | SBDB$KEY_TABLE |
| column_id | SBDB$KEY_COLUMN |
| data_type | SBDB$U16 |
| num_rows | SBDB$U64 |
| num_nulls | SBDB$U64 |
| null_fraction | SBDB$F32 |
| num_distinct | SBDB$U64 |
| avg_width | SBDB$F32 |
| mcv_oid | SBDB$LOB_REF |
| histogram_oid | SBDB$LOB_REF |
| histogram_type | SBDB$U8 |
| histogram_bucket_count | SBDB$U32 |
| last_analyzed_time | SBDB$TIME_US |
| sample_size | SBDB$U64 |
| sample_rate | SBDB$F32 |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.timezonerecord

| Column | Domain |
| --- | --- |
| timezone_id | SBDB$KEY_TIMEZONE |
| name | SBDB$NAME |
| abbreviation | SBDB$NAME_64 |
| std_offset_minutes | SBDB$I32 |
| observes_dst | SBDB$BOOL |
| dst_start_month | SBDB$U8 |
| dst_start_week | SBDB$U8 |
| dst_start_day | SBDB$U8 |
| dst_start_hour | SBDB$U8 |
| dst_end_month | SBDB$U8 |
| dst_end_week | SBDB$U8 |
| dst_end_day | SBDB$U8 |
| dst_end_hour | SBDB$U8 |
| dst_offset_minutes | SBDB$I32 |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.charsetrecord

| Column | Domain |
| --- | --- |
| charset_id | SBDB$KEY_CHARSET |
| name | SBDB$NAME |
| description | SBDB$NAME |
| min_bytes | SBDB$U8 |
| max_bytes | SBDB$U8 |
| variable_width | SBDB$BOOL |
| default_collation_id | SBDB$U32 |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.collationrecord

| Column | Domain |
| --- | --- |
| collation_id | SBDB$U32 |
| name | SBDB$NAME |
| charset_id | SBDB$KEY_CHARSET |
| collation_type | SBDB$U8 |
| strength | SBDB$U8 |
| pad_space | SBDB$BOOL |
| is_default | SBDB$BOOL |
| locale | SBDB$NAME_64 |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.commentrecord

| Column | Domain |
| --- | --- |
| comment_id | SBDB$KEY_COMMENT |
| object_id | SBDB$KEY_OBJECT |
| object_type | SBDB$OBJTYPE |
| owner_id | SBDB$KEY_USER |
| comment_text_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.objectdefinitionrecord

| Column | Domain |
| --- | --- |
| object_id | SBDB$KEY_OBJECT |
| object_type | SBDB$OBJTYPE |
| ddl_text_oid | SBDB$LOB_REF |
| bytecode_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.userrecord

| Column | Domain |
| --- | --- |
| user_id | SBDB$KEY_USER |
| username | SBDB$NAME |
| password_hash_oid | SBDB$LOB_REF |
| user_metadata_oid | SBDB$LOB_REF |
| default_schema_id | SBDB$KEY_SCHEMA |
| is_active | SBDB$BOOL |
| is_superuser | SBDB$BOOL |
| created_time | SBDB$TIME_US |
| last_login_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.rolerecord

| Column | Domain |
| --- | --- |
| role_id | SBDB$KEY_ROLE |
| role_name | SBDB$NAME |
| owner_id | SBDB$KEY_USER |
| default_schema_id | SBDB$KEY_SCHEMA |
| role_metadata_oid | SBDB$LOB_REF |
| is_active | SBDB$BOOL |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.grouprecord

| Column | Domain |
| --- | --- |
| group_id | SBDB$KEY_GROUP |
| group_name | SBDB$NAME |
| external_id | SBDB$NAME_512 |
| group_type | SBDB$U8 |
| default_schema_id | SBDB$KEY_SCHEMA |
| group_metadata_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.rolemembershiprecord

| Column | Domain |
| --- | --- |
| membership_id | SBDB$KEY_MEMBERSHIP |
| user_id | SBDB$KEY_USER |
| role_id | SBDB$KEY_ROLE |
| granted_by | SBDB$KEY_USER |
| with_admin_option | SBDB$U8 |
| granted_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.groupmembershiprecord

| Column | Domain |
| --- | --- |
| membership_id | SBDB$KEY_MEMBERSHIP |
| user_id | SBDB$KEY_USER |
| member_type | SBDB$U8 |
| group_id | SBDB$KEY_GROUP |
| granted_by | SBDB$KEY_USER |
| granted_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.groupmappingrecord

| Column | Domain |
| --- | --- |
| mapping_id | SBDB$KEY_MAPPING |
| external_group_name | SBDB$NAME |
| auth_method | SBDB$U8 |
| auto_create_users | SBDB$U8 |
| internal_group_id | SBDB$UUID_V7 |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.authkeyrecord

| Column | Domain |
| --- | --- |
| authkey_id | SBDB$KEY_AUTHKEY |
| issuer | SBDB$NAME |
| valid_from | SBDB$TIME_US |
| valid_to | SBDB$TIME_US |
| usage_limit | SBDB$U32 |
| usage_count | SBDB$U32 |
| status | SBDB$U8 |
| usage_type | SBDB$U8 |
| role_scope_oid | SBDB$LOB_REF |
| group_scope_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.sessionrecord

| Column | Domain |
| --- | --- |
| session_id | SBDB$KEY_SESSION |
| user_id | SBDB$KEY_USER |
| authkey_id | SBDB$KEY_AUTHKEY |
| emulation_mode | SBDB$NAME_64 |
| login_time | SBDB$TIME_US |
| last_activity_time | SBDB$TIME_US |
| current_schema_id | SBDB$KEY_SCHEMA |
| policy_epoch_global | SBDB$U64 |
| policy_epoch_table | SBDB$U64 |
| is_expired | SBDB$BOOL |
| is_valid | SBDB$BOOL |

## sys.auditlogrecord

| Column | Domain |
| --- | --- |
| event_id | SBDB$U64 |
| timestamp | SBDB$TIME_US |
| event_type | SBDB$U16 |
| success | SBDB$BOOL |
| session_id | SBDB$KEY_SESSION |
| authkey_id | SBDB$KEY_AUTHKEY |
| user_id | SBDB$KEY_USER |
| role_id | SBDB$KEY_ROLE |
| target_user_id | SBDB$KEY_USER |
| object_id | SBDB$KEY_OBJECT |
| username | SBDB$NAME |
| target_username | SBDB$NAME |
| object_type | SBDB$OBJTYPE |
| object_name | SBDB$NAME |
| details_oid | SBDB$LOB_REF |
| hash_prev | SBDB$HASH256 |
| hash_curr | SBDB$HASH256 |
| is_valid | SBDB$BOOL |

## sys.securitypolicyepochrecord

| Column | Domain |
| --- | --- |
| global_epoch | SBDB$U64 |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.procedurerecord

| Column | Domain |
| --- | --- |
| procedure_id | SBDB$KEY_PROCEDURE |
| schema_id | SBDB$KEY_SCHEMA |
| procedure_name | SBDB$NAME |
| owner_id | SBDB$KEY_USER |
| procedure_type | SBDB$U8 |
| is_selectable | SBDB$BOOL |
| language | SBDB$U8 |
| sql_security | SBDB$U8 |
| name_is_delimited | SBDB$BOOL |
| body_redacted | SBDB$BOOL |
| deterministic | SBDB$BOOL |
| parameter_count | SBDB$U32 |
| return_type_oid | SBDB$LOB_REF |
| body_oid | SBDB$LOB_REF |
| bytecode_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.procedureparameterrecord

| Column | Domain |
| --- | --- |
| parameter_id | SBDB$KEY_PROC_PARAM |
| procedure_id | SBDB$KEY_PROCEDURE |
| parameter_name | SBDB$NAME |
| parameter_position | SBDB$U16 |
| parameter_mode | SBDB$U8 |
| data_type_oid | SBDB$LOB_REF |
| default_value_oid | SBDB$LOB_REF |
| is_valid | SBDB$BOOL |

## sys.domainrecord

| Column | Domain |
| --- | --- |
| domain_id | SBDB$KEY_DOMAIN |
| schema_id | SBDB$KEY_SCHEMA |
| domain_name | SBDB$NAME |
| domain_type | SBDB$U8 |
| base_type | SBDB$U16 |
| precision | SBDB$U32 |
| scale | SBDB$U32 |
| nullable | SBDB$U8 |
| default_value | SBDB$NAME |
| parent_domain_id | SBDB$UUID_V7 |
| is_valid | SBDB$BOOL |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| constraints_oid | SBDB$LOB_REF |
| fields_oid | SBDB$LOB_REF |
| enum_values_oid | SBDB$LOB_REF |
| security_oid | SBDB$LOB_REF |
| integrity_oid | SBDB$LOB_REF |
| validation_oid | SBDB$LOB_REF |
| quality_oid | SBDB$LOB_REF |
| set_element_type | SBDB$U16 |
| dialect_tag | SBDB$NAME_64 |
| compat_name | SBDB$NAME |

## sys.udrrecord

| Column | Domain |
| --- | --- |
| udr_id | SBDB$KEY_UDR |
| schema_id | SBDB$KEY_SCHEMA |
| udr_name | SBDB$NAME |
| owner_id | SBDB$KEY_USER |
| library_path | SBDB$NAME_1024 |
| entry_point | SBDB$NAME_1024 |
| udr_type | SBDB$U8 |
| name_is_delimited | SBDB$BOOL |
| signature_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.packagerecord

| Column | Domain |
| --- | --- |
| package_id | SBDB$KEY_PACKAGE |
| schema_id | SBDB$KEY_SCHEMA |
| package_name | SBDB$NAME |
| owner_id | SBDB$KEY_USER |
| package_header_oid | SBDB$LOB_REF |
| package_body_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |
| name_is_delimited | SBDB$BOOL |

## sys.exceptionrecord

| Column | Domain |
| --- | --- |
| exception_id | SBDB$KEY_EXCEPTION |
| schema_id | SBDB$KEY_SCHEMA |
| name | SBDB$NAME |
| message_oid | SBDB$LOB_REF |
| owner_id | SBDB$KEY_USER |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |
| name_is_delimited | SBDB$BOOL |

## sys.emulationtyperecord

| Column | Domain |
| --- | --- |
| emulation_type_id | SBDB$KEY_EMULATION_TYPE |
| emulation_name | SBDB$NAME |
| version_major | SBDB$U8 |
| version_minor | SBDB$U8 |
| mapping_rules_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.emulationserverrecord

| Column | Domain |
| --- | --- |
| server_id | SBDB$KEY_SERVER |
| server_name | SBDB$NAME |
| emulation_type_id | SBDB$KEY_EMULATION_TYPE |
| owner_id | SBDB$KEY_USER |
| server_config_oid | SBDB$LOB_REF |
| is_active | SBDB$BOOL |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.emulateddatabaserecord

| Column | Domain |
| --- | --- |
| emulated_db_id | SBDB$KEY_EMULATED_DB |
| database_name | SBDB$NAME |
| server_id | SBDB$KEY_SERVER |
| schema_id | SBDB$KEY_SCHEMA |
| owner_id | SBDB$KEY_USER |
| db_metadata_oid | SBDB$LOB_REF |
| is_active | SBDB$BOOL |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.synonymrecord

| Column | Domain |
| --- | --- |
| synonym_id | SBDB$KEY_SYNONYM |
| schema_id | SBDB$KEY_SCHEMA |
| synonym_name | SBDB$NAME |
| owner_id | SBDB$KEY_USER |
| target_path_oid | SBDB$LOB_REF |
| target_type | SBDB$U8 |
| is_public | SBDB$BOOL |
| name_is_delimited | SBDB$BOOL |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.foreignserverrecord

| Column | Domain |
| --- | --- |
| server_id | SBDB$KEY_SERVER |
| server_name | SBDB$NAME |
| server_type | SBDB$NAME |
| host | SBDB$NAME_512 |
| port | SBDB$U16 |
| connection_options_oid | SBDB$LOB_REF |
| owner_id | SBDB$KEY_USER |
| is_active | SBDB$BOOL |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.foreigntablerecord

| Column | Domain |
| --- | --- |
| foreign_table_id | SBDB$KEY_FOREIGN_TABLE |
| schema_id | SBDB$KEY_SCHEMA |
| table_name | SBDB$NAME |
| foreign_server_id | SBDB$KEY_FOREIGN_SERVER |
| remote_schema | SBDB$NAME |
| remote_table | SBDB$NAME |
| owner_id | SBDB$KEY_USER |
| column_mapping_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |
| name_is_delimited | SBDB$BOOL |

## sys.usermappingrecord

| Column | Domain |
| --- | --- |
| mapping_id | SBDB$KEY_MAPPING |
| user_id | SBDB$KEY_USER |
| foreign_server_id | SBDB$KEY_FOREIGN_SERVER |
| remote_user | SBDB$NAME_256 |
| remote_credentials_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.serverregistryrecord

| Column | Domain |
| --- | --- |
| server_id | SBDB$KEY_SERVER |
| server_name | SBDB$NAME |
| host | SBDB$NAME_512 |
| port | SBDB$U16 |
| role | SBDB$U8 |
| state | SBDB$U8 |
| last_heartbeat | SBDB$TIME_US |
| last_xid | SBDB$KEY_TXN |
| replication_lag_ms | SBDB$U64 |
| cluster_id | SBDB$NAME_256 |
| server_version | SBDB$NAME_256 |
| metadata_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.udrenginerecord

| Column | Domain |
| --- | --- |
| engine_id | SBDB$KEY_UDR_ENGINE |
| engine_name | SBDB$NAME |
| engine_type | SBDB$U8 |
| is_active | SBDB$BOOL |
| is_default | SBDB$BOOL |
| plugin_path | SBDB$NAME_1024 |
| config_oid | SBDB$LOB_REF |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.udrmodulerecord

| Column | Domain |
| --- | --- |
| module_id | SBDB$KEY_UDR_MODULE |
| module_name | SBDB$NAME |
| engine_id | SBDB$KEY_UDR_ENGINE |
| library_path | SBDB$NAME_1024 |
| checksum | SBDB$NAME_256 |
| entry_point | SBDB$NAME_1024 |
| dependencies_oid | SBDB$LOB_REF |
| is_loaded | SBDB$BOOL |
| is_validated | SBDB$BOOL |
| loaded_count | SBDB$U64 |
| created_time | SBDB$TIME_US |
| last_modified_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.foreignkeyrecord

| Column | Domain |
| --- | --- |
| fk_id | SBDB$KEY_FOREIGN_KEY |
| fk_name | SBDB$NAME |
| child_table_id | SBDB$UUID_V7 |
| parent_table_id | SBDB$UUID_V7 |
| child_columns | SBDB$NAME |
| parent_columns | SBDB$NAME |
| on_delete | SBDB$U8 |
| on_update | SBDB$U8 |
| match_type | SBDB$U8 |
| is_enabled | SBDB$BOOL |
| created_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

## sys.migrationhistoryinfo

| Column | Domain |
| --- | --- |
| history_id | SBDB$KEY_MIGRATION_HISTORY |
| migration_id | SBDB$KEY_MIGRATION |
| table_id | SBDB$KEY_TABLE |
| source_tablespace | SBDB$KEY_TABLESPACE |
| target_tablespace | SBDB$KEY_TABLESPACE |
| final_phase | SBDB$U32 |
| migration_xid | SBDB$KEY_TXN |
| total_pages | SBDB$U32 |
| pages_copied | SBDB$U32 |
| start_time | SBDB$TIME_US |
| end_time | SBDB$TIME_US |
| catch_up_iterations | SBDB$U32 |
| total_bytes_copied | SBDB$U64 |
| is_valid | SBDB$BOOL |

## sys.dormanttransactionrecord

| Column | Domain |
| --- | --- |
| dormant_id | SBDB$KEY_DORMANT_TXN |
| attachment_id | SBDB$KEY_ATTACHMENT |
| proc_id | SBDB$U32 |
| txn_id | SBDB$KEY_TXN |
| session_id | SBDB$KEY_SESSION |
| user_id | SBDB$KEY_USER |
| session_user_id | SBDB$KEY_USER |
| role_id | SBDB$KEY_ROLE |
| isolation_level | SBDB$U8 |
| access_mode | SBDB$U8 |
| wait_mode | SBDB$U8 |
| autocommit_mode | SBDB$U8 |
| lock_timeout_seconds | SBDB$U32 |
| current_schema_id | SBDB$KEY_SCHEMA |
| session_settings_oid | SBDB$LOB_REF |
| last_statement_oid | SBDB$LOB_REF |
| last_statement_hash | SBDB$U64 |
| last_statement_type | SBDB$U8 |
| last_statement_status | SBDB$U8 |
| state | SBDB$U8 |
| start_time | SBDB$TIME_US |
| last_activity_time | SBDB$TIME_US |
| dormant_since | SBDB$TIME_US |
| lease_expires_at | SBDB$TIME_US |
| last_statement_time | SBDB$TIME_US |
| last_rows_affected | SBDB$I64 |
| last_error_code | SBDB$U32 |
| last_sqlstate | SBDB$SQLSTATE |
| server_instance_id | SBDB$KEY_SERVER_INSTANCE |
| is_valid | SBDB$BOOL |

## sys.preparedtransactionrecord

| Column | Domain |
| --- | --- |
| prepared_id | SBDB$KEY_PREPARED_TXN |
| txn_id | SBDB$KEY_TXN |
| owner_id | SBDB$KEY_USER |
| database_id | SBDB$KEY_DATABASE |
| gid | SBDB$NAME_256 |
| prepared_time | SBDB$TIME_US |
| is_valid | SBDB$BOOL |

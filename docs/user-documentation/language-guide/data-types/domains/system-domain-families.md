# System Domain Families
Last modified: 2026-02-19

Back links:
- [Domains README](README.md)
- [Data Types README](../README.md)

Next in series:
- [Default Domain Bindings](default-domain-bindings.md)

Canonical system domain families in current engine inventory:
- `[sb_dom]`: 56 canonical rows
- `[sb_pg_dom]`: 25 canonical rows
- `[sb_my_dom]`: 13 canonical rows
- `[sb_cas_dom]`: 7 canonical rows
- `[sb_mongo_dom]`: 13 canonical rows
- `[sb_redis_dom]`: 8 canonical rows
- `[sb_mil_dom]`: 1 canonical row

Canonical names by family:

## [sb_dom]
- `[sb_dom]uuid_v7_internal`
- `[sb_dom]cat_identifier`
- `[sb_dom]cat_database_name`
- `[sb_dom]cat_schema_name`
- `[sb_dom]cat_object_name`
- `[sb_dom]cat_column_name`
- `[sb_dom]cat_index_name`
- `[sb_dom]cat_constraint_name`
- `[sb_dom]cat_role_name`
- `[sb_dom]cat_user_name`
- `[sb_dom]cat_language_code`
- `[sb_dom]cat_host_name`
- `[sb_dom]cat_service_name`
- `[sb_dom]cat_policy_name`
- `[sb_dom]cat_job_name`
- `[sb_dom]cat_schema_path`
- `[sb_dom]cat_file_path`
- `[sb_dom]cat_text`
- `[sb_dom]cat_comment_text`
- `[sb_dom]cat_sql_text`
- `[sb_dom]cat_blob_text`
- `[sb_dom]cat_blob_binary`
- `[sb_dom]cat_blob_blr`
- `[sb_dom]cat_json`
- `[sb_dom]cat_hash32`
- `[sb_dom]cat_bool`
- `[sb_dom]cat_int16`
- `[sb_dom]cat_int32`
- `[sb_dom]cat_int64`
- `[sb_dom]cat_uint8`
- `[sb_dom]cat_uint16`
- `[sb_dom]cat_uint32`
- `[sb_dom]cat_uint64`
- `[sb_dom]cat_f32`
- `[sb_dom]cat_f64`
- `[sb_dom]cat_txid`
- `[sb_dom]cat_count_u64`
- `[sb_dom]cat_bytes_u64`
- `[sb_dom]cat_percent_u8`
- `[sb_dom]cat_weight_u8`
- `[sb_dom]cat_priority_u8`
- `[sb_dom]cat_version_u64`
- `[sb_dom]cat_port_u16`
- `[sb_dom]cat_duration_ms`
- `[sb_dom]cat_interval_ms`
- `[sb_dom]cat_timestamp`
- `[sb_dom]cat_uuid`
- `[sb_dom]timestamp_ns`
- `[sb_dom]int256`
- `[sb_dom]uint256`
- `[sb_dom]decimal256`
- `[sb_dom]tagged_union`
- `[sb_dom]dict_encoded`
- `[sb_dom]completion_field`
- `[sb_dom]prefix_search_field`
- `[sb_dom]flat_object`

## [sb_pg_dom]
- `[sb_pg_dom]smallserial`
- `[sb_pg_dom]serial`
- `[sb_pg_dom]bigserial`
- `[sb_pg_dom]money`
- `[sb_pg_dom]name`
- `[sb_pg_dom]pg_lsn`
- `[sb_pg_dom]oid`
- `[sb_pg_dom]regclass`
- `[sb_pg_dom]regtype`
- `[sb_pg_dom]regproc`
- `[sb_pg_dom]regprocedure`
- `[sb_pg_dom]regoper`
- `[sb_pg_dom]regoperator`
- `[sb_pg_dom]regnamespace`
- `[sb_pg_dom]regrole`
- `[sb_pg_dom]regcollation`
- `[sb_pg_dom]regconfig`
- `[sb_pg_dom]regdictionary`
- `[sb_pg_dom]xid`
- `[sb_pg_dom]xid8`
- `[sb_pg_dom]line`
- `[sb_pg_dom]lseg`
- `[sb_pg_dom]box`
- `[sb_pg_dom]path`
- `[sb_pg_dom]circle`

## [sb_my_dom]
- `[sb_my_dom]mediumint_signed`
- `[sb_my_dom]mediumint_unsigned`
- `[sb_my_dom]bool`
- `[sb_my_dom]time_interval`
- `[sb_my_dom]year`
- `[sb_my_dom]tinytext`
- `[sb_my_dom]text`
- `[sb_my_dom]mediumtext`
- `[sb_my_dom]longtext`
- `[sb_my_dom]tinyblob`
- `[sb_my_dom]blob`
- `[sb_my_dom]mediumblob`
- `[sb_my_dom]longblob`

## [sb_cas_dom]
- `[sb_cas_dom]ascii`
- `[sb_cas_dom]varint`
- `[sb_cas_dom]decimal`
- `[sb_cas_dom]time`
- `[sb_cas_dom]duration`
- `[sb_cas_dom]timeuuid`
- `[sb_cas_dom]counter`

## [sb_mil_dom]
- `[sb_mil_dom]string`

## [sb_mongo_dom]
- `[sb_mongo_dom]objectid`
- `[sb_mongo_dom]timestamp`
- `[sb_mongo_dom]bin_data`
- `[sb_mongo_dom]regex`
- `[sb_mongo_dom]javascript`
- `[sb_mongo_dom]javascript_scope`
- `[sb_mongo_dom]minkey`
- `[sb_mongo_dom]maxkey`
- `[sb_mongo_dom]undefined`
- `[sb_mongo_dom]dbpointer`
- `[sb_mongo_dom]symbol`
- `[sb_mongo_dom]array`
- `[sb_mongo_dom]null`

## [sb_redis_dom]
- `[sb_redis_dom]list`
- `[sb_redis_dom]set`
- `[sb_redis_dom]zset`
- `[sb_redis_dom]hash`
- `[sb_redis_dom]stream`
- `[sb_redis_dom]geo`
- `[sb_redis_dom]hll`
- `[sb_redis_dom]bitmap`

Complete canonical + legacy inventory details are maintained in:
- `docs/audit/SYSTEM_DOMAIN_INDEX_CONTEXT_INVENTORY_BETA_0_1_0.md`

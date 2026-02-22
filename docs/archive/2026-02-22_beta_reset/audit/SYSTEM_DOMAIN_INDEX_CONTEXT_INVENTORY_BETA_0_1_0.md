# System Domain, Index, and Context Inventory (Beta 0.1.0)

- Audit date: 2026-02-19
- Scope: source-derived inventory for native v3 language closure checks

## 1. Sources

- Domain bootstrapping: `src/core/domain_manager.cpp`
- Canonical domain rows: `include/scratchbird/core/system_domain_registry_rows.inc`
- System default domain binding: `src/core/catalog_manager.cpp`
- Parser index method mapping: `src/parser/parser_v3.cpp`
- Catalog index canonical set: `src/core/catalog_manager.cpp`
- Parser context expressions: `src/parser/parser_v3.cpp`
- Runtime context evaluation: `src/sblr/executor.cpp`

## 2. Canonical System Domain Registry

Family counts (from `system_domain_registry_rows.inc`):

- [sb_cas_dom]: 7
- [sb_dom]: 56
- [sb_mil_dom]: 1
- [sb_mongo_dom]: 13
- [sb_my_dom]: 13
- [sb_pg_dom]: 25
- [sb_redis_dom]: 8

Canonical names by family:

### [sb_dom]

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

### [sb_pg_dom]

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

### [sb_my_dom]

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

### [sb_cas_dom]

- `[sb_cas_dom]ascii`
- `[sb_cas_dom]varint`
- `[sb_cas_dom]decimal`
- `[sb_cas_dom]time`
- `[sb_cas_dom]duration`
- `[sb_cas_dom]timeuuid`
- `[sb_cas_dom]counter`

### [sb_mil_dom]

- `[sb_mil_dom]string`

### [sb_mongo_dom]

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

### [sb_redis_dom]

- `[sb_redis_dom]list`
- `[sb_redis_dom]set`
- `[sb_redis_dom]zset`
- `[sb_redis_dom]hash`
- `[sb_redis_dom]stream`
- `[sb_redis_dom]geo`
- `[sb_redis_dom]hll`
- `[sb_redis_dom]bitmap`

## 3. Legacy System Domain Bootstrap List

Legacy bootstrap names (`kLegacySystemDomains` in `src/core/domain_manager.cpp`):

- `[sb_dom]UUID_V7`
- `[sb_dom]NAME`
- `[sb_dom]NAME_64`
- `[sb_dom]NAME_256`
- `[sb_dom]NAME_512`
- `[sb_dom]NAME_1024`
- `[sb_dom]BOOL`
- `[sb_dom]BIT`
- `[sb_dom]U8`
- `[sb_dom]U16`
- `[sb_dom]U32`
- `[sb_dom]U64`
- `[sb_dom]U128`
- `[sb_dom]I8`
- `[sb_dom]I16`
- `[sb_dom]I32`
- `[sb_dom]I64`
- `[sb_dom]I128`
- `[sb_dom]F32`
- `[sb_dom]F64`
- `[sb_dom]DECIMAL`
- `[sb_dom]MONEY`
- `[sb_dom]DECFLOAT16`
- `[sb_dom]DECFLOAT34`
- `[sb_dom]TIME_US`
- `[sb_dom]DATE`
- `[sb_dom]TIME`
- `[sb_dom]TIMESTAMP`
- `[sb_dom]TIMESTAMPTZ`
- `[sb_dom]TIME_TZ`
- `[sb_dom]INTERVAL`
- `[sb_dom]YEAR`
- `[sb_dom]SQLSTATE`
- `[sb_dom]HASH256`
- `[sb_dom]BINARY`
- `[sb_dom]VARBINARY`
- `[sb_dom]BLOB`
- `[sb_dom]BYTEA`
- `[sb_dom]TEXT`
- `[sb_dom]JSON`
- `[sb_dom]JSONB`
- `[sb_dom]XML`
- `[sb_dom]VECTOR`
- `[sb_dom]POINT`
- `[sb_dom]LINESTRING`
- `[sb_dom]POLYGON`
- `[sb_dom]MULTIPOINT`
- `[sb_dom]MULTILINESTRING`
- `[sb_dom]MULTIPOLYGON`
- `[sb_dom]GEOMETRYCOLLECTION`
- `[sb_dom]GEOMETRY`
- `[sb_dom]INET`
- `[sb_dom]CIDR`
- `[sb_dom]MACADDR`
- `[sb_dom]MACADDR8`
- `[sb_dom]TSVECTOR`
- `[sb_dom]TSQUERY`
- `[sb_dom]RANGE_INT4`
- `[sb_dom]RANGE_INT8`
- `[sb_dom]RANGE_NUM`
- `[sb_dom]RANGE_TS`
- `[sb_dom]RANGE_TSTZ`
- `[sb_dom]RANGE_DATE`
- `[sb_dom]ARRAY`
- `[sb_dom]COMPOSITE`
- `[sb_dom]DOMAIN`
- `[sb_dom]ROW`
- `[sb_dom]ENUM`
- `[sb_dom]SET`
- `[sb_dom]VARIANT`
- `[sb_dom]TIMESTAMP_NS`
- `[sb_dom]INT256`
- `[sb_dom]UINT256`
- `[sb_dom]DECIMAL256`
- `[sb_dom]TAGGED_UNION`
- `[sb_dom]DICT_ENCODED`
- `[sb_dom]COMPLETION_FIELD`
- `[sb_dom]PREFIX_SEARCH_FIELD`
- `[sb_dom]FLAT_OBJECT`
- `[sb_dom]PAGE_ID`
- `[sb_dom]LOB_REF`
- `[sb_dom]OBJTYPE`
- `[sb_dom]SCHEMA_TYPE`
- `[sb_dom]INDEX_TYPE`
- `[sb_dom]TABLE_TYPE`
- `[sb_dom]POLICY_TYPE`
- `[sb_dom]SECURITY_FLAGS`
- `[sb_dom]PERMISSIONS_MASK`

## 4. Default Domain Mapping For System Columns

Mappings from `defaultDomainForType(DataType)` in `src/core/catalog_manager.cpp`:

- `UUID` -> `"[sb_dom]UUID_V7"`
- `BOOLEAN` -> `"[sb_dom]BOOL"`
- `BIT` -> `"[sb_dom]BIT"`
- `INT8` -> `"[sb_dom]I8"`
- `INT16` -> `"[sb_dom]I16"`
- `UINT8` -> `"[sb_dom]U8"`
- `UINT16` -> `"[sb_dom]U16"`
- `UINT32` -> `"[sb_dom]U32"`
- `UINT64` -> `"[sb_dom]U64"`
- `UINT128` -> `"[sb_dom]U128"`
- `INT32` -> `"[sb_dom]I32"`
- `INT64` -> `"[sb_dom]I64"`
- `INT128` -> `"[sb_dom]I128"`
- `FLOAT32` -> `"[sb_dom]F32"`
- `FLOAT64` -> `"[sb_dom]F64"`
- `DECIMAL` -> `"[sb_dom]DECIMAL"`
- `MONEY` -> `"[sb_dom]MONEY"`
- `DECFLOAT16` -> `"[sb_dom]DECFLOAT16"`
- `DECFLOAT34` -> `"[sb_dom]DECFLOAT34"`
- `DATE` -> `"[sb_dom]DATE"`
- `TIME` -> `"[sb_dom]TIME"`
- `TIMESTAMP` -> `"[sb_dom]TIMESTAMP"`
- `TIMESTAMP_WITH_ZONE` -> `"[sb_dom]TIMESTAMPTZ"`
- `TIME_WITH_ZONE` -> `"[sb_dom]TIME_TZ"`
- `INTERVAL` -> `"[sb_dom]INTERVAL"`
- `YEAR` -> `"[sb_dom]YEAR"`
- `CHAR:`
- `VARCHAR:`
- `TEXT:`
- `BINARY:`
- `VARBINARY:`
- `BLOB:`
- `BYTEA:`
- `JSON` -> `"[sb_dom]JSON"`
- `JSONB` -> `"[sb_dom]JSONB"`
- `XML` -> `"[sb_dom]XML"`
- `VECTOR` -> `"[sb_dom]VECTOR"`
- `POINT` -> `"[sb_dom]POINT"`
- `LINESTRING` -> `"[sb_dom]LINESTRING"`
- `POLYGON` -> `"[sb_dom]POLYGON"`
- `MULTIPOINT` -> `"[sb_dom]MULTIPOINT"`
- `MULTILINESTRING` -> `"[sb_dom]MULTILINESTRING"`
- `MULTIPOLYGON` -> `"[sb_dom]MULTIPOLYGON"`
- `GEOMETRYCOLLECTION` -> `"[sb_dom]GEOMETRYCOLLECTION"`
- `GEOMETRY` -> `"[sb_dom]GEOMETRY"`
- `INET` -> `"[sb_dom]INET"`
- `CIDR` -> `"[sb_dom]CIDR"`
- `MACADDR` -> `"[sb_dom]MACADDR"`
- `MACADDR8` -> `"[sb_dom]MACADDR8"`
- `TSVECTOR` -> `"[sb_dom]TSVECTOR"`
- `TSQUERY` -> `"[sb_dom]TSQUERY"`
- `INT4RANGE` -> `"[sb_dom]RANGE_INT4"`
- `INT8RANGE` -> `"[sb_dom]RANGE_INT8"`
- `NUMRANGE` -> `"[sb_dom]RANGE_NUM"`
- `TSRANGE` -> `"[sb_dom]RANGE_TS"`
- `TSTZRANGE` -> `"[sb_dom]RANGE_TSTZ"`
- `DATERANGE` -> `"[sb_dom]RANGE_DATE"`
- `ARRAY` -> `"[sb_dom]ARRAY"`
- `COMPOSITE` -> `"[sb_dom]COMPOSITE"`
- `DOMAIN` -> `"[sb_dom]DOMAIN"`
- `ROW` -> `"[sb_dom]ROW"`
- `ENUM` -> `"[sb_dom]ENUM"`
- `SET` -> `"[sb_dom]SET"`
- `VARIANT` -> `"[sb_dom]VARIANT"`

Binding flow for system schemas:

- `CatalogManager::applySystemDomainDefaults(...)` uses table+column map (`kSystemDomainByTableColumn`) first.
- Then it uses column-name map (`kSystemDomainByColumn`).
- If neither applies, it falls back to `defaultDomainForType(...)`.
- If still unresolved, DDL fails deterministically.

## 5. Index Method Inventory

Parser-accepted index method tokens (`indexTypeFromName`):

- `BTREE`
- `HASH`
- `HNSW`
- `VECTOR`
- `FULLTEXT`
- `GIN`
- `GIST`
- `BRIN`
- `RTREE`
- `SPATIAL`
- `SPGIST`
- `SP-GIST`
- `BITMAP`
- `COLUMNSTORE`
- `LSM`
- `IVF`
- `ZONEMAP`
- `ZONE_MAP`
- `ART`
- `BLOOM`
- `VECTOR_FLAT`
- `VECTOR_BIN_FLAT`
- `IVF_FLAT`
- `BIN_IVF_FLAT`
- `IVF_PQ`
- `IVF_SQ8`
- `IVF_SQ8_HYBRID`
- `RHNSW_PQ`
- `RHNSW_SQ`
- `ANNOY`
- `NSG`
- `DISKANN`
- `SCANN`
- `GPU_CAGRA`
- `MINHASH_LSH`
- `SPARSE_INVERTED`
- `SPARSE_WAND`
- `TRIE`
- `INVERTED`
- `STL_SORT`
- `NGRAM`
- `MONGODB_2D`
- `MONGODB_2DSPHERE`
- `MONGODB_2DSPHERE_BUCKET`
- `MONGODB_GEO_HAYSTACK`
- `MONGODB_WILDCARD`
- `MONGODB_ENCRYPTED_RANGE`
- `NEO4J_LOOKUP`
- `NEO4J_TEXT`
- `NEO4J_RANGE`
- `NEO4J_POINT`
- `NEO4J_VECTOR`
- `CASSANDRA_SASI`
- `CASSANDRA_SAI`
- `REDIS_STRING`
- `REDIS_HASH`
- `REDIS_LIST`
- `REDIS_SET`
- `REDIS_ZSET`
- `REDIS_STREAM`
- `REDIS_BITMAP`
- `REDIS_HLL`
- `REDIS_GEO`

- Total parser tokens: 63

Catalog canonical index type names (`isValidCanonicalIndexTypeName`):

- `BTREE`
- `HASH`
- `GIN`
- `GIST`
- `SPGIST`
- `BRIN`
- `FULLTEXT`
- `SPATIAL`
- `BITMAP`
- `COLUMNSTORE`
- `LSM`
- `HNSW`
- `IVF`
- `ART`
- `BLOOM`
- `VECTOR_FLAT`
- `VECTOR_BIN_FLAT`
- `IVF_FLAT`
- `BIN_IVF_FLAT`
- `IVF_PQ`
- `IVF_SQ8`
- `IVF_SQ8_HYBRID`
- `RHNSW_PQ`
- `RHNSW_SQ`
- `ANNOY`
- `NSG`
- `DISKANN`
- `SCANN`
- `GPU_CAGRA`
- `MINHASH_LSH`
- `SPARSE_INVERTED`
- `SPARSE_WAND`
- `TRIE`
- `INVERTED`
- `STL_SORT`
- `NGRAM`
- `MONGODB_2D`
- `MONGODB_2DSPHERE`
- `MONGODB_2DSPHERE_BUCKET`
- `MONGODB_GEO_HAYSTACK`
- `MONGODB_WILDCARD`
- `MONGODB_ENCRYPTED_RANGE`
- `NEO4J_LOOKUP`
- `NEO4J_TEXT`
- `NEO4J_RANGE`
- `NEO4J_POINT`
- `NEO4J_VECTOR`
- `CASSANDRA_SASI`
- `CASSANDRA_SAI`
- `REDIS_STRING`
- `REDIS_HASH`
- `REDIS_LIST`
- `REDIS_SET`
- `REDIS_ZSET`
- `REDIS_STREAM`
- `REDIS_BITMAP`
- `REDIS_HLL`
- `REDIS_GEO`

- Total canonical names: 58

Alias-only parser spellings that normalize to canonical names:

- `VECTOR` -> `HNSW`
- `SPATIAL` -> `RTREE`
- `SP-GIST` -> `SPGIST`
- `ZONE_MAP` -> `ZONEMAP`

## 6. Context Variable Inventory

Bare context keywords parsed as function expressions in native v3:

- `CURRENT_USER`
- `SESSION_USER`
- `CURRENT_ROLE`
- `CURRENT_CONNECTION`
- `CURRENT_SESSION` (normalized to `CURRENT_CONNECTION`)
- `CURRENT_TRANSACTION`
- `NOW`
- `CURRENT_DATE`
- `CURRENT_TIME`
- `CURRENT_TIMESTAMP`

Emitter opcode mapping (native v3):

- `NOW` -> `SBLR3_FUNC_NOW` (wall-clock at evaluation time)
- `CURRENT_TIMESTAMP` -> `SBLR3_FUNC_NOW` with CURRENT_TIMESTAMP semantic flag (transaction-start anchored when active transaction is present)
- `CURRENT_DATE` -> `SBLR3_FUNC_CURRENT_DATE`
- `CURRENT_TIME` -> `SBLR3_FUNC_CURRENT_TIME`
- `CURRENT_USER` / `SESSION_USER` -> `SBLR3_FUNC_CURRENT_USER`
- `CURRENT_ROLE` -> `SBLR3_FUNC_CURRENT_ROLE`
- `CURRENT_CONNECTION` / `CURRENT_SESSION` -> `SBLR3_FUNC_CURRENT_CONNECTION`
- `CURRENT_TRANSACTION` -> `SBLR3_FUNC_CURRENT_TRANSACTION`

Runtime executor handling in v3 expression evaluation:

- Current wall-clock/date/time values: `NOW`, `CURRENT_DATE`, `CURRENT_TIME`.
- Connection/session identity: `CURRENT_CONNECTION`.
- User/role identity: `CURRENT_USER`, `CURRENT_ROLE` (null when unavailable).
- Transaction identity: `CURRENT_TRANSACTION` (null when not in active transaction).

Trigger row context surfaces in executor:

- `TriggerContext::getOldValue(column)` maps `OLD.<column>` semantics.
- `TriggerContext::getNewValue(column)` maps `NEW.<column>` semantics.
- `StatementTriggerContext` exposes OLD TABLE / NEW TABLE transition-table accessors.

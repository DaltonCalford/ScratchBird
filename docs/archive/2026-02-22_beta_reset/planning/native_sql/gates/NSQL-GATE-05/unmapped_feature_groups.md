### Cassandra (7)
- `CSQ-003` (`P1`, `NFG_CSQ-003`, `index_vector_search_surface`): `STORAGE_ATTACHED_INDEXING_SAI`
- `CSQ-004` (`P1`, `NFG_CSQ-004`, `index_vector_search_surface`): `VECTOR_TYPE_AND_SEARCH`
- `CSQ-005` (`P1`, `NFG_CSQ-005`, `command_surface`): `CHANGE_DATA_CAPTURE`
- `CSQ-006` (`P1`, `NFG_CSQ-006`, `command_surface`): `COMPACTION_STRATEGY_CONTROLS`
- `CSQ-007` (`P1`, `NFG_CSQ-007`, `extensibility_surface`): `UDF_UDA_RUNTIME`
- `CSX-101` (`P2`, `NFG_CSX-101`, `extensibility_surface`): `STARGATE_API_GATEWAY`
- `CSX-102` (`P2`, `NFG_CSX-102`, `extensibility_surface`): `K8SSANDRA_OPERATOR_WORKFLOWS`

### ClickHouse (6)
- `CHK-003` (`P1`, `NFG_CHK-003`, `command_surface`): `ALTER_UPDATE_DELETE_MUTATIONS`
- `CHK-004` (`P1`, `NFG_CHK-004`, `index_vector_search_surface`): `DICTIONARIES`
- `CHK-005` (`P1`, `NFG_CHK-005`, `index_vector_search_surface`): `PROJECTIONS_AND_SKIPPING_INDEXES`
- `CHK-006` (`P1`, `NFG_CHK-006`, `command_surface`): `MERGETREE_VARIANTS_AND_TTL_DEPTH`
- `CHK-007` (`P1`, `NFG_CHK-007`, `security_surface`): `SETTINGS_PROFILES_AND_QUOTAS_FULL`
- `CHKX-101` (`P2`, `NFG_CHKX-101`, `extensibility_surface`): `KAFKA_TABLE_ENGINE`

### DuckDB (6)
- `DKD-001` (`P0`, `NFG_DKD-001`, `datatype_surface`): `BIGNUM_TYPE`
- `DKD-004` (`P1`, `NFG_DKD-004`, `security_surface`): `SECRETS_MANAGER_PARITY`
- `DKD-005` (`P1`, `NFG_DKD-005`, `command_surface`): `MACRO_AND_UDF_SURFACE`
- `DKD-006` (`P1`, `NFG_DKD-006`, `connector_surface`): `EXTENSION_DRIVEN_CONNECTORS`
- `DKD-007` (`P1`, `NFG_DKD-007`, `command_surface`): `PARQUET_AND_FILE_SCAN_PARITY`
- `DKDX-101` (`P2`, `NFG_DKDX-101`, `extensibility_surface`): `EXTENSION_ECOSYSTEM_BASELINE`

### FirebirdSQL (12)
- `FBQ-001` (`P0`, `NFG_FBQ-001`, `command_surface`): `WITH_RECURSIVE_CTE`
- `FBQ-010` (`P0`, `NFG_FBQ-010`, `command_surface`): `RDB_DB_KEY`
- `FBQ-011` (`P0`, `NFG_FBQ-011`, `command_surface`): `EXECUTE_PROCEDURE_STMT`
- `FBQ-019` (`P0`, `NFG_FBQ-019`, `command_surface`): `SUSPEND`
- `FBQ-020` (`P0`, `NFG_FBQ-020`, `command_surface`): `AUTONOMOUS_TRANSACTION_BLOCK`
- `FBQ-022` (`P0`, `NFG_FBQ-022`, `command_surface`): `POST_EVENT`
- `FBQ-025` (`P0`, `NFG_FBQ-025`, `extensibility_surface`): `UDR_EXTERNAL_ROUTINES`
- `FBQ-026` (`P0`, `NFG_FBQ-026`, `security_surface`): `TRIGGER_SQL_SECURITY`
- `FBQ-009` (`P1`, `NFG_FBQ-009`, `command_surface`): `RETURNING_OLD_NEW`
- `FBQ-012` (`P1`, `NFG_FBQ-012`, `command_surface`): `TEMP_TABLE_SQL_COMPAT`
- `FBQ-016` (`P1`, `NFG_FBQ-016`, `command_surface`): `PACKAGE_BODY`
- `FBQ-018` (`P1`, `NFG_FBQ-018`, `command_surface`): `EXECUTE_BLOCK`

### InfluxDB (6)
- `IFX-001` (`P0`, `NFG_IFX-001`, `command_surface`): `DISTINCT_VALUE_CACHE_ADMIN`
- `IFX-002` (`P0`, `NFG_IFX-002`, `command_surface`): `TASK_AND_SCRIPT_RUNTIME`
- `IFX-003` (`P1`, `NFG_IFX-003`, `command_surface`): `INFLUXQL_FULL_PARITY`
- `IFX-004` (`P1`, `NFG_IFX-004`, `command_surface`): `SQL_DIALECT_PARITY_FOR_INFLUX`
- `IFX-005` (`P1`, `NFG_IFX-005`, `security_surface`): `TOKEN_SCOPE_DETAIL_PARITY`
- `IFX-006` (`P2`, `NFG_IFX-006`, `extensibility_surface`): `TELEGRAF_ECOSYSTEM_INTEGRATION`

### MariaDB (9)
- `MDB-001` (`P0`, `NFG_MDB-001`, `streaming_replication_surface`): `GTID_REPLICATION_CONTROL`
- `MDB-003` (`P0`, `NFG_MDB-003`, `command_surface`): `PLUGGABLE_STORAGE_ENGINES`
- `MDB-002` (`P1`, `NFG_MDB-002`, `command_surface`): `GALERA_WSREP_SURFACE`
- `MDB-005` (`P1`, `NFG_MDB-005`, `command_surface`): `CONNECT_ENGINE`
- `MDB-006` (`P1`, `NFG_MDB-006`, `datatype_surface`): `DYNAMIC_COLUMNS`
- `MDB-007` (`P1`, `NFG_MDB-007`, `datatype_surface`): `INVISIBLE_COLUMNS`
- `MDB-008` (`P1`, `NFG_MDB-008`, `security_surface`): `AUTH_PLUGIN_PARITY`
- `MDB-004` (`P2`, `NFG_MDB-004`, `command_surface`): `SPIDER_ENGINE`
- `MDBX-101` (`P2`, `NFG_MDBX-101`, `extensibility_surface`): `MAXSCALE_ROUTING`

### Milvus (7)
- `MVQ-002` (`P1`, `NFG_MVQ-002`, `index_vector_search_surface`): `HYBRID_SEARCH_DENSE_SPARSE`
- `MVQ-003` (`P1`, `NFG_MVQ-003`, `command_surface`): `PARTITION_KEY_AND_CLUSTERING`
- `MVQ-004` (`P1`, `NFG_MVQ-004`, `runtime_surface`): `CONSISTENCY_LEVELS`
- `MVQ-005` (`P1`, `NFG_MVQ-005`, `security_surface`): `RBAC_PRIVILEGES`
- `MVQ-006` (`P1`, `NFG_MVQ-006`, `command_surface`): `SNAPSHOT_BACKUP_RESTORE`
- `MVX-101` (`P2`, `NFG_MVX-101`, `extensibility_surface`): `ATTU_ADMIN_UI`
- `MVX-102` (`P2`, `NFG_MVX-102`, `extensibility_surface`): `MILVUS_OPERATOR`

### MongoDB (9)
- `MGQ-001` (`P0`, `NFG_MGQ-001`, `command_surface`): `AGGREGATION_LOOKUP_AND_GRAPHLOOKUP`
- `MGQ-003` (`P0`, `NFG_MGQ-003`, `streaming_replication_surface`): `CHANGE_STREAMS`
- `MGQ-004` (`P0`, `NFG_MGQ-004`, `command_surface`): `SHARDED_MULTI_DOCUMENT_TRANSACTIONS`
- `MGQ-002` (`P1`, `NFG_MGQ-002`, `command_surface`): `SET_WINDOW_FIELDS_STAGE`
- `MGQ-005` (`P1`, `NFG_MGQ-005`, `command_surface`): `TIME_SERIES_COLLECTIONS`
- `MGQ-006` (`P1`, `NFG_MGQ-006`, `security_surface`): `QUERYABLE_ENCRYPTION`
- `MGQ-007` (`P1`, `NFG_MGQ-007`, `command_surface`): `RESHARD_COLLECTION_AND_ZONE_SHARDING`
- `MGX-101` (`P2`, `NFG_MGX-101`, `extensibility_surface`): `ATLAS_SEARCH`
- `MGX-102` (`P2`, `NFG_MGX-102`, `extensibility_surface`): `ATLAS_VECTOR_SEARCH`

### MySQL (32)
- `MYA-001` (`P0`, `NFG_MYA-001`, `command_surface`): `SHOW_OPERATIONAL_DIAGNOSTICS`
- `MYA-002` (`P0`, `NFG_MYA-002`, `streaming_replication_surface`): `REPLICA_CONTROL_SQL`
- `MYA-003` (`P0`, `NFG_MYA-003`, `runtime_surface`): `XA_SQL`
- `MYA-004` (`P0`, `NFG_MYA-004`, `extensibility_surface`): `PLUGIN_COMPONENT_LIFECYCLE`
- `MYD-002` (`P0`, `NFG_MYD-002`, `command_surface`): `GENERATED_COLUMNS`
- `MYD-003` (`P0`, `NFG_MYD-003`, `command_surface`): `INVISIBLE_COLUMNS_INDEXES`
- `MYD-007` (`P0`, `NFG_MYD-007`, `security_surface`): `ACCOUNT_MANAGEMENT_DDL`
- `MYD-008` (`P0`, `NFG_MYD-008`, `security_surface`): `ROLE_ACTIVATION_CONTROL`
- `MYQ-001` (`P0`, `NFG_MYQ-001`, `command_surface`): `WITH_RECURSIVE_CTE`
- `MYQ-003` (`P0`, `NFG_MYQ-003`, `command_surface`): `INSERT_IGNORE`
- `MYQ-004` (`P0`, `NFG_MYQ-004`, `command_surface`): `REPLACE_STATEMENT`
- `MYQ-005` (`P0`, `NFG_MYQ-005`, `command_surface`): `LOAD_DATA_INFILE`
- `MYQ-006` (`P0`, `NFG_MYQ-006`, `command_surface`): `LOCK_TABLES_UNLOCK_TABLES`
- `MYQ-007` (`P0`, `NFG_MYQ-007`, `command_surface`): `HANDLER_STATEMENTS`
- `MYQ-008` (`P0`, `NFG_MYQ-008`, `command_surface`): `INDEX_HINTS`
- `MYQ-009` (`P0`, `NFG_MYQ-009`, `command_surface`): `SELECT_MODIFIERS`
- `MYQ-010` (`P0`, `NFG_MYQ-010`, `command_surface`): `SELECT_INTO_VARIANTS`
- `MYQ-011` (`P0`, `NFG_MYQ-011`, `command_surface`): `WITH_ROLLUP`
- `MYQ-012` (`P0`, `NFG_MYQ-012`, `command_surface`): `JSON_TABLE`
- `MYQ-014` (`P0`, `NFG_MYQ-014`, `command_surface`): `CALL_STATEMENT`
- `MYD-001` (`P1`, `NFG_MYD-001`, `command_surface`): `AUTO_INCREMENT_ATTRIBUTE`
- `MYD-004` (`P1`, `NFG_MYD-004`, `command_surface`): `CREATE_VIEW_OPTIONS`
- `MYD-005` (`P1`, `NFG_MYD-005`, `command_surface`): `TRIGGER_ORDER_AND_BODY`
- `MYD-006` (`P1`, `NFG_MYD-006`, `command_surface`): `EVENT_SCHEDULER_FULL_SURFACE`
- `MYQ-002` (`P1`, `NFG_MYQ-002`, `command_surface`): `INSERT_ON_DUPLICATE_KEY_UPDATE`
- `MYQ-013` (`P1`, `NFG_MYQ-013`, `command_surface`): `PARTITION_SELECTION`
- `MYQ-015` (`P1`, `NFG_MYQ-015`, `command_surface`): `STORED_PROGRAM_DIAGNOSTICS`
- `MYX-101` (`P1`, `NFG_MYX-101`, `extensibility_surface`): `VITESS`
- `MYX-102` (`P1`, `NFG_MYX-102`, `extensibility_surface`): `PROXYSQL`
- `MYX-103` (`P1`, `NFG_MYX-103`, `extensibility_surface`): `ORCHESTRATOR`
- `MYX-104` (`P1`, `NFG_MYX-104`, `extensibility_surface`): `GH_OST`
- `MYX-105` (`P1`, `NFG_MYX-105`, `extensibility_surface`): `PERCONA_TOOLKIT`

### Neo4j (7)
- `N4Q-002` (`P1`, `NFG_N4Q-002`, `command_surface`): `CALL_SUBQUERIES_IN_TRANSACTIONS`
- `N4Q-003` (`P1`, `NFG_N4Q-003`, `index_vector_search_surface`): `VECTOR_INDEXES`
- `N4Q-004` (`P1`, `NFG_N4Q-004`, `index_vector_search_surface`): `FULLTEXT_INDEXES_AND_PROCS`
- `N4Q-006` (`P1`, `NFG_N4Q-006`, `security_surface`): `FINE_GRAINED_GRAPH_AUTHZ`
- `N4Q-005` (`P2`, `NFG_N4Q-005`, `command_surface`): `COMPOSITE_DATABASES_FABRIC`
- `N4X-101` (`P2`, `NFG_N4X-101`, `extensibility_surface`): `APOC_LIBRARY`
- `N4X-102` (`P2`, `NFG_N4X-102`, `extensibility_surface`): `GRAPH_DATA_SCIENCE_LIBRARY`

### OpenSearch (6)
- `OPS-003` (`P1`, `NFG_OPS-003`, `command_surface`): `INGEST_PIPELINES`
- `OPS-004` (`P1`, `NFG_OPS-004`, `command_surface`): `INDEX_STATE_MANAGEMENT`
- `OPS-005` (`P1`, `NFG_OPS-005`, `index_vector_search_surface`): `ANALYZER_AND_TOKENIZER_CONFIG`
- `OPS-007` (`P1`, `NFG_OPS-007`, `security_surface`): `DLS_FLS_ADVANCED_CLAUSES`
- `OPS-006` (`P2`, `NFG_OPS-006`, `index_vector_search_surface`): `SEARCH_PIPELINES`
- `OPSX-101` (`P2`, `NFG_OPSX-101`, `extensibility_surface`): `ANOMALY_DETECTION_PLUGIN`

### PostgreSQL (27)
- `PGQ-001` (`P0`, `NFG_PGQ-001`, `command_surface`): `WITH_RECURSIVE_SEARCH_CYCLE`
- `PGQ-002` (`P0`, `NFG_PGQ-002`, `command_surface`): `DISTINCT_ON`
- `PGQ-003` (`P0`, `NFG_PGQ-003`, `command_surface`): `LATERAL_WITH_ORDINALITY`
- `PGQ-004` (`P0`, `NFG_PGQ-004`, `command_surface`): `TABLESAMPLE`
- `PGQ-005` (`P0`, `NFG_PGQ-005`, `command_surface`): `GROUPING_SETS_ROLLUP_CUBE`
- `PGQ-006` (`P0`, `NFG_PGQ-006`, `command_surface`): `AGGREGATE_FILTER`
- `PGQ-007` (`P0`, `NFG_PGQ-007`, `command_surface`): `WINDOW_EXCLUDE`
- `PGQ-010` (`P0`, `NFG_PGQ-010`, `command_surface`): `DEFERRABLE_CONSTRAINTS`
- `PGQ-011` (`P0`, `NFG_PGQ-011`, `command_surface`): `EXCLUDE_USING`
- `PGQ-012` (`P0`, `NFG_PGQ-012`, `command_surface`): `GENERATED_COLUMNS`
- `PGX-101` (`P0`, `NFG_PGX-101`, `command_surface`): `ORAFCE`
- `PGX-107` (`P0`, `NFG_PGX-107`, `command_surface`): `PGAUDIT`
- `PGX-109` (`P0`, `NFG_PGX-109`, `command_surface`): `PGLOGICAL`
- `PGX-111` (`P0`, `NFG_PGX-111`, `command_surface`): `PG_TRGM`
- `PGX-113` (`P0`, `NFG_PGX-113`, `command_surface`): `TABLEFUNC`
- `PGX-114` (`P0`, `NFG_PGX-114`, `command_surface`): `PG_STAT_STATEMENTS`
- `PGQ-008` (`P1`, `NFG_PGQ-008`, `command_surface`): `ROW_LOCK_STRENGTHS`
- `PGQ-009` (`P1`, `NFG_PGQ-009`, `command_surface`): `FETCH_WITH_TIES`
- `PGQ-015` (`P1`, `NFG_PGQ-015`, `extensibility_surface`): `ALTER_SYSTEM`
- `PGX-102` (`P1`, `NFG_PGX-102`, `command_surface`): `POSTGIS`
- `PGX-103` (`P1`, `NFG_PGX-103`, `command_surface`): `TIMESCALEDB`
- `PGX-104` (`P1`, `NFG_PGX-104`, `command_surface`): `CITUS`
- `PGX-105` (`P1`, `NFG_PGX-105`, `command_surface`): `PGVECTOR`
- `PGX-106` (`P1`, `NFG_PGX-106`, `command_surface`): `PG_CRON`
- `PGX-108` (`P1`, `NFG_PGX-108`, `command_surface`): `PG_PARTMAN`
- `PGX-110` (`P1`, `NFG_PGX-110`, `command_surface`): `HSTORE`
- `PGX-112` (`P1`, `NFG_PGX-112`, `command_surface`): `CITEXT`

### Redis (7)
- `RDQ-002` (`P1`, `NFG_RDQ-002`, `extensibility_surface`): `REDIS_FUNCTIONS`
- `RDQ-004` (`P1`, `NFG_RDQ-004`, `security_surface`): `ACL_USERS_AND_CATEGORIES`
- `RDQ-005` (`P1`, `NFG_RDQ-005`, `extensibility_surface`): `MODULE_LIFECYCLE`
- `RDX-101` (`P2`, `NFG_RDX-101`, `extensibility_surface`): `REDISEARCH`
- `RDX-102` (`P2`, `NFG_RDX-102`, `extensibility_surface`): `REDISJSON`
- `RDX-103` (`P2`, `NFG_RDX-103`, `extensibility_surface`): `REDISTIMESERIES`
- `RDX-104` (`P2`, `NFG_RDX-104`, `extensibility_surface`): `REDISBLOOM`


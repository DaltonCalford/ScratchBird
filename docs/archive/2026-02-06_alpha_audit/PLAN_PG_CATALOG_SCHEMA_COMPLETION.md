# Plan: PostgreSQL + MySQL + Firebird Catalog Schema Completion

**Last Updated:** 2026-02-03

## Goal
Ensure all PostgreSQL, MySQL, and Firebird system catalog tables exposed by
ScratchBird match the upstream column layouts for the latest stable releases
and are present as schema-only views where full runtime data is not yet
implemented. This plan is documentation-only and intended to drive future
implementation work.

## Target Versions (Latest Stable)
- PostgreSQL: 16.x
- MySQL: 8.0.x
- Firebird: 5.0.x

## Scope
- PostgreSQL: pg_catalog tables + pg_stat views + pg_tables/pg_views
- MySQL: information_schema, mysql.*, performance_schema tables
- Firebird: RDB$ system tables + MON$ monitoring tables

## PostgreSQL Workstreams

### PG-WS1: Schema-Only Coverage for Missing pg_catalog Tables
Goal: Expose schema-only views (correct columns, all NULL/0) for all missing
pg_catalog tables.

Per-table checklist:
- [ ] pg_am
- [ ] pg_amop
- [ ] pg_amproc
- [ ] pg_attrdef
- [ ] pg_attribute (expand to full column list)
- [ ] pg_auth_members
- [ ] pg_cast
- [ ] pg_class (expand to full column list)
- [ ] pg_collation
- [ ] pg_constraint (expand to full column list)
- [ ] pg_conversion
- [ ] pg_database (expand to full column list)
- [ ] pg_depend
- [ ] pg_description
- [ ] pg_enum (expand to full column list if missing)
- [ ] pg_event_trigger
- [ ] pg_extension
- [ ] pg_foreign_data_wrapper
- [ ] pg_foreign_server
- [ ] pg_foreign_table
- [ ] pg_index (expand to full column list)
- [ ] pg_inherits (schema-only rows, full column list)
- [ ] pg_language
- [ ] pg_largeobject
- [ ] pg_largeobject_metadata
- [ ] pg_namespace (expand to full column list)
- [ ] pg_opclass
- [ ] pg_operator
- [ ] pg_opfamily
- [ ] pg_partitioned_table
- [ ] pg_policy
- [ ] pg_proc (expand to full column list)
- [ ] pg_publication
- [ ] pg_publication_rel
- [ ] pg_range
- [ ] pg_replication_origin
- [ ] pg_rewrite
- [ ] pg_roles (expand to full column list)
- [ ] pg_sequences
- [ ] pg_shdepend
- [ ] pg_shdescription
- [ ] pg_statistic
- [ ] pg_statistic_ext
- [ ] pg_statistic_ext_data
- [ ] pg_subscription
- [ ] pg_subscription_rel
- [ ] pg_tablespace (expand to full column list)
- [ ] pg_transform
- [ ] pg_trigger (expand to full column list)
- [ ] pg_ts_config
- [ ] pg_ts_config_map
- [ ] pg_ts_dict
- [ ] pg_ts_parser
- [ ] pg_ts_template
- [ ] pg_type (expand to full column list)

### PG-WS2: pg_stat Schema-Only Coverage
Goal: Expose schema-only views for missing pg_stat_* tables with correct columns.

Per-table checklist:
- [ ] pg_stat_activity (expand to full column list)
- [ ] pg_stat_all_tables (expand to full column list)
- [ ] pg_stat_user_tables (expand to full column list)
- [ ] pg_stat_sys_tables (expand to full column list)
- [ ] pg_stat_all_indexes
- [ ] pg_stat_user_indexes
- [ ] pg_stat_sys_indexes
- [ ] pg_stat_all_functions
- [ ] pg_stat_user_functions
- [ ] pg_stat_archiver
- [ ] pg_stat_bgwriter (expand to full column list)
- [ ] pg_stat_database (expand to full column list)
- [ ] pg_stat_database_conflicts
- [ ] pg_stat_progress_vacuum
- [ ] pg_stat_progress_analyze
- [ ] pg_stat_progress_basebackup
- [ ] pg_stat_progress_cluster
- [ ] pg_stat_progress_copy
- [ ] pg_stat_progress_create_index
- [ ] pg_stat_progress_create_stats
- [ ] pg_stat_replication
- [ ] pg_stat_replication_slots
- [ ] pg_stat_slru
- [ ] pg_stat_ssl
- [ ] pg_stat_subscription
- [ ] pg_stat_wal
- [ ] pg_stat_wal_receiver

### PG-WS3: Emulated View Parity
Goal: Ensure pg_tables/pg_views match PostgreSQL column layouts.

Per-table checklist:
- [ ] pg_tables (add columns: tableowner, tablespace, hasindexes, hasrules, hastriggers, rowsecurity)
- [ ] pg_views (add columns: viewowner, definition, check_option, security_barrier)

### PG-WS4: Documentation Sync
Goal: Keep strict per-column status/source tables in the wiki.

Checklist:
- [ ] Update `wiki/content/language-guides/postgresql/13_system_catalog.md` after each catalog parity change.
- [ ] Add strict per-column status tables for newly added pg_catalog/pg_stat tables.

## MySQL Workstreams

### MY-WS1: Schema-Only Coverage for information_schema Tables
Goal: Expose all MySQL information_schema tables with full column layouts and
NULL/0 defaults for untracked values.

Per-table checklist (information_schema):
- [ ] APPLICABLE_ROLES
- [ ] CHARACTER_SETS
- [ ] CHECK_CONSTRAINTS
- [ ] COLLATIONS
- [ ] COLLATION_CHARACTER_SET_APPLICABILITY
- [ ] COLUMNS
- [ ] COLUMN_STATISTICS
- [ ] ENABLED_ROLES
- [ ] EVENTS
- [ ] FILES
- [ ] INNODB_DATAFILES
- [ ] INNODB_FIELDS
- [ ] INNODB_FOREIGN
- [ ] INNODB_FOREIGN_COLS
- [ ] INNODB_TABLESPACES_BRIEF
- [ ] KEYWORDS
- [ ] KEY_COLUMN_USAGE
- [ ] PARAMETERS
- [ ] PARTITIONS
- [ ] REFERENTIAL_CONSTRAINTS
- [ ] RESOURCE_GROUPS
- [ ] ROLE_COLUMN_GRANTS
- [ ] ROLE_ROUTINE_GRANTS
- [ ] ROLE_TABLE_GRANTS
- [ ] ROUTINES
- [ ] SCHEMATA
- [ ] SCHEMATA_EXTENSIONS
- [ ] STATISTICS
- [ ] ST_GEOMETRY_COLUMNS
- [ ] ST_SPATIAL_REFERENCE_SYSTEMS
- [ ] ST_UNITS_OF_MEASURE
- [ ] TABLES
- [ ] TABLE_CONSTRAINTS
- [ ] TRIGGERS
- [ ] USER_ATTRIBUTES
- [ ] VIEWS
- [ ] VIEW_ROUTINE_USAGE
- [ ] VIEW_TABLE_USAGE

### MY-WS2: Schema-Only Coverage for mysql.* Tables
Goal: Expose all mysql schema tables with full column layouts and NULL/0 defaults.

Per-table checklist (mysql schema):
- [ ] columns_priv
- [ ] component
- [ ] db
- [ ] default_roles
- [ ] engine_cost
- [ ] func
- [ ] general_log
- [ ] global_grants
- [ ] gtid_executed
- [ ] help_category
- [ ] help_keyword
- [ ] help_relation
- [ ] help_topic
- [ ] ndb_binlog_index
- [ ] password_history
- [ ] plugin
- [ ] procs_priv
- [ ] proxies_priv
- [ ] replication_asynchronous_connection_failover
- [ ] replication_asynchronous_connection_failover_managed
- [ ] replication_group_configuration_version
- [ ] replication_group_member_actions
- [ ] role_edges
- [ ] server_cost
- [ ] servers
- [ ] slave_master_info
- [ ] slave_relay_log_info
- [ ] slave_worker_info
- [ ] slow_log
- [ ] tables_priv
- [ ] time_zone
- [ ] time_zone_leap_second
- [ ] time_zone_name
- [ ] time_zone_transition
- [ ] time_zone_transition_type
- [ ] user

### MY-WS3: Schema-Only Coverage for performance_schema Tables
Goal: Expose all performance_schema tables with full column layouts and
NULL/0 defaults for untracked values.

Per-table checklist (performance_schema):
- [ ] accounts
- [ ] cond_instances
- [ ] data_lock_waits
- [ ] data_locks
- [ ] error_log
- [ ] events_errors_summary_by_account_by_error
- [ ] events_errors_summary_by_host_by_error
- [ ] events_errors_summary_by_thread_by_error
- [ ] events_errors_summary_by_user_by_error
- [ ] events_errors_summary_global_by_error
- [ ] events_stages_current
- [ ] events_stages_history
- [ ] events_stages_history_long
- [ ] events_stages_summary_by_account_by_event_name
- [ ] events_stages_summary_by_host_by_event_name
- [ ] events_stages_summary_by_thread_by_event_name
- [ ] events_stages_summary_by_user_by_event_name
- [ ] events_stages_summary_global_by_event_name
- [ ] events_statements_current
- [ ] events_statements_histogram_by_digest
- [ ] events_statements_histogram_global
- [ ] events_statements_history
- [ ] events_statements_history_long
- [ ] events_statements_summary_by_account_by_event_name
- [ ] events_statements_summary_by_digest
- [ ] events_statements_summary_by_host_by_event_name
- [ ] events_statements_summary_by_program
- [ ] events_statements_summary_by_thread_by_event_name
- [ ] events_statements_summary_by_user_by_event_name
- [ ] events_statements_summary_global_by_event_name
- [ ] events_transactions_current
- [ ] events_transactions_history
- [ ] events_transactions_history_long
- [ ] events_transactions_summary_by_account_by_event_name
- [ ] events_transactions_summary_by_host_by_event_name
- [ ] events_transactions_summary_by_thread_by_event_name
- [ ] events_transactions_summary_by_user_by_event_name
- [ ] events_transactions_summary_global_by_event_name
- [ ] events_waits_current
- [ ] events_waits_history
- [ ] events_waits_history_long
- [ ] events_waits_summary_by_account_by_event_name
- [ ] events_waits_summary_by_host_by_event_name
- [ ] events_waits_summary_by_instance
- [ ] events_waits_summary_by_thread_by_event_name
- [ ] events_waits_summary_by_user_by_event_name
- [ ] events_waits_summary_global_by_event_name
- [ ] file_instances
- [ ] file_summary_by_event_name
- [ ] file_summary_by_instance
- [ ] global_status
- [ ] global_variables
- [ ] host_cache
- [ ] hosts
- [ ] keyring_component_status
- [ ] keyring_keys
- [ ] log_status
- [ ] memory_summary_by_account_by_event_name
- [ ] memory_summary_by_host_by_event_name
- [ ] memory_summary_by_thread_by_event_name
- [ ] memory_summary_by_user_by_event_name
- [ ] memory_summary_global_by_event_name
- [ ] metadata_locks
- [ ] mutex_instances
- [ ] objects_summary_global_by_type
- [ ] performance_timers
- [ ] persisted_variables
- [ ] prepared_statements_instances
- [ ] processlist
- [ ] replication_applier_configuration
- [ ] replication_applier_filters
- [ ] replication_applier_global_filters
- [ ] replication_applier_status
- [ ] replication_applier_status_by_coordinator
- [ ] replication_applier_status_by_worker
- [ ] replication_asynchronous_connection_failover
- [ ] replication_connection_configuration
- [ ] replication_connection_status
- [ ] replication_group_member_stats
- [ ] replication_group_members
- [ ] rwlock_instances
- [ ] session_account_connect_attrs
- [ ] session_connect_attrs
- [ ] session_status
- [ ] session_variables
- [ ] setup_actors
- [ ] setup_consumers
- [ ] setup_instruments
- [ ] setup_objects
- [ ] setup_threads
- [ ] socket_instances
- [ ] socket_summary_by_event_name
- [ ] socket_summary_by_instance
- [ ] status_by_account
- [ ] status_by_host
- [ ] status_by_thread
- [ ] status_by_user
- [ ] table_handles
- [ ] table_io_waits_summary_by_index_usage
- [ ] table_io_waits_summary_by_table
- [ ] table_lock_waits_summary_by_table
- [ ] threads
- [ ] user_defined_functions
- [ ] user_variables_by_thread
- [ ] users
- [ ] variables_by_thread
- [ ] variables_info

### MY-WS4: Documentation Sync
Goal: Keep strict per-column status/source tables in the wiki.

Checklist:
- [ ] Update `wiki/content/language-guides/mysql/13_system_catalog.md` after each catalog parity change.
- [ ] Update `wiki/content/language-guides/mysql/performance_schema.md` after each table is exposed/expanded.
- [ ] Add strict per-column status tables for all information_schema and mysql.* tables.

## Firebird Workstreams

### FB-WS1: RDB$ System Tables (Firebird 5.0)
Goal: Expose all Firebird RDB$ system tables with full column layouts and
NULL/0 defaults for untracked values.

Per-table checklist (RDB$):
- [ ] RDB$DATABASE
- [ ] RDB$RELATIONS
- [ ] RDB$RELATION_FIELDS
- [ ] RDB$FIELDS
- [ ] RDB$FIELD_DIMENSIONS
- [ ] RDB$TYPES
- [ ] RDB$CHARACTER_SETS
- [ ] RDB$COLLATIONS
- [ ] RDB$INDICES
- [ ] RDB$INDEX_SEGMENTS
- [ ] RDB$RELATION_CONSTRAINTS
- [ ] RDB$CHECK_CONSTRAINTS
- [ ] RDB$REF_CONSTRAINTS
- [ ] RDB$TRIGGERS
- [ ] RDB$TRIGGER_MESSAGES
- [ ] RDB$PROCEDURES
- [ ] RDB$PROCEDURE_PARAMETERS
- [ ] RDB$FUNCTIONS
- [ ] RDB$FUNCTION_ARGUMENTS
- [ ] RDB$PACKAGES
- [ ] RDB$PACKAGE_BODIES
- [ ] RDB$EXCEPTIONS
- [ ] RDB$GENERATORS
- [ ] RDB$USER_PRIVILEGES
- [ ] RDB$DEPENDENCIES
- [ ] RDB$VIEW_RELATIONS
- [ ] RDB$ROLES
- [ ] RDB$FILES
- [ ] RDB$LOG_FILES
- [ ] RDB$FILTERS
- [ ] RDB$FORMATS
- [ ] RDB$BACKUP_HISTORY

### FB-WS2: MON$ Monitoring Tables (Firebird 5.0)
Goal: Expose all MON$ monitoring tables with full column layouts and
NULL/0 defaults for untracked values.

Per-table checklist (MON$):
- [ ] MON$DATABASE
- [ ] MON$ATTACHMENTS
- [ ] MON$TRANSACTIONS
- [ ] MON$STATEMENTS
- [ ] MON$CALL_STACK
- [ ] MON$IO_STATS
- [ ] MON$RECORD_STATS
- [ ] MON$TABLE_STATS
- [ ] MON$INDEX_STATS
- [ ] MON$MEMORY_USAGE
- [ ] MON$CONTEXT_VARIABLES

### FB-WS3: Documentation Sync
Goal: Keep strict per-column status/source tables in the wiki.

Checklist:
- [ ] Update `wiki/content/language-guides/firebirdsql/13_system_catalog.md` after each catalog parity change.
- [ ] Add strict per-column status tables for all RDB$/MON$ tables.

## Acceptance Criteria
- All PostgreSQL/MySQL/Firebird catalog tables expected by upstream clients are discoverable.
- `SELECT * FROM <catalog>` returns correct column order and types.
- Untracked fields are consistently NULL/0 as documented.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*

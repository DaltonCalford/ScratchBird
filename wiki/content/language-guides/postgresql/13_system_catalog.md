# System Catalog

**Last Updated:** 2026-02-03

---

ScratchBird exposes PostgreSQL system catalogs through a combination of
pg_catalog virtual tables and emulated views. Every catalog table listed here
includes strict per-column status and source mapping.

Statuses:
- ScratchBird tracked: populated from a ScratchBird runtime source.
- Always NULL: column exists but is never populated.
- Always 0: column is always returned as 0.

---

## pg_catalog.pg_namespace

Table status: Implemented
Source: PgCatalogHandler::queryPgNamespace

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `oid` | ScratchBird tracked | oidFromUuid(schema.schema_id) or builtin constants | Row per schema; pg_catalog/info_schema added if missing. |
| `nspname` | ScratchBird tracked | schemaName(schema) | Row per schema. |
| `nspowner` | ScratchBird tracked | schema.owner_id | NULL when owner_id is zero. |
| `nspacl` | Always NULL | Not populated | Never. |

## pg_catalog.pg_class

Table status: Implemented
Source: PgCatalogHandler::queryPgClass

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `oid` | ScratchBird tracked | oidFromUuid(table/view/index/sequence/domain id) | Row per table, view, index, sequence, record domain. |
| `relname` | ScratchBird tracked | object name | Row per table/view/index/sequence/domain. |
| `relnamespace` | ScratchBird tracked | schema oid | Row per object. |
| `relkind` | ScratchBird tracked | pgRelKind/table-type constants | Row per object. |
| `relowner` | ScratchBird tracked | owner_id | NULL when owner_id is zero. |
| `reltablespace` | ScratchBird tracked | tablespace oid mapping | NULL when tablespace_id is unset or object has no tablespace. |
| `reltuples` | ScratchBird tracked | table.row_count or constant 0 | Tables use row_count; indexes/views/sequences/domains emit 0. |
| `relpages` | Always 0 | Constant 0 | Always. |
| `relnatts` | ScratchBird tracked | column_count or index column count | Tables use column_count; indexes/views/domains use size of column list. |
| `relhasindex` | ScratchBird tracked | indexes.empty() | True when table has indexes; false for other relkinds. |
| `relisshared` | Always 0 | Constant false | Always. |
| `relpersistence` | ScratchBird tracked | pgRelPersistence(table_type) or constant 'p' | Tables use persistence mapping; others use 'p'. |
| `reloptions` | Always NULL | Not populated | Never. |

## pg_catalog.pg_attribute

Table status: Implemented
Source: PgCatalogHandler::queryPgAttribute

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `attrelid` | ScratchBird tracked | oidFromUuid(table_id or record domain id) | Row per column or record domain field. |
| `attname` | ScratchBird tracked | column/field name | Row per column or record domain field. |
| `atttypid` | ScratchBird tracked | domain oid or pgBuiltinTypeOid | Domain columns use domain id; otherwise builtin type OID. |
| `attnum` | ScratchBird tracked | column.ordinal or field position | Row per column or field. |
| `attnotnull` | ScratchBird tracked | !column.nullable | Row per column or field. |
| `attisdropped` | Always 0 | Constant false | Always. |
| `atttypmod` | ScratchBird tracked | Constant -1 | Always. |

## pg_catalog.pg_type

Table status: Implemented
Source: PgCatalogHandler::queryPgType

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `oid` | ScratchBird tracked | pgBuiltinTypeOid or oidFromUuid(domain_id) | Row per builtin type and domain. |
| `typname` | ScratchBird tracked | pgBuiltinTypeName or domain.domain_name | Row per builtin type and domain. |
| `typnamespace` | ScratchBird tracked | kPgCatalogOid or domain.schema_id | Builtin types use pg_catalog; domains use their schema. |
| `typowner` | Always NULL | Not populated | Never. |
| `typlen` | ScratchBird tracked | pgTypeLen or -1 | Builtin/basic domains use pgTypeLen; others use -1. |
| `typbyval` | ScratchBird tracked | pgTypeByVal or false | Builtin/basic domains use pgTypeByVal; others false. |
| `typtype` | ScratchBird tracked | 'b' builtin; domain type mapping | Domain type sets d/e/c/p. |
| `typcategory` | ScratchBird tracked | pgTypeCategory or domain mapping | Domain types map to category codes. |
| `typrelid` | ScratchBird tracked | record domain relid or 0 | Record domains use domain id; others 0. |
| `typelem` | Always 0 | Constant 0 | Always. |
| `typarray` | Always 0 | Constant 0 | Always. |
| `typbasetype` | ScratchBird tracked | pgBuiltinTypeOid(base_type) or 0 | Basic domains map to base type OID; others 0. |
| `typnotnull` | ScratchBird tracked | !domain.nullable or false | Domains use nullable flag; builtin types false. |

## pg_catalog.pg_enum

Table status: Implemented
Source: PgCatalogHandler::queryPgEnum

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `enumtypid` | ScratchBird tracked | oidFromUuid(domain.domain_id) | Row per enum value. |
| `enumsortorder` | ScratchBird tracked | enum value position | Row per enum value. |
| `enumlabel` | ScratchBird tracked | enum value label | Row per enum value. |

## pg_catalog.pg_proc

Table status: Implemented
Source: PgCatalogHandler::queryPgProc

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `oid` | ScratchBird tracked | oidFromUuid(function_id/procedure_id) | Row per function/procedure. |
| `proname` | ScratchBird tracked | function/procedure name | Row per function/procedure. |
| `pronamespace` | ScratchBird tracked | schema oid | Row per function/procedure. |
| `proowner` | ScratchBird tracked | owner_id | NULL when owner_id is zero. |
| `prorettype` | ScratchBird tracked | pgBuiltinTypeOid(return_type) or kPgVoidOid | Functions map return type; procedures use void. |
| `prokind` | ScratchBird tracked | 'f' for functions, 'p' for procedures | Row per function/procedure. |
| `proargtypes` | ScratchBird tracked | formatArgTypes(parameters) | Space-separated type OIDs for IN parameters; empty when none. |

## pg_catalog.pg_trigger

Table status: Implemented
Source: PgCatalogHandler::queryPgTrigger

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `oid` | ScratchBird tracked | oidFromUuid(trigger_id) | Row per trigger. |
| `tgname` | ScratchBird tracked | trigger.trigger_name | Row per trigger. |
| `tgrelid` | ScratchBird tracked | oidFromUuid(trigger.table_id) | Row per trigger. |
| `tgenabled` | ScratchBird tracked | 'O' or 'D' | 'O' when enabled; 'D' when disabled. |

## pg_catalog.pg_constraint

Table status: Implemented
Source: PgCatalogHandler::queryPgConstraint

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `oid` | ScratchBird tracked | oidFromUuid(constraint_id) | Row per constraint. |
| `conname` | ScratchBird tracked | constraint.constraint_name | Row per constraint. |
| `connamespace` | ScratchBird tracked | schema oid | Row per constraint. |
| `conrelid` | ScratchBird tracked | table oid | Row per constraint. |
| `contype` | ScratchBird tracked | pgConstraintType(constraint_type) | Row per constraint. |
| `condeferrable` | ScratchBird tracked | constraint.is_deferrable | Row per constraint. |
| `condeferred` | ScratchBird tracked | constraint.initially_deferred | Row per constraint. |
| `confrelid` | Always 0 | Constant 0 | Always. |

## pg_catalog.pg_index

Table status: Implemented
Source: PgCatalogHandler::queryPgIndex

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `indexrelid` | ScratchBird tracked | oidFromUuid(index.index_id) | Row per index. |
| `indrelid` | ScratchBird tracked | oidFromUuid(table.table_id) | Row per index. |
| `indisunique` | ScratchBird tracked | index.is_unique | Row per index. |
| `indisprimary` | Always 0 | Constant false | Always. |
| `indisvalid` | ScratchBird tracked | index.state == ACTIVE | Row per index. |
| `indkey` | ScratchBird tracked | Space-separated column ordinals | NULL if no columns found. |

## pg_catalog.pg_roles

Table status: Implemented
Source: PgCatalogHandler::queryPgRoles

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `oid` | ScratchBird tracked | oidFromUuid(user_id/role_id) | Row per user/role. |
| `rolname` | ScratchBird tracked | user.username or role.role_name | Row per user/role. |
| `rolsuper` | ScratchBird tracked | user.is_superuser or false | True for superusers; false for roles. |
| `rolcanlogin` | ScratchBird tracked | true for users; false for roles | True for users; false for roles. |
| `rolcreaterole` | Always 0 | Constant false | Always. |
| `rolcreatedb` | Always 0 | Constant false | Always. |
| `rolreplication` | Always 0 | Constant false | Always. |
| `rolbypassrls` | ScratchBird tracked | user.is_superuser or false | True for superusers; false for roles. |

## pg_catalog.pg_authid

Table status: Implemented
Source: PgCatalogHandler::queryPgAuthid

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `oid` | ScratchBird tracked | oidFromUuid(user_id/role_id) | Row per user/role. |
| `rolname` | ScratchBird tracked | user.username or role.role_name | Row per user/role. |
| `rolsuper` | ScratchBird tracked | user.is_superuser or false | True for superusers; false for roles. |
| `rolcanlogin` | ScratchBird tracked | true for users; false for roles | True for users; false for roles. |
| `rolcreaterole` | Always 0 | Constant false | Always. |
| `rolcreatedb` | Always 0 | Constant false | Always. |
| `rolreplication` | Always 0 | Constant false | Always. |
| `rolbypassrls` | ScratchBird tracked | user.is_superuser or false | True for superusers; false for roles. |
| `rolpassword` | ScratchBird tracked | user.password_hash or NULL | Users return password hash when present; roles NULL. |

## pg_catalog.pg_database

Table status: Implemented
Source: PgCatalogHandler::queryPgDatabase

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `oid` | ScratchBird tracked | Constant 1 | Always. |
| `datname` | ScratchBird tracked | Constant 'scratchbird' | Always. |
| `datdba` | Always 0 | Constant 0 | Always. |
| `encoding` | ScratchBird tracked | Constant 6 | Always (UTF-8 encoding id). |

## pg_catalog.pg_tablespace

Table status: Implemented
Source: PgCatalogHandler::queryPgTablespace

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `oid` | ScratchBird tracked | oidFromUuid(tablespace_uuid) or default constant | Row per tablespace; adds pg_default if missing. |
| `spcname` | ScratchBird tracked | tablespace_name or 'pg_default' | Row per tablespace. |
| `spcowner` | Always 0 | Constant 0 | Always. |

## pg_catalog.pg_settings

Table status: Schema-only (no rows)
Source: PgCatalogHandler::queryPgSettings

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `name` | Always NULL | Not populated | Never (no rows emitted). |
| `setting` | Always NULL | Not populated | Never (no rows emitted). |

## pg_catalog.pg_inherits

Table status: Schema-only (no rows)
Source: PgCatalogHandler::queryPgInherits

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `inhrelid` | Always NULL | Not populated | Never (no rows emitted). |
| `inhparent` | Always NULL | Not populated | Never (no rows emitted). |
| `inhseqno` | Always NULL | Not populated | Never (no rows emitted). |

## pg_catalog.pg_locks

Table status: Implemented
Source: PgCatalogHandler::queryPgLocks

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `locktype` | ScratchBird tracked | pgLockTypeName(lock.tag.target_type) | Row per lock. |
| `database` | ScratchBird tracked | Constant 1 | Always. |
| `relation` | ScratchBird tracked | oidFromUuid(lock.tag.object_uuid) | Populated for relation/page/tuple locks; NULL otherwise. |
| `page` | ScratchBird tracked | lock.tag.page_num | Populated for page/tuple locks; NULL otherwise. |
| `tuple` | ScratchBird tracked | lock.tag.offset_num | Populated for tuple locks; NULL otherwise. |
| `virtualxid` | Always NULL | Not populated | Never. |
| `transactionid` | Always NULL | Not populated | Never. |
| `classid` | Always NULL | Not populated | Never. |
| `objid` | Always NULL | Not populated | Never. |
| `objsubid` | Always NULL | Not populated | Never. |
| `pid` | ScratchBird tracked | backend pid from ProcArrayManager | Populated when backend pid is known; NULL otherwise. |
| `mode` | ScratchBird tracked | pgLockModeName(lock.mode) | Row per lock. |
| `granted` | ScratchBird tracked | lock.granted | Row per lock. |

## pg_catalog.pg_stat_activity

Table status: Implemented
Source: PgCatalogHandler::queryPgStatActivity

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `datid` | ScratchBird tracked | Constant 1 | Always. |
| `datname` | ScratchBird tracked | Constant 'scratchbird' | Always. |
| `pid` | ScratchBird tracked | backend.backend_pid | NULL when backend pid is zero. |
| `leader_pid` | Always NULL | Not populated | Never. |
| `usesysid` | ScratchBird tracked | session.user_id | NULL when session lookup fails. |
| `usename` | ScratchBird tracked | session.username | NULL when session lookup fails. |
| `application_name` | Always NULL | Not populated | Never. |
| `client_addr` | Always NULL | Not populated | Never. |
| `client_hostname` | Always NULL | Not populated | Never. |
| `client_port` | Always NULL | Not populated | Never. |
| `backend_start` | ScratchBird tracked | backend.start_time | NULL when start_time is zero. |
| `xact_start` | ScratchBird tracked | backend.xact_start_time | NULL when not in transaction. |
| `query_start` | ScratchBird tracked | backend.query_start_time | NULL when idle. |
| `state_change` | ScratchBird tracked | backend.state_change_time or start_time | Always when backend snapshot exists. |
| `wait_event_type` | Always NULL | Not populated | Never. |
| `wait_event` | Always NULL | Not populated | Never. |
| `state` | ScratchBird tracked | Derived from backend query/txn state | active/idle/idle in transaction. |
| `backend_xid` | ScratchBird tracked | backend.xid | NULL when xid is zero. |
| `backend_xmin` | ScratchBird tracked | backend.backend_xmin | NULL when backend_xmin is zero. |
| `query_id` | Always NULL | Not populated | Never. |
| `query` | ScratchBird tracked | backend.query_text | NULL when empty. |
| `backend_type` | ScratchBird tracked | Constant 'client backend' | Always. |

## pg_catalog.pg_stat_user_tables

Table status: Implemented
Source: PgCatalogHandler::queryPgStatTables

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `relid` | ScratchBird tracked | oidFromUuid(table.table_id) | Row per table in scope. |
| `schemaname` | ScratchBird tracked | schemaName(schema) | Row per table in scope. |
| `relname` | ScratchBird tracked | table.table_name | Row per table in scope. |
| `seq_scan` | ScratchBird tracked | TableStatsSnapshot.seq_scan_count | 0 when no stats snapshot. |
| `seq_tup_read` | ScratchBird tracked | TableStatsSnapshot.seq_rows_read | 0 when no stats snapshot. |
| `idx_scan` | ScratchBird tracked | TableStatsSnapshot.idx_scan_count | 0 when no stats snapshot. |
| `idx_tup_fetch` | ScratchBird tracked | TableStatsSnapshot.idx_rows_fetch | 0 when no stats snapshot. |
| `n_tup_ins` | ScratchBird tracked | TableStatsSnapshot.rows_inserted | 0 when no stats snapshot. |
| `n_tup_upd` | ScratchBird tracked | TableStatsSnapshot.rows_updated | 0 when no stats snapshot. |
| `n_tup_del` | ScratchBird tracked | TableStatsSnapshot.rows_deleted | 0 when no stats snapshot. |
| `n_live_tup` | ScratchBird tracked | TableStatsSnapshot.live_rows_estimate or table.row_count | Uses live_rows_estimate when available; row_count otherwise. |
| `n_dead_tup` | ScratchBird tracked | TableStatsSnapshot.dead_rows_estimate | 0 when no stats snapshot. |

## pg_catalog.pg_stat_all_tables

Table status: Implemented
Source: PgCatalogHandler::queryPgStatTables

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `relid` | ScratchBird tracked | oidFromUuid(table.table_id) | Row per table in scope. |
| `schemaname` | ScratchBird tracked | schemaName(schema) | Row per table in scope. |
| `relname` | ScratchBird tracked | table.table_name | Row per table in scope. |
| `seq_scan` | ScratchBird tracked | TableStatsSnapshot.seq_scan_count | 0 when no stats snapshot. |
| `seq_tup_read` | ScratchBird tracked | TableStatsSnapshot.seq_rows_read | 0 when no stats snapshot. |
| `idx_scan` | ScratchBird tracked | TableStatsSnapshot.idx_scan_count | 0 when no stats snapshot. |
| `idx_tup_fetch` | ScratchBird tracked | TableStatsSnapshot.idx_rows_fetch | 0 when no stats snapshot. |
| `n_tup_ins` | ScratchBird tracked | TableStatsSnapshot.rows_inserted | 0 when no stats snapshot. |
| `n_tup_upd` | ScratchBird tracked | TableStatsSnapshot.rows_updated | 0 when no stats snapshot. |
| `n_tup_del` | ScratchBird tracked | TableStatsSnapshot.rows_deleted | 0 when no stats snapshot. |
| `n_live_tup` | ScratchBird tracked | TableStatsSnapshot.live_rows_estimate or table.row_count | Uses live_rows_estimate when available; row_count otherwise. |
| `n_dead_tup` | ScratchBird tracked | TableStatsSnapshot.dead_rows_estimate | 0 when no stats snapshot. |

## pg_catalog.pg_stat_sys_tables

Table status: Implemented
Source: PgCatalogHandler::queryPgStatTables

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `relid` | ScratchBird tracked | oidFromUuid(table.table_id) | Row per table in scope. |
| `schemaname` | ScratchBird tracked | schemaName(schema) | Row per table in scope. |
| `relname` | ScratchBird tracked | table.table_name | Row per table in scope. |
| `seq_scan` | ScratchBird tracked | TableStatsSnapshot.seq_scan_count | 0 when no stats snapshot. |
| `seq_tup_read` | ScratchBird tracked | TableStatsSnapshot.seq_rows_read | 0 when no stats snapshot. |
| `idx_scan` | ScratchBird tracked | TableStatsSnapshot.idx_scan_count | 0 when no stats snapshot. |
| `idx_tup_fetch` | ScratchBird tracked | TableStatsSnapshot.idx_rows_fetch | 0 when no stats snapshot. |
| `n_tup_ins` | ScratchBird tracked | TableStatsSnapshot.rows_inserted | 0 when no stats snapshot. |
| `n_tup_upd` | ScratchBird tracked | TableStatsSnapshot.rows_updated | 0 when no stats snapshot. |
| `n_tup_del` | ScratchBird tracked | TableStatsSnapshot.rows_deleted | 0 when no stats snapshot. |
| `n_live_tup` | ScratchBird tracked | TableStatsSnapshot.live_rows_estimate or table.row_count | Uses live_rows_estimate when available; row_count otherwise. |
| `n_dead_tup` | ScratchBird tracked | TableStatsSnapshot.dead_rows_estimate | 0 when no stats snapshot. |

## pg_catalog.pg_stat_user_indexes

Table status: Schema-only (not implemented)
Source: Not implemented

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `relid` | Always NULL | Schema-only view required | Never. |
| `indexrelid` | Always NULL | Schema-only view required | Never. |
| `schemaname` | Always NULL | Schema-only view required | Never. |
| `relname` | Always NULL | Schema-only view required | Never. |
| `indexrelname` | Always NULL | Schema-only view required | Never. |
| `idx_scan` | Always NULL | Schema-only view required | Never. |
| `idx_tup_read` | Always NULL | Schema-only view required | Never. |
| `idx_tup_fetch` | Always NULL | Schema-only view required | Never. |

## pg_catalog.pg_tables

Table status: Implemented (view)
Source: EmulatedViewGenerator::getPostgreSQLViews

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `schemaname` | ScratchBird tracked | sys.catalog.schemas.schema_name | Row per table in schema. |
| `tablename` | ScratchBird tracked | sys.catalog.tables.table_name | Row per table in schema. |
| `tableowner` | ScratchBird tracked | sys.catalog.schemas.owner_name | Row per table in schema. |
| `tablespace` | Always NULL | Not populated | Never. |
| `hasindexes` | ScratchBird tracked | sys.catalog.tables.has_indexes | Row per table in schema. |
| `hastriggers` | ScratchBird tracked | sys.catalog.tables.has_triggers | Row per table in schema. |
| `hasrules` | Always 0 | Constant false | Always. |

## pg_catalog.pg_views

Table status: Implemented (view)
Source: EmulatedViewGenerator::getPostgreSQLViews

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `schemaname` | ScratchBird tracked | sys.catalog.schemas.schema_name | Row per view in schema. |
| `viewname` | ScratchBird tracked | sys.catalog.views.view_name | Row per view in schema. |
| `viewowner` | ScratchBird tracked | sys.catalog.schemas.owner_name | Row per view in schema. |
| `definition` | ScratchBird tracked | sys.catalog.views.definition | Row per view in schema. |

## pg_catalog.pg_stat_database

Table status: Implemented (view)
Source: EmulatedViewGenerator::getPostgreSQLViews

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `datid` | Always NULL | Constant NULL | Always. |
| `datname` | ScratchBird tracked | '{database_name}' in view context | Always. |
| `numbackends` | ScratchBird tracked | sys.performance metric connections_active | 0 when metric missing. |
| `xact_commit` | ScratchBird tracked | sys.performance metric transactions_committed_total | 0 when metric missing. |
| `xact_rollback` | ScratchBird tracked | sys.performance metric transactions_rolled_back_total | 0 when metric missing. |
| `blks_read` | ScratchBird tracked | sys.performance metric buffer_pool_reads_total{source=disk} | 0 when metric missing. |
| `blks_hit` | ScratchBird tracked | sys.performance metric buffer_pool_reads_total{source=cache} | 0 when metric missing. |
| `tup_returned` | ScratchBird tracked | sys.performance metric query_rows_returned_total | 0 when metric missing. |
| `tup_fetched` | Always NULL | Not populated | Never. |
| `tup_inserted` | ScratchBird tracked | sys.performance metric query_rows_affected_total{type=insert} | 0 when metric missing. |
| `tup_updated` | ScratchBird tracked | sys.performance metric query_rows_affected_total{type=update} | 0 when metric missing. |
| `tup_deleted` | ScratchBird tracked | sys.performance metric query_rows_affected_total{type=delete} | 0 when metric missing. |
| `conflicts` | Always 0 | Constant 0 | Always. |
| `temp_files` | Always 0 | Constant 0 | Always. |
| `temp_bytes` | Always 0 | Constant 0 | Always. |
| `deadlocks` | ScratchBird tracked | sys.performance metric deadlocks_total | 0 when metric missing. |
| `blk_read_time` | Always NULL | Not populated | Never. |
| `blk_write_time` | Always NULL | Not populated | Never. |
| `stats_reset` | Always NULL | Not populated | Never. |

## pg_catalog.pg_stat_bgwriter

Table status: Implemented (view)
Source: EmulatedViewGenerator::getPostgreSQLViews

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `buffers_clean` | ScratchBird tracked | sys.performance metric buffer_pool_writes_total | 0 when metric missing. |
| `maxwritten_clean` | Always NULL | Not populated | Never. |
| `buffers_alloc` | ScratchBird tracked | sys.performance metric page_buffers | 0 when metric missing. |
| `stats_reset` | Always NULL | Not populated | Never. |

## pg_catalog.pg_stat_all_tables (extended view)

Table status: Implemented (view)
Source: EmulatedViewGenerator::getPostgreSQLViews

| Column | Status | Source | Populated when |
|--------|--------|--------|----------------|
| `relid` | Always NULL | Constant NULL | Always. |
| `schemaname` | ScratchBird tracked | sys.table_stats.schema_name | Row per table stat entry. |
| `relname` | ScratchBird tracked | sys.table_stats.table_name | Row per table stat entry. |
| `seq_scan` | ScratchBird tracked | sys.table_stats.seq_scan_count | Row per table stat entry. |
| `last_seq_scan` | ScratchBird tracked | sys.table_stats.last_seq_scan_at | Row per table stat entry. |
| `seq_tup_read` | ScratchBird tracked | sys.table_stats.seq_rows_read | Row per table stat entry. |
| `idx_scan` | ScratchBird tracked | sys.table_stats.idx_scan_count | Row per table stat entry. |
| `last_idx_scan` | ScratchBird tracked | sys.table_stats.last_idx_scan_at | Row per table stat entry. |
| `idx_tup_fetch` | ScratchBird tracked | sys.table_stats.idx_rows_fetch | Row per table stat entry. |
| `n_tup_ins` | ScratchBird tracked | sys.table_stats.rows_inserted | Row per table stat entry. |
| `n_tup_upd` | ScratchBird tracked | sys.table_stats.rows_updated | Row per table stat entry. |
| `n_tup_del` | ScratchBird tracked | sys.table_stats.rows_deleted | Row per table stat entry. |
| `n_tup_hot_upd` | ScratchBird tracked | sys.table_stats.rows_hot_updated | Row per table stat entry. |
| `n_tup_newpage_upd` | ScratchBird tracked | sys.table_stats.rows_newpage_updated | Row per table stat entry. |
| `n_live_tup` | ScratchBird tracked | sys.table_stats.live_rows_estimate | Row per table stat entry. |
| `n_dead_tup` | ScratchBird tracked | sys.table_stats.dead_rows_estimate | Row per table stat entry. |
| `n_mod_since_analyze` | ScratchBird tracked | sys.table_stats.mod_since_analyze | Row per table stat entry. |
| `n_ins_since_vacuum` | ScratchBird tracked | sys.table_stats.ins_since_vacuum | Row per table stat entry. |
| `last_vacuum` | ScratchBird tracked | sys.table_stats.last_vacuum_at | Row per table stat entry. |
| `last_autovacuum` | ScratchBird tracked | sys.table_stats.last_autovacuum_at | Row per table stat entry. |
| `last_analyze` | ScratchBird tracked | sys.table_stats.last_analyze_at | Row per table stat entry. |
| `last_autoanalyze` | ScratchBird tracked | sys.table_stats.last_autoanalyze_at | Row per table stat entry. |
| `vacuum_count` | ScratchBird tracked | sys.table_stats.vacuum_count | Row per table stat entry. |
| `autovacuum_count` | ScratchBird tracked | sys.table_stats.autovacuum_count | Row per table stat entry. |
| `analyze_count` | ScratchBird tracked | sys.table_stats.analyze_count | Row per table stat entry. |
| `autoanalyze_count` | ScratchBird tracked | sys.table_stats.autoanalyze_count | Row per table stat entry. |
| `total_vacuum_time` | ScratchBird tracked | sys.table_stats.total_vacuum_time_ms / 1000.0 | Row per table stat entry. |
| `total_autovacuum_time` | ScratchBird tracked | sys.table_stats.total_autovacuum_time_ms / 1000.0 | Row per table stat entry. |
| `total_analyze_time` | ScratchBird tracked | sys.table_stats.total_analyze_time_ms / 1000.0 | Row per table stat entry. |
| `total_autoanalyze_time` | ScratchBird tracked | sys.table_stats.total_autoanalyze_time_ms / 1000.0 | Row per table stat entry. |

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*

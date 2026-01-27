# MySQL/MariaDB UDR Specification

Status: Draft (Target). This specification defines the MySQL/MariaDB UDR that
connects using the native MySQL wire protocol without vendor drivers.

## Scope
- Supported MySQL versions: 5.7, 8.0.
- Supported MariaDB versions: 10.3+.
- Supports pass-through DDL/DML/PSQL via sys.remote_exec/remote_query.

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../Alpha Phase 2/11-Remote-Database-UDR-Specification.md
- ../../Alpha Phase 2/11c-MySQL-Client-Implementation.md
- ../../wire_protocols/mysql_wire_protocol.md

## UDR Module
- Library: libscratchbird_mysql_udr.so
- FDW handler: mysql_udr_handler
- FDW validator: mysql_udr_validator
- Capabilities: network

## Connection and Authentication
- TLS: supported; require verify-ca or verify-full in production.
- Auth plugins: caching_sha2_password, mysql_native_password.
- Optional session settings: sql_mode, time_zone, character_set_results.

## Server Options

| Option | Default | Description |
| --- | --- | --- |
| host | required | Server host (allowlisted) |
| port | 3306 | Server port |
| dbname | required | Database name |
| ssl_mode | prefer | disable/require/verify-ca/verify-full |
| ssl_ca | "" | CA cert path |
| ssl_cert | "" | Client cert path |
| ssl_key | "" | Client key path |
| connect_timeout | 5000 | ms |
| query_timeout | 30000 | ms |
| sql_mode | "" | Session sql_mode |
| allow_ddl | false | Allow CREATE/ALTER/DROP |
| allow_dml | true | Allow INSERT/UPDATE/DELETE |
| allow_psql | false | Allow stored routines |
| allow_passthrough | false | Allow sys.remote_exec/query |

## Example SQL Setup
```sql
CREATE FOREIGN DATA WRAPPER mysql_fdw
    HANDLER mysql_udr_handler
    VALIDATOR mysql_udr_validator;

CREATE SERVER legacy_mysql
    FOREIGN DATA WRAPPER mysql_fdw
    OPTIONS (host 'legacy-db', port '3306', dbname 'prod',
             allow_dml 'true', allow_ddl 'true', allow_passthrough 'true');

CREATE USER MAPPING FOR migration_role
    SERVER legacy_mysql
    OPTIONS (user 'legacy_user', password '***');
```

## Pass-through Examples
```sql
CALL sys.remote_exec('legacy_mysql', 'ALTER TABLE users ADD COLUMN foo int');
SELECT * FROM sys.remote_query('legacy_mysql', 'SELECT count(*) FROM users');
CALL sys.remote_call('legacy_mysql', 'rebuild_indexes', '{"schema":"prod"}');
```

## Metadata Discovery (Required)
Use information_schema for schema analysis:
- information_schema.schemata, tables, columns
- statistics, table_constraints, key_column_usage, referential_constraints
- routines, parameters, triggers

### Introspection Examples (MySQL/MariaDB)
```sql
-- Schemas (databases)
SELECT schema_name, default_character_set_name, default_collation_name
FROM information_schema.schemata;

-- Tables / views
SELECT table_schema, table_name, table_type, engine
FROM information_schema.tables
WHERE table_schema NOT IN ('mysql','performance_schema','information_schema','sys');

-- Columns
SELECT table_schema, table_name, column_name, data_type, column_type,
       is_nullable, column_default, extra
FROM information_schema.columns
WHERE table_schema = ? AND table_name = ?
ORDER BY ordinal_position;

-- Primary/unique/foreign keys
SELECT tc.constraint_name, tc.constraint_type, kcu.table_schema,
       kcu.table_name, kcu.column_name, kcu.referenced_table_schema,
       kcu.referenced_table_name, kcu.referenced_column_name
FROM information_schema.table_constraints tc
JOIN information_schema.key_column_usage kcu
  ON tc.constraint_schema = kcu.constraint_schema
 AND tc.constraint_name = kcu.constraint_name
WHERE tc.table_schema = ? AND tc.table_name = ?
  AND tc.constraint_type IN ('PRIMARY KEY','UNIQUE','FOREIGN KEY');

-- Indexes
SELECT table_schema, table_name, index_name, non_unique, seq_in_index, column_name
FROM information_schema.statistics
WHERE table_schema = ? AND table_name = ?
ORDER BY index_name, seq_in_index;

-- Routines
SELECT routine_schema, routine_name, routine_type, data_type
FROM information_schema.routines
WHERE routine_schema = ?;

-- Triggers
SELECT trigger_schema, trigger_name, event_manipulation, action_timing
FROM information_schema.triggers
WHERE trigger_schema = ?;
```

## Schema Mounting
MySQL/MariaDB database maps to schema (no separate schema layer). Imported
schemas are mounted under `legacy_<server>` (or `mount_root` override).

## Testing Checklist
- TLS handshake and auth plugin negotiation.
- Pass-through DDL denied when allow_ddl=false.
- Schema import with views and triggers.

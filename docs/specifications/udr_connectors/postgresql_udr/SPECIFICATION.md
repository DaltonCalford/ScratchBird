# PostgreSQL UDR Specification

Status: Draft (Target). This specification defines the PostgreSQL UDR that
connects to PostgreSQL using the native wire protocol without vendor drivers.

## Scope
- Supported PostgreSQL versions: 9.6 through 17.
- Uses wire protocol v3.0 (see wire_protocols/postgresql_wire_protocol.md).
- Supports pass-through DDL/DML/PSQL via sys.remote_exec/remote_query.

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../Alpha Phase 2/11-Remote-Database-UDR-Specification.md
- ../../Alpha Phase 2/11b-PostgreSQL-Client-Implementation.md
- ../../wire_protocols/postgresql_wire_protocol.md

## UDR Module
- Library: libscratchbird_postgresql_udr.so
- FDW handler: postgresql_udr_handler
- FDW validator: postgresql_udr_validator
- Capabilities: network

## Connection and Authentication
- TLS: required when available; verify server cert if ssl_mode=verify-*
- Auth methods: SCRAM-SHA-256, MD5 (legacy), TLS client cert
- Startup parameters: user, database, application_name, search_path

## Server Options

| Option | Default | Description |
| --- | --- | --- |
| host | required | Server host (allowlisted) |
| port | 5432 | Server port |
| dbname | required | Database name |
| ssl_mode | prefer | disable/allow/prefer/require/verify-ca/verify-full |
| ssl_rootcert | "" | CA cert path |
| ssl_cert | "" | Client cert path |
| ssl_key | "" | Client key path |
| connect_timeout | 5000 | ms |
| query_timeout | 30000 | ms |
| allow_ddl | false | Allow CREATE/ALTER/DROP |
| allow_dml | true | Allow INSERT/UPDATE/DELETE |
| allow_psql | false | Allow CALL/DO/EXECUTE |
| allow_passthrough | false | Allow sys.remote_exec/query |

## Example SQL Setup
```sql
CREATE FOREIGN DATA WRAPPER postgresql_fdw
    HANDLER postgresql_udr_handler
    VALIDATOR postgresql_udr_validator;

CREATE SERVER legacy_pg
    FOREIGN DATA WRAPPER postgresql_fdw
    OPTIONS (host 'legacy-db', port '5432', dbname 'prod',
             allow_dml 'true', allow_ddl 'true', allow_passthrough 'true');

CREATE USER MAPPING FOR migration_role
    SERVER legacy_pg
    OPTIONS (user 'legacy_user', password '***');
```

## Pass-through Examples
```sql
CALL sys.remote_exec('legacy_pg', 'CREATE TABLE tmp(id int)');
SELECT * FROM sys.remote_query('legacy_pg', 'SELECT count(*) FROM users');
CALL sys.remote_call('legacy_pg', 'refresh_materialized_view', '{"name":"mv"}');
```

## Metadata Discovery (Required)
Use pg_catalog and information_schema for schema analysis:
- pg_namespace, pg_class, pg_attribute
- pg_index, pg_constraint
- pg_proc, pg_trigger
- information_schema.columns for type/nullable defaults

### Introspection Examples (PostgreSQL)
```sql
-- Schemas
SELECT n.oid, n.nspname
FROM pg_namespace n
WHERE n.nspname NOT IN ('pg_catalog', 'information_schema')
  AND n.nspname NOT LIKE 'pg_toast%';

-- Tables / views / materialized views
SELECT c.oid, n.nspname, c.relname, c.relkind
FROM pg_class c
JOIN pg_namespace n ON n.oid = c.relnamespace
WHERE c.relkind IN ('r','p','v','m','f');

-- Columns (with defaults)
SELECT a.attrelid, a.attnum, a.attname, t.typname,
       a.atttypmod, a.attnotnull,
       pg_get_expr(d.adbin, d.adrelid) AS default_expr
FROM pg_attribute a
JOIN pg_type t ON t.oid = a.atttypid
LEFT JOIN pg_attrdef d ON d.adrelid = a.attrelid AND d.adnum = a.attnum
WHERE a.attnum > 0 AND NOT a.attisdropped;

-- Primary keys / unique constraints
SELECT c.oid, c.contype, c.conname, c.conrelid, c.conkey
FROM pg_constraint c
WHERE c.contype IN ('p','u');

-- Foreign keys
SELECT c.oid, c.conname, c.conrelid, c.confrelid, c.conkey, c.confkey
FROM pg_constraint c
WHERE c.contype = 'f';

-- Indexes
SELECT i.indexrelid, i.indrelid, i.indkey, i.indisunique, i.indisprimary
FROM pg_index i;

-- Routines (PG 11+ prokind; fallback to proisagg/proiswindow for older)
SELECT p.oid, n.nspname, p.proname, p.prokind, p.prorettype, p.proargtypes
FROM pg_proc p
JOIN pg_namespace n ON n.oid = p.pronamespace;

-- Triggers
SELECT t.oid, t.tgrelid, t.tgname, t.tgtype, t.tgenabled
FROM pg_trigger t
WHERE NOT t.tgisinternal;

-- Sequences
SELECT c.oid, n.nspname, c.relname
FROM pg_class c
JOIN pg_namespace n ON n.oid = c.relnamespace
WHERE c.relkind = 'S';
```

## Schema Emulation and Migration
- Import remote schema into `legacy_pg` schema using IMPORT FOREIGN SCHEMA.
- Generate emulated ScratchBird structures in `emulated_pg` schema.
- Pass-through DDL/DML/PSQL to legacy until verification passes.

## Example UDR Adapter Skeleton (C++)
```cpp
class PostgreSqlAdapter : public IRemoteAdapter {
public:
  Result<RemoteQueryResult> executeRaw(const std::string& sql);
  Result<RemoteQueryResult> executeParams(const std::string& sql,
    const std::vector<TypedValue>& params);
};
```

## Testing Checklist
- SCRAM auth and TLS verification.
- Pass-through DDL denied when allow_ddl=false.
- Schema import for tables and views.

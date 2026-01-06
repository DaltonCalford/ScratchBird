# PostgreSQL UDR Specification

Status: Draft (Target). This specification defines the PostgreSQL UDR that
connects to PostgreSQL using the native wire protocol without vendor drivers.

## Scope
- Supported PostgreSQL versions: 9.6 through 17.
- Uses wire protocol v3.0 (see wire_protocols/postgresql_wire_protocol.md).
- Supports pass-through DDL/DML/PSQL via sys.remote_exec/remote_query.

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../remote_database_udr/03-POSTGRESQL_ADAPTER.md
- ../../remote_database_udr/06-QUERY_EXECUTION.md
- ../../remote_database_udr/07-SCHEMA_INTROSPECTION.md
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

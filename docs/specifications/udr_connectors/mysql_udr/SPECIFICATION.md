# MySQL/MariaDB UDR Specification

Status: Draft (Target). This specification defines the MySQL/MariaDB UDR that
connects using the native MySQL wire protocol without vendor drivers.

## Scope
- Supported MySQL versions: 5.7, 8.0.
- Supported MariaDB versions: 10.3+.
- Supports pass-through DDL/DML/PSQL via sys.remote_exec/remote_query.

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../remote_database_udr/04-MYSQL_ADAPTER.md
- ../../remote_database_udr/06-QUERY_EXECUTION.md
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

## Testing Checklist
- TLS handshake and auth plugin negotiation.
- Pass-through DDL denied when allow_ddl=false.
- Schema import with views and triggers.

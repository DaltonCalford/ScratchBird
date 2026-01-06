# Firebird UDR Specification

Status: Draft (Target). This specification defines the Firebird UDR that
connects using the native Firebird wire protocol without vendor drivers.

## Scope
- Supported Firebird versions: 2.5, 3.0, 4.0, 5.0.
- Uses Firebird wire protocol (see wire_protocols/firebird_wire_protocol.md).
- Supports pass-through DDL/DML/PSQL via sys.remote_exec/remote_query.

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../remote_database_udr/05-MSSQL_FIREBIRD_ADAPTERS.md
- ../../remote_database_udr/06-QUERY_EXECUTION.md
- ../../wire_protocols/firebird_wire_protocol.md

## UDR Module
- Library: libscratchbird_firebird_udr.so
- FDW handler: firebird_udr_handler
- FDW validator: firebird_udr_validator
- Capabilities: network

## Connection and Authentication
- Auth methods: SRP (preferred), legacy auth (optional).
- DPB and TPB settings must be exposed for transaction semantics.
- Database path is a server-side path; ensure allowlist and safety.

## Server Options

| Option | Default | Description |
| --- | --- | --- |
| host | required | Server host (allowlisted) |
| port | 3050 | Server port |
| dbname | required | Database path or alias |
| charset | UTF8 | Connection charset |
| connect_timeout | 5000 | ms |
| query_timeout | 30000 | ms |
| allow_ddl | false | Allow CREATE/ALTER/DROP |
| allow_dml | true | Allow INSERT/UPDATE/DELETE |
| allow_psql | true | Allow EXECUTE PROCEDURE/PSQL |
| allow_passthrough | false | Allow sys.remote_exec/query |

## Example SQL Setup
```sql
CREATE FOREIGN DATA WRAPPER firebird_fdw
    HANDLER firebird_udr_handler
    VALIDATOR firebird_udr_validator;

CREATE SERVER legacy_fb
    FOREIGN DATA WRAPPER firebird_fdw
    OPTIONS (host 'legacy-db', port '3050', dbname '/data/legacy.fdb',
             allow_dml 'true', allow_psql 'true', allow_passthrough 'true');

CREATE USER MAPPING FOR migration_role
    SERVER legacy_fb
    OPTIONS (user 'SYSDBA', password '***');
```

## Pass-through Examples
```sql
CALL sys.remote_exec('legacy_fb', 'CREATE TABLE tmp(id int)');
SELECT * FROM sys.remote_query('legacy_fb', 'SELECT count(*) FROM users');
CALL sys.remote_call('legacy_fb', 'REBUILD_INDEXES', '{}');
```

## PSQL Considerations
- Firebird PSQL syntax is not identical to ScratchBird PSQL.
- The UDR must translate or pass-through PSQL as configured.
- View and procedure definitions must be stored in the emulated schema tree.

## Testing Checklist
- SRP auth and legacy auth.
- Pass-through PSQL execution gated by allow_psql.
- DPB/TPB settings mapped to ScratchBird transaction levels.

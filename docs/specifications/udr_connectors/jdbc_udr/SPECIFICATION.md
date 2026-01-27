# JDBC UDR Specification

Status: Draft (Beta). This specification defines the JDBC UDR that uses an
embedded Java runtime and bundled JDBC drivers (no system Java install).

## Scope
- JDBC 4.2 API surface sufficient for pass-through queries and metadata.
- Bundled drivers only (no dynamic system driver loading).
- Supports pass-through DDL/DML via sys.remote_exec/remote_query.
- Supports schema discovery + mounting for foreign tables and migration.

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../Alpha Phase 2/11-Remote-Database-UDR-Specification.md
- ../../Alpha Phase 2/11g-JDBC-Client-Implementation.md

## UDR Module
- Library: libscratchbird_jdbc_udr.so
- FDW handler: jdbc_udr_handler
- FDW validator: jdbc_udr_validator
- Capabilities: network

## Connection and Authentication
- Driver name required (bundled driver id)
- JDBC URL optional; DSN-less params supported
- TLS options when supported by driver

## Server Options

| Option | Default | Description |
| --- | --- | --- |
| driver_name | required | Bundled driver identifier |
| jdbc_url | "" | Optional JDBC URL |
| host | required | Server host (allowlisted) |
| port | required | Server port |
| database | required | Database name |
| connect_timeout | 5000 | ms |
| query_timeout | 30000 | ms |
| allow_ddl | false | Allow CREATE/ALTER/DROP |
| allow_dml | true | Allow INSERT/UPDATE/DELETE |
| allow_passthrough | false | Allow sys.remote_exec/query |

## Metadata Discovery (Required)
Use JDBC DatabaseMetaData to analyze the remote database:
- getCatalogs / getSchemas / getTables / getColumns
- getPrimaryKeys / getImportedKeys / getIndexInfo
- getProcedures / getProcedureColumns
- getTypeInfo

If a driver does not support a category, report it as `not_supported`.

### Introspection Examples (JDBC)
```java
DatabaseMetaData md = conn.getMetaData();

ResultSet schemas = md.getSchemas();
ResultSet tables = md.getTables(catalog, schema, "%", new String[] {"TABLE","VIEW"});
ResultSet columns = md.getColumns(catalog, schema, table, "%");

ResultSet pks = md.getPrimaryKeys(catalog, schema, table);
ResultSet fks = md.getImportedKeys(catalog, schema, table);
ResultSet indexes = md.getIndexInfo(catalog, schema, table, false, false);

ResultSet procs = md.getProcedures(catalog, schema, "%");
ResultSet procCols = md.getProcedureColumns(catalog, schema, proc, "%");

ResultSet types = md.getTypeInfo();
```

## Schema Mounting
Imported schemas are mounted under `legacy_<server>` (or `mount_root` override)
per the Remote Database UDR spec. JDBC drivers that expose catalogs should map
catalog -> schema for mount purposes.

## Pass-through and Migration
- Pass-through: sys.remote_exec, sys.remote_query, sys.remote_call
- Migration: optional and explicit; uses the migration state machine defined in
  UDR_CONNECTOR_BASELINE.md and the Remote Database UDR spec.

## Testing Checklist
- Metadata discovery for tables/columns/keys.
- Pass-through denied when allow_passthrough=false.
- Schema import for at least one catalog + schema.

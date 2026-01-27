# ODBC UDR Specification

Status: Draft (Beta). This specification defines the ODBC UDR that uses an
embedded ODBC driver manager and bundled drivers (no system ODBC install).

## Scope
- ODBC 3.8 API surface sufficient for pass-through queries and metadata.
- Bundled drivers only (no dynamic system driver loading).
- Supports pass-through DDL/DML via sys.remote_exec/remote_query.
- Supports schema discovery + mounting for foreign tables and migration.

## References
- ../UDR_CONNECTOR_BASELINE.md
- ../../Alpha Phase 2/11-Remote-Database-UDR-Specification.md
- ../../Alpha Phase 2/11f-ODBC-Client-Implementation.md

## UDR Module
- Library: libscratchbird_odbc_udr.so
- FDW handler: odbc_udr_handler
- FDW validator: odbc_udr_validator
- Capabilities: network

## Connection and Authentication
- Driver name required (bundled driver id)
- DSN optional; DSN-less params supported
- TLS options when supported by driver

## Server Options

| Option | Default | Description |
| --- | --- | --- |
| driver_name | required | Bundled driver identifier |
| dsn | "" | Optional DSN label |
| host | required | Server host (allowlisted) |
| port | required | Server port |
| database | required | Database name |
| connect_timeout | 5000 | ms |
| query_timeout | 30000 | ms |
| allow_ddl | false | Allow CREATE/ALTER/DROP |
| allow_dml | true | Allow INSERT/UPDATE/DELETE |
| allow_passthrough | false | Allow sys.remote_exec/query |

## Metadata Discovery (Required)
Use ODBC catalog APIs to analyze the remote database:
- SQLTables / SQLColumns
- SQLPrimaryKeys / SQLForeignKeys
- SQLStatistics
- SQLProcedures / SQLProcedureColumns
- SQLGetTypeInfo

If a driver does not support a category, report it as `not_supported`.

### Introspection Examples (ODBC)
```c
// Tables
SQLTables(hstmt,
  (SQLCHAR*)catalog, SQL_NTS,
  (SQLCHAR*)schema, SQL_NTS,
  (SQLCHAR*)table, SQL_NTS,
  (SQLCHAR*)"TABLE,VIEW", SQL_NTS);

// Columns
SQLColumns(hstmt,
  (SQLCHAR*)catalog, SQL_NTS,
  (SQLCHAR*)schema, SQL_NTS,
  (SQLCHAR*)table, SQL_NTS,
  NULL, 0);

// Primary/foreign keys
SQLPrimaryKeys(hstmt,
  (SQLCHAR*)catalog, SQL_NTS,
  (SQLCHAR*)schema, SQL_NTS,
  (SQLCHAR*)table, SQL_NTS);

SQLForeignKeys(hstmt,
  (SQLCHAR*)pk_catalog, SQL_NTS,
  (SQLCHAR*)pk_schema, SQL_NTS,
  (SQLCHAR*)pk_table, SQL_NTS,
  (SQLCHAR*)fk_catalog, SQL_NTS,
  (SQLCHAR*)fk_schema, SQL_NTS,
  (SQLCHAR*)fk_table, SQL_NTS);

// Indexes
SQLStatistics(hstmt,
  (SQLCHAR*)catalog, SQL_NTS,
  (SQLCHAR*)schema, SQL_NTS,
  (SQLCHAR*)table, SQL_NTS,
  SQL_INDEX_ALL, SQL_ENSURE);

// Routines
SQLProcedures(hstmt,
  (SQLCHAR*)catalog, SQL_NTS,
  (SQLCHAR*)schema, SQL_NTS,
  (SQLCHAR*)proc, SQL_NTS);

SQLProcedureColumns(hstmt,
  (SQLCHAR*)catalog, SQL_NTS,
  (SQLCHAR*)schema, SQL_NTS,
  (SQLCHAR*)proc, SQL_NTS,
  NULL, 0);

// Type info
SQLGetTypeInfo(hstmt, SQL_ALL_TYPES);
```

## Schema Mounting
Imported schemas are mounted under `legacy_<server>` (or `mount_root` override)
per the Remote Database UDR spec. ODBC drivers that expose catalogs should map
catalog -> schema for mount purposes.

## Pass-through and Migration
- Pass-through: sys.remote_exec, sys.remote_query, sys.remote_call
- Migration: optional and explicit; uses the migration state machine defined in
  UDR_CONNECTOR_BASELINE.md and the Remote Database UDR spec.

## Testing Checklist
- Catalog discovery for tables/columns/keys.
- Pass-through denied when allow_passthrough=false.
- Schema import for at least one catalog + schema.

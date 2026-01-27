# ODBC Connector Implementation

ODBC connector UDR for Remote Database UDR.

**See**: [11-Remote-Database-UDR-Specification.md](11-Remote-Database-UDR-Specification.md) for overview.

---

## Overview

Implements an embedded ODBC 3.8 driver manager and a curated set of bundled
ODBC drivers inside the UDR package. No system-level ODBC libraries are
required on the host OS.

**Protocol**: ODBC 3.8 API (driver manager + drivers in UDR bundle)  
**Library**: None (embedded in UDR package)  
**Supported Targets**: Drivers shipped in the bundle (initial set: PostgreSQL,
MySQL, MSSQL)

---

## Key Functions

### Connection Management
```c
void* create_odbc_connection(RemoteConnectionPoolConfig* config);
bool validate_odbc_connection(void* handle);
void destroy_odbc_connection(void* handle);
```

### Query Execution
```c
RemoteResultSet* execute_odbc_query(void* handle, const char* sql, IStatus* status);
void* prepare_odbc_statement(void* handle, const char* sql, IStatus* status);
RemoteResultSet* execute_odbc_prepared(void* handle, void* stmt, IMessageBuffer* params, IStatus* status);
```

### Schema Introspection
```c
List<RemoteTableMetadata*>* list_odbc_tables(void* handle, const char* schema, IStatus* status);
RemoteTableMetadata* get_odbc_table_metadata(void* handle, const char* schema, const char* table, IStatus* status);
```

The metadata feed must be sufficient to mount schemas and drive the migration
workflow described in 11-Remote-Database-UDR-Specification.md.

---

## Connection Options

Supported options (REGISTER REMOTE DATABASE):
- `protocol = 'odbc'`
- `driver_name` (matches a bundled driver identifier)
- `dsn` (optional DSN label)
- `host`, `port`, `database`, `username`, `password`
- TLS options (if supported by the bundled driver)

---

## Driver Packaging Rules

- All ODBC components ship within the UDR bundle.
- No dynamic loading of system ODBC drivers.
- Bundled drivers must be signed and listed in the UDR manifest.
- Each driver declares supported server versions.

---

## Limitations

- Only bundled drivers are available in Beta.
- For databases with native wire‑protocol connectors, the native connector is
  preferred. ODBC is for compatibility or mixed environments.

---

## Type Mapping

ODBC types are mapped via standard SQL type codes (SQL_INTEGER, SQL_VARCHAR,
SQL_VARBINARY, etc.) to ScratchBird internal types. Driver-specific overrides
are allowed where required.

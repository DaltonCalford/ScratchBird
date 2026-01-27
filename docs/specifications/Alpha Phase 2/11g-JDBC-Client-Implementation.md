# JDBC Connector Implementation

JDBC connector UDR for Remote Database UDR.

**See**: [11-Remote-Database-UDR-Specification.md](11-Remote-Database-UDR-Specification.md) for overview.

---

## Overview

Implements a minimal embedded Java runtime plus a curated set of JDBC drivers
bundled inside the UDR package. No system-level Java runtime or external
client libraries are required on the host OS.

**Protocol**: JDBC 4.2 API (embedded runtime + drivers in UDR bundle)  
**Library**: None (embedded in UDR package)  
**Supported Targets**: Drivers shipped in the bundle (initial set: PostgreSQL,
MySQL, MSSQL)

---

## Key Functions

### Connection Management
```c
void* create_jdbc_connection(RemoteConnectionPoolConfig* config);
bool validate_jdbc_connection(void* handle);
void destroy_jdbc_connection(void* handle);
```

### Query Execution
```c
RemoteResultSet* execute_jdbc_query(void* handle, const char* sql, IStatus* status);
void* prepare_jdbc_statement(void* handle, const char* sql, IStatus* status);
RemoteResultSet* execute_jdbc_prepared(void* handle, void* stmt, IMessageBuffer* params, IStatus* status);
```

### Schema Introspection
```c
List<RemoteTableMetadata*>* list_jdbc_tables(void* handle, const char* schema, IStatus* status);
RemoteTableMetadata* get_jdbc_table_metadata(void* handle, const char* schema, const char* table, IStatus* status);
```

The metadata feed must be sufficient to mount schemas and drive the migration
workflow described in 11-Remote-Database-UDR-Specification.md.

---

## Connection Options

Supported options (REGISTER REMOTE DATABASE):
- `protocol = 'jdbc'`
- `driver_name` (matches a bundled driver identifier)
- `jdbc_url` (optional, if using a driver-specific URL)
- `host`, `port`, `database`, `username`, `password`
- TLS options (if supported by the bundled driver)

---

## Driver Packaging Rules

- Embedded Java runtime and drivers ship within the UDR bundle.
- No dynamic loading of system JDBC drivers.
- Bundled drivers must be signed and listed in the UDR manifest.
- Each driver declares supported server versions.

---

## Limitations

- Only bundled drivers are available in Beta.
- For databases with native wire‑protocol connectors, the native connector is
  preferred. JDBC is for compatibility or mixed environments.

---

## Type Mapping

JDBC types are mapped via java.sql.Types metadata to ScratchBird internal
types. Driver-specific overrides are allowed where required.

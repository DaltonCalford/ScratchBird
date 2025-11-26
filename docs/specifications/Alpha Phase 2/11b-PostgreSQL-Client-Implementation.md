# PostgreSQL Client Implementation

PostgreSQL wire protocol client adapter for Remote Database UDR.

**See**: [11-Remote-Database-UDR-Specification.md](11-Remote-Database-UDR-Specification.md) for overview.

---

## Overview

Implements PostgreSQL wire protocol client using libpq library.

**Protocol**: PostgreSQL Frontend/Backend Protocol 3.0  
**Library**: libpq (PostgreSQL client library)  
**Supported Versions**: PostgreSQL 9.6 - 17.x

---

## Key Functions

### Connection Management
```c
void* create_postgresql_connection(RemoteConnectionPoolConfig* config);
bool validate_postgresql_connection(void* handle);
void destroy_postgresql_connection(void* handle);
```

### Query Execution
```c
RemoteResultSet* execute_postgresql_query(void* handle, const char* sql, IStatus* status);
void* prepare_postgresql_statement(void* handle, const char* sql, IStatus* status);
RemoteResultSet* execute_postgresql_prepared(void* handle, void* stmt, IMessageBuffer* params, IStatus* status);
```

### Schema Introspection
```c
List<RemoteTableMetadata*>* list_postgresql_tables(void* handle, const char* schema, IStatus* status);
RemoteTableMetadata* get_postgresql_table_metadata(void* handle, const char* schema, const char* table, IStatus* status);
```

---

## Implementation Notes

**Connection String Format:**
```
host=hostname port=5432 dbname=database user=username password=password sslmode=require
```

**Type Mapping:**
| PostgreSQL Type | Internal Type |
|----------------|---------------|
| SMALLINT | INT16 |
| INTEGER | INT32 |
| BIGINT | INT64 |
| REAL | FLOAT |
| DOUBLE PRECISION | DOUBLE |
| VARCHAR(n) | STRING |
| TEXT | STRING |
| BYTEA | BYTES |
| TIMESTAMP | TIMESTAMP |
| BOOLEAN | BOOL |

**Authentication:**
- Password (MD5)
- SCRAM-SHA-256 (PostgreSQL 10+)
- SSL/TLS certificates

**Build Requirements:**
```
libpq-dev (Debian/Ubuntu)
postgresql-devel (RHEL/CentOS)
```

See main specification for complete usage examples.

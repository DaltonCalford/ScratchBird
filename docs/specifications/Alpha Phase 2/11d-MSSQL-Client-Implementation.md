# MS SQL Server Client Implementation

MS SQL Server (TDS protocol) client adapter for Remote Database UDR.

**See**: [11-Remote-Database-UDR-Specification.md](11-Remote-Database-UDR-Specification.md) for overview.

---

## Overview

Implements TDS (Tabular Data Stream) protocol client using FreeTDS library.

**Protocol**: TDS 7.0 - 7.4  
**Library**: FreeTDS (open-source TDS implementation)  
**Supported Versions**: SQL Server 2016 - 2022, Azure SQL Database

---

## Key Functions

### Connection Management
```c
void* create_mssql_connection(RemoteConnectionPoolConfig* config);
bool validate_mssql_connection(void* handle);
void destroy_mssql_connection(void* handle);
```

### Query Execution
```c
RemoteResultSet* execute_mssql_query(void* handle, const char* sql, IStatus* status);
void* prepare_mssql_statement(void* handle, const char* sql, IStatus* status);
RemoteResultSet* execute_mssql_prepared(void* handle, void* stmt, IMessageBuffer* params, IStatus* status);
```

### Schema Introspection
```c
List<RemoteTableMetadata*>* list_mssql_tables(void* handle, const char* schema, IStatus* status);
RemoteTableMetadata* get_mssql_table_metadata(void* handle, const char* schema, const char* table, IStatus* status);
```

---

## Implementation Notes

**Connection String Format:**
```
DRIVER={FreeTDS};
SERVER=hostname;
PORT=1433;
DATABASE=database;
UID=username;
PWD=password;
TDS_Version=7.4;
Encrypt=yes;
```

**Type Mapping:**
| SQL Server Type | Internal Type |
|----------------|---------------|
| TINYINT | INT8 |
| SMALLINT | INT16 |
| INT | INT32 |
| BIGINT | INT64 |
| REAL | FLOAT |
| FLOAT | DOUBLE |
| NVARCHAR(n) | STRING |
| VARBINARY | BYTES |
| DATETIME2 | TIMESTAMP |
| BIT | BOOL |
| DECIMAL(p,s) | DECIMAL |

**Authentication:**
- SQL Server Authentication (username/password)
- Windows Authentication (Kerberos)
- Azure AD Authentication

**Build Requirements:**
```
freetds-dev (Debian/Ubuntu)
freetds-devel (RHEL/CentOS)
```

**Special Considerations:**
- Unicode handling (NVARCHAR vs VARCHAR)
- Case sensitivity settings (collation)
- Batch separators (GO statements)
- Transaction isolation levels
- SET NOCOUNT ON for performance

See main specification for complete usage examples.

# MySQL Client Implementation

MySQL wire protocol client adapter for Remote Database UDR.

**See**: [11-Remote-Database-UDR-Specification.md](11-Remote-Database-UDR-Specification.md) for overview.

---

## Overview

Implements MySQL wire protocol client using libmysqlclient library.

**Protocol**: MySQL Client/Server Protocol  
**Library**: libmysqlclient (MySQL client library)  
**Supported Versions**: MySQL 5.7, 8.0+, MariaDB 10.x

---

## Key Functions

### Connection Management
```c
void* create_mysql_connection(RemoteConnectionPoolConfig* config);
bool validate_mysql_connection(void* handle);
void destroy_mysql_connection(void* handle);
```

### Query Execution
```c
RemoteResultSet* execute_mysql_query(void* handle, const char* sql, IStatus* status);
void* prepare_mysql_statement(void* handle, const char* sql, IStatus* status);
RemoteResultSet* execute_mysql_prepared(void* handle, void* stmt, IMessageBuffer* params, IStatus* status);
```

### Schema Introspection
```c
List<RemoteTableMetadata*>* list_mysql_tables(void* handle, const char* schema, IStatus* status);
RemoteTableMetadata* get_mysql_table_metadata(void* handle, const char* schema, const char* table, IStatus* status);
```

---

## Implementation Notes

**Connection Parameters:**
```c
mysql_real_connect(
    mysql,
    "hostname",
    "username",
    "password",
    "database",
    3306,  // port
    NULL,  // unix_socket
    0      // client_flag
);
```

**Type Mapping:**
| MySQL Type | Internal Type |
|-----------|---------------|
| TINYINT | INT8 |
| SMALLINT | INT16 |
| INT | INT32 |
| BIGINT | INT64 |
| FLOAT | FLOAT |
| DOUBLE | DOUBLE |
| VARCHAR(n) | STRING |
| TEXT | STRING |
| BLOB | BYTES |
| DATETIME | TIMESTAMP |
| DECIMAL(p,s) | DECIMAL |

**Authentication:**
- mysql_native_password
- caching_sha2_password (MySQL 8.0+)
- SSL/TLS

**Build Requirements:**
```
libmysqlclient-dev (Debian/Ubuntu)
mysql-devel (RHEL/CentOS)
```

**Special Considerations:**
- Handle MySQL's unsigned integer types
- DATETIME vs TIMESTAMP timezone handling
- TEXT/BLOB size limits
- Character set conversion (UTF-8)

See main specification for complete usage examples.

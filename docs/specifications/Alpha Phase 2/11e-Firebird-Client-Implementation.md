# Firebird Client Implementation

Firebird wire protocol client adapter for Remote Database UDR.

**See**: [11-Remote-Database-UDR-Specification.md](11-Remote-Database-UDR-Specification.md) for overview.

---

## Overview

Implements Firebird wire protocol client using fbclient library.

**Protocol**: Firebird Wire Protocol  
**Library**: fbclient (Firebird client library)  
**Supported Versions**: Firebird 2.5, 3.0, 4.0, 5.0

---

## Key Functions

### Connection Management
```c
void* create_firebird_connection(RemoteConnectionPoolConfig* config);
bool validate_firebird_connection(void* handle);
void destroy_firebird_connection(void* handle);
```

### Query Execution
```c
RemoteResultSet* execute_firebird_query(void* handle, const char* sql, IStatus* status);
void* prepare_firebird_statement(void* handle, const char* sql, IStatus* status);
RemoteResultSet* execute_firebird_prepared(void* handle, void* stmt, IMessageBuffer* params, IStatus* status);
```

### Schema Introspection
```c
List<RemoteTableMetadata*>* list_firebird_tables(void* handle, const char* schema, IStatus* status);
RemoteTableMetadata* get_firebird_table_metadata(void* handle, const char* schema, const char* table, IStatus* status);
```

---

## Implementation Notes

**Connection String Format:**
```
hostname:database_path
hostname/port:database_path

Examples:
server.internal:/data/production.fdb
server.internal/3050:/data/production.fdb
```

**Type Mapping:**
| Firebird Type | Internal Type |
|--------------|---------------|
| SMALLINT | INT16 |
| INTEGER | INT32 |
| BIGINT | INT64 |
| FLOAT | FLOAT |
| DOUBLE PRECISION | DOUBLE |
| VARCHAR(n) | STRING |
| CHAR(n) | STRING |
| BLOB SUB_TYPE TEXT | STRING |
| BLOB SUB_TYPE 0 | BYTES |
| TIMESTAMP | TIMESTAMP |
| BOOLEAN | BOOL |

**Authentication:**
- Legacy password (FB 2.5)
- SRPA authentication (FB 3.0+)
- Win_Sspi (Windows)

**Build Requirements:**
```
firebird-dev (Debian/Ubuntu)
firebird-devel (RHEL/CentOS)
```

**Special Considerations:**
- Dialect handling (Dialect 3 recommended)
- BLOB handling (segmented vs stream)
- Character set specification (UTF-8, WIN1252, etc.)
- Generator (sequence) support
- Transaction parameterization (TPB)

**Schema Introspection Queries:**
```sql
-- List tables
SELECT RDB$RELATION_NAME 
FROM RDB$RELATIONS 
WHERE RDB$SYSTEM_FLAG = 0 AND RDB$VIEW_BLR IS NULL;

-- Get columns
SELECT 
    RDB$FIELD_NAME,
    RDB$FIELD_TYPE,
    RDB$FIELD_LENGTH,
    RDB$NULL_FLAG
FROM RDB$RELATION_FIELDS
WHERE RDB$RELATION_NAME = ?;
```

See main specification for complete usage examples.

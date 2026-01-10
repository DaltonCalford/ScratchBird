# ScratchBird ODBC Driver Specification

## 1. Overview

### 1.1 Purpose

The ScratchBird ODBC driver provides standard ODBC connectivity for:
1. **Client Applications** connecting TO ScratchBird databases
2. **ScratchBird Foreign Tables** connecting FROM ScratchBird to external databases (MSSQL, Oracle, DB2, etc.)

**Scope Note:** MSSQL external connectivity is post-gold; MSSQL examples are forward-looking.

### 1.2 ODBC Version

- **ODBC 3.8** compliance (with ODBC 3.52 backwards compatibility)
- Supports Unicode (SQLxxxW functions) and ANSI (SQLxxxA functions)
- Connection pooling aware (ODBC Driver Manager pooling)

### 1.3 Supported Platforms

| Platform | Architecture | Library |
|----------|--------------|---------|
| Linux | x86_64, aarch64 | `libscratchbird_odbc.so` |
| macOS | x86_64, arm64 | `libscratchbird_odbc.dylib` |
| Windows | x64, x86 | `scratchbird_odbc.dll` |

### 1.4 Alpha Limitations (Core/Basic)

- Core/Basic ODBC only (API Level 1, SQL Core, SQL-92 Entry).
- SQLBrowseConnect is not supported (returns HYC00).
- SQLCancel is not supported (returns HYC00).
- Multiple result sets are not supported (SQLMoreResults returns SQL_NO_DATA).
- Positioned updates and bulk operations are not supported (SQLSetPos/SQLBulkOperations return HYC00).
- Descriptor handles are not exposed (SQL_ATTR_IMP_ROW_DESC / SQL_ATTR_IMP_PARAM_DESC return NULL).
- ODBC connects via the network listener only (no direct embedded engine access).

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  Application (Excel, Tableau, Python, etc.)                      │
└─────────────────────┬───────────────────────────────────────────┘
                      │ ODBC API calls
┌─────────────────────▼───────────────────────────────────────────┐
│  ODBC Driver Manager (unixODBC, iODBC, Windows DM)              │
└─────────────────────┬───────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────┐
│  ScratchBird ODBC Driver                                         │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  Connection Manager                                       │  │
│  │  - Connection string parsing                              │  │
│  │  - SSL/TLS negotiation                                    │  │
│  │  - Authentication handling                                │  │
│  └───────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  Statement Processor                                      │  │
│  │  - SQL parsing and validation                             │  │
│  │  - Parameter binding                                      │  │
│  │  - Result set handling                                    │  │
│  └───────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  Protocol Layer                                           │  │
│  │  - ScratchBird Native Protocol (default)                  │  │
│  │  - PostgreSQL Protocol (compatibility mode)               │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │ TCP/IP or Unix Socket
┌─────────────────────▼───────────────────────────────────────────┐
│  ScratchBird Server                                              │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Installation

### 3.1 Linux (unixODBC)

```bash
# Install driver
sudo cp libscratchbird_odbc.so /usr/lib/x86_64-linux-gnu/odbc/

# Register driver in /etc/odbcinst.ini
[ScratchBird]
Description = ScratchBird ODBC Driver
Driver = /usr/lib/x86_64-linux-gnu/odbc/libscratchbird_odbc.so
Setup = /usr/lib/x86_64-linux-gnu/odbc/libscratchbird_odbc.so
UsageCount = 1

# Create DSN in /etc/odbc.ini or ~/.odbc.ini
[MyDatabase]
Description = My ScratchBird Database
Driver = ScratchBird
Server = localhost
Port = 3092
Database = mydb
```

### 3.2 Windows

```
; Registry or ODBC Data Source Administrator
[ODBC Data Sources]
MyDatabase = ScratchBird ODBC Driver

[MyDatabase]
Driver = C:\Program Files\ScratchBird\bin\scratchbird_odbc.dll
Server = localhost
Port = 3092
Database = mydb
```

### 3.3 macOS (iODBC)

```bash
# Install via Homebrew
brew install scratchbird-odbc

# Configure in ~/Library/ODBC/odbc.ini
[MyDatabase]
Driver = /usr/local/lib/libscratchbird_odbc.dylib
Server = localhost
Port = 3092
Database = mydb
```

---

## 4. Connection String

### 4.1 DSN Connection

```
DSN=MyDatabase;UID=user;PWD=password
```

### 4.2 DSN-less Connection

```
Driver={ScratchBird};Server=localhost;Port=3092;Database=mydb;UID=user;PWD=password
```

### 4.3 Connection String Parameters

| Parameter | Alias | Default | Description |
|-----------|-------|---------|-------------|
| `Driver` | - | - | Driver name (required for DSN-less) |
| `DSN` | - | - | Data Source Name |
| `Server` | `Host` | localhost | Server hostname or IP |
| `Port` | - | 3092 | Server port |
| `Database` | `DB` | - | Database name |
| `UID` | `User` | - | Username |
| `PWD` | `Password` | - | Password |
| `SSL` | `SSLMode` | prefer | disable, allow, prefer, require, verify-ca, verify-full |
| `SSLCert` | - | - | Client certificate path |
| `SSLKey` | - | - | Client private key path |
| `SSLRootCert` | - | - | CA certificate path |
| `Protocol` | - | native | native, postgresql (compatibility) |
| `Timeout` | `ConnectTimeout` | 30 | Connection timeout (seconds) |
| `QueryTimeout` | - | 0 | Query timeout (seconds, 0=unlimited) |
| `ApplicationName` | `App` | - | Application identifier |
| `Schema` | `CurrentSchema` | public | Default schema |
| `Charset` | `Encoding` | UTF8 | Character encoding |
| `ReadOnly` | - | false | Read-only connection |
| `AutoCommit` | - | true | Auto-commit mode |
| `PacketSize` | - | 8192 | Network packet size |
| `Pooling` | - | true | Enable connection pooling |

### 4.4 Example Connection Strings

```
# Basic connection
Driver={ScratchBird};Server=db.example.com;Database=production;UID=app;PWD=secret

# SSL with certificate authentication
Driver={ScratchBird};Server=db.example.com;Port=3092;Database=production;SSL=verify-full;SSLCert=/path/to/client.crt;SSLKey=/path/to/client.key;SSLRootCert=/path/to/ca.crt

# PostgreSQL compatibility mode
Driver={ScratchBird};Server=localhost;Port=5432;Database=mydb;Protocol=postgresql;UID=user;PWD=pass

# Unix socket connection
Driver={ScratchBird};Server=/var/run/scratchbird/.s.SBIRD.3092;Database=mydb;UID=user
```

---

## 5. ODBC API Implementation

### 5.1 Core Functions

| Function | Status | Notes |
|----------|--------|-------|
| `SQLAllocHandle` | ✅ Full | ENV, DBC, STMT, DESC |
| `SQLFreeHandle` | ✅ Full | All handle types |
| `SQLConnect` | ✅ Full | DSN connection |
| `SQLDriverConnect` | ✅ Full | Connection string |
| `SQLBrowseConnect` | ✅ Full | Iterative connection |
| `SQLDisconnect` | ✅ Full | |
| `SQLGetInfo` | ✅ Full | 200+ info types |
| `SQLGetFunctions` | ✅ Full | Function availability |
| `SQLGetTypeInfo` | ✅ Full | All ScratchBird types |

### 5.2 Statement Functions

| Function | Status | Notes |
|----------|--------|-------|
| `SQLPrepare` | ✅ Full | Prepared statements |
| `SQLExecute` | ✅ Full | Execute prepared |
| `SQLExecDirect` | ✅ Full | Direct execution |
| `SQLCancel` | ✅ Full | Cancel execution |
| `SQLCloseCursor` | ✅ Full | |
| `SQLNumParams` | ✅ Full | Parameter count |
| `SQLDescribeParam` | ✅ Full | Parameter metadata |
| `SQLBindParameter` | ✅ Full | All SQL types |
| `SQLNumResultCols` | ✅ Full | Column count |
| `SQLDescribeCol` | ✅ Full | Column metadata |
| `SQLColAttribute` | ✅ Full | Extended attributes |
| `SQLBindCol` | ✅ Full | Column binding |
| `SQLFetch` | ✅ Full | Row-by-row fetch |
| `SQLFetchScroll` | ✅ Full | Scrollable cursors |
| `SQLGetData` | ✅ Full | Unbound retrieval |
| `SQLSetPos` | ✅ Full | Positioned operations |
| `SQLBulkOperations` | ✅ Full | Bulk insert |
| `SQLMoreResults` | ✅ Full | Multiple result sets |
| `SQLRowCount` | ✅ Full | Affected rows |

### 5.3 Catalog Functions

| Function | Status | Notes |
|----------|--------|-------|
| `SQLTables` | ✅ Full | List tables/views |
| `SQLColumns` | ✅ Full | Column metadata |
| `SQLPrimaryKeys` | ✅ Full | Primary key info |
| `SQLForeignKeys` | ✅ Full | Foreign key info |
| `SQLStatistics` | ✅ Full | Index statistics |
| `SQLSpecialColumns` | ✅ Full | Unique/rowid columns |
| `SQLProcedures` | ✅ Full | Stored procedures |
| `SQLProcedureColumns` | ✅ Full | Procedure parameters |
| `SQLTablePrivileges` | ✅ Full | Table permissions |
| `SQLColumnPrivileges` | ✅ Full | Column permissions |

### 5.4 Transaction Functions

| Function | Status | Notes |
|----------|--------|-------|
| `SQLSetConnectAttr` (SQL_ATTR_AUTOCOMMIT) | ✅ Full | Auto-commit mode |
| `SQLEndTran` | ✅ Full | Commit/rollback |
| `SQLSetConnectAttr` (SQL_ATTR_TXN_ISOLATION) | ✅ Full | Isolation levels |

### 5.5 Diagnostic Functions

| Function | Status | Notes |
|----------|--------|-------|
| `SQLGetDiagRec` | ✅ Full | Error records |
| `SQLGetDiagField` | ✅ Full | Diagnostic fields |
| `SQLError` | ✅ Full | ODBC 2.x compat |

---

## 6. Data Type Mapping

### 6.1 ScratchBird to ODBC Type Mapping

| ScratchBird Type | ODBC SQL Type | ODBC C Type | Size |
|------------------|---------------|-------------|------|
| BOOLEAN | SQL_BIT | SQL_C_BIT | 1 |
| SMALLINT | SQL_SMALLINT | SQL_C_SHORT | 2 |
| INTEGER | SQL_INTEGER | SQL_C_LONG | 4 |
| BIGINT | SQL_BIGINT | SQL_C_SBIGINT | 8 |
| REAL | SQL_REAL | SQL_C_FLOAT | 4 |
| DOUBLE PRECISION | SQL_DOUBLE | SQL_C_DOUBLE | 8 |
| NUMERIC(p,s) | SQL_NUMERIC | SQL_C_NUMERIC | varies |
| DECIMAL(p,s) | SQL_DECIMAL | SQL_C_NUMERIC | varies |
| CHAR(n) | SQL_CHAR | SQL_C_CHAR | n |
| VARCHAR(n) | SQL_VARCHAR | SQL_C_CHAR | n |
| TEXT | SQL_LONGVARCHAR | SQL_C_CHAR | varies |
| BYTEA | SQL_LONGVARBINARY | SQL_C_BINARY | varies |
| DATE | SQL_TYPE_DATE | SQL_C_TYPE_DATE | 10 |
| TIME | SQL_TYPE_TIME | SQL_C_TYPE_TIME | 8 |
| TIMESTAMP | SQL_TYPE_TIMESTAMP | SQL_C_TYPE_TIMESTAMP | 26 |
| INTERVAL | SQL_INTERVAL_* | SQL_C_INTERVAL_* | varies |
| UUID | SQL_GUID | SQL_C_GUID | 16 |
| JSON | SQL_LONGVARCHAR | SQL_C_CHAR | varies |
| JSONB | SQL_LONGVARBINARY | SQL_C_BINARY | varies |
| ARRAY | SQL_ARRAY (ext) | SQL_C_CHAR (JSON) | varies |
| INET | SQL_VARCHAR | SQL_C_CHAR | 45 |
| CIDR | SQL_VARCHAR | SQL_C_CHAR | 49 |
| MACADDR | SQL_VARCHAR | SQL_C_CHAR | 17 |

### 6.2 ODBC to ScratchBird Type Mapping

| ODBC SQL Type | ScratchBird Type |
|---------------|------------------|
| SQL_BIT | BOOLEAN |
| SQL_TINYINT | SMALLINT |
| SQL_SMALLINT | SMALLINT |
| SQL_INTEGER | INTEGER |
| SQL_BIGINT | BIGINT |
| SQL_REAL | REAL |
| SQL_FLOAT | DOUBLE PRECISION |
| SQL_DOUBLE | DOUBLE PRECISION |
| SQL_NUMERIC | NUMERIC |
| SQL_DECIMAL | DECIMAL |
| SQL_CHAR | CHAR |
| SQL_VARCHAR | VARCHAR |
| SQL_LONGVARCHAR | TEXT |
| SQL_BINARY | BYTEA |
| SQL_VARBINARY | BYTEA |
| SQL_LONGVARBINARY | BYTEA |
| SQL_TYPE_DATE | DATE |
| SQL_TYPE_TIME | TIME |
| SQL_TYPE_TIMESTAMP | TIMESTAMP |
| SQL_GUID | UUID |

---

## 7. SQLGetInfo Values

### 7.1 Driver Information

| InfoType | Value |
|----------|-------|
| SQL_DRIVER_NAME | "ScratchBird ODBC Driver" |
| SQL_DRIVER_VER | "01.00.0000" |
| SQL_DRIVER_ODBC_VER | "03.80" |
| SQL_ODBC_VER | "03.80.0000" |
| SQL_ODBC_API_CONFORMANCE | SQL_OAC_LEVEL1 |
| SQL_ODBC_SQL_CONFORMANCE | SQL_OSC_CORE |

### 7.2 DBMS Information

| InfoType | Value |
|----------|-------|
| SQL_DBMS_NAME | "ScratchBird" |
| SQL_DBMS_VER | "01.00.0000" |
| SQL_DATABASE_NAME | (current database) |
| SQL_SERVER_NAME | (server hostname) |
| SQL_USER_NAME | (current user) |

### 7.3 SQL Conformance

| InfoType | Value |
|----------|-------|
| SQL_SQL_CONFORMANCE | SQL_SC_SQL92_ENTRY |
| SQL_SQL92_DATETIME_FUNCTIONS | SQL_SDF_CURRENT_DATE \| SQL_SDF_CURRENT_TIME \| SQL_SDF_CURRENT_TIMESTAMP |
| SQL_SQL92_NUMERIC_VALUE_FUNCTIONS | SQL_SNVF_BIT_LENGTH \| SQL_SNVF_CHAR_LENGTH \| ... |
| SQL_SQL92_STRING_FUNCTIONS | SQL_SSF_CONVERT \| SQL_SSF_LOWER \| SQL_SSF_UPPER \| ... |
| SQL_SQL92_PREDICATES | SQL_SP_BETWEEN \| SQL_SP_COMPARISON \| SQL_SP_EXISTS \| ... |

### 7.4 Transaction Support

| InfoType | Value |
|----------|-------|
| SQL_TXN_CAPABLE | SQL_TC_ALL |
| SQL_TXN_ISOLATION_OPTION | SQL_TXN_READ_UNCOMMITTED \| SQL_TXN_READ_COMMITTED \| SQL_TXN_REPEATABLE_READ \| SQL_TXN_SERIALIZABLE |
| SQL_DEFAULT_TXN_ISOLATION | SQL_TXN_READ_COMMITTED |

### 7.5 Limits

| InfoType | Value |
|----------|-------|
| SQL_MAX_CATALOG_NAME_LEN | 128 |
| SQL_MAX_SCHEMA_NAME_LEN | 128 |
| SQL_MAX_TABLE_NAME_LEN | 128 |
| SQL_MAX_COLUMN_NAME_LEN | 128 |
| SQL_MAX_COLUMNS_IN_INDEX | 32 |
| SQL_MAX_COLUMNS_IN_TABLE | 1600 |
| SQL_MAX_STATEMENT_LEN | 0 (unlimited) |

---

## 8. Error Handling

### 8.1 SQLSTATE Codes

| SQLSTATE | Description |
|----------|-------------|
| 00000 | Success |
| 01000 | General warning |
| 01004 | String data, right truncated |
| 07002 | COUNT field incorrect |
| 08001 | Unable to connect |
| 08003 | Connection not open |
| 08004 | Server rejected connection |
| 08S01 | Communication link failure |
| 22001 | String data, right truncated |
| 22003 | Numeric value out of range |
| 22007 | Invalid datetime format |
| 22012 | Division by zero |
| 23000 | Integrity constraint violation |
| 23505 | Unique violation |
| 23503 | Foreign key violation |
| 40001 | Serialization failure |
| 40003 | Statement completion unknown |
| 42000 | Syntax error or access violation |
| 42601 | Syntax error |
| 42703 | Undefined column |
| 42P01 | Undefined table |
| HY000 | General error |
| HY001 | Memory allocation error |
| HY010 | Function sequence error |
| HY090 | Invalid string or buffer length |
| HYC00 | Optional feature not implemented |
| IM001 | Driver does not support this function |

### 8.2 Native Error Codes

Native error codes map to ScratchBird SQLSTATE extensions:

```c
// Get detailed error information
SQLCHAR sqlstate[6], message[256];
SQLINTEGER native_error;
SQLSMALLINT msg_len;

SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1,
              sqlstate, &native_error, message, sizeof(message), &msg_len);
```

---

## 9. Performance Optimization

### 9.1 Connection Pooling

The driver is connection pooling aware:

```c
// Enable pooling before allocating environment
SQLSetEnvAttr(SQL_NULL_HANDLE, SQL_ATTR_CONNECTION_POOLING,
              (SQLPOINTER)SQL_CP_ONE_PER_DRIVER, 0);

SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
SQLSetEnvAttr(henv, SQL_ATTR_CP_MATCH, (SQLPOINTER)SQL_CP_RELAXED_MATCH, 0);
```

### 9.2 Array Binding

For bulk operations:

```c
#define ARRAY_SIZE 100

SQLULEN rows_processed;
SQLUSMALLINT row_status[ARRAY_SIZE];

SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER)ARRAY_SIZE, 0);
SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMS_PROCESSED_PTR, &rows_processed, 0);
SQLSetStmtAttr(hstmt, SQL_ATTR_PARAM_STATUS_PTR, row_status, 0);

// Bind arrays
SQLINTEGER ids[ARRAY_SIZE];
SQLCHAR names[ARRAY_SIZE][50];
SQLLEN id_lens[ARRAY_SIZE], name_lens[ARRAY_SIZE];

SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                 0, 0, ids, 0, id_lens);
SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                 50, 0, names, 50, name_lens);

SQLExecDirect(hstmt, "INSERT INTO users (id, name) VALUES (?, ?)", SQL_NTS);
```

### 9.3 Fetch Optimization

```c
// Set row array size for bulk fetch
SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)100, 0);
SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR, &rows_fetched, 0);
```

### 9.4 Statement Caching

The driver caches prepared statements:

```c
// Connection attribute to control cache size
SQLSetConnectAttr(hdbc, SQL_ATTR_STATEMENT_CACHE_SIZE, (SQLPOINTER)100, 0);
```

---

## 10. ODBC Foreign Data Wrapper

### 10.1 odbc_fdw for External Database Access

ScratchBird includes `odbc_fdw` for connecting to any ODBC-accessible database:

```sql
-- Create foreign server using ODBC
CREATE SERVER mssql_server
    FOREIGN DATA WRAPPER odbc_fdw
    OPTIONS (
        dsn 'MSSQLProduction',
        -- OR connection string:
        -- driver 'ODBC Driver 17 for SQL Server',
        -- server 'mssql.example.com',
        -- database 'production'
    );

-- Create user mapping
CREATE USER MAPPING FOR CURRENT_USER
    SERVER mssql_server
    OPTIONS (
        username 'sa',
        password 'secret'
    );

-- Import foreign schema
IMPORT FOREIGN SCHEMA dbo
    FROM SERVER mssql_server
    INTO mssql_schema;

-- Or create individual foreign table
CREATE FOREIGN TABLE mssql_customers (
    customer_id INTEGER,
    name VARCHAR(100),
    email VARCHAR(255)
)
SERVER mssql_server
OPTIONS (
    schema 'dbo',
    table 'Customers'
);

-- Query external data
SELECT * FROM mssql_schema.Customers WHERE region = 'EMEA';
```

### 10.2 Supported External Databases via ODBC

| Database | ODBC Driver | Notes |
|----------|-------------|-------|
| SQL Server | ODBC Driver 17/18 | Planned (post-gold) |
| Oracle | Oracle Instant Client | Full support |
| DB2 | IBM DB2 ODBC Driver | Full support |
| Teradata | Teradata ODBC Driver | Full support |
| SAP HANA | SAP HANA ODBC Driver | Full support |
| Snowflake | Snowflake ODBC Driver | Full support |
| BigQuery | Simba BigQuery ODBC | Read-only recommended |
| Access | Microsoft Access Driver | Read-only recommended |

### 10.3 Query Pushdown

The `odbc_fdw` supports pushdown of:
- WHERE clauses (when operators are supported)
- ORDER BY (when sorting matches)
- LIMIT/OFFSET (database-specific syntax translation)
- Aggregate functions (SUM, COUNT, AVG, MIN, MAX)
- JOINs between tables on same server

```sql
-- This query pushes most operations to SQL Server (post-gold)
SELECT region, COUNT(*), SUM(revenue)
FROM mssql_schema.Orders
WHERE order_date >= '2024-01-01'
GROUP BY region
ORDER BY SUM(revenue) DESC
LIMIT 10;
```

---

## 11. Configuration Reference

### 11.1 Driver-Level Settings (odbcinst.ini)

```ini
[ScratchBird]
Description = ScratchBird ODBC Driver
Driver = /usr/lib/odbc/libscratchbird_odbc.so
Setup = /usr/lib/odbc/libscratchbird_odbc.so
UsageCount = 1
Threading = 2          # Thread-safe
FileUsage = 0          # Network database
APILevel = 2           # ODBC API Level 2
SQLLevel = 3           # SQL92 Full
ConnectFunctions = YYY # SQLConnect, SQLDriverConnect, SQLBrowseConnect
CPTimeout = 60         # Connection pool timeout
```

### 11.2 DSN Settings (odbc.ini)

```ini
[ProductionDB]
Description = Production ScratchBird Database
Driver = ScratchBird
Server = db.example.com
Port = 3092
Database = production
SSL = verify-full
SSLRootCert = /etc/ssl/certs/ca-certificates.crt
Timeout = 30
QueryTimeout = 300
Charset = UTF8
ReadOnly = false
AutoCommit = true
Trace = false
TraceFile = /var/log/odbc_trace.log
```

### 11.3 Environment Variables

| Variable | Description |
|----------|-------------|
| `SCRATCHBIRD_ODBC_LOG` | Log file path |
| `SCRATCHBIRD_ODBC_LOG_LEVEL` | debug, info, warn, error |
| `SCRATCHBIRD_ODBC_TIMEOUT` | Default connection timeout |
| `SCRATCHBIRD_ODBC_SSL_MODE` | Default SSL mode |

---

## 12. Troubleshooting

### 12.1 Connection Issues

```bash
# Test ODBC connection
isql -v MyDSN username password

# Enable ODBC tracing
export ODBCINI=/etc/odbc.ini
export ODBCSYSINI=/etc
export ODBCINSTINI=/etc/odbcinst.ini

# Add to odbc.ini
[ODBC]
Trace = yes
TraceFile = /tmp/odbc.log
```

### 12.2 Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| IM002 | DSN not found | Check odbc.ini path and DSN name |
| 01000 | Driver not found | Verify odbcinst.ini driver path |
| 08001 | Server unreachable | Check server, port, firewall |
| 08004 | Auth failed | Verify username/password |
| HY000 | General error | Check server logs |

### 12.3 Performance Issues

```sql
-- Check ODBC connection statistics
SELECT * FROM SYS$ODBC_CONNECTIONS;

-- View query pushdown decisions
EXPLAIN VERBOSE SELECT * FROM foreign_table WHERE x = 1;
```

---

## 13. Application Examples

### 13.1 Python (pyodbc)

```python
import pyodbc

# DSN connection
conn = pyodbc.connect('DSN=MyDatabase;UID=user;PWD=password')

# DSN-less connection
conn = pyodbc.connect(
    'Driver={ScratchBird};'
    'Server=localhost;'
    'Port=3092;'
    'Database=mydb;'
    'UID=user;'
    'PWD=password'
)

cursor = conn.cursor()
cursor.execute("SELECT * FROM users WHERE active = ?", True)
for row in cursor.fetchall():
    print(row)

conn.close()
```

### 13.2 C/C++

```c
#include <sql.h>
#include <sqlext.h>

SQLHENV henv;
SQLHDBC hdbc;
SQLHSTMT hstmt;
SQLRETURN ret;

// Allocate handles
SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3_80, 0);
SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

// Connect
ret = SQLDriverConnect(hdbc, NULL,
    (SQLCHAR*)"Driver={ScratchBird};Server=localhost;Database=mydb;UID=user;PWD=pass",
    SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);

// Execute query
SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
SQLExecDirect(hstmt, (SQLCHAR*)"SELECT * FROM users", SQL_NTS);

// Fetch results
SQLCHAR name[50];
SQLINTEGER id;
while (SQLFetch(hstmt) == SQL_SUCCESS) {
    SQLGetData(hstmt, 1, SQL_C_LONG, &id, 0, NULL);
    SQLGetData(hstmt, 2, SQL_C_CHAR, name, sizeof(name), NULL);
    printf("%d: %s\n", id, name);
}

// Cleanup
SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
SQLDisconnect(hdbc);
SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
SQLFreeHandle(SQL_HANDLE_ENV, henv);
```

### 13.3 Excel / Power Query

```
1. Data → Get Data → From Other Sources → From ODBC
2. Select "ScratchBird" DSN or enter connection string
3. Enter credentials
4. Select tables or enter custom SQL
```

### 13.4 Tableau

```
1. Connect → More → Other Databases (ODBC)
2. Select "ScratchBird" DSN
3. Sign in with database credentials
4. Select schema and tables
```

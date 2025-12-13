# ODBC Connectivity

Connect to ScratchBird using ODBC drivers.

[Back to Connectivity Index](index.md) | [Back to Documentation Index](../index.md)

---

## Overview

ODBC (Open Database Connectivity) provides a standard API for database access. ScratchBird works with PostgreSQL ODBC drivers since it implements the PostgreSQL wire protocol.

---

## Driver Installation

### Linux

**PostgreSQL ODBC Driver:**
```bash
# Debian/Ubuntu
sudo apt install odbc-postgresql unixodbc

# RHEL/Fedora
sudo dnf install postgresql-odbc unixODBC
```

### Windows

1. Download PostgreSQL ODBC from [postgresql.org](https://www.postgresql.org/ftp/odbc/versions/)
2. Run installer (psqlodbc_*.msi)
3. Choose 32-bit or 64-bit based on your application

### macOS

```bash
# Using Homebrew
brew install psqlodbc unixodbc
```

---

## DSN Configuration

### Linux/macOS (odbc.ini)

Create or edit `/etc/odbc.ini` (system) or `~/.odbc.ini` (user):

```ini
[ScratchBird]
Description = ScratchBird Database
Driver = PostgreSQL
Servername = localhost
Port = 5432
Database = mydb
Username = admin
Password = secret
```

Edit `/etc/odbcinst.ini` for driver registration:

```ini
[PostgreSQL]
Description = PostgreSQL ODBC Driver
Driver = /usr/lib/x86_64-linux-gnu/odbc/psqlodbcw.so
Setup = /usr/lib/x86_64-linux-gnu/odbc/libodbcpsqlS.so
```

### Windows (ODBC Data Source Administrator)

1. Open "ODBC Data Sources" from Control Panel
2. Add new DSN (User or System)
3. Select "PostgreSQL Unicode"
4. Configure:
   - Data Source: ScratchBird
   - Server: localhost
   - Port: 5432
   - Database: mydb
   - User Name: admin
5. Test and save

---

## Connection Strings

### DSN-Based

```
DSN=ScratchBird;UID=admin;PWD=secret
```

### DSN-Less

```
Driver={PostgreSQL Unicode};Server=localhost;Port=5432;Database=mydb;Uid=admin;Pwd=secret
```

---

## Programming Examples

### Python (pyodbc)

**Install:**
```bash
pip install pyodbc
```

**Usage:**
```python
import pyodbc

# DSN connection
conn = pyodbc.connect('DSN=ScratchBird;UID=admin;PWD=secret')

# DSN-less
conn = pyodbc.connect(
    'Driver={PostgreSQL Unicode};'
    'Server=localhost;'
    'Port=5432;'
    'Database=mydb;'
    'Uid=admin;'
    'Pwd=secret'
)

cursor = conn.cursor()
cursor.execute("SELECT * FROM users WHERE id = ?", 1)
row = cursor.fetchone()

conn.close()
```

### C/C++

```c
#include <sql.h>
#include <sqlext.h>

SQLHENV env;
SQLHDBC dbc;
SQLHSTMT stmt;

// Allocate environment
SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);

// Allocate connection
SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

// Connect
SQLDriverConnect(dbc, NULL,
    (SQLCHAR*)"DSN=ScratchBird;UID=admin;PWD=secret",
    SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);

// Execute query
SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
SQLExecDirect(stmt, (SQLCHAR*)"SELECT * FROM users", SQL_NTS);

// Fetch results
while (SQLFetch(stmt) == SQL_SUCCESS) {
    // Process row
}

// Cleanup
SQLFreeHandle(SQL_HANDLE_STMT, stmt);
SQLDisconnect(dbc);
SQLFreeHandle(SQL_HANDLE_DBC, dbc);
SQLFreeHandle(SQL_HANDLE_ENV, env);
```

### .NET (System.Data.Odbc)

```csharp
using System.Data.Odbc;

var connStr = "DSN=ScratchBird;Uid=admin;Pwd=secret";

using var conn = new OdbcConnection(connStr);
conn.Open();

using var cmd = new OdbcCommand("SELECT * FROM users WHERE id = ?", conn);
cmd.Parameters.AddWithValue("@id", 1);

using var reader = cmd.ExecuteReader();
while (reader.Read())
{
    Console.WriteLine(reader["name"]);
}
```

### Excel / Access

1. Data → Get Data → From Other Sources → From ODBC
2. Select DSN "ScratchBird"
3. Enter credentials
4. Select tables to import

### Power BI

1. Get Data → ODBC
2. Select DSN or enter connection string
3. Navigator → select tables
4. Load or Transform

---

## Driver Options

Common PostgreSQL ODBC driver options:

| Option | Description | Default |
|--------|-------------|---------|
| `BoolsAsChar` | Return BOOLEANs as char | 1 |
| `MaxVarcharSize` | Max VARCHAR size | 255 |
| `MaxLongVarcharSize` | Max TEXT size | 8190 |
| `UseDeclareFetch` | Use cursors for large results | 0 |
| `Fetch` | Rows per fetch | 100 |
| `SSLmode` | SSL mode | disable |

Example with options:
```ini
[ScratchBird]
Driver = PostgreSQL
Servername = localhost
Port = 5432
Database = mydb
Username = admin
Password = secret
SSLmode = require
UseDeclareFetch = 1
Fetch = 1000
```

---

## SSL Configuration

```ini
[ScratchBird_SSL]
Driver = PostgreSQL
Servername = localhost
Port = 5432
Database = mydb
Username = admin
Password = secret
SSLmode = verify-full
SSLrootcert = /path/to/ca.crt
```

SSL modes:
- `disable` - No SSL
- `allow` - Prefer non-SSL
- `prefer` - Prefer SSL
- `require` - Require SSL
- `verify-ca` - Verify CA
- `verify-full` - Verify CA and hostname

---

## Testing Connection

### isql (Linux/macOS)

```bash
isql -v ScratchBird admin secret
```

### odbcinst

```bash
# List drivers
odbcinst -q -d

# List DSNs
odbcinst -q -s
```

---

## Troubleshooting

### "Data source name not found"

```bash
# Check DSN exists
odbcinst -q -s

# Verify odbc.ini location
echo $ODBCSYSINI
cat /etc/odbc.ini
```

### "Driver not found"

```bash
# Check driver registration
odbcinst -q -d

# Verify driver path
ls -la /usr/lib/x86_64-linux-gnu/odbc/
```

### "Connection failed"

```bash
# Test with isql
isql -v ScratchBird admin secret

# Check connectivity
nc -zv localhost 5432
```

### 32-bit vs 64-bit (Windows)

- 32-bit apps need 32-bit driver
- 64-bit apps need 64-bit driver
- Use matching ODBC administrator

---

## Performance Tips

1. **Use cursors for large results:**
   ```ini
   UseDeclareFetch = 1
   Fetch = 1000
   ```

2. **Enable connection pooling** in your application

3. **Use parameterized queries** to enable prepared statements

4. **Batch operations** when possible

---

## See Also

- [PostgreSQL Clients](postgresql-clients.md)
- [JDBC](jdbc.md)
- [SSL Setup](../configuration/ssl-setup.md)

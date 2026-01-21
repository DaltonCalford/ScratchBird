# ODBC Connectivity

Connect to ScratchBird using the native ScratchBird ODBC driver.

[Back to Connectivity Index](index.md) | [Back to Documentation Index](../index.md)

---

## Overview

ODBC (Open Database Connectivity) provides a standard API for database access. ScratchBird ships a native ODBC driver that talks to the ScratchBird network listener (default port 3092) using the ScratchBird wire protocol.

---

## Driver Installation

### Linux

1. Install unixODBC:
   ```bash
   # Debian/Ubuntu
   sudo apt install unixodbc

   # RHEL/Fedora
   sudo dnf install unixODBC
   ```
2. Build and install the ScratchBird ODBC driver (see "Build From Source").
3. Register the driver in `odbcinst.ini` (see next section).

### Windows

1. Install the ScratchBird ODBC driver (MSI).
2. Open "ODBC Data Sources" from Control Panel.
3. Add a User or System DSN for ScratchBird.

### macOS

1. Install the ScratchBird ODBC driver (dylib).
2. Register the driver with iODBC or unixODBC (see next section).

---

## Build From Source

### Linux/macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 24
cmake --install build --prefix /usr/local
```

This installs the shared driver library:
- Linux: `/usr/local/lib/libscratchbird_odbc.so`
- macOS: `/usr/local/lib/libscratchbird_odbc.dylib`

### Windows (MSVC)

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --prefix C:\ScratchBird
```

This installs `scratchbird_odbc.dll` under `C:\ScratchBird\bin`.

---

## Driver Registration (odbcinst.ini)

### Linux (system-wide)

Edit `/etc/odbcinst.ini`:

```ini
[ScratchBird]
Description = ScratchBird ODBC Driver
Driver = /usr/local/lib/libscratchbird_odbc.so
Setup = /usr/local/lib/libscratchbird_odbc.so
UsageCount = 1
```

### macOS (user)

Edit `~/Library/ODBC/odbcinst.ini`:

```ini
[ScratchBird]
Description = ScratchBird ODBC Driver
Driver = /usr/local/lib/libscratchbird_odbc.dylib
Setup = /usr/local/lib/libscratchbird_odbc.dylib
```

---

## DSN Configuration (odbc.ini)

Create or edit `/etc/odbc.ini` (system) or `~/.odbc.ini` (user):

```ini
[ScratchBird]
Description = ScratchBird Database
Driver = ScratchBird
Server = localhost
Port = 3092
Database = mydb
UID = admin
PWD = secret
```

### Windows (ODBC Data Source Administrator)

1. Open "ODBC Data Sources" from Control Panel
2. Add new DSN (User or System)
3. Select "ScratchBird ODBC Driver"
4. Configure:
   - Data Source: ScratchBird
   - Server: localhost
   - Port: 3092
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
Driver={ScratchBird ODBC Driver};Server=localhost;Port=3092;Database=mydb;UID=admin;PWD=secret
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
    'Driver={ScratchBird ODBC Driver};'
    'Server=localhost;'
    'Port=3092;'
    'Database=mydb;'
    'UID=admin;'
    'PWD=secret'
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

Common ScratchBird ODBC driver options:

| Option | Description | Default |
|--------|-------------|---------|
| `Driver` | Driver name | ScratchBird ODBC Driver |
| `Server` / `Host` | Server hostname | localhost |
| `Port` | Server port | 3092 |
| `Database` | Database name | (empty) |
| `UID` / `User` | Username | (empty) |
| `PWD` / `Password` | Password | (empty) |
| `SSLMode` | SSL mode | prefer |
| `SSLCert` | Client cert path | (empty) |
| `SSLKey` | Client key path | (empty) |
| `SSLRootCert` | CA cert path | (empty) |
| `Timeout` / `ConnectTimeout` | Connect timeout (sec) | 30 |
| `QueryTimeout` | Query timeout (sec) | 0 |
| `ApplicationName` / `App` | Application name | (empty) |
| `Schema` / `CurrentSchema` | Default schema | public |
| `Charset` / `Encoding` | Client encoding | UTF8 |
| `ReadOnly` | Read-only mode | false |
| `AutoCommit` | Autocommit mode | true |
| `PacketSize` | Packet size (bytes) | 8192 |
| `Pooling` | Driver-manager pooling hint | true |

Example with options:
```ini
[ScratchBird]
Driver = ScratchBird
Server = localhost
Port = 3092
Database = mydb
UID = admin
PWD = secret
SSLMode = require
ApplicationName = reporting
AutoCommit = true
```

---

## SSL Configuration

```ini
[ScratchBird_SSL]
Driver = ScratchBird
Server = localhost
Port = 3092
Database = mydb
UID = admin
PWD = secret
SSLMode = verify_full
SSLRootCert = /path/to/ca.crt
SSLCert = /path/to/client.crt
SSLKey = /path/to/client.key
```

SSL modes:
- `disable` - No SSL
- `allow` - Prefer non-SSL
- `prefer` - Prefer SSL
- `require` - Require SSL
- `verify_ca` - Verify CA
- `verify_full` - Verify CA and hostname

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
nc -zv localhost 3092
```

### 32-bit vs 64-bit (Windows)

- 32-bit apps need 32-bit driver
- 64-bit apps need 64-bit driver
- Use matching ODBC administrator

---

## Performance Tips

1. **Use parameterized queries** to enable prepared statements
2. **Keep result sets narrow** when possible
3. **Enable connection pooling** in your application when supported

---

## See Also

- [JDBC](jdbc.md)
- [SSL Setup](../configuration/ssl-setup.md)
- [ODBC Driver Specification](../../specifications/ODBC_DRIVER_SPECIFICATION.md)

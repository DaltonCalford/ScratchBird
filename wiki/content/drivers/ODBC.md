# ODBC Driver Guide

**Status:** Complete
**Last Updated:** 2026-01-20

---

## Overview

ODBC (Open Database Connectivity) provides a standard interface for connecting to ScratchBird from various applications, including spreadsheets, BI tools, and legacy systems.

| Protocol | Port | ODBC Driver | Best For |
|----------|------|-------------|----------|
| PostgreSQL | 5432 | psqlODBC | Most applications (recommended) |
| MySQL | 3306 | MySQL ODBC Connector | MySQL compatibility |
| Firebird | 3050 | Firebird ODBC | Firebird migration |
| Native | 3092 | ScratchBird ODBC (future) | Direct access |

**Recommendation:** Use **psqlODBC** (PostgreSQL ODBC driver) for most ODBC connections. It offers the best compatibility with ScratchBird's features.

---

## Part 1: Windows Setup

### Installing psqlODBC (PostgreSQL)

**Download and install:**
1. Visit https://www.postgresql.org/ftp/odbc/versions/
2. Download the latest `psqlodbc_xx_xx_xxxx-x64.zip` or `.msi` installer
3. Run installer with default options

**Or via command line:**
```powershell
# Using Chocolatey
choco install psqlodbc

# Or download MSI directly
Invoke-WebRequest -Uri "https://ftp.postgresql.org/pub/odbc/versions/msi/psqlodbc_16_00_0000-x64.msi" -OutFile "psqlodbc.msi"
Start-Process msiexec.exe -ArgumentList "/i", "psqlodbc.msi", "/quiet" -Wait
```

### Installing MySQL ODBC Connector

**Download and install:**
1. Visit https://dev.mysql.com/downloads/connector/odbc/
2. Download MySQL Connector/ODBC 8.x for Windows
3. Run installer

**Or via command line:**
```powershell
choco install mysql-odbc
```

### Installing Firebird ODBC

**Download and install:**
1. Visit https://firebirdsql.org/en/odbc-driver/
2. Download Firebird ODBC Driver for Windows
3. Run installer

### Configuring DSN (Data Source Name)

**Using ODBC Data Source Administrator:**

1. Open `ODBC Data Sources (64-bit)` from Start menu
   - Or run: `odbcad32.exe`
2. Select `System DSN` or `User DSN` tab
3. Click `Add...`
4. Select `PostgreSQL Unicode(x64)` (or appropriate driver)
5. Configure connection:

| Field | Value |
|-------|-------|
| Data Source | ScratchBird |
| Description | ScratchBird Database |
| Database | scratchbird |
| Server | localhost |
| Port | 5432 |
| User Name | app_user |
| Password | secret |

6. Click `Test` to verify connection
7. Click `Save`

**Connection string format:**
```
Driver={PostgreSQL Unicode(x64)};Server=localhost;Port=5432;Database=scratchbird;Uid=app_user;Pwd=secret;
```

### Registry-Based DSN (Scripted Setup)

```powershell
# Create System DSN via registry
$dsnPath = "HKLM:\SOFTWARE\ODBC\ODBC.INI\ScratchBird"
New-Item -Path $dsnPath -Force

Set-ItemProperty -Path $dsnPath -Name "Driver" -Value "C:\Program Files\psqlODBC\bin\psqlodbc35w.dll"
Set-ItemProperty -Path $dsnPath -Name "Description" -Value "ScratchBird Database"
Set-ItemProperty -Path $dsnPath -Name "Servername" -Value "localhost"
Set-ItemProperty -Path $dsnPath -Name "Port" -Value "5432"
Set-ItemProperty -Path $dsnPath -Name "Database" -Value "scratchbird"
Set-ItemProperty -Path $dsnPath -Name "Username" -Value "app_user"
Set-ItemProperty -Path $dsnPath -Name "Password" -Value "secret"

# Register DSN name
$dsnListPath = "HKLM:\SOFTWARE\ODBC\ODBC.INI\ODBC Data Sources"
Set-ItemProperty -Path $dsnListPath -Name "ScratchBird" -Value "PostgreSQL Unicode(x64)"
```

---

## Part 2: Linux Setup

### Installing psqlODBC

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install odbc-postgresql unixodbc unixodbc-dev
```

**CentOS/RHEL/Fedora:**
```bash
sudo dnf install postgresql-odbc unixODBC unixODBC-devel
```

**Arch Linux:**
```bash
sudo pacman -S psqlodbc unixodbc
```

### Installing MySQL ODBC

**Ubuntu/Debian:**
```bash
sudo apt install libmyodbc
```

**CentOS/RHEL:**
```bash
sudo dnf install mysql-connector-odbc
```

### Configuring ODBC on Linux

**Edit /etc/odbcinst.ini (driver registration):**
```ini
[PostgreSQL]
Description = ODBC for PostgreSQL
Driver = /usr/lib/x86_64-linux-gnu/odbc/psqlodbcw.so
Setup = /usr/lib/x86_64-linux-gnu/odbc/libodbcpsqlS.so
Driver64 = /usr/lib/x86_64-linux-gnu/odbc/psqlodbcw.so
Setup64 = /usr/lib/x86_64-linux-gnu/odbc/libodbcpsqlS.so
FileUsage = 1

[MySQL]
Description = ODBC for MySQL
Driver = /usr/lib/x86_64-linux-gnu/odbc/libmyodbc8w.so
FileUsage = 1
```

**Edit /etc/odbc.ini or ~/.odbc.ini (DSN configuration):**
```ini
[ScratchBird]
Description = ScratchBird Database
Driver = PostgreSQL
Servername = localhost
Port = 5432
Database = scratchbird
Username = app_user
Password = secret
SSLMode = prefer

[ScratchBird-MySQL]
Description = ScratchBird via MySQL Protocol
Driver = MySQL
Server = localhost
Port = 3306
Database = scratchbird
User = app_user
Password = secret
```

**Test connection:**
```bash
# Test DSN
isql -v ScratchBird app_user secret

# Or with connection string
isql -v "Driver=PostgreSQL;Server=localhost;Port=5432;Database=scratchbird;Uid=app_user;Pwd=secret"
```

---

## Part 3: macOS Setup

### Installing ODBC Manager and Drivers

**Install unixODBC:**
```bash
brew install unixodbc
```

**Install psqlODBC:**
```bash
brew install psqlodbc
```

**Install MySQL ODBC:**
```bash
# Download from MySQL website
# Or use Homebrew tap if available
```

### Configuring ODBC on macOS

**Create/edit ~/.odbcinst.ini:**
```ini
[PostgreSQL]
Description = PostgreSQL ODBC Driver
Driver = /opt/homebrew/lib/psqlodbcw.so
```

**Create/edit ~/.odbc.ini:**
```ini
[ScratchBird]
Description = ScratchBird Database
Driver = PostgreSQL
Servername = localhost
Port = 5432
Database = scratchbird
Username = app_user
Password = secret
```

**Or use ODBC Manager GUI:**
1. Install ODBC Manager from http://www.odbcmanager.net/
2. Open ODBC Manager
3. Add drivers and configure DSNs via GUI

---

## Part 4: Connection String Reference

### psqlODBC Connection Strings

**Basic connection:**
```
Driver={PostgreSQL Unicode(x64)};Server=localhost;Port=5432;Database=scratchbird;Uid=app_user;Pwd=secret;
```

**With SSL:**
```
Driver={PostgreSQL Unicode(x64)};Server=localhost;Port=5432;Database=scratchbird;Uid=app_user;Pwd=secret;SSLMode=require;
```

**Full options:**
```
Driver={PostgreSQL Unicode(x64)};
Server=localhost;
Port=5432;
Database=scratchbird;
Uid=app_user;
Pwd=secret;
SSLMode=prefer;
ReadOnly=0;
Protocol=7.4;
FakeOidIndex=0;
ShowOidColumn=0;
RowVersioning=0;
ShowSystemTables=0;
Fetch=100;
Socket=4096;
UnknownSizes=0;
MaxVarcharSize=255;
MaxLongVarcharSize=8190;
Debug=0;
CommLog=0;
UseDeclareFetch=0;
TextAsLongVarchar=1;
UnknownsAsLongVarchar=0;
BoolsAsChar=1;
Parse=0;
CancelAsFreeStmt=0;
ExtraSysTablePrefixes=dd_;
LFConversion=1;
UpdatableCursors=1;
DisallowPremature=0;
TrueIsMinus1=0;
BI=0;
ByteaAsLongVarBinary=1;
UseServerSidePrepare=1;
LowerCaseIdentifier=0;
```

### MySQL ODBC Connection Strings

**Basic connection:**
```
Driver={MySQL ODBC 8.0 Unicode Driver};Server=localhost;Port=3306;Database=scratchbird;User=app_user;Password=secret;
```

**With SSL:**
```
Driver={MySQL ODBC 8.0 Unicode Driver};Server=localhost;Port=3306;Database=scratchbird;User=app_user;Password=secret;SSLMODE=REQUIRED;
```

### Firebird ODBC Connection Strings

**Basic connection:**
```
Driver={Firebird/InterBase(r) driver};Dbname=localhost/3050:scratchbird;Uid=SYSDBA;Pwd=masterkey;
```

---

## Part 5: Microsoft Excel

### Connecting via ODBC

**Method 1: Data Connection Wizard**

1. Open Excel
2. Go to `Data` > `Get Data` > `From Other Sources` > `From ODBC`
3. Select your DSN (e.g., "ScratchBird")
4. Enter credentials if prompted
5. Select tables or enter SQL query
6. Click `Load`

**Method 2: Power Query**

1. Go to `Data` > `Get Data` > `From Other Sources` > `From ODBC`
2. In the dialog, select "Advanced options"
3. Enter connection string:
   ```
   Driver={PostgreSQL Unicode(x64)};Server=localhost;Port=5432;Database=scratchbird;Uid=app_user;Pwd=secret;
   ```
4. Enter SQL query in the SQL statement box
5. Click `OK`

### VBA Example

```vba
Sub QueryScratchBird()
    Dim conn As Object
    Dim rs As Object
    Dim connStr As String
    Dim sql As String
    Dim ws As Worksheet
    Dim row As Integer

    Set conn = CreateObject("ADODB.Connection")
    Set rs = CreateObject("ADODB.Recordset")

    ' Connection string
    connStr = "Driver={PostgreSQL Unicode(x64)};" & _
              "Server=localhost;Port=5432;" & _
              "Database=scratchbird;" & _
              "Uid=app_user;Pwd=secret;"

    ' SQL query
    sql = "SELECT id, name, email FROM users WHERE active = true ORDER BY name"

    ' Connect and execute
    conn.Open connStr
    rs.Open sql, conn, 3, 1  ' adOpenStatic, adLockReadOnly

    ' Output to worksheet
    Set ws = ActiveSheet
    row = 1

    ' Headers
    For i = 0 To rs.Fields.Count - 1
        ws.Cells(row, i + 1).Value = rs.Fields(i).Name
    Next i
    row = row + 1

    ' Data
    Do While Not rs.EOF
        For i = 0 To rs.Fields.Count - 1
            ws.Cells(row, i + 1).Value = rs.Fields(i).Value
        Next i
        row = row + 1
        rs.MoveNext
    Loop

    ' Cleanup
    rs.Close
    conn.Close
    Set rs = Nothing
    Set conn = Nothing

    MsgBox "Imported " & (row - 2) & " rows"
End Sub
```

---

## Part 6: Microsoft Power BI

### Connecting via ODBC

1. Open Power BI Desktop
2. Click `Get Data` > `More...`
3. Select `ODBC` from the list
4. Click `Connect`
5. Select DSN or enter connection string:
   ```
   Driver={PostgreSQL Unicode(x64)};Server=localhost;Port=5432;Database=scratchbird;Uid=app_user;Pwd=secret;
   ```
6. Enter credentials
7. Select tables or use advanced mode for SQL

### Direct Query Mode

For real-time data:

1. In the Navigator, click `Transform Data`
2. In Power Query Editor, go to `Home` > `Manage Parameters`
3. Set up connection parameters
4. Use `DirectQuery` mode for live connection

### DAX with ODBC Data

```dax
// Measure example
Total Sales =
    CALCULATE(
        SUM(orders[amount]),
        orders[status] = "completed"
    )

// Calculated column
Full Name = users[first_name] & " " & users[last_name]
```

---

## Part 7: Microsoft Access

### Linking Tables via ODBC

1. Open Access database
2. Go to `External Data` > `New Data Source` > `From Other Sources` > `ODBC Database`
3. Select `Link to the data source by creating a linked table`
4. Choose `Machine Data Source` tab
5. Select your DSN (e.g., "ScratchBird")
6. Enter credentials
7. Select tables to link
8. Click `OK`

### Pass-Through Queries

For complex queries, use pass-through:

1. Create new query in Design View
2. Close the "Show Table" dialog
3. Go to `Design` > `Pass-Through`
4. Go to `Design` > `Property Sheet`
5. Set `ODBC Connect Str` to your connection string
6. Enter SQL directly:

```sql
SELECT u.id, u.name, COUNT(o.id) as order_count
FROM users u
LEFT JOIN orders o ON u.id = o.user_id
GROUP BY u.id, u.name
ORDER BY order_count DESC
```

### VBA Connection Example

```vba
Sub ConnectToScratchBird()
    Dim db As DAO.Database
    Dim qdf As DAO.QueryDef
    Dim rs As DAO.Recordset

    Set db = CurrentDb

    ' Create pass-through query
    Set qdf = db.CreateQueryDef("")
    qdf.Connect = "ODBC;Driver={PostgreSQL Unicode(x64)};" & _
                  "Server=localhost;Port=5432;" & _
                  "Database=scratchbird;" & _
                  "Uid=app_user;Pwd=secret;"
    qdf.SQL = "SELECT * FROM users WHERE active = true"
    qdf.ReturnsRecords = True

    Set rs = qdf.OpenRecordset()

    Do While Not rs.EOF
        Debug.Print rs!name
        rs.MoveNext
    Loop

    rs.Close
    Set rs = Nothing
    Set qdf = Nothing
    Set db = Nothing
End Sub
```

---

## Part 8: Crystal Reports

### Adding ODBC Data Source

1. Open Crystal Reports
2. Create new report or open existing
3. Go to `Database` > `Database Expert`
4. Expand `Create New Connection` > `ODBC (RDO)`
5. Select your DSN or click `Make New Connection`
6. Enter credentials
7. Select tables
8. Link tables as needed
9. Click `OK`

### Connection String in Report

For embedding connection:

1. Right-click data source in Database Expert
2. Select `Set Datasource Location`
3. Update connection string:
   ```
   Driver={PostgreSQL Unicode(x64)};Server=localhost;Port=5432;Database=scratchbird;Uid=app_user;Pwd=secret;
   ```

### Parameterized Queries

```sql
-- In Command (SQL Expression)
SELECT *
FROM orders
WHERE order_date BETWEEN {?StartDate} AND {?EndDate}
AND status = {?Status}
```

---

## Part 9: Python with ODBC (pyodbc)

### Installation

```bash
pip install pyodbc
```

### Basic Usage

```python
import pyodbc

# Connection string
conn_str = (
    "Driver={PostgreSQL Unicode(x64)};"
    "Server=localhost;"
    "Port=5432;"
    "Database=scratchbird;"
    "Uid=app_user;"
    "Pwd=secret;"
)

# Connect
conn = pyodbc.connect(conn_str)
cursor = conn.cursor()

# Query
cursor.execute("SELECT id, name, email FROM users WHERE active = ?", True)

for row in cursor.fetchall():
    print(f"{row.id}: {row.name} <{row.email}>")

# Insert
cursor.execute(
    "INSERT INTO users (name, email) VALUES (?, ?)",
    ("Alice", "alice@example.com")
)
conn.commit()

# Close
cursor.close()
conn.close()
```

### Using DSN

```python
import pyodbc

# Connect via DSN
conn = pyodbc.connect("DSN=ScratchBird;UID=app_user;PWD=secret")

# Or DSN-less
conn = pyodbc.connect(
    "Driver={PostgreSQL Unicode(x64)};"
    "Server=localhost;Port=5432;"
    "Database=scratchbird;"
    "Uid=app_user;Pwd=secret;"
)
```

### With Pandas

```python
import pandas as pd
import pyodbc

conn_str = (
    "Driver={PostgreSQL Unicode(x64)};"
    "Server=localhost;Port=5432;"
    "Database=scratchbird;"
    "Uid=app_user;Pwd=secret;"
)

# Read into DataFrame
df = pd.read_sql("SELECT * FROM users WHERE active = true", conn_str)
print(df.head())

# Write DataFrame to table
df.to_sql('new_table', conn_str, if_exists='replace', index=False)
```

---

## Part 10: C/C++ with ODBC

### Basic Example

```c
#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <string.h>

int main() {
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;
    SQLRETURN ret;

    // Allocate environment handle
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);

    // Allocate connection handle
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    // Connect
    SQLCHAR connStr[] = "Driver={PostgreSQL Unicode(x64)};"
                        "Server=localhost;Port=5432;"
                        "Database=scratchbird;"
                        "Uid=app_user;Pwd=secret;";

    ret = SQLDriverConnect(dbc, NULL, connStr, SQL_NTS,
                          NULL, 0, NULL, SQL_DRIVER_NOPROMPT);

    if (SQL_SUCCEEDED(ret)) {
        printf("Connected!\n");

        // Allocate statement handle
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

        // Execute query
        SQLExecDirect(stmt,
            (SQLCHAR*)"SELECT id, name FROM users WHERE active = true",
            SQL_NTS);

        // Fetch results
        SQLINTEGER id;
        SQLCHAR name[256];
        SQLLEN idInd, nameInd;

        SQLBindCol(stmt, 1, SQL_C_LONG, &id, 0, &idInd);
        SQLBindCol(stmt, 2, SQL_C_CHAR, name, sizeof(name), &nameInd);

        while (SQL_SUCCEEDED(SQLFetch(stmt))) {
            printf("ID: %d, Name: %s\n", id, name);
        }

        // Cleanup
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    } else {
        printf("Connection failed!\n");
    }

    // Disconnect and cleanup
    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);

    return 0;
}
```

### Compilation

```bash
# Linux
gcc -o odbc_example odbc_example.c -lodbc

# Windows (MSVC)
cl odbc_example.c odbc32.lib
```

---

## Part 11: Troubleshooting

### Common Issues

**"Driver not found":**
```
# Windows: Verify driver is installed
odbcad32.exe  # Check Drivers tab

# Linux: Check odbcinst.ini
cat /etc/odbcinst.ini

# Verify driver file exists
ls -la /usr/lib/x86_64-linux-gnu/odbc/
```

**"Connection refused":**
```
# Check server is running
pg_isready -h localhost -p 5432

# Check firewall
sudo ufw status

# Test with psql first
psql -h localhost -p 5432 -U app_user -d scratchbird
```

**"Authentication failed":**
```
# Verify credentials
# Check pg_hba.conf allows your connection method
# For PostgreSQL, may need md5 or scram-sha-256
```

**"SSL errors":**
```
# Add SSL options to connection string
SSLMode=require;
# Or disable SSL for testing
SSLMode=disable;
```

### Enabling Debug Logging

**psqlODBC (Windows):**
1. Open ODBC Data Source Administrator
2. Edit your DSN
3. Click `Datasource` button
4. Check `CommLog` and `Debug`
5. Set log path

**psqlODBC (Linux):**
```ini
# In odbc.ini
[ScratchBird]
...
Debug=1
CommLog=1
```

### Testing Connections

**Windows:**
```powershell
# Test with odbcping (if available)
# Or use isql from unixODBC for Windows

# PowerShell test
$conn = New-Object System.Data.Odbc.OdbcConnection
$conn.ConnectionString = "DSN=ScratchBird;Uid=app_user;Pwd=secret;"
$conn.Open()
Write-Host "Connected: $($conn.State)"
$conn.Close()
```

**Linux/macOS:**
```bash
# Interactive test
isql -v ScratchBird app_user secret

# Non-interactive
echo "SELECT version();" | isql -b ScratchBird app_user secret
```

---

## Part 12: Performance Tuning

### Fetch Size

```
# psqlODBC - increase fetch size for large results
Fetch=1000;
```

### Connection Pooling

```
# Use ODBC connection pooling
# Windows: Enabled in ODBC Administrator > Connection Pooling tab

# Linux: Set in odbcinst.ini
[ODBC]
Pooling=Yes

# Or per-driver
[PostgreSQL]
CPTimeout=120
```

### Prepared Statements

```
# psqlODBC - enable server-side prepare
UseServerSidePrepare=1;
```

### Binary Mode for BLOBs

```
# psqlODBC
ByteaAsLongVarBinary=1;
```

---

## Quick Reference

### Connection String Templates

**PostgreSQL (psqlODBC):**
```
Driver={PostgreSQL Unicode(x64)};Server=localhost;Port=5432;Database=scratchbird;Uid=app_user;Pwd=secret;
```

**MySQL:**
```
Driver={MySQL ODBC 8.0 Unicode Driver};Server=localhost;Port=3306;Database=scratchbird;User=app_user;Password=secret;
```

**Firebird:**
```
Driver={Firebird/InterBase(r) driver};Dbname=localhost/3050:scratchbird;Uid=SYSDBA;Pwd=masterkey;
```

### ODBC Files by Platform

| Platform | Driver Config | DSN Config |
|----------|---------------|------------|
| Windows | Registry | Registry or .dsn files |
| Linux | /etc/odbcinst.ini | /etc/odbc.ini or ~/.odbc.ini |
| macOS | ~/.odbcinst.ini | ~/.odbc.ini |

### Common ODBC Functions

| Function | Purpose |
|----------|---------|
| SQLConnect | Connect using DSN |
| SQLDriverConnect | Connect using connection string |
| SQLExecDirect | Execute SQL directly |
| SQLPrepare | Prepare statement |
| SQLExecute | Execute prepared statement |
| SQLFetch | Fetch next row |
| SQLBindCol | Bind column to variable |
| SQLBindParameter | Bind parameter |
| SQLGetDiagRec | Get error details |
| SQLDisconnect | Disconnect |

### Tool Compatibility Matrix

| Tool | PostgreSQL | MySQL | Firebird |
|------|------------|-------|----------|
| Excel | Yes | Yes | Yes |
| Power BI | Yes | Yes | Yes |
| Access | Yes | Yes | Yes |
| Crystal Reports | Yes | Yes | Yes |
| Tableau | Yes | Yes | Yes |
| SSRS | Yes | Yes | Limited |

---

## See Also

- [Driver Comparison](Driver-Comparison.md) - Compare all available drivers
- [Connection Guide](../getting-started/first-connection.md) - First connection walkthrough
- [Performance Tuning](../user-guides/Performance-Tuning.md) - Optimize database performance
- [Windows Installation](../installation/Windows.md) - Windows setup guide

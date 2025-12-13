# Firebird Client Compatibility

Connect to ScratchBird using Firebird clients and drivers.

[Back to Connectivity Index](index.md) | [Back to Documentation Index](../index.md)

---

## Overview

ScratchBird implements the Firebird wire protocol, providing compatibility with Firebird clients, drivers, and tools.

**Default Port:** 3050

---

## GUI Tools

### FlameRobin

Cross-platform Firebird administration tool.

**Install:**
```bash
# Debian/Ubuntu
sudo apt install flamerobin

# macOS
brew install --cask flamerobin
```

**Connect:**
1. Server → Register Server
2. Name: ScratchBird
3. Hostname: localhost
4. Port: 3050
5. Register database under server
6. Database path: /path/to/database.sbdb
7. Username: admin

### IBExpert (Windows)

Professional Firebird administration.

1. Database → Register Database
2. Server: localhost:3050
3. Database: /path/to/database
4. User: admin
5. Connect

### Database Workbench

Commercial cross-database tool with Firebird support.

1. New connection → Firebird
2. Server: localhost
3. Port: 3050
4. Enter credentials

---

## Command-Line Tools

### isql (Firebird)

Firebird's interactive SQL tool.

```bash
isql -user admin -password secret localhost:3050:/path/to/database.sbdb
```

### ScratchBird's Firebird-Compatible isql

```bash
sb_fb_isql localhost:3050:/path/to/database.sbdb -user admin
```

---

## Programming Language Drivers

### Python - fdb

**Install:**
```bash
pip install fdb
```

**Usage:**
```python
import fdb

conn = fdb.connect(
    host='localhost',
    port=3050,
    database='/path/to/database.sbdb',
    user='admin',
    password='secret'
)

cur = conn.cursor()
cur.execute("SELECT * FROM users WHERE id = ?", (1,))
row = cur.fetchone()

cur.close()
conn.close()
```

### Python - firebird-driver

Modern async-capable Firebird driver.

```python
from firebird.driver import connect

with connect('localhost:3050/path/to/database.sbdb',
             user='admin', password='secret') as conn:
    cur = conn.cursor()
    cur.execute("SELECT * FROM users")
    for row in cur:
        print(row)
```

### Java - Jaybird (JDBC)

**Maven:**
```xml
<dependency>
    <groupId>org.firebirdsql.jdbc</groupId>
    <artifactId>jaybird</artifactId>
    <version>5.0.0.java11</version>
</dependency>
```

**Usage:**
```java
String url = "jdbc:firebird://localhost:3050/path/to/database.sbdb";
Connection conn = DriverManager.getConnection(url, "admin", "secret");

PreparedStatement stmt = conn.prepareStatement("SELECT * FROM users WHERE id = ?");
stmt.setInt(1, 1);
ResultSet rs = stmt.executeQuery();

while (rs.next()) {
    System.out.println(rs.getString("name"));
}

conn.close();
```

### .NET - FirebirdSql.Data.FirebirdClient

**NuGet:**
```bash
dotnet add package FirebirdSql.Data.FirebirdClient
```

**Usage:**
```csharp
using FirebirdSql.Data.FirebirdClient;

var connStr = "User=admin;Password=secret;Database=/path/to/db.sbdb;DataSource=localhost;Port=3050";

using var conn = new FbConnection(connStr);
conn.Open();

using var cmd = new FbCommand("SELECT * FROM users WHERE id = @id", conn);
cmd.Parameters.AddWithValue("@id", 1);

using var reader = cmd.ExecuteReader();
while (reader.Read())
{
    Console.WriteLine(reader["name"]);
}
```

### PHP - interbase / firebird

```php
<?php
$conn = ibase_connect(
    'localhost:3050/path/to/database.sbdb',
    'admin',
    'secret'
);

$result = ibase_query($conn, "SELECT * FROM users");
while ($row = ibase_fetch_assoc($result)) {
    echo $row['name'];
}

ibase_close($conn);
```

### Delphi / Free Pascal

```pascal
uses IBDatabase, IBQuery;

var
  Database: TIBDatabase;
  Query: TIBQuery;
begin
  Database := TIBDatabase.Create(nil);
  Database.DatabaseName := 'localhost:3050:/path/to/database.sbdb';
  Database.Params.Add('user_name=admin');
  Database.Params.Add('password=secret');
  Database.Connected := True;

  Query := TIBQuery.Create(nil);
  Query.Database := Database;
  Query.SQL.Text := 'SELECT * FROM users';
  Query.Open;

  while not Query.EOF do
  begin
    WriteLn(Query.FieldByName('name').AsString);
    Query.Next;
  end;
end;
```

---

## Connection String Formats

### Standard Format

```
host:port/database
localhost:3050/path/to/database.sbdb
```

### JDBC Format

```
jdbc:firebird://localhost:3050/path/to/database.sbdb
jdbc:firebirdsql://localhost:3050/path/to/database.sbdb
```

### .NET Format

```
User=admin;Password=secret;Database=/path/to/db.sbdb;DataSource=localhost;Port=3050;Charset=UTF8
```

---

## Firebird-Specific Syntax

When connected via Firebird protocol, ScratchBird accepts Firebird SQL syntax:

```sql
-- FIRST/SKIP (instead of LIMIT)
SELECT FIRST 10 SKIP 20 * FROM users;

-- Parameter markers
SELECT * FROM users WHERE id = ?;

-- Quoted identifiers
SELECT "Name" FROM "Users";

-- EXECUTE BLOCK
EXECUTE BLOCK
AS
DECLARE VARIABLE x INTEGER;
BEGIN
    x = 1;
END
```

---

## SQL Dialect

Firebird supports three SQL dialects. ScratchBird defaults to dialect 3:

| Dialect | Description |
|---------|-------------|
| 1 | Legacy InterBase compatibility |
| 2 | Transitional (deprecated) |
| 3 | Modern Firebird (recommended) |

Set dialect in connection:
```python
conn = fdb.connect(..., sql_dialect=3)
```

---

## Character Sets

Firebird uses character sets for encoding:

```sql
-- Connection charset
SET NAMES UTF8;

-- Table column charset
CREATE TABLE texts (
    content VARCHAR(1000) CHARACTER SET UTF8
);
```

Common charsets: UTF8, WIN1252, ISO8859_1

---

## Known Differences

| Feature | ScratchBird | Firebird |
|---------|-------------|----------|
| Page size | Internal | 4K-16K selectable |
| Generators | Supported | Supported |
| Domains | Supported | Supported |
| Stored procedures | Supported | Supported |
| EXECUTE BLOCK | Supported | Supported |

---

## Troubleshooting

### "Connection refused"

```bash
# Check Firebird port
ss -tlnp | grep 3050
```

### "Database not found"

Use full path to database:
```
localhost:3050:/var/lib/scratchbird/mydb.sbdb
```

### "Charset conversion error"

Specify UTF8 charset in connection:
```python
conn = fdb.connect(..., charset='UTF8')
```

---

## Firebird Migration

If migrating from Firebird to ScratchBird:

1. Export with gbak or isql
2. Review schema for compatibility
3. Import to ScratchBird
4. Test application

```bash
# Export from Firebird
gbak -b -user SYSDBA -password masterkey source.fdb backup.fbk

# Restore SQL
isql -o schema.sql -x source.fdb
```

---

## See Also

- [First Connection](../getting-started/first-connection.md)
- [Migration from PostgreSQL](../getting-started/tutorials/migration-from-postgres.md)
- [SQL Language Guide](../language-guide/index.md)

# MySQL Client Compatibility

Connect to ScratchBird using MySQL clients and drivers.

[Back to Connectivity Index](index.md) | [Back to Documentation Index](../index.md)

---

## Overview

ScratchBird implements the MySQL wire protocol, providing compatibility with MySQL clients, drivers, and tools.

**Default Port:** 3306

---

## Command-Line Client

### mysql CLI

**Install:**
```bash
# Debian/Ubuntu
sudo apt install mysql-client

# RHEL/Fedora
sudo dnf install mysql

# macOS
brew install mysql-client
```

**Connect:**
```bash
# Use 127.0.0.1 instead of localhost for TCP
mysql -h 127.0.0.1 -P 3306 -u admin -p mydb
```

**Note:** Use `127.0.0.1` instead of `localhost` to force TCP connection. MySQL client defaults to Unix socket for `localhost`.

**Common Commands:**
```
SHOW DATABASES;
USE mydb;
SHOW TABLES;
DESCRIBE users;
EXIT;
```

---

## GUI Tools

### MySQL Workbench

Official MySQL administration tool.

1. Download from [mysql.com](https://www.mysql.com/products/workbench/)
2. New connection:
   - Connection Method: Standard TCP/IP
   - Hostname: 127.0.0.1
   - Port: 3306
   - Username: admin
3. Test and connect

### DBeaver

Universal database tool.

1. New connection → MySQL
2. Host: 127.0.0.1, Port: 3306
3. Database: mydb, User: admin
4. Test connection

### HeidiSQL (Windows)

Lightweight MySQL client.

1. New session → MySQL
2. Host: 127.0.0.1, Port: 3306
3. Enter credentials
4. Connect

---

## Programming Language Drivers

### Python - mysql-connector-python

Official MySQL driver.

**Install:**
```bash
pip install mysql-connector-python
```

**Usage:**
```python
import mysql.connector

conn = mysql.connector.connect(
    host="127.0.0.1",
    port=3306,
    database="mydb",
    user="admin",
    password="secret"
)

cursor = conn.cursor()
cursor.execute("SELECT * FROM users WHERE id = %s", (1,))
row = cursor.fetchone()

cursor.close()
conn.close()
```

### Python - PyMySQL

Pure Python MySQL driver.

**Install:**
```bash
pip install pymysql
```

**Usage:**
```python
import pymysql

conn = pymysql.connect(
    host='127.0.0.1',
    port=3306,
    user='admin',
    password='secret',
    database='mydb'
)

with conn.cursor() as cursor:
    cursor.execute("SELECT * FROM users WHERE id = %s", (1,))
    row = cursor.fetchone()

conn.close()
```

### Python - SQLAlchemy (MySQL)

```python
from sqlalchemy import create_engine

engine = create_engine(
    "mysql+pymysql://admin:secret@127.0.0.1:3306/mydb"
)

with engine.connect() as conn:
    result = conn.execute("SELECT * FROM users")
```

### Node.js - mysql2

**Install:**
```bash
npm install mysql2
```

**Usage:**
```javascript
const mysql = require('mysql2/promise');

async function main() {
    const conn = await mysql.createConnection({
        host: '127.0.0.1',
        port: 3306,
        user: 'admin',
        password: 'secret',
        database: 'mydb'
    });

    const [rows] = await conn.execute('SELECT * FROM users WHERE id = ?', [1]);
    console.log(rows);

    await conn.end();
}

main();
```

### Go - go-sql-driver/mysql

**Install:**
```bash
go get github.com/go-sql-driver/mysql
```

**Usage:**
```go
package main

import (
    "database/sql"
    _ "github.com/go-sql-driver/mysql"
)

func main() {
    db, err := sql.Open("mysql", "admin:secret@tcp(127.0.0.1:3306)/mydb")
    if err != nil {
        panic(err)
    }
    defer db.Close()

    var name string
    err = db.QueryRow("SELECT name FROM users WHERE id = ?", 1).Scan(&name)
}
```

### Ruby - mysql2

**Install:**
```bash
gem install mysql2
```

**Usage:**
```ruby
require 'mysql2'

client = Mysql2::Client.new(
    host: '127.0.0.1',
    port: 3306,
    username: 'admin',
    password: 'secret',
    database: 'mydb'
)

results = client.query("SELECT * FROM users WHERE id = 1")
results.each { |row| puts row }

client.close
```

### PHP - PDO MySQL

```php
<?php
$dsn = "mysql:host=127.0.0.1;port=3306;dbname=mydb";
$pdo = new PDO($dsn, 'admin', 'secret');

$stmt = $pdo->prepare("SELECT * FROM users WHERE id = ?");
$stmt->execute([1]);
$row = $stmt->fetch();
```

### PHP - mysqli

```php
<?php
$conn = new mysqli('127.0.0.1', 'admin', 'secret', 'mydb', 3306);

$result = $conn->query("SELECT * FROM users");
while ($row = $result->fetch_assoc()) {
    echo $row['name'];
}

$conn->close();
```

### Java - JDBC

```java
String url = "jdbc:mysql://127.0.0.1:3306/mydb";
Connection conn = DriverManager.getConnection(url, "admin", "secret");

PreparedStatement stmt = conn.prepareStatement("SELECT * FROM users WHERE id = ?");
stmt.setInt(1, 1);
ResultSet rs = stmt.executeQuery();

while (rs.next()) {
    System.out.println(rs.getString("name"));
}

conn.close();
```

---

## Connection String Format

### Standard Format

```
mysql://user:password@host:port/database
mysql://admin:secret@127.0.0.1:3306/mydb
```

### JDBC Format

```
jdbc:mysql://127.0.0.1:3306/mydb?useSSL=true
```

### DSN Format (PHP)

```
mysql:host=127.0.0.1;port=3306;dbname=mydb
```

---

## SSL Connections

```python
# Python with SSL
conn = mysql.connector.connect(
    host="127.0.0.1",
    port=3306,
    database="mydb",
    user="admin",
    password="secret",
    ssl_ca="/path/to/ca.crt"
)
```

```javascript
// Node.js with SSL
const conn = await mysql.createConnection({
    host: '127.0.0.1',
    ssl: {
        ca: fs.readFileSync('/path/to/ca.crt')
    }
});
```

---

## MySQL-Specific Syntax

ScratchBird supports MySQL-specific syntax when connected via MySQL protocol:

```sql
-- Backtick identifiers
SELECT `name` FROM `users`;

-- LIMIT syntax
SELECT * FROM users LIMIT 10, 20;

-- SHOW commands
SHOW DATABASES;
SHOW TABLES;
SHOW CREATE TABLE users;
```

---

## Known Differences

| Feature | ScratchBird | MySQL |
|---------|-------------|-------|
| Storage engines | Single engine | Multiple |
| AUTO_INCREMENT | Supported | Supported |
| BOOLEAN | Native | TINYINT(1) |
| UUID | Native type | CHAR(36) |
| JSON | Native type | JSON type |

---

## Troubleshooting

### "Can't connect to MySQL server"

```bash
# Use IP address, not localhost
mysql -h 127.0.0.1 -P 3306 -u admin -p
```

### "Access denied"

Check:
1. Correct username/password
2. User exists in database
3. hba.conf allows MySQL connections

### Wrong Protocol

If seeing PostgreSQL errors, verify you're connecting to the correct port:
- MySQL: 3306
- PostgreSQL: 5432

---

## See Also

- [First Connection](../getting-started/first-connection.md)
- [SSL Setup](../configuration/ssl-setup.md)
- [PostgreSQL Clients](postgresql-clients.md)

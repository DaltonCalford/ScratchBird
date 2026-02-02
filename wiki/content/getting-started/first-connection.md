# First Connection

**Last Updated:** 2026-01-30

---

## Overview

This guide walks you through connecting to ScratchBird for the first time. ScratchBird supports multiple connection protocols, allowing you to use your preferred tools and drivers.

---

## Connection Protocols

ScratchBird listens on multiple ports, each supporting a different wire protocol:

| Protocol | Default Port | Use Case |
|----------|--------------|----------|
| Native ScratchBird | 3092 | Full feature access, best performance |
| PostgreSQL | 5432 | Use psql, JDBC, psycopg2, etc. |
| MySQL | 3306 | Use mysql CLI, MySQL Connector, etc. |
| Firebird | 3050 | Migration from Firebird |

You can connect to the **same database** using any protocol. The underlying data is identical.

---

## Prerequisites

Before connecting, ensure:

1. ScratchBird server is running
2. You know the server hostname/IP
3. You have valid credentials (default: `admin`)

### Verify Server is Running

```bash
# Check if server process is running
pgrep sb_server

# Or check systemd service status
sudo systemctl status scratchbird

# Or check Docker container
docker ps | grep scratchbird
```

### Check Listening Ports

```bash
# Linux/macOS
ss -tlnp | grep -E '3092|5432|3306|3050'

# Or using netstat
netstat -tlnp | grep -E '3092|5432|3306|3050'

# Windows PowerShell
netstat -an | findstr "3092 5432 3306 3050"
```

---

## Method 1: Native Client (sb_isql)

The native ScratchBird client provides full access to all features.

### Basic Connection

```bash
# Connect to localhost with default port
sb_isql -H localhost -p 3092 -U admin -d scratchbird

# Short form
sb_isql -H localhost -U admin -d scratchbird
```

### Connection with Password Prompt

```bash
# Prompt for password
sb_isql -H localhost -p 3092 -U admin -d scratchbird -W
```

### Connection String Format

```bash
# Using connection string
sb_isql -c "host=localhost port=3092 dbname=scratchbird user=admin password=secret"
```

### Example Session

```
$ sb_isql -H localhost -p 3092 -U admin -d scratchbird
Password: ********
Connected to ScratchBird 1.0.0
Type "help" for help.

scratchbird=> SELECT version();
                     version
------------------------------------------------
 ScratchBird 1.0.0 on Linux x86_64
(1 row)

scratchbird=> \q
```

### Useful sb_isql Commands

| Command | Description |
|---------|-------------|
| `\q` | Quit |
| `\h` | Help on SQL commands |
| `\?` | Help on sb_isql commands |
| `\dt` | List tables |
| `\d tablename` | Describe table |
| `\l` | List databases |
| `\c dbname` | Connect to database |
| `\timing` | Toggle query timing |

---

## Method 2: PostgreSQL Client (psql)

Connect using the standard PostgreSQL command-line client.

### Install psql

**Linux (Debian/Ubuntu):**
```bash
sudo apt install postgresql-client
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install postgresql
```

**macOS:**
```bash
brew install libpq
echo 'export PATH="/opt/homebrew/opt/libpq/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

**Windows:**
Download from [postgresql.org](https://www.postgresql.org/download/windows/) or use `psql` from a PostgreSQL installation.

### Connect

```bash
# Basic connection
psql -h localhost -p 5432 -U admin -d scratchbird

# With password in environment (not recommended for production)
PGPASSWORD=secret psql -h localhost -p 5432 -U admin -d scratchbird

# Using connection URI
psql "postgresql://admin:secret@localhost:5432/scratchbird"
```

### Example Session

```
$ psql -h localhost -p 5432 -U admin -d scratchbird
Password for user admin: ********
psql (16.1, server 15.0)
Type "help" for help.

scratchbird=# SELECT current_database();
 current_database
------------------
 scratchbird
(1 row)

scratchbird=# \dt
         List of relations
 Schema | Name  | Type  | Owner
--------+-------+-------+-------
 public | users | table | admin
(1 row)

scratchbird=# \q
```

---

## Method 3: MySQL Client

Connect using the MySQL command-line client.

### Install mysql Client

**Linux (Debian/Ubuntu):**
```bash
sudo apt install mysql-client
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install mysql
```

**macOS:**
```bash
brew install mysql-client
```

### Connect

```bash
# Basic connection
mysql -h localhost -P 3306 -u admin -p scratchbird

# With password (not recommended)
mysql -h localhost -P 3306 -u admin -psecret scratchbird
```

### Example Session

```
$ mysql -h localhost -P 3306 -u admin -p scratchbird
Enter password: ********
Welcome to the MySQL monitor.  Commands end with ; or \g.

mysql> SELECT DATABASE();
+-------------+
| DATABASE()  |
+-------------+
| scratchbird |
+-------------+
1 row in set (0.01 sec)

mysql> SHOW TABLES;
+------------------------+
| Tables_in_scratchbird  |
+------------------------+
| users                  |
+------------------------+
1 row in set (0.00 sec)

mysql> exit
Bye
```

---

## Method 4: GUI Database Tools

### DBeaver (Free, Cross-Platform)

1. Download from [dbeaver.io](https://dbeaver.io/download/)
2. Create new connection
3. Select **PostgreSQL** driver
4. Configure:
   - Host: `localhost`
   - Port: `5432`
   - Database: `scratchbird`
   - Username: `admin`
   - Password: your password
5. Click "Test Connection" then "Finish"

### TablePlus (macOS/Windows/Linux)

1. Download from [tableplus.com](https://tableplus.com/)
2. Click "Create a new connection"
3. Select **PostgreSQL**
4. Enter connection details
5. Click "Connect"

### DataGrip (JetBrains, Commercial)

1. File → New → Data Source → PostgreSQL
2. Enter connection details
3. Download driver if prompted
4. Test Connection → OK

### HeidiSQL (Windows, Free)

1. Download from [heidisql.com](https://www.heidisql.com/)
2. Create new session
3. Network type: **MySQL (TCP/IP)**
4. Hostname: `localhost`
5. Port: `3306`
6. User/Password: your credentials

---

## Method 5: Programming Languages

### Python (psycopg2)

```python
import psycopg2

conn = psycopg2.connect(
    host="localhost",
    port=5432,
    database="scratchbird",
    user="admin",
    password="secret"
)

cur = conn.cursor()
cur.execute("SELECT version()")
print(cur.fetchone()[0])

cur.close()
conn.close()
```

### Node.js (pg)

```javascript
const { Client } = require('pg');

const client = new Client({
    host: 'localhost',
    port: 5432,
    database: 'scratchbird',
    user: 'admin',
    password: 'secret'
});

async function main() {
    await client.connect();
    const res = await client.query('SELECT version()');
    console.log(res.rows[0].version);
    await client.end();
}

main();
```

### Java (JDBC)

```java
import java.sql.*;

public class ScratchBirdExample {
    public static void main(String[] args) throws SQLException {
        String url = "jdbc:postgresql://localhost:5432/scratchbird";
        String user = "admin";
        String password = "secret";

        try (Connection conn = DriverManager.getConnection(url, user, password);
             Statement stmt = conn.createStatement();
             ResultSet rs = stmt.executeQuery("SELECT version()")) {

            while (rs.next()) {
                System.out.println(rs.getString(1));
            }
        }
    }
}
```

### Go (pgx)

```go
package main

import (
    "context"
    "fmt"
    "github.com/jackc/pgx/v5"
)

func main() {
    conn, err := pgx.Connect(context.Background(),
        "postgres://admin:secret@localhost:5432/scratchbird")
    if err != nil {
        panic(err)
    }
    defer conn.Close(context.Background())

    var version string
    err = conn.QueryRow(context.Background(), "SELECT version()").Scan(&version)
    if err != nil {
        panic(err)
    }
    fmt.Println(version)
}
```

---

## Connection Troubleshooting

### "Connection refused"

**Cause:** Server not running or wrong port.

```bash
# Check server status
sudo systemctl status scratchbird

# Check if port is listening
ss -tlnp | grep 3092

# Start server if needed
sudo systemctl start scratchbird
```

### "Authentication failed"

**Cause:** Wrong username or password.

```bash
# Check user exists (native client)
sb_isql -H localhost -U admin -c "SELECT * FROM sb_catalog.users"

# Reset password (if you have access)
sb_security -c "ALTER USER admin WITH PASSWORD 'newpassword'"
```

### "Database does not exist"

**Cause:** Connecting to non-existent database.

```bash
# List databases
sb_isql -H localhost -U admin -c "SELECT * FROM sb_catalog.databases"

# Create database
sb_isql -H localhost -U admin -c "CREATE DATABASE mydb"
```

### "Network unreachable" or "Host not found"

**Cause:** Firewall blocking connection or wrong hostname.

```bash
# Test connectivity
ping <hostname>
telnet <hostname> 5432

# Check firewall (Linux)
sudo ufw status
sudo iptables -L -n | grep 5432
```

### "SSL connection required"

**Cause:** Server requires SSL but client isn't using it.

```bash
# Connect with SSL
psql "postgresql://admin@localhost:5432/scratchbird?sslmode=require"

# Or disable SSL requirement in server config (not recommended for production)
```

---

## Connection Security Best Practices

### Use Environment Variables for Passwords

```bash
# Set password in environment
export PGPASSWORD="secret"
psql -h localhost -U admin -d scratchbird

# Or use .pgpass file
echo "localhost:5432:scratchbird:admin:secret" >> ~/.pgpass
chmod 600 ~/.pgpass
```

### Enable SSL/TLS

For production, always use encrypted connections:

```bash
psql "postgresql://admin@localhost:5432/scratchbird?sslmode=verify-full&sslrootcert=/path/to/ca.crt"
```

### Use Connection Pooling

For applications, use a connection pooler to manage connections efficiently:

```python
# Python with connection pool
from psycopg2 import pool

connection_pool = pool.SimpleConnectionPool(
    minconn=1,
    maxconn=10,
    host="localhost",
    port=5432,
    database="scratchbird",
    user="admin",
    password="secret"
)

conn = connection_pool.getconn()
# ... use connection ...
connection_pool.putconn(conn)
```

---

## Verify Connection Success

Once connected, run these queries to verify everything is working:

```sql
-- Check database version
SELECT version();

-- Check current database
SELECT current_database();

-- Check current user
SELECT current_user;

-- List tables
SELECT * FROM sb_catalog.tables;

-- Simple query
SELECT 1 + 1 AS result;
```

---

## Next Steps

- [Basic SQL](basic-sql.md) - Learn CRUD operations
- [Language Guides](../language-guides/README.md) - SQL dialect documentation
- [CLI Tools](../cli-tools/README.md) - Command-line tools reference


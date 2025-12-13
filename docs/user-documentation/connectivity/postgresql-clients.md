# PostgreSQL Client Compatibility

Connect to ScratchBird using PostgreSQL clients and drivers.

[Back to Connectivity Index](index.md) | [Back to Documentation Index](../index.md)

---

## Overview

ScratchBird implements the PostgreSQL wire protocol (v3), providing compatibility with PostgreSQL clients, drivers, and tools.

**Default Port:** 5432

---

## Command-Line Tools

### psql

The standard PostgreSQL client works with ScratchBird.

**Install:**
```bash
# Debian/Ubuntu
sudo apt install postgresql-client

# RHEL/Fedora
sudo dnf install postgresql

# macOS
brew install libpq
```

**Connect:**
```bash
psql -h localhost -p 5432 -U admin -d mydb
```

**Common Commands:**
```
\l          List databases
\c dbname   Connect to database
\dt         List tables
\d table    Describe table
\q          Quit
```

### pg_dump / pg_restore

Backup and restore tools work for migration:

```bash
# Export
pg_dump -h localhost -U admin mydb > backup.sql

# Import
psql -h localhost -U admin -d newdb < backup.sql
```

---

## GUI Tools

### pgAdmin

Full-featured PostgreSQL administration tool.

1. Download from [pgadmin.org](https://www.pgadmin.org/)
2. Add new server:
   - Name: ScratchBird
   - Host: localhost
   - Port: 5432
   - Username: admin
3. Connect

### DBeaver

Universal database tool with excellent PostgreSQL support.

1. Download from [dbeaver.io](https://dbeaver.io/)
2. New connection → PostgreSQL
3. Enter connection details
4. Test and connect

### DataGrip

JetBrains IDE for databases.

1. New data source → PostgreSQL
2. Host: localhost, Port: 5432
3. User: admin, Database: mydb
4. Test connection

---

## Programming Language Drivers

### Python - psycopg2

Most popular PostgreSQL driver for Python.

**Install:**
```bash
pip install psycopg2-binary
```

**Usage:**
```python
import psycopg2

# Connect
conn = psycopg2.connect(
    host="localhost",
    port=5432,
    database="mydb",
    user="admin",
    password="secret"
)

# Query
cur = conn.cursor()
cur.execute("SELECT * FROM users WHERE id = %s", (1,))
row = cur.fetchone()

# Close
cur.close()
conn.close()
```

### Python - asyncpg

Async PostgreSQL driver.

**Install:**
```bash
pip install asyncpg
```

**Usage:**
```python
import asyncio
import asyncpg

async def main():
    conn = await asyncpg.connect(
        host='localhost',
        port=5432,
        database='mydb',
        user='admin',
        password='secret'
    )

    row = await conn.fetchrow('SELECT * FROM users WHERE id = $1', 1)
    await conn.close()

asyncio.run(main())
```

### Python - SQLAlchemy

ORM with PostgreSQL backend.

```python
from sqlalchemy import create_engine

engine = create_engine(
    "postgresql://admin:secret@localhost:5432/mydb",
    pool_size=10
)

with engine.connect() as conn:
    result = conn.execute("SELECT * FROM users")
    for row in result:
        print(row)
```

### Node.js - pg

**Install:**
```bash
npm install pg
```

**Usage:**
```javascript
const { Pool } = require('pg');

const pool = new Pool({
    host: 'localhost',
    port: 5432,
    database: 'mydb',
    user: 'admin',
    password: 'secret'
});

// Query
const result = await pool.query('SELECT * FROM users WHERE id = $1', [1]);
console.log(result.rows);

// Close
await pool.end();
```

### Go - lib/pq

**Install:**
```bash
go get github.com/lib/pq
```

**Usage:**
```go
package main

import (
    "database/sql"
    _ "github.com/lib/pq"
)

func main() {
    connStr := "host=localhost port=5432 user=admin password=secret dbname=mydb"
    db, err := sql.Open("postgres", connStr)
    if err != nil {
        panic(err)
    }
    defer db.Close()

    var name string
    err = db.QueryRow("SELECT name FROM users WHERE id = $1", 1).Scan(&name)
}
```

### Go - pgx

Higher-performance PostgreSQL driver.

```go
import "github.com/jackc/pgx/v5"

conn, err := pgx.Connect(context.Background(),
    "postgres://admin:secret@localhost:5432/mydb")
```

### Ruby - pg gem

**Install:**
```bash
gem install pg
```

**Usage:**
```ruby
require 'pg'

conn = PG.connect(
    host: 'localhost',
    port: 5432,
    dbname: 'mydb',
    user: 'admin',
    password: 'secret'
)

result = conn.exec_params('SELECT * FROM users WHERE id = $1', [1])
result.each { |row| puts row }

conn.close
```

### PHP - PDO

```php
<?php
$dsn = "pgsql:host=localhost;port=5432;dbname=mydb";
$pdo = new PDO($dsn, 'admin', 'secret');

$stmt = $pdo->prepare("SELECT * FROM users WHERE id = ?");
$stmt->execute([1]);
$row = $stmt->fetch();
```

### Rust - tokio-postgres

```rust
use tokio_postgres::{NoTls, Error};

#[tokio::main]
async fn main() -> Result<(), Error> {
    let (client, connection) = tokio_postgres::connect(
        "host=localhost port=5432 user=admin password=secret dbname=mydb",
        NoTls,
    ).await?;

    let row = client.query_one("SELECT name FROM users WHERE id = $1", &[&1i32]).await?;
    let name: &str = row.get(0);

    Ok(())
}
```

---

## Connection String Formats

### URI Format

```
postgresql://user:password@host:port/database?options
postgresql://admin:secret@localhost:5432/mydb?sslmode=require
```

### Key-Value Format

```
host=localhost port=5432 dbname=mydb user=admin password=secret sslmode=require
```

---

## SSL Connections

```python
# Python with SSL
conn = psycopg2.connect(
    host="localhost",
    port=5432,
    database="mydb",
    user="admin",
    password="secret",
    sslmode="require"
)
```

```javascript
// Node.js with SSL
const pool = new Pool({
    ssl: {
        rejectUnauthorized: false // or provide ca cert
    }
});
```

---

## Connection Pooling

### Built-in Application Pooling

Most drivers support pooling:

```python
# SQLAlchemy
engine = create_engine(url, pool_size=10, max_overflow=20)
```

```javascript
// Node.js pg
const pool = new Pool({ max: 20 });
```

### PgBouncer

ScratchBird works with PgBouncer for external pooling:

```ini
; pgbouncer.ini
[databases]
mydb = host=localhost port=5432

[pgbouncer]
pool_mode = transaction
max_client_conn = 1000
default_pool_size = 20
```

---

## Known Limitations

ScratchBird supports most PostgreSQL features, with some differences:

| Feature | Status |
|---------|--------|
| Basic SQL | Full support |
| Transactions | Full support |
| Prepared statements | Full support |
| COPY | Full support |
| LISTEN/NOTIFY | Partial support |
| Extensions | Not supported |

---

## Troubleshooting

### "SSL required"

```python
# Add sslmode
conn = psycopg2.connect(..., sslmode="require")
```

### "Authentication failed"

Check:
1. Username/password correct
2. User exists in database
3. hba.conf allows connection

### "Connection refused"

```bash
# Verify server listening
ss -tlnp | grep 5432
```

---

## See Also

- [First Connection](../getting-started/first-connection.md)
- [SSL Setup](../configuration/ssl-setup.md)
- [Authentication](../configuration/hba.conf.md)

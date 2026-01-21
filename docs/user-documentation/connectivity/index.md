# Connectivity Guide

Connect to ScratchBird from various clients and programming languages.

[Back to Documentation Index](../index.md)

---

## Protocol Support

ScratchBird supports multiple wire protocols simultaneously:

| Protocol | Port | Compatible Clients |
|----------|------|-------------------|
| PostgreSQL | 5432 | psql, pgAdmin, JDBC, psycopg2, node-pg |
| MySQL | 3306 | mysql, MySQL Workbench, JDBC, MySQLdb |
| Firebird | 3050 | FlameRobin, IBExpert, Jaybird |
| Native | 3092 | sb_isql, libscratchbird |

---

## Client Guides

| Guide | Description |
|-------|-------------|
| [PostgreSQL Clients](postgresql-clients.md) | psql, pgAdmin, drivers |
| [MySQL Clients](mysql-clients.md) | mysql CLI, MySQL Workbench |
| [Firebird Clients](firebird-clients.md) | FlameRobin, IBExpert |
| [ODBC](odbc.md) | ODBC driver setup |
| [JDBC](jdbc.md) | Java connectivity |
| [Native Client](native-client.md) | ScratchBird native library |

---

## Quick Connection Examples

### Python (psycopg2)

```python
import psycopg2

conn = psycopg2.connect(
    host="localhost",
    port=5432,
    database="mydb",
    user="admin",
    password="secret"
)
```

### Node.js (pg)

```javascript
const { Pool } = require('pg');

const pool = new Pool({
    host: 'localhost',
    port: 5432,
    database: 'mydb',
    user: 'admin',
    password: 'secret'
});
```

### Java (JDBC)

```java
String url = "jdbc:scratchbird://localhost:3092/mydb";
Connection conn = DriverManager.getConnection(url, "admin", "secret");
```

### Go (lib/pq)

```go
import "database/sql"
import _ "github.com/lib/pq"

db, err := sql.Open("postgres",
    "host=localhost port=5432 user=admin password=secret dbname=mydb")
```

---

## Connection Strings

### PostgreSQL Format

```
postgresql://user:password@host:port/database
postgresql://admin:secret@localhost:5432/mydb?sslmode=require
```

### Key-Value Format

```
host=localhost port=5432 dbname=mydb user=admin password=secret
```

### Native Format

```
sb://user:password@host:port/database
sb://admin:secret@localhost:3092/mydb
```

---

## SSL/TLS Connections

### Require SSL

```bash
psql "host=server port=5432 sslmode=require dbname=mydb"
```

### Verify Certificate

```bash
psql "host=server port=5432 sslmode=verify-full sslrootcert=ca.crt dbname=mydb"
```

### Client Certificate

```bash
psql "host=server port=5432 sslmode=verify-full \
      sslcert=client.crt sslkey=client.key sslrootcert=ca.crt"
```

---

## Connection Pooling

### Application-Side (Recommended)

Most applications should use connection pooling:

```python
# SQLAlchemy with pooling
engine = create_engine(
    "postgresql://...",
    pool_size=10,
    max_overflow=20
)
```

### External Poolers

Compatible with:
- PgBouncer
- pgpool-II
- ProxySQL (MySQL mode)

---

## Choosing a Protocol

| Use Case | Recommended Protocol |
|----------|---------------------|
| Existing PostgreSQL app | PostgreSQL (5432) |
| Existing MySQL app | MySQL (3306) |
| Existing Firebird app | Firebird (3050) |
| New application | PostgreSQL or Native |
| Best compatibility | PostgreSQL |
| Maximum performance | Native |

---

## Troubleshooting

### Connection Refused

```bash
# Check server is running
systemctl status scratchbird

# Check listening ports
ss -tlnp | grep -E "5432|3306|3050|3092"
```

### Authentication Failed

```bash
# Verify credentials work with sb_isql
sb_isql -H localhost -U admin -P 3092 mydb
```

### SSL Required

```bash
# Server requires SSL
psql "host=server sslmode=require ..."
```

---

## See Also

- [First Connection](../getting-started/first-connection.md)
- [SSL Setup](../configuration/ssl-setup.md)
- [Authentication](../configuration/hba.conf.md)

# Connecting to ScratchBird

Learn how to connect using various clients and protocols.

[Back to Getting Started](index.md) | [Back to Documentation Index](../index.md)

---

## Connection Overview

ScratchBird accepts connections on multiple protocols simultaneously:

| Protocol | Default Port | Wire Format |
|----------|-------------|-------------|
| Native | 3092 | ScratchBird native |
| PostgreSQL | 5432 | PostgreSQL v3 |
| MySQL | 3306 | MySQL protocol |
| Firebird | 3050 | Firebird wire protocol |

All protocols connect to the same database engine - choose based on your existing tools.

---

## Using sb_isql (Native Client)

The built-in `sb_isql` is the most direct way to connect.

### Basic Connection

```bash
# Connect to local server
sb_isql -H localhost -P 3092

# With username
sb_isql -H localhost -P 3092 -U admin

# With username and database
sb_isql -H localhost -P 3092 -U admin mydb

# With password (prompted)
sb_isql -H localhost -P 3092 -U admin -P
```

### Command-Line Options

| Option | Long Form | Description |
|--------|-----------|-------------|
| `-H` | `--host` | Server hostname |
| `-p` | `--port` | Server port |
| `-U` | `--user` | Username |
| `-P` | `--password` | Password (or prompt) |
| `-c` | `--command` | Execute single command |
| `-f` | `--file` | Execute from file |
| `-q` | `--quiet` | No welcome message |
| `-t` | `--tuples-only` | Data only, no headers |

### Interactive Session

```
$ sb_isql -H localhost -P 3092 -U admin
ScratchBird Interactive SQL Shell (sb_isql) v0.9.0
Type \? for help, \q to quit.

sb_isql> SELECT version();
 version
------------------
 ScratchBird 0.9.0
(1 row)

sb_isql> \q
```

### Meta-Commands

| Command | Description |
|---------|-------------|
| `\?` | Show help |
| `\q` | Quit |
| `\l` | List databases |
| `\c dbname` | Connect to database |
| `\d` | List tables |
| `\d table` | Describe table |
| `\di` | List indexes |
| `\du` | List users |
| `\timing on` | Show query timing |
| `\x` | Toggle expanded display |

---

## Using psql (PostgreSQL Client)

Connect using the standard PostgreSQL client.

### Install psql

```bash
# Debian/Ubuntu
sudo apt install postgresql-client

# RHEL/Fedora
sudo dnf install postgresql

# macOS
brew install libpq

# Windows
# Download from postgresql.org
```

### Connect

```bash
# Basic connection
psql -h localhost -p 5432 -U admin

# With database
psql -h localhost -p 5432 -U admin -d mydb

# Connection string
psql "host=localhost port=5432 user=admin dbname=mydb"

# URI format
psql postgresql://admin:password@localhost:5432/mydb
```

### Example Session

```
$ psql -h localhost -p 5432 -U admin
Password for user admin:
psql (15.0)
Type "help" for help.

admin=# \l
                              List of databases
   Name    |  Owner  | Encoding |   Collation   |    Ctype
-----------+---------+----------+---------------+---------------
 mydb      | admin   | UTF8     | en_US.UTF-8   | en_US.UTF-8
(1 row)

admin=# \c mydb
You are now connected to database "mydb" as user "admin".

mydb=# SELECT * FROM users;
 id | name  | email
----+-------+-------------------
  1 | Alice | alice@example.com
(1 row)

mydb=# \q
```

---

## Using mysql Client

Connect using the MySQL command-line client.

### Install mysql Client

```bash
# Debian/Ubuntu
sudo apt install mysql-client

# RHEL/Fedora
sudo dnf install mysql

# macOS
brew install mysql-client

# Windows
# Download from mysql.com
```

### Connect

```bash
# Basic connection (use 127.0.0.1, not localhost for TCP)
mysql -h 127.0.0.1 -P 3306 -u admin -p

# With database
mysql -h 127.0.0.1 -P 3306 -u admin -p mydb
```

**Note:** Use `127.0.0.1` instead of `localhost` to force TCP connection.

### Example Session

```
$ mysql -h 127.0.0.1 -P 3306 -u admin -p
Enter password:
Welcome to the MySQL monitor.

mysql> SHOW DATABASES;
+--------------------+
| Database           |
+--------------------+
| mydb               |
+--------------------+
1 row in set (0.00 sec)

mysql> USE mydb;
Database changed

mysql> SELECT * FROM users;
+----+-------+-------------------+
| id | name  | email             |
+----+-------+-------------------+
|  1 | Alice | alice@example.com |
+----+-------+-------------------+
1 row in set (0.00 sec)

mysql> quit
```

---

## Using GUI Clients

### DBeaver (Universal)

1. Download from [dbeaver.io](https://dbeaver.io/)
2. Create new connection
3. Choose "PostgreSQL" driver
4. Enter connection details:
   - Host: `localhost`
   - Port: `5432`
   - Database: `mydb`
   - Username: `admin`
5. Test connection
6. Connect

### pgAdmin (PostgreSQL)

1. Download from [pgadmin.org](https://www.pgadmin.org/)
2. Add new server
3. General tab: Name your connection
4. Connection tab:
   - Host: `localhost`
   - Port: `5432`
   - Username: `admin`
5. Save and connect

### MySQL Workbench

1. Download from [mysql.com](https://www.mysql.com/products/workbench/)
2. Create new connection
3. Connection Method: Standard TCP/IP
4. Parameters:
   - Hostname: `127.0.0.1`
   - Port: `3306`
   - Username: `admin`
5. Test connection

### FlameRobin (Firebird)

1. Download from [flamerobin.org](http://www.flamerobin.org/)
2. Server → Register Server
3. Enter details:
   - Name: `ScratchBird`
   - Hostname: `localhost`
   - Port: `3050`
4. Register database under server

---

## Connection Strings

### Native Format

```
sb://user:password@host:port/database
```

Examples:
```
sb://admin@localhost:3092/mydb
sb://admin:secret@192.168.1.100:3092/production
```

### PostgreSQL Format

```
postgresql://user:password@host:port/database
```

Or key-value:
```
host=localhost port=5432 dbname=mydb user=admin password=secret
```

### JDBC Format

```
jdbc:postgresql://localhost:5432/mydb
jdbc:mysql://localhost:3306/mydb
```

---

## Authentication

### Password Authentication

Default method - provide username and password:

```bash
sb_isql -H localhost -U admin
Password: ********
```

### SCRAM-SHA-256

ScratchBird uses SCRAM-SHA-256 by default (most secure):

```ini
# In sb_server.conf
[authentication]
methods = scram-sha-256
```

### Trust (Development Only)

For local development, you can disable password:

```ini
# WARNING: Not for production!
[authentication]
methods = trust
```

---

## Connection Pooling

For applications, use connection pooling:

### Application-Side Pooling

Most frameworks provide pooling:

```python
# Python with SQLAlchemy
from sqlalchemy import create_engine
engine = create_engine(
    "postgresql://admin:pass@localhost:5432/mydb",
    pool_size=10,
    max_overflow=20
)
```

### Server-Side Limits

Configure in `sb_server.conf`:

```ini
[server]
max_connections = 100
max_connections_per_user = 0
idle_timeout = 3600
```

---

## Testing Connection

### Using sb_isql

```bash
sb_isql -H localhost -P 3092 -c "SELECT 1"
```

### Using psql

```bash
psql -h localhost -p 5432 -U admin -c "SELECT 1"
```

### Using nc (netcat)

Test if port is open:

```bash
nc -zv localhost 3092
nc -zv localhost 5432
```

---

## Troubleshooting

### "Connection refused"

Server not running or wrong port:

```bash
# Check server status
systemctl status scratchbird

# Check listening ports
ss -tlnp | grep -E "3092|5432|3306|3050"
```

### "Authentication failed"

Wrong username or password:

```bash
# Reset password (as admin)
sb_security password admin --new-password
```

### "Database does not exist"

```bash
# List available databases
sb_isql -H localhost -P 3092 -c "\l"
```

### "Too many connections"

```bash
# Check connection count
sb_isql -H localhost -c "SELECT count(*) FROM pg_stat_activity"

# Increase limit in sb_server.conf
# max_connections = 200
```

### Firewall blocking

```bash
# Check firewall (Linux)
sudo ufw status
sudo firewall-cmd --list-ports

# Check firewall (Windows)
netsh advfirewall firewall show rule name=all | findstr 5432
```

---

## Next Steps

Now that you can connect:

1. [Learn basic SQL](basic-sql.md)
2. [Explore connectivity options](../connectivity/index.md)
3. [Set up security](../admin/security.md)

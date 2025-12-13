# Creating Your First Database

Learn how to create and manage databases in ScratchBird.

[Back to Getting Started](index.md) | [Back to Documentation Index](../index.md)

---

## Prerequisites

- ScratchBird server running
- Access to `sb_isql` or another SQL client

---

## Understanding Database Modes

ScratchBird supports two server modes (configured in `sb_server.conf`):

| Mode | Description | Use Case |
|------|-------------|----------|
| **multi-database** | Multiple databases in data directory | Most installations |
| **single-database** | One database file | Embedded applications |

Check your mode:
```bash
grep "mode" /etc/scratchbird/sb_server.conf
```

---

## Creating a Database

### Using sb_isql

```bash
# Connect to server
sb_isql -H localhost -P 3092 -U admin

# Create database
sb_isql> CREATE DATABASE myapp;
Database created.

# List databases
sb_isql> \l
        List of databases
  Name  |  Owner  | Encoding
--------+---------+----------
 myapp  | admin   | UTF8
(1 row)

# Connect to new database
sb_isql> \c myapp
Connected to database: myapp
```

### Using psql

```bash
psql -h localhost -p 5432 -U admin

postgres=# CREATE DATABASE myapp;
CREATE DATABASE

postgres=# \c myapp
You are now connected to database "myapp" as user "admin".
```

### Using mysql Client

```bash
mysql -h 127.0.0.1 -P 3306 -u admin -p

mysql> CREATE DATABASE myapp;
Query OK, 1 row affected

mysql> USE myapp;
Database changed
```

---

## CREATE DATABASE Syntax

Full syntax:

```sql
CREATE DATABASE database_name
    [OWNER = username]
    [ENCODING = 'encoding']
    [COLLATION = 'collation']
    [PAGE_SIZE = size]
    [DEFAULT CHARACTER SET charset];
```

### Examples

```sql
-- Simple database
CREATE DATABASE simple_db;

-- With owner
CREATE DATABASE team_db OWNER = team_lead;

-- With specific encoding
CREATE DATABASE utf8_db ENCODING = 'UTF8';

-- With page size (Firebird compatibility)
CREATE DATABASE large_db PAGE_SIZE = 16384;
```

### Database Options

| Option | Description | Default |
|--------|-------------|---------|
| `OWNER` | Database owner | Current user |
| `ENCODING` | Character encoding | UTF8 |
| `COLLATION` | Default collation | Based on encoding |
| `PAGE_SIZE` | Storage page size | 8192 |

---

## Database Files

In multi-database mode, each database creates a file:

```
/var/lib/scratchbird/
├── myapp.sbdb          # Main database file
├── myapp.sbdb-lock     # Lock file (while open)
└── other_db.sbdb
```

### File Locations

| Installation Type | Default Location |
|-------------------|------------------|
| DEB/RPM | `/var/lib/scratchbird/` |
| Tarball | Configurable |
| Docker | `/var/lib/scratchbird/` (in container) |
| Windows | `C:\ProgramData\ScratchBird\data\` |

---

## Connecting to a Database

### In sb_isql

```sql
-- Connect using \c
\c myapp

-- Connect with different user
\c myapp different_user

-- Connect to different server
\c myapp admin localhost 3092
```

### Connection String

```
sb://admin:password@localhost:3092/myapp
```

Components:
- Protocol: `sb://` (native), `postgresql://`, `mysql://`
- User: `admin`
- Password: `password`
- Host: `localhost`
- Port: `3092`
- Database: `myapp`

---

## Database Information

### Show Database Details

```sql
-- In sb_isql
\l+

-- Show current database
SELECT current_database();

-- Database size (approximate)
SELECT pg_database_size('myapp');
```

### System Tables

```sql
-- List all tables
SELECT * FROM information_schema.tables;

-- List all columns
SELECT * FROM information_schema.columns
WHERE table_name = 'my_table';
```

---

## Managing Databases

### Rename Database

```sql
ALTER DATABASE old_name RENAME TO new_name;
```

### Change Owner

```sql
ALTER DATABASE myapp OWNER TO new_owner;
```

### Drop Database

```sql
-- Cannot drop current database, disconnect first
\c other_db
DROP DATABASE myapp;
```

**Warning:** DROP DATABASE permanently deletes all data!

---

## Backup and Restore

### Quick Backup

```bash
sb_backup backup myapp.sbdb myapp_backup.sbbak
```

### Restore

```bash
sb_backup restore myapp_backup.sbbak myapp_restored.sbdb
```

See [Backup and Restore](../admin/backup-restore.md) for full details.

---

## Best Practices

### Naming Conventions

- Use lowercase: `myapp`, not `MyApp`
- Use underscores: `user_data`, not `user-data`
- Be descriptive: `inventory_2024`, not `db1`

### One Database Per Application

Each application should have its own database:
```sql
CREATE DATABASE webapp_production;
CREATE DATABASE webapp_staging;
CREATE DATABASE webapp_test;
```

### Plan for Growth

Consider initial settings for large databases:
```sql
CREATE DATABASE big_data PAGE_SIZE = 16384;
```

---

## Troubleshooting

### "Permission denied"

```
ERROR: permission denied to create database
```

**Solution:** Connect as a user with CREATEDB privilege:
```sql
-- As superuser
GRANT CREATEDB TO myuser;
```

### "Database already exists"

```
ERROR: database "myapp" already exists
```

**Solution:** Choose a different name or drop existing:
```sql
-- Check if exists first
SELECT datname FROM pg_database WHERE datname = 'myapp';
```

### "Cannot create database file"

Check disk space and permissions:
```bash
df -h /var/lib/scratchbird
ls -la /var/lib/scratchbird
```

---

## Next Steps

Now that you have a database:

1. [Connect with a client](first-connection.md)
2. [Run basic SQL](basic-sql.md)
3. [Create users](../admin/user-management.md)

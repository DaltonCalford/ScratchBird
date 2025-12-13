# CREATE DATABASE

Create a new database.

[Back to DDL Index](index.md) | [Back to Language Guide](../index.md)

---

## Syntax

```sql
CREATE DATABASE database_name
    [ OWNER owner_name ]
    [ ENCODING encoding ]
    [ COLLATION collation ]
    [ TEMPLATE template ]
    [ PAGE_SIZE page_size ];
```

---

## Parameters

| Parameter | Description |
|-----------|-------------|
| `database_name` | Name of the new database |
| `OWNER` | User who owns the database |
| `ENCODING` | Character encoding (default: UTF8) |
| `COLLATION` | Sort order rules |
| `TEMPLATE` | Template database to copy |
| `PAGE_SIZE` | Page size in bytes (4096, 8192, 16384) |

---

## Examples

### Basic Database

```sql
CREATE DATABASE myapp;
```

### With Owner

```sql
CREATE DATABASE myapp OWNER appuser;
```

### With Encoding

```sql
CREATE DATABASE myapp
    ENCODING 'UTF8'
    COLLATION 'en_US.UTF-8';
```

### With Page Size

```sql
CREATE DATABASE analytics PAGE_SIZE 16384;
```

---

## Database Files

In multi-database mode, each database is stored as a `.sbdb` file in the data directory:

```
/var/lib/scratchbird/
├── myapp.sbdb
├── analytics.sbdb
└── test.sbdb
```

---

## Connecting

After creation, connect with:

```bash
# sb_isql
sb_isql -H localhost -d myapp

# psql
psql -h localhost -d myapp
```

Or within a session:

```sql
\c myapp
```

---

## Listing Databases

```sql
-- sb_isql / psql
\l

-- SQL
SELECT datname FROM pg_database;
```

---

## ALTER DATABASE

```sql
-- Change owner
ALTER DATABASE myapp OWNER TO newowner;

-- Rename
ALTER DATABASE myapp RENAME TO newname;

-- Set configuration
ALTER DATABASE myapp SET work_mem = '64MB';
```

---

## DROP DATABASE

```sql
-- Drop database
DROP DATABASE myapp;

-- Drop if exists
DROP DATABASE IF EXISTS myapp;
```

**Warning:** DROP DATABASE permanently deletes all data.

---

## Permissions

Creating databases requires:
- Superuser status, or
- CREATEDB privilege

```sql
-- Grant CREATEDB
ALTER USER developer WITH CREATEDB;
```

---

## Notes

- Database names are case-insensitive unless quoted
- Cannot create database inside a transaction
- Cannot drop a database with active connections

---

## See Also

- [First Database Tutorial](../../getting-started/first-database.md)
- [User Management](../../admin/user-management.md)

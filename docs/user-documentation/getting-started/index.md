# Getting Started with ScratchBird

Welcome to ScratchBird! This guide will help you get up and running quickly.

[Back to Documentation Index](../index.md)

---

## Prerequisites

Before starting, ensure you have:
- ScratchBird installed ([Installation Guide](../installation/index.md))
- The server running (`systemctl status scratchbird`)
- Network access to the server (if remote)

---

## Quick Start Path

1. [Create your first database](first-database.md)
2. [Connect with a client](first-connection.md)
3. [Run basic SQL operations](basic-sql.md)

---

## What You'll Learn

### In This Guide

| Topic | What You'll Learn |
|-------|-------------------|
| [First Database](first-database.md) | Create and configure a database |
| [First Connection](first-connection.md) | Connect using various clients |
| [Basic SQL](basic-sql.md) | Tables, queries, indexes |

### Tutorials

For specific use cases:

| Tutorial | Description |
|----------|-------------|
| [Web App Backend](tutorials/web-app-backend.md) | Build a database for web applications |
| [Data Warehouse](tutorials/data-warehouse.md) | Set up for analytics workloads |
| [Migration from PostgreSQL](tutorials/migration-from-postgres.md) | Move existing data to ScratchBird |

---

## Connection Options

ScratchBird supports multiple connection protocols:

| Protocol | Port | Best For | Client Examples |
|----------|------|----------|-----------------|
| Native | 3092 | Direct ScratchBird apps | libscratchbird_client |
| PostgreSQL | 5432 | PostgreSQL ecosystem | psql, pgAdmin, DBeaver |
| MySQL | 3306 | MySQL ecosystem | mysql, MySQL Workbench |
| Firebird | 3050 | Firebird ecosystem | FlameRobin, IBExpert |

Connect using whichever protocol matches your existing tools.

---

## Using sb_isql

The `sb_isql` tool is ScratchBird's built-in interactive SQL shell.

### Start sb_isql

```bash
# Connect to local server
sb_isql -H localhost -P 3092

# Connect with username
sb_isql -H localhost -P 3092 -U admin

# Connect to specific database
sb_isql -H localhost -P 3092 -U admin mydb
```

### Common Commands

| Command | Description |
|---------|-------------|
| `\q` | Quit |
| `\?` | Show help |
| `\d` | List tables |
| `\d tablename` | Describe table |
| `\l` | List databases |
| `\timing on` | Show query timing |

### Example Session

```
sb_isql> CREATE DATABASE myapp;
Database created.

sb_isql> \c myapp
Connected to database: myapp

sb_isql> CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    email VARCHAR(255) UNIQUE
);
Table created.

sb_isql> INSERT INTO users VALUES (1, 'Alice', 'alice@example.com');
1 row inserted.

sb_isql> SELECT * FROM users;
 id | name  | email
----+-------+-------------------
  1 | Alice | alice@example.com
(1 row)

sb_isql> \q
```

---

## Using External Clients

### psql (PostgreSQL Client)

```bash
# Install psql
sudo apt install postgresql-client  # Debian/Ubuntu
sudo dnf install postgresql         # RHEL/Fedora

# Connect to ScratchBird
psql -h localhost -p 5432 -U admin
```

### mysql (MySQL Client)

```bash
# Install mysql client
sudo apt install mysql-client       # Debian/Ubuntu
sudo dnf install mysql              # RHEL/Fedora

# Connect to ScratchBird
mysql -h 127.0.0.1 -P 3306 -u admin -p
```

### FlameRobin (Firebird Client)

1. Download FlameRobin from [flamerobin.org](http://www.flamerobin.org/)
2. Create new server: Host `localhost`, Port `3050`
3. Register database

---

## Database Basics

### Creating a Database

```sql
-- Create a new database
CREATE DATABASE myapp;

-- Connect to it
\c myapp
```

### Creating Tables

```sql
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    price DECIMAL(10,2),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Inserting Data

```sql
INSERT INTO products (id, name, price) VALUES
    (1, 'Widget', 9.99),
    (2, 'Gadget', 19.99),
    (3, 'Gizmo', 29.99);
```

### Querying Data

```sql
-- Select all
SELECT * FROM products;

-- With conditions
SELECT name, price FROM products WHERE price < 20;

-- With ordering
SELECT * FROM products ORDER BY price DESC;
```

---

## Next Steps

After completing the getting started guides:

1. **Configure** - [Server Configuration](../configuration/sb_server.conf.md)
2. **Secure** - [Security Best Practices](../admin/security.md)
3. **Learn** - [SQL Language Guide](../language-guide/index.md)

---

## Getting Help

- **Documentation**: Browse this user guide
- **FAQ**: [Frequently Asked Questions](../faq/index.md)
- **Issues**: [GitHub Issues](https://github.com/DaltonCalford/ScratchBird/issues)
- **Troubleshooting**: [Troubleshooting Guide](../admin/troubleshooting.md)

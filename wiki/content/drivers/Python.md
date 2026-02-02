[Back to Drivers](Driver-Comparison.md) | [Back to Home](../Home.md)

# Python Driver Guide

**Status:** Alpha documentation
**Last Updated:** 2026-01-30

---

## Overview

Python applications can connect to ScratchBird through multiple protocols:

| Protocol | Port | Library | Best For |
|----------|------|---------|----------|
| Native | 3092 | scratchbird (SBWP v1.1) | Full ScratchBird feature set |
| PostgreSQL | 5432 | psycopg2, psycopg3, asyncpg | Ecosystem compatibility |
| MySQL | 3306 | mysql-connector-python, PyMySQL | MySQL migrations |
| Firebird | 3050 | fdb, firebird-driver | Firebird migrations |

**Recommendation:** Use the **ScratchBird native driver** for full SBWP v1.1 feature coverage. Use PostgreSQL/MySQL/Firebird drivers only when you need emulation compatibility.

---

## ScratchBird Native Driver (SBWP v1.1)

### Installation

```bash
pip install scratchbird
```

### Basic Connection

```python
import scratchbird

conn = scratchbird.connect("scratchbird://user:pass@localhost:3092/mydb")
cur = conn.cursor()
cur.execute("SELECT 1")
print(cur.fetchone())
cur.close()
conn.close()
```

The native driver uses SBWP v1.1 with server-side prepare/bind and binary-only parameters. Wrapper
types for JSONB/RANGE/GEOMETRY are exposed by the driver API. Use it when you want full ScratchBird
feature access on port 3092.

## Quick Start

### Installation

```bash
# Native ScratchBird driver (recommended)
pip install scratchbird

# PostgreSQL drivers (emulation)
pip install psycopg2-binary

# Or psycopg3 (newer, async support)
pip install psycopg[binary]

# Or for MySQL protocol
pip install mysql-connector-python

# Or for async PostgreSQL
pip install asyncpg
```

### Install via sb_setup (Installer Utility)

If you installed ScratchBird with the installer, you can add the native driver pack later:

```bash
sb_setup --interactive
```

Select `scratchbird-driver-python` or the `scratchbird-drivers-all` meta package. On Linux, run with `sudo`.

### Basic Connection

```python
import psycopg2

# Connect to ScratchBird via PostgreSQL protocol
conn = psycopg2.connect(
    host="localhost",
    port=5432,
    database="mydb",
    user="myuser",
    password="mypassword"
)

# Create a cursor and execute queries
cursor = conn.cursor()
cursor.execute("SELECT version()")
print(cursor.fetchone())

# Clean up
cursor.close()
conn.close()
```

---

## Connection Methods

### psycopg2 (Recommended)

The most widely used PostgreSQL adapter for Python.

```python
import psycopg2
from psycopg2 import sql

# Basic connection
conn = psycopg2.connect(
    host="localhost",
    port=5432,
    database="scratchbird",
    user="app_user",
    password="secret"
)

# Connection with connection string
conn = psycopg2.connect(
    "postgresql://app_user:secret@localhost:5432/scratchbird"
)

# Connection with SSL
conn = psycopg2.connect(
    host="localhost",
    port=5432,
    database="scratchbird",
    user="app_user",
    password="secret",
    sslmode="require"
)

# Connection with timeout
conn = psycopg2.connect(
    host="localhost",
    port=5432,
    database="scratchbird",
    user="app_user",
    password="secret",
    connect_timeout=10
)
```

### psycopg3 (Modern Alternative)

The next generation PostgreSQL adapter with native async support.

```python
import psycopg

# Synchronous connection
conn = psycopg.connect(
    "postgresql://app_user:secret@localhost:5432/scratchbird"
)

# Context manager (recommended)
with psycopg.connect("postgresql://localhost:5432/scratchbird") as conn:
    with conn.cursor() as cur:
        cur.execute("SELECT * FROM users")
        for record in cur:
            print(record)

# Async connection
import asyncio

async def main():
    async with await psycopg.AsyncConnection.connect(
        "postgresql://localhost:5432/scratchbird"
    ) as conn:
        async with conn.cursor() as cur:
            await cur.execute("SELECT * FROM users")
            async for record in cur:
                print(record)

asyncio.run(main())
```

### asyncpg (High Performance Async)

Optimized for async applications.

```python
import asyncio
import asyncpg

async def main():
    # Create connection
    conn = await asyncpg.connect(
        host='localhost',
        port=5432,
        user='app_user',
        password='secret',
        database='scratchbird'
    )

    # Execute query
    rows = await conn.fetch('SELECT * FROM users WHERE active = $1', True)
    for row in rows:
        print(row['username'], row['email'])

    # Close connection
    await conn.close()

asyncio.run(main())
```

### MySQL Protocol

```python
import mysql.connector

# MySQL Connector/Python
conn = mysql.connector.connect(
    host="localhost",
    port=3306,
    database="scratchbird",
    user="app_user",
    password="secret"
)

cursor = conn.cursor()
cursor.execute("SELECT * FROM users")
for row in cursor:
    print(row)

cursor.close()
conn.close()
```

```python
import pymysql

# PyMySQL (pure Python)
conn = pymysql.connect(
    host='localhost',
    port=3306,
    user='app_user',
    password='secret',
    database='scratchbird'
)

with conn.cursor() as cursor:
    cursor.execute("SELECT * FROM users")
    for row in cursor.fetchall():
        print(row)

conn.close()
```

### Firebird Protocol

```python
import firebird.driver

# firebird-driver (modern)
with firebird.driver.connect(
    'localhost:3050/scratchbird',
    user='app_user',
    password='secret'
) as conn:
    with conn.cursor() as cur:
        cur.execute("SELECT * FROM users")
        for row in cur:
            print(row)
```

---

## CRUD Operations

### Create (INSERT)

```python
import psycopg2

conn = psycopg2.connect("postgresql://localhost:5432/scratchbird")
cursor = conn.cursor()

# Single insert with parameters (safe from SQL injection)
cursor.execute(
    "INSERT INTO users (username, email, created_at) VALUES (%s, %s, NOW())",
    ("john_doe", "john@example.com")
)

# Insert with RETURNING
cursor.execute(
    """
    INSERT INTO users (username, email)
    VALUES (%s, %s)
    RETURNING id, created_at
    """,
    ("jane_doe", "jane@example.com")
)
user_id, created = cursor.fetchone()
print(f"Created user {user_id} at {created}")

# Batch insert
users = [
    ("user1", "user1@example.com"),
    ("user2", "user2@example.com"),
    ("user3", "user3@example.com"),
]
cursor.executemany(
    "INSERT INTO users (username, email) VALUES (%s, %s)",
    users
)

conn.commit()
cursor.close()
conn.close()
```

### Read (SELECT)

```python
import psycopg2
import psycopg2.extras

conn = psycopg2.connect("postgresql://localhost:5432/scratchbird")

# Basic fetch
cursor = conn.cursor()
cursor.execute("SELECT id, username, email FROM users")
rows = cursor.fetchall()
for row in rows:
    print(f"ID: {row[0]}, Username: {row[1]}, Email: {row[2]}")

# Fetch one
cursor.execute("SELECT * FROM users WHERE id = %s", (1,))
user = cursor.fetchone()

# Fetch with limit
cursor.execute("SELECT * FROM users ORDER BY created_at DESC LIMIT %s", (10,))
recent_users = cursor.fetchall()

# Dictionary cursor (access by column name)
dict_cursor = conn.cursor(cursor_factory=psycopg2.extras.DictCursor)
dict_cursor.execute("SELECT * FROM users WHERE id = %s", (1,))
user = dict_cursor.fetchone()
print(user['username'], user['email'])

# Named tuple cursor
nt_cursor = conn.cursor(cursor_factory=psycopg2.extras.NamedTupleCursor)
nt_cursor.execute("SELECT * FROM users")
for user in nt_cursor:
    print(user.username, user.email)

cursor.close()
conn.close()
```

### Update

```python
import psycopg2

conn = psycopg2.connect("postgresql://localhost:5432/scratchbird")
cursor = conn.cursor()

# Simple update
cursor.execute(
    "UPDATE users SET email = %s WHERE id = %s",
    ("newemail@example.com", 1)
)

# Update with RETURNING
cursor.execute(
    """
    UPDATE users
    SET last_login = NOW(), login_count = login_count + 1
    WHERE id = %s
    RETURNING last_login, login_count
    """,
    (1,)
)
result = cursor.fetchone()
print(f"Last login: {result[0]}, Count: {result[1]}")

# Conditional update
cursor.execute(
    """
    UPDATE products
    SET price = price * 1.10
    WHERE category = %s AND last_updated < %s
    """,
    ("electronics", "2024-01-01")
)
print(f"Updated {cursor.rowcount} products")

conn.commit()
cursor.close()
conn.close()
```

### Delete

```python
import psycopg2

conn = psycopg2.connect("postgresql://localhost:5432/scratchbird")
cursor = conn.cursor()

# Simple delete
cursor.execute("DELETE FROM sessions WHERE user_id = %s", (1,))

# Delete with RETURNING
cursor.execute(
    """
    DELETE FROM audit_logs
    WHERE created_at < NOW() - INTERVAL '1 year'
    RETURNING id, action
    """
)
deleted = cursor.fetchall()
print(f"Deleted {len(deleted)} old audit logs")

# Delete with subquery
cursor.execute(
    """
    DELETE FROM order_items
    WHERE order_id IN (
        SELECT id FROM orders WHERE status = 'cancelled'
    )
    """
)

conn.commit()
cursor.close()
conn.close()
```

---

## Transactions

### Manual Transaction Control

```python
import psycopg2

conn = psycopg2.connect("postgresql://localhost:5432/scratchbird")
conn.autocommit = False  # Default, explicit for clarity

try:
    cursor = conn.cursor()

    # Transfer funds between accounts
    cursor.execute(
        "UPDATE accounts SET balance = balance - %s WHERE id = %s",
        (100, 1)
    )
    cursor.execute(
        "UPDATE accounts SET balance = balance + %s WHERE id = %s",
        (100, 2)
    )

    # Record the transfer
    cursor.execute(
        """
        INSERT INTO transfers (from_account, to_account, amount)
        VALUES (%s, %s, %s)
        """,
        (1, 2, 100)
    )

    conn.commit()
    print("Transfer successful")

except Exception as e:
    conn.rollback()
    print(f"Transfer failed: {e}")

finally:
    cursor.close()
    conn.close()
```

### Context Manager Transactions

```python
import psycopg2
from contextlib import contextmanager

@contextmanager
def transaction(conn):
    """Transaction context manager with automatic commit/rollback."""
    try:
        yield conn.cursor()
        conn.commit()
    except Exception:
        conn.rollback()
        raise

conn = psycopg2.connect("postgresql://localhost:5432/scratchbird")

with transaction(conn) as cursor:
    cursor.execute("INSERT INTO users (username) VALUES (%s)", ("test",))
    cursor.execute("INSERT INTO profiles (user_id) VALUES (lastval())")
    # Commits automatically if no exception
```

### Savepoints

```python
import psycopg2

conn = psycopg2.connect("postgresql://localhost:5432/scratchbird")
cursor = conn.cursor()

try:
    cursor.execute("INSERT INTO orders (customer_id) VALUES (%s)", (1,))

    # Create savepoint before risky operation
    cursor.execute("SAVEPOINT before_items")

    try:
        cursor.execute(
            "INSERT INTO order_items (order_id, product_id) VALUES (%s, %s)",
            (1, 999)  # May fail if product doesn't exist
        )
    except psycopg2.Error:
        # Rollback to savepoint, keeping the order
        cursor.execute("ROLLBACK TO SAVEPOINT before_items")
        print("Item insertion failed, order kept")

    conn.commit()

except Exception as e:
    conn.rollback()
    print(f"Order failed: {e}")

cursor.close()
conn.close()
```

---

## Connection Pooling

### psycopg2 with ThreadedConnectionPool

```python
from psycopg2 import pool
import threading

# Create pool (min 2, max 10 connections)
connection_pool = pool.ThreadedConnectionPool(
    minconn=2,
    maxconn=10,
    host="localhost",
    port=5432,
    database="scratchbird",
    user="app_user",
    password="secret"
)

def get_users():
    conn = connection_pool.getconn()
    try:
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM users")
        return cursor.fetchall()
    finally:
        connection_pool.putconn(conn)

# Use in threads
threads = []
for _ in range(5):
    t = threading.Thread(target=get_users)
    threads.append(t)
    t.start()

for t in threads:
    t.join()

# Clean up
connection_pool.closeall()
```

### asyncpg Connection Pool

```python
import asyncio
import asyncpg

async def main():
    # Create pool
    pool = await asyncpg.create_pool(
        host='localhost',
        port=5432,
        database='scratchbird',
        user='app_user',
        password='secret',
        min_size=5,
        max_size=20
    )

    # Use connection from pool
    async with pool.acquire() as conn:
        rows = await conn.fetch("SELECT * FROM users")
        for row in rows:
            print(row)

    # Or use pool directly
    rows = await pool.fetch("SELECT COUNT(*) FROM users")

    # Clean up
    await pool.close()

asyncio.run(main())
```

### SQLAlchemy with Connection Pool

```python
from sqlalchemy import create_engine, text
from sqlalchemy.pool import QueuePool

# Create engine with pool configuration
engine = create_engine(
    "postgresql://app_user:secret@localhost:5432/scratchbird",
    poolclass=QueuePool,
    pool_size=5,
    max_overflow=10,
    pool_timeout=30,
    pool_recycle=1800
)

# Use connection
with engine.connect() as conn:
    result = conn.execute(text("SELECT * FROM users"))
    for row in result:
        print(row)

# With transaction
with engine.begin() as conn:
    conn.execute(
        text("INSERT INTO users (username) VALUES (:name)"),
        {"name": "test_user"}
    )
    # Auto-commits on exit
```

---

## Prepared Statements

### psycopg2 Prepared Statements

```python
import psycopg2

conn = psycopg2.connect("postgresql://localhost:5432/scratchbird")
cursor = conn.cursor()

# Prepare statement (server-side)
cursor.execute("PREPARE get_user AS SELECT * FROM users WHERE id = $1")

# Execute prepared statement multiple times
for user_id in [1, 2, 3, 4, 5]:
    cursor.execute("EXECUTE get_user(%s)", (user_id,))
    print(cursor.fetchone())

# Deallocate when done
cursor.execute("DEALLOCATE get_user")

cursor.close()
conn.close()
```

### asyncpg Prepared Statements

```python
import asyncio
import asyncpg

async def main():
    conn = await asyncpg.connect(
        "postgresql://localhost:5432/scratchbird"
    )

    # Prepare statement
    stmt = await conn.prepare("SELECT * FROM users WHERE id = $1")

    # Execute multiple times (very efficient)
    for user_id in range(1, 100):
        row = await stmt.fetchrow(user_id)
        if row:
            print(row['username'])

    await conn.close()

asyncio.run(main())
```

---

## Error Handling

```python
import psycopg2
from psycopg2 import errors

conn = psycopg2.connect("postgresql://localhost:5432/scratchbird")
cursor = conn.cursor()

try:
    cursor.execute(
        "INSERT INTO users (username, email) VALUES (%s, %s)",
        ("duplicate_user", "dup@example.com")
    )
    conn.commit()

except errors.UniqueViolation as e:
    print(f"Duplicate entry: {e.diag.message_detail}")
    conn.rollback()

except errors.ForeignKeyViolation as e:
    print(f"Foreign key violation: {e.diag.constraint_name}")
    conn.rollback()

except errors.CheckViolation as e:
    print(f"Check constraint failed: {e.diag.constraint_name}")
    conn.rollback()

except errors.NotNullViolation as e:
    print(f"NULL value in non-null column: {e.diag.column_name}")
    conn.rollback()

except psycopg2.OperationalError as e:
    print(f"Connection error: {e}")

except psycopg2.Error as e:
    print(f"Database error: {e.pgcode} - {e.pgerror}")
    conn.rollback()

finally:
    cursor.close()
    conn.close()
```

---

## Best Practices

### Use Parameterized Queries

```python
# GOOD: Parameterized (safe)
cursor.execute(
    "SELECT * FROM users WHERE username = %s",
    (username,)
)

# BAD: String formatting (SQL injection risk!)
cursor.execute(
    f"SELECT * FROM users WHERE username = '{username}'"
)
```

### Use Context Managers

```python
# GOOD: Resources automatically cleaned up
with psycopg2.connect(...) as conn:
    with conn.cursor() as cursor:
        cursor.execute("SELECT * FROM users")
        # ...

# BAD: Manual cleanup (may leak on exception)
conn = psycopg2.connect(...)
cursor = conn.cursor()
# ... if exception here, resources leak
cursor.close()
conn.close()
```

### Handle Connection Failures

```python
import psycopg2
import time

def get_connection(max_retries=3, retry_delay=1):
    """Get database connection with retry logic."""
    for attempt in range(max_retries):
        try:
            return psycopg2.connect(
                "postgresql://localhost:5432/scratchbird",
                connect_timeout=5
            )
        except psycopg2.OperationalError as e:
            if attempt < max_retries - 1:
                print(f"Connection failed, retrying in {retry_delay}s...")
                time.sleep(retry_delay)
            else:
                raise
```

### Use Connection Pooling in Production

```python
# Create pool at application startup
from psycopg2 import pool

db_pool = pool.ThreadedConnectionPool(
    minconn=5,
    maxconn=20,
    dsn="postgresql://localhost:5432/scratchbird"
)

# Use throughout application
def query_users():
    conn = db_pool.getconn()
    try:
        # ... use connection
        pass
    finally:
        db_pool.putconn(conn)

# Close pool at shutdown
# db_pool.closeall()
```

---

## Framework Integration

### Flask

```python
from flask import Flask, g
import psycopg2

app = Flask(__name__)

def get_db():
    if 'db' not in g:
        g.db = psycopg2.connect(
            "postgresql://localhost:5432/scratchbird"
        )
    return g.db

@app.teardown_appcontext
def close_db(error):
    db = g.pop('db', None)
    if db is not None:
        db.close()

@app.route('/users')
def list_users():
    db = get_db()
    cursor = db.cursor()
    cursor.execute("SELECT id, username FROM users")
    users = cursor.fetchall()
    return {'users': [{'id': u[0], 'name': u[1]} for u in users]}
```

### Django

```python
# settings.py
DATABASES = {
    'default': {
        'ENGINE': 'django.db.backends.postgresql',
        'NAME': 'scratchbird',
        'USER': 'app_user',
        'PASSWORD': 'secret',
        'HOST': 'localhost',
        'PORT': '5432',
        'CONN_MAX_AGE': 600,  # Connection pooling
    }
}

# Using raw SQL
from django.db import connection

def get_users():
    with connection.cursor() as cursor:
        cursor.execute("SELECT * FROM users WHERE active = %s", [True])
        return cursor.fetchall()
```

### FastAPI with asyncpg

```python
from fastapi import FastAPI, Depends
import asyncpg

app = FastAPI()
pool = None

@app.on_event("startup")
async def startup():
    global pool
    pool = await asyncpg.create_pool(
        "postgresql://localhost:5432/scratchbird",
        min_size=5,
        max_size=20
    )

@app.on_event("shutdown")
async def shutdown():
    await pool.close()

async def get_db():
    async with pool.acquire() as conn:
        yield conn

@app.get("/users/{user_id}")
async def get_user(user_id: int, db=Depends(get_db)):
    row = await db.fetchrow(
        "SELECT * FROM users WHERE id = $1",
        user_id
    )
    return dict(row) if row else {"error": "Not found"}
```

---

## Common Issues

### Issue: SSL Connection Required

```python
# Error: SSL connection is required

# Solution: Add sslmode parameter
conn = psycopg2.connect(
    host="localhost",
    port=5432,
    database="scratchbird",
    user="app_user",
    password="secret",
    sslmode="require"  # or "prefer", "verify-full"
)
```

### Issue: Connection Timeout

```python
# Error: connection timed out

# Solution: Increase timeout and check network
conn = psycopg2.connect(
    host="localhost",
    port=5432,
    database="scratchbird",
    user="app_user",
    password="secret",
    connect_timeout=30,
    options="-c statement_timeout=60000"  # 60s query timeout
)
```

### Issue: Too Many Connections

```python
# Error: too many connections

# Solution: Use connection pooling
from psycopg2 import pool

# Instead of creating new connections each time
db_pool = pool.ThreadedConnectionPool(
    minconn=2,
    maxconn=10,  # Limit total connections
    dsn="postgresql://localhost:5432/scratchbird"
)
```

### Issue: Character Encoding

```python
# Ensure UTF-8 encoding
conn = psycopg2.connect(
    "postgresql://localhost:5432/scratchbird",
    options="-c client_encoding=UTF8"
)
```

---

## See Also

- [Driver Comparison](Driver-Comparison.md) - Compare all drivers
- [First Connection](../getting-started/first-connection.md) - Getting started guide
- [Connection Problems](../troubleshooting/Connection-Problems.md) - Troubleshooting
- [Web App Tutorial (Flask)](../tutorials/Web-App-Python-Flask.md) - Full Flask example

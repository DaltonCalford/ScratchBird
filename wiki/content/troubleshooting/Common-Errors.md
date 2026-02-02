# Common Errors

**Status:** Complete
**Last Updated:** 2026-01-30

---

## Overview

This guide provides a comprehensive reference for common ScratchBird errors, their causes, and solutions. Errors are organized by category for quick lookup.

---

## Error Code Format

ScratchBird uses structured error codes:

```
0xCCXX
  │ │
  │ └── Specific error within category
  └──── Error category
```

| Category | Range | Description |
|----------|-------|-------------|
| Syntax | 0x1000-0x1FFF | SQL parse errors |
| Semantic | 0x2000-0x2FFF | Type and name resolution |
| Execution | 0x3000-0x3FFF | Runtime errors |
| Constraint | 0x4000-0x4FFF | Constraint violations |
| Transaction | 0x5000-0x5FFF | Transaction errors |
| Storage | 0x6000-0x6FFF | I/O and storage errors |
| Security | 0x7000-0x7FFF | Auth/authorization errors |
| System | 0x8000-0x8FFF | Internal system errors |

---

## Connection Errors

### Connection Refused

**Error:**
```
could not connect to server: Connection refused
Is the server running on host "localhost" and accepting TCP/IP connections on port 5432?
```

**Causes:**
1. Server not running
2. Wrong host or port
3. Firewall blocking connection
4. Server only listening on localhost

**Solutions:**
```bash
# Check if server is running
systemctl status scratchbird

# Check listening ports
ss -tlnp | grep -E '3092|5432|3306|3050'

# Start server if not running
sudo systemctl start scratchbird
```

See: [Connection Problems](Connection-Problems.md#connection-refused)

---

### Authentication Failed

**Error:**
```
FATAL: password authentication failed for user "app_user"
```

**Causes:**
1. Wrong password
2. User doesn't exist
3. Wrong authentication method
4. Host not allowed in sb_hba.conf

**Solutions:**
```sql
-- Reset password
ALTER USER app_user WITH PASSWORD 'new_password';

-- Check if user exists
SELECT usename FROM pg_user WHERE usename = 'app_user';

-- Create user if missing
CREATE USER app_user WITH PASSWORD 'secure_password';
```

See: [Connection Problems](Connection-Problems.md#authentication-failed)

---

### Too Many Connections

**Error:**
```
FATAL: sorry, too many clients already
FATAL: too many connections for role "app_user"
```

**Causes:**
1. max_connections limit reached
2. Per-user connection limit reached
3. Connection leak in application

**Solutions:**
```sql
-- Check current connections
SELECT count(*) FROM pg_stat_activity;

-- Check max connections
SHOW max_connections;

-- Terminate idle connections
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE state = 'idle'
AND state_change < now() - interval '1 hour';
```

```ini
# sb_server.conf - increase limit
[connections]
max_connections = 200
```

---

### SSL/TLS Errors

**Error:**
```
SSL error: certificate verify failed
SSL SYSCALL error: Connection reset by peer
```

**Causes:**
1. SSL not enabled on server
2. Certificate expired or invalid
3. SSL mode mismatch

**Solutions:**
```ini
# sb_server.conf - enable SSL
[ssl]
enabled = true
cert_file = /etc/scratchbird/ssl/server.crt
key_file = /etc/scratchbird/ssl/server.key
```

```python
# Client - adjust SSL mode
conn = psycopg2.connect("sslmode=require")  # or verify-ca, verify-full
```

---

## Syntax Errors (0x1xxx)

### General Syntax Error

**Error:**
```
ERROR: syntax error at or near "FORM"
LINE 1: SELECT * FORM users;
                 ^
```

**Cause:** Typo or invalid SQL syntax

**Solution:** Check SQL syntax carefully
```sql
-- Wrong
SELECT * FORM users;

-- Correct
SELECT * FROM users;
```

---

### Unexpected Token

**Error:**
```
ERROR: syntax error at or near ","
LINE 1: SELECT id, , name FROM users;
                   ^
```

**Cause:** Extra comma, missing value

**Solution:**
```sql
-- Wrong
SELECT id, , name FROM users;

-- Correct
SELECT id, name FROM users;
```

---

### Unterminated String

**Error:**
```
ERROR: unterminated quoted string at or near "'hello"
```

**Cause:** Missing closing quote

**Solution:**
```sql
-- Wrong
SELECT 'hello;

-- Correct
SELECT 'hello';

-- For strings with quotes, escape them
SELECT 'it''s working';
-- Or use dollar quoting
SELECT $$it's working$$;
```

---

### Reserved Word Used as Identifier

**Error:**
```
ERROR: syntax error at or near "user"
```

**Cause:** Using reserved keyword as column/table name

**Solution:**
```sql
-- Wrong
CREATE TABLE user (id INT);

-- Correct - quote the identifier
CREATE TABLE "user" (id INT);

-- Better - use a different name
CREATE TABLE users (id INT);
```

**Common reserved words:** `user`, `order`, `group`, `table`, `select`, `index`, `key`

---

## Semantic Errors (0x2xxx)

### Table Does Not Exist

**Error:**
```
ERROR: relation "userss" does not exist
LINE 1: SELECT * FROM userss;
                      ^
```

**Causes:**
1. Typo in table name
2. Table not created yet
3. Wrong schema
4. Case sensitivity issue

**Solutions:**
```sql
-- Check existing tables
SELECT tablename FROM pg_tables WHERE schemaname = 'public';

-- Check with schema
SELECT * FROM public.users;

-- Case sensitive (if created with quotes)
SELECT * FROM "Users";  -- if created as CREATE TABLE "Users"
```

---

### Column Does Not Exist

**Error:**
```
ERROR: column "emial" does not exist
LINE 1: SELECT emial FROM users;
               ^
HINT: Perhaps you meant to reference the column "users.email".
```

**Cause:** Typo or wrong column name

**Solution:**
```sql
-- Check column names
SELECT column_name FROM information_schema.columns
WHERE table_name = 'users';

-- Correct the typo
SELECT email FROM users;
```

---

### Ambiguous Column Reference

**Error:**
```
ERROR: column reference "id" is ambiguous
LINE 1: SELECT id FROM users JOIN orders ON users.id = orders.user_id;
               ^
```

**Cause:** Column exists in multiple tables in JOIN

**Solution:**
```sql
-- Wrong
SELECT id FROM users JOIN orders ON users.id = orders.user_id;

-- Correct - qualify column name
SELECT users.id FROM users JOIN orders ON users.id = orders.user_id;

-- Or use alias
SELECT u.id FROM users u JOIN orders o ON u.id = o.user_id;
```

---

### Type Mismatch

**Error:**
```
ERROR: operator does not exist: integer = text
HINT: No operator matches the given name and argument types.
```

**Cause:** Comparing incompatible types

**Solution:**
```sql
-- Wrong (comparing int to string)
SELECT * FROM users WHERE id = '123';

-- Correct - use proper type
SELECT * FROM users WHERE id = 123;

-- Or cast explicitly
SELECT * FROM users WHERE id = '123'::int;
```

---

### Function Does Not Exist

**Error:**
```
ERROR: function concat_ws(unknown, text, text) does not exist
HINT: No function matches the given name and argument types.
```

**Causes:**
1. Function name typo
2. Wrong argument types
3. Function not available in this dialect

**Solution:**
```sql
-- Check function exists
SELECT proname FROM pg_proc WHERE proname LIKE '%concat%';

-- Use correct function signature
SELECT concat_ws(' ', first_name, last_name) FROM users;
```

---

## Constraint Errors (0x4xxx)

### NOT NULL Violation

**Error:**
```
ERROR: null value in column "email" violates not-null constraint
DETAIL: Failing row contains (1, John, null).
```

**Cause:** Inserting NULL into NOT NULL column

**Solution:**
```sql
-- Wrong
INSERT INTO users (id, name, email) VALUES (1, 'John', NULL);

-- Correct - provide value
INSERT INTO users (id, name, email) VALUES (1, 'John', 'john@example.com');

-- Or set default
ALTER TABLE users ALTER COLUMN email SET DEFAULT 'unknown@example.com';
```

---

### Unique Violation

**Error:**
```
ERROR: duplicate key value violates unique constraint "users_email_key"
DETAIL: Key (email)=(john@example.com) already exists.
```

**Cause:** Inserting duplicate value in unique column

**Solutions:**
```sql
-- Check for existing value first
SELECT * FROM users WHERE email = 'john@example.com';

-- Use INSERT ... ON CONFLICT (upsert)
INSERT INTO users (id, name, email)
VALUES (1, 'John', 'john@example.com')
ON CONFLICT (email) DO UPDATE SET name = EXCLUDED.name;

-- Or use INSERT ... ON CONFLICT DO NOTHING
INSERT INTO users (id, name, email)
VALUES (1, 'John', 'john@example.com')
ON CONFLICT (email) DO NOTHING;
```

---

### Primary Key Violation

**Error:**
```
ERROR: duplicate key value violates unique constraint "users_pkey"
DETAIL: Key (id)=(1) already exists.
```

**Cause:** Inserting duplicate primary key

**Solutions:**
```sql
-- Use SERIAL/IDENTITY for auto-increment
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100)
);

-- Or let database generate ID
INSERT INTO users (name) VALUES ('John');  -- id auto-generated

-- Check max ID if manually managing
SELECT MAX(id) FROM users;
```

---

### Foreign Key Violation

**Error:**
```
ERROR: insert or update on table "orders" violates foreign key constraint "orders_customer_id_fkey"
DETAIL: Key (customer_id)=(999) is not present in table "customers".
```

**Cause:** Referencing non-existent parent record

**Solutions:**
```sql
-- Check if parent exists
SELECT * FROM customers WHERE id = 999;

-- Create parent first
INSERT INTO customers (id, name) VALUES (999, 'New Customer');
INSERT INTO orders (customer_id, total) VALUES (999, 100.00);

-- Or use valid customer_id
INSERT INTO orders (customer_id, total)
VALUES ((SELECT id FROM customers LIMIT 1), 100.00);
```

**On DELETE errors:**
```
ERROR: update or delete on table "customers" violates foreign key constraint
DETAIL: Key (id)=(1) is still referenced from table "orders".
```

**Solutions:**
```sql
-- Delete child records first
DELETE FROM orders WHERE customer_id = 1;
DELETE FROM customers WHERE id = 1;

-- Or use CASCADE (if FK allows)
ALTER TABLE orders DROP CONSTRAINT orders_customer_id_fkey;
ALTER TABLE orders ADD CONSTRAINT orders_customer_id_fkey
    FOREIGN KEY (customer_id) REFERENCES customers(id) ON DELETE CASCADE;
```

---

### Check Constraint Violation

**Error:**
```
ERROR: new row for relation "products" violates check constraint "products_price_check"
DETAIL: Failing row contains (1, Widget, -5.00).
```

**Cause:** Value doesn't satisfy CHECK constraint

**Solution:**
```sql
-- Check constraint definition
SELECT conname, pg_get_constraintdef(oid)
FROM pg_constraint
WHERE conrelid = 'products'::regclass;

-- Use valid value
INSERT INTO products (id, name, price) VALUES (1, 'Widget', 5.00);  -- positive price
```

---

## Transaction Errors (0x5xxx)

### Serialization Failure

**Error:**
```
ERROR: could not serialize access due to concurrent update
```

**Cause:** Concurrent transactions conflict in SERIALIZABLE isolation

**Solution:**
```python
# Retry transaction
import time

max_retries = 3
for attempt in range(max_retries):
    try:
        with conn.cursor() as cur:
            cur.execute("BEGIN ISOLATION LEVEL SERIALIZABLE")
            # ... your queries ...
            cur.execute("COMMIT")
        break
    except psycopg2.errors.SerializationFailure:
        conn.rollback()
        if attempt < max_retries - 1:
            time.sleep(0.1 * (attempt + 1))  # Exponential backoff
        else:
            raise
```

---

### Deadlock Detected

**Error:**
```
ERROR: deadlock detected
DETAIL: Process 1234 waits for ShareLock on transaction 5678; blocked by process 9012.
Process 9012 waits for ShareLock on transaction 1234; blocked by process 1234.
HINT: See server log for query details.
```

**Cause:** Two transactions waiting for each other's locks

**Solutions:**
```sql
-- Access tables in consistent order
-- Transaction 1 and 2 should both do:
BEGIN;
UPDATE accounts SET balance = balance - 100 WHERE id = 1;  -- Always lock lower ID first
UPDATE accounts SET balance = balance + 100 WHERE id = 2;
COMMIT;

-- Use shorter transactions
-- Use SELECT ... FOR UPDATE NOWAIT or SKIP LOCKED
SELECT * FROM accounts WHERE id = 1 FOR UPDATE NOWAIT;
```

---

### Transaction Aborted

**Error:**
```
ERROR: current transaction is aborted, commands ignored until end of transaction block
```

**Cause:** Previous command in transaction failed

**Solution:**
```sql
-- Must ROLLBACK before continuing
ROLLBACK;

-- Then start fresh
BEGIN;
-- ... new queries ...
COMMIT;
```

```python
# In application code
try:
    cursor.execute("INSERT INTO ...")
except Exception:
    conn.rollback()  # Must rollback
    raise
```

---

## Execution Errors (0x3xxx)

### Division by Zero

**Error:**
```
ERROR: division by zero
```

**Solution:**
```sql
-- Wrong
SELECT total / quantity FROM orders;

-- Correct - use NULLIF to avoid division by zero
SELECT total / NULLIF(quantity, 0) FROM orders;

-- Or use CASE
SELECT CASE WHEN quantity = 0 THEN 0 ELSE total / quantity END FROM orders;
```

---

### Numeric Overflow

**Error:**
```
ERROR: numeric field overflow
DETAIL: A field with precision 5, scale 2 cannot hold a value of 1000000.
```

**Cause:** Value too large for column precision

**Solutions:**
```sql
-- Check column definition
\d products  -- shows price NUMERIC(5,2) = max 999.99

-- Use larger precision
ALTER TABLE products ALTER COLUMN price TYPE NUMERIC(10,2);

-- Or use appropriate value
UPDATE products SET price = 999.99 WHERE price > 999.99;
```

---

### String Too Long

**Error:**
```
ERROR: value too long for type character varying(50)
```

**Cause:** String exceeds VARCHAR length

**Solutions:**
```sql
-- Increase column size
ALTER TABLE users ALTER COLUMN name TYPE VARCHAR(100);

-- Or truncate input
INSERT INTO users (name) VALUES (LEFT('very long name...', 50));
```

---

### Invalid Input Syntax

**Error:**
```
ERROR: invalid input syntax for type integer: "abc"
```

**Cause:** String cannot be converted to target type

**Solution:**
```sql
-- Wrong
SELECT '123abc'::integer;

-- Correct - use valid value
SELECT '123'::integer;

-- Or handle with CASE
SELECT CASE
    WHEN value ~ '^[0-9]+$' THEN value::integer
    ELSE NULL
END FROM data;
```

---

### Date/Time Errors

**Error:**
```
ERROR: date/time field value out of range: "2026-13-45"
```

**Cause:** Invalid date/time value

**Solution:**
```sql
-- Wrong
SELECT '2026-13-45'::date;

-- Correct
SELECT '2026-01-20'::date;

-- Validate before inserting
SELECT CASE
    WHEN value ~ '^\d{4}-\d{2}-\d{2}$' THEN value::date
    ELSE NULL
END;
```

---

## Storage Errors (0x6xxx)

### Disk Full

**Error:**
```
ERROR: could not extend file "base/16384/12345": No space left on device
HINT: Check free disk space.
```

**Solutions:**
```bash
# Check disk space
df -h /var/lib/scratchbird

# Free space
# - Delete old WAL files (if archived)
# - VACUUM FULL to reclaim space
# - Delete unnecessary data
# - Add more disk space
```

```sql
-- Reclaim space from deleted rows
VACUUM FULL large_table;

-- Delete old data
DELETE FROM logs WHERE created_at < now() - interval '90 days';
VACUUM logs;
```

---

### File Not Found

**Error:**
```
ERROR: could not open file "base/16384/12345": No such file or directory
```

**Cause:** Data file missing or corrupted

**Solution:**
```bash
# Check data directory integrity
# Restore from backup if files are missing/corrupted
sb_restore -d scratchbird backup.sbk
```

---

## Security Errors (0x7xxx)

### Permission Denied

**Error:**
```
ERROR: permission denied for table customers
```

**Cause:** User lacks required privileges

**Solutions:**
```sql
-- Grant required permissions (as superuser)
GRANT SELECT ON customers TO app_user;
GRANT INSERT, UPDATE, DELETE ON customers TO app_user;

-- Grant on all tables in schema
GRANT SELECT ON ALL TABLES IN SCHEMA public TO app_user;

-- Check current permissions
SELECT grantee, privilege_type
FROM information_schema.table_privileges
WHERE table_name = 'customers';
```

---

### Must Be Owner

**Error:**
```
ERROR: must be owner of table customers
```

**Cause:** Operation requires ownership

**Solutions:**
```sql
-- Change owner (as superuser)
ALTER TABLE customers OWNER TO app_user;

-- Or grant necessary privileges instead
GRANT ALL ON customers TO app_user;
```

---

### Insufficient Privilege

**Error:**
```
ERROR: permission denied to create database
```

**Solution:**
```sql
-- Grant CREATEDB privilege (as superuser)
ALTER USER app_user CREATEDB;

-- Or create database as superuser
CREATE DATABASE newdb OWNER app_user;
```

---

## Quick Lookup by Error Message

| Error Contains | Likely Cause | Quick Fix |
|----------------|--------------|-----------|
| "Connection refused" | Server not running | `systemctl start scratchbird` |
| "password authentication failed" | Wrong password | Reset password |
| "too many clients" | Connection limit | Increase max_connections |
| "does not exist" | Typo or missing object | Check spelling |
| "violates not-null" | Missing required value | Provide value |
| "violates unique" | Duplicate value | Use ON CONFLICT |
| "violates foreign key" | Missing parent record | Create parent first |
| "deadlock detected" | Lock conflict | Retry transaction |
| "permission denied" | Missing GRANT | Grant privileges |
| "syntax error" | SQL typo | Check SQL syntax |
| "type mismatch" | Wrong data type | Cast or fix type |
| "division by zero" | Zero divisor | Use NULLIF |
| "No space left" | Disk full | Free space |

---

## Debugging Tips

### Get More Error Details

```sql
-- Enable verbose errors
SET client_min_messages TO DEBUG;

-- Check server log for details
-- /var/log/scratchbird/scratchbird.log
```

### Capture Error in Application

```python
# Python - get full error details
try:
    cursor.execute(sql)
except psycopg2.Error as e:
    print(f"Error code: {e.pgcode}")
    print(f"Error message: {e.pgerror}")
    print(f"Diagnostic: {e.diag.message_detail}")
```

```javascript
// Node.js
try {
    await client.query(sql);
} catch (err) {
    console.log('Code:', err.code);
    console.log('Detail:', err.detail);
    console.log('Hint:', err.hint);
}
```

### Check Server Log

```bash
# View recent errors
sudo journalctl -u scratchbird --since "1 hour ago" | grep -i error

# Or check log file
sudo tail -100 /var/log/scratchbird/scratchbird.log | grep -i error
```

---

## See Also

- [Connection Problems](Connection-Problems.md) - Connection troubleshooting
- [Performance Issues](Performance-Issues.md) - Performance troubleshooting
- [Error Codes Reference](../reference/Error-Codes.md) - Complete error code list
- [Security Guide](../admin/security.md) - Permissions and authentication

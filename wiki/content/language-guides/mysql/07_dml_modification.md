[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL Data Modification Language (INSERT, UPDATE, DELETE, REPLACE)

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

This document covers data modification operations in MySQL emulation mode. These statements allow inserting, updating, deleting, and replacing rows in database tables.

**Important:** INSERT, UPDATE, and DELETE emit correct bytecode for basic operations. ON DUPLICATE KEY UPDATE is implemented via EXT_ON_CONFLICT opcodes. ORDER BY/LIMIT on UPDATE/DELETE are parsed but not emitted.

---

## INSERT

Adds new rows to a table.

### Syntax

```sql
INSERT [LOW_PRIORITY | DELAYED | HIGH_PRIORITY] [IGNORE]
    INTO table_name
    [(column_list)]
    {VALUES (value_list) [, (value_list)] ... | SELECT ...}
    [ON DUPLICATE KEY UPDATE column=expr [, column=expr] ...]
```

### Basic INSERT

**Single row:**
```sql
INSERT INTO users (id, name, email) VALUES (1, 'John Doe', 'john@example.com');
```

**Multiple rows:**
```sql
INSERT INTO users (id, name, email) VALUES
    (1, 'John Doe', 'john@example.com'),
    (2, 'Jane Smith', 'jane@example.com'),
    (3, 'Bob Johnson', 'bob@example.com');
```

**All columns (implicit order):**
```sql
INSERT INTO products VALUES (1, 'Widget', 29.99, 100);
```

**INSERT with DEFAULT:**
```sql
INSERT INTO users (id, name, status) VALUES (1, 'John', DEFAULT);
```

**INSERT with expressions:**
```sql
INSERT INTO logs (user_id, message, created_at)
VALUES (1, 'Login successful', NOW());
```

### INSERT SELECT

Insert rows from a query:
```sql
INSERT INTO user_backup (id, name, email)
SELECT id, name, email FROM users WHERE created_at < '2020-01-01';
```

### ON DUPLICATE KEY UPDATE

Update existing row if duplicate key found:
```sql
INSERT INTO inventory (product_id, quantity) VALUES (1, 10)
ON DUPLICATE KEY UPDATE quantity = quantity + VALUES(quantity);

INSERT INTO users (id, email, login_count) VALUES (1, 'user@example.com', 1)
ON DUPLICATE KEY UPDATE login_count = login_count + 1;
```

### INSERT Modifiers

**INSERT IGNORE:**
```sql
-- Ignore errors on duplicate keys:
INSERT IGNORE INTO users (id, email) VALUES (1, 'existing@example.com');
```

**LOW_PRIORITY, HIGH_PRIORITY, DELAYED:**
```sql
INSERT LOW_PRIORITY INTO logs VALUES (...);  -- Wait for no concurrent reads
INSERT DELAYED INTO logs VALUES (...);        -- Asynchronous (deprecated)
```

### Current Status

**Implemented:** INSERT emits correct bytecode:
- Single-row and multi-row INSERT with VALUES emit INSERT opcode, TABLE_REF, column list, and value rows
- INSERT SELECT captures and emits the SELECT bytecode inline
- ON DUPLICATE KEY UPDATE emits EXT_ON_CONFLICT + EXT_ON_CONFLICT_DO_UPDATE with assignment list
- DEFAULT values handling supported (explicit column filtering)
- Column list auto-resolution from catalog when no column list specified
- INSERT modifiers (LOW_PRIORITY, DELAYED, HIGH_PRIORITY, IGNORE) are parsed but ignored

---

## UPDATE

Modifies existing rows in a table.

### Syntax

```sql
UPDATE [LOW_PRIORITY] [IGNORE] table_name
    SET column=expr [, column=expr] ...
    [WHERE condition]
    [ORDER BY ...] [LIMIT count]
```

### Basic UPDATE

**Update single column:**
```sql
UPDATE users SET email = 'newemail@example.com' WHERE id = 1;
```

**Update multiple columns:**
```sql
UPDATE products
SET price = 29.99, stock = stock - 1
WHERE id = 100;
```

**Update with expressions:**
```sql
UPDATE orders
SET total = subtotal + (subtotal * tax_rate)
WHERE status = 'pending';
```

**UPDATE all rows (no WHERE):**
```sql
UPDATE settings SET last_updated = NOW();
```

### UPDATE with JOIN

```sql
UPDATE inventory i
JOIN products p ON i.product_id = p.id
SET i.price = p.retail_price * 0.9
WHERE p.category = 'clearance';
```

### UPDATE with ORDER BY and LIMIT

```sql
-- Update oldest 10 records:
UPDATE logs
SET archived = 1
ORDER BY created_at ASC
LIMIT 10;
```

### UPDATE with Subquery

```sql
UPDATE products
SET price = (SELECT AVG(price) FROM products WHERE category = products.category)
WHERE price IS NULL;
```

### Current Status

**Implemented:** UPDATE emits correct bytecode:
- UPDATE opcode with TABLE_REF (name + alias)
- SET clause emits ASSIGNMENT list with COLUMN_REF and expression bytecode
- WHERE clause emitted via WHERE_CLAUSE opcode
- Multi-table UPDATE: additional tables after comma are parsed but skipped (emit disabled)
- ORDER BY and LIMIT are parsed but not emitted to bytecode

---

## DELETE

Removes rows from a table.

### Syntax

```sql
DELETE [LOW_PRIORITY] [QUICK] [IGNORE]
    FROM table_name
    [WHERE condition]
    [ORDER BY ...] [LIMIT count]
```

### Basic DELETE

**Delete specific rows:**
```sql
DELETE FROM users WHERE id = 1;
```

**Delete with complex condition:**
```sql
DELETE FROM logs
WHERE created_at < DATE_SUB(NOW(), INTERVAL 30 DAY)
  AND level = 'debug';
```

**DELETE all rows (caution!):**
```sql
DELETE FROM temp_data;
```

### DELETE with ORDER BY and LIMIT

```sql
-- Delete oldest 100 records:
DELETE FROM logs
ORDER BY created_at ASC
LIMIT 100;
```

### DELETE with JOIN

```sql
DELETE users
FROM users
INNER JOIN user_status ON users.id = user_status.user_id
WHERE user_status.inactive_days > 365;
```

### DELETE vs TRUNCATE

| Feature | DELETE | TRUNCATE |
|---------|--------|----------|
| WHERE clause | Supported | Not supported |
| Triggers | Fired | Not fired |
| Speed | Slower | Faster |
| Rollback | Can rollback | Cannot rollback (usually) |
| Auto-increment | Preserved | Reset |

### Current Status

**Implemented:** DELETE emits correct bytecode:
- DELETE opcode with TABLE_REF (name + alias)
- WHERE clause emitted via WHERE_CLAUSE opcode
- ORDER BY and LIMIT are parsed but not emitted to bytecode
- Multi-table DELETE syntax is not supported

---

## REPLACE

Replaces existing rows or inserts new ones.

### Overview

`REPLACE` works like INSERT, but if a duplicate key is found, it deletes the old row and inserts the new one.

### Syntax

```sql
REPLACE [LOW_PRIORITY | DELAYED]
    INTO table_name
    [(column_list)]
    {VALUES (value_list) [, (value_list)] ... | SELECT ...}
```

### Examples

**Basic replace:**
```sql
REPLACE INTO users (id, name, email)
VALUES (1, 'John Doe', 'john@example.com');
```

**Replace multiple rows:**
```sql
REPLACE INTO cache (key_name, value) VALUES
    ('user:1', 'John'),
    ('user:2', 'Jane');
```

**Replace from SELECT:**
```sql
REPLACE INTO user_summary (user_id, total_orders)
SELECT customer_id, COUNT(*) FROM orders GROUP BY customer_id;
```

### REPLACE vs INSERT ... ON DUPLICATE KEY UPDATE

**REPLACE behavior:**
- DELETE old row (if exists)
- INSERT new row
- Triggers fire for both DELETE and INSERT
- Row OID changes

**INSERT ... ON DUPLICATE KEY UPDATE behavior:**
- UPDATE existing row
- Only UPDATE triggers fire
- Same row OID preserved

### Current Status

**Implemented (with semantic difference):** REPLACE emits as INSERT opcode plus EXT_ON_CONFLICT_DO_UPDATE:
- Uses ON CONFLICT UPDATE semantics instead of MySQL's DELETE + INSERT semantics
- Trigger behavior differs: MySQL fires DELETE + INSERT triggers; ScratchBird fires UPDATE triggers
- Row identity may differ (MySQL creates new row; ScratchBird updates in-place)

---

## Modifiers

### INSERT/UPDATE/DELETE Modifiers

**LOW_PRIORITY:**
```sql
INSERT LOW_PRIORITY INTO table VALUES (...);
UPDATE LOW_PRIORITY table SET ...;
DELETE LOW_PRIORITY FROM table ...;
```
- Waits until no concurrent reads
- MyISAM-specific (not applicable to ScratchBird)

**HIGH_PRIORITY:**
```sql
INSERT HIGH_PRIORITY INTO table VALUES (...);
```
- Jumps the read queue
- MyISAM-specific

**DELAYED:**
```sql
INSERT DELAYED INTO table VALUES (...);
```
- Asynchronous insert (deprecated in MySQL 5.6+)

**IGNORE:**
```sql
INSERT IGNORE INTO table VALUES (...);
UPDATE IGNORE table SET ...;
DELETE IGNORE FROM table ...;
```
- Suppresses errors (e.g., duplicate key violations)
- Converts errors to warnings

### Current Status

**Parsed but ignored:** All modifiers are consumed during parsing but have no runtime effect:
- LOW_PRIORITY/HIGH_PRIORITY/DELAYED are MySQL engine-level hints not applicable to ScratchBird
- IGNORE is parsed but not mapped to ON CONFLICT DO NOTHING behavior

---

## Best Practices

### INSERT Best Practices

1. **Use explicit column lists:**
   ```sql
   -- Good:
   INSERT INTO users (id, name) VALUES (1, 'John');

   -- Avoid (brittle):
   INSERT INTO users VALUES (1, 'John', ...);
   ```

2. **Batch inserts for performance:**
   ```sql
   INSERT INTO logs VALUES (1, 'msg1'), (2, 'msg2'), (3, 'msg3');
   ```

3. **Use ON DUPLICATE KEY UPDATE for upserts:**
   ```sql
   INSERT INTO counters (id, count) VALUES (1, 1)
   ON DUPLICATE KEY UPDATE count = count + 1;
   ```

### UPDATE Best Practices

1. **Always use WHERE (unless updating all rows):**
   ```sql
   -- Dangerous without WHERE:
   UPDATE users SET active = 0;  -- Updates ALL users!
   ```

2. **Test with SELECT first:**
   ```sql
   -- Test what will be updated:
   SELECT * FROM users WHERE last_login < '2020-01-01';

   -- Then update:
   UPDATE users SET active = 0 WHERE last_login < '2020-01-01';
   ```

3. **Use LIMIT with ORDER BY for partial updates:**
   ```sql
   UPDATE logs SET archived = 1 ORDER BY created_at LIMIT 1000;
   ```

### DELETE Best Practices

1. **Use WHERE clause:**
   ```sql
   -- Always specify what to delete:
   DELETE FROM logs WHERE created_at < '2024-01-01';
   ```

2. **Use transactions for safety:**
   ```sql
   START TRANSACTION;
   DELETE FROM users WHERE inactive_days > 365;
   -- Review affected rows
   ROLLBACK;  -- or COMMIT;
   ```

3. **Consider soft deletes:**
   ```sql
   -- Instead of DELETE:
   UPDATE users SET deleted_at = NOW() WHERE id = 1;
   ```

---

## Known Limitations

### What Works

- **INSERT**: Single/multi-row, INSERT SELECT, ON DUPLICATE KEY UPDATE, DEFAULT values, catalog column resolution
- **UPDATE**: Single-table with SET, WHERE
- **DELETE**: Single-table with WHERE
- **REPLACE**: Parsed as INSERT + ON CONFLICT DO UPDATE

### Partial Implementation

- **UPDATE/DELETE ORDER BY and LIMIT**: Parsed but bytecode emission is disabled (no runtime effect)
- **Multi-table UPDATE**: Additional tables after comma are parsed but skipped during emission
- **REPLACE semantics**: Maps to ON CONFLICT UPDATE instead of DELETE + INSERT (different trigger and row identity behavior)
- **INSERT modifiers**: LOW_PRIORITY, HIGH_PRIORITY, DELAYED, IGNORE are parsed but ignored at runtime

### Missing Features

- **Multi-table DELETE**: Not supported
- **INSERT IGNORE → ON CONFLICT DO NOTHING mapping**: IGNORE keyword is consumed but has no bytecode effect

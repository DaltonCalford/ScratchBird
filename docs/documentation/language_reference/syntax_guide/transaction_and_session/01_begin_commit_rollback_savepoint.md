<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# Transaction Control

[Prev](./README.md) | [Next](./02_set_transaction_and_isolation.md) | [Topic README](./README.md) | [Syntax Guide README](../README.md)

## Coverage and Evidence Status

Status: Complete

## Synopsis

Transactions ensure ACID properties for database operations. ScratchBird uses MGA (Multi-Generational Architecture) for transaction management.

## Transaction Statements

### BEGIN

Start a new transaction.

```sql
-- Basic transaction
BEGIN;
-- ... SQL statements ...
COMMIT;

-- With transaction mode
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE READ WRITE;

-- START TRANSACTION (alternative syntax)
START TRANSACTION;
```

### COMMIT

Commit the current transaction.

```sql
-- Commit transaction
COMMIT;

-- Alternative syntax
COMMIT WORK;

-- Commit and chain (start new transaction immediately)
COMMIT AND CHAIN;
```

### ROLLBACK

Abort the current transaction.

```sql
-- Rollback entire transaction
ROLLBACK;

-- Alternative syntax
ROLLBACK WORK;

-- Rollback and chain
ROLLBACK AND CHAIN;
```

## SAVEPOINT

Savepoints allow partial rollback within a transaction.

### Creating Savepoints

```sql
BEGIN;

-- Insert user
INSERT INTO users (name, email) VALUES ('John', 'john@example.com');
SAVEPOINT user_created;

-- Insert profile (may fail)
INSERT INTO profiles (user_id, bio) VALUES (lastval(), 'Bio');

-- If profile insert fails, rollback to savepoint
-- ROLLBACK TO SAVEPOINT user_created;

-- Continue with other operations
INSERT INTO audit_log (event) VALUES ('user_created');

COMMIT;
```

### Savepoint Operations

```sql
-- Create savepoint
SAVEPOINT savepoint_name;

-- Rollback to savepoint
ROLLBACK TO SAVEPOINT savepoint_name;

-- Alternative syntax
ROLLBACK TO savepoint_name;

-- Release savepoint (commit it)
RELEASE SAVEPOINT savepoint_name;
```

### Savepoint Example

```sql
BEGIN;

-- Step 1: Create order
INSERT INTO orders (user_id, total) VALUES (1, 100.00);
SAVEPOINT order_created;

-- Step 2: Add items (loop with potential failures)
FOR item IN SELECT * FROM cart_items WHERE user_id = 1 LOOP
    BEGIN
        INSERT INTO order_items (order_id, product_id, quantity)
        VALUES (lastval(), item.product_id, item.quantity);
    EXCEPTION WHEN insufficient_stock THEN
        -- Skip this item but continue
        ROLLBACK TO SAVEPOINT order_created;
        INSERT INTO failed_items (product_id) VALUES (item.product_id);
        SAVEPOINT order_created;
    END;
END LOOP;

-- Step 3: Clear cart
DELETE FROM cart_items WHERE user_id = 1;

COMMIT;
```

## Transaction States

```
┌─────────────┐
│   IDLE      │
└──────┬──────┘
       │ BEGIN
       ▼
┌─────────────┐
│  ACTIVE     │◄──────────┐
└──────┬──────┘           │
       │                  │
   ┌───┴───┐              │
   │       │              │
   ▼       ▼              │
┌──────┐ ┌──────┐         │
│COMMIT│ │ROLLBACK│        │
└──────┘ └──────┘         │
       │                  │
       │ SAVEPOINT        │
       ▼                  │
┌─────────────┐           │
│ SAVEPOINT   │───────────┘
└─────────────┘ ROLLBACK TO
```

## MGA Transaction Behavior

Under MGA:

```
Transaction 100:
BEGIN;
INSERT INTO users (name) VALUES ('Alice');  -- Creates Version 1 (TXN 100, ACTIVE)
UPDATE users SET name = 'Bob' WHERE id = 1;  -- Creates Version 2 (TXN 100, ACTIVE)
COMMIT;                                       -- Versions: ACTIVE → COMMITTED

Other transactions see:
- Before commit: Old versions or nothing
- After commit: New versions based on their snapshot
```

## Auto-Commit Mode

```sql
-- Check auto-commit status
SHOW AUTOCOMMIT;

-- Enable auto-commit (each statement is a transaction)
SET AUTOCOMMIT = ON;

-- Disable auto-commit (explicit transactions required)
SET AUTOCOMMIT = OFF;
```

## Error Handling

```sql
BEGIN;

-- Option 1: Let errors rollback automatically
-- (Default behavior: transaction aborted on error)

-- Option 2: Handle errors explicitly
DO $$
BEGIN
    INSERT INTO users (email) VALUES ('invalid');
EXCEPTION WHEN unique_violation THEN
    -- Handle error
    RAISE NOTICE 'User already exists';
END $$;

COMMIT;
```

## DDL in Transactions

ScratchBird supports DDL in transactions:

```sql
BEGIN;

CREATE TABLE temp_data (id INT, value TEXT);
INSERT INTO temp_data VALUES (1, 'test');

-- Can rollback DDL!
ROLLBACK;  -- Table temp_data never created
```

## Long-Running Transactions

### Impact

Long transactions in MGA:
- Hold snapshots (prevent GC of old versions)
- Increase storage usage
- Can cause "snapshot too old" errors

### Best Practices

```sql
-- Keep transactions short
BEGIN;
-- Do work
COMMIT;

-- For batch operations, commit periodically
BEGIN;
FOR i IN 1..1000000 LOOP
    INSERT INTO data VALUES (i);
    IF i % 10000 = 0 THEN
        COMMIT;
        BEGIN;
    END IF;
END LOOP;
COMMIT;
```

## Parser Acceptance Cases

```sql
BEGIN;
BEGIN TRANSACTION;
START TRANSACTION;
COMMIT;
COMMIT WORK;
ROLLBACK;
SAVEPOINT sp1;
ROLLBACK TO SAVEPOINT sp1;
RELEASE SAVEPOINT sp1;
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `no_active_transaction` | COMMIT/ROLLBACK without BEGIN |
| `savepoint_does_not_exist` | ROLLBACK TO unknown savepoint |
| `transaction_aborted` | Statement failed, transaction in aborted state |

## See Also

- [Transaction Isolation](02_set_transaction_and_isolation.md)
- [MGA Principles](../../developers_guide/transactions_and_mga/01_mga_principles.md)
- [Lock and Concurrency](03_lock_and_concurrency_controls.md)

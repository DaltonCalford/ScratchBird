<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# INSERT

[Prev](./06_select_set_operations_and_subqueries.md) | [Next](./08_update_syntax.md) | [Topic README](./README.md) | [DML README](./README.md) | [Syntax Guide README](../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

## Synopsis

INSERT adds new rows to a table.

## Syntax

```sql
INSERT INTO table_name [ AS alias ] [ ( column_name [, ...] ) ]
    { DEFAULT VALUES
    | VALUES ( { expression | DEFAULT } [, ...] ) [, ...]
    | query }
    [ ON CONFLICT [ conflict_target ] conflict_action ]
    [ RETURNING { * | output_expression [ [ AS ] output_name ] } [, ...] ]

where conflict_target can be:
    ( { index_column_name | ( index_expression ) } [ COLLATE collation ] [ opclass ] [, ...] )
    ON CONSTRAINT constraint_name

where conflict_action is one of:
    DO NOTHING
    DO UPDATE SET { column_name = { expression | DEFAULT } |
                    ( column_name [, ...] ) = [ ROW ] ( { expression | DEFAULT } [, ...] ) |
                    ( column_name [, ...] ) = ( sub-SELECT )
                  } [, ...]
              [ WHERE condition ]
```

## Basic INSERT

### Single Row

```sql
-- Insert with all columns
INSERT INTO users VALUES (1, 'John', 'john@example.com');

-- Insert with specific columns
INSERT INTO users (name, email) VALUES ('John', 'john@example.com');

-- Insert with DEFAULT
INSERT INTO users (id, name, email, created_at) 
VALUES (1, 'John', 'john@example.com', DEFAULT);

-- Multiple rows
INSERT INTO users (name, email) VALUES 
    ('John', 'john@example.com'),
    ('Jane', 'jane@example.com'),
    ('Bob', 'bob@example.com');
```

### From SELECT

```sql
-- Insert from another table
INSERT INTO active_users (id, name, email)
SELECT id, name, email FROM users WHERE status = 'active';

-- Insert with transformation
INSERT INTO user_stats (user_id, login_count)
SELECT user_id, COUNT(*) FROM logins 
GROUP BY user_id;
```

### DEFAULT VALUES

```sql
-- Insert row with all defaults
INSERT INTO users DEFAULT VALUES;

-- Insert with some defaults
INSERT INTO users (id) VALUES (DEFAULT);
```

## ON CONFLICT (Upsert)

### DO NOTHING

```sql
-- Ignore duplicate key violations
INSERT INTO users (id, email) VALUES (1, 'john@example.com')
ON CONFLICT (email) DO NOTHING;

-- With RETURNING
INSERT INTO users (id, email) VALUES (1, 'john@example.com')
ON CONFLICT (email) DO NOTHING
RETURNING id;
```

### DO UPDATE

```sql
-- Update on conflict
INSERT INTO users (id, email, name) VALUES (1, 'john@example.com', 'John')
ON CONFLICT (email) DO UPDATE SET name = EXCLUDED.name;

-- Conditional update
INSERT INTO users (id, email, name, updated_at) 
VALUES (1, 'john@example.com', 'John', NOW())
ON CONFLICT (email) DO UPDATE SET 
    name = EXCLUDED.name,
    updated_at = EXCLUDED.updated_at
WHERE users.updated_at < EXCLUDED.updated_at;

-- Multiple columns
INSERT INTO inventory (product_id, quantity) VALUES (1, 100)
ON CONFLICT (product_id) DO UPDATE SET 
    quantity = inventory.quantity + EXCLUDED.quantity;
```

### Conflict Target

```sql
-- On specific column(s)
INSERT INTO users (id, email) VALUES (1, 'test@example.com')
ON CONFLICT (email) DO NOTHING;

-- On constraint name
INSERT INTO users (id, email) VALUES (1, 'test@example.com')
ON CONFLICT ON CONSTRAINT users_email_key DO NOTHING;

-- On expression
INSERT INTO events (event_time, data) VALUES (NOW(), '{}')
ON CONFLICT ((DATE(event_time))) DO NOTHING;
```

## RETURNING Clause

```sql
-- Return inserted values
INSERT INTO users (name, email) VALUES ('John', 'john@example.com')
RETURNING id;

-- Return all columns
INSERT INTO users (name, email) VALUES ('John', 'john@example.com')
RETURNING *;

-- Return computed values
INSERT INTO orders (user_id, amount) VALUES (1, 100)
RETURNING id, created_at, amount * 1.1 AS amount_with_tax;

-- Return after upsert
INSERT INTO users (id, email, name) VALUES (1, 'test@test.com', 'Test')
ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name
RETURNING id, name, xmax = 0 AS inserted;
```

## Complete Examples

### Bulk Insert

```sql
-- Efficient multi-row insert
INSERT INTO logs (level, message, created_at)
VALUES 
    ('INFO', 'Application started', NOW()),
    ('DEBUG', 'Config loaded', NOW()),
    ('INFO', 'Server listening', NOW());
```

### Insert with CTE

```sql
WITH new_user AS (
    INSERT INTO users (name, email) VALUES ('John', 'john@example.com')
    RETURNING id
)
INSERT INTO user_profiles (user_id, bio)
SELECT id, 'New user' FROM new_user;
```

### Insert from JSON

```sql
INSERT INTO users (name, email)
SELECT value->>'name', value->>'email'
FROM jsonb_array_elements('[{"name":"John","email":"john@test.com"}]'::jsonb);
```

## Parser Acceptance Cases

```sql
INSERT INTO t1 VALUES (1, 2, 3);
INSERT INTO t1 (a, b) VALUES (1, 2);
INSERT INTO t1 SELECT * FROM t2;
INSERT INTO t1 VALUES (1) ON CONFLICT DO NOTHING;
INSERT INTO t1 VALUES (1) ON CONFLICT (a) DO UPDATE SET b = 2;
INSERT INTO t1 VALUES (1) RETURNING *;
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `unique_violation` | Duplicate key (no ON CONFLICT) |
| `not_null_violation` | NULL in NOT NULL column |
| `foreign_key_violation` | Referenced row doesn't exist |
| `check_violation` | CHECK constraint failed |

## See Also

- [UPDATE](08_update_syntax.md)
- [DELETE](09_delete_syntax.md)
- [SELECT](01_select_core_syntax.md)
- [CTEs](12_cte_and_recursive_query_syntax.md)

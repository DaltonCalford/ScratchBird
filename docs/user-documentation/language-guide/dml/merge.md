# MERGE (Upsert)

Insert or update rows in a single statement.

[Back to DML Index](index.md) | [Back to Language Guide](../index.md)

---

## Overview

MERGE performs "upsert" operations: INSERT if the row doesn't exist, UPDATE if it does. ScratchBird supports multiple syntaxes.

---

## INSERT ON CONFLICT (PostgreSQL Style)

### DO NOTHING

Skip on conflict:

```sql
INSERT INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com')
ON CONFLICT (id) DO NOTHING;
```

### DO UPDATE

Update on conflict:

```sql
INSERT INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com')
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    email = EXCLUDED.email,
    updated_at = CURRENT_TIMESTAMP;
```

### With WHERE

Conditional update:

```sql
INSERT INTO products (id, name, price, version)
VALUES (1, 'Widget', 29.99, 5)
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    price = EXCLUDED.price,
    version = EXCLUDED.version
WHERE products.version < EXCLUDED.version;
```

### Multiple Conflict Columns

```sql
INSERT INTO subscriptions (user_id, service_id, status)
VALUES (1, 2, 'active')
ON CONFLICT (user_id, service_id) DO UPDATE SET
    status = EXCLUDED.status,
    updated_at = CURRENT_TIMESTAMP;
```

### Conflict on Constraint Name

```sql
INSERT INTO users (id, email)
VALUES (1, 'alice@example.com')
ON CONFLICT ON CONSTRAINT users_email_key DO UPDATE SET
    email = EXCLUDED.email;
```

---

## MERGE Statement (SQL Standard)

```sql
MERGE INTO target_table AS t
USING source_table AS s
ON t.id = s.id
WHEN MATCHED THEN
    UPDATE SET t.name = s.name, t.email = s.email
WHEN NOT MATCHED THEN
    INSERT (id, name, email)
    VALUES (s.id, s.name, s.email);
```

### With Values

```sql
MERGE INTO products AS p
USING (VALUES (1, 'Widget', 29.99)) AS s(id, name, price)
ON p.id = s.id
WHEN MATCHED THEN
    UPDATE SET name = s.name, price = s.price
WHEN NOT MATCHED THEN
    INSERT (id, name, price) VALUES (s.id, s.name, s.price);
```

### Multiple WHEN Clauses

```sql
MERGE INTO inventory AS i
USING shipments AS s
ON i.product_id = s.product_id
WHEN MATCHED AND s.quantity > 0 THEN
    UPDATE SET quantity = i.quantity + s.quantity
WHEN MATCHED AND s.quantity < 0 THEN
    UPDATE SET quantity = GREATEST(i.quantity + s.quantity, 0)
WHEN NOT MATCHED THEN
    INSERT (product_id, quantity)
    VALUES (s.product_id, s.quantity);
```

### WHEN MATCHED DELETE

```sql
MERGE INTO products AS p
USING discontinued AS d
ON p.id = d.product_id
WHEN MATCHED THEN DELETE;
```

---

## REPLACE (MySQL Style)

Delete and re-insert:

```sql
REPLACE INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com');
```

**Note:** REPLACE deletes then inserts, which may affect auto-increment and triggers differently than UPDATE.

---

## Bulk Upsert

### From SELECT

```sql
INSERT INTO summary (user_id, total_orders, total_spent)
SELECT
    user_id,
    COUNT(*) AS total_orders,
    SUM(total) AS total_spent
FROM orders
GROUP BY user_id
ON CONFLICT (user_id) DO UPDATE SET
    total_orders = EXCLUDED.total_orders,
    total_spent = EXCLUDED.total_spent;
```

### From VALUES List

```sql
INSERT INTO products (id, name, price) VALUES
    (1, 'Widget', 29.99),
    (2, 'Gadget', 49.99),
    (3, 'Gizmo', 19.99)
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    price = EXCLUDED.price;
```

---

## With RETURNING

```sql
INSERT INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com')
ON CONFLICT (id) DO UPDATE SET
    name = EXCLUDED.name,
    email = EXCLUDED.email
RETURNING id, name,
    CASE xmax WHEN 0 THEN 'inserted' ELSE 'updated' END AS action;
```

---

## Common Patterns

### Increment Counter

```sql
INSERT INTO counters (name, value)
VALUES ('page_views', 1)
ON CONFLICT (name) DO UPDATE SET
    value = counters.value + 1;
```

### Update Only if Newer

```sql
INSERT INTO documents (id, content, version, updated_at)
VALUES (1, 'New content', 5, CURRENT_TIMESTAMP)
ON CONFLICT (id) DO UPDATE SET
    content = EXCLUDED.content,
    version = EXCLUDED.version,
    updated_at = EXCLUDED.updated_at
WHERE documents.version < EXCLUDED.version;
```

### Sync from External Source

```sql
INSERT INTO products (external_id, name, price, synced_at)
SELECT external_id, name, price, CURRENT_TIMESTAMP
FROM external_products
ON CONFLICT (external_id) DO UPDATE SET
    name = EXCLUDED.name,
    price = EXCLUDED.price,
    synced_at = EXCLUDED.synced_at;
```

### Upsert with Defaults

```sql
INSERT INTO user_settings (user_id, theme, notifications)
VALUES (1, 'dark', TRUE)
ON CONFLICT (user_id) DO UPDATE SET
    theme = COALESCE(EXCLUDED.theme, user_settings.theme),
    notifications = COALESCE(EXCLUDED.notifications, user_settings.notifications);
```

---

## Performance Considerations

1. **Unique index required** for ON CONFLICT
2. **Bulk upserts** more efficient than individual
3. **Use MERGE** for complex conditional logic
4. **Index conflict columns** for performance

---

## Notes

- ON CONFLICT requires a unique constraint or index
- EXCLUDED refers to proposed insert values
- MERGE is SQL standard and more flexible
- REPLACE deletes then inserts (different semantics)

---

## See Also

- [INSERT](insert.md)
- [UPDATE](update.md)
- [CREATE INDEX](../ddl/create-index.md)

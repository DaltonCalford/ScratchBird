# JSON Types

JSON and JSONB data types.

[Back to Data Types Index](index.md) | [Back to Language Guide](../index.md)

---

## JSON vs JSONB

| Feature | JSON | JSONB |
|---------|------|-------|
| Storage | Text | Binary |
| Parse on insert | No | Yes |
| Parse on read | Yes | No |
| Preserves formatting | Yes | No |
| Duplicate keys | Preserved | Last value wins |
| Key order | Preserved | Not guaranteed |
| Indexing | Limited | Full GIN support |
| Operators | Basic | All operators |

**Recommendation:** Use JSONB for most cases.

---

## Creating JSON

```sql
CREATE TABLE documents (
    id SERIAL PRIMARY KEY,
    data JSONB
);

-- Insert JSON
INSERT INTO documents (data) VALUES (
    '{"name": "Alice", "age": 30, "tags": ["admin", "user"]}'
);

-- Build JSON
INSERT INTO documents (data) VALUES (
    JSON_BUILD_OBJECT('name', 'Bob', 'age', 25)
);
```

---

## JSON Operators

### Access Operators

| Operator | Description | Returns |
|----------|-------------|---------|
| `->` | Get element | JSON |
| `->>` | Get element | Text |
| `#>` | Get path | JSON |
| `#>>` | Get path | Text |

```sql
-- Get JSON value
SELECT data->'name' FROM documents;
-- Result: "Alice" (JSON string)

-- Get text value
SELECT data->>'name' FROM documents;
-- Result: Alice (text)

-- Array access (0-indexed)
SELECT data->'tags'->0 FROM documents;
-- Result: "admin"

-- Nested path
SELECT data#>'{address,city}' FROM documents;

-- Path as text
SELECT data#>>'{address,city}' FROM documents;
```

### Containment Operators (JSONB only)

| Operator | Description |
|----------|-------------|
| `@>` | Contains |
| `<@` | Contained by |
| `?` | Key exists |
| `?\|` | Any key exists |
| `?&` | All keys exist |

```sql
-- Contains
SELECT * FROM documents WHERE data @> '{"name": "Alice"}';

-- Key exists
SELECT * FROM documents WHERE data ? 'email';

-- Any key exists
SELECT * FROM documents WHERE data ?| ARRAY['email', 'phone'];

-- All keys exist
SELECT * FROM documents WHERE data ?& ARRAY['name', 'age'];
```

### Modification Operators (JSONB only)

| Operator | Description |
|----------|-------------|
| `\|\|` | Concatenate |
| `-` | Delete key |
| `#-` | Delete path |

```sql
-- Merge objects
SELECT '{"a": 1}'::jsonb || '{"b": 2}'::jsonb;
-- Result: {"a": 1, "b": 2}

-- Delete key
SELECT '{"a": 1, "b": 2}'::jsonb - 'a';
-- Result: {"b": 2}

-- Delete path
SELECT '{"a": {"b": 1}}'::jsonb #- '{a,b}';
-- Result: {"a": {}}
```

---

## JSON Functions

### Creating

```sql
JSON_BUILD_OBJECT('name', 'Alice', 'age', 30)
-- {"name": "Alice", "age": 30}

JSON_BUILD_ARRAY(1, 2, 'three')
-- [1, 2, "three"]

TO_JSON(value)
TO_JSONB(value)

ROW_TO_JSON(record)
```

### Querying

```sql
JSONB_TYPEOF(value)          -- 'object', 'array', 'string', etc.
JSONB_ARRAY_LENGTH(array)    -- Length of array
JSONB_OBJECT_KEYS(object)    -- Set of keys
```

### Expanding

```sql
-- Array to rows
SELECT * FROM JSONB_ARRAY_ELEMENTS('[1, 2, 3]');

-- Object to key-value rows
SELECT * FROM JSONB_EACH('{"a": 1, "b": 2}');

-- Object to key-value text rows
SELECT * FROM JSONB_EACH_TEXT('{"a": 1, "b": 2}');
```

### Modifying

```sql
-- Set value at path
JSONB_SET('{"a": 1}', '{b}', '2')
-- {"a": 1, "b": 2}

-- Insert (array)
JSONB_INSERT('[1, 2]', '{1}', '1.5')
-- [1, 1.5, 2]
```

### Aggregating

```sql
-- Collect to array
SELECT JSONB_AGG(name) FROM users;
-- ["Alice", "Bob"]

-- Collect to object
SELECT JSONB_OBJECT_AGG(id, name) FROM users;
-- {"1": "Alice", "2": "Bob"}
```

---

## Indexing JSONB

### GIN Index

For containment and existence:

```sql
CREATE INDEX idx_data ON documents USING GIN (data);

-- Supports:
WHERE data @> '{"type": "order"}'
WHERE data ? 'email'
WHERE data ?| ARRAY['email', 'phone']
```

### GIN with jsonb_path_ops

Smaller, faster for `@>`:

```sql
CREATE INDEX idx_data_path ON documents USING GIN (data jsonb_path_ops);

-- Supports:
WHERE data @> '{"type": "order"}'
-- Does NOT support ? operator
```

### Expression Index

For specific keys:

```sql
CREATE INDEX idx_type ON documents ((data->>'type'));

-- Supports:
WHERE data->>'type' = 'order'
```

---

## Common Patterns

### Querying Nested Data

```sql
-- Find by nested value
SELECT * FROM documents
WHERE data->'address'->>'city' = 'New York';

-- Find in array
SELECT * FROM documents
WHERE data->'tags' ? 'admin';
```

### Updating JSON

```sql
-- Set single field
UPDATE documents
SET data = JSONB_SET(data, '{status}', '"active"')
WHERE id = 1;

-- Increment counter
UPDATE documents
SET data = JSONB_SET(
    data,
    '{views}',
    ((data->>'views')::int + 1)::text::jsonb
)
WHERE id = 1;

-- Add to array
UPDATE documents
SET data = JSONB_SET(
    data,
    '{tags}',
    data->'tags' || '["new-tag"]'::jsonb
)
WHERE id = 1;
```

### Building JSON Response

```sql
SELECT JSON_BUILD_OBJECT(
    'id', u.id,
    'name', u.name,
    'orders', (
        SELECT JSON_AGG(JSON_BUILD_OBJECT(
            'id', o.id,
            'total', o.total
        ))
        FROM orders o
        WHERE o.user_id = u.id
    )
) AS response
FROM users u;
```

### Searching in Arrays

```sql
-- Element in array
SELECT * FROM products
WHERE EXISTS (
    SELECT 1
    FROM JSONB_ARRAY_ELEMENTS_TEXT(tags) AS tag
    WHERE tag = 'sale'
);

-- Multiple elements
SELECT * FROM products
WHERE tags @> '["sale", "featured"]'::jsonb;
```

---

## JSON Path (PostgreSQL 12+)

```sql
-- Query with path
SELECT * FROM documents
WHERE data @? '$.tags[*] ? (@ == "admin")';

-- Extract with path
SELECT jsonb_path_query(data, '$.tags[*]')
FROM documents;
```

---

## Best Practices

1. **Use JSONB** unless you need exact formatting
2. **Index for queries** - GIN for containment
3. **Denormalize selectively** - extract frequently queried fields
4. **Validate in application** - no schema enforcement
5. **Use for flexible data** - not structured relations

---

## When to Use JSON

**Good for:**
- Flexible/dynamic attributes
- API payloads
- Configuration
- Logs/events
- Unstructured data

**Avoid for:**
- Highly relational data
- Frequently joined fields
- Heavily queried columns

---

## See Also

- [JSON Functions](../functions/json-functions.md)
- [CREATE INDEX](../ddl/create-index.md)

# JSON Functions

JSON data manipulation.

[Back to Functions Index](index.md) | [Back to Language Guide](../index.md)

---

## Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `->` | Get JSON element | `data->'name'` |
| `->>` | Get JSON as text | `data->>'name'` |
| `#>` | Get path | `data#>'{address,city}'` |
| `#>>` | Get path as text | `data#>>'{address,city}'` |
| `@>` | Contains | `data @> '{"active": true}'` |
| `<@` | Contained by | `'{"a": 1}' <@ data` |
| `?` | Key exists | `data ? 'name'` |
| `?\|` | Any key exists | `data ?\| array['a', 'b']` |
| `?&` | All keys exist | `data ?& array['a', 'b']` |
| `\|\|` | Concatenate | `'{"a":1}' \|\| '{"b":2}'` |
| `-` | Delete key | `data - 'name'` |
| `#-` | Delete path | `data #- '{address,city}'` |

---

## Creating JSON

| Function | Description | Example |
|----------|-------------|---------|
| `JSON_BUILD_OBJECT(k1, v1, ...)` | Build object | See below |
| `JSON_BUILD_ARRAY(v1, v2, ...)` | Build array | See below |
| `ROW_TO_JSON(record)` | Row to JSON | See below |
| `TO_JSON(value)` | Convert to JSON | `TO_JSON('hello')` → `'"hello"'` |
| `TO_JSONB(value)` | Convert to JSONB | `TO_JSONB(123)` → `'123'` |

```sql
-- Build object
JSON_BUILD_OBJECT('name', 'Alice', 'age', 30)
→ '{"name": "Alice", "age": 30}'

-- Build array
JSON_BUILD_ARRAY(1, 2, 'three')
→ '[1, 2, "three"]'

-- Row to JSON
SELECT ROW_TO_JSON(users) FROM users;
→ '{"id": 1, "name": "Alice", "email": "alice@example.com"}'
```

---

## Querying JSON

### Access Elements

```sql
-- Object property (JSON result)
SELECT data->'name' FROM documents;
→ '"Alice"'

-- Object property (text result)
SELECT data->>'name' FROM documents;
→ 'Alice'

-- Array element (0-indexed)
SELECT data->0 FROM documents;

-- Nested access
SELECT data->'address'->'city' FROM documents;
SELECT data->>'address'->>'city' FROM documents;  -- Error!
SELECT data->'address'->>'city' FROM documents;   -- Correct
```

### Path Access

```sql
-- Path as array
SELECT data#>'{address,city}' FROM documents;
SELECT data#>>'{address,city}' FROM documents;  -- As text

-- Equivalent to
SELECT data->'address'->'city' FROM documents;
```

---

## Containment

```sql
-- Check if JSON contains
SELECT * FROM documents
WHERE data @> '{"status": "active"}';

-- Check if key exists
SELECT * FROM documents
WHERE data ? 'email';

-- Any of these keys exist
SELECT * FROM documents
WHERE data ?| ARRAY['email', 'phone'];

-- All of these keys exist
SELECT * FROM documents
WHERE data ?& ARRAY['name', 'email'];
```

---

## Modifying JSON

| Function | Description | Example |
|----------|-------------|---------|
| `JSONB_SET(target, path, value)` | Set value at path | See below |
| `JSONB_INSERT(target, path, value)` | Insert value | See below |
| `target \|\| source` | Merge objects | See below |
| `target - key` | Remove key | See below |

```sql
-- Set value
JSONB_SET('{"a": 1}', '{b}', '2')
→ '{"a": 1, "b": 2}'

-- Set nested value
JSONB_SET('{"a": {"b": 1}}', '{a,c}', '2')
→ '{"a": {"b": 1, "c": 2}}'

-- Merge
'{"a": 1}'::jsonb || '{"b": 2}'::jsonb
→ '{"a": 1, "b": 2}'

-- Remove key
'{"a": 1, "b": 2}'::jsonb - 'a'
→ '{"b": 2}'

-- Remove nested key
'{"a": {"b": 1, "c": 2}}'::jsonb #- '{a,b}'
→ '{"a": {"c": 2}}'
```

---

## Array Functions

| Function | Description | Example |
|----------|-------------|---------|
| `JSONB_ARRAY_LENGTH(arr)` | Array length | `JSONB_ARRAY_LENGTH('[1,2,3]')` → `3` |
| `JSONB_ARRAY_ELEMENTS(arr)` | Expand to rows | See below |
| `JSONB_ARRAY_ELEMENTS_TEXT(arr)` | Expand as text | See below |

```sql
-- Expand array to rows
SELECT * FROM JSONB_ARRAY_ELEMENTS('[1, 2, 3]');
→ 1
→ 2
→ 3

-- Expand array from column
SELECT id, elem
FROM documents, JSONB_ARRAY_ELEMENTS(data->'tags') AS elem;
```

---

## Object Functions

| Function | Description | Example |
|----------|-------------|---------|
| `JSONB_OBJECT_KEYS(obj)` | Get keys | See below |
| `JSONB_EACH(obj)` | Key-value pairs | See below |
| `JSONB_EACH_TEXT(obj)` | Key-value as text | See below |
| `JSONB_TYPEOF(val)` | JSON type | See below |

```sql
-- Get keys
SELECT JSONB_OBJECT_KEYS('{"a": 1, "b": 2}');
→ 'a'
→ 'b'

-- Key-value pairs
SELECT * FROM JSONB_EACH('{"a": 1, "b": 2}');
→ ('a', '1')
→ ('b', '2')

-- Type of value
JSONB_TYPEOF('123')      → 'number'
JSONB_TYPEOF('"hello"')  → 'string'
JSONB_TYPEOF('true')     → 'boolean'
JSONB_TYPEOF('[1,2]')    → 'array'
JSONB_TYPEOF('{"a":1}')  → 'object'
JSONB_TYPEOF('null')     → 'null'
```

---

## Aggregation

| Function | Description |
|----------|-------------|
| `JSON_AGG(expr)` | Aggregate to JSON array |
| `JSONB_AGG(expr)` | Aggregate to JSONB array |
| `JSON_OBJECT_AGG(k, v)` | Aggregate to JSON object |
| `JSONB_OBJECT_AGG(k, v)` | Aggregate to JSONB object |

```sql
-- Aggregate rows to array
SELECT JSON_AGG(name) FROM users;
→ '["Alice", "Bob", "Carol"]'

-- Aggregate to object
SELECT JSON_OBJECT_AGG(id, name) FROM users;
→ '{"1": "Alice", "2": "Bob", "3": "Carol"}'

-- Aggregate with ORDER BY
SELECT JSON_AGG(name ORDER BY name) FROM users;
```

---

## Examples

### Query Nested JSON

```sql
-- Find documents with specific nested value
SELECT * FROM documents
WHERE data->'address'->>'country' = 'USA';

-- Find in array
SELECT * FROM documents
WHERE data->'tags' ? 'important';
```

### Update JSON Field

```sql
UPDATE documents
SET data = JSONB_SET(data, '{status}', '"active"')
WHERE id = 1;

-- Increment counter in JSON
UPDATE documents
SET data = JSONB_SET(data, '{views}', ((data->>'views')::int + 1)::text::jsonb)
WHERE id = 1;
```

### Build JSON Response

```sql
SELECT JSON_BUILD_OBJECT(
    'user', JSON_BUILD_OBJECT(
        'id', u.id,
        'name', u.name
    ),
    'orders', (
        SELECT JSON_AGG(JSON_BUILD_OBJECT(
            'id', o.id,
            'total', o.total
        ))
        FROM orders o WHERE o.user_id = u.id
    )
) AS response
FROM users u;
```

### Search in JSON Array

```sql
SELECT *
FROM products
WHERE EXISTS (
    SELECT 1
    FROM JSONB_ARRAY_ELEMENTS_TEXT(tags) AS tag
    WHERE tag ILIKE '%sale%'
);
```

---

## Indexing JSON

```sql
-- GIN index for containment queries
CREATE INDEX idx_data ON documents USING GIN (data);

-- GIN index for specific operators
CREATE INDEX idx_data_ops ON documents USING GIN (data jsonb_path_ops);

-- Index specific path
CREATE INDEX idx_status ON documents ((data->>'status'));
```

---

## See Also

- [Data Types - JSON](../data-types/json-types.md)
- [CREATE INDEX](../ddl/create-index.md)

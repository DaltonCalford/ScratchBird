# JSON Functions

[Categories README](./README.md) | [Operations Guide README](../../README.md)

## Synopsis

Functions for working with JSON and JSONB data types.

## JSON Creation

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `to_json(val)` | Convert to JSON | `to_json('hello')` | `"hello"` |
| `to_jsonb(val)` | Convert to JSONB | `to_jsonb(123)` | `123` |
| `json_build_object(...)` | Build JSON object | `json_build_object('a', 1)` | `{"a": 1}` |
| `jsonb_build_object(...)` | Build JSONB object | Same | `{"a": 1}` |
| `json_build_array(...)` | Build JSON array | `json_build_array(1, 2)` | `[1, 2]` |
| `jsonb_build_array(...)` | Build JSONB array | Same | `[1, 2]` |
| `json_object(keys, vals)` | Build from arrays | `json_object('{a}', '{1}')` | `{"a": 1}` |
| `jsonb_object(keys, vals)` | Build JSONB from arrays | Same | `{"a": 1}` |

## JSON Aggregation

| Function | Description |
|----------|-------------|
| `json_agg(expr)` | Aggregate values to JSON array |
| `jsonb_agg(expr)` | Aggregate to JSONB array |
| `json_object_agg(key, val)` | Aggregate to JSON object |
| `jsonb_object_agg(key, val)` | Aggregate to JSONB object |

## JSON Extraction

### By Key/Path

| Function | Description | Example |
|----------|-------------|---------|
| `json -> text` | Get JSON field | `'{"a": 1}'::json -> 'a'` → `1` |
| `json ->> text` | Get as text | `'{"a": 1}'::json ->> 'a'` → `'1'` |
| `json #> path` | Get by path | `'{"a": {"b": 1}}'::json #> '{a,b}'` |
| `json #>> path` | Get path as text | Same |

### JSONB Specific

| Function | Description | Example |
|----------|-------------|---------|
| `jsonb @> jsonb` | Contains | `'{"a": 1}'::jsonb @> '{"a": 1}'` |
| `jsonb <@ jsonb` | Contained by | - |
| `jsonb ? text` | Has key | `'{"a": 1}'::jsonb ? 'a'` |
| `jsonb ?\| text[]` | Has any key | - |
| `jsonb ?& text[]` | Has all keys | - |
| `jsonb || jsonb` | Concatenate | - |
| `jsonb - text` | Delete key | `'{"a": 1}'::jsonb - 'a'` |
| `jsonb #- path` | Delete at path | - |

## JSON Processing

| Function | Description | Example |
|----------|-------------|---------|
| `json_each(json)` | Expand to key-value rows | - |
| `jsonb_each(jsonb)` | Expand JSONB to rows | - |
| `json_each_text(json)` | Expand to text | - |
| `jsonb_each_text(jsonb)` | Expand JSONB to text | - |
| `json_array_elements(json)` | Expand array to rows | - |
| `jsonb_array_elements(jsonb)` | Expand JSONB array | - |
| `json_populate_record(base, json)` | JSON to record | - |
| `jsonb_populate_record(base, jsonb)` | JSONB to record | - |
| `json_populate_recordset(base, json)` | JSON array to records | - |
| `jsonb_populate_recordset(base, jsonb)` | JSONB array to records | - |

## Examples

```sql
-- Create JSON object
SELECT json_build_object('name', 'John', 'age', 30);
-- Result: {"name": "John", "age": 30}

-- Extract field
SELECT data->>'name' FROM users WHERE data->>'type' = 'admin';

-- Check if contains
SELECT * FROM logs WHERE data @> '{"level": "error"}';

-- Aggregate to JSON array
SELECT json_agg(name) FROM products;

-- Expand JSON array
SELECT * FROM jsonb_array_elements('[1, 2, 3]'::jsonb);

-- Update JSONB
UPDATE users SET data = data || '{"last_login": "2024-01-01"}'::jsonb;

-- Delete key
UPDATE users SET data = data - 'temp_field';

-- Query nested data
SELECT data#>'{address, city}' FROM users;

-- Create from row
SELECT to_jsonb(u.*) FROM users u;
```

## Indexing JSONB

```sql
-- GIN index for containment queries
CREATE INDEX idx_logs_data ON logs USING GIN (data);

-- Specific key index
CREATE INDEX idx_users_name ON users ((data->>'name'));
```

## See Also

- [JSON Types](../../type_system/07_json_document_and_semistructured_types.md)
- [GIN Index](../../../syntax_guide/ddl/table_and_constraints/04_create_index.md)

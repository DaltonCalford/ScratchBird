# UUID Functions

[Categories README](./README.md)

## Synopsis

Functions for generating and manipulating UUIDs (Universally Unique Identifiers).

## Generation Functions

| Function | Description | Example |
|----------|-------------|---------|
| `gen_random_uuid()` | Generate random UUID v4 | `gen_random_uuid()` → `'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'` |
| `uuid_generate_v1()` | Generate UUID v1 (timestamp + MAC) | - |
| `uuid_generate_v4()` | Generate UUID v4 (random) | Same as gen_random_uuid() |
| `uuid_generate_v7()` | Generate UUID v7 (timestamp-based) | Recommended for SB |
| `uuid_nil()` | Return nil UUID | `'00000000-0000-0000-0000-000000000000'` |

## Conversion Functions

| Function | Description | Example |
|----------|-------------|---------|
| `uuid_to_text(uuid)` | UUID to text | `uuid_to_text(uuid)` |
| `text_to_uuid(text)` | Text to UUID | `text_to_uuid('a0eebc...')` |

## Validation Functions

| Function | Description | Example |
|----------|-------------|---------|
| `is_uuid(text)` | Check if valid UUID format | `is_uuid('a0eebc99...')` → `true` |

## Examples

```sql
-- Generate UUID as default
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name TEXT
);

-- Insert with generated UUID
INSERT INTO users (name) VALUES ('John');

-- Generate multiple UUIDs
SELECT gen_random_uuid() FROM generate_series(1, 10);

-- Check if valid UUID
SELECT is_uuid('not-a-uuid');  -- false
SELECT is_uuid('a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11');  -- true

-- UUID in query
SELECT * FROM users WHERE id = 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'::UUID;

-- UUID ordering (time-based)
-- UUID v7 includes timestamp, sortable by time
SELECT * FROM events ORDER BY id;  -- Chronological order with v7
```

## UUID Version 7 (Recommended)

ScratchBird uses UUID v7 by default:
- First 48 bits: Unix timestamp (milliseconds)
- Next bits: Version (7) and random
- Last bits: Random + variant

Benefits:
- Time-ordered (database-friendly indexing)
- No MAC address exposure
- No coordination required
- Monotonic sort order

## See Also

- [UUID Identity System](../../../developers_guide/architecture/09_group_and_cluster_trust_models.md)

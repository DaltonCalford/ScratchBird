# Special Types

UUID, boolean, arrays, binary, and other special types.

[Back to Data Types Index](index.md) | [Back to Language Guide](../index.md)

---

## BOOLEAN

True/false values:

| Value | Represents TRUE | Represents FALSE |
|-------|-----------------|------------------|
| Literal | `TRUE`, `'t'`, `'true'`, `'yes'`, `'on'`, `'1'` | `FALSE`, `'f'`, `'false'`, `'no'`, `'off'`, `'0'` |

```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    active BOOLEAN DEFAULT TRUE,
    verified BOOLEAN NOT NULL
);

INSERT INTO users (active, verified) VALUES (TRUE, FALSE);
INSERT INTO users (active, verified) VALUES ('yes', 'no');

SELECT * FROM users WHERE active;
SELECT * FROM users WHERE NOT verified;
```

---

## UUID

128-bit universally unique identifier:

```sql
CREATE TABLE sessions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id INTEGER,
    created_at TIMESTAMP
);

-- Generate UUID
SELECT gen_random_uuid();
-- Result: a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11

-- Insert with UUID
INSERT INTO sessions (id, user_id)
VALUES ('a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11', 1);

-- Auto-generate
INSERT INTO sessions (user_id) VALUES (1);
```

### When to Use UUID

**Advantages:**
- Globally unique without coordination
- No sequential IDs to guess
- Good for distributed systems

**Disadvantages:**
- Larger than INTEGER (16 bytes vs 4)
- Slower index performance
- Not human-readable

---

## Arrays

Arrays of any type:

```sql
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    tags TEXT[],
    prices DECIMAL(10,2)[]
);

-- Insert arrays
INSERT INTO products (name, tags, prices) VALUES (
    'Widget',
    ARRAY['sale', 'featured'],
    ARRAY[29.99, 24.99, 19.99]
);

INSERT INTO products (name, tags, prices) VALUES (
    'Gadget',
    '{"new", "popular"}',  -- Alternative syntax
    '{39.99, 34.99}'
);
```

### Array Operations

```sql
-- Access element (1-indexed)
SELECT tags[1] FROM products;

-- Slice
SELECT prices[1:2] FROM products;

-- Length
SELECT ARRAY_LENGTH(tags, 1) FROM products;

-- Contains
SELECT * FROM products WHERE 'sale' = ANY(tags);
SELECT * FROM products WHERE tags @> ARRAY['sale'];

-- Overlap
SELECT * FROM products WHERE tags && ARRAY['sale', 'new'];
```

### Array Functions

```sql
ARRAY_AGG(column)                    -- Aggregate to array
ARRAY_APPEND(array, element)         -- Add element
ARRAY_PREPEND(element, array)        -- Add at start
ARRAY_CAT(array1, array2)            -- Concatenate
ARRAY_REMOVE(array, element)         -- Remove element
ARRAY_POSITION(array, element)       -- Find position
UNNEST(array)                        -- Expand to rows
```

---

## BYTEA

Binary data:

```sql
CREATE TABLE files (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255),
    content BYTEA
);

-- Insert binary (hex format)
INSERT INTO files (name, content) VALUES ('test', E'\\xDEADBEEF');

-- Insert binary (escape format)
INSERT INTO files (name, content) VALUES ('test2', E'\\000\\001\\002');
```

### Binary Functions

```sql
LENGTH(bytea)                    -- Byte length
OCTET_LENGTH(bytea)              -- Same
ENCODE(bytea, 'hex')             -- To hex string
ENCODE(bytea, 'base64')          -- To base64
DECODE(text, 'hex')              -- From hex
DECODE(text, 'base64')           -- From base64
```

---

## Enum Types

Custom enumerated types:

```sql
CREATE TYPE status AS ENUM ('pending', 'active', 'suspended', 'deleted');

CREATE TABLE accounts (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    status status DEFAULT 'pending'
);

INSERT INTO accounts (name, status) VALUES ('Test', 'active');

-- Comparison uses enum order
SELECT * FROM accounts WHERE status > 'pending';
```

### Enum Operations

```sql
-- Add value
ALTER TYPE status ADD VALUE 'archived' AFTER 'deleted';

-- List values
SELECT enum_range(NULL::status);
```

---

## Composite Types

Custom row types:

```sql
CREATE TYPE address AS (
    street VARCHAR(100),
    city VARCHAR(50),
    country VARCHAR(50)
);

CREATE TABLE customers (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    shipping_address address,
    billing_address address
);

INSERT INTO customers (name, shipping_address) VALUES (
    'Alice',
    ROW('123 Main St', 'New York', 'USA')
);

-- Access fields
SELECT (shipping_address).city FROM customers;
```

---

## Range Types

Continuous ranges:

| Type | Description |
|------|-------------|
| `INT4RANGE` | Integer range |
| `INT8RANGE` | Bigint range |
| `NUMRANGE` | Numeric range |
| `DATERANGE` | Date range |
| `TSRANGE` | Timestamp range |
| `TSTZRANGE` | Timestamp with TZ range |

```sql
CREATE TABLE events (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    duration TSRANGE
);

INSERT INTO events (name, duration) VALUES (
    'Conference',
    '[2024-01-15 09:00, 2024-01-15 17:00)'
);

-- Range operators
SELECT * FROM events WHERE duration @> NOW();  -- Contains
SELECT * FROM events WHERE duration && '[2024-01-15, 2024-01-16)'; -- Overlaps
```

### Range Syntax

```
[a,b]  -- Inclusive both ends
(a,b)  -- Exclusive both ends
[a,b)  -- Inclusive start, exclusive end
(a,b]  -- Exclusive start, inclusive end
```

---

## Network Types

| Type | Description |
|------|-------------|
| `INET` | IP address (v4 or v6) |
| `CIDR` | Network address |
| `MACADDR` | MAC address |

```sql
CREATE TABLE hosts (
    id SERIAL PRIMARY KEY,
    ip INET,
    network CIDR,
    mac MACADDR
);

INSERT INTO hosts VALUES (1, '192.168.1.100', '192.168.1.0/24', '08:00:2b:01:02:03');

-- Network operations
SELECT * FROM hosts WHERE ip << '192.168.0.0/16';  -- Contained in
SELECT * FROM hosts WHERE network >>= '192.168.1.50';  -- Contains
```

---

## Geometric Types

| Type | Description |
|------|-------------|
| `POINT` | (x, y) |
| `LINE` | Infinite line |
| `LSEG` | Line segment |
| `BOX` | Rectangle |
| `PATH` | Sequence of points |
| `POLYGON` | Closed polygon |
| `CIRCLE` | Circle |

```sql
CREATE TABLE locations (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    position POINT
);

INSERT INTO locations VALUES (1, 'Office', '(40.7128, -74.0060)');

-- Distance
SELECT name, position <-> '(40.7580, -73.9855)' AS distance
FROM locations;
```

---

## NULL

Special value for missing/unknown:

```sql
-- Check for NULL
SELECT * FROM users WHERE email IS NULL;
SELECT * FROM users WHERE email IS NOT NULL;

-- COALESCE for defaults
SELECT COALESCE(nickname, name) FROM users;

-- NULLIF (returns NULL if equal)
SELECT NULLIF(value, 0);  -- NULL if 0

-- NULL in expressions
SELECT 1 + NULL;  -- NULL
SELECT 'a' || NULL;  -- NULL
```

---

## XML

XML data type:

```sql
CREATE TABLE documents (
    id SERIAL PRIMARY KEY,
    content XML
);

INSERT INTO documents (content) VALUES (
    '<root><item>Value</item></root>'
);

-- XPath query
SELECT xpath('/root/item/text()', content) FROM documents;
```

---

## See Also

- [CREATE TABLE](../ddl/create-table.md)
- [Numeric Types](numeric-types.md)

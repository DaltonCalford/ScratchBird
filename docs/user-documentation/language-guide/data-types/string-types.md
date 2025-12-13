# String Types

Character and text types.

[Back to Data Types Index](index.md) | [Back to Language Guide](../index.md)

---

## Character Types

| Type | Description |
|------|-------------|
| `CHAR(n)` / `CHARACTER(n)` | Fixed-length, padded |
| `VARCHAR(n)` / `CHARACTER VARYING(n)` | Variable-length, max n |
| `TEXT` | Unlimited length |

### CHAR(n)

Fixed-length, space-padded:

```sql
CREATE TABLE codes (
    country_code CHAR(2),  -- Always 2 characters
    state_code CHAR(3)
);

INSERT INTO codes VALUES ('US', 'CA');
-- 'US' stored as 'US' (trailing spaces for comparison)
```

### VARCHAR(n)

Variable-length up to n characters:

```sql
CREATE TABLE users (
    name VARCHAR(100),
    email VARCHAR(255)
);

-- Stores only actual characters used
INSERT INTO users VALUES ('Alice', 'alice@example.com');
```

### TEXT

Unlimited length:

```sql
CREATE TABLE posts (
    title VARCHAR(200),
    content TEXT
);

INSERT INTO posts VALUES ('Hello', 'Very long content...');
```

---

## Literals

```sql
-- Single quotes
'Hello, World'

-- Escaping quotes
'It''s a test'

-- Dollar quoting (for complex strings)
$$String with 'quotes' and special characters$$
$tag$Another way$tag$

-- Unicode
U&'d\0061t\0061'  -- 'data'
```

---

## String Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `\|\|` | Concatenate | `'Hello' \|\| ' World'` → `'Hello World'` |
| `LIKE` | Pattern match | `name LIKE 'A%'` |
| `ILIKE` | Case-insensitive | `name ILIKE 'alice'` |
| `~` | Regex match | `email ~ '@.*\.com$'` |
| `~*` | Regex case-insensitive | `name ~* '^[a-z]'` |

---

## Pattern Matching

### LIKE

| Pattern | Matches |
|---------|---------|
| `%` | Any sequence |
| `_` | Any single character |

```sql
WHERE name LIKE 'A%'        -- Starts with A
WHERE name LIKE '%son'      -- Ends with son
WHERE name LIKE '%john%'    -- Contains john
WHERE code LIKE 'A_B'       -- A + any char + B
WHERE name LIKE '%\%%' ESCAPE '\' -- Contains literal %
```

### ILIKE (Case-Insensitive)

```sql
WHERE name ILIKE 'alice'    -- Matches Alice, ALICE, aLiCe
```

### Regular Expressions

```sql
WHERE email ~ '^[a-z]+@'           -- Starts with letters + @
WHERE phone ~ '^\d{3}-\d{3}-\d{4}$' -- 123-456-7890 format
```

---

## Common Functions

### Length

```sql
LENGTH('hello')          -- 5
CHAR_LENGTH('hello')     -- 5
OCTET_LENGTH('hello')    -- 5 (bytes)
```

### Case

```sql
UPPER('hello')           -- 'HELLO'
LOWER('HELLO')           -- 'hello'
INITCAP('hello world')   -- 'Hello World'
```

### Trim

```sql
TRIM('  hello  ')        -- 'hello'
LTRIM('  hello')         -- 'hello'
RTRIM('hello  ')         -- 'hello'
TRIM(BOTH 'x' FROM 'xxhelloxx')  -- 'hello'
```

### Substring

```sql
SUBSTRING('hello', 2, 3)  -- 'ell'
LEFT('hello', 3)          -- 'hel'
RIGHT('hello', 3)         -- 'llo'
```

### Concatenate

```sql
CONCAT('a', 'b', 'c')            -- 'abc'
CONCAT_WS('-', 'a', 'b', 'c')    -- 'a-b-c'
'Hello' || ' ' || 'World'        -- 'Hello World'
```

### Replace

```sql
REPLACE('hello', 'l', 'L')       -- 'heLLo'
TRANSLATE('hello', 'el', 'ip')   -- 'hippo'
```

### Position

```sql
POSITION('l' IN 'hello')         -- 3
STRPOS('hello', 'l')             -- 3
```

### Split

```sql
SPLIT_PART('a,b,c', ',', 2)      -- 'b'
STRING_TO_ARRAY('a,b,c', ',')    -- {a,b,c}
```

---

## Collation

Sort order and comparison rules:

```sql
-- Default collation
SELECT name FROM users ORDER BY name;

-- Specific collation
SELECT name FROM users ORDER BY name COLLATE "en_US";

-- Case-insensitive collation
CREATE TABLE words (
    word VARCHAR(100) COLLATE "en_US.utf8"
);
```

---

## Character Sets

Default is UTF-8. Specify in database creation:

```sql
CREATE DATABASE mydb ENCODING 'UTF8';
```

Convert encoding:

```sql
CONVERT_TO('hello', 'UTF8')
CONVERT_FROM(bytes, 'UTF8')
```

---

## NULL vs Empty String

```sql
-- NULL = unknown/missing
SELECT * FROM users WHERE name IS NULL;

-- Empty string = known, empty value
SELECT * FROM users WHERE name = '';

-- Both
SELECT * FROM users WHERE name IS NULL OR name = '';

-- COALESCE for defaults
SELECT COALESCE(name, 'Unknown') FROM users;
```

---

## Best Practices

1. **Use VARCHAR for most text** - More efficient than CHAR
2. **Use TEXT for unlimited** - No performance penalty vs VARCHAR
3. **Validate at application** - Don't rely on length limits alone
4. **Use ILIKE cautiously** - Can be slow without proper indexes
5. **Index for search** - Use GIN/trigram for text search

---

## Indexing Strings

```sql
-- B-tree for exact and prefix
CREATE INDEX idx_name ON users(name);
-- Supports: name = 'Alice', name LIKE 'Ali%'

-- GIN for full-text
CREATE INDEX idx_content ON posts USING GIN (to_tsvector('english', content));
-- Supports full-text search

-- Trigram for LIKE anywhere
CREATE EXTENSION pg_trgm;
CREATE INDEX idx_name_trgm ON users USING GIN (name gin_trgm_ops);
-- Supports: name LIKE '%ali%'
```

---

## See Also

- [String Functions](../functions/string-functions.md)
- [CREATE INDEX](../ddl/create-index.md)

# Comparison Operators

[Operators README](../README.md)

## Synopsis

Comparison operators test the relationship between values.

## Basic Comparison

| Operator | Description | Example | Result |
|----------|-------------|---------|--------|
| `=` | Equal | `5 = 5` | `true` |
| `<>` or `!=` | Not equal | `5 <> 3` | `true` |
| `<` | Less than | `3 < 5` | `true` |
| `>` | Greater than | `5 > 3` | `true` |
| `<=` | Less than or equal | `5 <= 5` | `true` |
| `>=` | Greater than or equal | `5 >= 3` | `true` |

## Range Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `BETWEEN` | Within range | `value BETWEEN 10 AND 20` |
| `NOT BETWEEN` | Outside range | `value NOT BETWEEN 10 AND 20` |
| `BETWEEN SYMMETRIC` | Range in any order | `value BETWEEN SYMMETRIC 20 AND 10` |

## Null Comparison

| Operator | Description | Example | Result |
|----------|-------------|---------|--------|
| `IS NULL` | Is null | `NULL IS NULL` | `true` |
| `IS NOT NULL` | Is not null | `'text' IS NOT NULL` | `true` |
| `IS DISTINCT FROM` | Different (null safe) | `NULL IS DISTINCT FROM 1` | `true` |
| `IS NOT DISTINCT FROM` | Equal (null safe) | `NULL IS NOT DISTINCT FROM NULL` | `true` |

## Pattern Matching

| Operator | Description | Example |
|----------|-------------|---------|
| `LIKE` | SQL pattern match | `name LIKE 'John%'` |
| `NOT LIKE` | Negated LIKE | `name NOT LIKE '%test%'` |
| `ILIKE` | Case-insensitive LIKE | `name ILIKE 'john%'` |
| `~` | Regex match | `name ~ '^[A-Z]'` |
| `~*` | Case-insensitive regex | `name ~* '^john'` |
| `!~` | Not regex match | `name !~ '^[0-9]'` |
| `SIMILAR TO` | SQL regex | `name SIMILAR TO '(John\|Jane)%'` |

## Array Comparison

| Operator | Description | Example |
|----------|-------------|---------|
| `@>` | Contains | `ARRAY[1,2,3] @> ARRAY[1,2]` |
| `<@` | Contained by | `ARRAY[1] <@ ARRAY[1,2,3]` |
| `&&` | Overlap | `ARRAY[1,2] && ARRAY[2,3]` |
| `=` | Array equality | `ARRAY[1,2] = ARRAY[1,2]` |

## JSON Comparison

| Operator | Description | Example |
|----------|-------------|---------|
| `@>` | JSON contains | `'{"a":1}'::jsonb @> '{"a":1}'::jsonb` |
| `<@` | JSON contained by | - |
| `?` | Has key | `'{"a":1}'::jsonb ? 'a'` |
| `?\|` | Has any key | `'{"a":1}'::jsonb ?\| array['a','b']` |
| `?&` | Has all keys | `'{"a":1,"b":2}'::jsonb ?& array['a','b']` |

## Row Comparison

```sql
-- Compare entire rows
SELECT * FROM users WHERE (name, age) = ('John', 30);

-- Range comparison
SELECT * FROM users WHERE (age, salary) > (25, 50000);
```

## Examples

```sql
-- Basic comparisons
SELECT * FROM products WHERE price > 100;
SELECT * FROM users WHERE age BETWEEN 18 AND 65;

-- Null handling
SELECT * FROM users WHERE phone IS NULL;
SELECT * FROM users WHERE deleted_at IS DISTINCT FROM NULL;

-- Pattern matching
SELECT * FROM users WHERE email LIKE '%@company.com';
SELECT * FROM users WHERE name ~* '^[aeiou]';

-- Array operations
SELECT * FROM articles WHERE tags @> ARRAY['database'];
SELECT * FROM products WHERE categories && ARRAY['electronics', 'computers'];

-- JSON operations
SELECT * FROM logs WHERE data @> '{"level": "error"}'::jsonb;
SELECT * FROM config WHERE settings ? 'feature_flag';
```

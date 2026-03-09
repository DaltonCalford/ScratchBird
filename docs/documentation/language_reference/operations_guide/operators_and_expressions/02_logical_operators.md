# Logical Operators

[Operators README](../README.md)

## Synopsis

Logical operators combine boolean values.

## Operators

| Operator | Description | Example | Result |
|----------|-------------|---------|--------|
| `AND` | Logical AND | `true AND false` | `false` |
| `OR` | Logical OR | `true OR false` | `true` |
| `NOT` | Logical NOT | `NOT true` | `false` |

## Truth Table

| a | b | a AND b | a OR b | NOT a |
|---|---|---------|--------|-------|
| TRUE | TRUE | TRUE | TRUE | FALSE |
| TRUE | FALSE | FALSE | TRUE | FALSE |
| FALSE | TRUE | FALSE | TRUE | TRUE |
| FALSE | FALSE | FALSE | FALSE | TRUE |
| NULL | TRUE | NULL | TRUE | NULL |
| NULL | FALSE | FALSE | NULL | NULL |
| NULL | NULL | NULL | NULL | NULL |

## Short-Circuit Evaluation

```sql
-- AND: stops at first false
condition1 AND condition2 AND condition3

-- OR: stops at first true
condition1 OR condition2 OR condition3
```

## Operator Precedence

1. `NOT` (highest)
2. `AND`
3. `OR` (lowest)

Use parentheses to override:

```sql
-- Without parentheses
WHERE a OR b AND c  -- a OR (b AND c)

-- With parentheses
WHERE (a OR b) AND c
```

## Examples

```sql
-- AND
SELECT * FROM users WHERE age >= 18 AND status = 'active';

-- OR
SELECT * FROM users WHERE status = 'admin' OR status = 'moderator';

-- NOT
SELECT * FROM users WHERE NOT deleted;

-- Combined
SELECT * FROM users 
WHERE (status = 'active' OR status = 'pending') 
  AND NOT banned;

-- With NULL
SELECT * FROM users 
WHERE (verified = true OR verified IS NULL) 
  AND active = true;
```

## NULL Handling

```sql
-- NULL in conditions
SELECT NULL AND true;   -- NULL
SELECT NULL AND false;  -- FALSE
SELECT NULL OR true;    -- TRUE
SELECT NULL OR false;   -- NULL
SELECT NOT NULL;        -- NULL

-- Use IS NULL / IS NOT NULL
SELECT * FROM users 
WHERE verified = true 
   OR verified IS NULL;  -- Include unknown
```

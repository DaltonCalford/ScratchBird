# Mathematical Operators

[Operators README](../README.md)

## Synopsis

Arithmetic operators perform mathematical operations.

## Basic Arithmetic

| Operator | Description | Example | Result |
|----------|-------------|---------|--------|
| `+` | Addition | `5 + 3` | `8` |
| `-` | Subtraction | `5 - 3` | `2` |
| `*` | Multiplication | `5 * 3` | `15` |
| `/` | Division | `5 / 2` | `2.5` |
| `%` | Modulo | `5 % 2` | `1` |
| `^` | Exponentiation | `2 ^ 3` | `8` |
| `\|/` | Square root | `\|/ 16` | `4` |
| `\|\|/` | Cube root | `\|\|/ 27` | `3` |
| `!` | Factorial | `5 !` | `120` |
| `!!` | Factorial (prefix) | `!! 5` | `120` |
| `@` | Absolute value | `@ -5` | `5` |

## Bitwise Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `&` | Bitwise AND | `5 & 3` → `1` |
| `\|` | Bitwise OR | `5 \| 3` → `7` |
| `#` | Bitwise XOR | `5 # 3` → `6` |
| `~` | Bitwise NOT | `~ 5` → `-6` |
| `<<` | Left shift | `1 << 3` → `8` |
| `>>` | Right shift | `8 >> 2` → `2` |

## String Concatenation

| Operator | Description | Example | Result |
|----------|-------------|---------|--------|
| `\|\|` | Concatenation | `'Hello' \|\| ' ' \|\| 'World'` | `'Hello World'` |

## Examples

```sql
-- Basic arithmetic
SELECT 10 + 5, 10 - 5, 10 * 5, 10 / 5;

-- Division (integer vs numeric)
SELECT 10 / 3;      -- 3 (integer)
SELECT 10.0 / 3;    -- 3.333... (numeric)

-- Modulo
SELECT * FROM events WHERE id % 10 = 0;  -- Every 10th

-- Exponentiation
SELECT 2 ^ 10;  -- 1024

-- Square root
SELECT \|/ area as side FROM squares;

-- Factorial
SELECT 5 !;  -- 120

-- Absolute value
SELECT @ -10;  -- 10

-- Bitwise
SELECT * FROM flags WHERE permissions & 4 = 4;  -- Check bit 2

-- String concatenation
SELECT first_name \|\| ' ' \|\| last_name AS full_name FROM users;
```

## Operator Precedence

1. `^` (right associative)
2. `*`, `/`, `%`
3. `+`, `-`
4. `||`
5. Comparison operators
6. `NOT`
7. `AND`
8. `OR` (lowest)

```sql
-- Use parentheses for clarity
SELECT (2 + 3) * 4;  -- 20
SELECT 2 + 3 * 4;    -- 14 (multiplication first)
```

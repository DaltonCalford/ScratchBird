[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Native (V2) - Operators

Source: `ScratchBird/docs/audit/29_operator_inventory_actual.md`.

Legend: Implemented, Partial, Missing.

## Examples (actual)

```sql
-- Arithmetic
SELECT 1 + 2 * 3;
SELECT -price FROM items;

-- Pattern match and regex
SELECT name LIKE 'A%';
SELECT name ILIKE 'a%';
SELECT code SIMILAR TO '[A-Z]{3}-[0-9]+';
SELECT 'abc' ~ 'a.*';

-- JSON
SELECT doc->'status', doc->>'status' FROM logs;
SELECT doc#>'{a,0,b}', doc#>>'{a,0,b}' FROM logs;

-- Array literal (operators not available)
SELECT ARRAY['a','b','c'];
```

## Known quirks (actual)

- `||` parses but is encoded as numeric add (fails for string concat).
- `IS DISTINCT FROM` is not null-safe (uses EQ/NE).
- unary `NOT` is not 3VL-safe (encoded as `= 0`).
- Bitwise and array operators are tokenized but not parsed.
- `::` cast is not parsed; use `CAST(...)`.

## Arithmetic
- `+`, `-`, `*`, `/`, `%`: Implemented (numeric).
- unary `-`: Implemented.
- unary `+`: Missing.
- `^` power: Missing (use POWER()).

## String and pattern
- `||` concat: Partial (parsed but encoded as numeric add).
- `LIKE`, `ILIKE`, `SIMILAR TO`: Implemented.
- regex `~`, `~*`, `!~`, `!~*`: Implemented.

## Comparison and boolean
- `=`, `<>`, `<`, `<=`, `>`, `>=`: Implemented.
- `IS NULL` / `IS NOT NULL`: Implemented.
- `IS TRUE` / `IS FALSE`: Implemented.
- `IS DISTINCT FROM` / `IS NOT DISTINCT FROM`: Partial (not null-safe).
- `AND` / `OR`: Implemented.
- unary `NOT`: Partial (not 3VL-safe).

## Predicates
- `IN` / `NOT IN`: Implemented.
- `BETWEEN` / `NOT BETWEEN`: Partial (NULL handling fragile).

## Element selectors (EXTRACT / ALTER_ELEMENT)

### Description
EXTRACT pulls a specific field from a value (time, UUID, arrays, JSON-like
variants). ALTER_ELEMENT replaces a selected element in a composite value.

### Syntax (actual)
```sql
EXTRACT(<field> FROM <expr>)
ALTER_ELEMENT(<selector> IN <expr> TO <new_value>)
```

### Examples
```sql
SELECT EXTRACT(YEAR FROM order_date);
SELECT ALTER_ELEMENT(2 IN tags TO 'vip');
```

### Notes
- Field names are resolved via `extract_element_catalog` (case-insensitive).
- Common fields include YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, EPOCH.
- Selector can be an index or key depending on value type.
- Spec reference: `ScratchBird/docs/specifications/ddl/EXTRACT_AND_ALTER_ELEMENT.md`.

### Status
Implemented.

## Context variables and special date/time literals

### Description
V2 exposes a limited set of date/time context values as zero-argument functions.

### Syntax (actual)
```sql
NOW()
CURRENT_TIMESTAMP()
CURRENT_DATE()
CURRENT_TIME()
```

### Examples
```sql
SELECT NOW(), CURRENT_DATE(), CURRENT_TIME(), CURRENT_TIMESTAMP();
```

### Status
- Implemented: NOW(), CURRENT_TIMESTAMP(), CURRENT_DATE(), CURRENT_TIME().
- Missing: CURRENT_USER, CURRENT_ROLE, CURRENT_CONNECTION, CURRENT_TRANSACTION,
  LOCALTIME, LOCALTIMESTAMP, TODAY, YESTERDAY, TOMORROW.
- Notes: bare keyword forms (without parentheses) are parsed as identifiers,
  not context variables.

## JSON
- `->`, `->>`, `#>`, `#>>`: Implemented.
- `?`, `?|`, `?&`: Missing.

## Arrays
- `ARRAY[...]` literal: Implemented (stored as JSON strings).
- `@>`, `<@`, `&&`, subscript `[]`: Missing in V2.

## Bitwise
- `&`, `|`, `^`, `~`, `<<`, `>>`: Missing in V2.

Spec delta:
- Add CONCAT encoding or explicit opcode for `||`.
- Add null-safe operators and NOT opcode for correct three-valued logic.

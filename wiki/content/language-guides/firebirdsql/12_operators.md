[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - Operators

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Source: `ScratchBird/docs/audit/30_operator_matrix_by_dialect_actual.md`.

## Examples (actual)

```sql
-- Arithmetic
SELECT 10 / 4 FROM RDB$DATABASE;

-- String and pattern
SELECT first_name || ' ' || last_name FROM people;
SELECT name LIKE 'A%' FROM people;
SELECT name SIMILAR TO '[A-Z]+' FROM people;

-- Context variables
SELECT CURRENT_DATE, CURRENT_TIMESTAMP FROM RDB$DATABASE;
```

## Known quirks (actual)

- `%` is not parsed (use `MOD()` instead).
- `||` is parsed but encoded via numeric add (string concat fails).
- `CONTAINING` and `STARTING WITH` are parsed but not encoded in V2 generator.
- `IS TRUE` / `IS FALSE` are not parsed.
- JSON and bitwise operators are not parsed.

## Arithmetic
- `+`, `-`, `*`, `/`: Implemented.
- `%`: Missing (not parsed).
- unary `+`: Implemented.

## String and pattern
- `||` concat: Partial (parsed but encoded via numeric add).
- `LIKE`: Implemented.
- `SIMILAR TO`: Implemented (regex-based).
- `CONTAINING`, `STARTING WITH`: Partial (parsed, not encoded by V2 generator).

## Comparison and boolean
- `=`, `<>`, `<`, `<=`, `>`, `>=`: Implemented.
- `IS NULL` / `IS NOT NULL`: Implemented.
- `IS TRUE` / `IS FALSE`: Missing.
- `AND` / `OR`: Implemented.
- unary `NOT`: Partial (not 3VL-safe).

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
SELECT EXTRACT(MONTH FROM order_date) FROM orders;
SELECT ALTER_ELEMENT(1 IN tags TO 'vip') FROM users;
```

### Notes
- Field names are resolved via `extract_element_catalog` (case-insensitive).
- Common fields include YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, EPOCH.
- Selector can be an index or key depending on value type.
- Spec reference: `ScratchBird/docs/specifications/ddl/EXTRACT_AND_ALTER_ELEMENT.md`.

### Status
Implemented (ALTER_ELEMENT is ScratchBird extension).

## Context variables and special date/time literals

### Description
Firebird uses keyword-style context variables (no parentheses).

### Syntax (actual)
```sql
CURRENT_DATE
CURRENT_TIME
CURRENT_TIMESTAMP
CURRENT_USER
CURRENT_ROLE
CURRENT_CONNECTION
CURRENT_TRANSACTION
```

### Examples
```sql
SELECT CURRENT_DATE, CURRENT_TIMESTAMP FROM RDB$DATABASE;
```

### Status
- Implemented end-to-end: CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP.
- Parsed but unresolved: CURRENT_USER, CURRENT_ROLE, CURRENT_CONNECTION,
  CURRENT_TRANSACTION (not mapped to built-in opcodes).
- Extension: NOW() works as a ScratchBird function call.
- Missing: LOCALTIME, LOCALTIMESTAMP (keywords exist but not parsed),
  TODAY, YESTERDAY, TOMORROW.
- Notes: keyword form is required; parentheses are not supported.

## Arrays
- `ARRAY[...]` literal: Implemented.
- Array operators (`@>`, `<@`, `&&`, subscript): Missing.

## JSON / bitwise
- JSON operators: Missing.
- Bitwise operators: Missing.

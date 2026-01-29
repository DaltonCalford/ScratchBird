[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL - Operators

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Source: `ScratchBird/docs/audit/30_operator_matrix_by_dialect_actual.md`.

## Examples (actual)

```sql
-- Arithmetic
SELECT 10 DIV 3;
SELECT 10 % 3;

-- String and regex
SELECT CONCAT(first, ' ', last) FROM people;
SELECT name REGEXP '^[A-Z]+' FROM people;

-- Null-safe comparison and logical ops
SELECT * FROM t WHERE a <=> b;
SELECT * FROM t WHERE a = 1 && b = 2;
SELECT * FROM t WHERE a = 1 || b = 2; -- OR
```

## Known quirks (actual)

- `||` is logical OR (not string concat).
- `DIV` is parsed but emitted as modulo opcode.
- `NOT` is encoded as bitwise NOT; `XOR` maps to bitwise XOR.
- JSON and array operators are not parsed.

## Arithmetic
- `+`, `-`, `*`, `/`, `%`: Implemented.
- `DIV`: Partial (mapped to modulo opcode).
- unary `+`, unary `-`: Implemented.

## String and pattern
- `LIKE`: Implemented.
- `REGEXP` / `RLIKE`: Implemented (regex opcodes).
- `||`: Parsed as logical OR (MySQL behavior); no string concat operator.

## Comparison and boolean
- `=`, `<>`, `<`, `<=`, `>`, `>=`: Implemented.
- `IS NULL` / `IS NOT NULL`: Implemented.
- `IS TRUE` / `IS FALSE`: Implemented.
- `<=>` (null-safe equality): Implemented.
- `AND` / `OR` / `&&` / `||`: Implemented.
- `NOT`: Partial (bitwise NOT encoding).
- `XOR`: Partial (mapped to bitwise XOR).

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
SELECT ALTER_ELEMENT(1 IN tags TO 'vip');
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
MySQL provides date/time context functions; ScratchBird supports a subset as
function calls.

### Syntax (actual)
```sql
NOW()
CURRENT_TIMESTAMP()
CURRENT_DATE()  -- or CURDATE()
```

### Examples
```sql
SELECT NOW(), CURRENT_TIMESTAMP(), CURRENT_DATE();
```

### Status
- Implemented: NOW(), CURRENT_TIMESTAMP(), CURRENT_DATE()/CURDATE().
- Parsed as generic function call only (requires UDR): CURRENT_USER(),
  CURRENT_ROLE(), CURRENT_SCHEMA().
- Missing: CURRENT_TIME(), LOCALTIME(), LOCALTIMESTAMP(), TODAY, YESTERDAY,
  TOMORROW.
- Notes: bare keyword forms (without parentheses) are treated as identifiers,
  not context variables.

## Bitwise
- `&`, `|`, `^`, `~`, `<<`, `>>`: Implemented.

## JSON / arrays
- JSON operators (`->`, `->>`): Not implemented.
- JSON functions (JSON_EXTRACT, JSON_OBJECT): Implemented via function calls.
- Array operators: Not implemented.

# PostgreSQL - Operators

Source: `ScratchBird/docs/audit/30_operator_matrix_by_dialect_actual.md`.

## Arithmetic
- `+`, `-`, `*`, `/`, `%`: Implemented.
- unary `+`, unary `-`: Implemented.

## String and pattern
- `||`: Partial (encoded as array concat opcode).
- `LIKE`, `ILIKE`, `SIMILAR TO`: Implemented.
- Regex `~` operators: Not parsed in PG parser.

## Comparison and boolean
- `=`, `<>`, `<`, `<=`, `>`, `>=`: Implemented.
- `IS NULL` / `IS NOT NULL`: Implemented.
- `IS DISTINCT FROM`: Implemented (null-safe).
- `AND` / `OR`: Implemented.
- unary `NOT`: Partial (encoded as bitwise NOT).

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
SELECT EXTRACT(EPOCH FROM created_at);
SELECT ALTER_ELEMENT(2 IN tags TO 'vip');
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
PostgreSQL defines keyword-style context variables; ScratchBird currently wires
only NOW() as a function call.

### Syntax (actual)
```sql
NOW()
```

### Examples
```sql
SELECT NOW();
```

### Status
- Implemented: NOW().
- Missing: CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP, CURRENT_USER,
  CURRENT_ROLE, CURRENT_SCHEMA, CURRENT_CATALOG, CURRENT_DATABASE,
  LOCALTIME, LOCALTIMESTAMP, TRANSACTION_TIMESTAMP().
- Notes: lexer emits CURRENT_* as keywords and parsePrimary does not handle
  them, so keyword forms and function-call variants are unreachable.

## JSON
- `->`, `->>`, `#>`, `#>>`: Implemented.
- `?`, `?|`, `?&`: Implemented.

## Arrays
- `ARRAY[...]` literal: Implemented.
- `@>`, `<@`, `&&`: Implemented.
- `[]` subscript: Stubbed (executor lacks handler).

## Bitwise
- `&`, `|`, `^`, `~`, `<<`, `>>`: Implemented.

## Other
- Cast `::`: Implemented.
- Text search `@@`: Implemented.

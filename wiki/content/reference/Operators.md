# Operators

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

This page summarizes ScratchBird operators, their purpose, and examples.
Operator availability can vary by dialect; see language guides for per-dialect
coverage.

## Operator precedence (high to low)

1. `::` (PostgreSQL-style cast)
2. `[]` (array subscript)
3. `.` (member selection)
4. unary `-`, unary `+`, unary `~`
5. `^` (exponentiation)
6. `*`, `/`, `%`
7. `+`, `-`
8. `<<`, `>>`, `&`, `|`, `#`
9. `=`, `<>`, `!=`, `<`, `>`, `<=`, `>=`
10. `BETWEEN`, `IN`, `LIKE`, `ILIKE`, `SIMILAR TO`, `EXISTS`
11. `IS NULL`, `IS NOT NULL`, `IS TRUE`, `IS FALSE`, `IS UNKNOWN`
12. `NOT`
13. `AND`
14. `OR`

## Arithmetic operators

| Operator | Purpose | Example |
| --- | --- | --- |
| `+` | Addition | `SELECT 2 + 3;` |
| `-` | Subtraction | `SELECT 10 - 4;` |
| `*` | Multiplication | `SELECT 6 * 7;` |
| `/` | Division | `SELECT 20 / 5;` |
| `%` | Modulo | `SELECT 10 % 3;` |
| `^`, `**` | Exponentiation | `SELECT 2 ^ 8;` |
| unary `+` | Numeric identity | `SELECT +price FROM items;` |
| unary `-` | Numeric negation | `SELECT -balance FROM accounts;` |

## Comparison operators

| Operator | Purpose | Example |
| --- | --- | --- |
| `=` | Equal | `WHERE status = 'ACTIVE'` |
| `<>`, `!=` | Not equal | `WHERE status <> 'ACTIVE'` |
| `<`, `<=`, `>`, `>=` | Ordering | `WHERE amount >= 100` |
| `IS DISTINCT FROM` | Null-safe inequality | `WHERE a IS DISTINCT FROM b` |
| `IS NOT DISTINCT FROM` | Null-safe equality | `WHERE a IS NOT DISTINCT FROM b` |

## Null and boolean predicates

| Operator | Purpose | Example |
| --- | --- | --- |
| `IS NULL` / `IS NOT NULL` | Null checks | `WHERE deleted_at IS NULL` |
| `IS TRUE` / `IS FALSE` | Boolean checks | `WHERE is_active IS TRUE` |
| `IS UNKNOWN` | Three-valued logic | `WHERE flag IS UNKNOWN` |

## Range and membership predicates

| Operator | Purpose | Example |
| --- | --- | --- |
| `BETWEEN` | Inclusive range | `WHERE age BETWEEN 18 AND 65` |
| `NOT BETWEEN` | Outside range | `WHERE age NOT BETWEEN 18 AND 65` |
| `IN (...)` | Set membership | `WHERE status IN ('NEW','HOLD')` |
| `NOT IN (...)` | Not in set | `WHERE status NOT IN ('NEW','HOLD')` |
| `EXISTS (...)` | Subquery existence | `WHERE EXISTS (SELECT 1 FROM orders ...)` |

## Quantified comparisons

Compare a value against a subquery with `ANY`, `ALL`, or `SOME`:

```
SELECT *
FROM products
WHERE price > ANY (SELECT price FROM discounts);
```

## Logical operators

| Operator | Purpose | Example |
| --- | --- | --- |
| `AND` | Logical AND | `WHERE a = 1 AND b = 2` |
| `OR` | Logical OR | `WHERE a = 1 OR b = 2` |
| `NOT` | Logical NOT | `WHERE NOT is_deleted` |

## String concatenation

| Operator | Purpose | Example |
| --- | --- | --- |
| `||` | Concatenate strings | `SELECT first || ' ' || last;` |
| `CONCAT` | Concatenate strings | `SELECT CONCAT(first, ' ', last);` |

## Pattern matching and regex

| Operator | Purpose | Example |
| --- | --- | --- |
| `LIKE` | Pattern match | `WHERE name LIKE 'A%'` |
| `ILIKE` | Case-insensitive LIKE | `WHERE name ILIKE 'a%'` |
| `SIMILAR TO` | SQL regex | `WHERE code SIMILAR TO '[A-Z]{3}-[0-9]+'` |
| `REGEXP` | Regex match | `WHERE name REGEXP '^[A-Z]+'` |

ESCAPE can be used with LIKE/ILIKE/SIMILAR TO:

```
SELECT '100%'
WHERE '100%' LIKE '100\%' ESCAPE '\\';
```

## Bitwise operators

| Operator | Purpose | Example |
| --- | --- | --- |
| `&` | Bitwise AND | `SELECT flags & 4;` |
| `|` | Bitwise OR | `SELECT flags | 4;` |
| `#` | Bitwise XOR | `SELECT flags # 4;` |
| `~` | Bitwise NOT | `SELECT ~flags;` |
| `<<` | Shift left | `SELECT flags << 2;` |
| `>>` | Shift right | `SELECT flags >> 2;` |

## Array operators (dialect-specific)

| Operator | Purpose | Example |
| --- | --- | --- |
| `@>` | Contains | `WHERE tags @> ARRAY['sql']` |
| `<@` | Is contained by | `WHERE tags <@ ARRAY['sql','db']` |
| `&&` | Overlap | `WHERE tags && ARRAY['sql','ml']` |
| `||` | Array concat | `SELECT ARRAY[1,2] || ARRAY[3,4];` |
| `[n]` | Subscript | `SELECT tags[1] FROM items;` |
| `[n:m]` | Slice | `SELECT tags[1:3] FROM items;` |

## JSON/JSONB operators (dialect-specific)

| Operator | Purpose | Example |
| --- | --- | --- |
| `->` | Get JSON field | `SELECT doc->'status' FROM logs;` |
| `->>` | Get JSON field as text | `SELECT doc->>'status' FROM logs;` |
| `#>` | Get JSON path | `SELECT doc#>'{a,0,b}' FROM logs;` |
| `#>>` | Get JSON path as text | `SELECT doc#>>'{a,0,b}' FROM logs;` |
| `?` | Key existence | `SELECT doc ? 'status' FROM logs;` |
| `?|` | Any key existence | `SELECT doc ?| ARRAY['a','b'] FROM logs;` |
| `?&` | All keys existence | `SELECT doc ?& ARRAY['a','b'] FROM logs;` |

## Cast and conversion

| Operator | Purpose | Example |
| --- | --- | --- |
| `::` | PostgreSQL-style cast | `SELECT amount::DECIMAL(12,2);` |
| `CAST` | SQL cast | `SELECT CAST(amount AS DECIMAL(12,2));` |
| `CONVERT` | MySQL-style cast | `SELECT CONVERT(amount, DECIMAL(12,2));` |

## Set operators (query-level)

| Operator | Purpose | Example |
| --- | --- | --- |
| `UNION` | Combine rows (distinct) | `SELECT a FROM t1 UNION SELECT a FROM t2;` |
| `UNION ALL` | Combine rows (all) | `SELECT a FROM t1 UNION ALL SELECT a FROM t2;` |
| `INTERSECT` | Common rows | `SELECT a FROM t1 INTERSECT SELECT a FROM t2;` |
| `EXCEPT` | Rows in left not right | `SELECT a FROM t1 EXCEPT SELECT a FROM t2;` |

## Appendix: Per-dialect operator parity (actual)

Legend:
- Y = parsed and executed correctly
- P = parsed but incorrect/partial semantics
- N = not parsed in that dialect
- U = parsed/emitted but executor missing

### Arithmetic and numeric

| Operator | Native (V2) | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `+` | Y | Y | Y | Y | |
| `-` | Y | Y | Y | Y | |
| `*` | Y | Y | Y | Y | |
| `/` | Y | Y | Y | Y | |
| `%` | Y | N | Y | Y | Firebird parser does not accept `%` |
| `DIV` | N | N | N | P | MySQL parses DIV but emits modulo opcode |
| unary `+` | N | Y | Y | Y | V2 does not parse unary plus |
| unary `-` | Y | Y | Y | Y | |
| `^` (power) | N | N | N | N | Use POWER()/POW() instead |

### String and pattern

| Operator | Native (V2) | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `||` (concat) | P | P | P | N | V2/Firebird emit wrong opcode; PG uses EXT_ARRAY_CAT; MySQL treats `||` as OR |
| `LIKE` | Y | Y | Y | Y | |
| `ILIKE` | Y | N | Y | N | |
| `SIMILAR TO` | Y | Y | Y | N | Uses regex opcodes |
| `CONTAINING` | N | P | N | N | Firebird parsed but not encoded by V2 generator |
| `STARTING WITH` | N | P | N | N | Firebird parsed but not encoded by V2 generator |
| `REGEXP`/`RLIKE` | N | N | N | Y | MySQL uses EXT_REGEX_MATCH |
| `~`, `~*`, `!~`, `!~*` | Y | N | N | N | V2 supports regex operators; PG parser does not |

### Comparison and NULL-safe

| Operator | Native (V2) | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `=`, `<>`/`!=`, `<`, `<=`, `>`, `>=` | Y | Y | Y | Y | |
| `IS NULL` / `IS NOT NULL` | Y | Y | Y | Y | |
| `IS TRUE` / `IS FALSE` | Y | N | Y | Y | Firebird parser only handles IS NULL |
| `IS DISTINCT FROM` | P | N | Y | N | V2 emits EQ/NE; PG uses EXT_NULL_SAFE_EQ |
| `<=>` (null-safe EQ) | N | N | N | Y | MySQL only |

### Logical

| Operator | Native (V2) | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `AND` / `OR` | Y | Y | Y | Y | MySQL also accepts `&&`/`||` |
| `NOT` | P | P | P | P | V2 uses `= 0`; PG/MySQL emit EXT_BIT_NOT |
| `XOR` | N | N | N | P | MySQL maps XOR to bitwise XOR opcode |

### JSON

| Operator | Native (V2) | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `->` | Y | N | Y | N | |
| `->>` | Y | N | Y | N | |
| `#>` | Y | N | Y | N | |
| `#>>` | Y | N | Y | N | |
| `?`, `?|`, `?&` | N | N | Y | N | PG parser maps to JSON/array opcodes |

### Arrays

| Operator | Native (V2) | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `ARRAY[...]` literal | Y | Y | Y | N | Stored as JSON strings in executor |
| `[]` subscript | N | N | U | N | PG emits EXT_ARRAY_SUBSCRIPT; executor lacks handler |
| `@>`, `<@`, `&&` | N | N | Y | N | PG emits EXT_ARRAY_*; executor supports |

### Bitwise

| Operator | Native (V2) | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `&`, `|`, `^` | N | N | Y | Y | PG/MySQL emit EXT_BIT_* |
| `~` (bitwise NOT) | N | N | Y | Y | PG/MySQL emit EXT_BIT_NOT |
| `<<`, `>>` | N | N | Y | Y | PG/MySQL emit EXT_BIT_SHIFT_* |

### Cast and other

| Operator | Native (V2) | Firebird | PostgreSQL | MySQL | Notes |
| --- | --- | --- | --- | --- | --- |
| `::` (cast) | N | N | Y | N | V2 supports CAST() only |
| `IN` / `NOT IN` | Y | Y | Y | P | MySQL does not parse NOT IN explicitly |
| `BETWEEN` / `NOT BETWEEN` | Y | Y | Y | Y | V2 NOT BETWEEN uses equality inversion |
| `@@` (text search match) | N | N | Y | N | PG emits EXT_TSMATCH |

## Appendix: Per-dialect operator examples and quirks (actual)

These examples mirror the current parser/executor behavior from the audit
matrix. Use them to validate parser parity and note quirks in emulation.

### Native V2 (ScratchBird)

**Arithmetic**
```
SELECT 1 + 2 * 3;
SELECT -price FROM items;
```

**String and pattern**
```
SELECT name LIKE 'A%';
SELECT name ILIKE 'a%';
SELECT code SIMILAR TO '[A-Z]{3}-[0-9]+';
SELECT 'abc' ~ 'a.*';
```

**Comparison and NULL checks**
```
SELECT * FROM t WHERE status IS NULL;
SELECT * FROM t WHERE a IS DISTINCT FROM b;
```

**JSON**
```
SELECT doc->'status', doc->>'status' FROM logs;
SELECT doc#>'{a,0,b}', doc#>>'{a,0,b}' FROM logs;
```

**Array literal (no operators)**
```
SELECT ARRAY['a','b','c'];
```

**Quirks (actual)**
- `||` parses but is encoded as numeric add (fails for string concat).
- `IS DISTINCT FROM` is not null-safe (uses EQ/NE).
- unary `NOT` is not 3VL-safe (encoded as `= 0`).
- Bitwise and array operators are tokenized but not parsed.
- `::` cast is not parsed; use `CAST(...)`.

### Firebird SQL (emulation)

**Arithmetic**
```
SELECT 10 / 4 FROM RDB$DATABASE;
```

**String and pattern**
```
SELECT first_name || ' ' || last_name FROM people;
SELECT name LIKE 'A%' FROM people;
SELECT name SIMILAR TO '[A-Z]+' FROM people;
```

**Context variables**
```
SELECT CURRENT_DATE, CURRENT_TIMESTAMP FROM RDB$DATABASE;
```

**Quirks (actual)**
- `%` is not parsed (use `MOD()` instead).
- `||` is parsed but encoded via numeric add (string concat fails).
- `CONTAINING` and `STARTING WITH` are parsed but not encoded in V2 generator.
- `IS TRUE` / `IS FALSE` are not parsed.
- JSON and bitwise operators are not parsed.

### PostgreSQL (emulation)

**Arithmetic and casts**
```
SELECT 1 + 2 * 3;
SELECT amount::DECIMAL(12,2);
```

**String and pattern**
```
SELECT first || ' ' || last FROM people;
SELECT name ILIKE 'a%' FROM people;
SELECT name ~* '^[a-z]+' FROM people;
```

**JSON and arrays**
```
SELECT doc->'status', doc->>'status' FROM logs;
SELECT tags @> ARRAY['sql'] FROM items;
```

**Bitwise**
```
SELECT flags & 4, flags << 2 FROM t;
```

**Quirks (actual)**
- `||` is emitted as EXT_ARRAY_CAT; verify string concat semantics.
- Array subscript `[]` is emitted but executor lacks handler (U).

### MySQL (emulation)

**Arithmetic and casts**
```
SELECT 10 DIV 3;
SELECT CAST(amount AS DECIMAL(12,2));
```

**String and pattern**
```
SELECT CONCAT(first, ' ', last) FROM people;
SELECT name REGEXP '^[A-Z]+' FROM people;
```

**NULL-safe comparison**
```
SELECT * FROM t WHERE a <=> b;
```

**Logical operators**
```
SELECT * FROM t WHERE a = 1 && b = 2;
```

**Quirks (actual)**
- `||` is treated as OR (not string concat).
- `DIV` is parsed but emitted as modulo opcode.
- `NOT IN` is not parsed explicitly (partial).
- `SIMILAR TO` and `ILIKE` are not parsed.

## References

- `docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md`
- `docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md`
- `docs/audit/languages/*/12_operators.md`
- `docs/audit/30_operator_matrix_by_dialect_actual.md`

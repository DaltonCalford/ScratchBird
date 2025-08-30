### Data Types and Type Descriptors

What it is
- Rules for parsing type specifications (name, length/precision/scale, charset/collate, arrays), and special forms like CAST and TYPEOF.

Why it matters
- Ensures types are understood consistently in SQL and PSQL; affects how values are interpreted and validated.

How to use it
- Use the examples to specify types in DDL/PSQL, and CAST/TYPEOF to normalize expressions.

Type descriptor parsing is implemented in `src/engine/parser_expr.cpp::parse_type_descriptor` and a lite variant in `src/engine/parser_psql.cpp`.

Recognized components:
- Base type name (identifier before space or `(`)
- Length or precision/scale in parentheses, e.g. `CHAR(20)`, `DECIMAL(10,2)`
- Optional `CHARACTER SET name` and `COLLATE name`
- Array rank via `[]` suffixes

Special forms:
- `CAST(expr AS type)`
- `TYPE OF ...` and `TYPE OF COLUMN ...` normalized to `TYPEOF(...)`

Examples:
```sql
SELECT CAST('42' AS INTEGER);
EXECUTE BLOCK AS DECLARE VARIABLE s VARCHAR(32) CHARACTER SET UTF8; BEGIN END
```

Code anchors: `src/engine/parser_expr.cpp` (parse_type_descriptor)

See also
- [Operators](./sql-operators.md) · [Lexical](./sql-lexical.md)

Catalog of common data types (as seen in parsing and runtime structures)

Numeric
- SMALLINT, INTEGER, BIGINT: signed integers for general arithmetic
- NUMERIC(p,s), DECIMAL(p,s): fixed-point with precision/scale
- FLOAT, DOUBLE PRECISION: IEEE floating-point (use with care for exact money)
- DECFLOAT: decimal floating-point variant

Text
- CHAR(n), VARCHAR(n): fixed/variable length character types
- CITEXT: case-insensitive text (when enabled)
- CHARACTER SET name, COLLATE name: control encoding and collations for text comparison

Boolean
- BOOLEAN: true/false/null values

Binary/Other
- BLOB: binary large object
- UUID: universally unique identifier (also recognized via `UUID '...'` or `X'...'` literal forms in the lexer)
- JSON: structured text storage (when enabled)

Temporal
- DATE: calendar date
- TIME [WITH TIME ZONE]: time of day (lexer supports validations and timezone suffixes)
- TIMESTAMP [WITH TIME ZONE]: date and time (lexer validates common forms)

Arrays and special forms
- Any base type may be annotated with array brackets: `INTEGER[]`, `VARCHAR(20)[][]`
- TYPE OF, TYPEOF(...): refer to the type of existing columns/domains

Usage examples
```sql
-- Fixed-point numeric
CREATE TABLE orders (
  id BIGINT PRIMARY KEY,
  total DECIMAL(12,2) NOT NULL
);

-- Text with charset/collation
CREATE TABLE users (
  name VARCHAR(100) CHARACTER SET UTF8 COLLATE unicode
);

-- Temporal with validation at parse/lex time
INSERT INTO events(ts) VALUES (TIMESTAMP '2024-01-02 03:04:05Z');

-- Arrays and TYPEOF
CREATE TABLE vectors (
  embedding VECTOR(1536),
  tags VARCHAR(32)[]
);
SELECT CAST(value AS TYPEOF(column_name)) FROM t;
```


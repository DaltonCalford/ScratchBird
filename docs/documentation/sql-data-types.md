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


### Data Types and Type Descriptors

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


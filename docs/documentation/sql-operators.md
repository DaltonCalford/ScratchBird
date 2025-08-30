### Operators and Precedence

Parsing precedence is defined in `src/engine/parser_expr.cpp` via Pratt parser binding powers; runtime boolean evaluation is in `src/engine/expr.cpp`.

- Arithmetic: `*` `/` (bp 70), `+` `-` (bp 60)
- String concatenation: `||` (bp 55)
- Comparisons: `=` `<>` `!=` `<` `>` `<=` `>=` (bp 50)
- Cast: `lhs::type` normalized to `CAST(lhs AS type)` (bp 80); prefix `CAST(...)`, `TYPE OF [...]` recognized
- Keyword operators: `IN`, `BETWEEN`, `LIKE`, `SIMILAR [TO]`, `IS [NOT]` (bp 45)
- Logical: `AND` (bp 30), `OR` (bp 20); unary `NOT`

Runtime evaluation currently implements: `AND`, `OR`, `NOT`, comparisons, and `IS [NOT] NULL` for predicates. Other operators normalize in expressions but may not be evaluated in `expr.cpp`.

Examples:
```sql
-- Concatenation and comparisons
SELECT 'a' || 'b' AS s WHERE 2 >= 1 AND NOT (1 = 0);
```

Code anchors: `src/engine/parser_expr.cpp`, `src/engine/expr.cpp`

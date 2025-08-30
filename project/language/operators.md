## Operators and Precedence

Binary operators and special forms as parsed in expressions:

- Multiplicative: `*`, `/` (bp 70)
- Additive: `+`, `-` (bp 60)
- String concatenation: `||` (bp 55)
- Comparisons: `=`, `<>`, `!=`, `<`, `>`, `<=`, `>=` (bp 50)
- Cast: `::` (normalized to `CAST(lhs AS type)`) (bp 80)
- Keywords as operators: `IN`, `BETWEEN`, `LIKE`, `SIMILAR [TO]`, `IS [NOT]` (bp 45)
- Logical: `AND` (bp 30), `OR` (bp 20)
- Prefix forms: `NOT` (unary), `+` (unary), `CAST(expr AS type)`, `TYPE OF [...]`.

Associativity: left-associative for binary operators under Pratt parser rules at given binding powers.

Nullability: comparisons with NULL yield NULL and are treated as false in predicate evaluation. `IS NULL`/`IS NOT NULL` produce boolean values. Logical operators treat NULL as false.

### Implementation References
- Parser precedence: `src/engine/parser_expr.cpp` (lbp, nud/expr_bp handling)
- Predicate evaluation semantics: `src/engine/expr.cpp` (evaluate_predicate, NULL handling)
- Lexer tokens: `include/scratchbird/engine/lexer.h`

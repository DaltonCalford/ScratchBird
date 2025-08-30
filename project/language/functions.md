## Built-in Functions

Scratchbird currently treats most expression forms as textual and does not implement a full scalar function engine in `expr.cpp`. However, the PSQL executor inlines a few deterministic string functions inside procedural code:

- `UPPER(text)` -> text (folds to uppercase when argument is a constant string)
- `LOWER(text)` -> text (folds to lowercase when constant)
- `LENGTH(text)` -> integer (folds to integer length when constant)

Type and nullability:
- UPPER/LOWER: input non-null text -> non-null text. NULL remains NULL when evaluated at runtime.
- LENGTH: input non-null text -> non-null integer. NULL remains NULL when evaluated at runtime.

Note: In SQL SELECT expressions, arbitrary identifiers and literals are propagated as strings or integers; comparison/boolean evaluation is defined in the predicate engine.

### Implementation References
- Procedural inlining: `src/engine/psql_executor.cpp` (inline_deterministic_functions)
- Expression normalization/AST: `src/engine/parser_expr.cpp` (parse_expression_to_string, parse_type_descriptor)
- Predicate evaluation: `src/engine/expr.cpp`


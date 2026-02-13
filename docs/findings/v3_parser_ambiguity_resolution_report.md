# Parser Ambiguity Resolution Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/PARSER_AMBIGUITY_RESOLUTION.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
Core operator precedence (OR/AND/NOT, comparisons, concat, additive/multiplicative) and ORDER BY numeric position handling are implemented. However, precedence ordering in the spec does not fully match the parser: comparison operators are currently parsed *after* IN/BETWEEN/LIKE in `parseComparisonExpr`, which conflicts with the specified precedence order. JOIN binding rules and CROSS JOIN precedence are not explicitly enforced beyond left-associative parsing. Some disambiguation rules (CREATE TABLE vs CTAS, WITH placement) are not verified here.

## Findings by Spec Item

### 1) Operator Precedence
- [~] Parentheses, unary (+/-/NOT), additive, multiplicative, concat, comparisons implemented.
  - Expression parser uses `parseOrExpr` → `parseAndExpr` → `parseNotExpr` → `parseComparisonExpr` → `parseConcatExpr` → bit ops → shift → add → mul → power → unary → primary. See `src/parser/parser_v3.cpp:7516-8335`.
- [ ] Precedence order of comparisons vs special comparisons/patterns differs from spec.
  - `parseComparisonExpr` handles `IN/BETWEEN/LIKE/...` **before** binary comparison operators (`=`, `<`, etc.). Spec puts comparisons *before* pattern and `BETWEEN/IN`. This changes parse associativity for expressions mixing these operators.
- [~] Exponentiation implemented as right-associative `^` in `parsePowerExpr`.
  - See `src/parser/parser_v3.cpp:8288-8305`.

### 2) Set Operator Precedence
- [ ] Set operator precedence (INTERSECT > UNION > EXCEPT) not verified.
  - `parseSetOperation` exists but precedence enforcement beyond parenthesis check not validated in this review.

### 3) JOIN Binding
- [~] JOINs parsed left-associatively in `parseFromClause`/`parseJoin`.
  - No explicit precedence between JOIN types beyond parsing order. See `src/parser/parser_v3.cpp:6407-6594`.
- [ ] CROSS JOIN lowest precedence and NATURAL binding not explicitly enforced in AST or emission.
  - NATURAL is encoded into join_type; no explicit precedence resolution.

### 4) Statement Disambiguation
- [ ] CREATE TABLE vs CREATE TABLE AS rules not verified here.
- [~] INSERT DEFAULT VALUES vs VALUES handled via `DEFAULT VALUES` branch in `parseInsert`.
  - See `src/parser/parser_v3.cpp:6882-6886`.
- [~] CAST vs type name handled; typed literals parse WITH TIME ZONE.
  - Cast (`::`) handled in `parseUnaryExpr`; typed literals handled in `parsePrimaryExpr` with WITH TIME ZONE parsing. See `src/parser/parser_v3.cpp:8320-8450`.
- [ ] WITH in DML vs WITH options not verified.

### 5) ORDER BY Numeric Positions
- [*] Implemented; resolves numeric positions and errors out of range.
  - See `src/parser/parser_v3.cpp:6233-6259`.

### 6) Ambiguous NULL / DEFAULT
- [~] DEFAULT in INSERT values becomes `LiteralType::DEFAULT` and emits `SBLR3_DEFAULT_VALUE`.
  - See `src/parser/parser_v3.cpp:6919-6923` and `src/parser/v3_emitter.cpp:3557-3560`.
- [~] NULL emits `SBLR3_LITERAL_NULL` via literal parsing.

## Notes
- The precedence mismatch between comparison operators and `IN/BETWEEN/LIKE` is a concrete divergence from the spec and could change parse trees in mixed expressions.

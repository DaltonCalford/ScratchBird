# Dependencies

## Upstream sections

- Section `08`: transaction lifecycle and always-in-transaction rules
- Section `14`: base scalar type definitions and target type identity
- Section `15`: complex type definitions and target type identity
- Section `22`: SBLR expression and cast lowering contracts
- Section `23`: executor and VM implementation ownership
- Section `28`: parser ownership for `CAST` expression syntax and `SET` syntax
- Section `36`: planner and rewrite boundaries for expression shaping before execution

## Adjacent implementation authorities

- `src/core/typed_value.cpp`: canonical explicit cast implementation
- `src/sblr/executor.cpp`: write-path coercion, function binding, and branch-local implicit coercion helpers
- `src/sblr/expression_evaluator.cpp`: expression-family-local coercion behavior
- `src/parser/parser_v3.cpp`: shorthand `SET operator.strict_mode ON` syntax support
- `include/scratchbird/core/expression.h`: operator enum ownership

## Explicit exclusions

This section does not depend on any WAL, redo-log, or LSN model. It inherits ScratchBird MGA semantics only.

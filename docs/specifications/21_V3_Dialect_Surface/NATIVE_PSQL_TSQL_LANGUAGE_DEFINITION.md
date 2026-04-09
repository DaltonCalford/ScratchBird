# Native PSQL and TSQL Language Definition

## Current code-backed truth
- `parsePsqlBody()` is real.
- Transaction-control parse entry points for begin, commit, rollback, savepoint, and prepare-transaction are real.
- Procedural AST and lowering surfaces exist in the parser and emitter stack.

## Proven anchors
- `include/scratchbird/parser/parser_v3.h`
- `src/parser/parser_v3.cpp`
- `include/scratchbird/parser/ast_v3.h`

## Boundary
- Procedural parsing is code-backed.
- Full TSQL alias, cursor, exception, and control-flow parity remains partial unless covered by current tests.
- This file should be treated as parser-front-door authority, not blanket executor-semantic proof.

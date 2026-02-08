# V3 Module Layout (Proposed)

Date: 2026-02-08

This layout keeps V2 intact while enabling a clean V3 pipeline. Adjust as needed for final integration.

## Parser / AST
- `include/scratchbird/parser/v3/ast.h`
- `include/scratchbird/parser/v3/types.h`
- `include/scratchbird/parser/v3/literals.h`
- `src/parser/v3/ast.cpp`
- `src/parser/v3/parser.cpp`
- `src/parser/v3/lexer.cpp`
- `src/parser/v3/resolver.cpp` (catalog UUID v7 binding)

## SBLR V3
- `include/scratchbird/sblr/v3/opcodes.h`
- `include/scratchbird/sblr/v3/container.h`
- `include/scratchbird/sblr/v3/payloads.h`
- `include/scratchbird/sblr/v3/constant_pool.h`
- `include/scratchbird/sblr/v3/validator.h`
- `include/scratchbird/sblr/v3/canonicalizer.h`
- `src/sblr/v3/bytecode_generator.cpp`
- `src/sblr/v3/validator.cpp`
- `src/sblr/v3/canonicalizer.cpp`

## Executor V3
- `include/scratchbird/executor/v3/executor.h`
- `src/executor/v3/executor.cpp`
- `src/executor/v3/sql_engine.cpp`
- `src/executor/v3/psql_runtime.cpp`

## Dialect Emulation
- `src/parser/mysql/*` -> emit to V3 SBLR generator
- `src/parser/postgresql/*` -> emit to V3 SBLR generator

## Tests (V3)
- `tests/v3/parser/*`
- `tests/v3/sblr/*`
- `tests/v3/executor/*`
- `tests/v3/dialect/*`
- `tests/v3/ipc/*`
- `tests/v3/protocol/*`
- `tests/v3/ops/*`

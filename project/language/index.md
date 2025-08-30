## Scratchbird Language Reference

Welcome to the Scratchbird SQL dialect reference.

- See `grammar.md` for grammar constructs and parsing entry points.
- See `statements.md` for statement syntax, semantics, side-effects, and code anchors.
- See `functions.md` for built-in and pseudo-functions.
- See `operators.md` for operators, precedence, and null semantics.
- See `keywords.md` for the keyword list recognized by the lexer.

### Code Anchors
- Parser entry: `include/scratchbird/engine/parser.h`
- Lexer: `include/scratchbird/engine/lexer.h` and `src/engine/lexer.cpp`
- Expression: `include/scratchbird/engine/parser_expr.h`, `src/engine/parser_expr.cpp`, `src/engine/expr.cpp`
- SELECT: `include/scratchbird/engine/parser_select.h`, `src/engine/parser_select.cpp`
- DML: `include/scratchbird/engine/parser_dml.h`, `src/engine/parser_dml.cpp`
- Session: `src/engine/parser_session.cpp`
- DDL: `src/engine/parser_ddl.cpp`
- Executor: `include/scratchbird/engine/executor.h`, `src/engine/executor.cpp`

# Scratchbird Language Reference

This section documents the Scratchbird SQL dialect: grammar, statements, functions, operators, and keywords.

- See  for an overview of grammar and constructs.
- See  for statement semantics.
- See  for built-in functions.
- See  for operators and precedence.
- See  for reserved words.


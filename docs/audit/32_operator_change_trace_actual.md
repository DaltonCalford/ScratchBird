# Operator Change Trace (Actual)

Purpose: Trace each missing/incorrect operator to the exact code locations that need changes in parser, bytecode generator, and executor.

Status: static code review snapshot; no runtime execution performed.

## 1) `||` string concatenation (V2/Firebird)
- Parser: `ScratchBird/src/parser/parser_v2.cpp` (`parseAddExpr`, `parseAddExprWithLeft` already set `BinaryOp::CONCAT`).
- Semantic: `ScratchBird/src/sblr/semantic_analyzer_v2.cpp` (`getCommonType` already handles `BinaryOp::CONCAT`).
- Bytecode: `ScratchBird/src/sblr/bytecode_generator_v2.cpp`
  - `binaryOpToOpcode` has no CONCAT mapping (falls to EXPR_ADD).
  - `generateBinaryExpr` has no CONCAT branch.
- Executor: `ScratchBird/src/sblr/executor.cpp`
  - Add an opcode handler for string concat (new EXPR_CONCAT or an EXT op).
- Supporting definitions: `ScratchBird/include/scratchbird/sblr/opcodes.h` (add EXPR_CONCAT or EXT_STRING_CONCAT).

## 2) `||` string concatenation (PostgreSQL)
- Parser: `ScratchBird/src/parser/postgresql/pg_parser_expr.cpp` (`parseAdditiveExpr` emits EXT_ARRAY_CAT for `||`).
- Executor: `ScratchBird/src/sblr/executor.cpp` (EXT_ARRAY_CAT is JSON array concat).
- Fix: emit string-concat opcode instead of EXT_ARRAY_CAT and implement it in executor.

## 3) `IS DISTINCT FROM` / `IS NOT DISTINCT FROM` (V2)
- Parser: `ScratchBird/src/parser/parser_v2.cpp` (`parseComparisonExpr` uses EQ/NE for DISTINCT).
- Semantic: `ScratchBird/src/sblr/semantic_analyzer_v2.cpp` (add a null-safe comparison branch if new op is introduced).
- Bytecode: `ScratchBird/src/sblr/bytecode_generator_v2.cpp`
  - Emit EXT_NULL_SAFE_EQ with inversion for DISTINCT vs NOT DISTINCT.
- Executor: `ScratchBird/src/sblr/executor.cpp` already supports EXT_NULL_SAFE_EQ.

## 4) Unary `NOT` (V2)
- Parser: `ScratchBird/src/parser/parser_v2.cpp` (`parseNotExpr` builds UnaryOp::NOT).
- Bytecode: `ScratchBird/src/sblr/bytecode_generator_v2.cpp` (`generateUnaryExpr` encodes NOT as `= 0`).
- Executor: `ScratchBird/src/sblr/executor.cpp` (no boolean NOT opcode).
- Fix: add a boolean NOT opcode (e.g., EXPR_NOT or EXT_BOOL_NOT) with NULL-preserving semantics; update generator to emit it.

## 5) Unary `NOT` (PostgreSQL/MySQL)
- PG parser: `ScratchBird/src/parser/postgresql/pg_parser_expr.cpp` (`parseNotExpr` emits EXT_BIT_NOT).
- MySQL parser: `ScratchBird/src/parser/mysql/mysql_parser.cpp` (`parseNotExpr` emits EXT_BIT_NOT).
- Fix: emit the boolean NOT opcode once added; keep EXT_BIT_NOT for `~` only.

## 6) Bitwise operators in V2 (`&`, `|`, `^`, `~`, `<<`, `>>`)
- Parser: `ScratchBird/src/parser/parser_v2.cpp` (add bitwise/shift precedence functions similar to PG/MySQL).
- AST: `ScratchBird/include/scratchbird/parser/ast_v2.h` (BinaryOp already contains BIT_* and SHIFT_*).
- Semantic: `ScratchBird/src/sblr/semantic_analyzer_v2.cpp` (`getCommonType` needs numeric/bitwise rules).
- Bytecode: `ScratchBird/src/sblr/bytecode_generator_v2.cpp`
  - Add cases in `generateBinaryExpr` to emit EXT_BIT_* opcodes.
  - Add unary `~` handling in `generateUnaryExpr` (EXT_BIT_NOT).
- Executor: already supports EXT_BIT_* in `ScratchBird/src/sblr/executor.cpp`.

## 7) Array operators in V2 (`@>`, `<@`, `&&`)
- Parser: `ScratchBird/src/parser/parser_v2.cpp` (add infix operators for AT_GREATER/LESS_AT/DOUBLE_AMPERSAND).
- AST: `ScratchBird/include/scratchbird/parser/ast_v2.h` (BinaryOp has ARRAY_*).
- Semantic: `ScratchBird/src/sblr/semantic_analyzer_v2.cpp` (type rules for arrays/JSON strings).
- Bytecode: `ScratchBird/src/sblr/bytecode_generator_v2.cpp` (emit EXT_ARRAY_CONTAINS/CONTAINED_BY/OVERLAP).
- Executor: already supports EXT_ARRAY_*.

## 8) Array subscript `[]`
- PG parser emits EXT_ARRAY_SUBSCRIPT (`ScratchBird/src/parser/postgresql/pg_parser_expr.cpp`).
- Executor: missing handler for EXT_ARRAY_SUBSCRIPT in `ScratchBird/src/sblr/executor.cpp`.
- V2 parser: no AST node for subscript; add a new expression node and bytecode emission if needed.

## 9) JSON existence operators (`?`, `?|`, `?&`)
- Parser: `ScratchBird/src/parser/parser_v2.cpp` (add postfix operators in expression parsing).
- Bytecode: map to existing executor-supported patterns, e.g.:
  - `?` -> JSON_EXTRACT + LITERAL_NULL + EXPR_NE (see PG parser pattern).
  - `?|` -> EXT_ARRAY_OVERLAP (PG behavior).
  - `?&` -> EXT_ARRAY_CONTAINS (PG behavior).
- Executor: already supports JSON and array ops used above.

## 10) `::` cast operator
- Parser: `ScratchBird/src/parser/parser_v2.cpp` (add postfix `DOUBLE_COLON` handling; parse type name).
- AST: `ScratchBird/include/scratchbird/parser/ast_v2.h` has `CastExpr` already.
- Bytecode: `ScratchBird/src/sblr/bytecode_generator_v2.cpp` already emits EXPR_CAST.
- Executor: EXPR_CAST is implemented in `ScratchBird/src/sblr/executor.cpp`.

## 11) Firebird `CONTAINING` and `STARTING WITH`
- Parser: `ScratchBird/src/parser/firebird/firebird_parser.cpp` sets `LikeMatchKind::CONTAINING/STARTING`.
- Bytecode: `ScratchBird/src/sblr/bytecode_generator_v2.cpp` (`generateLike`) ignores these kinds.
- Fix: implement these match kinds (likely rewrite to LIKE/ILIKE with `%` wrapping or add new opcodes).

## 12) MySQL `XOR` and `DIV`
- XOR parser: `ScratchBird/src/parser/mysql/mysql_parser.cpp` (`parseXorExpr` emits EXT_BIT_XOR).
- DIV parser: `ScratchBird/src/parser/mysql/mysql_parser.cpp` (`parseMultiplicativeExpr` maps DIV to EXPR_MODULO).
- Fix: implement boolean XOR (compose with OR/AND/NOT or add opcode) and integer division for DIV.

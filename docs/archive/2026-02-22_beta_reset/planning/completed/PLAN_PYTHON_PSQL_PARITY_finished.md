# Plan: Python-to-PSQL Parity (Operators/Functions)

Status: Draft (Alpha). Goal is full Python functionality coverage using Firebird/V2-style
syntax (operators and functions), not Python syntax.

## Scope
Target items from `ScratchBird/docs/findings/V2_PYTHON_OPERATOR_TYPE_GAP_ANALYSIS.md`:
- DIV integer division operator.
- STARTING WITH and CONTAINING predicates.
- REPLACE, ENDS_WITH, LEAST/GREATEST.
- ARRAY_POSITION and ARRAY_SLICE (array slicing support).
- JSON_EXISTS / JSON_HAS_KEY for dict-style membership.
- TO_CHAR / TO_DATE / TO_TIMESTAMP formatting/parsing helpers.
- Native UNNEST (table-returning) behavior.

Out of scope:
- Python syntax parity (e.g., `//`, `startswith`) in SQL.
- Emulated dialect changes unless needed for parity.

## Dependencies
- SBLR opcode registry updates (planned/reserved values).
- Parser token and grammar coverage for new keywords and function names.
- Executor support for new opcodes and table-returning functions.

## Work Breakdown

### 1) SBLR Opcode Definitions
- [x] Add planned opcodes to `ScratchBird/include/scratchbird/sblr/opcodes.h`.
  - EXT_EXPR_DIV_INT
  - EXT_PRED_STARTING_WITH
  - EXT_PRED_CONTAINING
  - EXT_FUNC_REPLACE
  - EXT_FUNC_ENDS_WITH
  - EXT_FUNC_ARRAY_POSITION
  - EXT_ARRAY_SLICE
  - EXT_FUNC_JSON_EXISTS
  - EXT_FUNC_JSON_HAS_KEY
  - EXT_FUNC_TO_CHAR
  - EXT_FUNC_TO_DATE
  - EXT_FUNC_TO_TIMESTAMP
  - EXT_FUNC_LEAST
  - EXT_FUNC_GREATEST
- [x] Keep `ScratchBird/docs/specifications/sblr/SBLR_OPCODE_REGISTRY.md` in sync.

### 2) Parser (V2)
- [x] Lexer: add keywords `DIV`, `STARTING`, `CONTAINING`.
  - File: `ScratchBird/src/parser/lexer_v2.cpp`.
- [x] Parser expressions:
  - [x] `expr DIV expr` -> EXT_EXPR_DIV_INT.
  - [x] `expr [NOT] STARTING WITH expr` -> EXT_PRED_STARTING_WITH.
  - [x] `expr [NOT] CONTAINING expr` -> EXT_PRED_CONTAINING.
  - Files: `ScratchBird/src/parser/parser_v2.cpp`, `ScratchBird/src/parser/ast_v2.cpp`.
- [x] Function recognition:
  - Map REPLACE/ENDS_WITH/ARRAY_POSITION/ARRAY_SLICE/JSON_EXISTS/JSON_HAS_KEY/
    TO_CHAR/TO_DATE/TO_TIMESTAMP/LEAST/GREATEST to new opcodes.
  - Files: `ScratchBird/src/parser/parser_v2.cpp`, `ScratchBird/src/parser/parser_state_v2.cpp`.
- [x] Array slice syntax `expr[lo:hi]` -> EXT_ARRAY_SLICE
  - File: `ScratchBird/src/parser/ast_v2.cpp` (ensure AST node supports slice).

### 3) Bytecode Generator
- [x] Emit new opcodes for the added operators/functions.
- [x] Map `expr[lo:hi]` to EXT_ARRAY_SLICE payload.
  - File: `ScratchBird/src/sblr/bytecode_generator_v2.cpp`.

### 4) Executor
- [x] Implement new extended opcodes in `ScratchBird/src/sblr/executor.cpp`.
  - DIV: integer division (truncate toward zero; division-by-zero error).
  - STARTING WITH: prefix match using collation-aware comparison.
  - CONTAINING: substring match with case-insensitive semantics per collation.
  - REPLACE: replace all occurrences of `search` in `str`.
  - ENDS_WITH: suffix match, collation-aware.
  - ARRAY_POSITION: 1-based index; NULL if not found.
  - ARRAY_SLICE: inclusive bounds, NULL allowed for open-ended slice.
  - JSON_EXISTS / JSON_HAS_KEY: path or key existence predicates.
  - TO_CHAR/TO_DATE/TO_TIMESTAMP: format parse/format using a Firebird-style format
    token set defined in spec.
  - LEAST/GREATEST: type promotion, ignore NULLs unless all NULL.
- [x] Native UNNEST:
  - Implement `Opcode::UNNEST` table-returning behavior (expand to rows) instead of
    returning arrays.
  - Consolidate existing `STRING_TO_TABLE` and `REGEXP_SPLIT_TO_TABLE` to follow the
    same table-returning execution path.

### 5) Tests
- [x] Unit tests for new opcodes in `ScratchBird/tests/unit`.
- [x] SQL parsing tests in `ScratchBird/tests/sql`.
- [ ] End-to-end tests (SELECT/WHERE predicates, array slices, JSON_EXISTS).

## Validation Checklist
- Parser accepts all new syntax in V2.
- Bytecode validator accepts new opcodes.
- Executor produces correct results under NULL semantics and collation rules.
- Documentation updated for user-facing syntax and migration mapping.

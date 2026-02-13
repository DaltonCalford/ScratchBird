# V3 PSQL Runtime Semantics Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/PSQL_RUNTIME_V3.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
Core PSQL opcodes (BLOCK, DECLARE, ASSIGN, IF/WHILE/LOOP, FOR SELECT/EXECUTE, RAISE, CURSOR ops) exist in the V3 executor. Variable frames support nested scope lookup, and domain-typed assignment performs coercion + validation. However, several normative requirements are missing or differ: declaration ordering is not enforced, loop/handler scope semantics are incomplete, sub-savepoints are not used for exceptions, and cursor close-on-closed is treated as an error rather than a no-op.

## Findings by Spec Item

### 1. Execution Model
- [~] PSQL executes within routine frame with local variables and handlers.
  - V3 executor handles `SBLR3_BLOCK`, `SBLR3_DECLARE`, `SBLR3_ASSIGN`, `SBLR3_RAISE`, cursor ops. See `src/sblr/executor.cpp:50354-51490`.

### 2. Variable Scoping (Normative)
- [~] Nested scope lookup implemented via `VariableStack` (inner → outer).
  - `VariableStack::getVariable` searches frames from innermost to outermost. See `src/sblr/executor.cpp:39363-39390`.
- [ ] Declaration ordering (DECLARE before executable) not enforced.
  - `SBLR3_DECLARE` can be emitted/executed anywhere; no enforcement in executor.
- [ ] Loop-scope variables not implemented.
  - No frame push for loops; only block frames are used. See `SBLR3_WHILE`/`SBLR3_LOOP` handling `src/sblr/executor.cpp:50780-51080`.
- [ ] Handler-scope locals not implemented.
  - Handler execution pushes a frame for SQLSTATE/SQLERRM only; no handler-local declarations.
- [ ] “Shadowing must be explicit” not enforced.
  - Duplicate names are rejected only within the same frame; inner scopes can silently shadow.

### 3. Assignment and Type Semantics
- [~] ASSIGN performs type coercion and domain validation.
  - `assignPsqlVariable` calls `coercePsqlValue`, normalization, and validation. See `src/sblr/executor.cpp:17260-17370`.
- [ ] NULL assignment to NOT NULL raises exception (implemented for domain vars; behavior for non-domain typed vars depends on nullable flag in `VariableEntry` but not fully verified).

### 4. Exception Propagation (Normative)
- [~] Exception handlers exist and are matched by SQLSTATE/name/ANY.
  - `SBLR3_BLOCK` handles `exception_handlers` list and matches by SQLSTATE/name/ANY. See `src/sblr/executor.cpp:50354-50610`.
- [ ] Sub-savepoint per PSQL statement not implemented.
  - No implicit savepoint/rollback logic observed for PSQL statements on exception.
- [ ] Handler re-raise semantics not explicitly implemented.

### 5. Cursor Lifecycle (Normative)
- [~] CURSOR_DECLARE/OPEN/FETCH/CLOSE implemented with basic lifecycle checks.
  - See `src/sblr/executor.cpp:51315-51490`.
- [ ] Close on already-closed cursor should be no-op unless strict; current behavior errors.
  - `executeCursorClose` errors if cursor not open.

### 6. Loop Semantics (Normative)
- [~] WHILE/LOOP/FOR SELECT/FOR EXECUTE implemented; EXIT/CONTINUE supported.
  - See `src/sblr/executor.cpp:50780-51220`.
- [ ] Loop-scope variable lifetime not implemented (no frame management per loop).

### 7. Control Flow Semantics
- [~] IF/ELSIF/ELSE and JUMP/labels exist; NULL treated as false via `toBoolean()`.
  - IF/WHILE use `toBoolean()`. See `src/sblr/executor.cpp:50587-50760`.
- [ ] “Jumping into deeper scope invalid” not enforced.

### 8. Determinism and Side Effects
- [ ] Not verified.

## Notes
- PSQL exception handling exists in two mechanisms: V3 `exception_handlers` on `SBLR3_BLOCK` and legacy TRY/EXCEPT machinery. Savepoint behavior is not implemented in the V3 path.

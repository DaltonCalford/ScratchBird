# PSQL Statements Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/PSQL_STATEMENTS.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
PSQL statement dispatch is implemented in `parsePSQLStatement`, covering control flow, cursor ops, exception handling, assignment, calls, and embedded DML. The ordering is mostly aligned but does not explicitly handle TRY/EXCEPT in the dispatch, and assignment parsing accepts `=` in addition to `:=`.

## Findings by Spec Item

### Statement Categories / Dispatch Rules
- [~] Control flow: IF/CASE/WHILE/FOR/LOOP handled first.
  - See `src/parser/parser_v3.cpp:11018-11040`.
- [~] Cursor control: DECLARE/OPEN/FETCH/CLOSE handled before assignment and DML.
  - See `src/parser/parser_v3.cpp:11064-11090`.
- [~] Exception handling: EXCEPTION handled; TRY/EXCEPT not explicitly dispatched here.
  - EXCEPTION uses `parseExceptionStatement` (`src/parser/parser_v3.cpp:11056-11062`). TRY/EXCEPT parsing not observed in this dispatch.
- [~] Assignment or CALL handled before embedded DML.
  - Assignment uses `:=` or `=`; CALL via `parseCall` is reached via EXECUTE/CALL handling. See `src/parser/parser_v3.cpp:11083-11102`.
- [~] Embedded DML (SELECT/INSERT/UPDATE/DELETE) handled last.

### SBLR Emission
- [~] PSQL opcodes are emitted by `emitPsql` in `v3_emitter.cpp`.
  - Coverage includes BLOCK/DECLARE/ASSIGN/IF/WHILE/LOOP/FOR/RAISE/CURSOR ops/POST_EVENT/CALL. See `src/parser/v3_emitter.cpp:2680-2965`.

## Notes
- Spec references `TRY/EXCEPT`; parsing/emission for TRY/EXCEPT in V3 PSQL was not found in this dispatch and should be verified in dedicated TRY/EXCEPT specs if any exist.

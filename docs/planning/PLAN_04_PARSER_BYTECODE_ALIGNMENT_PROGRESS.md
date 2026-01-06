# Plan 04 Parser Bytecode Alignment Progress (2026-01-06)

## Scope
Align emulated dialect parsers (PostgreSQL/MySQL/Firebird) to the executor's v1 SBLR bytecode format, document executor gaps, and track what remains for full dialect parity.

## Work Completed
- PostgreSQL DML aligned to v1 SBLR emission (SELECT/INSERT/UPDATE/DELETE) with executor-compatible layouts.
- MySQL DML aligned to v1 SBLR emission (SELECT/INSERT/UPDATE/DELETE) with executor-compatible layouts.
- DEFAULT/GENERATED column expressions captured as SBLR bytecode for PostgreSQL/MySQL CREATE TABLE.
- PostgreSQL partial index predicates emitted as SBLR bytecode; executor now falls back to SBLR predicate evaluation if expression deserialization fails.
- Parser non-emitting paths hardened to avoid bytecode patching when emit is disabled (fixes parser test crashes).
- Test target cycle removed from `tests/CMakeLists.txt`.

## Executor Gaps (Documented for Follow-up)
- DML extras not supported: `ON CONFLICT`, `UPDATE ... FROM`, `DELETE ... USING`, `INSERT ... SELECT`, multi-row `VALUES`, `RETURNING`.
- SELECT limitations: DISTINCT not handled, SELECT-list expressions with FROM not supported, ORDER BY/GROUP BY expressions limited to column refs.
- DML ORDER BY/LIMIT for UPDATE/DELETE not supported.
- JOIN emission not wired for emulated parsers (executor supports v1 join opcodes, dialect parsers currently skip join emission).

## Remaining Work
- Firebird parser alignment:
  - Decide v2 AST pipeline vs. direct v1 SBLR emission for DDL/DML.
  - Implement CREATE INDEX/VIEW/DDL payloads matching executor expectations.
- Executor feature additions (if parity is required):
  - Support ON CONFLICT, UPDATE FROM, DELETE USING, INSERT SELECT, multi-row VALUES, RETURNING.
  - Support SELECT DISTINCT and SELECT-list expressions with FROM.
  - Support ORDER BY/GROUP BY expressions beyond column refs.
- Dialect parity tests (Firebird -> MySQL -> PostgreSQL) once executor gaps are closed.

## Tests Run
- `cmake --build build`
- `ctest --test-dir build` (2,007 passed, 42 skipped: network/socket gating)

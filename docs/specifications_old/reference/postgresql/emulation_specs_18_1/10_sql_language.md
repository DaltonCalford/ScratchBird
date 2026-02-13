# PostgreSQL 18.1 SQL — Draft 1

## Authoritative Grammar
- `src/backend/parser/gram.y` (copied to `emulation_specs/postgresql-18.1/sql_grammar_full.y`).
- Lexer: `src/backend/parser/scan.l`.

## Semantics
- Semantic analysis and query transform: `src/backend/parser/*`.

## PL/pgSQL
- Implementation in `src/pl/plpgsql/src/*`.

## Compliance Rule
If any SQL detail is ambiguous, follow the source copies exactly.

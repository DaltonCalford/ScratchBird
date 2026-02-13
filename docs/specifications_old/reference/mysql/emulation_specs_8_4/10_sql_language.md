# MySQL 8.4 SQL — Draft 1

## Authoritative Grammar
The full grammar is defined in:
- `source_copies/sql/sql_yacc.yy`
A direct copy is provided at:
- `emulation_specs/mysql-8.4/sql_grammar_full.yy`

## Lexer and Keywords
- `source_copies/sql/sql_lex.cc` defines lexical rules, keyword tables, and SQL mode effects.

## Semantic Rules
- `source_copies/sql/*` defines semantic analysis, name resolution, privilege checks, and execution behavior.

## Stored Programs
- Stored procedures/functions/triggers/events are implemented in `source_copies/sql/sp_*` and related parser productions in `sql_yacc.yy`.

## SQL Modes
- SQL mode parsing and effects are implemented under `source_copies/sql/` (e.g., `sql_lex.cc`, `sql_class.cc`, `sql_error.cc`).

## Compatibility Rule
If any SQL detail is ambiguous, follow the source copies exactly.

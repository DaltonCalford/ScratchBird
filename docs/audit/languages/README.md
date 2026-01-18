# SQL Language Audit (Parser Surfaces)

Purpose: consolidated, code-derived SQL surface documentation for the four parsers
(native V2, FirebirdSQL, PostgreSQL, MySQL). Each language is broken down by
object lifecycle (DDL), DML, programmable SQL, transactions, security, session
controls (SHOW/SET), utilities, and operators.

Status: static code review snapshot; no runtime execution performed. The
implementation source code is the only source of truth; documentation and specs
may be ahead or behind.

## Delta since 2026-01-07 audit
- V2 PSQL + CTE parsing implemented and wired through semantic + bytecode paths.
- Temporary table semantics implemented across parsers (temp types + ON COMMIT).
- TEMPORARY VIEW/SEQUENCE now session-scoped temp metadata; UNLOGGED warns under MGA.
- V2 index type coverage completed for all 11 index types (AST/parser/semantic/bytecode/tests).
- PG/MySQL bytecode alignment has partial fixes applied; remaining mismatches still tracked.
- Phase 0 verification (2026-01-14) enumerated remaining parser->executor deferrals; see
  `docs/audit/parsers/CRITICAL_FINDINGS.md`.

## Status legend
- Implemented: parser + pipeline/executor align; statement should execute.
- Partial: statement parses, but options/semantics are limited.
- Stubbed: statement parses/emits bytecode, but executor format/handler mismatch
  prevents execution.
- Missing: parser rejects or does not implement the statement.

## Language directories
- native: ScratchBird V2 parser (core SQL).
- firebirdsql: Firebird emulation parser.
- postgresql: PostgreSQL emulation parser.
- mysql: MySQL emulation parser.

Each language folder contains a README with an index and scope notes.

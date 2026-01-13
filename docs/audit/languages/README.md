# SQL Language Audit (Parser Surfaces)

Purpose: consolidated, code-derived SQL surface documentation for the four parsers
(native V2, FirebirdSQL, PostgreSQL, MySQL). Each language is broken down by
object lifecycle (DDL), DML, programmable SQL, transactions, security, session
controls (SHOW/SET), utilities, and operators.

Status: static code review snapshot; no runtime execution performed. The
implementation source code is the only source of truth; documentation and specs
may be ahead or behind.

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

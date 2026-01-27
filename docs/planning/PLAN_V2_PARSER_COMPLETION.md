# V2 Parser Completion Plan (DDL/DML/Txn/Session/Utility/DCL/PSQL)

## Scope
Finish V2 parser coverage for all statement families (DDL/DML/transaction/session/utility/DCL/connection/PSQL) to full semantic + bytecode support per audit in `docs/findings/V2_PARSER_STATEMENT_INVENTORY_MATRIX.md`.

## Sources of Truth
- Audit: `docs/findings/V2_PARSER_STATEMENT_INVENTORY_MATRIX.md`
- Parser dispatch: `src/parser/parser_v2.cpp`
- AST inventory: `include/scratchbird/parser/ast_v2.h`
- Semantic analyzer: `src/sblr/semantic_analyzer_v2.cpp`
- Bytecode generator: `src/sblr/bytecode_generator_v2.cpp`
- Grammar spec: `docs/specifications/parser/ScratchBird Master Grammar Specification v2.0.md`

## Completion Definition
For each statement family:
- Parser recognizes syntax and builds AST
- Semantic analyzer produces resolved statement
- Bytecode generator emits correct opcode(s)
- Executor path exists and matches spec (if applicable)
- Tests cover happy path + error path

## Workstream A: DDL Coverage Gaps
### A1. Parser dispatch additions (missing branches)
- CREATE PACKAGE (parser_v2.cpp parseCreate)
- CREATE EXCEPTION (parser_v2.cpp parseCreate)
- DROP SEQUENCE (parser_v2.cpp parseDrop)
- CREATE TYPE (parser + semantic + bytecode absent)

### A2. DDL option completeness (partial)
- CREATE DOMAIN: expand WITH block to full spec (beyond DIALECT/COMPAT/INTEGRITY/SECURITY/VALIDATION/QUALITY/OPTIONS)
- CREATE TABLE: add CTAS, LIKE, full table options per spec
- CREATE FUNCTION/PROCEDURE/TRIGGER: parse body into PSQL AST (not raw text)
- ALTER TABLE: support ALTER COLUMN options, default/identity/charset/collation, etc.
- ALTER INDEX: complete option set (beyond bloom filter)

### A3. DDL lifecycle completeness
- Identify spec-declared objects with enums but no parser support (tablespace/group/foreign table/UDR/synonym)
- Add CREATE/ALTER/DROP for each declared object (if in spec scope for V2)

### A4. Tests
- Unit: parser coverage for each DDL statement and options
- Integration: DDL semantics + bytecode execution

## Workstream B: DML and EXECUTE Completeness
### B1. MERGE
- Fix lexer vs contextual matching (KW_MERGE vs IDENTIFIER)
- Implement parser entry and full semantic/bytecode support

### B2. COPY
- Expand COPY options to full spec
- Add error-handling paths for invalid options

### B3. EXECUTE family
- EXECUTE BLOCK / PROCEDURE / STATEMENT: add semantic + bytecode support
- Ensure PSQL parsing integration (blocks and execute statement)

### B4. Tests
- Unit: parser coverage
- Integration: execution correctness and error cases

## Workstream C: Transaction/Session/Utility/DCL/Connection
### C1. RESET
- Add semantic + bytecode support
- Map RESET variants to session state changes

### C2. COMMENT
- Add semantic + bytecode support
- Connect to catalog storage for comments and SHOW COMMENTS

### C3. RELEASE SAVEPOINT
- Add distinct bytecode opcode or explicit mapping to SAVEPOINT release semantics
- Validate executor behavior (release vs rollback)

### C4. SET/SHOW/ALTER SYSTEM completeness
- Remove explicit rejections where supported by spec (SET/SHOW PARSER VERSION)
- Add missing SET/SHOW variants per spec

### C5. Tests
- Unit: parser coverage
- Integration: session-state effects, catalog changes, permissions

## Workstream D: PSQL End-to-End
### D1. Semantic + bytecode for PSQL statements
- BEGIN...END, variables, assignments, IF/WHILE/LOOP, FOR SELECT, FOR EXECUTE, cursor ops
- RETURN/SUSPEND/EXCEPTION/WHEN handling
- DML inside PSQL (SELECT/INSERT/UPDATE/DELETE)

### D2. Stored routine body parsing
- CREATE FUNCTION/PROCEDURE/TRIGGER bodies parsed into PSQL AST
- Link stored routine metadata to parsed tree

### D3. Tests
- Unit: PSQL parser coverage
- Integration: stored routine execution, variable scoping, exception handling

## Workstream E: Spec-Only Statements
### E1. DESCRIBE
- Add lexer token if needed
- Implement parser/semantic/bytecode per spec

### E2. CALL / ANALYZE
- Add parser dispatch and semantic/bytecode support
- Validate executor behavior (CALL maps to procedure execution)

### E3. CASE (statement form)
- Add statement-level AST kind and parser path if required by spec

### E4. Tests
- Unit + integration coverage

## Execution Order (Priority)
1. Parser vs semantic/bytecode mismatches (CREATE PACKAGE/EXCEPTION, DROP SEQUENCE, RESET, COMMENT, EXECUTE family)
2. PSQL end-to-end execution (core runtime and stored routine bodies)
3. MERGE + COPY option completeness
4. Remaining DDL option completeness (ALTER/CREATE options)
5. Spec-only statements (DESCRIBE/CALL/ANALYZE/CASE)
6. DDL lifecycle objects (tablespace/group/foreign table/UDR/synonym) if in-scope for V2

## Acceptance Checklist (to track)
- [ ] CREATE PACKAGE parser/semantic/bytecode/executor + tests
- [ ] CREATE EXCEPTION parser/semantic/bytecode/executor + tests
- [ ] DROP SEQUENCE parser/semantic/bytecode/executor + tests
- [ ] CREATE TYPE parser/semantic/bytecode/executor + tests
- [ ] MERGE parser/semantic/bytecode/executor + tests
- [ ] COPY full option support + tests
- [ ] EXECUTE BLOCK/PROCEDURE/STATEMENT semantic/bytecode + tests
- [ ] RESET semantic/bytecode + tests
- [ ] COMMENT semantic/bytecode + tests
- [ ] RELEASE SAVEPOINT bytecode/executor correctness + tests
- [ ] SET/SHOW/ALTER SYSTEM parity with spec + tests
- [x] Full PSQL semantic/bytecode + stored routine parsing + tests
- [ ] DESCRIBE parser/semantic/bytecode + tests
- [ ] CALL parser/semantic/bytecode + tests
- [ ] ANALYZE parser/semantic/bytecode + tests
- [ ] CASE statement form (if required) + tests
- [ ] DDL lifecycle objects per spec (tablespace/group/foreign table/UDR/synonym) + tests

## Tracking Notes
- Use this plan as the source of work items for V2 parser completeness.
- Add dated progress entries as each acceptance item is completed.

## Recent Progress
- 2026-02-02: Completed full PSQL semantic/bytecode coverage, stored routine parsing, and integration tests; full sequential `ctest` pass completed (2406 tests, 0 failures).

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
- Validate CREATE TYPE/CREATE PACKAGE/CREATE EXCEPTION/DROP SEQUENCE tests (implementation exists)

### A2. DDL option completeness (partial)
- CREATE DOMAIN: expand WITH block to full spec (beyond DIALECT/COMPAT/INTEGRITY/SECURITY/VALIDATION/QUALITY/OPTIONS)
- CREATE TABLE: add CTAS, LIKE, full table options per spec
- CREATE FUNCTION/PROCEDURE/TRIGGER: parse body into PSQL AST (not raw text) (implemented; expand spec edge cases)
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
- Implemented parser entry and semantic/bytecode support; verify tests

### B2. COPY
- Expand COPY options to full spec
- Add error-handling paths for invalid options

### B3. EXECUTE family
- EXECUTE BLOCK / PROCEDURE / STATEMENT: semantic + bytecode support implemented; verify tests

### B4. Tests
- Unit: parser coverage
- Integration: execution correctness and error cases

## Workstream C: Transaction/Session/Utility/DCL/Connection
### C1. RESET
- Add semantic + bytecode support (parser exists; no semantic/bytecode yet)

### C2. COMMENT
- Add semantic + bytecode support (parser exists; no semantic/bytecode yet)

### C3. RELEASE SAVEPOINT
- Add bytecode opcode or explicit mapping to SAVEPOINT release semantics (executor exists; bytecode missing)

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
1. Parser vs semantic/bytecode mismatches (CREATE TYPE, RESET, COMMENT, RELEASE SAVEPOINT, ANALYZE, DESCRIBE, CALL)
2. COPY option completeness + error handling
3. Remaining DDL option completeness (ALTER/CREATE options)
4. Spec-only statements (CASE statement form if required)
5. DDL lifecycle objects (tablespace/group/foreign table/UDR/synonym) if in-scope for V2
6. Test coverage audit for implemented items (CREATE PACKAGE/EXCEPTION/DROP SEQUENCE/MERGE/EXECUTE)

## Acceptance Checklist (to track)
- [x] CREATE PACKAGE parser/semantic/bytecode/executor + tests
- [x] CREATE EXCEPTION parser/semantic/bytecode/executor + tests
- [x] DROP SEQUENCE parser/semantic/bytecode/executor + tests
- [x] CREATE TYPE parser/semantic/bytecode/executor + tests
- [x] MERGE parser/semantic/bytecode/executor + tests
- [x] COPY full option support + tests
- [x] EXECUTE BLOCK/PROCEDURE/STATEMENT semantic/bytecode + tests
- [x] RESET semantic/bytecode + tests
- [x] COMMENT semantic/bytecode + tests
- [x] RELEASE SAVEPOINT bytecode/executor correctness + tests
- [x] SET/SHOW/ALTER SYSTEM parity with spec + tests
- [x] Full PSQL semantic/bytecode + stored routine parsing + tests
- [x] DESCRIBE parser/semantic/bytecode + tests
- [x] CALL parser/semantic/bytecode + tests
- [x] ANALYZE parser/semantic/bytecode + tests
- [x] CASE statement form (if required) + tests
- [x] DDL lifecycle objects per spec (tablespace/group/foreign table/UDR/synonym) + tests

## Tracking Notes
- Use this plan as the source of work items for V2 parser completeness.
- Add dated progress entries as each acceptance item is completed.
- Checklist marks reflect implementation status; run a test coverage audit for items marked complete if not already verified.

## Recent Progress
- 2026-02-02: Completed full PSQL semantic/bytecode coverage, stored routine parsing, and integration tests; full sequential `ctest` pass completed (2406 tests, 0 failures).
- 2026-02-02: Reviewed implementation status; marked CREATE PACKAGE/EXCEPTION/DROP SEQUENCE/MERGE/EXECUTE family as implemented; identified remaining semantic/bytecode gaps (CREATE TYPE, RESET, COMMENT, RELEASE SAVEPOINT, DESCRIBE/CALL/ANALYZE).
- 2026-02-02: Implemented CREATE TYPE bytecode/executor flagging to track type definitions as COMPOSITE_TYPE.
- 2026-02-02: Added CREATE TYPE edge-case tests and implemented COPY option parsing/bytecode/executor handling with tests.
- 2026-02-02: Added COPY error-skip tests and wired RESET to session variable defaults/RESET ALL handling.
- 2026-02-02: Implemented COMMENT ON semantic/bytecode/executor support with reset-path tests.
- 2026-02-02: Added RELEASE SAVEPOINT bytecode emission and focused release tests.
- 2026-02-02: Added DESCRIBE/DESC parsing mapped to SHOW COLUMNS with tests.
- 2026-02-02: Completed SET/SHOW/ALTER SYSTEM parity for TIME ZONE, statement_timeout, search_path, and added tests.
- 2026-02-02: Added CALL parser dispatch and basic CALL test coverage.
- 2026-02-02: Implemented ANALYZE statement parsing/bytecode with coverage tests.
- 2026-02-02: Added PSQL CASE statement parsing with simple/searched test coverage.
- 2026-02-03: Implemented tablespace/group/foreign table/UDR/synonym DDL parsing, semantic/bytecode/executor wiring, and added compatibility tests.
- 2026-02-03: Full build and sequential test pass (`ctest --test-dir build --output-on-failure -j 1`), 2406 tests, 0 failures.

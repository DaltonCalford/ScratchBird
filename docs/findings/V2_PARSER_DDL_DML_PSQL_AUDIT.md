# V2 Parser DDL/DML/PSQL Audit (Missing + Partial Coverage)

## Scope
- Parser implementation (read-only):
  `ScratchBird/src/parser/parser_v2.cpp`,
  `ScratchBird/src/parser/parser_state_v2.cpp`,
  `ScratchBird/include/scratchbird/parser/lexer_v2.h`,
  `ScratchBird/include/scratchbird/parser/ast_v2.h`
- V2 grammar spec:
  `ScratchBird/docs/specifications/parser/ScratchBird Master Grammar Specification v2.0.md`

## Implemented (code-truth summary)
- Statement dispatch covers CREATE/ALTER/DROP/TRUNCATE; SELECT/INSERT/UPDATE/DELETE/COPY; transaction; session (SET/SHOW/RESET); utility (EXPLAIN/SWEEP); DCL (GRANT/REVOKE); connect/disconnect; COMMENT; EXECUTE (block/procedure/statement); job DDL; PSQL blocks and control flow.
  - Code: `ScratchBird/src/parser/parser_v2.cpp:159-226`
- PSQL block/flow statements (IF/WHILE/FOR/LOOP/LEAVE/CONTINUE/EXIT/SUSPEND/RETURN/WHEN, cursor DECLARE/OPEN/FETCH/CLOSE, EXECUTE BLOCK/STATEMENT/PROCEDURE).
  - Code: `ScratchBird/src/parser/parser_v2.cpp:7677-8288`

## Missing or Partial vs Spec

### F-V2P-001 MERGE is effectively unreachable (gatekeeper mismatch)
- Lexer emits KW_MERGE as a reserved token, but parser dispatch uses `matchContextual("MERGE")` which only matches IDENTIFIER tokens.
  - Code: `ScratchBird/include/scratchbird/parser/lexer_v2.h:124-148`
  - Code: `ScratchBird/src/parser/parser_state_v2.cpp:70-88`
  - Code: `ScratchBird/src/parser/parser_v2.cpp:223-224`
- Status: Missing (MERGE statements never enter the parser).

### F-V2P-002 CALL/ANALYZE/DESCRIBE statements are declared but not parsed
- `KW_CALL` and `KW_ANALYZE` are reserved tokens, yet no dispatch exists in `parseStatementInternal`.
  - Code: `ScratchBird/include/scratchbird/parser/lexer_v2.h:124-148`
  - Code: `ScratchBird/src/parser/parser_v2.cpp:159-209`
- The grammar spec lists `DESCRIBE` as a gatekeeper keyword, but there is no lexer token or parser branch for it.
  - Spec: `ScratchBird/docs/specifications/parser/ScratchBird Master Grammar Specification v2.0.md:30-35`
  - Code: `ScratchBird/include/scratchbird/parser/lexer_v2.h:120-170`
  - Code: `ScratchBird/src/parser/parser_v2.cpp:159-226`
- Status: Missing.

### F-V2P-003 CREATE PACKAGE / CREATE EXCEPTION / CREATE TYPE are absent
- AST defines `CreatePackageStmt` and `CreateExceptionStmt`, and ASTKind includes `CreateTypeStmt`, but parseCreate never dispatches PACKAGE/EXCEPTION/TYPE.
  - AST: `ScratchBird/include/scratchbird/parser/ast_v2.h:52-90`, `ScratchBird/include/scratchbird/parser/ast_v2.h:754-803`
  - Parser: `ScratchBird/src/parser/parser_v2.cpp:290-397`
- Status: Missing.

### F-V2P-004 Tablespace DDL is missing (create/drop/alter beyond rename/move)
- DDL object types include TABLESPACE, but parser only supports rename/move through ALTER, with no CREATE/DROP TABLESPACE.
  - AST: `ScratchBird/include/scratchbird/parser/ast_v2.h:1157-1176`
  - Parser (ALTER rename/move only): `ScratchBird/src/parser/parser_v2.cpp:2942-2944`
  - Parser (no CREATE/DROP tablespace): `ScratchBird/src/parser/parser_v2.cpp:290-397`, `ScratchBird/src/parser/parser_v2.cpp:3587-3611`
- Status: Missing.

### F-V2P-005 Sequence lifecycle is incomplete (no DROP, no ALTER options)
- CREATE SEQUENCE exists, but DROP SEQUENCE is not dispatched and ALTER SEQUENCE only supports rename/move via generic ALTER.
  - AST has DropSequenceStmt: `ScratchBird/include/scratchbird/parser/ast_v2.h:1327-1339`
  - Parser CREATE: `ScratchBird/src/parser/parser_v2.cpp:1908-1977`
  - Parser DROP dispatch (no sequence): `ScratchBird/src/parser/parser_v2.cpp:3587-3603`
  - Parser ALTER (rename/move only): `ScratchBird/src/parser/parser_v2.cpp:2937-2944`
- Status: Missing/Partial.

### F-V2P-006 Users/Roles/Groups lifecycle is incomplete
- CREATE USER exists but there is no DROP USER and no ALTER USER options.
  - Parser CREATE USER: `ScratchBird/src/parser/parser_v2.cpp:404-435`
  - Parser DROP dispatch (no USER): `ScratchBird/src/parser/parser_v2.cpp:3587-3603`
  - Parser ALTER (rename/move only): `ScratchBird/src/parser/parser_v2.cpp:2944-2946`
- CREATE ROLE exists; DROP ROLE exists; ALTER ROLE is rename/move only (no option changes).
  - Parser CREATE ROLE: `ScratchBird/src/parser/parser_v2.cpp:438-445`
  - Parser DROP ROLE: `ScratchBird/src/parser/parser_v2.cpp:3600-3602`
  - Parser ALTER ROLE (rename/move only): `ScratchBird/src/parser/parser_v2.cpp:2944-2945`
- GROUP is supported only for rename/move; no CREATE/DROP GROUP.
  - AST: `ScratchBird/include/scratchbird/parser/ast_v2.h:1157-1176`
  - Parser ALTER GROUP: `ScratchBird/src/parser/parser_v2.cpp:2946`
- Status: Missing/Partial.

### F-V2P-007 ALTER VIEW/PROCEDURE/FUNCTION/TRIGGER are rename/move only
- ALTER dispatch only supports rename/move for these object types; no ALTER ... AS / ALTER ... SET option parsing exists.
  - Parser: `ScratchBird/src/parser/parser_v2.cpp:2821-2944`
- Status: Partial.

### F-V2P-008 Foreign objects and synonyms have no DDL coverage
- DDL object types list FOREIGN_TABLE, UDR, and SYNONYM but there are no CREATE/ALTER/DROP branches.
  - AST: `ScratchBird/include/scratchbird/parser/ast_v2.h:1157-1176`
  - Parser dispatch lacks these: `ScratchBird/src/parser/parser_v2.cpp:290-397`, `ScratchBird/src/parser/parser_v2.cpp:2773-2949`, `ScratchBird/src/parser/parser_v2.cpp:3587-3611`
- Status: Missing.

### F-V2P-009 ALTER TABLE lacks ALTER COLUMN semantics
- AST defines `AlterTableAction::ALTER_COLUMN`, but parser never branches into it (only ADD/DROP/RENAME, SET TABLESPACE/SCHEMA, ENABLE/DISABLE RLS).
  - AST: `ScratchBird/include/scratchbird/parser/ast_v2.h:1185-1204`
  - Parser: `ScratchBird/src/parser/parser_v2.cpp:3479-3577`
- Status: Missing.

### F-V2P-010 ALTER INDEX option coverage is minimal
- ALTER INDEX SET only accepts BLOOM_FILTER/BLOOM_FPR (no per-index-type options, rebuild, or storage parameters).
  - Parser: `ScratchBird/src/parser/parser_v2.cpp:2821-2935`
- Status: Partial.

### F-V2P-011 CREATE TABLE does not support CTAS/LIKE
- Parser requires a column-definition `(` immediately after table name, so `CREATE TABLE AS SELECT` and `CREATE TABLE ... LIKE` are not accepted.
  - Code: `ScratchBird/src/parser/parser_v2.cpp:785-865`
- Status: Missing.

### F-V2P-012 WITH (CTE) support is SELECT-only and limited to SELECT/INSERT/UPDATE/DELETE
- WITH clause dispatch rejects MERGE or other statements; CTE bodies require SELECT only (no data-modifying CTEs).
  - Code: `ScratchBird/src/parser/parser_v2.cpp:3950-3978`, `ScratchBird/src/parser/parser_v2.cpp:4019-4024`
- Status: Partial.

### F-V2P-013 SELECT/UPDATE/DELETE cursor semantics are missing in AST
- SELECT `FOR UPDATE OF` is parsed but table list is discarded because AST lacks support.
  - Code: `ScratchBird/src/parser/parser_v2.cpp:4128-4151`
- UPDATE/DELETE `ONLY` and `WHERE CURRENT OF` are explicitly skipped.
  - Code: `ScratchBird/src/parser/parser_v2.cpp:4731-4774`, `ScratchBird/src/parser/parser_v2.cpp:4818-4859`
- Status: Partial.

### F-V2P-014 COPY options are limited to a subset
- Unsupported options cause a hard error; only FORMAT/DELIMITER/NULL/HEADER/QUOTE/ESCAPE/ENCODING are parsed.
  - Code: `ScratchBird/src/parser/parser_v2.cpp:4968-5040`
- Status: Partial.

### F-V2P-015 CREATE DOMAIN WITH blocks are limited
- Unknown WITH block types error out; only DIALECT/COMPAT/INTEGRITY/SECURITY/VALIDATION/QUALITY/OPTIONS are supported.
  - Code: `ScratchBird/src/parser/parser_v2.cpp:2504-2533`
- Status: Partial.

### F-V2P-016 SET/SHOW PARSER VERSION intentionally unsupported
- Parser accepts SET/SHOW PARSER VERSION but returns errors.
  - Code: `ScratchBird/src/parser/parser_v2.cpp:6841-6847`, `ScratchBird/src/parser/parser_v2.cpp:7100-7104`
- Status: Missing (explicitly unsupported).

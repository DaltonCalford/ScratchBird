# Plan 04 - Parser Coverage and Compatibility

## Scope
Complete V2 (ScratchBird) parser coverage and fill compatibility gaps for Firebird/MySQL/PostgreSQL parsers. Includes ScratchBird transaction control grammar (SQL-standard + Firebird legacy), autocommit toggles, and conflict-action syntax. This plan is parser + semantic + bytecode emission only (executor changes are in other plans).

## Priority
P1 (blocks SQL feature coverage and compatibility).

## References
- `docs/specifications/01_SQL_DIALECT_OVERVIEW.md`
- `docs/specifications/ScratchBird SQL Language Specification - Master Document.md`
- `docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
- `docs/specifications/POSTGRESQL_PARSER_SPECIFICATION.md`
- `docs/specifications/MYSQL_PARSER_SPECIFICATION.md`
- `docs/specifications/firebird_spec.md`
- `docs/findings/engine_gap_report.md` (parser gaps)
- `docs/planning/plan_02_uuid_resolution_and_rename_move.md` (rename/move opcodes)
- `docs/planning/plan_03_sblr_version2_extended_opcodes.md` (SBLR v2)

## Order of Implementation
1) V2 parser DDL coverage (CREATE/ALTER).
2) ScratchBird transaction control syntax (SQL-standard + Firebird legacy + conflict action + autocommit).
3) V2 semantic validation (GROUP BY and dependency collection).
4) Firebird parser gaps (window specs, predicates).
5) MySQL parser gaps (NULL-safe, placeholders, constraints, geometry).
6) PostgreSQL parser gaps (ESCAPE, arrays, CREATE stubs).

## Concrete Code Touchpoints (Exact Files + Functions)
- ScratchBird V2:
  - `src/parser/parser_v2.cpp`:
    - `Parser::parseCreate()` (currently commented out for FUNCTION/PROCEDURE/TRIGGER).
    - `Parser::parseAlter()` (only ALTER TABLE implemented).
    - `Parser::parseStartTransaction()` (add Firebird legacy clauses + conflict action).
    - `Parser::parseCommit()` / `Parser::parseRollback()` (add RETAINING + 2PC syntax).
    - `Parser::parseSet()` (add SET AUTOCOMMIT and SET TRANSACTION AUTOCOMMIT).
    - Implement `parseCreateDomain`, `parseAlterDomain`, `parseCreateFunction`, `parseCreateProcedure`, `parseCreateTrigger`, `parseCreatePackage`, `parseCreateException`, `parseCreateUDR`.
  - `include/scratchbird/parser/ast_v2.h`:
    - Extend `CreateDomainStmt` to include `domain_kind`, `dialect_tag`, `compat_name`, `inherits`, WITH blocks.
    - Add AST nodes for `AlterDomainStmt`, `DropDomainStmt`, `RebindDomainStmt` if not present.
  - `include/scratchbird/parser/ast_v2.h`:
    - Add `TxnConflictAction` enum.
    - Extend `StartTransactionStmt` with `conflict_action`, optional `conflict_error_code`, optional `autocommit_mode`, Firebird-legacy fields (WAIT/NO WAIT, LOCK TIMEOUT, RESERVING).
    - Extend `CommitStmt` / `RollbackStmt` with `retaining` + 2PC fields (`prepare_gid`, `prepared_gid`).
    - Extend `SetStmt` with `SetType::AUTOCOMMIT` and `autocommit_mode`.
  - `src/sblr/semantic_analyzer_v2.cpp`:
    - `SemanticAnalyzerV2::analyzeAlterTable()` (currently returns nullptr).
    - Add analyze handlers for new DDL nodes.
    - Add transaction analyzer: validate conflict_action, autocommit values, and map isolation levels.
  - `include/scratchbird/sblr/semantic_analyzer_v2.h`: add resolved types for new DDL.
  - `src/sblr/bytecode_generator_v2.cpp`:
    - Emit updated START_TRANSACTION payload with conflict_action/autocommit fields.
    - Emit new extended opcodes for COMMIT/ROLLBACK RETAINING and 2PC (see Plan 03).
    - Emit SET AUTOCOMMIT (new extended opcode).
- MySQL parser:
  - `src/parser/mysql/mysql_parser.cpp`:
    - `parseComparisonExpr()` (NULL-safe `<=>` TODO).
    - `parseLikeExpr()` ESCAPE handling TODO.
    - Placeholder emission TODO.
    - Table constraints parse TODO in CREATE TABLE.
    - Geometry types TODO.
- PostgreSQL parser:
  - `src/parser/postgresql/pg_parser_expr.cpp`:
    - ESCAPE handling TODO in `parseLikeExpr()`.
    - Array subscript TODO in `parsePostfixExpr()`.
  - `src/parser/postgresql/pg_parser_ddl.cpp`:
    - Stubbed CREATE statements section.
- Firebird parser:
  - `src/parser/firebird/firebird_parser.cpp`:
    - Window specification TODO in `parseFunctionCall()`.
    - Predicate variant tracking TODO in `parseLikeExpression()`.
    - Clause parsing stubs around window/predicates.

## Known TODO/Stubs (Must Be Removed)
- `src/parser/mysql/mysql_parser.cpp`:
  - `emit(EXPR_EQ)` for NULL-safe `<=>` (comment: TODO NULL-safe semantics).
  - ESCAPE clause TODO in `parseLikeExpr()`.
  - Placeholder handling TODO (line ~1097: `LITERAL_NULL`).
  - Table constraints TODO around CREATE TABLE.
  - Geometry type TODO around `parseGeometryType()`.
- `src/parser/postgresql/pg_parser_expr.cpp`:
  - ESCAPE clause TODO in `parseLikeExpr()`.
  - Array subscript TODO uses `EXT_ARRAY_LENGTH` placeholder.
- `src/parser/postgresql/pg_parser_ddl.cpp`:
  - "Other CREATE statements (stubs for now)" block.
- `src/parser/firebird/firebird_parser.cpp`:
  - Window specification TODO in `parseFunctionCall()`.
  - Predicate variant TODO in `parseLikeExpression()`.

## Dialect Guardrails (No ScratchBird Feature Bleed)
- For each emulated parser, build a **dialect allowlist** from its specification document and reject any statement not in that allowlist with a clear error (e.g., `Unsupported in MySQL dialect`).
- Emulated parsers must **not** emit ScratchBird-only opcodes or grammar constructs:
  - **No hierarchical schema path tokens** (PARENT/CURRENT/ABSOLUTE) in MySQL/PostgreSQL/Firebird parsers.
  - **No EXT_RENAME_OBJECT / EXT_MOVE_OBJECT** emission from emulated parsers (these are ScratchBird-only opcodes).
  - **No ScratchBird-only SHOW commands** (e.g., SHOW SCHEMA TREE / SHOW SCHEMA PATH / SHOW OBJECTS) in emulated parsers.
  - **No ScratchBird-only DOMAIN features** (RECORD/SET/VARIANT domains, WITH SECURITY/INTEGRITY/VALIDATION/QUALITY blocks) in emulated parsers unless the dialect spec explicitly allows them.
  - **No ScratchBird-only transaction extensions** in emulated parsers:
    - Reject `ON CONFLICT` clauses on START/SET TRANSACTION.
    - Reject `SET AUTOCOMMIT` and `SET TRANSACTION AUTOCOMMIT` unless the dialect explicitly supports it.
    - Reject `COMMIT/ROLLBACK RETAINING` unless the dialect explicitly supports it.
- **MySQL-specific guardrails** (minimum set):\n
  - MySQL parser must reject `CREATE/ALTER/DROP DOMAIN` and any ScratchBird domain blocks.\n
  - MySQL parser must treat `SCHEMA` only as `DATABASE` alias (no hierarchical schema path semantics).\n
- **PostgreSQL/Firebird guardrails**:\n
  - Accept only domain forms and options present in their specs; reject ScratchBird-only domain kinds (RECORD/SET/VARIANT) and WITH blocks.\n
  - Reject ScratchBird-only schema path tokens (PARENT/CURRENT/ABSOLUTE).\n

## Implementation Tasks
- Implement V2 CREATE/ALTER statement coverage and connect to bytecode generation.
- Implement ScratchBird transaction grammar (SQL-standard + Firebird legacy) and conflict-action syntax.
- Implement SET AUTOCOMMIT and SET TRANSACTION AUTOCOMMIT parsing.
- Implement COMMIT/ROLLBACK RETAINING and 2PC statements (PREPARE TRANSACTION, COMMIT PREPARED, ROLLBACK PREPARED).
- Implement GROUP BY validation in `SemanticAnalyzerV2`.
- Complete Firebird parser window/predicate parsing.
- Complete MySQL parser NULL-safe equality, placeholders, constraints, geometry types.
- Complete PostgreSQL parser ESCAPE handling, array subscripts, CREATE statements.
- Add dialect guardrails (allowlist-based rejection for non-dialect features and ScratchBird-only constructs).
- Extend DOMAIN DDL parsing to carry `dialect_tag` and `compat_name` into SBLR.
- Add SBLR opcodes for ALTER/DROP DOMAIN and conflict resolution operations.
- Implement CREATE DOMAIN grammar for RECORD/ENUM/SET/VARIANT and INHERITS clauses.
- Parse domain-specific WITH blocks (SECURITY/INTEGRITY/VALIDATION/QUALITY) and WITH OPTIONS (ENUM WRAP).
- Emit domain type descriptors in column definitions (TYPE_DOMAIN + UUID, arrays-of-domain).

## Required Data/Schema Changes
- None (parser/bytecode changes only).

## Completion Checklist (Developer)
- [ ] V2 CREATE/ALTER coverage matches specification.
- [ ] GROUP BY validation rejects invalid queries.
- [ ] Firebird parser handles window specs and predicate variants.
- [ ] MySQL parser handles NULL-safe, placeholders, constraints, geometry.
- [ ] PostgreSQL parser supports ESCAPE, arrays, missing CREATE types.

## Completion Checklist (Auditor)
- [ ] Parser tests pass per dialect with expected feature coverage.
- [ ] Unsupported dialect features fail gracefully with clear errors.
- [ ] Compatibility mappings to ScratchBird core are correct.

## Testing Requirements
- Dialect-specific parser tests (positive/negative).
- Bytecode roundtrip tests for critical DDL/DML.
- Regression tests for known TODO cases.
- Dialect guardrail tests that verify ScratchBird-only constructs are rejected by emulated parsers.
- Transaction grammar tests for ScratchBird (START/SET TRANSACTION, AUTOCOMMIT, conflict action, RETAINING, 2PC).
- Update/add tests in:
  - `tests/unit/test_parser_v2_ddl.cpp`
  - `tests/unit/test_parser_dml_v2.cpp`
  - `tests/unit/test_semantic_analyzer_v2.cpp`
  - `tests/unit/test_bytecode_generator_v2.cpp`
  - `tests/unit/test_mysql_parser.cpp`
  - `tests/unit/test_postgresql_parser.cpp`
  - `tests/unit/test_firebird_parser.cpp`

## Acceptance Criteria
- All parser TODOs in the findings report are closed with tests.
- V2 parser covers full superset features per master SQL spec.
- Emulated parsers support their native dialect features and fail gracefully on unsupported features.

## Implementation Notes (Concrete)
- **V2**: implement missing CREATE/ALTER handlers in `parser_v2.cpp`; ensure SBLR bytecode for each DDL action.
- **DOMAIN DDL**: accept optional `WITH DIALECT(<tag>)` and `WITH COMPAT(<name>)` clauses; default `dialect_tag` = current parser dialect (scratchbird/firebird/mysql/postgres).
- **DOMAIN advanced forms**: support `CREATE DOMAIN ... AS RECORD (...)`, `AS ENUM (...)`, `AS SET OF <type>`, `AS VARIANT (...)` plus `INHERITS <parent_domain>`.
- **WITH blocks**: parse `WITH SECURITY(...)`, `WITH INTEGRITY(...)`, `WITH VALIDATION(...)`, `WITH QUALITY(...)`, and `WITH OPTIONS(WRAP=...)` and serialize options into SBLR payloads.
- **Semantic**: implement `validateGroupBy()`; enforce non-aggregate columns must be in GROUP BY.
- **Firebird**: add window specification parsing and predicate variants mapping (LIKE/CONTAINING/STARTING/SIMILAR TO).
- **MySQL**: implement NULL-safe equality (`<=>`), placeholder handling, table constraints, geometry mapping.
- **PostgreSQL**: implement ESCAPE handling, array subscripts, and missing CREATE variants.
- **Failure mode**: unsupported syntax must return clear, consistent errors per dialect.
- **Transaction syntax**: ScratchBird V2 accepts both SQL-standard and Firebird legacy clauses. Emulated parsers accept only their dialect forms.

## Full Implementation Detail (No Ambiguity)
### 1) ScratchBird V2 Parser Coverage
- `Parser::parseCreate()`:
  - Un-comment and implement handlers for FUNCTION/PROCEDURE/TRIGGER/PACKAGE/EXCEPTION/UDR/DOMAIN.
  - For DOMAIN: create `CreateDomainStmt` with `domain_kind`, `dialect_tag`, `compat_name`, `inherits`, and WITH blocks.
- `Parser::parseAlter()`:
  - Add ALTER INDEX/VIEW/SEQUENCE/DOMAIN/SCHEMA/TRIGGER/ROLE/USER/GROUP parsing.
  - Add ALTER ... SET SCHEMA for schema moves (used by rename/move opcodes).
- `Parser::parseDrop()`:
  - Add DROP DOMAIN and DROP PACKAGE if missing.

### 2) Semantic Analyzer V2
- Implement `SemanticAnalyzerV2::analyzeAlterTable()`:
  - Produce resolved statement that includes action type, target table UUID, and any column/constraint IDs.
- Add new `Resolved*` types for ALTER/DROP DOMAIN and other DDL.
- Populate `ResolvedStatement::object_uuids` for dependency tracking.

### 3) Bytecode Emission (SBLR v2)
- Update `src/sblr/bytecode_generator_v2.cpp` to emit:
  - `EXT_CREATE_DOMAIN`, `EXT_ALTER_DOMAIN`, `EXT_DROP_DOMAIN`, `EXT_REBIND_DOMAIN`, `EXT_RESOLVE_DOMAIN_CONFLICT`.
  - `EXT_RENAME_OBJECT` / `EXT_MOVE_OBJECT` (from plan_02).
- For domain types in column definitions: emit `TYPE_DOMAIN` + UUID payload and `TYPE_ARRAY` wrappers.

### 4) Firebird Parser
- Implement window specs in `firebird_parser.cpp`:
  - Parse `OVER (PARTITION BY ... ORDER BY ... ROWS ... )` and record in AST.
- Track predicate variant in `parseLikeExpression()`:
  - Record whether LIKE/CONTAINING/STARTING/SIMILAR TO was used and emit matching opcode.

### 5) MySQL Parser
- `<=>` NULL-safe equality:
  - Emit dedicated opcode (new extended opcode if needed) that compares NULL semantics (NULL <=> NULL is true).
- Placeholders:
  - Emit placeholder descriptor with position and expected type; do not emit `LITERAL_NULL` as a proxy.
- Table constraints:
  - Parse and emit UNIQUE/PRIMARY/FOREIGN/CHECK table constraints.
- Geometry:
  - Map geometry types to ScratchBird geometry type set or emit explicit unsupported error.

### 6) PostgreSQL Parser
- ESCAPE handling:
  - Parse ESCAPE literal and emit opcode with escape-char payload.
- Array subscripts:
  - Emit `EXT_ARRAY_SUBSCRIPT` (new or existing) with base expression and index expression.
- CREATE stubs:
  - Implement remaining CREATE variants in `pg_parser_ddl.cpp` (VIEW/MATERIALIZED VIEW/SEQUENCE/INDEX/TYPE).

### 7) ScratchBird Transaction Control (SQL + Firebird Legacy)
**Goal:** ScratchBird accepts both SQL-standard and Firebird legacy transaction syntax, and always starts a new transaction (no in-place edits).

**Accepted forms (ScratchBird only):**
```
START TRANSACTION [<sql_std_opts>] [<fb_opts>] [ON CONFLICT <action>];
BEGIN [WORK|TRANSACTION] [<sql_std_opts>] [<fb_opts>] [ON CONFLICT <action>];
SET TRANSACTION [<sql_std_opts>] [<fb_opts>] [ON CONFLICT <action>];
SET AUTOCOMMIT {ON|OFF|1|0} [ON CONFLICT <action>];
SET TRANSACTION AUTOCOMMIT {ON|OFF|1|0} [ON CONFLICT <action>];
COMMIT [WORK|TRANSACTION] [AND [NO] CHAIN] [RETAINING];
ROLLBACK [WORK|TRANSACTION] [AND [NO] CHAIN] [RETAINING];
PREPARE TRANSACTION '<gid>';
COMMIT PREPARED '<gid>';
ROLLBACK PREPARED '<gid>';
```

**SQL-standard options (`<sql_std_opts>`):**
- `ISOLATION LEVEL {READ COMMITTED|READ UNCOMMITTED|REPEATABLE READ|SERIALIZABLE}`
- `READ ONLY` / `READ WRITE`
- `DEFERRABLE` / `NOT DEFERRABLE`

**Firebird legacy options (`<fb_opts>`):**
- `ISOLATION LEVEL {READ COMMITTED|SNAPSHOT|SNAPSHOT TABLE STABILITY}`
- `WAIT` / `NO WAIT`
- `LOCK TIMEOUT <seconds>`
- `RESERVING <table> FOR {SHARED|PROTECTED} {READ|WRITE} [, ...]`

**Isolation mapping (ScratchBird):**
- `READ COMMITTED` -> READ_COMMITTED
- `READ UNCOMMITTED` -> READ_COMMITTED (flag `read_uncommitted` stored but executor treats as RC)
- `REPEATABLE READ` -> SNAPSHOT
- `SERIALIZABLE` -> SNAPSHOT TABLE STABILITY

**Conflict action clause (`ON CONFLICT <action>`):**
- `COMMIT`: commit current transaction, then start new one.
- `ROLLBACK`: rollback current transaction, then start new one.
- `ERROR [<code>]`: error and keep current transaction.
- `KEEP`: return success but do not start new transaction (current stays active).
- If omitted: use per-user/role default (global fallback is ROLLBACK).

**Parser rules:**
- Accept both SQL and Firebird options in the same statement; parse all recognized clauses in any order.
- `SET TRANSACTION` **must** behave like `START TRANSACTION` (never edits in place).
- `SET AUTOCOMMIT` is a transaction boundary command (handles active transaction per conflict action).

**AST changes (ScratchBird V2):**
- `StartTransactionStmt` gains: `conflict_action`, `conflict_error_code`, `autocommit_mode`, Firebird legacy fields.
- `CommitStmt` / `RollbackStmt`: add `retaining` and 2PC `prepared_gid` fields.
- `SetStmt`: add `SetType::AUTOCOMMIT` with `autocommit_mode` and `conflict_action`.

**Bytecode emission:**
- Use updated START_TRANSACTION payload (Plan 03) with conflict_action + autocommit fields.
- Emit `EXT_SET_AUTOCOMMIT`, `EXT_COMMIT_RETAINING`, `EXT_ROLLBACK_RETAINING`, `EXT_PREPARE_TXN`, `EXT_COMMIT_PREPARED`, `EXT_ROLLBACK_PREPARED`.

**Dialect guardrails:**
- Emulated parsers must reject the `ON CONFLICT` clause and `SET AUTOCOMMIT` forms unless the native dialect explicitly supports them.

## SBLR Opcode Requirements for Domains
- `EXT_CREATE_DOMAIN` payload must include: `domain_id`, `domain_name`, `dialect_tag`, `compat_name`, `owner_id`, `base_type_oid`, `default_expr_oid`, `check_expr_oid`, `cast_map_oid`, `storage_hash`, `definition_hash`, `not_null`, `origin_node_id`, `origin_cluster_id`.
- Add `EXT_ALTER_DOMAIN` (rename, default/check, compat updates).
- Add `EXT_DROP_DOMAIN` (RESTRICT-only in executor).
- Add `EXT_REBIND_DOMAIN` (repoint dependent objects to new domain UUID).
- Add `EXT_RESOLVE_DOMAIN_CONFLICT` (admin resolution).

## Concrete Test Cases
- **V2**: parse/compile each new CREATE/ALTER statement and execute DDL.
- **V2 DOMAIN**: parse `CREATE DOMAIN ... WITH DIALECT(...) WITH COMPAT(...)` and verify SBLR payload fields.
- **V2 DOMAIN advanced**: parse RECORD/ENUM/SET/VARIANT and WITH blocks, verify payload structure.
- **Firebird**: parse WHERE predicates with CONTAINING/STARTING/SIMILAR TO and window clauses.
- **MySQL**: parse `<=>`, placeholders, and constraints in CREATE TABLE.
- **PostgreSQL**: parse ESCAPE in LIKE and array subscripts in expressions.

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.

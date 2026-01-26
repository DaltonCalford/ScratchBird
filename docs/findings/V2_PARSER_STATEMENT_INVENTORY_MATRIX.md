# V2 Statement Inventory Matrix (Parser vs Semantic vs Bytecode)

## Scope and Sources (code-truth)
- Parser dispatch: `ScratchBird/src/parser/parser_v2.cpp:159-226`
- DDL dispatch: `ScratchBird/src/parser/parser_v2.cpp:234-397` (CREATE),
  `ScratchBird/src/parser/parser_v2.cpp:2773-2949` (ALTER),
  `ScratchBird/src/parser/parser_v2.cpp:3587-3611` (DROP),
  `ScratchBird/src/parser/parser_v2.cpp:3906-3943` (TRUNCATE)
- DML/EXECUTE/PSQL dispatch: `ScratchBird/src/parser/parser_v2.cpp:3950-4035` (WITH),
  `ScratchBird/src/parser/parser_v2.cpp:4057-4859` (SELECT/INSERT/UPDATE/DELETE),
  `ScratchBird/src/parser/parser_v2.cpp:8088-8211` (EXECUTE ...),
  `ScratchBird/src/parser/parser_v2.cpp:7715-7773` (PSQL statement dispatcher)
- AST statement inventory: `ScratchBird/include/scratchbird/parser/ast_v2.h:52-155`
- Semantic analyzer statement dispatch: `ScratchBird/src/sblr/semantic_analyzer_v2.cpp:3496-3634`
- Bytecode generator statement dispatch: `ScratchBird/src/sblr/bytecode_generator_v2.cpp:520-625`
- Gatekeeper context matching: `ScratchBird/src/parser/parser_state_v2.cpp:70-88`
- Lexer keywords (MERGE/CALL/ANALYZE): `ScratchBird/include/scratchbird/parser/lexer_v2.h:124-148`
- Grammar spec references: `ScratchBird/docs/specifications/parser/ScratchBird Master Grammar Specification v2.0.md:30-35`

## Legend
- Parser/Semantic/Bytecode: **Y** = supported, **P** = partial/limited, **N** = missing, **U** = unreachable.
- "Partial" includes parsing or execution limitations (e.g., body captured as text, options missing).

## DDL Statement Inventory
| Statement | Parser | Semantic | Bytecode | Notes / References |
| --- | --- | --- | --- | --- |
| CREATE SCHEMA | Y | Y | Y | Dispatch in `parser_v2.cpp:290-297`. |
| CREATE DATABASE | Y | Y | Y | Dispatch in `parser_v2.cpp:298-303`. |
| CREATE DOMAIN | P | Y | Y | WITH block limited to DIALECT/COMPAT/INTEGRITY/SECURITY/VALIDATION/QUALITY/OPTIONS (`parser_v2.cpp:2504-2523`). |
| CREATE TABLE | P | Y | Y | No CTAS/LIKE parsing; options limited to ON COMMIT/TABLESPACE/INHERITS/PARTITION (`parser_v2.cpp:785-898`). |
| CREATE INDEX | Y | Y | Y | UNIQUE handled in parseCreate (`parser_v2.cpp:251-333`). |
| CREATE VIEW | Y | Y | Y | MATERIALIZED/TEMPORARY flags parsed (`parser_v2.cpp:283-345`). |
| CREATE SEQUENCE | Y | Y | Y | TEMP/OR REPLACE flags (`parser_v2.cpp:347-357`). |
| CREATE FUNCTION | P | Y | Y | Body captured as text, not parsed into PSQL (`parser_v2.cpp:2595-2601`). |
| CREATE PROCEDURE | P | Y | Y | Body captured as text, not parsed into PSQL (`parser_v2.cpp:2658-2664`). |
| CREATE TRIGGER | P | Y | Y | Body captured as text, not parsed into PSQL (`parser_v2.cpp:2750-2764`). |
| CREATE PACKAGE | N | Y | Y | AST + semantic/generator exist, parseCreate has no PACKAGE branch (`ast_v2.h:67-88`, `parser_v2.cpp:290-397`). |
| CREATE EXCEPTION | N | Y | Y | AST + semantic/generator exist, parseCreate has no EXCEPTION branch (`ast_v2.h:70-90`, `parser_v2.cpp:290-397`). |
| CREATE TYPE | N | N | N | ASTKind includes CreateTypeStmt, no parser/semantic/bytecode (`ast_v2.h:52-90`). |
| CREATE USER | Y | Y | Y | Dispatch in `parser_v2.cpp:377-381`. |
| CREATE ROLE | Y | Y | Y | Dispatch in `parser_v2.cpp:383-388`. |
| CREATE JOB | Y | Y | Y | Dispatch in `parser_v2.cpp:389-394`. |
| ALTER TABLE | P | Y | Y | Supports add/drop columns, constraints, RLS/tablespace/schema; no ALTER COLUMN options (`parser_v2.cpp:2990-3580`). |
| ALTER INDEX | P | Y | Y | Only SET options (bloom filter), rename/move handled separately (`parser_v2.cpp:2822-2935`). |
| ALTER SCHEMA | Y | Y | Y | Rename/owner only (`parser_v2.cpp:2952-2990`). |
| ALTER DATABASE | Y | Y | Y | Limited options in parser (`parser_v2.cpp:3324-3390`). |
| ALTER DOMAIN | Y | Y | Y | Basic set/drop default/not null; no rich options (`parser_v2.cpp:3184-3321`). |
| ALTER JOB | Y | Y | Y | Job options parsed (`parser_v2.cpp:1350-1903`). |
| ALTER SYSTEM | Y | Y | Y | Parser/semantic/generator present (`parser_v2.cpp:7246-7316`). |
| RENAME OBJECT | Y | Y | Y | Generic rename/move handler (`parser_v2.cpp:2783-2856`). |
| MOVE OBJECT (SET SCHEMA) | Y | Y | Y | Generic rename/move handler (`parser_v2.cpp:2805-2815`). |
| DROP SCHEMA | Y | Y | Y | Dispatch in `parser_v2.cpp:3590-3591`. |
| DROP DATABASE | Y | Y | Y | Dispatch in `parser_v2.cpp:3591-3592`. |
| DROP TABLE | Y | Y | Y | Dispatch in `parser_v2.cpp:3592-3593`. |
| DROP INDEX | Y | Y | Y | Dispatch in `parser_v2.cpp:3593-3594`. |
| DROP VIEW | Y | Y | Y | Dispatch in `parser_v2.cpp:3594-3595`. |
| DROP JOB | Y | Y | Y | Dispatch in `parser_v2.cpp:3595-3596`. |
| DROP DOMAIN | Y | Y | Y | Dispatch in `parser_v2.cpp:3596-3597`. |
| DROP FUNCTION | Y | Y | Y | Dispatch in `parser_v2.cpp:3597-3598`. |
| DROP PROCEDURE | Y | Y | Y | Dispatch in `parser_v2.cpp:3598-3599`. |
| DROP TRIGGER | Y | Y | Y | Dispatch in `parser_v2.cpp:3599-3600`. |
| DROP PACKAGE | Y | Y | Y | Dispatch in `parser_v2.cpp:3600-3601`. |
| DROP ROLE | Y | Y | Y | Dispatch in `parser_v2.cpp:3601-3602`. |
| DROP EXCEPTION | Y | Y | Y | Dispatch in `parser_v2.cpp:3602-3603`. |
| DROP SEQUENCE | N | Y | Y | AST + semantic/generator exist, parseDrop has no SEQUENCE branch (`parser_v2.cpp:3587-3611`). |
| TRUNCATE TABLE | Y | Y | Y | Parsed by `parseTruncateTable` (`parser_v2.cpp:3906-3943`). |

## DML Statement Inventory
| Statement | Parser | Semantic | Bytecode | Notes / References |
| --- | --- | --- | --- | --- |
| SELECT | Y | Y | Y | WITH supported for SELECT only in CTE bodies (`parser_v2.cpp:3950-4035`). |
| INSERT | Y | Y | Y | WITH supported (`parser_v2.cpp:3950-3975`). |
| UPDATE | Y | Y | Y | WITH supported (`parser_v2.cpp:3950-3975`). |
| DELETE | Y | Y | Y | WITH supported (`parser_v2.cpp:3950-3975`). |
| COPY | P | Y | Y | Option set limited (`parser_v2.cpp:5010-5039`). |
| MERGE | U | N | N | Lexer emits KW_MERGE; parser checks `matchContextual("MERGE")` (IDENTIFIER only). (`lexer_v2.h:124-148`, `parser_state_v2.cpp:70-88`, `parser_v2.cpp:223-224`) |
| EXECUTE BLOCK | Y | N | N | Parsed via `EXECUTE BLOCK`, no semantic/bytecode handler (`parser_v2.cpp:8088-8160`, `semantic_analyzer_v2.cpp:3496-3634`). |
| EXECUTE PROCEDURE | Y | N | N | Parsed via `EXECUTE PROCEDURE`, no semantic/bytecode handler (`parser_v2.cpp:8162-8190`). |
| EXECUTE STATEMENT | Y | N | N | Parsed via `EXECUTE STATEMENT`, no semantic/bytecode handler (`parser_v2.cpp:8192-8211`). |

## Transaction / Session / Utility / DCL / Connection
| Statement | Parser | Semantic | Bytecode | Notes / References |
| --- | --- | --- | --- | --- |
| START/BEGIN TRANSACTION | Y | Y | Y | `parser_v2.cpp:181-183` |
| PREPARE TRANSACTION | Y | Y | Y | `parser_v2.cpp:183-184` |
| COMMIT | Y | Y | Y | `parser_v2.cpp:184-185` |
| ROLLBACK | Y | Y | Y | `parser_v2.cpp:185-186` |
| SAVEPOINT | Y | Y | Y | `parser_v2.cpp:186-187` |
| RELEASE SAVEPOINT | Y | Y | P | Resolved as `ResolvedSavepointStmt`, no distinct bytecode (`semantic_analyzer_v2.cpp:3704-3708`, `bytecode_generator_v2.cpp:598-600`). |
| SET | P | Y | Y | `SET PARSER VERSION` explicitly rejected (`parser_v2.cpp:6841-6847`). |
| RESET | Y | N | N | Parsed but no semantic/bytecode handler (`parser_v2.cpp:6858-6871`, `semantic_analyzer_v2.cpp:3496-3634`). |
| SHOW | Y | Y | Y | Parser dispatch present (`parser_v2.cpp:191-192`). |
| EXPLAIN | Y | Y | Y | Parser dispatch present (`parser_v2.cpp:195`). |
| SWEEP | Y | Y | Y | Parser dispatch present (`parser_v2.cpp:196`). |
| ALTER SYSTEM | Y | Y | Y | Parser dispatch present (`parser_v2.cpp:2781-2782`). |
| GRANT | Y | Y | Y | Parser dispatch present (`parser_v2.cpp:212-214`). |
| REVOKE | Y | Y | Y | Parser dispatch present (`parser_v2.cpp:213-214`). |
| CONNECT | Y | Y | Y | Parser dispatch present (`parser_v2.cpp:216-218`). |
| DISCONNECT | Y | Y | Y | Parser dispatch present (`parser_v2.cpp:217-218`). |
| COMMENT | Y | N | N | Parsed but no semantic/bytecode handler (`parser_v2.cpp:7472-7515`, `semantic_analyzer_v2.cpp:3496-3634`). |
| EXECUTE JOB | Y | Y | Y | Parsed via `EXECUTE JOB` (`parser_v2.cpp:204-208`). |
| CANCEL JOB RUN | Y | Y | Y | Parsed via `CANCEL JOB RUN` (`parser_v2.cpp:197-203`, `parser_v2.cpp:8065-8085`). |

### Session Command Detail (SET / SHOW / ALTER SYSTEM)

#### SET variants (parser-supported)
| Command | Parser | Notes / References |
| --- | --- | --- |
| SET SESSION AUTHORIZATION <user \| DEFAULT> | Y | `parser_v2.cpp:6458-6471` |
| SET LOCAL <name> =/TO <value> | Y | Scope set to LOCAL, then variable assignment (`parser_v2.cpp:6472-6839`) |
| SET SESSION <name> =/TO <value> | Y | Scope set to SESSION, then variable assignment (`parser_v2.cpp:6458-6839`) |
| SET TIME ZONE <value \| LOCAL \| DEFAULT> | Y | `parser_v2.cpp:6597-6608` |
| SET AUTOCOMMIT <ON\|OFF\|1\|0> [ON CONFLICT ...] | Y | `parser_v2.cpp:6611-6622` |
| SET TRANSACTION ... | Y | Isolation/access/lock options (`parser_v2.cpp:6625-6751`) |
| SET SQL DIALECT <1\|2\|3> | Y | `parser_v2.cpp:6753-6771` |
| SET NAMES <charset> | Y | `parser_v2.cpp:6773-6782` |
| SET LOCAL_TIMEOUT <seconds> | Y | `parser_v2.cpp:6784-6799` |
| SET ROLE <name \| NONE \| DEFAULT> | Y | `parser_v2.cpp:6801-6809` |
| SET PARSER VERSION <n> | P | Explicitly rejected (`parser_v2.cpp:6841-6847`) |
| SET <name> =/TO <value \| DEFAULT> | Y | Variable assignment (`parser_v2.cpp:6812-6839`) |

#### SHOW variants (parser-supported)
| Command | Parser | Notes / References |
| --- | --- | --- |
| SHOW ALL | Y | `parser_v2.cpp:6897-6900` |
| SHOW TRANSACTION ISOLATION LEVEL | Y | `parser_v2.cpp:6901-6906` |
| SHOW TABLES [FROM <db>] [LIKE <pattern>] | Y | `parser_v2.cpp:6907-6912` |
| SHOW DATABASES [LIKE <pattern>] | Y | `parser_v2.cpp:6913-6917` |
| SHOW COLUMNS FROM <table> [LIKE <pattern>] | Y | `parser_v2.cpp:6918-6924` |
| SHOW INDEXES FROM <table> | Y | `parser_v2.cpp:6925-6939` |
| SHOW INDEX <name> | Y | Firebird-style (`parser_v2.cpp:6932-6936`) |
| SHOW CREATE TABLE <name> | Y | `parser_v2.cpp:6941-6946` |
| SHOW TABLE <name> | Y | Firebird-style (`parser_v2.cpp:6947-6955`) |
| SHOW TRIGGER(S) [name] | Y | `parser_v2.cpp:6956-6962` |
| SHOW VIEW(S) [name] | Y | `parser_v2.cpp:6963-6969` |
| SHOW PROCEDURE(S) [name] | Y | `parser_v2.cpp:6970-6976` |
| SHOW FUNCTION(S) [name] | Y | `parser_v2.cpp:6977-6983` |
| SHOW DOMAIN(S) [name] | Y | `parser_v2.cpp:6984-6990` |
| SHOW GENERATOR(S) / SEQUENCE(S) [name] | Y | `parser_v2.cpp:6991-6998` |
| SHOW SCHEMA(S) [name] | Y | `parser_v2.cpp:6999-7005` |
| SHOW ROLE(S) [name] | Y | `parser_v2.cpp:7006-7012` |
| SHOW GRANTS [FOR <name>] | Y | `parser_v2.cpp:7013-7019` |
| SHOW JOBS [LIKE <pattern>] | Y | `parser_v2.cpp:7020-7024` |
| SHOW JOB <name> | Y | `parser_v2.cpp:7025-7046` |
| SHOW JOB RUNS [FOR] <job> | Y | `parser_v2.cpp:7025-7038` |
| SHOW CHECKS [<table>] | Y | `parser_v2.cpp:7048-7054` |
| SHOW COLLATIONS [LIKE <pattern>] | Y | `parser_v2.cpp:7055-7059` |
| SHOW COMMENTS [<object>] | Y | `parser_v2.cpp:7060-7066` |
| SHOW DEPENDENCIES [<object>] | Y | `parser_v2.cpp:7067-7073` |
| SHOW PACKAGE(S) <name> | Y | `parser_v2.cpp:7074-7078` |
| SHOW SQL DIALECT | Y | `parser_v2.cpp:7079-7083` |
| SHOW VERSION | Y | `parser_v2.cpp:7084-7087` |
| SHOW DATABASE | Y | `parser_v2.cpp:7088-7091` |
| SHOW SYSTEM | Y | `parser_v2.cpp:7092-7095` |
| SHOW METRICS | Y | `parser_v2.cpp:7096-7099` |
| SHOW PARSER VERSION | P | Explicitly rejected (`parser_v2.cpp:7100-7105`) |
| SHOW <variable> | Y | Default variable case (`parser_v2.cpp:7106-7110`) |

#### ALTER SYSTEM variants (parser-supported)
| Command | Parser | Notes / References |
| --- | --- | --- |
| ALTER SYSTEM SET <key> = <expr> | Y | Key supports dot segments or string literal (`parser_v2.cpp:3437-3476`) |

## PSQL Statement Inventory
| Statement | Parser | Semantic | Bytecode | Notes / References |
| --- | --- | --- | --- | --- |
| BEGIN...END (Compound) | Y | N | N | Parsed in `parseBeginEndBlock` (`parser_v2.cpp:7775-7801`). |
| DECLARE VARIABLE | Y | N | N | `parser_v2.cpp:8030-8045`. |
| DECLARE CURSOR | Y | N | N | `parser_v2.cpp:8214-8288`. |
| ASSIGNMENT (:=) | Y | N | N | `parser_v2.cpp:7752-7762`. |
| IF / ELSE | Y | N | N | `parser_v2.cpp:7803-7832`. |
| WHILE | Y | N | N | `parser_v2.cpp:7834-7852`. |
| FOR SELECT | Y | N | N | `parser_v2.cpp:7854-7880`. |
| FOR EXECUTE STATEMENT | Y | N | N | `parser_v2.cpp:7882-7918`. |
| LOOP / END LOOP | Y | N | N | `parser_v2.cpp:7924-7946`. |
| LEAVE | Y | N | N | `parser_v2.cpp:7948-7954`. |
| CONTINUE | Y | N | N | `parser_v2.cpp:7956-7962`. |
| EXIT | Y | N | N | `parser_v2.cpp:7964-7966`. |
| SUSPEND | Y | N | N | `parser_v2.cpp:7968-7970`. |
| RETURN | Y | N | N | `parser_v2.cpp:7972-7979`. |
| EXCEPTION (RAISE) | Y | N | N | `parser_v2.cpp:7982-7990`. |
| WHEN ... DO | Y | N | N | `parser_v2.cpp:7993-8028`. |
| POST EVENT | Y | N | N | `parser_v2.cpp:7722-7724`. |
| OPEN CURSOR | Y | N | N | `parser_v2.cpp:7726-7728`. |
| FETCH CURSOR | Y | N | N | `parser_v2.cpp:7730-7732`. |
| CLOSE CURSOR | Y | N | N | `parser_v2.cpp:7734-7736`. |
| SELECT/INSERT/UPDATE/DELETE in PSQL | Y | N | N | Parsed by `parsePSQLStatement` but no semantic/bytecode support. |

### PSQL Parsing Context Note
- PSQL statements are only parsed when `parsePSQLStatement` is invoked (notably in `EXECUTE BLOCK`).
- Stored routine bodies for `CREATE FUNCTION/PROCEDURE/TRIGGER` are captured as raw text, not parsed into PSQL AST (`parser_v2.cpp:2595-2601`, `parser_v2.cpp:2658-2664`, `parser_v2.cpp:2750-2764`).

## Spec-Only or Declared-Only Statements (Missing)
- **DESCRIBE**: Listed in grammar spec, no lexer token or parser dispatch (`ScratchBird Master Grammar Specification v2.0.md:30-35`, `lexer_v2.h:120-170`, `parser_v2.cpp:159-226`).
- **CALL**: Listed in grammar spec; lexer has `KW_CALL` but no parser dispatch (`lexer_v2.h:145-146`, `parser_v2.cpp:159-226`).
- **ANALYZE**: Lexer has `KW_ANALYZE` but no parser dispatch (`lexer_v2.h:145-146`, `parser_v2.cpp:159-226`).
- **CASE (statement form)**: Present as expression only; no statement-level parser entry (no ASTKind for CASE statement).

## Cross-Check Summary (Parser vs Semantic vs Bytecode)
1. **Parser-only statements without semantic/bytecode**:
   - RESET, COMMENT, EXECUTE BLOCK/PROCEDURE/STATEMENT, all PSQL statements.
2. **Semantic/bytecode statements with no parser entry**:
   - CREATE PACKAGE, CREATE EXCEPTION, DROP SEQUENCE (AST + semantic/generator exist, parser lacks dispatch).
3. **Unreachable parser paths**:
   - MERGE dispatch uses contextual match (IDENTIFIER only) while lexer emits KW_MERGE.
4. **Semantic/bytecode ambiguity**:
   - RELEASE SAVEPOINT resolves to `ResolvedSavepointStmt`, no distinct release opcode in bytecode.
5. **DDL lifecycle gaps**:
   - No CREATE/DROP TABLESPACE, CREATE/DROP GROUP, and no CREATE/ALTER/DROP for FOREIGN TABLE/UDR/SYNONYM despite DDL object enums listing them.

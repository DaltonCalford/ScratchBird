### Outstanding TODO (Parser/Expressions/SELECT)

- Expressions
  - Full type parsing for CAST/:: into `TypeDescriptor` (precision/scale/length, charset, arrays)
  - Dollar-quoted strings: additional edge-cases (nested tags not allowed; validate tags)
  - Numeric literals: full coverage (leading-dot .5, underscores if allowed by spec)
  - Error diagnostics and source spans; structured AST nodes for all expression forms

- SELECT
  - Derived tables/subqueries in FROM with aliases and optional LATERAL
  - ORDER BY: ordinals; normalize NULLS FIRST/LAST; stable normalization for ASC/DESC defaults
  - WINDOW: parse frame specs (ROWS/RANGE … BETWEEN/UNBOUNDED/CURRENT ROW) and references
  - Set operations: structural parse with precedence (UNION/INTERSECT/EXCEPT) and parentheses
  - PLAN clause: full grammar parse instead of raw capture
  - Semantic validation stubs: NATURAL/USING column alignment; DISTINCT ON expression subset

- Diagnostics/AST
  - Define AST node structs (expressions, order items, window specs, set ops) with source ranges
  - Improve error messages and minimal recovery to continue parsing for multiple diagnostics
  - Pretty-printer: enhance `format_set_tree` to safely include projections/relations (guard sizes)

- Identifiers/Normalization
  - Add compile definitions `SCRATCHBIRD_WITH_ICU` or `SCRATCHBIRD_WITH_UTF8PROC` and link the corresponding libs in CMake, then implement the real folding calls in `casefold_unicode`.

### Outstanding parser coverage vs Firebird

- SQL session/database
  - CREATE DATABASE / ALTER DATABASE / DROP DATABASE
  - CONNECT / DISCONNECT (engine attachment commands)
  - SET NAMES, SET ROLE, SET SQL DIALECT
  - SET TRANSACTION, COMMIT [WORK], ROLLBACK, SAVEPOINT, RELEASE SAVEPOINT
- DDL (schema objects)
  - CREATE/ALTER/DROP TABLE (columns, defaults, identity, computed, constraints)
  - Constraints: PRIMARY KEY, UNIQUE, CHECK, FOREIGN KEY (DEFERRABLE, actions)
  - CREATE/ALTER/DROP VIEW [WITH CHECK OPTION]
  - CREATE/ALTER/DROP INDEX (ASC/DESC, unique, expressions, active/inactive)
  - CREATE/ALTER/DROP SEQUENCE/GENERATOR; SET GENERATOR/ALTER SEQUENCE RESTART
  - CREATE/ALTER/DROP DOMAIN (default, constraints, collation)
  - CREATE/ALTER/DROP COLLATION; CREATE/ALTER CHARACTER SET
  - CREATE/ALTER/DROP EXCEPTION
  - GRANT/REVOKE (roles, privileges on objects; WITH GRANT OPTION)
  - CREATE/ALTER/DROP ROLE; CREATE/ALTER/DROP USER (if exposed via SQL)
- PSQL (procedural SQL)
  - EXECUTE BLOCK
  - CREATE/ALTER/DROP PROCEDURE (PSQL body)
  - CREATE/ALTER/DROP FUNCTION (PSQL body, external)
  - CREATE/ALTER/DROP TRIGGER (BEFORE/AFTER, statement/row, active/inactive)
  - Packages: CREATE/ALTER/DROP PACKAGE and PACKAGE BODY
  - PSQL statements: DECLARE VARIABLE, BEGIN..END, IF, WHILE, FOR SELECT, FOR EXECUTE STATEMENT,
    INTO, SUSPEND, CURSOR operations, WHEN..DO (exception handling), LEAVE/EXIT, RETURN
  - EXECUTE STATEMENT (dynamic SQL) with parameters and INTO
- DML gaps
  - MERGE (full syntax), UPDATE FROM, INSERT .. RETURNING with expressions/subqueries
  - DELETE FROM ... USING (if supported), updatable CTE targets (if supported)
- SQL features
  - CREATE OR ALTER variants where Firebird supports them
  - COMMENT ON, ALTER ... RENAME TO, ALTER ... DROP COLUMN/ADD COLUMN forms
  - Collation/charset resolution rules in parser layer (currently deferred)
- isql meta-commands (client-side)
  - SET AUTODDL, INPUT/OUTPUT, SHOW, SHELL, EDIT, ECHO, TERM, PLAN DISPLAY, TIMING, etc.
  - Note: these are not engine SQL; handled by isql frontend, not parsed by EngineLib.

Action: add minimal parsers/stubs + specs/tests for the above, starting with database/table/view/index/sequence DDL and PSQL EXECUTE BLOCK, aligning with Firebird grammar and our specs.

### Long-term Implementation Roadmap (defer engine functionality; parser-first)

- External providers (ODBC/MSSQL) — parser & specs only for now
  - Specs:
    - `specs/server/providers.yaml` (SPI contract)
    - `specs/sql/sql-foreign-data.yaml` (FOREIGN SERVER, USER MAPPING, FOREIGN TABLE, IMPORT FOREIGN SCHEMA)
  - Parser:
    - Accept DDL forms defined above (no execution backend yet)
  - Defer engine work (to be implemented after parser freeze):
    - Provider loader/DSO, ODBC and MSSQL sessions/statements, schema import
    - Optimizer ForeignScan and pushdown rules
    - DML pushdown enablement, error mapping, security/credential storage

- Smart terminator & doc comments — parser/feeder only
  - Implemented CLI toggles and feeder helpers; no engine changes required

- Database links — parser complete; execution routing deferred
  - GRANT/REVOKE on DATABASE LINK: parser accepted; no runtime behavior

- Spatial types — parser acceptance only; no storage/index ops

- Index method-specific behavior — parser capture/validation; no physical index engines

# SHOW/SET Surface (Spec-Reconciled)

Purpose: Single authoritative SHOW/SET surface for documentation and planning, reconciled across the BNF, session-control, and Master Grammar specs.

Status: spec synthesis snapshot; no implementation claims.

## Source set
- `ScratchBird/docs/specifications/00_GRAMMAR_BNF.md`
- `ScratchBird/docs/specifications/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `ScratchBird/docs/specifications/07_TRANSACTION_AND_SESSION_CONTROL.md`
- `ScratchBird/docs/specifications/ScratchBird Master Grammar Specification v2.0.md`
- `ScratchBird/docs/specifications/SCHEMA_PATH_RESOLUTION.md`

## Reconciliation principles
- Use the BNF system commands as the baseline SHOW surface.
- Add session-control SHOW commands (SEARCH_PATH, TIME ZONE, ALL).
- Adopt Master Grammar schema navigation SET commands as first-class, but keep BNF syntax as aliases.
- Treat SHOW DATABASES and SHOW SCHEMAS as synonyms for listing schemas.
- Treat SHOW ALL and SHOW VARIABLES as synonyms for session parameter display.
- Accept both SEARCH_PATH and SEARCH PATH tokenization; document SEARCH_PATH as canonical.
- Accept both SET SCHEMA and SET CURRENT SCHEMA; document SET SCHEMA as canonical.

## Authoritative SHOW surface (canonical forms and aliases)
| Area | Canonical form | Aliases/notes |
| --- | --- | --- |
| Schema list | SHOW DATABASES [LIKE pattern] | Alias: SHOW SCHEMAS |
| Table list | SHOW TABLES [FROM schema_path] [LIKE pattern] | FROM uses schema path resolution |
| Columns | SHOW COLUMNS FROM table [LIKE pattern] | Alias: DESCRIBE/DESC table |
| Indexes | SHOW INDEXES FROM table | Alias: SHOW INDEX FROM table |
| Create table | SHOW CREATE TABLE table | |
| Schema detail | SHOW SCHEMA [schema_path] | Empty means list schemas |
| Session params | SHOW <parameter_name> | Includes SEARCH_PATH, TIME ZONE, etc |
| Session params | SHOW ALL | Alias: SHOW VARIABLES |
| Transaction | SHOW TRANSACTION ISOLATION LEVEL | |
| Schema navigation | SHOW SCHEMA PATH | Full path to current schema |
| Schema navigation | SHOW SCHEMA TREE [DEPTH n] [FROM path] | FROM defaults to root |
| Schema navigation | SHOW SEARCH PATH | Current search path |
| Schema navigation | SHOW LOCATION OF [type] name | type optional (TABLE/VIEW/etc) |
| Schema navigation | SHOW RESOLVED name | Resolution target in search path |
| Schema navigation | SHOW OBJECTS [IN CURRENT|PATH|SCHEMA schema_path] [LIKE pattern] | Scope defaults to CURRENT |
| Firebird-style detail | SHOW TABLE/INDEX/TRIGGER/VIEW/PROCEDURE/FUNCTION/DOMAIN/GENERATOR/ROLE/GRANTS/CHECKS/COLLATIONS/COMMENTS/DEPENDENCIES/PACKAGE/SQL DIALECT/VERSION/DATABASE/SYSTEM | Object-detail format |
| MySQL diagnostics (optional) | SHOW PROCESSLIST, SHOW STATUS [LIKE], SHOW WARNINGS, SHOW ERRORS | Keep as optional extensions |
| MySQL DDL (optional) | SHOW CREATE DATABASE database | Optional extension |

## Authoritative SET surface (canonical forms and aliases)
| Area | Canonical form | Aliases/notes |
| --- | --- | --- |
| Transaction | SET TRANSACTION <options> | Alias: SET SESSION CHARACTERISTICS AS TRANSACTION <options> |
| Autocommit | SET AUTOCOMMIT {ON|OFF|1|0} [ON CONFLICT <action>] | ScratchBird extension |
| Session param | SET [SESSION|LOCAL] <parameter> TO|= <value> | Generic config parameter |
| Time zone | SET [SESSION|LOCAL] TIME ZONE <value|LOCAL|DEFAULT> | |
| Role | SET [SESSION|LOCAL] ROLE <role|DEFAULT|NONE> | |
| Authorization | SET [SESSION|LOCAL] SESSION AUTHORIZATION <user|DEFAULT> | |
| Schema | SET SCHEMA <schema_path> | Alias: SET CURRENT SCHEMA {UP|DEFAULT|schema_path} |
| Search path | SET SEARCH_PATH TO <schema_list> | Alias: SET SEARCH PATH <path_list> |
| Constraints | SET CONSTRAINTS {ALL|name_list} {DEFERRED|IMMEDIATE} | |
| Charset | SET NAMES <charset> | |
| SQL dialect | SET SQL DIALECT n | |
| Timeout | SET LOCAL_TIMEOUT n | |
| MySQL variables (optional) | SET @var = expr / SET @var := expr | Optional extension |

Notes:
- Schema path tokens PARENT/CURRENT/ABSOLUTE are valid in schema navigation and search path rules.
- Client-side ISQL SET commands (SET TERM, SET STATS, etc.) are not part of the SQL engine surface.

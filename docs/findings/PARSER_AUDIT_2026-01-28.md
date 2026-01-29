# Parser Audit (2026-01-28)

## Scope
Audit of Firebird/MySQL/PostgreSQL emulated parsers against current specs and
code paths to confirm implementation status and surface remaining gaps.

## MySQL Parser (src/parser/mysql/mysql_parser.cpp)

### Verified Implemented
- TEMPORARY table flags are emitted in CREATE TABLE.
  - `src/parser/mysql/mysql_parser.cpp:3479-3515`
- ON DUPLICATE KEY UPDATE is parsed and emits ON CONFLICT update list.
  - `src/parser/mysql/mysql_parser.cpp:2330-2395`
- UNSIGNED/ ZEROFILL types map to unsigned numeric types.
  - `src/parser/mysql/mysql_parser.cpp:118-131`
  - `src/parser/mysql/mysql_parser.cpp:4307-4312`
- CREATE INDEX and CREATE VIEW parsing emit bytecode.
  - `src/parser/mysql/mysql_parser.cpp:4409-4560`
- CREATE TABLE options emit EXT_TABLE_OPTIONS.
  - `src/parser/mysql/mysql_parser.cpp:3940-3985`
- CREATE PROCEDURE parsing emits opcode stub.
  - `src/parser/mysql/mysql_parser.cpp:4665-4900`

### Verified Gaps
- INSERT modifiers (LOW_PRIORITY/DELAYED/HIGH_PRIORITY/IGNORE) are parsed but ignored.
  - `src/parser/mysql/mysql_parser.cpp:2128-2140`
- INSERT IGNORE does not map to ON CONFLICT DO NOTHING yet.
  - `src/parser/mysql/mysql_parser.cpp:2135`

Reference: `docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md`

## PostgreSQL Parser (src/parser/postgresql/pg_parser*.cpp)

### Verified Implemented
- CREATE TEMP/UNLOGGED flags are recorded and emitted.
  - `src/parser/postgresql/pg_parser_ddl.cpp:158-560`

### Verified Gaps
- ARRAY types still map to VARCHAR in typeToOpcode/emitTypeDefinition.
  - `src/parser/postgresql/pg_parser.cpp:594-660`
Reference: `docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md`

## Firebird Parser (src/parser/firebird/firebird_parser.cpp)

### Verified Implemented
- GLOBAL TEMPORARY tables + ON COMMIT clause.
  - `src/parser/firebird/firebird_parser.cpp:1735-2304`
Reference: `docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md`

## Next Steps
1. Resolve the remaining MySQL INSERT modifier semantics and PostgreSQL ARRAY type mapping.
2. Update the parser gap specs to reflect the verified implementations above.

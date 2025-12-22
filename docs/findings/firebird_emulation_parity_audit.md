# Firebird Emulation Parity Audit

Date: 2025-12-20

This audit focuses on Firebird protocol parity with native Firebird clients,
including parser coverage (DDL/DML/PSQL), wire protocol behavior, and
metadata/catalog API surfaces (RDB$, MON$, SEC$).

## Scope
- Parser: `src/parser/firebird/` -> AST/SBLR
- Wire protocol adapter: `src/protocol/adapters/firebird_adapter.cpp`
- Catalog/metadata: `src/catalog/firebird_catalog.cpp`,
  `include/scratchbird/catalog/emulation_view_generator.h`

## Reference Specs
- `docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `docs/specifications/firebird_spec.md`
- `docs/specifications/FirebirdReferenceDocument.md`
- `docs/specifications/wire_protocols/firebird_wire_protocol.md`
- `docs/findings/firebird_wire_protocol_gaps.md` (existing gap list)

## Parser Gaps (Missing or Stubbed Features)

### DDL coverage is incomplete
- ALTER for object types other than TABLE/INDEX is not implemented.
  `src/parser/firebird/firebird_parser.cpp:1371`.
- DROP for object types other than TABLE/INDEX/VIEW/SEQUENCE is not implemented.
  `src/parser/firebird/firebird_parser.cpp:1383`.
- RECREATE for object types other than TABLE/VIEW/INDEX is not implemented.
  `src/parser/firebird/firebird_parser.cpp:1416`.
- DROP SEQUENCE/GENERATOR is not implemented.
  `src/parser/firebird/firebird_parser.cpp:1699`.
- CREATE PROCEDURE / FUNCTION / TRIGGER / DOMAIN / EXCEPTION / ROLE / PACKAGE
  are all stubbed with errors.
  `src/parser/firebird/firebird_parser.cpp:1711` through `:1736`.
- ALTER INDEX is not implemented.
  `src/parser/firebird/firebird_parser.cpp:1626`.

### DML / procedural gaps
- MERGE is not implemented.
  `src/parser/firebird/firebird_parser.cpp:1962`.
- EXECUTE PROCEDURE is not implemented.
  `src/parser/firebird/firebird_parser.cpp:2012`.
- EXECUTE STATEMENT is not implemented.
  `src/parser/firebird/firebird_parser.cpp:2561`.

### Transaction / session / DCL gaps
- SET TRANSACTION is not implemented.
  `src/parser/firebird/firebird_parser.cpp:2018`.
- SET statement is not implemented.
  `src/parser/firebird/firebird_parser.cpp:2074`.
- SHOW statement (ISQL compatibility) is not implemented.
  `src/parser/firebird/firebird_parser.cpp:2079`.
- GRANT and REVOKE are not implemented.
  `src/parser/firebird/firebird_parser.cpp:2085` and `:2090`.
- COMMENT statement is not implemented.
  `src/parser/firebird/firebird_parser.cpp:2096`.

### PSQL gaps
- FOR EXECUTE STATEMENT is not implemented.
  `src/parser/firebird/firebird_parser.cpp:2357`.
- LOOP statement is not implemented.
  `src/parser/firebird/firebird_parser.cpp:2367`.

### Expression gaps
- Window specification parsing is TODO for OVER clauses.
  `src/parser/firebird/firebird_parser.cpp:871`.
- LIKE/CONTAINING/STARTING/SIMILAR variant tracking is TODO; parser does not
  distinguish variants in the AST.
  `src/parser/firebird/firebird_parser.cpp:982`.

### Qualified name parsing allows feature bleed
- `parseSchemaPath` accepts unlimited dotted components, which is more than
  Firebird allows (Firebird does not have schemas; packages are the exception).
  `src/parser/firebird/firebird_parser.cpp:1259`.
  For strict parity, allow only single identifiers for table names, except
  where Firebird grammar permits `package.procedure`.

## Wire Protocol and Session API Gaps
- DROP DATABASE is a stub (no actual database drop).
  `src/protocol/adapters/firebird_adapter.cpp:1379`.

## Catalog and Metadata API Gaps

### RDB$ coverage is partial and some tables are stubbed
`FirebirdCatalogHandler` implements a subset of RDB$ tables:
RDB$DATABASE, RDB$RELATIONS, RDB$FIELDS, RDB$RELATION_FIELDS,
RDB$INDICES, RDB$INDEX_SEGMENTS, RDB$GENERATORS, RDB$PROCEDURES,
RDB$FUNCTIONS, RDB$TRIGGERS, RDB$CONSTRAINTS, RDB$CHARACTER_SETS,
RDB$COLLATIONS, RDB$TYPES, RDB$USER_PRIVILEGES, RDB$ROLES.
See `src/catalog/firebird_catalog.cpp:149`.

Within that subset:
- RDB$INDICES is empty (TODO to list indexes).
  `src/catalog/firebird_catalog.cpp:363`.
- RDB$INDEX_SEGMENTS is empty (TODO).
  `src/catalog/firebird_catalog.cpp:381`.
- RDB$GENERATORS is empty (TODO).
  `src/catalog/firebird_catalog.cpp:395`.
- RDB$PROCEDURES/FUNCTIONS/TRIGGERS return empty until PSQL/UDF/trigger
  support is implemented.
  `src/catalog/firebird_catalog.cpp:413`, `:429`, `:445`.

### MON$ and SEC$ coverage is minimal
- MON$ tables implemented: MON$DATABASE, MON$ATTACHMENTS, MON$TRANSACTIONS,
  MON$STATEMENTS only. `src/catalog/firebird_catalog.cpp:615`.
- SEC$ tables implemented: SEC$USERS and SEC$USER_ATTRIBUTES only.
  `src/catalog/firebird_catalog.cpp:758`.
Spec requires full MON$ (12) and SEC$ (4) coverage for Firebird 5.0.

### Emulation view generator is incomplete
- Firebird emulation views include only a small subset of RDB$ tables
  (RELATIONS, FIELDS, RELATION_FIELDS, INDICES, PROCEDURES, TRIGGERS).
  `include/scratchbird/catalog/emulation_view_generator.h:259`.
- No MON$ or SEC$ views are generated.

## Test Coverage Gaps
- Parser tests exist but do not cover PSQL, GRANT/REVOKE, SHOW, SET, or
  DDL for procedures/functions/triggers/domains.
  `tests/unit/test_firebird_parser.cpp`.
- No tests validate RDB$/MON$/SEC$ queries, or Firebird wire protocol
  metadata workflows.

## Summary (Firebird Parity Risk)
Firebird emulation is missing large sections of DDL, DCL, and PSQL, and
catalog coverage is far below the required RDB$/MON$/SEC$ sets. Wire protocol
DROP DATABASE is stubbed. A native Firebird client will fail to rely on
system catalogs and administration statements. These gaps must be addressed
for 1:1 parity with Firebird 5.0 clients.

# V3 Temporal Tables Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_TEMPORAL_TABLES.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- No V3 parsing, AST, emitter, or executor support was found for **system-versioned temporal tables**, `PERIOD FOR SYSTEM_TIME`, `WITH SYSTEM VERSIONING`, or temporal query clauses.
- Time-travel SELECT syntax (`FOR SYSTEM_TIME AS OF/BETWEEN/ALL`) is **not implemented**.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE TABLE ... WITH SYSTEM VERSIONING
[ ] No parser support for `WITH SYSTEM VERSIONING`, `ROW START/ROW END`, or `PERIOD FOR SYSTEM_TIME`.
[ ] No AST fields for temporal metadata (history table, system-time period columns).
[ ] No V3 emitter/executor support to create history tables or manage system-time columns.

### Temporal Query Syntax
[ ] `FOR SYSTEM_TIME AS OF`, `FOR SYSTEM_TIME BETWEEN ... AND ...`, `FOR SYSTEM_TIME ALL` not parsed or emitted.

### ALTER TABLE Temporal Management
[ ] `ALTER TABLE ... ADD SYSTEM VERSIONING` / `DROP SYSTEM VERSIONING` not parsed or executed.

### DROP TABLE Semantics
[ ] No logic to drop associated history tables when dropping a temporal table.

## Key References
- No matches for `SYSTEM VERSIONING`, `SYSTEM_TIME`, `ROW START`, `ROW END`, or `PERIOD FOR` in V3 parser/AST.
- Parser DDL/SELECT handling lacks temporal keywords: `src/parser/parser_v3.cpp`
- AST definitions contain no temporal table fields: `include/scratchbird/parser/ast_v3.h`

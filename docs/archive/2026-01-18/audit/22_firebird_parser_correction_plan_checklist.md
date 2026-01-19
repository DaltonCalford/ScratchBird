# Firebird Parser Correction Plan Checklist (Actual)

Purpose: Extracted mismatch list from `16_firebird_parser_statement_reference_actual.md` for implementation planning.

Status: derived from static code review; no runtime execution performed.

## Bytecode format mismatches (execution blockers)
- [ ] CREATE INDEX: V2 bytecode uses UUIDs/method/column indexes; executor expects name/table/column names.
- [ ] CREATE VIEW: V2 bytecode writes schema UUID + SBLR query; executor expects SQL definition string and flags layout.
- [ ] DROP TABLE/INDEX/VIEW: V2 bytecode writes UUID list; executor expects single name string + flags.
- [ ] TRUNCATE TABLE (V2): emits flags + UUID list; executor expects string + flags.
- [ ] CREATE TABLE FK payload: V2 writes TABLE_FK after tablespace with parent table placeholder; executor reads TABLE_FK before tablespace and expects parent table name strings.

## Missing parser implementations (Firebird syntax)
- [ ] CREATE PROCEDURE / FUNCTION / TRIGGER / PACKAGE / ROLE / EXCEPTION: parser emits errors.
- [ ] ALTER INDEX: parser emits error.
- [ ] DROP SEQUENCE / GENERATOR: parser emits error.
- [ ] MERGE: parser emits error.
- [ ] EXECUTE PROCEDURE / EXECUTE STATEMENT: parser emits error.
- [ ] FOR EXECUTE STATEMENT / LOOP: parser emits error.
- [ ] SET / SHOW / GRANT / REVOKE / COMMENT: parser emits error.
- [ ] Firebird ISQL SHOW variants (SHOW TABLE/INDEX/TRIGGER/VIEW/PROCEDURE/FUNCTION/DOMAIN/GENERATOR/SCHEMA/ROLE/GRANTS/CHECKS/COLLATIONS/COMMENTS/DEPENDENCIES/PACKAGE/SYSTEM/SQL_DIALECT/VERSION/DATABASE): not parsed; executor expects opcode + payload string or no payload depending on variant.

## Semantic/bytecode gaps (AST accepted but not executed)
- [ ] PSQL statements (BEGIN...END, IF/WHILE/FOR, SUSPEND, etc.) are parsed but not supported by SemanticAnalyzerV2/BytecodeGeneratorV2.
- [ ] UPDATE OR INSERT compiles as INSERT only; UPDATE semantics not implemented.
- [ ] CREATE SEQUENCE/GENERATOR is parsed but rejected by semantic analyzer.

## Feature drops (parsed but not enforced)
- [ ] CREATE TABLE: OR REPLACE/OR ALTER, TEMPORARY/ON COMMIT, PK/UNIQUE/FK/CHECK/GENERATED/COMPUTED constraints not enforced by executor.
- [ ] CREATE INDEX: expression indexes and WHERE clause parsed but not executed due to bytecode mismatch.
- [ ] CREATE VIEW: WITH CHECK OPTION parsed but not executed due to bytecode mismatch.
- [ ] Schema-qualified names are rejected; parser logs errors on dotted identifiers.

## Cross-cutting
- [ ] Decide whether to align V2 bytecode with executor or extend executor to support V2 formats.
- [ ] Implement PSQL pipeline or explicitly disable PSQL in Firebird parser to avoid false-positive coverage.

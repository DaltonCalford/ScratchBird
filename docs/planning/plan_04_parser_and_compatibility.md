# Plan 04 - Parser Coverage and Compatibility

## Scope
Complete V2 (ScratchBird) parser coverage and fill compatibility gaps for Firebird/MySQL/PostgreSQL parsers.

## Priority
P1 (blocks SQL feature coverage and compatibility).

## References
- `docs/specifications/01_SQL_DIALECT_OVERVIEW.md`
- `docs/specifications/ScratchBird SQL Language Specification - Master Document.md`
- `docs/specifications/POSTGRESQL_PARSER_SPECIFICATION.md`
- `docs/specifications/MYSQL_PARSER_SPECIFICATION.md`
- `docs/specifications/firebird_spec.md`
- `docs/findings/engine_gap_report.md` (parser gaps)

## Order of Implementation
1) V2 parser DDL coverage (CREATE/ALTER).
2) V2 semantic validation (GROUP BY and dependency collection).
3) Firebird parser gaps (window specs, predicates).
4) MySQL parser gaps (NULL-safe, placeholders, constraints, geometry).
5) PostgreSQL parser gaps (ESCAPE, arrays, CREATE stubs).

## Implementation Tasks
- Implement V2 CREATE/ALTER statement coverage.
- Implement GROUP BY validation in SemanticAnalyzerV2.
- Complete Firebird parser window/predicate parsing.
- Complete MySQL parser NULL-safe equality, placeholders, constraints, geometry.
- Complete PostgreSQL parser ESCAPE handling, array subscripts, CREATE statements.

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

## Acceptance Criteria
- All parser TODOs in the findings report are closed with tests.
- V2 parser covers full superset features per master SQL spec.
- Emulated parsers support their native dialect features and fail gracefully on unsupported features.

## Implementation Notes (Concrete)
- **V2**: implement missing CREATE/ALTER handlers in `parser_v2.cpp`; ensure SBLR bytecode for each DDL action.
- **Semantic**: implement `validateGroupBy()`; enforce non-aggregate columns must be in GROUP BY.
- **Firebird**: add window specification parsing and predicate variants mapping.
- **MySQL**: implement NULL-safe equality (`<=>`), placeholder handling, table constraints, geometry mapping.
- **PostgreSQL**: implement ESCAPE handling, array subscripts, and missing CREATE variants.
- **Failure mode**: unsupported syntax must return clear, consistent errors per dialect.

## Expanded API/Schema Details
- **Parser V2**:
  - `Parser::parseCreate()` must cover FUNCTION/PROCEDURE/TRIGGER/PACKAGE/etc.
  - `Parser::parseAlter()` must support ALTER INDEX/VIEW/SEQUENCE.
  - `BytecodeGeneratorV2` must emit opcodes for new DDL actions.
- **Semantic**:
  - `SemanticAnalyzerV2::validateGroupBy(ResolvedSelectStmt*)` must enforce SQL rules.
  - `ResolvedStatement::object_uuids` must include all referenced objects.
- **Dialect parsers**:
  - Firebird: `firebird_parser.cpp` window/predicate parsing must map to SBLR equivalents.
  - MySQL: `mysql_parser.cpp` must map `<=>` to NULL-safe compare opcode; placeholders must be typed.
  - PostgreSQL: `pg_parser_expr.cpp` must implement ESCAPE and array subscripts; `pg_parser_ddl.cpp` must cover missing CREATE types.

## Full Implementation Detail (No Ambiguity)
- **V2 CREATE coverage**:
  - Implement `CREATE FUNCTION/PROCEDURE/TRIGGER/PACKAGE/DOMAIN/EXCEPTION` parsing in `parser_v2.cpp`.
  - Ensure `BytecodeGeneratorV2` emits appropriate opcodes and required metadata (UUIDs, schema IDs).
- **ALTER coverage**:
  - Implement `ALTER INDEX`, `ALTER VIEW`, `ALTER SEQUENCE` parsing and bytecode.
- **Semantic validation**:
  - Enforce GROUP BY rules: every non-aggregate SELECT column must be present in GROUP BY.
- **Dialect-specific**:
  - Firebird: parse window specs and predicate variants; map to SBLR opcodes.
  - MySQL: implement NULL-safe equality and placeholders; parse table constraints and geometry types.
  - PostgreSQL: implement ESCAPE char, array subscripts, and missing CREATE statements.

## Concrete Test Cases
- **V2**: parse/compile each new CREATE/ALTER statement and execute DDL.
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

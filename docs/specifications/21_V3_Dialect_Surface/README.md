# 21_V3_Dialect_Surface

## Purpose
Canonical specification area for the V3 parser surface, parser-to-SBLR lowering boundary, and the bounded native and emulated dialect front doors that are currently code-backed.

## Status
Authoritative - approved for implementation.
Current audit status: `partial`.

## Capability-state vocabulary
- `supported_parser_surface`: parser front door and AST entry path are code-backed.
- `supported_parser_and_lowering_surface`: parser plus lowering or emit path are code-backed.
- `supported_listener_path`: native listener-path conformance proof exists.
- `partial`: framework and some proof exist, but exact per-feature or per-clause closure is incomplete.
- `target_state_only`: design or checklist target without current code-backed proof.
- `fail_closed`: broader parity or runtime authority must not be inferred.

## Current code-backed truth
- A real V3 parser stack exists in `parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`, and `ast_sblr_lowerer`.
- A real native listener-path conformance suite exists in `tests/conformance/v3_native_inet`.
- Real emulated parser families exist under `include/scratchbird/parser/{mysql,postgresql,firebird}` and `src/parser/{mysql,postgresql,firebird}`.
- Builtin emulation package scaffolds are real for `postgresql`, `mysql`, and `firebirdsql`.
- Section `21` is authoritative for parser-layer syntax, normalization, and lowering contracts, not for section-wide runtime parity of every listed SQL surface.

## First-wave closure axes
- native parser statement-family capability authority
- gatekeeper keyword, naming, and normalization or rejection closure
- listener-path, direct-parse, and lowering authority split
- emulated parser family and builtin scaffold capability authority
- JDBC compatibility and promotion boundary
- extension front-door and bounded runtime ownership closure

## Main fail-closed boundary
- Do not treat every statement family named in section `21` as runtime-proven end to end.
- Treat section `21` as parser and lowering authority first.
- Runtime, catalog, observability, security, connector, and cluster behavior remain owned by their implementation sections.

## Direct audit lookup anchors
- `src/parser/parser_v3.cpp` search key `ParseResult Parser::parseStatement()`
- `src/parser/v3_emitter.cpp` search key `V3Emitter::emitStatementToContainer(`
- `src/sblr/ast_sblr_lowerer.cpp` search key `AstSblrLowerer::emitStatementToContainer(`

## Files
- Canonical documents for this section are listed in the file index below.
- Use this README as the section navigation root.
- Do not use legacy material as implementation authority.

## Links
- Back to root index: [../README.md](../README.md)

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [BETA2_DONOR_DATATYPE_GRAMMAR_AND_AST_EXTENSION_MODEL.md](BETA2_DONOR_DATATYPE_GRAMMAR_AND_AST_EXTENSION_MODEL.md)
- [BETA2_DONOR_DIALECT_AST_GRAMMAR_AND_DESUGAR_EXPANSION_MODEL.md](BETA2_DONOR_DIALECT_AST_GRAMMAR_AND_DESUGAR_EXPANSION_MODEL.md)
- [BETA2_FUNCTION_SURFACE_COMPLETION_MODEL.md](BETA2_FUNCTION_SURFACE_COMPLETION_MODEL.md)
- [BRANCH_CHANGESET_SQL_SURFACE.md](BRANCH_CHANGESET_SQL_SURFACE.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [NATIVE_ADMIN_LANGUAGE_DEFINITION.md](NATIVE_ADMIN_LANGUAGE_DEFINITION.md)
- [NATIVE_CLUSTER_FABRIC_SQL.md](NATIVE_CLUSTER_FABRIC_SQL.md)
- [NATIVE_DDL_LANGUAGE_DEFINITION.md](NATIVE_DDL_LANGUAGE_DEFINITION.md)
- [NATIVE_DIAGNOSTICS_SQL.md](NATIVE_DIAGNOSTICS_SQL.md)
- [NATIVE_DML_LANGUAGE_DEFINITION.md](NATIVE_DML_LANGUAGE_DEFINITION.md)
- [NATIVE_INFRASTRUCTURE_SQL.md](NATIVE_INFRASTRUCTURE_SQL.md)
- [NATIVE_JDBC_COMPATIBILITY_SQL.md](NATIVE_JDBC_COMPATIBILITY_SQL.md)
- [NATIVE_JDBC_DRIVER_SQL_PROMOTION_MATRIX.md](NATIVE_JDBC_DRIVER_SQL_PROMOTION_MATRIX.md)
- [NATIVE_LISTENER_CONTROL_SQL.md](NATIVE_LISTENER_CONTROL_SQL.md)
- [NATIVE_PARSER_FEATURE_FAMILIES.md](NATIVE_PARSER_FEATURE_FAMILIES.md)
- [NATIVE_PARSER_NORMALIZATION_AND_REJECTION_MATRIX.md](NATIVE_PARSER_NORMALIZATION_AND_REJECTION_MATRIX.md)
- [NATIVE_PSQL_TSQL_LANGUAGE_DEFINITION.md](NATIVE_PSQL_TSQL_LANGUAGE_DEFINITION.md)
- [NATIVE_SQL_SURFACE.md](NATIVE_SQL_SURFACE.md)
- [NATIVE_STORAGE_AND_BLOB_FILTER_SQL.md](NATIVE_STORAGE_AND_BLOB_FILTER_SQL.md)
- [NATIVE_SUPERSET_COMPATIBILITY_MATRIX.md](NATIVE_SUPERSET_COMPATIBILITY_MATRIX.md)
- [NATIVE_UDR_REMOTE_CONNECTOR_SQL.md](NATIVE_UDR_REMOTE_CONNECTOR_SQL.md)
- [NORMATIVE_ADMIN_DDL_DML_PSQL_TSQL_IMPLEMENTATION_CHECKLIST.md](NORMATIVE_ADMIN_DDL_DML_PSQL_TSQL_IMPLEMENTATION_CHECKLIST.md)
- [NORMATIVE_JDBC_SQL_PROMOTION_IMPLEMENTATION_CHECKLIST.md](NORMATIVE_JDBC_SQL_PROMOTION_IMPLEMENTATION_CHECKLIST.md)
- [OBJECT_NAMING_AND_QUOTING.md](OBJECT_NAMING_AND_QUOTING.md)
- `RC1_COMMAND_SYNTAX_COMPARISON.ebnf`
- [RESERVED_WORDS_AND_KEYWORDS.md](RESERVED_WORDS_AND_KEYWORDS.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [SYSTEM_COLUMNS.md](SYSTEM_COLUMNS.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Maintenance
- Update file list with `../skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh` when numbered-section files are added, removed, or renamed.

# TEMPORARY_TABLES_SPECIFICATION.md - Implementation Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/TEMPORARY_TABLES_SPECIFICATION.md`

Status notes:
- Document explicitly states **Non-Authoritative** and references `AUTHORITATIVE_SPEC_INVENTORY.md`, but contains an internal "Status: Authoritative (V3)" label. Treat as non-authoritative.

Summary:
- Parser recognizes TEMP/TEMPORARY and GLOBAL TEMPORARY and parses ON COMMIT actions into AST.
- V3 SBLR emission for CREATE TABLE does not encode temp_type or on_commit in the CREATE TABLE flags as required by this spec.
- Executor expects legacy/extended opcode bytecode for CREATE TABLE with temp flags in a single byte, not the SCHEMA_DDL_CREATE_TABLE payload emitted by the V3 emitter.

Key findings:

## Parser
[~] TEMP/TEMPORARY and GLOBAL TEMPORARY are parsed; AST captures `temp_type` and `on_commit` (`src/parser/parser_v3.cpp`, `include/scratchbird/parser/ast_v3.h`).
[ ] Dialect-specific ON COMMIT rejection rules are not enforced in parser (no dialect gating in V3 parser for MySQL vs PostgreSQL/Firebird).

## SBLR Encoding
[ ] Spec requires CREATE_TABLE flags bitfield (temp_type + on_commit) in SBLR; V3 emitter only sets `flags = if_not_exists` and ignores `temp_type` and `on_commit` (`src/parser/v3_emitter.cpp`).

## Executor
[~] Executor’s CREATE TABLE path reads legacy bytecode and extracts `flags` with temp_type/on_commit (`src/sblr/executor.cpp`), but this does not match V3 SCHEMA-based emission.
[ ] No V3 executor path shown that consumes SCHEMA_DDL_CREATE_TABLE temp flags or on_commit fields.

## Catalog / Metadata
[~] CatalogManager includes temp metadata fields (temp_metadata_scope, temp_data_scope, temp_on_commit, creating_session_id, temp_schema_id), indicating partial support (`src/core/catalog_manager.cpp`).

## Cleanup / Visibility
[ ] Session/transaction cleanup behaviors for temp tables are not verified in V3 execution path beyond catalog metadata handling.


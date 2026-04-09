# Legacy V3 Gap Migration Map

## Purpose
Record how key legacy `parser/v3` requirements were migrated into canonical section 28 documents and identify any remaining handoff points to other canonical sections.

## Legacy-to-Canonical Coverage

| Legacy Topic | Representative Legacy Files | Canonical Coverage | Status |
| --- | --- | --- | --- |
| Separated parser architecture and untrusted parser boundary | `docs/specifications_old/parser/v3/ARCHITECTURE_CLARIFICATIONS.md` | `PARSER_IMPLEMENTATION_CANONICAL_SPEC.md` | Covered |
| Dedicated parser per dialect, no fallback | `docs/specifications_old/parser/v3/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md` | `DECISION_RECORD.md`, `DIALECT_PROFILE_MATRIX.md` | Covered |
| Deterministic single path from parse to execute | `docs/specifications_old/parser/v3/V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md` | `SBLR_TRANSLATION_PIPELINE.md`, `PARSER_IMPLEMENTATION_CANONICAL_SPEC.md` | Covered |
| Parser-engine IPC metadata requirements | `docs/specifications_old/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md` | `SBLR_TRANSLATION_PIPELINE.md`, `PARSER_IMPLEMENTATION_CANONICAL_SPEC.md` | Covered |
| Prepared statement and streaming interaction model | `docs/specifications_old/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md` | `TEST_CONTRACT.md`, `PARSER_IMPLEMENTATION_CANONICAL_SPEC.md` | Covered |
| Listener handoff and parser worker lifecycle expectations | `docs/specifications_old/parser/v3/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md` | `PARSER_IMPLEMENTATION_CANONICAL_SPEC.md` and dependency on section 29 | Covered |
| Feature remap strategy (implement/remap/reject) | `docs/specifications_old/parser/v3/PARSER_REMAPPING_AND_IMPLEMENTATION_STRATEGY.md` | `DIALECT_PROFILE_MATRIX.md`, `SBLR_TRANSLATION_PIPELINE.md` | Covered |
| SQL or command traceability requirements | `docs/specifications_old/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md` | `SBLR_TRANSLATION_PIPELINE.md`, `ERROR_MAPPING_AND_DIAGNOSTICS.md` | Covered |
| Dialect-specific error mapping | `docs/specifications_old/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md` | `ERROR_MAPPING_AND_DIAGNOSTICS.md`, `TEST_CONTRACT.md` | Covered |
| Session variable and naming surface controls | `docs/specifications_old/parser/v3/SESSION_AND_UTILITY.md` | `DIALECT_PROFILE_MATRIX.md`, `PARSER_IMPLEMENTATION_CANONICAL_SPEC.md` | Covered |
| Driver conformance harness behavior | `docs/specifications_old/parser/v3/DRIVER_CONFORMANCE_TEST_HARNESS.md` | `TEST_CONTRACT.md` | Covered |
| Legacy focus on only Firebird/PostgreSQL/MySQL emulation | multiple parser-v3 docs | Expanded to include Cassandra, MongoDB, Neo4j, Redis, Milvus in `DIALECT_PROFILE_MATRIX.md` | Covered |

## Section Handoff Items
- Full grammar-level DDL and DML syntax definitions remain in section 21.
- Engine opcode payload definitions remain in section 22.
- Engine execution semantics remain in section 23.
- Catalog table definitions for profile and map storage remain in section 24.
- Wire protocol frame-level definitions remain in section 26.
- Listener control-plane details remain in section 29.

## Migration Notes
- Legacy content was used as requirement input only.
- Canonical wording and structure follow current section 28 templates and invariants.
- No legacy file in `legacy_imports` is treated as canonical authority.

## Audit normalization note (2026-03-28)
- Current code-backed parser authority is bounded to the native V3 stack (`parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`) plus dedicated shipped emulated SQL-family parser code for Firebird, PostgreSQL, and MySQL.
- Dedicated parser-agent and listener proof currently exists only for `sb_parser_fb`, `sb_parser_pg`, `sb_parser_mysql`, and the matching listener front doors; universal nine-family dedicated parser parity is not current implementation proof.
- Builtin emulation package scaffold proof is currently limited to `firebirdsql`, `postgresql`, and `mysql`.
- Cassandra, MongoDB, Neo4j, Redis, and Milvus are currently represented in this section by native-V3 feature vocabulary, catalog/runtime vocabulary, or checklist material rather than shipped dedicated parser implementations.
- Broad section-wide parity, corpus cardinality, and universal profile-generation claims are therefore bounded and must not be treated as present-day implementation proof without family-local source evidence.

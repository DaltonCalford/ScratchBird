# V3 Spec Gap Analysis (Initial Pass)

Date: 2026-02-08

This report maps each V3 specification to current implementation evidence in the codebase.
Statuses: **Implemented**, **Partial**, **Missing**. "Partial" often indicates a V2-era implementation or incomplete alignment with V3.

## Summary
- The codebase contains a **V2 parser + V2 SBLR pipeline**, with no explicit V3 parser or V3 SBLR implementation.
- Many V3 specs are therefore **missing** or only **partially** covered by V2-era code.
- This suggests **rewriting the parser/SBLR pipeline** is likely lower risk than incremental fixes.

## Spec-by-Spec Findings
| Spec | Status | Evidence | Corrections Needed |
| --- | --- | --- | --- |
| `/docs/specifications/parser/v3/server/SCRATCHBIRD_ARCHITECTURE_OVERVIEW.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/server/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/server/SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/server/SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/server/ARCHITECTURE_CLARIFICATIONS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/server/MEMORY_MANAGEMENT.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/server/SCRATCHBIRD_CONNECTION_RECOVERY_MODEL.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/server/SCRATCHBIRD_EMBEDDED_MODE_SPECIFICATION.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/IMPLEMENTATION_SAFETY_SUMMARY.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/AST_TYPE_AND_LITERAL_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CONTAINER.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CANONICALIZATION.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/PSQL_RUNTIME_V3.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DDL_SBDB.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_STRUCTURE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DOMAIN_MAP.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/catalog/SCHEMA_PATH_RESOLUTION.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/catalog/SCHEMA_PATH_SECURITY_DEFAULTS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/catalog/UUID_LIFECYCLE_RULES.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/types/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/types/03_TYPES_AND_DOMAINS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/types/BINARY_LAYOUT_ANNEX.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/types/SBLR_TYPE_MAP.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/storage/ON_DISK_FORMAT.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/storage/STORAGE_ENGINE_PAGE_MANAGEMENT.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/storage/STORAGE_ENGINE_BUFFER_POOL.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/storage/TOAST_LOB_STORAGE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/storage/HEAP_TOAST_INTEGRATION.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/transaction/TRANSACTION_MAIN.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/transaction/TRANSACTION_LOCK_MANAGER.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/INDEX_ARCHITECTURE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/INDEX_GC_PROTOCOL.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/AdvancedIndexes.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/server/SCRATCHBIRD_SECURITY_AND_ACCESS_MODEL.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/security/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/network/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/wire_protocols/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/api/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/scheduler/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/scheduler/SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/operations/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/operations/MONITORING_SQL_VIEWS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/operations/PROMETHEUS_METRICS_REFERENCE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/operations/LISTENER_POOL_METRICS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/operations/OID_MAPPING_STRATEGY.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/sblr/SBLR_V3_BYTECODE_EXAMPLES.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS_FULL.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md` | Missing (no V3 parser/emission rules) | Parser implementation is V2-only: `src/parser/parser_v2.cpp`, `src/parser/lexer_v2.cpp`, `src/parser/ast_v2.cpp`. | Implement V3 grammar, ambiguity resolution, and emission rules; replace V2 AST nodes where required. |
| `/docs/specifications/parser/v3/PARSER_AMBIGUITY_RESOLUTION.md` | Missing (no V3 parser/emission rules) | Parser implementation is V2-only: `src/parser/parser_v2.cpp`, `src/parser/lexer_v2.cpp`, `src/parser/ast_v2.cpp`. | Implement V3 grammar, ambiguity resolution, and emission rules; replace V2 AST nodes where required. |
| `/docs/specifications/parser/v3/query/QUERY_OPTIMIZER_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/query/PARALLEL_EXECUTION_ARCHITECTURE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/PERFORMANCE_BENCHMARKS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/types/VALUE_SPEC_STORAGE_ENCODINGS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/findings/NO_GREY_AREAS_GATE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/findings/DIALECT_GAP_EXAMPLES.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/server/BACKUP_AND_RESTORE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/deployment/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/server/INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/tools/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/tools/SB_BUILD_AND_TEST_CLI_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/testing/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/testing/DIALECT_CONFORMANCE_ASSERTIONS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/server/DATABASE_REGISTRY_SPECIFICATION_CORRECTED.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/ACCESS_CONTROL.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/BETA_SQL2023_IMPLEMENTATION_SPECIFICATION.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/DDL_ALTER.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/DDL_CREATE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/DDL_DROP_TRUNCATE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/DELETE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/INSERT.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/JOINS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/MERGE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/PSQL_STATEMENTS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/SBLR_V3_OLD_TO_NEW_MAPPING.md` | Missing (no V3 SBLR/executor code) | Only V2 SBLR pipeline present: `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`. | Implement V3 bytecode container/opcodes/payloads/validation/canonicalization and a V3 executor; update compiler to emit V3 SBLR. |
| `/docs/specifications/parser/v3/SELECT_AND_QUERY.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/SESSION_AND_UTILITY.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/TRANSACTION_CONTROL.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/UPDATE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/UTILITY_COPY.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/V3_SERVER_SPEC_INDEX.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/V3_ZERO_AMBIGUITY_BUILD_CHECKLIST.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/WINDOWING.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/AdaptiveRadixTreeIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/BITMAP_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/BRIN_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/BTREE_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/BloomFilterIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/COLUMNSTORE_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/CountMinSketchIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |

## Detailed Comparisons (Batch 1)

**Spec** `/docs/specifications/parser/v3/AST_TYPE_AND_LITERAL_SPEC.md`
Status: Not aligned with V3
Evidence: V2 AST types are `TypeName` + `LiteralExpr` in `include/scratchbird/parser/ast_v2.h`. No catalog UUIDs, no V3 `TypeSpec`, no V3 literal variants.
Correct: V2 supports basic literal kinds (int/float/string/blob/bool/null/default) and basic type modifiers (precision/scale/length/array/with_time_zone).
Incorrect: Missing V3 type/literal nodes (ENUM/SET/ROW/COMPOSITE/GEOMETRY/BIT/YEAR/DATETIME/BLOB_TEXT/JSONPATH), missing UUID v7 catalog IDs, and missing fully typed literal variants.
Corrections needed: Replace `TypeName`/`LiteralExpr` with V3 TypeSpec/Literal structures or add parallel V3 AST; resolve catalog IDs to UUID v7 before emission.

**Spec** `/docs/specifications/parser/v3/PARSER_AMBIGUITY_RESOLUTION.md`
Status: Not aligned with V3
Evidence: Expression precedence in `src/parser/parser_v2.cpp` uses `parseAddExpr` where `||` shares precedence with `+/-`, and comparison parsing mixes NOT IN/BETWEEN/LIKE in a custom order.
Correct: Left-associative AND/OR, basic unary NOT, support for `IS DISTINCT FROM`, `BETWEEN`, `IN`, `LIKE`, regex operators.
Incorrect: String concatenation precedence should be lower than additive but higher than comparisons; V2 treats it as additive. Additional precedence details for pattern and special comparisons are not enforced as specified.
Corrections needed: Rework expression precedence stack to match V3 ordering and add regression tests for ambiguous cases.

**Spec** `/docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md`
Status: Not aligned with V3
Evidence: Emission currently targets V2 bytecode (`src/sblr/bytecode_generator_v2.cpp`) and V2 semantic analyzer, with no V3 SBLR opcodes or payload rules.
Correct: Parser captures most structural elements for CREATE/SELECT/INSERT/UPDATE/DELETE/MERGE.
Incorrect: No V3 canonicalization, no V3 opcode emission, no required edge-case handling (DEFAULT emission, alias emission rules, CTAS rules, V3-only opcodes).
Corrections needed: Implement V3 SBLR emission layer, enforce identifier canonicalization, and implement all edge-case emission rules.

**Spec** `/docs/specifications/parser/v3/DDL_CREATE.md`
Status: Aligned with current parser
Evidence: Parsing algorithm and references match `src/parser/parser_v2.cpp` dispatch and CREATE handlers.
Correct: Modifier ordering, dispatch targets, and current constraints match spec.
Incorrect: None observed in parser; downstream emission still V2.
Corrections needed: Align emission to V3 SBLR once V3 pipeline exists.

**Spec** `/docs/specifications/parser/v3/DDL_ALTER.md`
Status: Aligned with current parser
Evidence: Parsing algorithm and references match `src/parser/parser_v2.cpp` ALTER dispatch and ALTER TABLE actions.
Correct: Supported ALTER TABLE actions and generic rename/move logic.
Incorrect: None observed in parser; downstream emission still V2.
Corrections needed: Align emission and action mapping to V3 SBLR actions.

**Spec** `/docs/specifications/parser/v3/DDL_DROP_TRUNCATE.md`
Status: Aligned with current parser
Evidence: Parsing algorithm and references match `src/parser/parser_v2.cpp`.
Correct: DROP/TRUNCATE dispatch and options.
Incorrect: None observed in parser; downstream emission still V2.
Corrections needed: Align emission to V3 SBLR.

**Spec** `/docs/specifications/parser/v3/SELECT_AND_QUERY.md`
Status: Aligned with current parser
Evidence: Parsing algorithm matches `src/parser/parser_v2.cpp` SELECT/CTE/set-op implementation.
Correct: WITH gatekeeper, select list, FROM/WHERE/GROUP/HAVING/ORDER/LIMIT/FETCH, set ops.
Incorrect: `FOR UPDATE/SHARE OF` table list parsed but discarded; may violate V3 if required.
Corrections needed: Preserve FOR UPDATE/SHARE target list if required by V3 execution rules.

**Spec** `/docs/specifications/parser/v3/JOINS.md`
Status: Aligned with current parser
Evidence: JOIN parsing matches `parseFromClause`, `parseTableRef`, `parseJoinType`, `parseJoin`.
Correct: NATURAL, INNER/LEFT/RIGHT/FULL/CROSS, ON/USING, LATERAL subqueries.
Incorrect: None observed in parser.
Corrections needed: None in parser; verify emission/semantic behavior.

**Spec** `/docs/specifications/parser/v3/WINDOWING.md`
Status: Aligned with current parser
Evidence: Window function detection and `OVER(...)` parsing match `src/parser/parser_v2.cpp`.
Correct: Window function set, argument validation, frame clauses.
Incorrect: None observed in parser.
Corrections needed: Ensure V3 type/literal and SBLR emission once pipeline exists.

**Spec** `/docs/specifications/parser/v3/INSERT.md`
Status: Aligned with current parser
Evidence: Parsing algorithm matches `parseInsert` and related helpers.
Correct: INTO/alias/columns, VALUES/SELECT/DEFAULT VALUES, ON CONFLICT, RETURNING.
Incorrect: DEFAULT in values is parsed as `LiteralType::DEFAULT` but V3 emission rules require `SBLR3_DEFAULT_VALUE`.
Corrections needed: Update emission to V3 default handling and conflict action mapping.

**Spec** `/docs/specifications/parser/v3/UPDATE.md`
Status: Aligned with current parser
Evidence: Parsing algorithm matches `parseUpdate` and helpers.
Correct: SET assignments, FROM clause, WHERE, RETURNING.
Incorrect: DEFAULT handling and alias emission rules not guaranteed in V2 bytecode.
Corrections needed: Enforce V3 emission rules for alias and default semantics.

**Spec** `/docs/specifications/parser/v3/DELETE.md`
Status: Aligned with current parser
Evidence: Parsing algorithm matches `parseDelete` and USING join parsing.
Correct: FROM, USING, WHERE, RETURNING.
Incorrect: USING emission rules not implemented in V2 SBLR.
Corrections needed: Implement V3 emission for USING and join lists.

**Spec** `/docs/specifications/parser/v3/MERGE.md`
Status: Aligned with current parser
Evidence: Parsing algorithm matches `parseMerge`.
Correct: MATCHED/NOT MATCHED clauses, UPDATE/DELETE/INSERT branches.
Incorrect: V3 emission rule forbids EXT_MERGE_* opcodes, but V2 has no V3 emission.
Corrections needed: Emit only V3 MERGE opcode with correct ordering and clause semantics.

**Spec** `/docs/specifications/parser/v3/SESSION_AND_UTILITY.md`
Status: Aligned with current parser
Evidence: Parsing algorithm matches `parseSet`, `parseShow`, `parseReset`, `parseExplain`, `parseAnalyze`, `parseConnect`, `parseDisconnect`.
Correct: Supported SET/SHOW/RESET/EXPLAIN/ANALYZE/SWEEP/COMMENT/CONNECT/DISCONNECT forms.
Incorrect: None observed in parser.
Corrections needed: Map results to V3 SBLR/engine actions as required.

**Spec** `/docs/specifications/parser/v3/TRANSACTION_CONTROL.md`
Status: Aligned with current parser
Evidence: Parsing algorithm matches `parseStartTransaction`, `parseCommit`, `parseRollback`, `parseSavepoint`.
Correct: Free-order characteristics, savepoints, prepared tx.
Incorrect: None observed in parser.
Corrections needed: Ensure V3 transaction characteristic encoding and validation.

**Spec** `/docs/specifications/parser/v3/UTILITY_COPY.md`
Status: Aligned with current parser
Evidence: Parsing algorithm matches `parseCopy`.
Correct: COPY (SELECT) TO, table FROM/TO, WITH options.
Incorrect: None observed in parser.
Corrections needed: Implement V3 option validation and emission rules.

**Spec** `/docs/specifications/parser/v3/PSQL_STATEMENTS.md`
Status: Aligned with current parser
Evidence: Dispatch and per-statement parsing matches `parsePSQLStatement` and helpers.
Correct: IF/CASE/WHILE/FOR/LOOP/RETURN/EXCEPTION/WHEN/CURSOR/EXECUTE.
Incorrect: V3 emission rules on scope shadowing and handler ordering are not enforced in V2 semantic analyzer.
Corrections needed: Implement V3 PSQL symbol table scoping and emission ordering.
| `/docs/specifications/parser/v3/indexes/FSTIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/GIN_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/GIST_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/GeohashS2Index.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/HASH_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/HNSW_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/HyperLogLogIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/INDEX_COMPLETION_CHECKLIST.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_GUIDE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_REFERENCE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/IVFIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/InvertedIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/JSONPathIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/LOW_LEVEL_SPECIFICATION_GIN_INDEX.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/LSMTimeSeriesIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/LSM_TREE_ARCHITECTURE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/LSM_TREE_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/LearnedIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/QuadtreeOctreeIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/RTREE_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/SPGIST_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/SuffixIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/ZOrderIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/indexes/ZoneMapsIndex.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/network/CONTROL_PLANE_PROTOCOL_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/network/DIALECT_AUTH_MAPPING_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/network/NETWORK_LAYER_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/network/PARSER_AGENT_SPEC.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/network/WIRE_PROTOCOL_SPECIFICATIONS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/network/Y_VALVE_DESIGN_PRINCIPLES.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/parser/01_SQL_DIALECT_OVERVIEW.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/parser/05_PSQL_PROCEDURAL_LANGUAGE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/parser/MYSQL_PARSER_SPECIFICATION.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/parser/POSTGRESQL_PARSER_SPECIFICATION.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_CORE_LANGUAGE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/parser/SCRATCHBIRD_UNIFIED_NOSQL_EXTENSIONS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/parser/ScratchBird Master Grammar Specification v2.0.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/parser/ScratchBird SQL Language Specification - Master Document.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/transaction/07_TRANSACTION_AND_SESSION_CONTROL.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/transaction/FIREBIRD_CONSTANTS_REFERENCE.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/transaction/README.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |
| `/docs/specifications/parser/v3/transaction/TRANSACTION_DISTRIBUTED.md` | Partial (V2 parser exists) | V2 parser/AST present: `src/parser/parser_v2.cpp`, `src/parser/ast_v2.cpp`, `src/parser/lexer_v2.cpp`. | Port grammar and AST to V3; reconcile dialect, statement coverage, and syntax rules with V3 specs. |

## Detailed Comparisons (Batch 2)

**Spec** `/docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
Status: Not aligned with V3
Evidence: Current opcode set is V2 in `include/scratchbird/sblr/opcodes.h` with `SBLR_VERSION = 2`.
Correct: V2 opcodes exist for core DDL/DML, but they are not the V3 optimized series.
Incorrect: No V3 opcode values, no V3 extended opcode series, no V3 container version.
Corrections needed: Replace opcode definitions with V3 series and update all emit/parse paths.

**Spec** `/docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md`
Status: Not aligned with V3
Evidence: Bytecode generator is V2 (`src/sblr/bytecode_generator_v2.cpp`) and emits V2 payload layouts.
Correct: V2 payloads exist but do not match V3 schemas.
Incorrect: No V3 payload layout handling for type specs, literals, or statement schemas.
Corrections needed: Implement V3 payload encoders/decoders and migrate AST emission accordingly.

**Spec** `/docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md`
Status: Not aligned with V3
Evidence: Executor executes V2 bytecode in `src/sblr/executor.cpp` and validates V2 in `src/sblr/bytecode_validator.cpp`.
Correct: Execution exists for V2 opcodes.
Incorrect: No V3 opcode semantics or validation rules implemented.
Corrections needed: Implement V3 executor semantics or a compatibility layer with full V3 coverage.

**Spec** `/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CONTAINER.md`
Status: Not aligned with V3
Evidence: Validator expects `Opcode::VERSION` then `SBLR_VERSION = 2` (`include/scratchbird/sblr/opcodes.h`).
Correct: V2 container format exists.
Incorrect: V3 container layout, headers, and sections not implemented.
Corrections needed: Implement V3 container format and migration tooling.

**Spec** `/docs/specifications/parser/v3/SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`
Status: Not aligned with V3
Evidence: No V3 constant pool structures; V2 emission directly embeds strings/ids.
Correct: V2 has limited symbol handling in bytecode generator.
Incorrect: No V3 constant pool, symbol table, or interning rules.
Corrections needed: Implement V3 constant pool and update emission/validation.

**Spec** `/docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md`
Status: Not aligned with V3
Evidence: Validation only checks V2 version/opcode sequencing (`src/sblr/bytecode_validator.cpp`).
Correct: Basic V2 validation exists.
Incorrect: No V3 structural validation, canonicalization, or payload schema checks.
Corrections needed: Implement V3 validator per spec.

**Spec** `/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CANONICALIZATION.md`
Status: Not aligned with V3
Evidence: No V3 canonicalization layer exists.
Correct: None.
Incorrect: V3 canonicalization rules are unimplemented.
Corrections needed: Implement canonicalization pass for deterministic bytecode output.

**Spec** `/docs/specifications/parser/v3/SBLR_V3_OLD_TO_NEW_MAPPING.md`
Status: Not aligned with V3
Evidence: No migration/mapping layer from V2 to V3 opcodes.
Correct: None.
Incorrect: Missing translation mapping for legacy bytecode.
Corrections needed: Implement mapping or drop legacy support explicitly.

**Spec** `/docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md`
Status: Not aligned with V3
Evidence: Executor reads V2 opcodes and payloads (`src/sblr/executor.cpp`).
Correct: V2 executor exists.
Incorrect: V3 bytecode execution semantics not present.
Corrections needed: Implement V3 executor or a compatibility shim with full opcode coverage.

**Spec** `/docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`
Status: Not aligned with V3
Evidence: SQL engine consumes V2 bytecode with V2 AST and semantic analyzer (`src/sblr/semantic_analyzer_v2.cpp`).
Correct: Core execution pipeline exists.
Incorrect: V3 execution semantics, type/literal rules, and engine contracts not aligned.
Corrections needed: Rework analyzer, optimizer, and executor to V3 contracts.

**Spec** `/docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`
Status: Not aligned with V3
Evidence: Executor contains lock/GC behaviors but not validated against V3 matrix.
Correct: Locking/GC exists for V2 behavior.
Incorrect: V3 matrix requirements not enforced or tested.
Corrections needed: Audit executor lock/GC behavior and implement V3 matrix compliance tests.

**Spec** `/docs/specifications/parser/v3/PSQL_RUNTIME_V3.md`
Status: Not aligned with V3
Evidence: PSQL compiled/executed via V2 analyzer/generator/executor.
Correct: PSQL runtime exists for V2.
Incorrect: V3 runtime semantics (scoping, exception order, emission rules) not enforced.
Corrections needed: Implement V3 PSQL runtime semantics and symbol scoping rules.

## Detailed Comparisons (Batch 3)

**Spec** `/docs/specifications/parser/v3/parser/01_SQL_DIALECT_OVERVIEW.md`
Status: Partially aligned
Evidence: Native parser exists (`src/parser/parser_v2.cpp`) with multi-dialect hooks and separate emulated parsers (`src/parser/mysql/*`, `src/parser/postgresql/*`).
Correct: Core DDL/DML/PSQL/utility coverage largely exists in parser and executor.
Incorrect: Several advertised dialect features (MSSQL TOP/IDENTITY, TRY/EXCEPT, full multi-dialect translation) are not fully implemented in the V2 parser/PSQL runtime.
Corrections needed: Validate each stated feature against actual parser and enforce the V3 dialect boundary rules.

**Spec** `/docs/specifications/parser/v3/parser/05_PSQL_PROCEDURAL_LANGUAGE.md`
Status: Partially aligned
Evidence: PSQL parser and runtime exist in `src/parser/parser_v2.cpp` and `src/sblr/executor.cpp`.
Correct: Core IF/CASE/WHILE/FOR/LOOP, EXECUTE BLOCK/PROCEDURE, cursors, exceptions.
Incorrect: Spec mentions TRY/EXCEPT and additional constructs not in current parser; V3 runtime semantics not enforced (scoping, typed literals).
Corrections needed: Implement missing constructs or update spec; enforce V3 PSQL runtime semantics.

**Spec** `/docs/specifications/parser/v3/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
Status: Partially aligned
Evidence: EMULATED parsing path exists in `src/parser/parser_v2.cpp` and parser agents exist in `src/parser/mysql/*`, `src/parser/postgresql/*`.
Correct: EMULATED dialect selection and source specification parsing.
Incorrect: Full emulation coverage and canonical ScratchBird emission rules are not implemented.
Corrections needed: Complete emulation adapters and V3 canonical emission.

**Spec** `/docs/specifications/parser/v3/parser/MYSQL_PARSER_SPECIFICATION.md`
Status: Partially aligned
Evidence: MySQL lexer/parser exists in `src/parser/mysql/*`.
Correct: Basic MySQL parsing pipeline present.
Incorrect: No V3 SBLR emission; coverage vs spec not verified.
Corrections needed: Align MySQL grammar coverage and emit V3 canonical SBLR.

**Spec** `/docs/specifications/parser/v3/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
Status: Partially aligned
Evidence: PostgreSQL lexer/parser exists in `src/parser/postgresql/*`.
Correct: Basic PostgreSQL parsing pipeline present.
Incorrect: No V3 SBLR emission; coverage vs spec not verified.
Corrections needed: Align PostgreSQL grammar coverage and emit V3 canonical SBLR.

**Spec** `/docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
Status: Partially aligned
Evidence: V2 grammar is implemented in `src/parser/parser_v2.cpp` and V2 lexer.
Correct: Many productions implemented (DDL/DML/PSQL).
Incorrect: Grammar likely diverges in precedence, type/literal rules, and V3-only constructs.
Corrections needed: Reconcile grammar with V3 BNF and update parser/lexer.

**Spec** `/docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_CORE_LANGUAGE.md`
Status: Partially aligned
Evidence: Core language parsing present in V2 parser.
Correct: Core DDL/DML parsing exists.
Incorrect: V3 type/literal system and emission rules not implemented; some dialect-only constructs likely violate V3 separation.
Corrections needed: Enforce V3 core language rules and update AST/type system.

**Spec** `/docs/specifications/parser/v3/parser/SCRATCHBIRD_UNIFIED_NOSQL_EXTENSIONS.md`
Status: Not aligned with V3
Evidence: No explicit parser/runtime support for unified NoSQL extensions found in core parser.
Correct: None.
Incorrect: Missing syntax handling and execution semantics.
Corrections needed: Implement parser support and execution semantics for unified NoSQL extensions.

**Spec** `/docs/specifications/parser/v3/parser/ScratchBird Master Grammar Specification v2.0.md`
Status: Partially aligned
Evidence: V2 parser appears to track the v2 master grammar.
Correct: V2 grammar coverage largely implemented.
Incorrect: V3 corrected grammar not enforced.
Corrections needed: Reconcile with V3 grammar documents and update parser.

**Spec** `/docs/specifications/parser/v3/parser/ScratchBird SQL Language Specification - Master Document.md`
Status: Partially aligned
Evidence: V2 parser covers much of the described language.
Correct: Core DDL/DML/PSQL exist.
Incorrect: V3 corrections and constraints not enforced.
Corrections needed: Align to V3 constraints and emission rules.

**Spec** `/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DOMAIN_MAP.md`
Status: Partially aligned
Evidence: Catalog manager exists (`src/core/catalog_manager.cpp`, `src/catalog/*`).
Correct: Catalog subsystem and domain support present.
Incorrect: V3 domain mapping rules and catalog schema details not verified.
Corrections needed: Audit catalog schema and domain mappings against V3.

**Spec** `/docs/specifications/parser/v3/catalog/UUID_LIFECYCLE_RULES.md`
Status: Partially aligned
Evidence: UUID v7 utilities exist (`src/core/uuidv7.cpp`); IDs used across core/catalog/index types.
Correct: UUID v7 is present and used for IDs.
Incorrect: Lifecycle rules (generation, reuse, persistence) not verified against spec.
Corrections needed: Audit ID lifecycle, persistence, and catalog usage vs V3 rules.

**Spec** `/docs/specifications/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md`
Status: Partially aligned
Evidence: IPC layer exists (`src/ipc/*`), parser agent interfaces (`src/ipc/parser_agent.cpp`).
Correct: IPC framework and parser agent channels exist.
Incorrect: V3 contract fields, error codes, and message ordering not validated.
Corrections needed: Align IPC schemas and error handling with V3 contract.

**Spec** `/docs/specifications/parser/v3/network/PARSER_AGENT_SPEC.md`
Status: Partially aligned
Evidence: Parser agent exists in `src/ipc/parser_agent.cpp` and `src/ipc/external_agents/*`.
Correct: Agent structure and request/response flow exist.
Incorrect: V3 protocol semantics and coverage not verified.
Corrections needed: Implement V3 agent protocol and capability reporting.

**Spec** `/docs/specifications/parser/v3/network/NETWORK_LAYER_SPEC.md`
Status: Partially aligned
Evidence: Network layer exists (`src/network/*`, `src/protocol/*`).
Correct: Listener, connection handler, protocol adapters exist.
Incorrect: V3 network layer requirements (timeouts, backpressure, metrics) not verified.
Corrections needed: Audit and align network behavior to V3 spec.

**Spec** `/docs/specifications/parser/v3/network/CONTROL_PLANE_PROTOCOL_SPEC.md`
Status: Partially aligned
Evidence: Control plane exists in `src/network/control_plane.cpp`.
Correct: Control plane implementation exists.
Incorrect: V3 protocol schema and commands not verified.
Corrections needed: Align control plane messages and commands to V3 spec.

**Spec** `/docs/specifications/parser/v3/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`
Status: Partially aligned
Evidence: Listener and thread pool exist (`src/network/sb_listener_main.cpp`, `src/network/thread_pool.cpp`).
Correct: Listener + pool architecture present.
Incorrect: Pool sizing, scheduling, and metrics may diverge from V3.
Corrections needed: Audit listener/pool behavior vs V3 requirements.

**Spec** `/docs/specifications/parser/v3/network/WIRE_PROTOCOL_SPECIFICATIONS.md`
Status: Partially aligned
Evidence: Wire protocol handling exists (`src/protocol/*`, `src/network/connection_handler.cpp`).
Correct: PostgreSQL v3 wire protocol support present.
Incorrect: Full V3 wire protocol set and mapping not verified.
Corrections needed: Validate protocol coverage and compliance with V3 specs.

**Spec** `/docs/specifications/parser/v3/network/DIALECT_AUTH_MAPPING_SPEC.md`
Status: Partially aligned
Evidence: Dialect handling appears in parser and connection context; auth in `src/security/*`.
Correct: Dialect tags exist in parser/SBLR pipeline.
Incorrect: Spec-defined mapping between dialect and auth not verified.
Corrections needed: Implement explicit mapping per V3 spec and tests.

**Spec** `/docs/specifications/parser/v3/operations/MONITORING_SQL_VIEWS.md`
Status: Partially aligned
Evidence: Executor contains monitoring views (`src/sblr/executor.cpp` has monitoring queries).
Correct: Some monitoring output exists.
Incorrect: V3 view definitions and columns not verified.
Corrections needed: Implement/align monitoring views with V3 definitions.

**Spec** `/docs/specifications/parser/v3/operations/PROMETHEUS_METRICS_REFERENCE.md`
Status: Partially aligned
Evidence: Metrics hooks appear in server/network; no full Prometheus export verified.
Correct: Some metrics counters exist.
Incorrect: V3 metrics set and labels not verified.
Corrections needed: Implement Prometheus metrics per V3 reference.

**Spec** `/docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md`
Status: Partially aligned
Evidence: Dialect tags exist; monitoring mappings not verified.
Correct: Dialect tags and parser settings exist.
Incorrect: Mapping tables not implemented.
Corrections needed: Implement V3 mapping views.

**Spec** `/docs/specifications/parser/v3/operations/LISTENER_POOL_METRICS.md`
Status: Partially aligned
Evidence: Listener/pool exist; metrics not verified.
Correct: Thread pool exists.
Incorrect: Metrics definitions not implemented.
Corrections needed: Add metrics per V3 spec.

**Spec** `/docs/specifications/parser/v3/operations/OID_MAPPING_STRATEGY.md`
Status: Partially aligned
Evidence: OID handling appears in PostgreSQL protocol adapter (`src/protocol/adapters/postgresql_adapter.cpp`).
Correct: Some OID mapping logic exists.
Incorrect: V3 OID mapping strategy not verified.
Corrections needed: Align OID mapping strategy and catalog mappings.

**Spec** `/docs/specifications/parser/v3/types/BINARY_LAYOUT_ANNEX.md`
Status: Partially aligned
Evidence: Type encoding exists in `src/core/typed_value.cpp`.
Correct: Basic binary encodings exist.
Incorrect: V3 type layout rules and extended types not implemented.
Corrections needed: Update encoding and persistence to V3 type system.

**Spec** `/docs/specifications/parser/v3/types/SBLR_TYPE_MAP.md`
Status: Not aligned with V3
Evidence: V2 SBLR opcodes include type markers in `include/scratchbird/sblr/opcodes.h`.
Correct: V2 type opcodes exist.
Incorrect: No V3 SBLR type map or TypeSpec encoding.
Corrections needed: Implement V3 type mapping and update compiler/executor.

**Spec** `/docs/specifications/parser/v3/types/VALUE_SPEC_STORAGE_ENCODINGS.md`
Status: Partially aligned
Evidence: Storage encodings exist in `src/core/typed_value.cpp` and storage engine.
Correct: Base type encodings exist.
Incorrect: V3 value specs and new types not implemented.
Corrections needed: Align storage encodings with V3 value spec.

**Spec** `/docs/specifications/parser/v3/transaction/TRANSACTION_MAIN.md`
Status: Partially aligned
Evidence: Transaction manager exists in `src/core/transaction_manager.cpp`.
Correct: Core transaction lifecycle exists.
Incorrect: V3 MGA and distributed transaction rules not verified.
Corrections needed: Audit transaction behavior vs V3 specs.

**Spec** `/docs/specifications/parser/v3/transaction/TRANSACTION_LOCK_MANAGER.md`
Status: Partially aligned
Evidence: Lock manager exists in `include/scratchbird/core/lock_manager.h` and `src/core/lock_manager.cpp`.
Correct: Locking subsystem exists.
Incorrect: V3 lock modes and conflict matrices not verified.
Corrections needed: Align lock manager to V3 rules and tests.

**Spec** `/docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md`
Status: Partially aligned
Evidence: MGA-style visibility is used in indexes and storage (e.g., `src/core/btree.cpp`).
Correct: MGA concepts present.
Incorrect: V3 MGA rules not verified.
Corrections needed: Audit MGA visibility and sweep rules vs V3 spec.

**Spec** `/docs/specifications/parser/v3/indexes/INDEX_ARCHITECTURE.md`
Status: Partially aligned
Evidence: Index implementations exist across `src/core/*_index.cpp`.
Correct: Multiple index types implemented (BTREE, GIN, GIST, RTREE, BRIN, HASH, LSM, HNSW, etc.).
Incorrect: V3 index architecture rules and GC protocols not verified.
Corrections needed: Audit each index implementation vs V3 architecture spec.

**Spec** `/docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md`
Status: Partially aligned
Evidence: Index factory and concrete indexes exist (`src/core/index_factory.cpp`, `src/core/*_index.cpp`).
Correct: Index APIs and creation exist.
Incorrect: V3 implementation requirements and payload formats not verified.
Corrections needed: Align each index to V3 specification.

**Spec** `/docs/specifications/parser/v3/indexes/INDEX_GC_PROTOCOL.md`
Status: Partially aligned
Evidence: GC manager exists (`src/core/gc_manager.cpp`), index GC hooks in executor.
Correct: GC infrastructure exists.
Incorrect: V3 index GC protocol not verified.
Corrections needed: Align index GC behavior to V3 protocol.

**Spec** `/docs/specifications/parser/v3/indexes/AdvancedIndexes.md`
Status: Partially aligned
Evidence: Some advanced index types exist (HNSW, columnstore, inverted, bitmap).
Correct: Partial advanced index coverage.
Incorrect: Many advanced index types in spec are missing or incomplete.
Corrections needed: Implement missing advanced index types or explicitly defer.

## Detailed Comparisons (Batch 4)

**Spec** `/docs/specifications/parser/v3/server/SCRATCHBIRD_ARCHITECTURE_OVERVIEW.md`
Status: Partially aligned
Evidence: Server runtime exists (`src/server/*`, `src/network/*`).
Correct: Core server components, listener, connection handling are present.
Incorrect: V3 architecture constraints and component boundaries not verified.
Corrections needed: Audit server module boundaries and dataflow vs V3 architecture.

**Spec** `/docs/specifications/parser/v3/server/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md`
Status: Partially aligned
Evidence: Consolidated server subsystems exist but no explicit V3 alignment audit.
Correct: Server/core subsystems present.
Incorrect: V3 consolidated architecture requirements not verified.
Corrections needed: Map current server modules to V3 architecture and resolve gaps.

**Spec** `/docs/specifications/parser/v3/server/SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md`
Status: Partially aligned
Evidence: Connection lifecycle implemented in `src/network/connection_handler.cpp` and `src/server/server_session.cpp`.
Correct: Connection handling and session lifecycle exist.
Incorrect: V3 lifecycle states and error handling not verified.
Corrections needed: Audit lifecycle transitions and failure handling vs V3.

**Spec** `/docs/specifications/parser/v3/server/SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md`
Status: Partially aligned
Evidence: Startup handled in `src/server/sb_server_main.cpp`, `src/server/daemon.cpp`.
Correct: Startup/shutdown flow present.
Incorrect: V3 startup phases and checks not verified.
Corrections needed: Align startup sequence and required checks with V3.

**Spec** `/docs/specifications/parser/v3/server/ARCHITECTURE_CLARIFICATIONS.md`
Status: Partially aligned
Evidence: Clarifications not yet mapped to implementation.
Correct: None verified.
Incorrect: Unknown until audited.
Corrections needed: Compare each clarification item to implementation.

**Spec** `/docs/specifications/parser/v3/server/MEMORY_MANAGEMENT.md`
Status: Partially aligned
Evidence: Custom allocators and pools exist (`src/pool/*`, arena usage in parser).
Correct: Memory pool usage exists.
Incorrect: V3 memory management rules not verified.
Corrections needed: Audit pool ownership, lifetimes, and leak strategy vs V3.

**Spec** `/docs/specifications/parser/v3/server/SCRATCHBIRD_CONNECTION_RECOVERY_MODEL.md`
Status: Partially aligned
Evidence: Connection recovery handled in `src/network/connection_handler.cpp` and session layer.
Correct: Basic recovery/cleanup exists.
Incorrect: V3 recovery semantics not verified.
Corrections needed: Align recovery state machine to V3 model.

**Spec** `/docs/specifications/parser/v3/server/SCRATCHBIRD_EMBEDDED_MODE_SPECIFICATION.md`
Status: Partially aligned
Evidence: Embedded mode not clearly implemented; only general server/client code exists.
Correct: None verified.
Incorrect: Embedded mode support likely incomplete.
Corrections needed: Implement or document embedded mode behavior per V3.

**Spec** `/docs/specifications/parser/v3/README.md`
Status: Partially aligned
Evidence: Parser and server exist; README not fully validated against implementation.
Correct: General project description matches components.
Incorrect: V3-specific claims not verified.
Corrections needed: Update README or code to ensure compliance.

**Spec** `/docs/specifications/parser/v3/IMPLEMENTATION_SAFETY_SUMMARY.md`
Status: Partially aligned
Evidence: No systematic safety compliance checks found.
Correct: Some safety checks exist (parser errors, bytecode validation).
Incorrect: V3 safety requirements not verified.
Corrections needed: Audit safety requirements and implement missing checks.

**Spec** `/docs/specifications/parser/v3/V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md`
Status: Partially aligned
Evidence: Current codebase mixes v2/v3 specs; no single-path implementation enforced.
Correct: Some consolidation exists in parser and executor.
Incorrect: V3 single-path requirements not enforced.
Corrections needed: Refactor to a single V3 pipeline.

**Spec** `/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DDL_SBDB.md`
Status: Partially aligned
Evidence: Catalog DDL exists in `src/catalog/*` and `src/core/catalog_manager.cpp`.
Correct: System catalog definitions and DDL handling exist.
Incorrect: V3 catalog schema likely diverges.
Corrections needed: Compare catalog schema and DDL to V3 SBDB definitions.

**Spec** `/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_STRUCTURE.md`
Status: Partially aligned
Evidence: System catalog tables exist in `src/catalog/sys_catalog.cpp`.
Correct: Basic system catalog structure exists.
Incorrect: V3 structure and columns not verified.
Corrections needed: Align system catalog tables to V3 structure.

**Spec** `/docs/specifications/parser/v3/catalog/SCHEMA_PATH_RESOLUTION.md`
Status: Partially aligned
Evidence: Schema path exists (`src/parser/schema_path_v2.*`).
Correct: Schema path parsing exists.
Incorrect: V3 resolution rules not verified.
Corrections needed: Align resolution and search path behavior with V3.

**Spec** `/docs/specifications/parser/v3/catalog/SCHEMA_PATH_SECURITY_DEFAULTS.md`
Status: Partially aligned
Evidence: Security and schema defaults exist in config/security layers.
Correct: Basic access model exists.
Incorrect: V3 schema path security defaults not verified.
Corrections needed: Implement security defaults per V3.

**Spec** `/docs/specifications/parser/v3/types/README.md`
Status: Partially aligned
Evidence: Type system exists but not fully V3 aligned.
Correct: Basic types and encodings present.
Incorrect: V3 type rules and mappings not fully implemented.
Corrections needed: Align type system to V3 spec set.

**Spec** `/docs/specifications/parser/v3/types/03_TYPES_AND_DOMAINS.md`
Status: Partially aligned
Evidence: Type/domain support exists in parser and catalog.
Correct: Core domains and types exist.
Incorrect: V3 extended types not implemented.
Corrections needed: Implement V3 domain/type behaviors and constraints.

**Spec** `/docs/specifications/parser/v3/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md`
Status: Partially aligned
Evidence: Type persistence and casts exist in `src/core/typed_value.cpp` and executor.
Correct: Base type casting exists.
Incorrect: V3-specific persistence rules not verified.
Corrections needed: Align cast and storage persistence rules with V3.

**Spec** `/docs/specifications/parser/v3/storage/ON_DISK_FORMAT.md`
Status: Partially aligned
Evidence: Storage engine exists (`src/core/storage_engine.cpp`, `src/core/heap_page.h`).
Correct: On-disk formats exist.
Incorrect: V3 on-disk format not verified.
Corrections needed: Compare on-disk page layouts to V3.

**Spec** `/docs/specifications/parser/v3/storage/STORAGE_ENGINE_PAGE_MANAGEMENT.md`
Status: Partially aligned
Evidence: Page management in `src/core/storage_engine.cpp`, `src/core/heap_page.*`.
Correct: Page allocation and management exist.
Incorrect: V3 page management rules not verified.
Corrections needed: Audit page lifecycle and free space management vs V3.

**Spec** `/docs/specifications/parser/v3/storage/STORAGE_ENGINE_BUFFER_POOL.md`
Status: Partially aligned
Evidence: Buffer pool exists in `src/core/buffer_pool.*` (if present) or storage engine.
Correct: Buffering logic exists.
Incorrect: V3 buffer pool policies not verified.
Corrections needed: Align buffer pool behavior to V3 spec.

**Spec** `/docs/specifications/parser/v3/storage/TOAST_LOB_STORAGE.md`
Status: Partially aligned
Evidence: LOB/TOAST handling exists in `src/core/storage_engine.cpp` and LOB helpers.
Correct: Some LOB support exists.
Incorrect: V3 TOAST rules not verified.
Corrections needed: Align TOAST/LOB storage to V3 spec.

**Spec** `/docs/specifications/parser/v3/storage/HEAP_TOAST_INTEGRATION.md`
Status: Partially aligned
Evidence: Heap and LOB integration exist in storage engine.
Correct: Basic integration exists.
Incorrect: V3 integration rules not verified.
Corrections needed: Align heap/TOAST integration to V3.

**Spec** `/docs/specifications/parser/v3/server/SCRATCHBIRD_SECURITY_AND_ACCESS_MODEL.md`
Status: Partially aligned
Evidence: Security subsystem exists (`src/security/*`, `src/core/auth*`).
Correct: Authentication and access control exist.
Incorrect: V3 access model not fully verified.
Corrections needed: Audit roles, privileges, and security defaults vs V3.

**Spec** `/docs/specifications/parser/v3/security/README.md`
Status: Partially aligned
Evidence: Security module exists but not verified against V3 doc.
Correct: Security primitives exist.
Incorrect: V3 security doc compliance not verified.
Corrections needed: Align security policy and configuration to V3.

**Spec** `/docs/specifications/parser/v3/network/README.md`
Status: Partially aligned
Evidence: Network subsystem exists.
Correct: Listener and connection handling implemented.
Incorrect: V3 network doc requirements not verified.
Corrections needed: Audit network docs vs implementation.

**Spec** `/docs/specifications/parser/v3/wire_protocols/README.md`
Status: Partially aligned
Evidence: Protocol adapters exist (`src/protocol/*`).
Correct: PostgreSQL v3 protocol support exists.
Incorrect: Full V3 wire protocol set not verified.
Corrections needed: Align wire protocol support to V3 specs.

**Spec** `/docs/specifications/parser/v3/api/README.md`
Status: Partially aligned
Evidence: Client API exists (`src/client/*`), but V3 API contracts not verified.
Correct: Client connection and statement execution exist.
Incorrect: API contract compliance not verified.
Corrections needed: Align API surface and semantics to V3 docs.

**Spec** `/docs/specifications/parser/v3/scheduler/README.md`
Status: Partially aligned
Evidence: Scheduler utilities exist (`src/core/job_scheduler_utils.cpp`).
Correct: Some scheduling support exists.
Incorrect: Full scheduler system not implemented.
Corrections needed: Implement scheduler service per V3.

**Spec** `/docs/specifications/parser/v3/scheduler/SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md`
Status: Partially aligned
Evidence: No dedicated job runner service found.
Correct: Job parsing exists in parser.
Incorrect: Runner execution semantics not implemented.
Corrections needed: Implement job runner per V3 spec.

**Spec** `/docs/specifications/parser/v3/operations/README.md`
Status: Partially aligned
Evidence: Ops functionality exists but not verified.
Correct: Some monitoring and metrics present.
Incorrect: V3 ops doc compliance not verified.
Corrections needed: Align ops tooling to V3.

**Spec** `/docs/specifications/parser/v3/sblr/SBLR_V3_BYTECODE_EXAMPLES.md`
Status: Not aligned with V3
Evidence: No V3 bytecode generation to compare examples.
Correct: None.
Incorrect: Examples cannot be produced by current pipeline.
Corrections needed: Implement V3 SBLR and validate against examples.

**Spec** `/docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS.md`
Status: Not aligned with V3
Evidence: No V3 bytecode generator; cannot run vectors.
Correct: None.
Incorrect: Test vectors not executable.
Corrections needed: Implement V3 SBLR pipeline and conformance tests.

**Spec** `/docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS_FULL.md`
Status: Not aligned with V3
Evidence: No V3 bytecode generator; cannot run vectors.
Correct: None.
Incorrect: Full vectors cannot be validated.
Corrections needed: Implement V3 SBLR pipeline.

**Spec** `/docs/specifications/parser/v3/query/QUERY_OPTIMIZER_SPEC.md`
Status: Partially aligned
Evidence: Optimizer module exists (`src/optimizer/*`).
Correct: Optimizer framework present.
Incorrect: V3 optimizer rules and cost model not verified.
Corrections needed: Align optimizer with V3 spec.

**Spec** `/docs/specifications/parser/v3/query/PARALLEL_EXECUTION_ARCHITECTURE.md`
Status: Partially aligned
Evidence: Thread pool exists; no explicit parallel query engine found.
Correct: Basic concurrency primitives exist.
Incorrect: V3 parallel execution architecture not implemented.
Corrections needed: Implement parallel query pipeline per V3 spec.

**Spec** `/docs/specifications/parser/v3/PERFORMANCE_BENCHMARKS.md`
Status: Partially aligned
Evidence: Benchmark scripts and docs exist; no conformance data verified.
Correct: Benchmarking intent exists.
Incorrect: V3 benchmark targets not measured.
Corrections needed: Implement benchmark suite and report results.

**Spec** `/docs/specifications/parser/v3/findings/NO_GREY_AREAS_GATE.md`
Status: Partially aligned
Evidence: No automated gate enforcing the “no grey areas” rule.
Correct: Documentation exists.
Incorrect: Gate not implemented.
Corrections needed: Add enforcement checks in CI/build tooling.

**Spec** `/docs/specifications/parser/v3/findings/DIALECT_GAP_EXAMPLES.md`
Status: Partially aligned
Evidence: Some dialect gaps known; no formal gap tracking in code.
Correct: Dialect examples exist.
Incorrect: No enforcement or tests.
Corrections needed: Add tests for dialect gaps and update emulation rules.

**Spec** `/docs/specifications/parser/v3/server/BACKUP_AND_RESTORE.md`
Status: Partially aligned
Evidence: Backup manager exists (`src/core/backup_manager.*`).
Correct: Backup/restore infrastructure exists.
Incorrect: V3 backup/restore flow not verified.
Corrections needed: Align backup/restore behavior with V3 spec.

**Spec** `/docs/specifications/parser/v3/deployment/README.md`
Status: Partially aligned
Evidence: Deployment tooling exists (`docs/deployment`, scripts) but not verified.
Correct: Some deployment docs exist.
Incorrect: V3 deployment requirements not validated.
Corrections needed: Align deployment steps and tooling to V3.

**Spec** `/docs/specifications/parser/v3/server/INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md`
Status: Partially aligned
Evidence: Server init logic exists (`src/server/*`, `src/core/database.*`).
Correct: Initialization routines present.
Incorrect: V3 initialization requirements not verified.
Corrections needed: Align initialization to V3 spec.

**Spec** `/docs/specifications/parser/v3/tools/README.md`
Status: Partially aligned
Evidence: Tooling exists in `tools/` but not validated.
Correct: Some tools present.
Incorrect: V3 tool requirements not verified.
Corrections needed: Align tool suite to V3 docs.

**Spec** `/docs/specifications/parser/v3/tools/SB_BUILD_AND_TEST_CLI_SPEC.md`
Status: Partially aligned
Evidence: Build/test scripts exist (`scripts/`, `tools/`).
Correct: Some CLI tooling exists.
Incorrect: V3 CLI spec not verified.
Corrections needed: Implement CLI per V3 spec.

**Spec** `/docs/specifications/parser/v3/testing/README.md`
Status: Partially aligned
Evidence: Tests exist in `tests/` but not verified against V3 testing plan.
Correct: Test infrastructure exists.
Incorrect: V3 testing coverage not implemented.
Corrections needed: Align test plans and suites with V3 docs.

**Spec** `/docs/specifications/parser/v3/testing/DIALECT_CONFORMANCE_ASSERTIONS.md`
Status: Partially aligned
Evidence: No automated conformance suite identified.
Correct: None.
Incorrect: Assertions not enforced.
Corrections needed: Implement conformance tests for dialects.

**Spec** `/docs/specifications/parser/v3/server/DATABASE_REGISTRY_SPECIFICATION_CORRECTED.md`
Status: Partially aligned
Evidence: Database registry exists in `src/core/database_registry.*` and catalog manager.
Correct: Registry subsystem exists.
Incorrect: V3 corrected spec not verified.
Corrections needed: Align registry schema and lifecycle to V3.

**Spec** `/docs/specifications/parser/v3/ACCESS_CONTROL.md`
Status: Partially aligned
Evidence: Access control logic exists in security module and catalog grants.
Correct: Basic GRANT/REVOKE and privilege checks exist.
Incorrect: V3 access control specifics not verified.
Corrections needed: Align access control model to V3 spec.

**Spec** `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`
Status: N/A (inventory)
Evidence: Inventory file present.
Correct: Lists authoritative specs.
Incorrect: None.
Corrections needed: None.

**Spec** `/docs/specifications/parser/v3/BETA_SQL2023_IMPLEMENTATION_SPECIFICATION.md`
Status: Partially aligned
Evidence: SQL2023 features partially present in parser/executor.
Correct: Some standard features exist.
Incorrect: Full SQL2023 coverage not verified.
Corrections needed: Audit SQL2023 features vs V3 spec.

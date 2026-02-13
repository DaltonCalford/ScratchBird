# ScratchBird V3 Parser Implementation Specifications

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


This folder consolidates V2 alpha/beta parser specifications into a single
implementation-first view. The intent is to document *how the parser is currently implemented*
so that the SQL dialect can be re-implemented without ambiguity.

## Authoritative Scope Notice

- This V3 tree is the authoritative specification source for implementation.
- **Non-authoritative** folders: `archive/`, `planning/`, `status/`, `audits/`, `project/`, `issues/`,
  `testing/` (except `testing/DIALECT_CONFORMANCE_ASSERTIONS.md`).
  These are historical logs or coordination artifacts and MUST NOT be used for implementation.
- `Alpha_Phase_1_Archive/` is **non-authoritative** historical material and must not be used for implementation unless explicitly instructed.
- Non-authoritative or archived documents must be treated as reference only and never override V3 specifications.

For the full server build specification set, see:
- `/docs/specifications/parser/v3/V3_SERVER_SPEC_INDEX.md`

## Consolidated Sources (V2 Alpha/Beta)

Primary inputs used for V3 consolidation:
- `/docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_CORE_LANGUAGE.md`
- `/docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `/docs/specifications/parser/v3/parser/ScratchBird Master Grammar Specification v2.0.md`
- `/docs/specifications/parser/v3/parser/ScratchBird SQL Language Specification - Master Document.md`
- `/docs/specifications/parser/v3/parser/01_SQL_DIALECT_OVERVIEW.md`
- `/docs/specifications/parser/v3/parser/SCRATCHBIRD_UNIFIED_NOSQL_EXTENSIONS.md`
- `/docs/specifications/parser/v3/parser/05_PSQL_PROCEDURAL_LANGUAGE.md`
- `/docs/specifications/parser/v3/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `/docs/specifications/parser/v3/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `/docs/specifications/parser/v3/parser/MYSQL_PARSER_SPECIFICATION.md`

V3 focuses on the native ScratchBird parser implementation (V2 parser code). Emulated
parsers (PostgreSQL/MySQL/Firebird/MSSQL) remain separate and are not
merged into the core V3 command algorithms.

**Reject policy (mandatory):** If an emulated dialect or any reserved feature is
disabled, the parser MUST reject the statement with `ERR_FEATURE_DISABLED` and
MUST NOT attempt partial parsing or silent fallback.

## V3 Documents (Implementation-First)

Each document provides step-by-step parsing behavior and explicit references to the
current implementation locations (path/file/line). All command docs are written to
avoid undocumented behavior.

**Authoritative rule:** Only files listed in
`/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative for V3
implementation. All other files in this tree are non-authoritative references and
MUST NOT be used to implement behavior.

- `/docs/specifications/parser/v3/SELECT_AND_QUERY.md`
- `/docs/specifications/parser/v3/JOINS.md`
- `/docs/specifications/parser/v3/WINDOWING.md`
- `/docs/specifications/parser/v3/INSERT.md`
- `/docs/specifications/parser/v3/UPDATE.md`
- `/docs/specifications/parser/v3/DELETE.md`
- `/docs/specifications/parser/v3/MERGE.md`
- `/docs/specifications/parser/v3/DDL_CREATE.md`
- `/docs/specifications/parser/v3/DDL_ALTER.md`
- `/docs/specifications/parser/v3/DDL_DROP_TRUNCATE.md`
- `/docs/specifications/parser/v3/TRANSACTION_CONTROL.md`
- `/docs/specifications/parser/v3/ACCESS_CONTROL.md`
- `/docs/specifications/parser/v3/UTILITY_COPY.md`
- `/docs/specifications/parser/v3/SESSION_AND_UTILITY.md`
- `/docs/specifications/parser/v3/PSQL_STATEMENTS.md`
- `/docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
- `/docs/specifications/parser/v3/SBLR_V3_OLD_TO_NEW_MAPPING.md` (see “Type Opcode Additions (V3)” and “Literal Opcode Additions (V3)”)
- `/docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md`
- `/docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md`
- `/docs/specifications/parser/v3/V3_ZERO_AMBIGUITY_BUILD_CHECKLIST.md`
- `/docs/specifications/parser/v3/V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md`
- `/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CONTAINER.md`
- `/docs/specifications/parser/v3/SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`
- `/docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md`
- `/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CANONICALIZATION.md`
- `/docs/specifications/parser/v3/sblr/SBLR_V3_BYTECODE_EXAMPLES.md`
- `/docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS.md`
- `/docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS_FULL.md`
- `/docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md`
- `/docs/specifications/parser/v3/PARSER_AMBIGUITY_RESOLUTION.md`
- `/docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md`
- `/docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`
- `/docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`
- `/docs/specifications/parser/v3/IMPLEMENTATION_SAFETY_SUMMARY.md`
- `/docs/specifications/parser/v3/PSQL_RUNTIME_V3.md`
- `/docs/specifications/parser/v3/types/SBLR_TYPE_MAP.md`
- `/docs/specifications/parser/v3/types/VALUE_SPEC_STORAGE_ENCODINGS.md`
- `/docs/specifications/parser/v3/types/BINARY_LAYOUT_ANNEX.md`
- `/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DOMAIN_MAP.md`
- `/docs/specifications/parser/v3/catalog/UUID_LIFECYCLE_RULES.md`
- `/docs/specifications/parser/v3/tools/SB_BUILD_AND_TEST_CLI_SPEC.md`
- `/docs/specifications/parser/v3/testing/DIALECT_CONFORMANCE_ASSERTIONS.md`
- `/docs/specifications/parser/v3/operations/README.md`
- `/docs/specifications/parser/v3/operations/PROMETHEUS_METRICS_REFERENCE.md`
- `/docs/specifications/parser/v3/operations/MONITORING_SQL_VIEWS.md`
- `/docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md`
- `/docs/specifications/parser/v3/operations/LISTENER_POOL_METRICS.md`
- `/docs/specifications/parser/v3/operations/OID_MAPPING_STRATEGY.md`
- `/docs/specifications/parser/v3/query/QUERY_OPTIMIZER_SPEC.md`
- `/docs/specifications/parser/v3/query/PARALLEL_EXECUTION_ARCHITECTURE.md`
- `/docs/specifications/parser/v3/PERFORMANCE_BENCHMARKS.md`
- `/docs/specifications/parser/v3/findings/NO_GREY_AREAS_GATE.md`
- `/docs/specifications/parser/v3/findings/DIALECT_GAP_EXAMPLES.md`
- `/docs/specifications/parser/v3/BETA_SQL2023_IMPLEMENTATION_SPECIFICATION.md`
- `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`
- `/docs/specifications/parser/v3/indexes/` (all per-index specs)
- `/docs/specifications/parser/v3/transaction/` (MGA, locks, distributed transactions)
- `/docs/specifications/parser/v3/network/` (listener, protocol routing, parser pools)
- `/docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md`

## Scope Notes

- These docs describe parsing behavior and AST construction, not execution.
- All file/line references are to the current implementation and must be revalidated
  after refactors.
- If behavior is mentioned in V2 specs but not present in code, V3 will call it out
  explicitly rather than implying implementation.

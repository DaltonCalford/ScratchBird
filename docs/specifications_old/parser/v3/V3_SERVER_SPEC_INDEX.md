# V3 Server Specification Index
Last Updated: 2026-02-08
Status: Authoritative (V3)

This index lists the V3 specification set required to build a ScratchBird
server from scratch. All referenced documents live under
`/docs/specifications/parser/v3/` and supersede earlier locations.

## Core Architecture
- `/docs/specifications/parser/v3/server/SCRATCHBIRD_ARCHITECTURE_OVERVIEW.md`
- `/docs/specifications/parser/v3/server/SCRATCHBIRD_SERVER_ARCHITECTURE_CONSOLIDATED.md`
- `/docs/specifications/parser/v3/server/SERVER_ARCHITECTURE_AND_CONNECTION_LIFECYCLE.md`
- `/docs/specifications/parser/v3/server/SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md`
- `/docs/specifications/parser/v3/server/ARCHITECTURE_CLARIFICATIONS.md`
- `/docs/specifications/parser/v3/server/MEMORY_MANAGEMENT.md`
- `/docs/specifications/parser/v3/server/SCRATCHBIRD_CONNECTION_RECOVERY_MODEL.md`
- `/docs/specifications/parser/v3/server/SCRATCHBIRD_EMBEDDED_MODE_SPECIFICATION.md`

## Parser, AST, SBLR, Executor
- `/docs/specifications/parser/v3/README.md`
- `/docs/specifications/parser/v3/IMPLEMENTATION_SAFETY_SUMMARY.md`
- `/docs/specifications/parser/v3/AST_TYPE_AND_LITERAL_SPEC.md`
- `/docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
- `/docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md`
- `/docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md`
- `/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CONTAINER.md`
- `/docs/specifications/parser/v3/SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`
- `/docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md`
- `/docs/specifications/parser/v3/SBLR_V3_BYTECODE_CANONICALIZATION.md`
- `/docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md`
- `/docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`
- `/docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`
- `/docs/specifications/parser/v3/PSQL_RUNTIME_V3.md`
- `/docs/specifications/parser/v3/V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md`

## Catalog and Domains
- `/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DDL_SBDB.md`
- `/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_STRUCTURE.md`
- `/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DOMAIN_MAP.md`
- `/docs/specifications/parser/v3/catalog/SCHEMA_PATH_RESOLUTION.md`
- `/docs/specifications/parser/v3/catalog/SCHEMA_PATH_SECURITY_DEFAULTS.md`
- `/docs/specifications/parser/v3/catalog/UUID_LIFECYCLE_RULES.md`

## Types and Persistence
- `/docs/specifications/parser/v3/types/README.md`
- `/docs/specifications/parser/v3/types/BINARY_LAYOUT_ANNEX.md`
- `/docs/specifications/parser/v3/types/SBLR_TYPE_MAP.md`
- `/docs/specifications/parser/v3/types/VALUE_SPEC_STORAGE_ENCODINGS.md`

## Storage Engine
- `/docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md`

## Transactions and MVCC
- `/docs/specifications/parser/v3/transaction/TRANSACTION_MAIN.md`
- `/docs/specifications/parser/v3/transaction/TRANSACTION_LOCK_MANAGER.md`
- `/docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md`

## Indexes
- `/docs/specifications/parser/v3/indexes/INDEX_ARCHITECTURE.md`
- `/docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md`
- `/docs/specifications/parser/v3/indexes/INDEX_GC_PROTOCOL.md`
- `/docs/specifications/parser/v3/indexes/AdvancedIndexes.md`

## Security and Access
- `/docs/specifications/parser/v3/server/SCRATCHBIRD_SECURITY_AND_ACCESS_MODEL.md`
- `/docs/specifications/parser/v3/security/README.md`

## Networking and Protocols
- `/docs/specifications/parser/v3/network/README.md`
- `/docs/specifications/parser/v3/wire_protocols/README.md`
- `/docs/specifications/parser/v3/api/README.md`

## Scheduler and Jobs
- `/docs/specifications/parser/v3/scheduler/README.md`
- `/docs/specifications/parser/v3/scheduler/SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md`

## Operations and Monitoring
- `/docs/specifications/parser/v3/operations/README.md`
- `/docs/specifications/parser/v3/operations/MONITORING_SQL_VIEWS.md`
- `/docs/specifications/parser/v3/operations/PROMETHEUS_METRICS_REFERENCE.md`
- `/docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md`
- `/docs/specifications/parser/v3/operations/LISTENER_POOL_METRICS.md`
- `/docs/specifications/parser/v3/operations/OID_MAPPING_STRATEGY.md`

## SBLR Bytecode Examples
- `/docs/specifications/parser/v3/sblr/SBLR_V3_BYTECODE_EXAMPLES.md`
- `/docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS.md`
- `/docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS_FULL.md`

## Parser Emission Rules
- `/docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md`
- `/docs/specifications/parser/v3/PARSER_AMBIGUITY_RESOLUTION.md`

## Performance and Optimization
- `/docs/specifications/parser/v3/query/QUERY_OPTIMIZER_SPEC.md`
- `/docs/specifications/parser/v3/query/PARALLEL_EXECUTION_ARCHITECTURE.md`
- `/docs/specifications/parser/v3/PERFORMANCE_BENCHMARKS.md`

## Type Storage Encodings
- `/docs/specifications/parser/v3/types/VALUE_SPEC_STORAGE_ENCODINGS.md`

## Audit and Findings
- `/docs/specifications/parser/v3/findings/NO_GREY_AREAS_GATE.md`
- `/docs/specifications/parser/v3/findings/DIALECT_GAP_EXAMPLES.md`

## Backup/Restore and Deployment
- `/docs/specifications/parser/v3/server/BACKUP_AND_RESTORE.md`
- `/docs/specifications/parser/v3/deployment/README.md`
- `/docs/specifications/parser/v3/server/INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md`

## Tools and Testing
- `/docs/specifications/parser/v3/tools/README.md`
- `/docs/specifications/parser/v3/tools/SB_BUILD_AND_TEST_CLI_SPEC.md`
- `/docs/specifications/parser/v3/testing/README.md`
- `/docs/specifications/parser/v3/testing/DIALECT_CONFORMANCE_ASSERTIONS.md`

## Registry and Metadata
- `/docs/specifications/parser/v3/server/DATABASE_REGISTRY_SPECIFICATION_CORRECTED.md`

---

## Update Procedure (Authoritative)

1. This index MUST list only authoritative V3 specs (see `AUTHORITATIVE_SPEC_INVENTORY.md`).
2. When a file is added/removed from the authoritative set, update this index.
3. All links must be workspace‑relative and point to existing files.

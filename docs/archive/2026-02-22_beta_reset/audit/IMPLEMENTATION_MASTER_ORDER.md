# V3 Implementation Master Order

This file orders implementation across authoritative V3 specs. If tasks span multiple specs, the cross-spec entry appears under **Cross-Spec Work Orders**.

## Cross-Spec Work Orders
- Parser → SBLR → Executor bindings for each SQL statement family
- Storage + Index integration: page types, WAL/GC/visibility consistency
- Dialect conformance baselines (PostgreSQL 16+, MySQL 8.x, Firebird 5.x)

## Ordered Implementation

### CORE
- `docs/specifications/parser/v3/ACCESS_CONTROL.md` → `docs/audit/PLAN__docs__specifications__parser__v3__ACCESS_CONTROL.md`
- `docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` → `docs/audit/PLAN__docs__specifications__parser__v3__AUTHORITATIVE_SPEC_INVENTORY.md`
- `docs/specifications/parser/v3/BETA_SQL2023_IMPLEMENTATION_SPECIFICATION.md` → `docs/audit/PLAN__docs__specifications__parser__v3__BETA_SQL2023_IMPLEMENTATION_SPECIFICATION.md`
- `docs/specifications/parser/v3/IMPLEMENTATION_SAFETY_SUMMARY.md` → `docs/audit/PLAN__docs__specifications__parser__v3__IMPLEMENTATION_SAFETY_SUMMARY.md`
- `docs/specifications/parser/v3/JOINS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__JOINS.md`
- `docs/specifications/parser/v3/PERFORMANCE_BENCHMARKS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__PERFORMANCE_BENCHMARKS.md`
- `docs/specifications/parser/v3/SESSION_AND_UTILITY.md` → `docs/audit/PLAN__docs__specifications__parser__v3__SESSION_AND_UTILITY.md`
- `docs/specifications/parser/v3/TRANSACTION_CONTROL.md` → `docs/audit/PLAN__docs__specifications__parser__v3__TRANSACTION_CONTROL.md`
- `docs/specifications/parser/v3/UTILITY_COPY.md` → `docs/audit/PLAN__docs__specifications__parser__v3__UTILITY_COPY.md`
- `docs/specifications/parser/v3/V3_SERVER_SPEC_INDEX.md` → `docs/audit/PLAN__docs__specifications__parser__v3__V3_SERVER_SPEC_INDEX.md`
- `docs/specifications/parser/v3/V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md`
- `docs/specifications/parser/v3/V3_ZERO_AMBIGUITY_BUILD_CHECKLIST.md` → `docs/audit/PLAN__docs__specifications__parser__v3__V3_ZERO_AMBIGUITY_BUILD_CHECKLIST.md`
- `docs/specifications/parser/v3/WINDOWING.md` → `docs/audit/PLAN__docs__specifications__parser__v3__WINDOWING.md`
- `docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DOMAIN_MAP.md` → `docs/audit/PLAN__docs__specifications__parser__v3__catalog__SYSTEM_CATALOG_DOMAIN_MAP.md`
- `docs/specifications/parser/v3/catalog/UUID_LIFECYCLE_RULES.md` → `docs/audit/PLAN__docs__specifications__parser__v3__catalog__UUID_LIFECYCLE_RULES.md`
- `docs/specifications/parser/v3/findings/DIALECT_GAP_EXAMPLES.md` → `docs/audit/PLAN__docs__specifications__parser__v3__findings__DIALECT_GAP_EXAMPLES.md`
- `docs/specifications/parser/v3/findings/NO_GREY_AREAS_GATE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__findings__NO_GREY_AREAS_GATE.md`
- `docs/specifications/parser/v3/network/CONTROL_PLANE_PROTOCOL_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__network__CONTROL_PLANE_PROTOCOL_SPEC.md`
- `docs/specifications/parser/v3/network/DIALECT_AUTH_MAPPING_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__network__DIALECT_AUTH_MAPPING_SPEC.md`
- `docs/specifications/parser/v3/network/NETWORK_LAYER_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__network__NETWORK_LAYER_SPEC.md`
- `docs/specifications/parser/v3/network/README.md` → `docs/audit/PLAN__docs__specifications__parser__v3__network__README.md`
- `docs/specifications/parser/v3/network/WIRE_PROTOCOL_SPECIFICATIONS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__network__WIRE_PROTOCOL_SPECIFICATIONS.md`
- `docs/specifications/parser/v3/network/Y_VALVE_DESIGN_PRINCIPLES.md` → `docs/audit/PLAN__docs__specifications__parser__v3__network__Y_VALVE_DESIGN_PRINCIPLES.md`
- `docs/specifications/parser/v3/operations/LISTENER_POOL_METRICS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__operations__LISTENER_POOL_METRICS.md`
- `docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__operations__MONITORING_DIALECT_MAPPINGS.md`
- `docs/specifications/parser/v3/operations/MONITORING_SQL_VIEWS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__operations__MONITORING_SQL_VIEWS.md`
- `docs/specifications/parser/v3/operations/OID_MAPPING_STRATEGY.md` → `docs/audit/PLAN__docs__specifications__parser__v3__operations__OID_MAPPING_STRATEGY.md`
- `docs/specifications/parser/v3/operations/PROMETHEUS_METRICS_REFERENCE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__operations__PROMETHEUS_METRICS_REFERENCE.md`
- `docs/specifications/parser/v3/operations/README.md` → `docs/audit/PLAN__docs__specifications__parser__v3__operations__README.md`
- `docs/specifications/parser/v3/parser/01_SQL_DIALECT_OVERVIEW.md` → `docs/audit/PLAN__docs__specifications__parser__v3__parser__01_SQL_DIALECT_OVERVIEW.md`
- `docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md` → `docs/audit/PLAN__docs__specifications__parser__v3__parser__SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_CORE_LANGUAGE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__parser__SCRATCHBIRD_SQL_CORE_LANGUAGE.md`
- `docs/specifications/parser/v3/parser/SCRATCHBIRD_UNIFIED_NOSQL_EXTENSIONS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__parser__SCRATCHBIRD_UNIFIED_NOSQL_EXTENSIONS.md`
- `docs/specifications/parser/v3/parser/ScratchBird Master Grammar Specification v2.0.md` → `docs/audit/PLAN__docs__specifications__parser__v3__parser__ScratchBird Master Grammar Specification v2.0.md`
- `docs/specifications/parser/v3/parser/ScratchBird SQL Language Specification - Master Document.md` → `docs/audit/PLAN__docs__specifications__parser__v3__parser__ScratchBird SQL Language Specification - Master Document.md`
- `docs/specifications/parser/v3/query/PARALLEL_EXECUTION_ARCHITECTURE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__query__PARALLEL_EXECUTION_ARCHITECTURE.md`
- `docs/specifications/parser/v3/query/QUERY_OPTIMIZER_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__query__QUERY_OPTIMIZER_SPEC.md`
- `docs/specifications/parser/v3/testing/DIALECT_CONFORMANCE_ASSERTIONS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__testing__DIALECT_CONFORMANCE_ASSERTIONS.md`
- `docs/specifications/parser/v3/tools/SB_BUILD_AND_TEST_CLI_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__tools__SB_BUILD_AND_TEST_CLI_SPEC.md`
- `docs/specifications/parser/v3/types/BINARY_LAYOUT_ANNEX.md` → `docs/audit/PLAN__docs__specifications__parser__v3__types__BINARY_LAYOUT_ANNEX.md`
- `docs/specifications/parser/v3/types/VALUE_SPEC_STORAGE_ENCODINGS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__types__VALUE_SPEC_STORAGE_ENCODINGS.md`

### STORAGE
- `docs/specifications/parser/v3/storage/PAGE_TYPES_AND_LAYOUTS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__storage__PAGE_TYPES_AND_LAYOUTS.md`

### TRANSACTION
- `docs/specifications/parser/v3/transaction/07_TRANSACTION_AND_SESSION_CONTROL.md` → `docs/audit/PLAN__docs__specifications__parser__v3__transaction__07_TRANSACTION_AND_SESSION_CONTROL.md`
- `docs/specifications/parser/v3/transaction/FIREBIRD_CONSTANTS_REFERENCE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__transaction__FIREBIRD_CONSTANTS_REFERENCE.md`
- `docs/specifications/parser/v3/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md` → `docs/audit/PLAN__docs__specifications__parser__v3__transaction__FIREBIRD_GC_SWEEP_GLOSSARY.md`
- `docs/specifications/parser/v3/transaction/README.md` → `docs/audit/PLAN__docs__specifications__parser__v3__transaction__README.md`
- `docs/specifications/parser/v3/transaction/TRANSACTION_DISTRIBUTED.md` → `docs/audit/PLAN__docs__specifications__parser__v3__transaction__TRANSACTION_DISTRIBUTED.md`
- `docs/specifications/parser/v3/transaction/TRANSACTION_LOCK_MANAGER.md` → `docs/audit/PLAN__docs__specifications__parser__v3__transaction__TRANSACTION_LOCK_MANAGER.md`
- `docs/specifications/parser/v3/transaction/TRANSACTION_MAIN.md` → `docs/audit/PLAN__docs__specifications__parser__v3__transaction__TRANSACTION_MAIN.md`
- `docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__transaction__TRANSACTION_MGA_CORE.md`

### SBLR
- `docs/specifications/parser/v3/EXECUTOR_V3_SBLR.md` → `docs/audit/PLAN__docs__specifications__parser__v3__EXECUTOR_V3_SBLR.md`
- `docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md` → `docs/audit/PLAN__docs__specifications__parser__v3__PARSER_TO_SBLR_EMISSION_RULES.md`
- `docs/specifications/parser/v3/SBLR_V3_BYTECODE_CANONICALIZATION.md` → `docs/audit/PLAN__docs__specifications__parser__v3__SBLR_V3_BYTECODE_CANONICALIZATION.md`
- `docs/specifications/parser/v3/SBLR_V3_BYTECODE_CONTAINER.md` → `docs/audit/PLAN__docs__specifications__parser__v3__SBLR_V3_BYTECODE_CONTAINER.md`
- `docs/specifications/parser/v3/SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`
- `docs/specifications/parser/v3/SBLR_V3_OLD_TO_NEW_MAPPING.md` → `docs/audit/PLAN__docs__specifications__parser__v3__SBLR_V3_OLD_TO_NEW_MAPPING.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__SBLR_V3_OPCODE_PAYLOADS.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__SBLR_V3_OPCODE_SEMANTICS.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__SBLR_V3_OPCODE_SPEC.md`
- `docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md` → `docs/audit/PLAN__docs__specifications__parser__v3__SBLR_V3_VALIDATION_RULES.md`
- `docs/specifications/parser/v3/sblr/SBLR_V3_BYTECODE_EXAMPLES.md` → `docs/audit/PLAN__docs__specifications__parser__v3__sblr__SBLR_V3_BYTECODE_EXAMPLES.md`
- `docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__sblr__SBLR_V3_TEST_VECTORS.md`
- `docs/specifications/parser/v3/sblr/SBLR_V3_TEST_VECTORS_FULL.md` → `docs/audit/PLAN__docs__specifications__parser__v3__sblr__SBLR_V3_TEST_VECTORS_FULL.md`
- `docs/specifications/parser/v3/types/SBLR_TYPE_MAP.md` → `docs/audit/PLAN__docs__specifications__parser__v3__types__SBLR_TYPE_MAP.md`

### PARSER
- `docs/specifications/parser/v3/PARSER_AMBIGUITY_RESOLUTION.md` → `docs/audit/PLAN__docs__specifications__parser__v3__PARSER_AMBIGUITY_RESOLUTION.md`
- `docs/specifications/parser/v3/PSQL_RUNTIME_V3.md` → `docs/audit/PLAN__docs__specifications__parser__v3__PSQL_RUNTIME_V3.md`
- `docs/specifications/parser/v3/PSQL_STATEMENTS.md` → `docs/audit/PLAN__docs__specifications__parser__v3__PSQL_STATEMENTS.md`
- `docs/specifications/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md` → `docs/audit/PLAN__docs__specifications__parser__v3__network__ENGINE_PARSER_IPC_CONTRACT.md`
- `docs/specifications/parser/v3/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__network__NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`
- `docs/specifications/parser/v3/network/PARSER_AGENT_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__network__PARSER_AGENT_SPEC.md`
- `docs/specifications/parser/v3/parser/05_PSQL_PROCEDURAL_LANGUAGE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__parser__05_PSQL_PROCEDURAL_LANGUAGE.md`
- `docs/specifications/parser/v3/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md` → `docs/audit/PLAN__docs__specifications__parser__v3__parser__EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/v3/parser/MYSQL_PARSER_SPECIFICATION.md` → `docs/audit/PLAN__docs__specifications__parser__v3__parser__MYSQL_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/v3/parser/POSTGRESQL_PARSER_SPECIFICATION.md` → `docs/audit/PLAN__docs__specifications__parser__v3__parser__POSTGRESQL_PARSER_SPECIFICATION.md`

### EXECUTOR
- `docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md` → `docs/audit/PLAN__docs__specifications__parser__v3__EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`
- `docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__EXECUTOR_V3_SQL_ENGINE.md`

### SQL
- `docs/specifications/parser/v3/DDL_ALTER.md` → `docs/audit/PLAN__docs__specifications__parser__v3__DDL_ALTER.md`
- `docs/specifications/parser/v3/DDL_CREATE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__DDL_CREATE.md`
- `docs/specifications/parser/v3/DDL_DROP_TRUNCATE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__DDL_DROP_TRUNCATE.md`
- `docs/specifications/parser/v3/DELETE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__DELETE.md`
- `docs/specifications/parser/v3/INSERT.md` → `docs/audit/PLAN__docs__specifications__parser__v3__INSERT.md`
- `docs/specifications/parser/v3/MERGE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__MERGE.md`
- `docs/specifications/parser/v3/SELECT_AND_QUERY.md` → `docs/audit/PLAN__docs__specifications__parser__v3__SELECT_AND_QUERY.md`
- `docs/specifications/parser/v3/UPDATE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__UPDATE.md`

### INDEXES
- `docs/specifications/parser/v3/indexes/AdaptiveRadixTreeIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__AdaptiveRadixTreeIndex.md`
- `docs/specifications/parser/v3/indexes/AdvancedIndexes.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__AdvancedIndexes.md`
- `docs/specifications/parser/v3/indexes/BITMAP_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__BITMAP_SPEC.md`
- `docs/specifications/parser/v3/indexes/BRIN_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__BRIN_SPEC.md`
- `docs/specifications/parser/v3/indexes/BTREE_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__BTREE_SPEC.md`
- `docs/specifications/parser/v3/indexes/BloomFilterIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__BloomFilterIndex.md`
- `docs/specifications/parser/v3/indexes/COLUMNSTORE_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__COLUMNSTORE_SPEC.md`
- `docs/specifications/parser/v3/indexes/CountMinSketchIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__CountMinSketchIndex.md`
- `docs/specifications/parser/v3/indexes/FSTIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__FSTIndex.md`
- `docs/specifications/parser/v3/indexes/GIN_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__GIN_SPEC.md`
- `docs/specifications/parser/v3/indexes/GIST_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__GIST_SPEC.md`
- `docs/specifications/parser/v3/indexes/GeohashS2Index.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__GeohashS2Index.md`
- `docs/specifications/parser/v3/indexes/HASH_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__HASH_SPEC.md`
- `docs/specifications/parser/v3/indexes/HNSW_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__HNSW_SPEC.md`
- `docs/specifications/parser/v3/indexes/HyperLogLogIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__HyperLogLogIndex.md`
- `docs/specifications/parser/v3/indexes/INDEX_ARCHITECTURE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__INDEX_ARCHITECTURE.md`
- `docs/specifications/parser/v3/indexes/INDEX_COMPLETION_CHECKLIST.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__INDEX_COMPLETION_CHECKLIST.md`
- `docs/specifications/parser/v3/indexes/INDEX_GC_PROTOCOL.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__INDEX_GC_PROTOCOL.md`
- `docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_GUIDE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__INDEX_IMPLEMENTATION_GUIDE.md`
- `docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_REFERENCE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__INDEX_IMPLEMENTATION_REFERENCE.md`
- `docs/specifications/parser/v3/indexes/INDEX_IMPLEMENTATION_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__INDEX_IMPLEMENTATION_SPEC.md`
- `docs/specifications/parser/v3/indexes/IVFIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__IVFIndex.md`
- `docs/specifications/parser/v3/indexes/InvertedIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__InvertedIndex.md`
- `docs/specifications/parser/v3/indexes/JSONPathIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__JSONPathIndex.md`
- `docs/specifications/parser/v3/indexes/LOW_LEVEL_SPECIFICATION_GIN_INDEX.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__LOW_LEVEL_SPECIFICATION_GIN_INDEX.md`
- `docs/specifications/parser/v3/indexes/LSMTimeSeriesIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__LSMTimeSeriesIndex.md`
- `docs/specifications/parser/v3/indexes/LSM_TREE_ARCHITECTURE.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__LSM_TREE_ARCHITECTURE.md`
- `docs/specifications/parser/v3/indexes/LSM_TREE_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__LSM_TREE_SPEC.md`
- `docs/specifications/parser/v3/indexes/LearnedIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__LearnedIndex.md`
- `docs/specifications/parser/v3/indexes/QuadtreeOctreeIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__QuadtreeOctreeIndex.md`
- `docs/specifications/parser/v3/indexes/README.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__README.md`
- `docs/specifications/parser/v3/indexes/RTREE_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__RTREE_SPEC.md`
- `docs/specifications/parser/v3/indexes/SPGIST_SPEC.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__SPGIST_SPEC.md`
- `docs/specifications/parser/v3/indexes/SuffixIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__SuffixIndex.md`
- `docs/specifications/parser/v3/indexes/ZOrderIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__ZOrderIndex.md`
- `docs/specifications/parser/v3/indexes/ZoneMapsIndex.md` → `docs/audit/PLAN__docs__specifications__parser__v3__indexes__ZoneMapsIndex.md`

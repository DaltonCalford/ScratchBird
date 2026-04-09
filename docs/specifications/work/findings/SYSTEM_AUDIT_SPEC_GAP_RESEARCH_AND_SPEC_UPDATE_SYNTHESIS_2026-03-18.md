# System Audit Spec Gap Research and Spec Update Synthesis

Date: 2026-03-18

## Objective and Bounded Scope
- Convert the 2026-03-18 system-audit gap report into authoritative numbered
  specs and the minimum cross-section revisions needed to make those specs
  implementation-ready.
- Keep normative spec text ScratchBird-native and free of donor URLs or clone
  path references.

## ScratchBird Live-Code Anchors Reviewed
- `ScratchBird/include/scratchbird/sblr/query_limits.h`
- `ScratchBird/include/scratchbird/core/workload_governance.h`
- `ScratchBird/include/scratchbird/executor/parallel_executor.h`
- `ScratchBird/src/optimizer/query_planner.cpp`
- `ScratchBird/src/sblr/query_result_cache.cpp`
- `ScratchBird/src/sblr/jit/jit_invalidation.cpp`
- `ScratchBird/src/core/catalog_manager.cpp`
- `ScratchBird/src/core/lock_manager.cpp`
- `ScratchBird/src/core/clock_control.cpp`
- `ScratchBird/src/network/thread_pool.cpp`
- `ScratchBird/src/core/backup_manager.cpp`

## Authoritative ScratchBird Specs Reviewed
- section 00 README and doctrine files
- section 01 config catalog baseline
- section 09 lock manager contract
- section 12 temp-table contract
- section 23 optimizer and execution contracts
- section 24 catalog inventory and schema-path contracts
- section 25 layered runtime stack
- section 31 test and gate baselines

## Local Donor Set Used
- PostgreSQL: memory-context discipline, COPY surface, cache invalidation, lock
  and explicit-locking patterns
- MySQL: metadata locking and online-DDL policy framing
- Firebird: MGA mindset and restart-truth discipline

## Web Sources Used
- OWASP Threat Modeling Cheat Sheet:
  https://cheatsheetseries.owasp.org/cheatsheets/Threat_Modeling_Cheat_Sheet.html
- MySQL 8.4 Metadata Locking:
  https://dev.mysql.com/doc/refman/8.4/en/metadata-locking.html
- PostgreSQL 18 COPY:
  https://www.postgresql.org/docs/current/sql-copy.html
- PostgreSQL 18 Resource Consumption:
  https://www.postgresql.org/docs/current/runtime-config-resource.html
- PostgreSQL 18 Explicit Locking:
  https://www.postgresql.org/docs/current/explicit-locking.html

## Patterns to Adopt
- explicit memory-context ownership and bounded operator budgets
- metadata-lock ordering and explicit DDL concurrency classes
- dependency-driven invalidation for plan, result, and artifact caches
- COPY contract with explicit reject handling and privileged server-file modes
- threat modeling as a living control matrix, not a one-time checklist
- monotonic-clock discipline for deadlines and timeout logic

## Patterns to Adapt
- PostgreSQL-style `work_mem` and hash-memory thinking adapted into ScratchBird
  query, statement, and operator budgets rather than copied directly
- MySQL metadata-lock rigor adapted to ScratchBird UUID and catalog-publication
  model instead of name-ordered engine semantics
- PostgreSQL invalidation ideas adapted to ScratchBird catalog epochs and
  dependency signatures without adopting WAL-coupled assumptions

## Patterns to Reject
- WAL or redo as Alpha recovery truth
- unbounded global heap use as execution-memory policy
- hidden optimizer rewrites with no trace inventory
- silent certification claims for platforms or features lacking explicit scope

## MGA and Parser Boundary Compatibility Note
- All adopted patterns were translated into ScratchBird-native language that
  preserves MGA inventory truth, UUID identity, and the parser-to-SBLR
  boundary.
- No normative spec added in this round treats parser behavior as core-engine
  correctness truth.

## Target Spec Files Created
- section 00:
  - `SUBSYSTEM_CORRECTNESS_INVARIANT_CATALOG.md`
  - `SUBSYSTEM_OWNERSHIP_AND_DEPENDENCY_BOUNDARIES.md`
- section 02:
  - `TABLE_STORAGE_MODES_AND_ROW_MOVEMENT_MODEL.md`
- section 05:
  - `ON_DISK_FORMAT_INVENTORY_AND_VERSION_MANIFEST.md`
- section 08:
  - `BACKUP_RESTORE_SUPPORT_MATRIX_AND_VALIDATION.md`
- section 09:
  - `ISOLATION_LEVEL_AND_PHANTOM_PROTECTION_MATRIX.md`
- section 12:
  - `TEMP_WORKFILE_AND_OPERATOR_SPILL_CONTRACT.md`
- section 19:
  - `THREAT_MODEL_AND_SECURITY_CONTROL_MATRIX.md`
- section 23:
  - `EXECUTION_MEMORY_CONTEXTS_AND_BUDGETS.md`
  - `LOGICAL_REWRITE_INVENTORY_AND_TRACE.md`
  - `BULK_LOAD_AND_COPY_EXECUTION_CONTRACT.md`
- section 24:
  - `CATALOG_INVALIDATION_DEPENDENCY_GRAPH_AND_SELF_CHECK.md`
  - `DDL_BEHAVIOR_MATRIX_AND_METADATA_LOCK_POLICY.md`
- section 25:
  - `ENGINE_RESOURCE_GOVERNANCE_AND_BUDGETS.md`
  - `ENGINE_TIME_SOURCE_AND_ORDERING_DISCIPLINE.md`
  - `ENGINE_THREAD_WORKER_AND_TASK_MODEL.md`
- section 31:
  - `PLATFORM_SUPPORT_MATRIX_AND_CERTIFICATION_SCOPE.md`
  - `TEST_OWNERSHIP_EXCLUSION_AND_FLAKE_POLICY.md`
  - `FEATURE_LIFECYCLE_AND_CROSS_VERSION_COMPATIBILITY_MATRIX.md`

## Existing Spec Files Revised
- `01_Configuration_Subsystem/CONFIG_CATALOG_AND_BOOTSTRAP.md`
- `09_Lock_Manager_Core/LOCK_MANAGER_NORMATIVE_IMPLEMENTATION.md`
- `12_Temporary_Tables/TEMP_TABLES_NORMATIVE_IMPLEMENTATION.md`
- `23_SBLR_VM_Compiler_and_Executor/OPTIMIZER_PASS_PIPELINE.md`
- `24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md`
- `31_Conformance_Performance_and_Reliability_Gates/TEST_CONTRACT.md`

## Unresolved Decisions or Follow-Up Questions
- serializable-implementation mechanism choice remains open between stronger
  predicate locking and commit-time validation detail
- exact stable versus preview platform list may widen after future CI evidence
- append and columnar storage modes remain lifecycle-governed and not yet
  certification-ready

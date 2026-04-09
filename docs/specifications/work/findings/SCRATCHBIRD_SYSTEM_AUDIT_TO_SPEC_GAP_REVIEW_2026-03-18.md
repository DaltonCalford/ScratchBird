# ScratchBird System Audit to Specification Gap Review

Date: 2026-03-18

## Scope
- Reviewed the 38-file system/code audit in `local_work/findings/Full_Detailed_System_Audit/ScratchBird/2026-03-18/`.
- Compared audit recommendations against the authoritative specification tree in `docs/specifications/`.
- Counted an item as a gap only when either:
  - no authoritative numbered-section contract exists, or
  - related material exists but the audit recommendation still requires a single canonical contract that does not currently exist.

## Summary
- Many audit recommendations are already covered by the current canonical spec tree and should not be reopened as missing work.
- The main remaining gaps are cross-cutting contracts: memory ownership, rewrite inventory, metadata invalidation, DDL concurrency, threat model, workload budgets, worker/thread model, temp/spill management, time semantics, platform scope, test governance, bulk-load behavior, and lifecycle compatibility.
- A smaller set of areas has partial coverage but still lacks one authoritative consolidation point: subsystem ownership boundaries, full on-disk format inventory, and the storage-mode maturity story.

## Covered Enough To Exclude From Gap List
- Transaction lifecycle, savepoints, restart, and limbo handling are already specified in section 08, especially `TRANSACTION_LIFECYCLE.md`, `SAVEPOINT_AND_SUBTRANSACTION_SEMANTICS.md`, and `MGA_TRANSACTION_PUBLICATION_AND_RESTART_SEMANTICS.md`.
- Lock manager fairness, wait queues, deadlock handling, and escalation rules are already specified in `09_Lock_Manager_Core/LOCK_MANAGER_NORMATIVE_IMPLEMENTATION.md`.
- Checkpoint/recovery/failure classification are already specified in `08_Transaction_Core/CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md` and `08_Transaction_Core/FAILURE_MODEL_AND_RECOVERY_CLASSIFICATION.md`.
- Recovery/checkpoint and buffer telemetry are already materially covered in section 20, especially `RECOVERY_AND_CHECKPOINT_OBSERVABILITY.md`, `MGA_OBSERVABILITY_AND_OPERATOR_DIAGNOSTICS.md`, and `BUFFER_CACHE_OBSERVABILITY.md`.
- Connection/session lifecycle is already specified in `29_Listener_and_Server_Orchestration/CONNECTION_AND_SESSION_LIFECYCLE.md`.
- UDR and extension baseline contracts already exist in section 17, with additional signed-plugin auth ABI coverage in section 19.
- Driver and client API baselines already exist in section 30 and should not be treated as absent.
- Replication and HA contracts already exist across sections 25, 29, and 31, even if implementation maturity is a separate issue.
- Configuration catalog, mutability, and bootstrap precedence are already specified in section 01.
- Benchmark methodology is already specified in `31_Conformance_Performance_and_Reliability_Gates/PERFORMANCE_SLO_AND_BENCHMARK_METHOD.md`.

## Missing Or Materially Incomplete Authoritative Contracts

| Priority | Gap | Audit Sources | Why It Still Counts As Missing | Recommended Canonical Target |
| --- | --- | --- | --- | --- |
| P0 | Unified memory-context, allocation-lifetime, and OOM semantics | `02_memory_management.md`, `15_execution_engine.md`, `27_workload_management_and_resource_governance.md` | The current specs define buffer-pool memory domains and mention planner memory budgets, but there is no single contract for per-query, per-session, per-transaction, operator-local, and background-task memory ownership, nor one place that defines throttle, spill, cancel, or reject behavior under OOM pressure. | New `23_SBLR_VM_Compiler_and_Executor/EXECUTION_MEMORY_CONTEXTS_AND_BUDGETS.md`; update section 03 and section 25 for interaction rules. |
| P0 | Metadata invalidation, dependency propagation, and catalog self-check contract | `16_statistics_and_metadata_subsystem.md`, `17_schema_management_and_ddl.md` | Section 24 defines dependency tables, but no authoritative document defines invalidation producers/consumers, stale-metadata rules, repair/self-check workflow, or planner/executor cache invalidation boundaries. | New `24_Catalog_Model_and_Virtual_Overlays/CATALOG_INVALIDATION_DEPENDENCY_GRAPH_AND_SELF_CHECK.md`. |
| P0 | DDL behavior matrix and metadata-lock / concurrent DDL-DML contract | `17_schema_management_and_ddl.md` | Transactional DDL lineage exists, but there is no authoritative matrix for transactional vs non-transactional behavior, online-safe vs offline-only operations, failed-DDL rollback, metadata lock scope, or concurrent DDL/DML guarantees. | New `24_Catalog_Model_and_Virtual_Overlays/DDL_BEHAVIOR_MATRIX_AND_METADATA_LOCK_POLICY.md`; update section 09 lock rules for metadata/schema resources. |
| P0 | Formal subsystem correctness invariant catalog | `29_integrity_and_correctness_mechanisms.md` | Section 00 has top-level doctrine, but there is no consolidated invariant catalog for MVCC visibility, heap/index correspondence, page ownership, recovery normalization, and corruption/quarantine boundaries. | New `00_Governance_and_Invarients/SUBSYSTEM_CORRECTNESS_INVARIANT_CATALOG.md`; cross-link from sections 03, 05, 08, 09, 10, 18, and 31. |
| P0 | Temp/workfile/operator-spill subsystem contract | `32_temporary_data_and_spill_subsystem.md`, `02_memory_management.md` | Temp tables are specified and memory domains mention spill pages, but there is no single contract for workfile identity, quota enforcement, crash cleanup, operator spill artifacts, spill diagnostics, or deterministic spill/refusal behavior. | New `12_Temporary_Tables/TEMP_WORKFILE_AND_OPERATOR_SPILL_CONTRACT.md`; update section 23 execution docs and section 20 telemetry. |
| P0 | Engine-wide time-source and ordering discipline | `33_time_clocks_and_ordering_assumptions.md` | Cluster clock policy exists, but there is no engine-wide contract for monotonic vs wall-clock use, fake-clock injection, timeout safety, or ordering assumptions for background maintenance and local execution. | New `25_Runtime_Modes/ENGINE_TIME_SOURCE_AND_ORDERING_DISCIPLINE.md`; cross-link with `CLUSTER_CLOCK_DISCIPLINE_AND_SKEW_POLICY.md`. |
| P0 | First-class resource governance for query/session/background work | `27_workload_management_and_resource_governance.md`, `02_memory_management.md`, `11_checkpointing_and_background_maintenance.md` | Cluster admission and SLO docs exist, but there is no authoritative engine contract for per-session, per-query, temp-space, and background-maintenance budgets, nor for surfaced refusal/throttle semantics. | New `25_Runtime_Modes/ENGINE_RESOURCE_GOVERNANCE_AND_BUDGETS.md`; update section 01 config catalog and section 20 diagnostics. |
| P0 | Bulk-load and COPY execution contract | `31_bulk_data_paths.md` | Parser and SBLR surfaces mention COPY/bulk, but there is no canonical contract for bulk-path durability, secondary-index maintenance mode, interrupted-load recovery, post-load validation, or bulk-specific throttling/governance. | New `23_SBLR_VM_Compiler_and_Executor/BULK_LOAD_AND_COPY_EXECUTION_CONTRACT.md`; link from sections 21, 28, and 31. |
| P1 | Backup/restore support matrix and restore-validation contract | `23_backup_restore_and_import_export.md` | SQL and wire surfaces mention backup/restore and section 31 has drill gates, but there is no authoritative numbered-section matrix that defines supported physical/logical/online/offline/incremental modes, restore validation obligations, and unsupported cases. | New `08_Transaction_Core/BACKUP_RESTORE_SUPPORT_MATRIX_AND_VALIDATION.md`; cross-link from sections 21, 26, and 31. |
| P1 | Security threat model and control matrix | `24_security_model.md` | Section 19 covers specific auth/key/plugin surfaces, but no canonical threat model or control-coverage matrix defines attacker classes, trust boundaries, secure defaults, and implemented vs partial protections across front doors and plugins. | New `19_Security_Model/THREAT_MODEL_AND_SECURITY_CONTROL_MATRIX.md`. |
| P1 | Engine thread/worker/task model and scalability contract | `28_parallelism_and_scalability_model.md` | Listener/process docs exist, but there is no engine-wide worker/thread model naming background workers, query workers, scheduling boundaries, hot-contention expectations, or certified scalability claims. | New `25_Runtime_Modes/ENGINE_THREAD_WORKER_AND_TASK_MODEL.md`; cross-link from sections 23 and 29. |
| P1 | Platform support matrix and portability boundary | `34_portability_and_platform_interface.md` | There is no authoritative platform matrix by OS, filesystem, architecture, and subsystem capability. Current docs mention supported platforms only indirectly. | New `31_Conformance_Performance_and_Reliability_Gates/PLATFORM_SUPPORT_MATRIX_AND_CERTIFICATION_SCOPE.md`. |
| P1 | Test ownership, exclusion, and flake-governance policy | `35_build_testing_and_verification_infrastructure.md` | Test contracts exist, but there is no canonical policy that defines excluded-test handling, flake tracking, subsystem ownership, or promotion rules for aggregate-harness truth. | New `31_Conformance_Performance_and_Reliability_Gates/TEST_OWNERSHIP_EXCLUSION_AND_FLAKE_POLICY.md`. |
| P1 | Cross-version compatibility and feature lifecycle matrix | `37_upgrade_compatibility_and_lifecycle_management.md`, `18_data_types_and_encoding.md`, `06_index_subsystem.md` | Specific compatibility matrices exist for parser and storage-version fragments, but there is no single lifecycle matrix for syntax, semantics, protocol, storage, and operational compatibility, nor one place that marks experimental vs stable vs partial feature states. | New `31_Conformance_Performance_and_Reliability_Gates/FEATURE_LIFECYCLE_AND_CROSS_VERSION_COMPATIBILITY_MATRIX.md`. |
| P1 | Rewrite inventory and rewrite-trace/debug contract | `13_query_rewrite_and_logical_transformation.md` | `OPTIMIZER_PASS_PIPELINE.md` mentions equivalence-preserving rewrites, but there is no authoritative pass inventory, pass ordering contract, or trace/debug output contract for logical rewrites. | New `23_SBLR_VM_Compiler_and_Executor/LOGICAL_REWRITE_INVENTORY_AND_TRACE.md`. |
| P1 | Isolation-level phantom-protection and predicate/range-lock guarantee matrix | `08_concurrency_control.md` | Section 09 covers lock algorithms, fairness, and DDL/DML intent mapping, but no authoritative document states predicate/range-lock behavior or phantom guarantees per isolation level. | New `09_Lock_Manager_Core/ISOLATION_LEVEL_AND_PHANTOM_PROTECTION_MATRIX.md`. |

## Fragmented Coverage That Still Needs A Single Canonical Contract

| Priority | Gap | Audit Sources | Existing Coverage | Missing Consolidation Target |
| --- | --- | --- | --- | --- |
| P1 | Subsystem ownership and one-way dependency map | `01_system_architecture_and_component_boundaries.md`, `12_query_parser_and_sql_front_end_language_layer.md` | High-level doctrine exists in section 00; parser/engine and listener dependency material exists in sections 23, 28, and 29. | New `00_Governance_and_Invarients/SUBSYSTEM_OWNERSHIP_AND_DEPENDENCY_BOUNDARIES.md`. |
| P1 | Full on-disk format inventory and version manifest | `03_on_disk_storage_format.md` | Sections 05 and 06 define many page and header layouts and `db_version` compatibility rules. | New `05_Page_Taxonomy_and_Binary_Layouts/ON_DISK_FORMAT_INVENTORY_AND_VERSION_MANIFEST.md` covering every page, tuple, pointer, checksum, and compatibility constraint. |
| P1 | Table-storage model and storage-mode maturity classification | `05_access_methods_and_table_storage.md` | Heap/page/visibility pieces exist across sections 02, 03, 05, 08, and 18. | New `02_Filespace_Lifecycle/TABLE_STORAGE_MODES_AND_ROW_MOVEMENT_MODEL.md` plus a maturity matrix for heap, secondary storage, and columnar/append-style modes. |

## Recommended Follow-On Spec Work Order
1. Close the P0 contracts first: memory ownership, metadata invalidation, DDL behavior, invariant catalog, temp/spill, time semantics, governance budgets, and bulk-load behavior.
2. Add the P1 cross-cutting contracts next: threat model, thread/worker model, platform scope, test governance, compatibility lifecycle, rewrite inventory, and phantom-protection matrix.
3. Finish the consolidation specs after that: subsystem ownership map, on-disk format inventory, and table-storage maturity model.

## Notes
- This review intentionally does not treat implementation immaturity as a spec gap when the canonical contract already exists.
- Several audit recommendations remain valid as implementation or certification work, but they are not missing from the specification tree and therefore are excluded here.

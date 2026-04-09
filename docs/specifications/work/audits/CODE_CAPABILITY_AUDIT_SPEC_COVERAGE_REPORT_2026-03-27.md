# Code Capability Audit vs Canonical Specification Coverage Report

Date: `2026-03-27`

## Scope

This report compares the combined audit in `local_work/findings/CodeCapabilityAudit.md` against the authoritative specification tree in `docs/specifications/`.

Primary authority sources used for this pass:

- `AUTHORITATIVE_SPEC_INVENTORY.md`
- canonical numbered sections under `00_*` through `31_*`
- targeted anchor specs for optimizer, governance, replication, time discipline, platform scope, driver baselines, and lifecycle

## Executive Assessment

The main conclusion is that the specification set is materially ahead of the implementation in many areas that the audit marked as missing or weak.

- A large share of the audit's `missing work/features` are already present in canonical specs.
- Most of the audit's `suggestions` should become implementation work-plans, traceability work, or certification/gate work, not fresh specification drafting.
- The dominant specification problem is not wholesale absence. It is fragmentation in a smaller set of topics that are spread across multiple sections without one clean audit-facing contract.

Coverage summary for the 38 audit sections:

- `31` sections are `Explicitly covered in canonical specs`
- `7` sections are `Partially / fragmentedly covered`
- `0` sections are `clearly absent as major themes`

## Coverage classes

- `Explicit`: the audit topic is directly represented by named authoritative specs.
- `Partial / fragmented`: substantial spec coverage exists, but it is distributed and would benefit from a consolidating canonical contract.
- `Not clear`: no clean canonical contract found for the audit topic.

## Section-by-section coverage matrix

| Audit section | Coverage | Primary canonical anchors | Assessment |
| --- | --- | --- | --- |
| `01` System architecture and component boundaries | `Explicit` | `00_Governance_and_Invarients/SUBSYSTEM_OWNERSHIP_AND_DEPENDENCY_BOUNDARIES.md`, `00_Governance_and_Invarients/SUBSYSTEM_CORRECTNESS_INVARIANT_CATALOG.md` | Treat audit gaps here as implementation-boundary and traceability work, not missing specification authority. |
| `02` Memory management | `Explicit` | `03_Disk_Allocator_and_Free_Space/MEMORY_POLICY_DOMAINS_AND_RESIDENCY_SEGMENTS.md`, `03_Disk_Allocator_and_Free_Space/NUMA_LOCALITY_AND_FRAME_OWNERSHIP.md`, `23_SBLR_VM_Compiler_and_Executor/EXECUTION_MEMORY_CONTEXTS_AND_BUDGETS.md` | Memory-domain, residency, NUMA, and execution-budget concerns are already specified. |
| `03` On-disk storage format | `Explicit` | `05_Page_Taxonomy_and_Binary_Layouts/ON_DISK_FORMAT_INVENTORY_AND_VERSION_MANIFEST.md`, `05_Page_Taxonomy_and_Binary_Layouts/PAGE_HEADER_LAYOUT.md` | Format/versioning work is already canonically defined. |
| `04` Storage space management | `Explicit` | `02_Filespace_Lifecycle/*`, `03_Disk_Allocator_and_Free_Space/*` | Allocation, writeback, locality, and fragmentation policy are already present. |
| `05` Access methods and table storage | `Partial / fragmented` | `02_Filespace_Lifecycle/TABLE_STORAGE_MODES_AND_ROW_MOVEMENT_MODEL.md`, `03_Disk_Allocator_and_Free_Space/VERSION_PLACEMENT_LOCALITY_AND_FRAGMENTATION_POLICY.md` | Table-storage behavior exists in pieces, but there is not yet one clean access-method/table-storage contract matching the audit section. |
| `06` Index subsystem | `Explicit` | `18_Index_Framework/*`, `31_Conformance_Performance_and_Reliability_Gates/INDEX_GOVERNANCE_AND_SCORECARD_CONTRACT.md` | The index specification surface is broad and already stronger than the code in many places. |
| `07` Transaction management | `Explicit` | `08_Transaction_Core/TRANSACTION_LIFECYCLE.md`, `08_Transaction_Core/MGA_TRANSACTION_PUBLICATION_AND_RESTART_SEMANTICS.md` | Transaction lifecycle, publication, savepoints, and inventory are specified. |
| `08` Concurrency control | `Explicit` | `09_Lock_Manager_Core/LOCK_MANAGER_NORMATIVE_IMPLEMENTATION.md`, `09_Lock_Manager_Core/ISOLATION_LEVEL_AND_PHANTOM_PROTECTION_MATRIX.md` | Locking and isolation concerns are already canonically defined. |
| `09` Logging and durability | `Explicit` | `08_Transaction_Core/ALPHA_DURABILITY_MODES_AND_FLUSH_ORDERING.md`, `03_Disk_Allocator_and_Free_Space/BACKGROUND_WRITER_DIRTY_PAGE_TRACKING_AND_WRITEBACK.md` | Durability policy and write ordering already exist in specs. |
| `10` Crash recovery | `Explicit` | `08_Transaction_Core/CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md`, `08_Transaction_Core/FAILURE_MODEL_AND_RECOVERY_CLASSIFICATION.md` | Recovery classification and restart-state rules are explicitly specified. |
| `11` Checkpointing and background maintenance | `Explicit` | `08_Transaction_Core/CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md`, `03_Disk_Allocator_and_Free_Space/BACKGROUND_WRITER_DIRTY_PAGE_TRACKING_AND_WRITEBACK.md`, `10_GC_and_Sweep/*` | This area is already specified, though across multiple sections. |
| `12` Query parser and SQL front-end language layer | `Explicit` | `21_V3_Dialect_Surface/*`, `28_Parser_Implementations/*` | Parser, normalization, dialect, and front-end language behavior are already specified. |
| `13` Query rewrite and logical transformation | `Explicit` | `23_SBLR_VM_Compiler_and_Executor/LOGICAL_REWRITE_INVENTORY_AND_TRACE.md`, `23_SBLR_VM_Compiler_and_Executor/OPTIMIZER_PASS_PIPELINE.md` | Rewrite inventory, ordering, and traceability are already canonical. |
| `14` Optimizer and planner | `Explicit` | `23_SBLR_VM_Compiler_and_Executor/OPTIMIZER_ARCHITECTURE_AND_MAIN_PATH_INTEGRATION.md`, `23_SBLR_VM_Compiler_and_Executor/PLANNER_FRONT_DOOR_AND_STATEMENT_PLANNING_API.md`, `23_SBLR_VM_Compiler_and_Executor/JOIN_SEARCH_AND_METHOD_ENUMERATION.md` | Optimizer/planner work is largely a code-closure problem, not a spec gap. |
| `15` Execution engine | `Explicit` | `23_SBLR_VM_Compiler_and_Executor/VM_EXECUTION_ARCHITECTURE.md`, `23_SBLR_VM_Compiler_and_Executor/NORMATIVE_ENGINE_PLAN_AND_EXECUTION_CHECKLIST.md` | Execution runtime, dispatch, diagnostics, and cache contracts are already present. |
| `16` Statistics and metadata subsystem | `Partial / fragmented` | `23_SBLR_VM_Compiler_and_Executor/CARDINALITY_STATISTICS_AND_COST_MODEL_GOVERNANCE.md`, `24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_INVENTORY.md`, `24_Catalog_Model_and_Virtual_Overlays/EMULATED_STATS_MAPPING.md` | Strong pieces exist, but statistics lifecycle and metadata ownership are distributed rather than consolidated into one audit-facing subsystem contract. |
| `17` Schema management and DDL | `Partial / fragmented` | `21_V3_Dialect_Surface/NATIVE_SQL_SURFACE.md`, `28_Parser_Implementations/*`, `24_Catalog_Model_and_Virtual_Overlays/CATALOG_OBJECT_SCHEMA_BRANCH_ASSIGNMENT.md` | DDL syntax, parser behavior, and catalog placement exist, but a unified schema/DDL lifecycle/state-machine spec is still diffuse. |
| `18` Data types and encoding | `Explicit` | `13_Operator_Model_and_Coercion/*`, `14_Base_Scalar_Types/*`, `15_Complex_Types/*` | Type-system and encoding topics are already represented canonically. |
| `19` Procedural runtime and extensibility | `Explicit` | `17_Functions_and_Procedures/*`, `23_SBLR_VM_Compiler_and_Executor/NATIVE_COMPILATION_AND_ARTIFACT_LIFECYCLE.md` | UDR/procedural/runtime extensibility is already deeply specified. |
| `20` Networking protocol and session layer | `Explicit` | `26_Native_Wire_Protocol/*`, `27_Native_Handshake/*`, `29_Listener_and_Server_Orchestration/CONNECTION_AND_SESSION_LIFECYCLE.md` | Wire protocol, handshake, and session lifecycle are all specified. |
| `21` Client driver interface and API contract | `Explicit` | `30_Client_Tooling/*`, `21_V3_Dialect_Surface/NATIVE_JDBC_COMPATIBILITY_SQL.md` | Driver/API contract coverage is already extensive and multi-language. |
| `22` Replication and high availability | `Explicit` | `29_Listener_and_Server_Orchestration/NORMATIVE_LISTENER_ONE_WAY_AND_BIDIRECTIONAL_REPLICATION_CHECKLIST.md`, `26_Native_Wire_Protocol/CLUSTER_UDR_FABRIC_CHANNEL_SPEC.md`, `30_Client_Tooling/NATIVE_REPLICATION_SQL_CONTROL_CONTRACT.md` | Replication/HA is already in specs, including ordering, fencing, lag, and split-brain handling. |
| `23` Backup, restore, and import/export | `Explicit` | `08_Transaction_Core/BACKUP_RESTORE_SUPPORT_MATRIX_AND_VALIDATION.md`, `23_SBLR_VM_Compiler_and_Executor/BULK_LOAD_AND_COPY_EXECUTION_CONTRACT.md`, `30_Client_Tooling/TOOL_COMMAND_SURFACE_CONTRACTS.md` | Backup/restore and bulk ingest/export are already specified. |
| `24` Security model | `Explicit` | `19_Security_Model/*` | Security coverage is already canonical and broad. |
| `25` Observability and diagnostics | `Explicit` | `20_Diagnostics_Audit_and_Observability/*`, `29_Listener_and_Server_Orchestration/LISTENER_OBSERVABILITY_AND_AUDIT_CONTRACT.md` | Observability, audit, and diagnostic control surfaces are already specified. |
| `26` Configuration and tuning surface | `Explicit` | `01_Configuration_Subsystem/*` | Config/bootstrap/tuning coverage is already canonical. |
| `27` Workload management and resource governance | `Explicit` | `25_Runtime_Modes/ENGINE_RESOURCE_GOVERNANCE_AND_BUDGETS.md`, `23_SBLR_VM_Compiler_and_Executor/EXECUTION_MEMORY_CONTEXTS_AND_BUDGETS.md` | Admission, budgets, throttling, queueing, and reservation semantics are already present. |
| `28` Parallelism and scalability model | `Partial / fragmented` | `23_SBLR_VM_Compiler_and_Executor/PLAN_CACHE_PARALLELISM_AND_OPTIMIZER_FEEDBACK.md`, `25_Runtime_Modes/ENGINE_THREAD_WORKER_AND_TASK_MODEL.md`, `25_Runtime_Modes/NORMATIVE_P2_CLUSTER_COST_AWARE_PLACEMENT_AND_SCHEDULING_CHECKLIST.md` | Parallel and scalability topics exist, but there is no single unified engine-wide parallelism/scaling contract. |
| `29` Integrity and correctness mechanisms | `Explicit` | `00_Governance_and_Invarients/*`, `05_Page_Taxonomy_and_Binary_Layouts/CHECKSUM_AND_INTEGRITY.md`, `08_Transaction_Core/FAILURE_MODEL_AND_RECOVERY_CLASSIFICATION.md` | Correctness invariants and integrity mechanisms are already present. |
| `30` Maintenance operations | `Partial / fragmented` | `10_GC_and_Sweep/*`, `18_Index_Framework/INDEX_BUILD_AND_MAINTENANCE.md`, `31_Conformance_Performance_and_Reliability_Gates/RELIABILITY_CHAOS_AND_RECOVERY_GATES.md` | Sweep, rebuild, analyze, repair, verify, and maintenance gates exist, but not under one consolidated maintenance-ops contract. |
| `31` Bulk data paths | `Explicit` | `23_SBLR_VM_Compiler_and_Executor/BULK_LOAD_AND_COPY_EXECUTION_CONTRACT.md`, `30_Client_Tooling/TOOL_COMMAND_SURFACE_CONTRACTS.md` | Bulk ingest/export work is explicitly specified. |
| `32` Temporary data and spill subsystem | `Explicit` | `12_Temporary_Tables/TEMP_WORKFILE_AND_OPERATOR_SPILL_CONTRACT.md`, `23_SBLR_VM_Compiler_and_Executor/EXECUTION_MEMORY_CONTEXTS_AND_BUDGETS.md` | Temp tables, workfiles, spill policy, and quotas are already specified. |
| `33` Time, clocks, and ordering assumptions | `Explicit` | `25_Runtime_Modes/ENGINE_TIME_SOURCE_AND_ORDERING_DISCIPLINE.md`, `25_Runtime_Modes/CLUSTER_CLOCK_DISCIPLINE_AND_SKEW_POLICY.md` | Time discipline and ordering assumptions are explicitly specified. |
| `34` Portability and platform interface | `Partial / fragmented` | `31_Conformance_Performance_and_Reliability_Gates/PLATFORM_SUPPORT_MATRIX_AND_CERTIFICATION_SCOPE.md`, `26_Native_Wire_Protocol/IPC_SBWP_FRAME_SPEC.md`, `23_SBLR_VM_Compiler_and_Executor/NATIVE_COMPILATION_AND_ARTIFACT_LIFECYCLE.md` | Platform certification, endianness, and artifact portability exist, but there is not one unified engine platform-interface/filesystem contract. |
| `35` Build, testing, and verification infrastructure | `Explicit` | `31_Conformance_Performance_and_Reliability_Gates/*` | Build/test/gate ownership is already strongly specified. |
| `36` Performance engineering framework | `Explicit` | `31_Conformance_Performance_and_Reliability_Gates/P1_P2_OPTIMIZATION_GATE_PROFILE.md`, `31_Conformance_Performance_and_Reliability_Gates/INDEX_BENCHMARK_CORPUS_AND_RELEASE_GATES.md`, `23_SBLR_VM_Compiler_and_Executor/CARDINALITY_STATISTICS_AND_COST_MODEL_GOVERNANCE.md` | Performance work already has gate, benchmark, and optimizer-governance specification anchors. |
| `37` Upgrade, compatibility, and lifecycle management | `Partial / fragmented` | `05_Page_Taxonomy_and_Binary_Layouts/ON_DISK_FORMAT_INVENTORY_AND_VERSION_MANIFEST.md`, `31_Conformance_Performance_and_Reliability_Gates/FEATURE_LIFECYCLE_AND_CROSS_VERSION_COMPATIBILITY_MATRIX.md` | Cross-version and feature lifecycle rules exist, but there is no single operational upgrade/rollback lifecycle contract covering the whole system. |
| `38` Failure model and fault tolerance assumptions | `Explicit` | `08_Transaction_Core/FAILURE_MODEL_AND_RECOVERY_CLASSIFICATION.md`, `31_Conformance_Performance_and_Reliability_Gates/RELIABILITY_CHAOS_AND_RECOVERY_GATES.md` | Failure classes and tolerance assumptions are already canonically represented. |

## What is already in the specifications

These are the strongest examples where the audit's missing items are already specification-backed and should be treated as implementation closure work:

- subsystem ownership and dependency boundaries
- memory policy domains, NUMA locality, execution budgets, and spill policy
- on-disk format manifest, page taxonomy, and compatibility/version rules
- MGA transaction lifecycle, durability modes, checkpoint/recovery state machine, and failure classification
- lock manager and isolation/phantom protection matrix
- parser normalization, dialect surface, and JDBC promotion/de-rewrite authority
- logical rewrite inventory and trace surfaces
- optimizer pass pipeline, planner front door, join search, access-path planning, typed statistics, and feedback loops
- VM execution architecture, diagnostics, native compilation lifecycle, and cache invalidation
- driver baselines across JDBC, ODBC, C++, .NET, Go, Rust, Node, Python, PHP, Ruby, Pascal, Mojo, CLI, Dart, Swift, and R
- replication/HA orchestration including stream ordering, lag handling, split-brain fencing, and control surfaces
- backup/restore support matrix plus `COPY` and bulk-load contract
- security control model
- diagnostics, audit, forensic, and observability contracts
- runtime resource governance, queueing, throttling, and budget floors
- time discipline, monotonic versus wall-clock rules, and cluster skew policy
- platform certification matrix, lifecycle state taxonomy, and cross-version compatibility axes

## What is not cleanly represented as a single canonical contract

These are the highest-value specification consolidation candidates.

### `05` Access methods and table storage

What is present:

- storage modes
- row movement rules
- locality/fragmentation policy

What is still weak:

- one unified table access-method model covering heap, overflow, version placement, and alternative storage modes under a single authority document

Recommended spec work:

- create a consolidated table storage and access-method contract, then link out to allocator/layout details

### `16` Statistics and metadata subsystem

What is present:

- typed statistics and cost governance
- catalog inventory and stats mapping

What is still weak:

- one authoritative subsystem contract for metadata ownership, refresh lifecycle, invalidation, persistence classes, and audit surfaces

Recommended spec work:

- create a canonical statistics-and-metadata subsystem contract that ties section `23` and section `24` together

### `17` Schema management and DDL

What is present:

- DDL surface in the SQL specs
- parser rules
- catalog placement and branch assignment

What is still weak:

- one schema/DDL state-machine contract for create/alter/drop/rename, dependency invalidation, publication order, rollback, and migration safety

Recommended spec work:

- add a consolidated schema-change lifecycle specification and explicit DDL publication rules

### `28` Parallelism and scalability model

What is present:

- parallel planning references
- worker/task model
- cluster placement and scheduling
- governance budgets for parallel workers

What is still weak:

- a single model that explains how single-node parallel execution, cluster scheduling, admission, and scaling limits fit together

Recommended spec work:

- add a unified parallelism/scalability contract with local versus cluster scope made explicit

### `30` Maintenance operations

What is present:

- sweep and GC
- index build/maintenance
- verify/repair/reliability gates

What is still weak:

- one operator-facing maintenance contract tying together sweep, analyze, rebuild, verify, backup windows, and maintenance safety classes

Recommended spec work:

- create a maintenance-operations umbrella spec and reference the lower-level subsystem specs from it

### `34` Portability and platform interface

What is present:

- platform certification scope
- wire endianness/encoding rules
- native artifact portability/rebasing rules

What is still weak:

- one engine-wide platform-interface contract for OS assumptions, filesystem expectations, supported runtime facilities, and portability boundaries

Recommended spec work:

- add a dedicated platform-interface/filesystem contract

### `37` Upgrade, compatibility, and lifecycle management

What is present:

- on-disk format version manifest
- feature lifecycle state matrix
- compatibility axes

What is still weak:

- one operational upgrade/rollback lifecycle contract across binaries, catalog, storage, protocols, and driver/client expectations

Recommended spec work:

- add a system upgrade and rollback orchestration specification that references format and lifecycle matrices

## Net interpretation for planning

The audit should now be split into two work streams.

### Stream A: implementation-against-existing-spec

These sections already have enough canonical specification authority that the next work should be implementation, closure, and conformance evidence:

`01`, `02`, `03`, `04`, `06`, `07`, `08`, `09`, `10`, `11`, `12`, `13`, `14`, `15`, `18`, `19`, `20`, `21`, `22`, `23`, `24`, `25`, `26`, `27`, `29`, `31`, `32`, `33`, `35`, `36`, `38`

### Stream B: spec-consolidation before or alongside implementation planning

These sections need canonical consolidation so later work-plans are cleaner and less contradictory:

`05`, `16`, `17`, `28`, `30`, `34`, `37`

## Recommended next work-plans

1. Build a `code-vs-spec traceability matrix` for the `Explicit` sections.
2. Create `seven spec-consolidation workpacks` for sections `05`, `16`, `17`, `28`, `30`, `34`, and `37`.
3. For each `Explicit` section, convert the audit's `missing work/features` into implementation backlog rows tagged as:
   - `spec exists / implementation missing`
   - `spec exists / tests or gates missing`
   - `spec exists / partial implementation`
4. For each `Partial / fragmented` section, write one short canonical umbrella spec before starting large implementation plans.
5. After those two passes, rerun the combined audit and classify every remaining weakness as either:
   - `implementation gap`
   - `test/gate gap`
   - `spec gap`

## Bottom line

The current specification problem is mostly not "we forgot to specify this."

The stronger and more useful reading is:

- the code audit exposed many areas where implementation is behind already-written specs
- the remaining spec work is mainly consolidation in a small number of cross-cutting areas
- the next planning cycle should therefore be split between `implementation closure` and `spec consolidation`, not broad new specification drafting

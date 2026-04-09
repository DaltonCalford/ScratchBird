# Definitive Specset Index

## Scope Controller

The authoritative scope for this package is controlled by:

- `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`

Every canonical file listed there is in scope for verification.

## Root Canonical Inputs

- `docs/specifications/README.md`
- `docs/specifications/Reference_Documentation_specification.md`
- `docs/specifications/00_Governance_and_Invarients/WORK_PLAN_MANAGEMENT_STANDARD_AND_LIFECYCLE.md`
- `docs/specifications/00_Governance_and_Invarients/CANONICAL_STATUS_VOCABULARY_AND_NORMALIZATION_RULE.md`
- `docs/specifications/00_Governance_and_Invarients/CROSS_SECTION_PRECEDENCE_AND_CONTRADICTION_RESOLUTION_RULE.md`
- `docs/specifications/00_Governance_and_Invarients/RECONSTRUCTED_REQUIRED_BEHAVIOR_AND_IMPLEMENTATION_DRIFT_RULE.md`

## Numbered Canonical Section Roots

- `docs/specifications/00_Governance_and_Invarients/README.md`
- `docs/specifications/01_Configuration_Subsystem/README.md`
- `docs/specifications/02_Filespace_Lifecycle/README.md`
- `docs/specifications/03_Disk_Allocator_and_Free_Space/README.md`
- `docs/specifications/04_Page_Size_Policy/README.md`
- `docs/specifications/05_Page_Taxonomy_and_Binary_Layouts/README.md`
- `docs/specifications/06_Fixed_Bootstrap_Page_Map/README.md`
- `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/README.md`
- `docs/specifications/08_Transaction_Core/README.md`
- `docs/specifications/09_Lock_Manager_Core/README.md`
- `docs/specifications/10_GC_and_Sweep/README.md`
- `docs/specifications/11_TOAST_and_LOB_Storage/README.md`
- `docs/specifications/12_Temporary_Tables/README.md`
- `docs/specifications/13_Operator_Model_and_Coercion/README.md`
- `docs/specifications/14_Base_Scalar_Types/README.md`
- `docs/specifications/15_Complex_Types/README.md`
- `docs/specifications/16_Context_Variables/README.md`
- `docs/specifications/17_Functions_and_Procedures/README.md`
- `docs/specifications/18_Index_Framework/README.md`
- `docs/specifications/19_Security_Model/README.md`
- `docs/specifications/20_Diagnostics_Audit_and_Observability/README.md`
- `docs/specifications/21_V3_Dialect_Surface/README.md`
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/README.md`
- `docs/specifications/23_SBLR_VM_Compiler_and_Executor/README.md`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/README.md`
- `docs/specifications/25_Runtime_Modes/README.md`
- `docs/specifications/26_Native_Wire_Protocol/README.md`
- `docs/specifications/27_Native_Handshake/README.md`
- `docs/specifications/28_Parser_Implementations/README.md`
- `docs/specifications/29_Listener_and_Server_Orchestration/README.md`
- `docs/specifications/30_Client_Tooling/README.md`
- `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/README.md`
- `docs/specifications/32_Architecture_and_Component_Boundaries/README.md`
- `docs/specifications/33_Memory_Management/README.md`
- `docs/specifications/34_Table_Storage_and_Access_Methods/README.md`
- `docs/specifications/35_Durability_Crash_Recovery_and_Checkpoint_Model/README.md`
- `docs/specifications/36_Query_Rewrite_and_Planner/README.md`
- `docs/specifications/37_Statistics_Metadata_and_Schema_DDL/README.md`
- `docs/specifications/38_Workload_Governance_and_Parallelism/README.md`
- `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/README.md`
- `docs/specifications/40_Time_Clocks_and_Ordering_Assumptions/README.md`
- `docs/specifications/41_Platform_Interface_and_Lifecycle_Management/README.md`
- `docs/specifications/42_Failure_Model_and_Fault_Tolerance/README.md`

## Live Implementation Roots

- `include/`
- `src/`
- `tests/`
- `scripts/`
- `docs/` for repo-local implementation notes that are the only current proof
  for a status claim
- `../ScratchBird-driver/`
- `../ScratchBird-Benchmarks/`
- `../ScratchRobin/`

## Audit Anchor Rule

Every live-code reference created or maintained by this package must use:

- `implementation_path`
- one file-local `unique_search_key`

Line-number anchors are prohibited.

## Final Rollup Outputs Owned By This Package

- `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`
- `SPEC_STATUS_CLASSIFICATION.csv`
- `LINE_NUMBER_TO_SEARCH_KEY_MIGRATION_LOG.csv`
- `FINISHED_SPECIFICATIONS.md`
- `PARTIAL_SPECIFICATIONS.md`
- `OUTSTANDING_SPECIFICATIONS.md`

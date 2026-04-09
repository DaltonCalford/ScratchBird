# Definitive Specset Index

## Global Classification Rule

For Beta 1 planning, every canonical requirement is in scope unless it is
explicitly marked Beta 2 or Beta 3.

Mixed files remain in scope for their non-Beta 2 and non-Beta 3 clauses.

## Governance And Work-Plan Authorities

- `../specifications/00_Governance_and_Invarients/WORK_PLAN_MANAGEMENT_STANDARD_AND_LIFECYCLE.md`
- `../specifications/AUTHORITATIVE_SPEC_INVENTORY.md`
- `../specifications/Reference_Documentation_specification.md`
- `../reference/README.md`
- `README.md`
- `WORKPLAN_GENERATION_INPUT.md`

## Beta 1 Scoping Authorities

- `../specifications/25_Runtime_Modes/CLOUD_SUPPORT_SCOPE_AND_BETA1_BETA2_PROGRAM_MODEL.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/PUBLIC_BETA_REQUIRED_GATE_EXECUTION_AND_FAILURE_MODEL.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/PUBLIC_BETA_REQUIRED_GATE_CATEGORY_AND_STEP_MODEL.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/SCRATCHBIRD_BENCHMARKS_PROJECT_AND_MATRIX_MODEL.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/FULL_CLEAN_BUILD_TEST_AND_BENCHMARK_ARTIFACT_MODEL.md`
- `../TEST.md`

## Canonical Section Inventory By Downstream Work-Plan

### `01-Core_MGA_Storage_Recovery_Buffers`

- `../specifications/02_Filespace_Lifecycle/README.md`
- `../specifications/03_Disk_Allocator_and_Free_Space/README.md`
- `../specifications/04_Page_Size_Policy/README.md`
- `../specifications/05_Page_Taxonomy_and_Binary_Layouts/README.md`
- `../specifications/06_Fixed_Bootstrap_Page_Map/README.md`
- `../specifications/08_Transaction_Core/README.md`
- `../specifications/09_Lock_Manager_Core/README.md`
- `../specifications/10_GC_and_Sweep/README.md`
- `../specifications/11_TOAST_and_LOB_Storage/README.md`
- `../specifications/35_Durability_Crash_Recovery_and_Checkpoint_Model/README.md`
- `../specifications/40_Time_Clocks_and_Ordering_Assumptions/README.md`
- `../specifications/42_Failure_Model_and_Fault_Tolerance/README.md`

### `02-Catalog_UUID_Metadata_DDL_Schema`

- `../specifications/01_Configuration_Subsystem/README.md`
- `../specifications/07_Catalog_Bootstrap_and_UUID_Mapping/README.md`
- `../specifications/24_Catalog_Model_and_Virtual_Overlays/README.md`
- `../specifications/37_Statistics_Metadata_and_Schema_DDL/README.md`

### `03-Type_System_SBLR_V3_Parser_Execution`

- `../specifications/12_Temporary_Tables/README.md`
- `../specifications/13_Operator_Model_and_Coercion/README.md`
- `../specifications/14_Base_Scalar_Types/README.md`
- `../specifications/15_Complex_Types/README.md`
- `../specifications/16_Context_Variables/README.md`
- `../specifications/17_Functions_and_Procedures/README.md`
- `../specifications/21_V3_Dialect_Surface/README.md`
- `../specifications/22_SBLR_Canonical_Model_and_Opcodes/README.md`
- `../specifications/23_SBLR_VM_Compiler_and_Executor/README.md`
- `../specifications/28_Parser_Implementations/README.md`

### `04-Access_Methods_Indexes_Optimizer_Memory`

- `../specifications/18_Index_Framework/README.md`
- `../specifications/33_Memory_Management/README.md`
- `../specifications/34_Table_Storage_and_Access_Methods/README.md`
- `../specifications/36_Query_Rewrite_and_Planner/README.md`
- `../specifications/38_Workload_Governance_and_Parallelism/README.md`

### `05-Service_Stack_LocalIPC_Wire_Listeners_Manager`

- `../specifications/25_Runtime_Modes/README.md`
- `../specifications/26_Native_Wire_Protocol/README.md`
- `../specifications/27_Native_Handshake/README.md`
- `../specifications/29_Listener_and_Server_Orchestration/README.md`
- `../specifications/32_Architecture_and_Component_Boundaries/README.md`

### `06-Security_Authorization_Audit_Sandboxing`

- `../specifications/19_Security_Model/README.md`
- `../specifications/20_Diagnostics_Audit_and_Observability/README.md`

### `07-Backup_Restore_Migration_Cloud_Beta1_Ops`

- `../specifications/25_Runtime_Modes/CLOUD_SUPPORT_SCOPE_AND_BETA1_BETA2_PROGRAM_MODEL.md`
- `../specifications/39_Backup_Restore_and_Bulk_Data_Paths/README.md`
- `../specifications/41_Platform_Interface_and_Lifecycle_Management/README.md`
- `../specifications/30_Client_Tooling/README.md`

### `08-Tooling_Drivers_Benchmarks_Gates_Release`

- `../specifications/30_Client_Tooling/README.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/README.md`

## Explicit Beta 2 Or Beta 3-Only Exclusions

The following canonical files are not part of the Beta 1 downstream
implementation set because they are explicitly Beta 2-only today.

- `../specifications/31_Conformance_Performance_and_Reliability_Gates/BETA2_OPTIMIZER_CERTIFICATION_AND_REGRESSION_GATES.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/CLOUD_READINESS_AND_BETA2_CLUSTER_CERTIFICATION_MODEL.md`
- `../specifications/36_Query_Rewrite_and_Planner/BETA2_COMMERCIAL_GRADE_OPTIMIZER_PROGRAM_MODEL.md`
- `../specifications/36_Query_Rewrite_and_Planner/PLANNER_UNIFICATION_PHYSICAL_PROPERTY_AND_ACCESS_TRUST_BETA2_MODEL.md`
- `../specifications/36_Query_Rewrite_and_Planner/COMMERCIAL_PLAN_STORE_BASELINES_AND_REGRESSION_GOVERNANCE_BETA2_MODEL.md`
- `../specifications/36_Query_Rewrite_and_Planner/ADAPTIVE_QUERY_PROCESSING_MEMORY_GRANT_AND_INTERLEAVED_EXECUTION_BETA2_MODEL.md`
- `../specifications/36_Query_Rewrite_and_Planner/PARAMETER_SENSITIVE_PLAN_OPTIMIZATION_AND_WORKLOAD_FEEDBACK_BETA2_MODEL.md`

If later canonical files are explicitly marked Beta 3, they are excluded by the
same rule and must be added to this register when this work-plan is executed.

## Gate And Certification Authorities

- `../specifications/31_Conformance_Performance_and_Reliability_Gates/PUBLIC_BETA_REQUIRED_GATE_EXECUTION_AND_FAILURE_MODEL.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/PUBLIC_BETA_REQUIRED_GATE_CATEGORY_AND_STEP_MODEL.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/TEST_CONTRACT.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/SCRATCHBIRD_BENCHMARKS_PROJECT_AND_MATRIX_MODEL.md`
- `../specifications/31_Conformance_Performance_and_Reliability_Gates/FULL_CLEAN_BUILD_TEST_AND_BENCHMARK_ARTIFACT_MODEL.md`
- `../TEST.md`

## Live Audit Anchors

| Authority | Implementation path | Unique search key | Use |
| --- | --- | --- | --- |
| public beta gate runner | `tests/conformance/public_beta/run_required_public_beta_gate.sh` | `declare -A CATEGORY_PASS=(` | minimum Beta 1 gate categories |
| public beta gate runner | `tests/conformance/public_beta/run_required_public_beta_gate.sh` | `run_script_step "wire_protocol" "compat_postgresql"` | wire protocol baseline anchor |
| public beta gate runner | `tests/conformance/public_beta/run_required_public_beta_gate.sh` | `run_script_step "transaction_semantics" "transaction_truth_matrix"` | transaction baseline anchor |
| public beta gate runner | `tests/conformance/public_beta/run_required_public_beta_gate.sh` | `run_script_step "security_enforcement" "security_parity_matrix"` | security baseline anchor |
| public beta gate runner | `tests/conformance/public_beta/run_required_public_beta_gate.sh` | `run_ctest_exact "modal_nosql" "parser_search_dsl_surface"` | modal or NoSQL baseline anchor |
| public beta gate runner | `tests/conformance/public_beta/run_required_public_beta_gate.sh` | `run_ctest_exact "cluster_infra" "cluster_fencing_term"` | current bounded cluster-infra gate anchor |
| test policy doc | `docs/TEST.md` | `## Required Public Beta Gate` | human-readable release baseline |
| test policy doc | `docs/TEST.md` | `## Test Policy for Public Beta` | release policy baseline |

## Planning Style Inputs

These are not scope authorities, but they are approved package-style inputs for
how the downstream plans should be structured.

- `../../../local_work/docs/planning/archive/MEMORY_MODEL_SPEC_IMPLEMENTATION_WORKTREE/README.md`
- `../../../local_work/docs/planning/archive/MEMORY_MODEL_SPEC_IMPLEMENTATION_WORKTREE/WORKPLAN_GENERATION_INPUT.md`
- `../../../local_work/docs/planning/archive/MEMORY_MODEL_SPEC_IMPLEMENTATION_WORKTREE/BOUNDED_TICKET_SET.md`

## Required Research Order For Generated Downstream Plans

1. assigned canonical specifications
2. consumed cross-section canonical specifications
3. `docs/reference/` local authority tree
4. web research only when the local reference tree does not contain the needed
   authority

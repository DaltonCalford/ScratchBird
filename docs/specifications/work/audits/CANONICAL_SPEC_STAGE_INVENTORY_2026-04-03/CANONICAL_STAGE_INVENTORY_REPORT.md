# Canonical Stage Inventory Report

Date: 2026-04-03

## Classification Rule

- explicit BETA3_* marker -> Beta 3
- explicit BETA2_* or *_BETA2_* marker -> Beta 2
- explicit cross-stage cloud files with both Beta 1 and Beta 2 scope -> included in both Beta 1 and Beta 2 inventories
- no explicit later-stage marker -> Beta 1

## Rollup

- authoritative canonical artifacts scanned: 1202
- implementation-driving artifacts after meta-file filtering: 987
- Beta 1 primary artifacts: 905
- Beta 1 / Beta 2 bridge artifacts: 2
- Beta 2 primary artifacts: 79
- Beta 3 primary artifacts: 1

## Definitive Lists

- Beta 1: [BETA1_IMPLEMENTATION_SCOPE.csv](BETA1_IMPLEMENTATION_SCOPE.csv)
- Beta 2: [BETA2_IMPLEMENTATION_SCOPE.csv](BETA2_IMPLEMENTATION_SCOPE.csv)
- Beta 3: [BETA3_IMPLEMENTATION_SCOPE.csv](BETA3_IMPLEMENTATION_SCOPE.csv)
- Full classified inventory: [CANONICAL_IMPLEMENTATION_STAGE_CLASSIFICATION.csv](CANONICAL_IMPLEMENTATION_STAGE_CLASSIFICATION.csv)
- Section counts: [SECTION_STAGE_COUNTS.csv](SECTION_STAGE_COUNTS.csv)

## Beta 1 / Beta 2 Bridge Files

- [docs/specifications/25_Runtime_Modes/CLOUD_SUPPORT_SCOPE_AND_BETA1_BETA2_PROGRAM_MODEL.md](docs/specifications/25_Runtime_Modes/CLOUD_SUPPORT_SCOPE_AND_BETA1_BETA2_PROGRAM_MODEL.md)
- [docs/specifications/31_Conformance_Performance_and_Reliability_Gates/CLOUD_READINESS_AND_BETA2_CLUSTER_CERTIFICATION_MODEL.md](docs/specifications/31_Conformance_Performance_and_Reliability_Gates/CLOUD_READINESS_AND_BETA2_CLUSTER_CERTIFICATION_MODEL.md)

## Beta 1 Section Counts

- 00_Governance_and_Invarients: 10
- 01_Configuration_Subsystem: 4
- 02_Filespace_Lifecycle: 8
- 03_Disk_Allocator_and_Free_Space: 20
- 04_Page_Size_Policy: 2
- 05_Page_Taxonomy_and_Binary_Layouts: 9
- 06_Fixed_Bootstrap_Page_Map: 1
- 07_Catalog_Bootstrap_and_UUID_Mapping: 3
- 08_Transaction_Core: 17
- 09_Lock_Manager_Core: 3
- 10_GC_and_Sweep: 9
- 11_TOAST_and_LOB_Storage: 5
- 12_Temporary_Tables: 2
- 13_Operator_Model_and_Coercion: 2
- 14_Base_Scalar_Types: 4
- 15_Complex_Types: 6
- 16_Context_Variables: 1
- 17_Functions_and_Procedures: 8
- 18_Index_Framework: 94
- 19_Security_Model: 58
- 20_Diagnostics_Audit_and_Observability: 22
- 21_V3_Dialect_Surface: 23
- 22_SBLR_Canonical_Model_and_Opcodes: 18
- 23_SBLR_VM_Compiler_and_Executor: 42
- 24_Catalog_Model_and_Virtual_Overlays: 83
- 25_Runtime_Modes: 46
- 26_Native_Wire_Protocol: 19
- 27_Native_Handshake: 5
- 28_Parser_Implementations: 55
- 29_Listener_and_Server_Orchestration: 23
- 30_Client_Tooling: 39
- 31_Conformance_Performance_and_Reliability_Gates: 110
- 32_Architecture_and_Component_Boundaries: 10
- 33_Memory_Management: 29
- 34_Table_Storage_and_Access_Methods: 6
- 35_Durability_Crash_Recovery_and_Checkpoint_Model: 9
- 36_Query_Rewrite_and_Planner: 25
- 37_Statistics_Metadata_and_Schema_DDL: 12
- 38_Workload_Governance_and_Parallelism: 10
- 39_Backup_Restore_and_Bulk_Data_Paths: 20
- 40_Time_Clocks_and_Ordering_Assumptions: 5
- 41_Platform_Interface_and_Lifecycle_Management: 22
- 42_Failure_Model_and_Fault_Tolerance: 7
- Reference_Documentation_specification.md: 1

## Beta 2 Section Counts

- 13_Operator_Model_and_Coercion: 1
- 14_Base_Scalar_Types: 1
- 15_Complex_Types: 1
- 17_Functions_and_Procedures: 30
- 18_Index_Framework: 2
- 21_V3_Dialect_Surface: 3
- 22_SBLR_Canonical_Model_and_Opcodes: 3
- 23_SBLR_VM_Compiler_and_Executor: 3
- 25_Runtime_Modes: 1
- 28_Parser_Implementations: 29
- 31_Conformance_Performance_and_Reliability_Gates: 2
- 36_Query_Rewrite_and_Planner: 5

## Beta 3 Section Counts

- 28_Parser_Implementations: 1

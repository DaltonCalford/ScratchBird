# ScratchBird Specifications (Canonical)

This tree is the canonical specification baseline for ScratchBird. Implementation
repo docs are informative, but canonical spec and planning control live here.

## Active Baseline

- Baseline version: `0.1.0` (initial early beta)
- Baseline date: `2026-02-19`

## Canonical Controls

- Reference documentation specification:
  `docs/specifications/Reference_Documentation_specification.md`
- Authoritative inventory:
  `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`
- Work-area findings:
  `docs/specifications/work/findings/`
- Work-area audits:
  `docs/specifications/work/audits/`
- Historical spec-tied work artifacts:
  `docs/specifications/work/`
- Active broader planning:
  `../local_work/docs/planning/`

## Section Index

- [00_Governance_and_Invarients](00_Governance_and_Invarients/README.md)
- [01_Configuration_Subsystem](01_Configuration_Subsystem/README.md)
- [02_Filespace_Lifecycle](02_Filespace_Lifecycle/README.md)
- [03_Disk_Allocator_and_Free_Space](03_Disk_Allocator_and_Free_Space/README.md)
- [04_Page_Size_Policy](04_Page_Size_Policy/README.md)
- [05_Page_Taxonomy_and_Binary_Layouts](05_Page_Taxonomy_and_Binary_Layouts/README.md)
- [06_Fixed_Bootstrap_Page_Map](06_Fixed_Bootstrap_Page_Map/README.md)
- [07_Catalog_Bootstrap_and_UUID_Mapping](07_Catalog_Bootstrap_and_UUID_Mapping/README.md)
- [08_Transaction_Core](08_Transaction_Core/README.md)
- [09_Lock_Manager_Core](09_Lock_Manager_Core/README.md)
- [10_GC_and_Sweep](10_GC_and_Sweep/README.md)
- [11_TOAST_and_LOB_Storage](11_TOAST_and_LOB_Storage/README.md)
- [12_Temporary_Tables](12_Temporary_Tables/README.md)
- [13_Operator_Model_and_Coercion](13_Operator_Model_and_Coercion/README.md)
- [14_Base_Scalar_Types](14_Base_Scalar_Types/README.md)
- [15_Complex_Types](15_Complex_Types/README.md)
- [16_Context_Variables](16_Context_Variables/README.md)
- [17_Functions_and_Procedures](17_Functions_and_Procedures/README.md)
- [18_Index_Framework](18_Index_Framework/README.md)
- [19_Security_Model](19_Security_Model/README.md)
- [20_Diagnostics_Audit_and_Observability](20_Diagnostics_Audit_and_Observability/README.md)
- [21_V3_Dialect_Surface](21_V3_Dialect_Surface/README.md)
- [22_SBLR_Canonical_Model_and_Opcodes](22_SBLR_Canonical_Model_and_Opcodes/README.md)
- [23_SBLR_VM_Compiler_and_Executor](23_SBLR_VM_Compiler_and_Executor/README.md)
- [24_Catalog_Model_and_Virtual_Overlays](24_Catalog_Model_and_Virtual_Overlays/README.md)
- [25_Runtime_Modes](25_Runtime_Modes/README.md)
- [26_Native_Wire_Protocol](26_Native_Wire_Protocol/README.md)
- [27_Native_Handshake](27_Native_Handshake/README.md)
- [28_Parser_Implementations](28_Parser_Implementations/README.md)
- [29_Listener_and_Server_Orchestration](29_Listener_and_Server_Orchestration/README.md)
- [30_Client_Tooling](30_Client_Tooling/README.md)
- [31_Conformance_Performance_and_Reliability_Gates](31_Conformance_Performance_and_Reliability_Gates/README.md)
- [32_Architecture_and_Component_Boundaries](32_Architecture_and_Component_Boundaries/README.md)
- [33_Memory_Management](33_Memory_Management/README.md)
- [34_Table_Storage_and_Access_Methods](34_Table_Storage_and_Access_Methods/README.md)
- [35_Durability_Crash_Recovery_and_Checkpoint_Model](35_Durability_Crash_Recovery_and_Checkpoint_Model/README.md)
- [36_Query_Rewrite_and_Planner](36_Query_Rewrite_and_Planner/README.md)
- [37_Statistics_Metadata_and_Schema_DDL](37_Statistics_Metadata_and_Schema_DDL/README.md)
- [38_Workload_Governance_and_Parallelism](38_Workload_Governance_and_Parallelism/README.md)
- [39_Backup_Restore_and_Bulk_Data_Paths](39_Backup_Restore_and_Bulk_Data_Paths/README.md)
- [40_Time_Clocks_and_Ordering_Assumptions](40_Time_Clocks_and_Ordering_Assumptions/README.md)
- [41_Platform_Interface_and_Lifecycle_Management](41_Platform_Interface_and_Lifecycle_Management/README.md)
- [42_Failure_Model_and_Fault_Tolerance](42_Failure_Model_and_Fault_Tolerance/README.md)

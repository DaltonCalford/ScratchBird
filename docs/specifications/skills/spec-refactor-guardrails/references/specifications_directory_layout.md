# Specifications Directory Layout

This is the canonical refactor layout under `docs/specifications/`.

## Root Controls
- `README.md`
- `AUTHORITATIVE_SPEC_INVENTORY.md`
- `skills/spec-refactor-guardrails/SKILL.md`
- `skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`
- `library/README.md`
- `work/README.md`

## Section Directories
- `00_Governance_and_Invarients/`
- `01_Configuration_Subsystem/`
- `02_Filespace_Lifecycle/`
- `03_Disk_Allocator_and_Free_Space/`
- `04_Page_Size_Policy/`
- `05_Page_Taxonomy_and_Binary_Layouts/`
- `06_Fixed_Bootstrap_Page_Map/`
- `07_Catalog_Bootstrap_and_UUID_Mapping/`
- `08_Transaction_Core/`
- `09_Lock_Manager_Core/`
- `10_GC_and_Sweep/`
- `11_TOAST_and_LOB_Storage/`
- `12_Temporary_Tables/`
- `13_Operator_Model_and_Coercion/`
- `14_Base_Scalar_Types/`
- `15_Complex_Types/`
- `16_Context_Variables/`
- `17_Functions_and_Procedures/`
- `18_Index_Framework/`
- `19_Security_Model/`
- `20_Diagnostics_Audit_and_Observability/`
- `21_V3_Dialect_Surface/`
- `22_SBLR_Canonical_Model_and_Opcodes/`
- `23_SBLR_VM_Compiler_and_Executor/`
- `24_Catalog_Model_and_Virtual_Overlays/`
- `25_Runtime_Modes/`
- `26_Native_Wire_Protocol/`
- `27_Native_Handshake/`
- `28_Parser_Implementations/`
- `29_Listener_and_Server_Orchestration/`
- `30_Client_Tooling/`
- `31_Conformance_Performance_and_Reliability_Gates/`

## Library Directory
- `library/manuals/`
- `library/technical_specs/`
- `library/whitepapers/`
- `library/third_party_implementations/`
- `library/development_environment/`
- `library/development_environment/REQUIRED_SOFTWARE.md`

## Work Directory
- `work/audits/`
- `work/implementation_tracks/`
- `work/planning/`
- `work/findings/`
- `work/migration_notes/`

## Placement Rule
- Numbered section directories contain canonical spec documents only.
- Non-authoritative work artifacts must be stored under `work/`.

## README Maintenance Contract
- Every section directory must contain a `README.md`.
- Every section `README.md` must include an auto-generated file index block.
- File index block must be synced after any file add, rename, or delete.
- Sync command from `docs/specifications/`:
  - `./skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`

## Git Hook
- `.githooks/pre-commit` runs README sync for section changes before commit.
- Expected repository setting: `core.hooksPath=.githooks`.
- Hook also blocks work-area folders under numbered sections.

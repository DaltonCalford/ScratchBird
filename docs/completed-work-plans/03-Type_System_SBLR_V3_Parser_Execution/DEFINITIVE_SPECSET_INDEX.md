# Definitive Specset Index

## Assigned Sections

- docs/specifications/12_Temporary_Tables/README.md
- docs/specifications/13_Operator_Model_and_Coercion/README.md
- docs/specifications/14_Base_Scalar_Types/README.md
- docs/specifications/15_Complex_Types/README.md
- docs/specifications/16_Context_Variables/README.md
- docs/specifications/17_Functions_and_Procedures/README.md
- docs/specifications/21_V3_Dialect_Surface/README.md
- docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/README.md
- docs/specifications/23_SBLR_VM_Compiler_and_Executor/README.md
- docs/specifications/28_Parser_Implementations/PARSER_ISOLATION_AND_DIALECT_LOCAL_SBLR_LOWERING_RULE.md
- docs/specifications/28_Parser_Implementations/SBLR_TO_V3_CONVERTER_AND_NAME_RECOVERY_RULES.md
- docs/specifications/28_Parser_Implementations/SBLR_TO_V3_RENDERING_AND_CONTEXT_RECONSTRUCTION_MODEL.md

## Package Boundary Note

- section `28` is intentionally bounded in this package to native-V3 parser isolation, direct native lowering, and SBLR-to-V3 reconstruction surfaces
- broader emulated-parser section-28 wire, listener, and family-parity material is deferred to later work-plans and is not a blocker for package `03`

## Global Governance Inputs

- docs/specifications/00_Governance_and_Invarients/WORK_PLAN_MANAGEMENT_STANDARD_AND_LIFECYCLE.md
- docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md
- docs/completed-work-plans/00-Beta1_Tasks/README.md
- docs/completed-work-plans/00-Beta1_Tasks/WORKPLAN_GENERATION_INPUT.md

## Required Research Order

1. assigned canonical specs
2. consumed cross-section canonical specs
3. docs/reference local authority tree
4. web research when local authority is insufficient

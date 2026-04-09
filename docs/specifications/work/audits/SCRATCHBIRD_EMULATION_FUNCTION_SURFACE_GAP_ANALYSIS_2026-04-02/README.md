# ScratchBird Emulation Function Surface Gap Analysis 2026-04-02

This audit compares the emulation-target donor engines against the current
Beta 2 parser or AST or SBLR specification set to isolate function surfaces
that still do not have a shared canonical landing zone.

Files:

- `EMULATION_FUNCTION_SURFACE_GAP_ANALYSIS.md`
- `FUNCTION_SURFACE_GAP_MATRIX.csv`
- `ENGINE_FUNCTION_GAP_SUMMARY.csv`

Scope rules:

- Only source-backed donor function or function-like surfaces are listed.
- Ordinary `name(args...)` scalar calls are not treated as gaps when the Beta 2
  shared `FunctionCallExpr` already covers them.
- This package is limited to function and function-like surfaces. DDL, command
  verbs, wire protocol, authentication, catalog overlays, and datatype/index
  work are out of scope here.
- `SQLite` remains an evidence-gap lane because the local donor packet still has
  no parser source clone for `src/parse.y`.

Primary canonical files reviewed:

- `/home/dcalford/CliWork/ScratchBird/docs/specifications/21_V3_Dialect_Surface/BETA2_DONOR_DIALECT_AST_GRAMMAR_AND_DESUGAR_EXPANSION_MODEL.md`
- `/home/dcalford/CliWork/ScratchBird/docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/BETA2_DONOR_DIALECT_SBLR_PAYLOAD_AND_OPCODE_EXPANSION_MODEL.md`
- `/home/dcalford/CliWork/ScratchBird/docs/specifications/23_SBLR_VM_Compiler_and_Executor/BETA2_DONOR_DIALECT_EXECUTION_AND_PLANNER_BINDING_MODEL.md`
- `/home/dcalford/CliWork/ScratchBird/docs/specifications/28_Parser_Implementations/BETA2_EMULATED_DONOR_MAPPING_AND_SHARED_LOWERING_MODEL.md`

# SBLR V3 Old-to-New Mapping Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_OLD_TO_NEW_MAPPING.md`
Date: 2026-02-09
Status: Partially verified (reference mapping)

## Summary
This document is a mapping table. Code verification here focuses on whether the V3 opcode set exists and is used. The V3 opcode list (`v3_opcodes.generated.cpp`) includes many mapped opcodes, but the system still emits/executes legacy patterns in multiple areas (e.g., MERGE uses `SBLR3_MERGE_START`, DDL uses specialized opcodes instead of `SBLR3_DDL_*` abstractions). Full mapping compliance is not verified.

## Findings
- [~] V3 opcode registry exists and includes the mapped opcode names.
  - See `src/sblr/v3_opcodes.generated.cpp`.
- [ ] Many parser/emitter paths still use non-canonical or legacy-style opcodes, so mapping adherence is incomplete.
  - Examples: MERGE uses `SBLR3_MERGE_START`; INSERT/UPDATE/DELETE use `SBLR3_*` instead of DML wrapper opcodes required by newer specs.
- [ ] No automated verification of mapping coverage found.

## Notes
This mapping is reference data. Enforcement must be validated via emission rules and opcode-spec compliance in other authoritative specs.

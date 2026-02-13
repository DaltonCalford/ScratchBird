# SBLR_V3_OPCODE_SPEC.md - Implementation Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md` (Authoritative, updated 2026-02-08)

Summary:
- The opcode registry exists as generated enum constants (`include/scratchbird/sblr/v3_opcodes.generated.h`), but several opcodes referenced in schema/payload mapping are missing from the registry.
- Instruction header encoding matches the spec, but enforcement of flags/payload rules is incomplete.
- The spec defines `varint` and `string_id` primitives; the codec does not implement `varint` (ZigZag) or `string_id` as a distinct field type.

Key findings:

## Instruction Header / Stream Rules
[*] Instruction header encoding matches `[opcode:u16][flags:u16][payload_len:u32]` and is little-endian (`src/sblr/v3_codec.cpp`, `src/sblr/v3_payloads.cpp`).
[~] `SBLR3_VERSION` must be first and `SBLR3_END` must be last: validator enforces ordering, but does not enforce `SBLR3_END` payload_len = 0 (only checks it is last). See `src/sblr/v3_validator.cpp`.
[ ] Flags must be zero unless specified: no validator enforcement; encoder will accept non-zero flags without error.
[ ] `SBLR3_EXTENDED_OPCODE` payload format is not enforced (no validation that payload starts with a u16 extended opcode or matches nested payload rules).

## Value Encoding (Normative)
[ ] `varint` (signed LEB128 with ZigZag) is not implemented in the V3 codec (`src/sblr/v3_codec.cpp` provides only `encodeVaruint` / `decodeVaruint`).
[ ] `string_id` primitive is not represented in schema field types; all identifiers are encoded as strings/idents.

## Opcode Registry Consistency
[ ] The generated opcode registry (`include/scratchbird/sblr/v3_opcodes.generated.h`) is missing several opcodes referenced elsewhere:
- `SBLR3_SET`, `SBLR3_SHOW`, `SBLR3_RESET`, `SBLR3_RESET_ALL`, `SBLR3_RESET_ROLE`, `SBLR3_RESET_SESSION_AUTH`, `SBLR3_RESET_TIME_ZONE`, `SBLR3_SET_TIME_ZONE` are referenced in `src/sblr/v3_payload_map.generated.cpp` and `src/sblr/v3_payloads.cpp` but do not exist in the enum.
- The spec’s SESSION section lists RESET/SET TIME ZONE opcodes; these are absent from the registry.

[~] Many opcode names used by the parser/emitter diverge from the spec naming conventions (e.g., `*_STMT`, `SBLR3_CASE_WHEN`, `SBLR3_IN_LIST`), and rely on custom mapping logic. This violates the “single consistent opcode namespace” goal even if numeric values match the generated registry.


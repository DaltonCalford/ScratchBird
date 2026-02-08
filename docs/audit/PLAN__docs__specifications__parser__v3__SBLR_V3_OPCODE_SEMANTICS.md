# Implementation Plan: SBLR_V3_OPCODE_SEMANTICS.md

**Spec Path:** `docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md`

**Category:** sblr

## Scope Summary
- Implement this SBLR registry/spec and align with parsers/executors.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md`

## Implementation Steps (Detailed)
- Complete per‑opcode runtime semantics for all opcode families
- Define stack input/output types for each opcode
- Define lock/GC/constraint integration for each opcode
- Define error/SQLSTATE mapping for each opcode
- Add execution examples and edge case notes
- Cross‑link each opcode to payload schema and registry

## Manual Gap Analysis (Missing/Unclear Details)
- Semantics are extensive for DDL/DML but likely incomplete for all opcode families
- Stack input/output types are not specified for many opcodes
- Error mappings are generic; per‑opcode SQLSTATE not defined
- Some entries reference WAL (forbidden in V3)

## Verification
- Opcode registry conformance tests.
- Serializer/deserializer round‑trip tests.

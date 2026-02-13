# Executor: SBLR V3 (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the SBLR V3 executor contract, including input validation and execution
rules. The executor runs SBLR only; it never parses SQL.

## Input Contract

- Input is a V3 bytecode container per `SBLR_V3_BYTECODE_CONTAINER.md`.
- Bytecode must pass `SBLR_V3_VALIDATION_RULES.md`.
- Constant pools and symbol tables are resolved per
  `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`.

## Execution Rules

- Execute opcodes in order with a stack-based VM.
- Enforce opcode semantics per `SBLR_V3_OPCODE_SEMANTICS.md`.
- Enforce lock/GC/constraint rules per `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`.

## Error Rules

- Validation failure -> `SBX-INVALID-BYTECODE`.
- Unsupported opcode -> `SBX-UNSUPPORTED-OPCODE`.
- Runtime constraint violation -> SQLSTATE per opcode semantics.

## Related Specs

- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md`
- `docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md`
- `docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`

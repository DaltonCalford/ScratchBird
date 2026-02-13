# SBLR_V3_VALIDATION_RULES.md - Implementation Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md` (Authoritative, updated 2026-02-08)

Summary:
- The V3 validator performs minimal stream integrity checks and a small subset of expression stack checks.
- The majority of required validation rules (error codes, stack/type stack discipline, ordering rules, canonicalization, literal bounds, symbol/constant references) are not implemented.

Key findings:

## Global Module Validation
[~] VERSION-first and END-last rules are enforced in `validateContainer()`; unknown opcodes are rejected, but extended opcode envelope rules are not enforced. See `src/sblr/v3_validator.cpp`.
[ ] Container validation rules from `SBLR_V3_BYTECODE_CONTAINER.md` are not fully enforced (see prior container report). Validator only checks 8-byte alignment and non-empty bytecode stream.
[ ] Symbol and constant pool canonicalization rules from `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md` are not enforced.

## Verifier Error Codes
[ ] No deterministic `V3E-XXXX` error codes are emitted. Validator returns plain strings (e.g., "unknown opcode", "stack underflow").

## Stack Discipline
[ ] `max_stack_depth` is not validated; module metadata is not used to compute max runtime depth.
[ ] Full per-opcode `stack_in` / `stack_out` validation is not implemented. Only a small subset of schema-based expressions are checked (literals, unary/binary, IN/BETWEEN/LIKE, FUNC/AGG/WINDOW).
[ ] No type stack discipline is implemented (TYPE/LITERAL/CAST rules ignored).

## Operand Type Validation
[ ] No operand type checking is performed for numeric/boolean/comparison/array/range/spatial/textsearch operators.

## Opcode Ordering Rules
[ ] DDL/DML/PSQL/query ordering rules are not enforced (e.g., WITH before SELECT, ORDER BY before LIMIT/OFFSET, MERGE sequencing, PSQL DECLARE ordering).

## Canonicalization Validation
[ ] Canonicalization rules (symbol/constant ordering, OPTION_KV sorting, GRANT/REVOKE privilege sort, ON CONFLICT, INDEX INCLUDE) are not enforced.

## Reference Validation
[ ] `string_id` and `const_id` range checks are not implemented; inline constants are permitted without restriction.

## Literal Validation
[ ] Numeric bounds, time zone offsets, range/array consistency, and TSVECTOR/TSQUERY canonicalization are not validated.

## Error Reporting
[ ] Validator does not report opcode offsets or specific rule identifiers beyond a generic error string.


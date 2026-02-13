# SBLR_V3_OPCODE_SEMANTICS.md - Implementation Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md` (Authoritative, updated 2026-02-08)

Summary:
- The spec defines detailed per-opcode runtime semantics, stack effects, lock ordering, and error categories.
- Current implementation provides only minimal semantics metadata (`getOpcodeSemantics`) and a shallow validator; there is no opcode-level execution model that matches the spec.
- Executor behavior is implemented at higher-level statement interpretation (e.g., `SBLR3_SELECT` schema), not via the opcode-level semantics described here.

Key findings:

## Lock Ordering (Normative)
[ ] Lock ordering is not enforced. `getOpcodeSemantics()` only sets `requires_lock_order` for broad opcode name prefixes and does not implement or validate the normative lock ordering sequence. See `src/sblr/v3_semantics.cpp`.
[ ] No V3 executor path enforces the explicit lock ordering described in the spec (catalog DDL lock -> schema -> object -> table DML -> indexes -> rows -> LOB). No dedicated lock-order coordinator found in V3 executor paths.

## CONTROL
[~] Container validator checks `SBLR3_VERSION` is first and `SBLR3_END` is last, and validates payload sizes for VERSION/EXTENDED, but does not enforce runtime semantics or error categories beyond basic stream integrity (`src/sblr/v3_validator.cpp`).

## DDL / DML / TXN / DCL / SESSION Semantics
[ ] Per-opcode semantics (dependency checks, constraint order, cache invalidation, SQLSTATE error classes, and lock behavior) are not implemented as specified. The V3 executor handles only a subset of opcodes with bespoke logic and does not encode the spec’s per-opcode `stack_in` / `stack_out` model.
[ ] `getOpcodeSemantics()` does not populate stack effects, error classes, or per-opcode semantics beyond a coarse `is_expression` / `is_statement` / `requires_lock_order` flag (`src/sblr/v3_semantics.cpp`).
[ ] `validateContainer()` only performs best-effort expression stack checks for a small set of schema-based expressions; it does not validate DDL/DML/TXN/DCL/SESSION semantics or stack behavior (`src/sblr/v3_validator.cpp`).

## QUERY / EXPR / FUNC / AGG / WINDOW
[ ] The spec’s opcode-level query plan operators (`SBLR3_HASH_JOIN`, `SBLR3_GROUP_BY`, `SBLR3_SELECT_STAR`, etc.) are not executed as separate opcodes in V3. The executor interprets `SBLR3_SELECT` payloads and performs query planning internally (see `src/sblr/executor.cpp`), rather than dispatching the discrete opcode semantics described here.
[ ] Expression semantics are not enforced via opcode-level stack execution. The validator only uses schema-based stack checks for a subset of expression opcodes, and the executor does not implement a general expression VM for all opcodes in this spec.

## TYPE / LITERAL
[ ] Type stack semantics (e.g., `SBLR3_TYPE_*` pushing descriptors) and literal decoding behavior are not implemented in a central opcode VM. Literals are encoded/decoded via schema and ad-hoc payload parsing, not through the type stack semantics defined here.

## INDEX / ARRAY / TEXTSEARCH / SPATIAL / JOB
[ ] No V3 executor dispatch found for `SBLR3_INDEX_*`, `SBLR3_ARRAY_*`, `SBLR3_RANGE_*`, `SBLR3_*TS*`, or `SBLR3_ST_*` opcodes. These semantics are specified but not implemented in the V3 execution path.


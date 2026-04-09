# SBLR Feature to Opcode Matrix

Status: current_authority

## Matrix intent

The feature-to-opcode matrix exists to prevent silent parser drift. Every supported language feature must map to an explicit canonical opcode and payload pattern. Unsupported features must be rejected before execution.

## Required mapping discipline

- simple scalar expressions map to canonical scalar and literal opcode families
- boolean predicates map to predicate and comparison opcode families
- query shape constructs map to canonical statement-structure payloads
- mutation constructs map to canonical write and target-binding payloads
- schema constructs map to canonical transactional DDL payloads only when the engine supports them

## Refusal rules

- A parser must not invent private opcodes for a feature that belongs in the canonical matrix.
- An engine implementation must not special-case a parser family to accept a feature absent from the canonical matrix.
- If a feature exists only as target-state work, the parser must reject it instead of emitting quasi-canonical SBLR.

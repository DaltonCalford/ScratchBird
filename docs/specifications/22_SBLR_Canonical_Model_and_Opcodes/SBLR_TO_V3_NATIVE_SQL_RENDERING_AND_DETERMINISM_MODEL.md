# SBLR_TO_V3_NATIVE_SQL_RENDERING_AND_DETERMINISM_MODEL

## Status

Current code-backed authority with reconstructed target-language clarification.

## Purpose

This document defines the current back-conversion path from canonical SBLR/v3 instructions to deterministic native SQL text.

## Governing rule

The current render target is canonical native/v3 SQL text.

It is not an attempt to recover the original source dialect.

That means:

1. original Firebird, PostgreSQL, MySQL, or other source syntax is not the render target
2. the render target is the canonical ScratchBird native/v3 SQL surface
3. repeated render of the same canonical instruction must be deterministic

## Current renderer inputs and outputs

The current renderer accepts:

1. a root `v3::Instruction`
2. optionally, a name resolver that can replace UUID text with resolved object names

The current renderer emits:

1. SQL text
2. render contract ID
3. canonical opcode symbol
4. result shape classification

## Contract-table model

The render system is driven by an explicit contract table.

Each contract binds:

1. `contract_id`
2. opcode
3. canonical opcode symbol
4. grammar signature
5. result shape
6. optional classifier-key prefix

This prevents render behavior from being inferred ad hoc from opcode numbers alone.

## Current contract coverage

Current code-backed contract families include at least:

1. document-path filter
2. time-series bucket aggregation
3. search DSL evaluation
4. vector ANN query
5. hybrid bridge exchange
6. UDR compile dispatch
7. UDR SQL template validation
8. create/alter/drop user
9. create/alter/drop policy
10. create/alter/drop job
11. connection-rule create/alter/drop
12. token create/alter/revoke/drop
13. quota-profile create/alter/drop
14. measurement-retention alteration
15. generic `ALTER SYSTEM`

## Determinism rule

Current tests require that repeated render of the same decoded canonical instruction produce:

1. identical SQL text
2. identical contract ID
3. identical canonical opcode symbol

Another agent shall not introduce nondeterministic render choices.

## Name-resolution model

UUID-like text may be converted to resolved names through an optional resolver.

Current resolver behavior is:

1. only attempt name resolution when the token looks like UUID text
2. use an object-type hint when provided
3. leave the token unchanged if no resolution succeeds

Current catalog-backed resolution uses catalog object resolution and can return:

1. object name
2. full path

## Failure behavior

If an instruction has no render contract or is malformed for the contract, the renderer shall fail rather than fabricate SQL text.

The current renderer uses placeholders such as `<expr>` only when expression-like payloads are not losslessly representable from the available payload fields.

## Canonical target clarification

The render target is best understood as:

1. SBLR/v3 instruction
2. deterministic native/v3 SQL text

It is not:

1. original-dialect round-trip
2. source-formatter recovery
3. parser-specific pretty-print recreation

## Required implementer interpretation

Another agent implementing or extending back-conversion shall preserve:

1. canonical native/v3 target only
2. contract-table driven rendering
3. deterministic repeated render
4. optional but bounded UUID-to-name resolution
5. fail-closed behavior for unknown or malformed instructions

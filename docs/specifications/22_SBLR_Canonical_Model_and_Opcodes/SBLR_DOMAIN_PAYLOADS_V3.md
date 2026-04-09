# SBLR Domain Payloads v3

Status: current_authority_with_reconstructed_expansion

## Purpose

V3 domain payloads provide canonical type descriptors for values, expressions, coercions, parameter binding, and result-shape validation.

## Required Content

A domain payload must encode the logical information required by current compilation and verification paths, including as applicable:

- canonical type family
- nullability
- fixed or variable length bounds
- precision and scale
- collation or character-set identity when relevant
- collection or compound element-domain linkage when relevant
- coercion or affinity flags only when consumed by current engine logic

## Current Codec and Schema Rules

Current code-backed V3 schema and codec rules already imply:
- payload encoding is schema-driven rather than ad hoc
- field encoding is typed by field definition
- missing fields may fall back only where schema or default-value rules allow it
- strings and byte arrays are length-prefixed with varuint lengths
- instruction payloads are carried as raw encoded bytes behind a fixed instruction header

## Domain-Payload Implications for V3 Reconstruction

The SBLR-to-V3 converter requires domain payloads to preserve:
- enough type identity to rebuild V3 type nodes
- enough nullability and length metadata to rebuild V3 shape constraints
- enough charset, collation, and collection identity to avoid donor-dialect guessing
- enough coercion and affinity data to preserve current V3 semantic decisions

## Required Reconstructed Expansion

Commercial-grade canon additionally requires domain payloads to preserve:
- display-stable domain labels where V3 needs them for diagnostics
- parameter-display identity where parameter names are user-significant
- collection element-domain identity for nested and composite shapes
- deterministic source-order identity when multiple domain-bearing items appear in one statement payload

## Rejection Rules

- unknown domain family tags are fatal verifier errors
- missing required domain attributes for a family are fatal verifier errors
- domain payloads must not encode parser-family private semantics that bypass sections `13`, `14`, or `15`
- any converter path that would need donor-dialect inference because the domain payload is underspecified must fail closed

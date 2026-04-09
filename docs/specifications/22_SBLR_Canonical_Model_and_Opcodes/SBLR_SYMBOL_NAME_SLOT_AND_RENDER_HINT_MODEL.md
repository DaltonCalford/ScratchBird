Status: reconstructed_required

# SBLR Symbol Name Slot and Render Hint Model

## Purpose

This document defines the canonical payload extensions required so SBLR can carry enough user-facing identity to support deterministic SBLR-to-v3 conversion.

## Canonical Rule

SBLR remains the rigid canonical execution representation. Name and render metadata are permitted only as bounded payloads that do not alter execution semantics.

## Canonical Metadata Classes

The following metadata classes are admitted:

- variable-name slots
- alias-name slots
- parameter-name slots
- scope-class markers
- render-hint markers

## Execution-Semantics Rule

These metadata classes are non-semantic for execution. Execution continues to rely on canonical opcodes, ordinals, UUID bindings, and verified payload structure.

## Variable-Name Slots

Variable-name slots shall record:

- stable slot ordinal
- canonical identifier string
- scope class
- optional synthesized-name marker

## Alias and Parameter Slots

Alias and parameter slots shall record:

- stable ordinal
- canonical identifier string
- render visibility class

## Render-Hint Markers

Render hints may describe:

- preferred visible alias
- grouping or projection display identity
- deterministic synthesized-name class
- explicit preservation of a canonical user-facing token

Render hints shall never override canonical object binding or execution order.

## Verification Rules

The verifier shall reject payloads when:

- a slot ordinal is duplicated within the same class
- a render hint references a missing slot
- a slot conflicts with canonical parameter or variable ordering
- metadata attempts to redefine execution meaning

## Conversion Rule

Section 28 may use these slots and hints to produce v3 render shapes. No parser is allowed to depend on any other parser’s private sidecar format for this purpose.

## Non-Guarantees

This file does not require storing every lexical token from the original dialect. It admits only the bounded canonical metadata needed for deterministic v3 conversion and user-facing naming recovery.

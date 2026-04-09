Status: reconstructed_required_with_current_substrate

# SBLR to V3 Variable Name and Context Recovery Model

## Purpose

This document defines the one-way conversion from rigid canonical SBLR into the context-aware v3 parser render model, with emphasis on variable names, aliases, and other user-facing recovery hints.

## Canonical Rule

The SBLR-to-v3 converter is one-way only. It converts canonical SBLR into v3 render structures. It does not restore the original source dialect, original spelling, or original parser-specific formatting.

## Input Rule

The converter accepts only canonical SBLR plus admitted render-hint payloads. It does not depend on parser-private ASTs, parser-private caches, or other parser implementations.

## Variable Name Recovery

When canonical SBLR includes explicit variable-name or slot-name metadata, the converter shall:

- preserve the canonical variable name
- preserve stable ordinal position
- preserve scope class
- render v3 variables using the canonical name where legal

When explicit variable-name metadata is absent, the converter shall:

- synthesize deterministic v3-local names
- keep ordinals stable within the render result
- mark the render as synthesized-name output

## Context Recovery Inputs

The converter may use only:

- canonical symbol slots
- canonical render hints
- canonical alias hints
- canonical parameter ordinals
- canonical object UUID bindings and resolved names where admitted by the rendering contract

## Context Recovery Prohibitions

The converter shall not:

- infer the original SQL dialect
- reintroduce parser-specific sugar that is not represented canonically
- depend on another parser for reconstruction
- fabricate user-defined names with no canonical basis

## v3 Output Requirements

The converter shall produce a v3 render shape that includes:

- variable identifiers
- parameter identifiers
- visible aliases
- render-hint provenance
- synthesized-name markers where applicable

## Name Stability Rule

For the same canonical SBLR payload and the same render-hint payload, the converter shall produce the same v3 variable and alias names deterministically.

## Failure Rules

Conversion shall fail closed when:

- required symbol slots are inconsistent
- render hints conflict with canonical ordinals
- a name collision cannot be resolved deterministically

## Non-Guarantees

This file does not require full recreation of the original SQL text. It guarantees only deterministic conversion from canonical SBLR into the v3 context-aware representation.

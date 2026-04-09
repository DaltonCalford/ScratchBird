Status: reconstructed_required

# SBLR Canonical Name Metadata Scope and Erasure Boundary

## Purpose

This document defines what user-facing naming information may survive canonical lowering into SBLR and what information is intentionally erased.

## Canonical Rule

SBLR preserves only bounded canonical naming metadata needed for deterministic downstream rendering and diagnostics. Dialect-private or parser-private naming context beyond that boundary is erased.

## Preserved Metadata

The canonical model may preserve:

- parameter names
- variable names
- alias hints
- scope markers
- deterministic render hints

## Erased Metadata

The canonical model shall erase:

- dialect-specific lexical form
- parser-private formatting
- dialect-specific quoting as semantic input
- parser-private symbol tables not admitted into canonical slots

## Execution Rule

Preserved naming metadata does not affect execution semantics, object binding, visibility, locking, or durability. It exists only for rendering, diagnostics, and user-facing reconstruction within the admitted scope.

## Verification Rule

The verifier shall reject metadata that tries to cross the erasure boundary by reintroducing parser-private semantics as canonical execution meaning.

## Non-Guarantees

This file does not require preserving every original variable spelling from every dialect. It defines the bounded canonical naming surface only.

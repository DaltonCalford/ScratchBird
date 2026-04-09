Status: reconstructed_required

# SBLR to V3 Dialect Origin Erasure and Render Scope Model

## Purpose

This document defines the scope and limits of the SBLR-to-v3 converter with respect to original dialect identity.

## Canonical Rule

The SBLR-to-v3 converter does not preserve original dialect identity as a semantic input. Once SQL has been lowered into canonical SBLR, the converter targets only the v3 render model.

## Origin-Erasure Rule

The converter shall treat the source dialect as erased unless explicit canonical metadata is preserved for user-facing naming or render-hint purposes. That metadata does not restore dialect semantics.

## v3-Only Render Scope

The converter shall produce:

- v3 identifiers
- v3-visible aliases
- v3 parameter and variable naming
- v3-compatible render structure

The converter shall not produce:

- original dialect text
- original dialect-specific quoting rules
- original dialect-only syntactic sugar

## Determinism Rule

For a given canonical SBLR payload and admitted render-hint payload, the converter shall always produce the same v3 render result regardless of the original parser that created the SBLR.

## Parser Isolation Rule

No parser may depend on another parser’s AST, token stream, or private lowering sidecar to enable SBLR-to-v3 conversion.

## Non-Guarantees

This file does not guarantee human-identical pretty-printing of the originating source text. It guarantees deterministic v3 rendering from canonical SBLR.

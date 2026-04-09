Status: reconstructed_required

# Parser Independence Library Packaging and No Shared Lowering Model

## Purpose

This document defines the canonical parser packaging rule that every parser is an optional independent library with no dependency on other parsers and no shared lowering implementation.

## Canonical Rule

Each parser library shall be buildable, packageable, loadable, and removable independently. No parser may depend on another parser for SQL parsing, SQL-to-SBLR lowering, render reconstruction, or dialect-specific result shaping.

## Independence Requirements

Every parser library shall own:

- its grammar and tokenization
- its dialect-local lowering to canonical SBLR
- its dialect-local diagnostics
- its dialect-local response shaping

Every parser library shall not depend on:

- another parser library
- a shared cross-parser lowering library
- another parser’s AST or token model
- another parser’s private normalization helpers

## Shared Canonical Boundary

The only shared boundary across parsers is canonical engine-facing substrate such as:

- SBLR definitions
- UUID-bound object references
- engine capability contracts
- IPC and result framing contracts admitted by canon

## Packaging Rule

Parser packaging may be:

- statically linked
- dynamically linked
- process-isolated as part of a parser-agent executable

The packaging choice does not weaken the no-shared-lowering rule.

## Optionality Rule

A deployment may include:

- no parser libraries at all
- exactly one parser library
- multiple parser libraries

No parser’s presence shall be a prerequisite for another parser’s correct operation.

## Non-Guarantees

This file does not require identical feature coverage across all parsers. It requires independent ownership and no cross-parser dependency.

Status: reconstructed_required

# Parser Library IPC Library and Response Shaping Model

## Purpose

This document defines the canonical role of parser libraries in embedded and server-stack deployments.

## Canonical Rule

Each parser is a library. It performs dialect-local SQL handling, lowers to SBLR, and shapes the response expected by that dialect or client surface. Parsers are optional and independent.

## Parser Library Responsibilities

Each parser library owns:

- dialect-local SQL parsing
- dialect-local SQL to SBLR lowering
- dialect-local response shaping
- parser-local validation and diagnostics

Each parser library does not own:

- engine storage truth
- transaction truth
- MGA visibility semantics
- cross-parser reuse or dependency

## IPC Library Rule

When the parser does not embed the engine directly, it may use the IPC library to communicate with a local threaded IPC server. This permits local shared-database use without any IP networking requirement.

## Embedded Rule

For an embedded local-only deployment:

- a parser library may call the engine library directly
- or a parser library may call the IPC library to a local shared server

Both are valid. The parser library remains optional in both cases.

## Response-Shaping Rule

Response shaping is parser-owned. The engine exposes canonical execution results; the parser library maps those results into the client-facing form required by its dialect contract.

## Independence Rule

No parser may depend on another parser for:

- SQL parsing
- SBLR lowering
- response shaping
- dialect-specific metadata interpretation

## Non-Guarantees

This file does not require all parsers to expose identical client contracts. It requires them to remain independent optional libraries.

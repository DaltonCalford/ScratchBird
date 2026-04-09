# Parser Isolation and Dialect-Local SBLR Lowering Rule

Status: reconstructed_required

## Purpose

This file defines the non-negotiable parser isolation rule for ScratchBird: every parser is responsible for its own source-language to SBLR conversion, parsers are optional, and parsers must not depend on one another.

## Core rule

All parsers for all dialects perform their own language-to-SBLR conversion.
No parser may delegate SQL or query normalization to another parser.
No parser may require another parser to be present in order to build, load, negotiate capability, or lower statements into SBLR.

## Non-negotiable parser independence rules

1. Each parser is an optional component.
2. Any parser may be omitted from a build or deployment without breaking the correctness of any other parser.
3. No parser may call into another parser for:
- lexing
- parsing
- AST normalization
- name binding
- SQL-to-SBLR lowering
- dialect fallback
- error recovery
- donor-dialect compatibility handling
4. No parser may route its input through a different parser as an intermediate normalization step.
5. No parser may treat the V3 parser as a universal shared front door for other dialects.

## SBLR lowering ownership rule

The ownership rule is:
- input dialect parser owns dialect parsing and dialect-local lowering logic
- engine owns SBLR execution and internal procedures only
- section `22` owns the canonical SBLR container, schema, and verifier
- section `28` owns per-parser lowering rules and parser isolation

That means:
- PostgreSQL parser lowers PostgreSQL directly to SBLR
- MySQL parser lowers MySQL directly to SBLR
- Firebird parser lowers Firebird directly to SBLR
- every other parser lowers its own language directly to SBLR

## Allowed shared infrastructure

The no-cross-parser rule does not forbid shared parser-agnostic infrastructure.
Allowed shared surfaces are limited to parser-neutral components such as:
- SBLR container, opcode, schema, and verifier libraries
- UUID and catalog helper APIs
- parser-agent IPC contracts
- common error and status envelopes
- parser test harness infrastructure
- parser-local utility code packaged inside the same parser component

Shared infrastructure must remain parser-neutral.
It must not become a disguised cross-parser lowering layer.

## Forbidden shared infrastructure

The following are non-conforming:
- a common SQL AST used as the mandatory front door for multiple dialect parsers
- one parser lowering into an intermediate AST owned by another parser
- a shared cross-dialect lowerer that performs the semantic lowering step for multiple parsers
- a parser importing dialect-specific grammar, token, or normalization rules from another parser
- a parser that becomes mandatory because another parser depends on its code path

## Relationship to the SBLR-to-V3 converter

The one-way SBLR-to-V3 converter is an output-side reconstruction surface only.
It does not authorize any input parser to use the V3 parser as a shared normalization dependency.

Allowed:
- canonical SBLR to V3 reconstruction for diagnostics, tooling, or export

Forbidden:
- donor dialect to V3 parser to SBLR as the normal parser architecture
- donor dialect parser depending on V3 parser internals to complete lowering

## Packaging and deployment rule

Every parser package must be independently:
- buildable
- loadable
- discoverable
- negotiable
- rejectable

The absence of one parser package must not change the runtime correctness of another parser package beyond removing that omitted dialect surface.

## Fail-closed rule

If a parser would need another parser in order to lower a statement class, that parser surface is non-conforming and must fail closed rather than silently using a cross-parser dependency.

## Direct audit lookup anchors

- `src/parser/parser_v3.cpp` search key `ParseResult Parser::parseStatement()`
- `src/parser/v3_emitter.cpp` search key `V3Emitter::emitStatementToContainer(`
- `src/sblr/ast_sblr_lowerer.cpp` search key `AstSblrLowerer::emitStatementToContainer(`

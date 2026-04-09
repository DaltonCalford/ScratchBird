# SBLR to V3 Render Contract Name Recovery and Result Shape Model

Status: reconstructed_required_with_current_substrate

## Purpose

This file defines the current code-backed contract for rendering canonical SBLR
back into V3 SQL text and classifying the rendered result shape.

It exists to keep a limited implementer from confusing:

- parser-local SQL to SBLR lowering
- rigid SBLR storage
- V3-only context-aware reverse rendering

## Governing rules

1. every parser lowers its own dialect to SBLR independently
2. no parser-to-parser sharing is allowed
3. reverse conversion targets V3 only
4. reverse conversion does not recreate the original input dialect

## Current code-backed authority

The current render contract is grounded in:

- `src/sblr/native_sql_renderer.cpp`
- `scratchbird/sblr/native_sql_render_contract.h`
- `src/sblr/v3_codec.cpp`
- `src/sblr/v3_payloads.cpp`

## Render contract table

The current renderer exposes a contract table keyed by opcode.

Each contract row carries:

- contract id
- opcode
- canonical opcode symbol
- grammar signature
- result shape
- classifier-key prefix

This means rendering is not a free-form pretty-printer.
It is a contract-driven opcode-to-V3 rendering system.

## Result-shape contract

Current result-shape classes are:

- `COMMAND_STATUS`
- `ROWSET_OR_MUTATION`
- `STREAM_STATUS`

Rendered SQL and downstream handling must retain the distinction between these
shapes.

## Name-recovery contract

The current renderer performs name recovery through a resolver boundary rather
than assuming SBLR already stores every human-readable name in final form.

Current behavior includes:

- UUID-like token detection
- optional resolver-backed UUID-to-name recovery
- object-type hints during name recovery
- schema-path rendering from list payloads

This is current proof of context-aware rendering rather than literal opcode
dumping.

## Literal and expression recovery model

The current renderer explicitly handles:

- SQL string quoting
- boolean rendering
- numeric rendering
- null rendering
- schema-path rendering
- list rendering for selected payload families
- literal extraction from instruction-pointer payloads

The codec and payload layers also prove:

- little-endian payload encoding
- varuint lengths
- typed literal payload families
- schema lookup by opcode family

This means the reverse path is constrained by canonical payload schema, not by
ad hoc textual reconstruction.

## V3-only recovery boundary

The renderer is V3-only because:

- the current contract table is for native SQL rendering
- the current name recovery path is oriented around V3-facing canonical SQL
- the recovered grammar signatures belong to the native render contract

It is non-conforming to treat this path as a generic original-dialect
round-tripper.

## Reconstructed required payload expansion

Current code already proves contract-driven rendering and catalog-backed name
recovery.

The rebuilt spec still requires richer retained context in SBLR for stronger V3
recovery, including:

- variable names
- stable symbol identity
- additional contextual naming elements

That requirement is reconstructed-required behavior with current converter
substrate, not yet proof that all such context is already fully preserved in the
current payloads.

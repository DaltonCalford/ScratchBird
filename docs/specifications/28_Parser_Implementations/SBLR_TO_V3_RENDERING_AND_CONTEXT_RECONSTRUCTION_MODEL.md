Status: reconstructed_required_with_current_substrate

# SBLR to V3 Rendering and Context Reconstruction Model

## Purpose

This file defines the end-to-end reverse-conversion path from canonical SBLR to
V3 source text.

## Governing rules

1. Every SQL dialect parser lowers to SBLR independently.
2. No parser-to-parser sharing is allowed.
3. Reverse conversion targets V3 only.
4. Reverse conversion does not attempt to recreate the original input dialect.

## Current code-backed baseline

The current converter path proves:

1. SBLR can be decoded and canonicalized for V3-facing rendering
2. object names can be recovered through catalog-backed UUID resolution
3. the renderer is context-aware rather than a direct opcode pretty-printer
4. current native-V3 lowerers emit a versioned retained-symbol carrier alongside
   inline compatibility names

## Required reconstruction algorithm

The reverse-conversion algorithm shall be:

1. load the SBLR container
2. verify the container and payload version
3. resolve persistent object UUIDs using:
   - `sb_catalog_resolve_uuid_to_path_name`
   - committed catalog snapshot or current session-local overlay where applicable
4. load retained render-recovery payloads for:
   - local variables
   - parameters
   - labels
   - aliases
   - cursor names
5. canonicalize expression and statement structure to V3 grammar
6. render deterministic V3 text

## Name recovery order

The renderer shall recover names in this order:

1. retained local-symbol payload
2. committed or session-visible catalog resolution for persistent objects
3. deterministic synthesized V3-safe fallback names

The renderer shall not depend on any other parser for missing names.

## Session-overlay rule

When reverse conversion occurs inside an active transaction with unpublished
local DDL overlay:

1. UUID resolution shall first consult the session-local overlay
2. then consult the committed catalog baseline
3. then apply deterministic fallback only if neither produces a renderable name

This keeps reverse rendering aligned with the always-in-transaction MGA model.

## Deterministic V3 output rule

Rendered V3 output shall be deterministic for the same:

- SBLR container
- catalog visibility state
- retained symbol payload
- rendering policy version

## Fail-closed rules

Reverse conversion shall fail or degrade deterministically when:

1. the SBLR payload version is incompatible
2. a required UUID cannot be resolved and no bounded fallback is allowed
3. required render-recovery payload is absent for a construct that cannot be rendered safely
4. catalog visibility is ambiguous because the caller did not supply the correct transaction or overlay context

## Reconstructed required expansion

The rebuild requires:

1. full symbol-retention support in SBLR for V3 reverse rendering
2. stable synthesized-name rules for degraded legacy payloads
3. deterministic row or message contracts exposing when rendering used:
   - exact retained names
   - catalog-resolved names
   - synthesized names

Package `03` closes the retained-symbol carrier requirement for this
reverse-render lane. The current inline-retention substrate may still be
consumed as a compatibility path for degraded payloads and currently shipped
native-V3 statement families, but inline-only payloads are not sufficient Beta
1 completion.

## Non-authority boundaries

The following are not part of this model:

1. reverse conversion to Firebird, PostgreSQL, MySQL, or any other source dialect
2. shared parser libraries that reconstruct text for multiple dialect front ends
3. using parser-specific AST helpers as the truth after SBLR has been produced

## Direct audit lookup anchors

- `src/sblr/native_sql_renderer.cpp` search key `renderNativeSqlInstruction(`
- `src/sblr/language_udr_sql_render_endpoint.cpp` search key `renderNativeSqlInstruction(`
- `src/sblr/native_sql_render_contract.cpp` search key `nativeSqlRenderContractForInstruction(`

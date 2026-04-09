# SBLR to V3 Converter and Name Recovery Rules

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines:
- what current code already proves about rendering canonical `SBLR` back into V3-facing text
- what the recovered commercial-grade converter must do beyond the current renderer
- how retained symbols, UUID-based object identity, and session-local overlays interact
- security and DCL symbol recovery rules
- how parser isolation constrains the converter architecture

## Non-negotiable scope rule

The converter supports conversion from `SBLR` to V3 only.

It does not attempt to recover:
- the original donor dialect
- exact donor formatting
- donor-specific quoting preferences
- donor-specific alias wording when that wording was never preserved in `SBLR`

It also does not depend on:

- any donor parser package
- any cross-parser helper service
- any parser-private lowering metadata not already normalized into canonical
  `SBLR`

## Current code-backed baseline

Current code-backed authority proves:
- V3 AST to `SBLR` lowering through `AstSblrLowerer` and `V3Emitter`
- verified `SBLR` container and payload decoding
- native SQL rendering for supported opcode and payload families
- resolver-assisted UUID-to-name recovery on render

Current code does not yet prove a universal full-AST reifier from arbitrary canonical `SBLR` back into a complete V3 AST for every statement family.

The rebuild therefore treats the converter as:

- current code-backed for deterministic V3-facing rendering on supported payloads
- required reconstructed for full symbol-complete one-way V3 recovery

## Required converter inputs

The recovered converter must consume:
- verified `SBLR` container
- statement payloads and domain payloads
- retained symbol payloads from section `22`
- committed catalog resolver results for UUID-backed durable objects
- current-transaction local overlay for uncommitted same-session DDL and naming changes

## Required converter algorithm

1. verify the `SBLR` container and schema-bound payloads
2. decode the statement opcode family and statement-local payload tree
3. load normalized retained-symbol data when present
4. fall back to inline retained payload names when current authority allows that family
5. reconstruct the canonical V3 AST shape using statement opcode semantics and retained symbol/context payloads
6. resolve durable UUID-backed names through local overlay first, then committed catalog resolver
7. apply V3-only qualification and formatting rules
8. emit either canonical V3 AST or canonical V3 SQL text
9. refuse conversion when a user-significant symbol would otherwise be guessed

The converter must preserve an explicit distinction between:

- durable object identity resolved by UUID and catalog
- local or parser-surface symbol identity resolved only from retained payload
- session-local unpublished names resolved only from current-transaction overlay

## Security and DCL symbol rules

The converter must treat these names as user-significant and non-guessable:
- user names
- role names
- group names
- grantee names
- policy names
- security-object names
- object paths in grant or revoke statements
- session authorization target names
- active role names

Generated fallback names are not allowed for these classes.

## Name recovery order

The converter must prefer names in this order:
1. normalized retained symbol for the exact scope and symbol class
2. inline retained payload name already present in `SBLR`
3. same-session local overlay for uncommitted local `DDL`
4. committed catalog resolver name for UUID-backed durable objects
5. deterministic non-durable fallback only when the symbol class explicitly allows it

## Stable fallback rules

Generated fallback names are allowed only for:
- anonymous parameters
- unnamed derived outputs
- unnamed procedural temporaries whose textual name is not user-significant

Generated fallback names are also allowed for engine-generated anonymous helper
symbols created after canonical lowering, provided they were never user-authored.

Generated fallback names are not allowed for:
- relation aliases that affect visible SQL text
- CTE names
- cursor names
- block labels
- user-visible output labels
- durable object names
- security and DCL principal names

The converter must treat user-authored local variables as user-significant
symbols unless the canonical lowering contract marks them engine-generated.

## Parser-isolation architecture rule

The converter is a canonical consumer of:

- verified `SBLR`
- section `22` retained-symbol payloads
- catalog helper functions
- same-session overlay resolution

It is not a meta-parser and shall not:

1. invoke a donor parser to recover text
2. borrow another dialect parser's symbol tables
3. depend on parser-to-parser lowering compatibility
4. reconstruct a parser-specific AST only to render V3 indirectly

Its output target is canonical V3 and only canonical V3.

## Canonical outcome rule

When conversion succeeds, the result is canonical V3 output.
It is not a promise of recovering the exact original source text typed by the user.

When conversion fails, it must fail because canonical payload or canonical
resolver evidence was insufficient, not because a parser dependency was missing.

## Direct audit lookup anchors

- `src/sblr/native_sql_renderer.cpp` search key `renderNativeSqlInstruction(`
- `src/sblr/native_sql_render_contract.cpp` search key `nativeSqlRenderContractForInstruction(`
- `src/parser/v3_emitter.cpp` search key `select_aliases`

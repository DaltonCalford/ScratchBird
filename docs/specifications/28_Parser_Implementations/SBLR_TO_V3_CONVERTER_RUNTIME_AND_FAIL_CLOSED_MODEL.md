# SBLR to V3 Converter Runtime and Fail-Closed Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define the runtime algorithm for converting rigid SBLR payloads into context-aware V3 structures.

## Scope

- conversion target is V3 only
- no conversion back to donor dialect SQL
- no cross-parser lowering reuse
- no parser-to-parser dependency creation

## Core Invariants

1. All parsers lower their own SQL to SBLR; no parser may depend on another parser’s lowering.
2. The converter target is V3 only.
3. The converter may use retained payloads and catalog name recovery for durable UUID-backed objects.
4. The converter must fail closed rather than invent donor-dialect text or parser-specific semantics.
5. User-authored local names must come from retained payload, not from synthesized fallback.

## Runtime Algorithm

1. validate container and instruction stream
2. decode instructions with the V3 codec and schema registry
3. apply canonical payload interpretation rules
4. reconstruct V3 nodes from opcode family and payload schema
5. recover names in this precedence order:
   - normalized retained-symbol layer
   - inline retained payload names
   - same-session local overlay for unpublished names
   - catalog or resolver-derived durable object names
   - deterministic fallback for engine-generated unnamed non-durable symbols only
6. reject any node that still requires guessing after those steps
7. emit V3 AST or V3 statement structures only

## Symbol-class handling matrix

The converter must distinguish at least these symbol classes:

1. durable catalog objects
2. user-authored local variables and parameters
3. user-authored aliases and CTE names
4. security-principal names
5. transaction-local control names such as savepoints
6. engine-generated anonymous helper symbols

Only class `6` may use deterministic synthesized fallback names.

Classes `2` through `5` must fail closed if the retained recovery payload is
missing.

## Catalog Name Recovery Rule

Catalog name recovery is allowed only for durable UUID-backed objects.

Current code-backed resolver behavior already proves:
- UUID text must parse exactly
- object-type hint must match actual catalog object type
- resolver may return object name or full path
- hint mismatch must fail closed

This means:
- durable object names may be recovered from the catalog
- aliases, local variables, cursor names, block labels, and other scope-local symbols may not be recovered from the catalog

## Native Rendering Boundary

Current code also proves a native SQL rendering path that can resolve UUID-like tokens through the catalog resolver.

That renderer is:
- useful as one current proof surface for deterministic name recovery
- not the canonical target of this converter

The converter target remains V3, not donor dialect text.

## Required Retained Structures

The converter requires:
- statement payload names already emitted inline
- retained symbol and scope payloads from section `22`
- domain payloads sufficient to rebuild V3 type nodes
- source-order retention where node ordering is user-significant

## Fail-Closed Conditions

Conversion must fail closed when:
- required alias or label is missing
- a user-authored variable or parameter name is missing
- a security-principal display name is missing
- a scope-local symbol is ambiguous
- a UUID resolves to the wrong object class for the statement context
- the payload is under-specified for V3 reconstruction
- reconstruction would require donor-dialect syntax guessing

It must also fail closed when:

- a parser-specific dependency would be required to continue
- a symbol exists only in another parser's private lowering metadata
- a reverse conversion path would need to infer the original donor dialect

## Non-Goals

- no round-trip back to original dialect text
- no cross-parser shared lowering
- no parser-private semantics injected into the converter
- no widening from V3 output into a generic SQL pretty-printer contract

## Current-versus-required boundary

Current code-backed authority proves:

- deterministic V3-facing rendering exists
- UUID-backed durable-name recovery exists
- the path is context-aware

Required reconstructed behavior extends this to:

- full retained-symbol completeness for user-authored local names
- explicit same-session overlay precedence
- no-synthesis rule for user-authored names
- strict parser-isolation at converter runtime

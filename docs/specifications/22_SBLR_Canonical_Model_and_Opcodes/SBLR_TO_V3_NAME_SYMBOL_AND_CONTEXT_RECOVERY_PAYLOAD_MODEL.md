Status: reconstructed_required_with_current_substrate

# SBLR to V3 Name, Symbol, and Context Recovery Payload Model

## Purpose

This file defines the payload expansion required for rendering rigid canonical
SBLR back into context-aware V3 source text.

## Governing rule

SBLR is the canonical executable representation.

The reverse renderer supports output to V3 only.

The reverse renderer does not preserve or reproduce the original input dialect.

## Current code-backed baseline

The current converter path proves the following:

1. there is a real codec and canonicalization path for V3-facing rendering
2. native SQL name resolution already uses catalog-backed UUID-to-name recovery
3. V3 rendering is context-aware and not a byte-for-byte mirror of rigid SBLR layout
4. current reverse conversion depends on a combination of:
   - SBLR payload information
   - catalog name resolution
   - V3 canonicalization rules

## Reconstructed required payload expansion

To render stable and readable V3 text without guessing, SBLR shall retain a
render-recovery payload for name- and scope-sensitive constructs.

The minimum retained payload set is:

1. variable display name
2. parameter display name
3. local declaration ordinal
4. scope identifier
5. block or routine-local symbol owner
6. cursor name
7. loop label
8. exception label or named condition token
9. package member display name when applicable
10. trigger or routine argument display names
11. alias-display preference for table and derived-column aliases
12. explicit correlation-name display token when the user supplied one
13. security-principal display name for user, role, and group references
14. grant and revoke subject display names
15. savepoint or transaction-local label names
16. CTE display names

Package `03` treats this retained payload expansion as required Beta 1 work for
the shared SBLR substrate, not as a later optional enhancement limited to the
renderer.

## UUID and name division of labor

The recovered model is:

1. UUIDs remain the authoritative object identity
2. catalog helpers recover stable schema path and object names
3. retained display-name payload recovers user-facing local symbol names that the catalog cannot know

Catalog resolution alone is insufficient for:

- local variable names
- cursor names
- block labels
- argument display names
- local alias preference

## Render-recovery record model

Each render-sensitive local symbol shall carry at least:

- symbol kind
- owning statement or block identifier
- ordinal within the owner
- display name
- symbol origin class
- user-supplied or engine-synthesized flag
- flags for quoted or unquoted display preference

`symbol origin class` must distinguish at minimum:

- durable catalog object
- local user-authored symbol
- engine-generated anonymous symbol
- security-principal symbol
- transaction-local control symbol

## Versioning rule

Payload expansion for V3 recovery shall be versioned.

Older SBLR payloads that lack full recovery fields may still execute, but
reverse rendering shall treat them as degraded input and shall not invent
unstable names.

## Deterministic degraded-mode rule

When a required V3 recovery name is absent:

1. prefer catalog-resolved names for persistent objects
2. use retained payload names for local or security-visible symbols
3. use same-session local overlay for unpublished local DDL names when present
4. emit deterministic synthesized V3-safe identifiers only for engine-generated
   anonymous symbols whose user-facing name was never authored
5. otherwise fail closed

The renderer shall not invent nondeterministic, parser-specific, or donor-
dialect-specific names.

User-authored local names are not eligible for synthesis.

## Non-authority boundaries

The following are not permitted:

1. round-tripping back to the original input dialect
2. parser-to-parser reverse conversion
3. using one dialect parser to reconstruct another dialect's surface text
4. losing UUID truth in favor of text names
5. synthesizing user-authored local names because the retained symbol payload was
   not emitted

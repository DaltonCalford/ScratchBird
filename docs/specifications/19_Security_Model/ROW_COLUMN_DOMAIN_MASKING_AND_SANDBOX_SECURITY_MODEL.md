# Row, Column, Domain, Masking, and Sandbox Security Model

Status: reconstructed_required_with_current_substrate

## Purpose

This file defines the canonical fine-grained ScratchBird security stack for:
- row-level security
- column-level security
- domain-level security
- masking and unmask privilege
- security-definer and emulated-schema sandbox boundaries
- shared user, role, and group authorization inputs

## Canonical layer order

All read and write authorization is evaluated in this order:
1. attachment and session identity establishment
2. user, role, group, and shared-rights expansion
3. object-level permission checks
4. row-level security policy filtering
5. column-level permission filtering
6. domain-level permission and masking checks
7. executable-object sandbox checks for views, procedures, functions, packages, and emulated surfaces
8. audit signaling for allow, deny, mask, and privilege-bypass events where policy requires it

No lower layer may weaken a higher layer.

## Authorization objects

The canonical authorization inputs are:
- users
- roles
- groups
- shared-rights bundles or synchronized external-identity mappings when configured
- object grants
- column grants
- row-level policies
- domain security payloads
- executable-object security contexts

## Row-level security

Row-level security is part of the same always-in-transaction MGA visibility world as ordinary data access.

Rules:
- row filtering applies to `SELECT`, `UPDATE`, and `DELETE` candidate sets
- write paths must support `WITH CHECK`-style validation so a write may not create a row state the caller is not authorized to create
- RLS enable, disable, force, and no-force are transactional DDL operations
- non-forced RLS may allow owner or superuser bypass where current runtime does so
- forced RLS removes that bypass
- policy publication follows ordinary commit publication rules

## Column-level security

Column-level authorization is independent from table-level authorization.

The canonical privilege families for column security are:
- `SELECT`
- `UPDATE`
- `DELETE` where column participation is part of a protected write contract
- `REFERENCES`
- `VISIBLE`
- `EXECUTE` where a computed or protected expression surface is exposed through the column boundary

Current code-backed proof is strongest for column grant, revoke, permission check, accessible-column discovery, cache invalidation, and `SELECT`-path enforcement.

Reconstructed canon requires all admitted column privilege families to remain subordinate to:
- object permission checks
- row-level policy filtering
- domain masking and unmask privilege rules

## Domain-level security

Domains may carry persisted security state for:
- masking configuration
- required privilege for unmasked disclosure
- permission-mask authorization
- encryption-at-rest intent
- audit enablement

Domain security is additive. It does not replace row, column, or object authorization.

## Masking

Masking may be attached at:
- domain level
- column level through domain or column policy mapping
- higher policy layers that resolve into canonical domain masking behavior

Masking bypass is a separate authorization decision from ordinary read permission.

## Current masking algorithm recovered from code

The current masking runtime is not a generic placeholder. It already enforces these rules:

- if `has_privilege` is true, return the original value unchanged
- if masking type is `NONE`, return the original value unchanged
- `FULL` masking:
  - requires a non-empty mask character
  - counts UTF-8 characters when the input is valid UTF-8
  - falls back to byte-count masking when the input is not valid UTF-8
- `PARTIAL` masking:
  - requires a non-empty pattern
  - requires a non-empty mask character
  - token `#` preserves the next source character when available
  - token `X` consumes the next source character and emits the mask token
  - literal pattern characters are copied into the result
  - remaining source characters beyond the pattern are masked

Current code-backed tests prove:
- no-mask passthrough
- full masking
- partial masking with a social-security-style pattern
- privilege bypass
- UTF-8-aware full masking
- empty-pattern rejection

These token and UTF-8 rules are now canonical and must not be reinterpreted by parsers or renderers.

## Sandbox model

A view, procedure, function, package, or emulated-engine surface may execute with a controlled effective security context while the caller lacks direct rights on underlying objects.

Rules:
- direct base-object access outside the executable boundary remains denied
- effective access may be provided only through the executable boundary
- audit must preserve both caller identity and effective security context
- security-barrier options may constrain optimizer behavior, not only privilege substitution
- emulated-engine schema roots are security and name-resolution boundaries, not cosmetic aliases

## Current-versus-required sandbox split

Current code-backed authority is strongest for:
- persisted masking behavior
- security-definer metadata
- security-barrier metadata
- security-context stack handling

Current code also proves a stronger fail-closed boundary than earlier package
audits had recorded:
- view-security stack handling is code-backed
- security-definer table and column access deny rather than auto-allow when no
  integrated permission backend is present
- `WITH CHECK OPTION` validation denies rather than silently succeeding when no
  predicate-evaluation backend is present

Canon rule:
- the stricter sandbox and policy model in this file is authoritative
- fail-closed denial is acceptable until a stronger integrated enforcement path
  exists
- permissive success in the absence of that enforcement path remains
  implementation drift and is not permitted

## Shared-rights consequence

External identity, shared cluster rights, and synchronized group information are identity inputs only. They become effective rights only after canonical ScratchBird authorization mapping.

## Current-versus-required split

Current code-backed authority proves at least:
- column permission CRUD and lookup surfaces
- domain masking payload persistence and privilege evaluation
- security-definer and security-barrier view options
- table RLS enable and force state
- policy catalog surfaces and executor-side RLS helpers
- users, roles, groups, and role-membership catalog surfaces

Required reconstructed canon extends that into a full commercial-grade model for:
- `VISIBLE` and domain-inherited disclosure rules
- shared-rights propagation across managed deployments
- emulated-engine sandbox equivalence across supported dialects
- cluster-aware security object synchronization

## Non-guarantees

This file does not claim every parser or emulation already exposes every fine-grained security statement in SQL text form.
It defines the canonical security model that implementation must meet.

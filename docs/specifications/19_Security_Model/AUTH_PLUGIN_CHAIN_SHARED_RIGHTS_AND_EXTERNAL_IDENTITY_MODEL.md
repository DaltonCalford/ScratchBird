Status: reconstructed_required_with_current_substrate

# Auth Plugin Chain Shared Rights and External Identity Model

## Purpose

This document defines how ScratchBird authentication plugins, external identity mapping, shared rights, and security-definer surfaces interact.

## Canonical Separation

ScratchBird shall separate:

- authentication
- identity mapping
- role and group expansion
- rights evaluation
- security-definer or sandboxed execution

Success in one stage does not imply success in the next.

## Authentication Plugin Chain

The canonical authentication chain is:

1. transport or session identity intake
2. plugin selection under policy
3. plugin authentication result
4. external identity normalization
5. local principal binding
6. role, group, and shared-right expansion
7. session capability publication

Each stage shall produce a deterministic success or refusal result.

## Optional Plugin Rule

Authentication plugins are optional modules. No plugin may require another specific plugin to exist. Policy may allow ordered fallback, but inter-plugin code dependency is non-conforming.

## External Identity Mapping

External identities may map to local users, roles, or groups only through explicit policy and catalog-backed mapping rules. Name similarity, transport metadata, or client assertions alone shall never grant rights.

## Shared Rights Model

Shared rights shall be evaluated as the union of explicitly granted rights from:

- the bound local principal
- attached roles
- attached groups
- shared-right bundles or mapped external identities

Rights inherited through a shared-right surface shall remain traceable to their grant source for audit and revoke semantics.

## Security-Definer and Sandbox Rule

A user may have rights to execute a view, procedure, function, or package without having direct rights to every underlying object that surface uses. This is allowed only when:

- the surface is explicitly admitted as security-definer or sandboxed
- the object owner or grantor chain is valid
- the effective-rights boundary is auditable
- row, column, domain, and masking policies are still applied according to the object contract

## Row, Column, Domain, and Masking Interaction

Authentication and role expansion do not bypass:

- row-level filters
- column visibility rules
- domain masking rules
- security-definer boundary rules

Those policies are evaluated after principal binding and before result publication.

## Refusal Rules

The engine shall refuse session establishment or capability publication when:

- plugin authentication succeeds but local identity mapping fails
- the requested plugin is not allowed by policy
- required shared-right bundles are unresolved
- the resulting principal has conflicting or quarantined security state

## Audit Requirements

The engine shall record:

- plugin selected
- authentication result code
- external identity normalized value or stable digest
- local principal bound
- rights sources expanded
- step-up or MFA state if applicable
- refusal reason when capability publication fails

## Non-Guarantees

This file does not require every external identity provider to map to a local principal automatically. Explicit mapping and policy remain authoritative.

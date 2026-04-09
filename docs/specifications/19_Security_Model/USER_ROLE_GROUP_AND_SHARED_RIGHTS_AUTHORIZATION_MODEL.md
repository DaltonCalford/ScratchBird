# User, Role, Group, and Shared Rights Authorization Model

Status: reconstructed_required_with_current_substrate

## Purpose

This file defines the canonical ScratchBird authorization object model for:
- users
- roles
- groups
- memberships
- object and column grants
- active role state
- shared-rights expansion across managed deployments

## Principal objects

Current code-backed catalog and test surfaces prove first-class security objects for:
- user
- role
- group
- role membership
- object grants
- column grants

These are canonical security objects, not emulation-only metadata.

## Current permission-cache and epoch consequence

The authorization model is not evaluated from raw grants on every access. Current code-backed authority also includes:
- a permission cache keyed by user, object, object type, and privilege
- cache entries bound to committed global security policy epoch
- table-scoped cache entries additionally bound to committed table policy epoch
- TTL and LRU behavior for cached authorization answers

An authorization answer is valid only while its committed epoch anchors still match the current committed anchors.

## Membership model

Rules:
- a user may receive one or more roles
- roles and groups may act as grant aggregation surfaces
- membership changes are transactional security DDL
- effective authorization is computed from the active attachment identity plus resolved memberships

## Grant model

ScratchBird authorization may be granted at least across these layers:
- object-level privilege grants
- column-level privilege grants
- domain-level privilege gates via masking or permission masks
- executable-object security boundaries

Current code-backed proof is strongest for:
- object grant and revoke catalog paths
- role grant and revoke paths
- column grant, revoke, and lookup paths
- role and group CRUD catalog paths

## Active role state

Connection context already carries active-role state and role-switch policy.

Canon requires:
- active role changes are explicit session-security state changes
- active role affects privilege evaluation but does not erase caller identity for audit
- active role switching must remain subordinate to granted membership and policy rules

## Shared-rights evaluation boundary

Shared-rights or synchronized external rights do not bypass the canonical authorization path.

Required order:
1. ingest external identity or shared-rights payload
2. map it into canonical ScratchBird principal and scope form
3. resolve local grants, memberships, policy epochs, and refusal state
4. compute effective authorization

A propagated right that is refused locally must remain refused locally until a new committed security state changes that outcome.

## Parser-versus-runtime split

Current integration tests prove a meaningful security runtime and catalog surface while also proving parser drift:
- many DCL operations are conceptually supported and exercised at catalog/runtime level
- some parser-v3 DCL SQL paths remain pending or gated in tests

Canon rule:
- catalog/runtime security object behavior is authoritative
- missing parser coverage is implementation drift, not permission to weaken the model

## Shared-rights expansion

Across remote-management or managed-cluster deployments, shared-rights expansion must preserve:
- principal identity source
- policy version
- scope
- local override or refusal state
- auditability of propagated changes

Shared rights must never collapse into "cluster member equals full privilege."

## Beta 1 package 06 boundary

Package `06` closes the local principal graph:

- users
- roles
- groups
- memberships
- active-role state
- local permission-cache and policy-epoch enforcement

External identity may still map into those local principals, but cluster-shared
rights propagation, remote drift repair, and managed-deployment grant bundle
distribution remain explicit non-Beta 1 surfaces here and must fail closed
rather than weakening local authorization.

## Security-definer consequence

Granting access to a view, procedure, function, or package does not imply direct access to the underlying objects it uses.
The executable boundary remains the grant surface unless direct object grants are also present.

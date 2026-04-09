Status: reconstructed_required

# User Role Group Authz and Schema Sandbox Resolution Model

## Purpose

This document defines how ScratchBird resolves user, role, group, shared-right, and schema-sandbox authorization into an effective capability set.

## Canonical Resolution Order

Effective authorization shall be resolved in this order:

1. bind the authenticated local principal
2. expand attached roles
3. expand attached groups
4. expand shared-right bundles
5. apply object-level grants
6. apply schema-sandbox and security-definer boundaries
7. apply row, column, domain, and masking policy
8. publish the effective capability set

## User, Role, and Group Rule

Users, roles, and groups are distinct objects. Group membership does not imply role ownership, and role activation does not erase direct user grants. The effective set is the policy-allowed union after conflict resolution.

## Shared Rights Rule

Shared-right grants are additive only where explicitly defined. They remain auditable to their source grant and are revocable independently of the local user object.

## Schema Sandbox Rule

An emulated engine object such as a view, procedure, function, or package may execute against underlying objects the caller cannot access directly, but only within the admitted sandbox or security-definer boundary for that object.

The caller receives:

- rights to invoke the sandboxed surface
- no automatic direct rights to the underlying objects

## Visibility and Mutation Classes

Authorization shall be resolved separately for:

- `SELECT`
- `INSERT`
- `UPDATE`
- `DELETE`
- `REFERENCES`
- `EXECUTE`
- `VISIBLE`
- domain-level use
- masking bypass if any

## Conflict Rule

If one grant source allows invocation of a sandboxed object and another source denies direct underlying-object access, the sandbox boundary remains authoritative. Direct access stays denied unless separately granted.

## Audit Requirements

The engine shall be able to explain:

- bound user
- activated roles
- expanded groups
- shared-right bundles
- object grant sources
- sandbox boundary used
- final reason a request was allowed or refused

## Non-Guarantees

This file does not imply that every emulated engine surface is already fully implemented. It defines the canonical required authorization model for those surfaces.

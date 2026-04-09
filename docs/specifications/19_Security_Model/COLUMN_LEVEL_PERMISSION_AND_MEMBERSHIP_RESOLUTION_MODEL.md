# Column Level Permission and Membership Resolution Model

## Purpose

Define how column-level permissions are resolved and how shared rights flow through direct grants, roles, and groups.

## Resolution Order

Column-level permission checks shall execute in this order:

1. table-level permission for the requested privilege
2. direct user column permission
3. effective role memberships
4. effective group memberships

The first positive match grants access.

## Table-Level Short Circuit

If the caller already has the required table-level privilege, column-level checks shall return success immediately for that column.

This prevents redundant per-column denial when the broader table grant is already authoritative.

## Column Grant Record Shape

Column permission resolution is keyed by:

- table identity
- column name
- grantee identity
- grantee type
- privilege mask

Supported grantee classes proven in code are:

- `USER`
- `ROLE`
- `GROUP`

## Shared Rights Model

Shared rights flow through the effective role set and effective group set for the caller. Column access is therefore not limited to direct grants.

This is the canonical base for:

- shared user-rights models
- role security
- group security

## Rename Propagation

Column rename is a security-sensitive metadata change. When a column name changes, the stored column-permission records shall be updated so the security binding follows the renamed column instead of becoming orphaned.

## Cache and Invalidation

Column-permission caches or lookup accelerators shall be invalidated or refreshed after:

- grant
- revoke
- role-membership change
- group-membership change
- column rename

## Privilege Classes

The column security model uses a generic privilege mask. Current tests prove the `SELECT` path, and the same storage model is intended to support other column-scoped privilege classes admitted elsewhere in canon.

## Fail-Closed Rules

The engine shall deny column access when:

- no matching table-level or column-level grant is found
- the grantee class is unsupported
- security metadata is unreadable or corrupted

## Current Proof and Rebuild Boundary

Current code proves:

- grant and revoke of column permissions
- direct user lookup
- role-derived lookup
- group-derived lookup
- rename propagation into column-permission records

This specification reconstructs the product rule that column permissions are part of the shared-rights security model and are not limited to direct user grants.

# Row Level Security Policy and Force RLS Model

## Purpose

Define the canonical row-level security model for table policy storage, enablement, FORCE behavior, and DML enforcement.

## Table State

Every table may independently carry:

- `rls_enabled`
- `rls_forced`

`rls_enabled` controls whether table policies participate in access control.

`rls_forced` removes privileged bypass and makes even superuser-style callers obey the active policies.

## Policy Identity

Each policy is scoped to one table and is identified by:

- table identity
- policy name
- policy type

Duplicate policy names on the same table are not allowed.

## Policy Types

Current code and tests prove the following policy classes:

- `SELECT`
- `INSERT`
- `UPDATE`
- `DELETE`
- `ALL`

## Policy Fields

A policy may carry:

- enabled state
- target roles
- `USING` expression
- `WITH CHECK` expression

Expressions are stored as policy payloads and may be TOAST-backed when large.

## Enable and Force Operations

The canonical table operations are:

- enable RLS
- disable RLS
- force RLS
- remove FORCE RLS

These operations update table metadata and shall advance the security-policy epoch surfaces so cached security decisions can be invalidated.

## Enforcement Model

### `SELECT`

`USING` determines row visibility.

### `INSERT`

`WITH CHECK` determines whether the proposed new row is admissible.

### `UPDATE`

`USING` determines whether the existing row is visible for update.

`WITH CHECK` determines whether the proposed updated row is admissible.

### `DELETE`

`USING` determines whether the row is visible for deletion.

## Multi-Policy Combination

When multiple applicable policies exist for the same operation, the current enforcement model is conjunction-based. All applicable policy checks must pass.

## Disabled-State Rules

- If table RLS is not enabled, stored policies do not participate in enforcement.
- If a specific policy is disabled, that policy does not participate in enforcement.

## FORCE Behavior

When `rls_forced` is set, privileged callers do not bypass the row-level policy path.

When `rls_forced` is not set, the privileged-bypass model is governed by the broader security subsystem.

## Error and Skip Semantics

Violation of `WITH CHECK` is an error.

Failure of `USING` on an existing row may result in the row being treated as invisible rather than as an error, depending on the statement class.

## Current Proof and Rebuild Boundary

Current code and tests prove:

- policy CRUD
- table enable, disable, and FORCE state
- DML enforcement for `INSERT`, `UPDATE`, and `DELETE`
- role-targeted policy application
- multi-policy conjunction behavior

This specification reconstructs the product rule that row-level security is a full table-policy subsystem, not just parser syntax.

Status: reconstructed_required_with_current_substrate

# GRANT REVOKE PERMISSION RESOLUTION AND SHARED RIGHTS PROPAGATION MODEL

## Purpose

This file defines the canonical ScratchBird permission-resolution order, grant and revoke
behavior, public grants, object-permission evaluation, shared-rights propagation, and
policy interaction.

This is a rebuild-stage authority file. It records:

- current code-backed behavior recovered from parser and catalog code
- required reconstructed specification where the original security model is stronger than
  the currently recovered implementation

## Scope

This file covers:

- permission grants to `USER`, `ROLE`, `GROUP`, and `PUBLIC`
- object-permission resolution order
- `WITH GRANT OPTION`
- cascade revoke behavior
- interaction with effective role and group closure

This file does not redefine:

- row-level policy expression semantics
- column/domain masking payload rules
- security-definer sandbox execution

Those remain specified in their dedicated section `19` files.

## Parser-backed privilege surface

Current recovered parser support includes `GRANT` and `REVOKE` parsing for object-style
privileges including:

- `SELECT`
- `INSERT`
- `UPDATE`
- `DELETE`
- `TRUNCATE`
- `REFERENCES`
- `TRIGGER`
- `EXECUTE`
- `EXECUTE EXTERNAL JOB`
- `USAGE`
- `COPY`
- `CREATE`
- `CREATE JOB`
- `CONNECT`
- `TEMPORARY`
- `VIEW JOB HISTORY`
- `ALL [PRIVILEGES]`

Current recovered parser object classes include:

- `TABLE`
- `JOB`
- `SEQUENCE`
- `FUNCTION`
- `PROCEDURE`
- `SCHEMA`
- `DATABASE`

Current parser behavior also supports:

- `WITH GRANT OPTION`
- `REVOKE GRANT OPTION FOR`
- `PUBLIC` grantee

## Catalog permission forms

Current code uses two closely related permission paths:

- generic permission records keyed by `PermissionObjectType`
- object-permission records keyed by canonical `ObjectType`

The reconstructed specification treats these as one authorization family with two current
storage and API entry paths.

Required rule:

- both paths must preserve the same effective authorization semantics
- neither path may weaken or bypass the principal-resolution order defined below

## Permission resolution order

Current code-backed resolution order is:

1. if the user is `SUPERUSER`, authorize immediately
2. check direct `USER` permission grants
3. check `PUBLIC` grants
4. compute effective roles and check `ROLE` grants
5. compute effective groups and check `GROUP` grants
6. otherwise deny

This order is canonical.

The optimizer, parser, listener, manager, and client tooling are not permitted to invent a
different permission order.

## Effective shared-rights propagation

Shared rights propagate through:

- direct user grants
- transitive role closure
- transitive group closure
- public grants

This means every authorization decision is grounded in:

- the current principal id
- the committed role-membership graph
- the committed group-membership graph
- the committed permission rows

## Grant behavior

Current code-backed grant behavior is:

1. locate an existing permission row for the same object and grantee
2. if found, merge the privilege bitmask with logical OR
3. set grant-option state when requested
4. otherwise create a new permission row
5. bump the security-policy epoch
6. bump the table-policy epoch for table-scoped objects where applicable

Grant publication is transaction-scoped. Uncommitted grants are not authoritative to other
transactions.

## Revoke behavior

Current code-backed revoke behavior is:

1. locate the permission row for the object and grantee
2. clear only the requested privilege bits
3. if no privilege bits remain, delete or invalidate the row according to the path in use
4. bump the security-policy epoch
5. bump the table-policy epoch for table-scoped objects where applicable

`REVOKE GRANT OPTION FOR` is a parser-recognized form and remains part of the canonical SQL
surface even where deeper executor recovery is still being rebuilt.

## Cascade revoke behavior

Current recovered code includes a cascade revoke path that:

- finds permissions on the same object granted by the revoked grantee
- removes the matching privilege bits from those downstream grants
- then revokes the original permission

Required reconstructed behavior:

- cascade revoke must remain deterministic and complete
- transitive downstream privilege loss must be derived from grant lineage, not from ad hoc
  heuristic scans
- if lineage cannot be proven, the revoke path must fail closed instead of silently
  preserving unauthorized downstream rights

## Public grants

`PUBLIC` is a real grant target in ScratchBird and is checked before role and group grants.

Required rule:

- `PUBLIC` must be treated as a committed shared-rights baseline, not as an overlay that
  bypasses later row, masking, or sandbox checks

## Object permissions and sandbox interaction

Object permissions are candidate admission checks, not final proof of visibility.

A request that passes object permission may still be restricted by:

- row-level policies
- column or domain masking
- security-definer sandbox boundary
- emulated schema sandbox boundary

Therefore:

- object permission is necessary but not always sufficient
- later policy layers refine the final visible or executable surface

## Policy interaction

Current recovered code applies table policies by:

- loading table policies
- resolving policy role bindings
- computing effective roles for the user
- admitting policies that match the user or any effective role

Current code stores policy role bindings as serialized names which are then resolved back to
principal UUIDs when loading.

Required reconstructed behavior:

- policy principal binding should converge on stable canonical principal ids
- parser and executor recovery must not depend on ambiguous role-name resolution where a
  canonical principal id is available

## Required reconstructed parity rules

The following are mandatory specification requirements even where the current recovered code
still shows drift:

- all permission-bearing index, storage, DDL, and remote-management surfaces must feed the
  same principal-resolution order
- no optimizer, cache, or management layer may treat any index family or security surface
  as secondary to the point of skipping proper permission resolution
- permission-cache invalidation must occur for every committed membership or grant mutation
  that changes effective authorization
- shared-rights propagation must remain fully transaction-scoped under the always-in-
  transaction MGA model

## MGA publication rule

Security rows follow the same MGA rules as other catalog state:

- grant and revoke rows are committed database state
- no WAL or replay log is authoritative for authorization truth
- restart must reconcile committed security rows, epochs, and caches from durable state

## Fail-closed boundary

The following are non-conforming:

- evaluating authorization from listener-local cached identities alone
- granting rights from parser-local name guesses without committed principal resolution
- treating role or group expansion as optional
- skipping a grant target because it is considered a secondary class

If permission resolution cannot prove authorization through the canonical principal graph,
the engine must deny.

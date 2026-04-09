Status: reconstructed_required

# System Row UUID Cluster Tracking and User UUID Aliasing Model

## Purpose

This document defines the canonical system row UUID model, its use for cluster tracking, and the aliasing rule when a table uses a UUID identity column.

## Canonical Rule

Every logical row has an engine-generated system row UUID assigned at logical row creation or insert time. This UUID is the canonical row identity used by cluster and replication or migration tracking surfaces unless a stricter canonical identity surface is explicitly defined.

## Generation Rule

The engine shall assign the system row UUID:

- when a new logical row is first created
- before the row becomes committed and visible
- independently of user-supplied name or application formatting

## Logical Row Identity Rule

The system row UUID identifies the logical row, not merely one transient physical version. Under MGA versioning:

- updates create new record versions
- the logical row UUID remains stable across the version lineage of that row
- delete state and rollback do not create a second logical row UUID for the same logical row lineage

## Cluster Tracking Rule

The system row UUID is used to:

- track row identity across cluster movement or placement
- correlate row lineage during migration, synchronization, or remote management workflows
- avoid name-based or position-based ambiguity for row identity

## Hidden-by-Default Rule

If a table has no user-visible UUID identity column, the system row UUID may remain a system-managed internal identity surfaced only through administrative, cluster, replication, or diagnostic surfaces admitted by canon.

## User UUID Aliasing Rule

If a table uses a UUID column as its logical identity field, ScratchBird shall bind the system row UUID to that user-visible column rather than maintaining two separate logical UUID identities for the same row.

Required effects:

- the user-visible UUID identity column and the system row UUID refer to the same logical row identity
- the engine does not generate a second competing logical row UUID for that row
- cluster tracking and user-visible identity remain aligned

## DDL Rule

The schema layer shall be able to distinguish:

- a UUID-typed column that is the row’s logical identity alias
- an ordinary UUID-typed user column that is not the row identity alias

Only the former binds to the system row UUID.

## Non-Guarantees

This file does not require every UUID-typed column to be the row identity alias. It defines the rule only when the table uses a UUID identity field as the row identifier.

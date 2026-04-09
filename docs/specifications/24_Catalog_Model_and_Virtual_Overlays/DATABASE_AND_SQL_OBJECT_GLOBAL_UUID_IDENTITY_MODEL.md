Status: reconstructed_required

# Database and SQL Object Global UUID Identity Model

## Purpose

This document defines the canonical UUID identity model for databases and SQL objects.

## Canonical Rule

ScratchBird assigns engine-generated UUIDs to the database itself and to SQL objects as their immutable system identity. User-visible names are not the system identity and may change without changing the UUID.

## Covered Object Classes

The canonical UUID-bearing object classes are:

- database
- schema
- table
- view
- domain
- index
- sequence
- procedure
- function
- package
- trigger
- event
- custom error message
- role
- user
- group
- security policy objects
- other admitted SQL objects persisted in catalog truth

## Generation Rule

The engine shall generate UUIDs:

- at database creation for the database identity
- at SQL object creation for object identity
- before committed catalog publication of the new object

UUID generation shall be engine-owned and shall not rely on user-supplied names for uniqueness.

## Immutability Rule

Once assigned and committed:

- an object UUID is immutable for the life of that object
- renaming an object does not change its UUID
- moving an object between user-visible naming scopes does not change its UUID if the logical object persists
- drop and recreate creates a new UUID

## Global Uniqueness Rule

The UUID namespace for databases and SQL objects is global, not merely local to one catalog table name or one database name. No two live database identities or live SQL object identities may intentionally reuse the same UUID for different logical objects.

## Catalog Rule

The system catalog is the authority for database and SQL object UUID persistence. Name-based resolution is always a user-layer lookup that ends in UUID-bound identity, not the reverse.

## Cluster Rule

Cluster, migration, remote management, and metadata synchronization surfaces shall use database and object UUIDs as the canonical durable identity. Name-based routing or comparison may assist operator workflows, but UUID identity remains authoritative.

Objects whose design requires cross-node common definition shall preserve the same UUID and canonical definition across the cluster. This class includes at minimum:

- domains
- events
- custom error messages

These shared-definition objects are cluster-common metadata, not node-local reinterpretations.

## Non-Guarantees

This file does not require every user-visible object class to expose its UUID directly in ordinary SQL output. It defines the system identity model.

# Remote Connector Metadata Capability and Passthrough Policy Model

## Purpose

Define the catalog-backed remote connector model used for foreign-engine metadata discovery, passthrough control, prepared lifecycle tracking, and degraded-mode operation.

## Remote Connector Record

A remote connector record carries:

- connector identity
- foreign-server identity
- foreign-data-wrapper identity
- connector name
- engine name
- engine version lineage
- endpoint URI
- default mapping identity
- optional policy identity
- connector state
- failure counts and probe times
- module checksum attestation

## Admission and Integrity Rules

The connector catalog fails closed when:

- module checksum attestation is absent
- engine version lineage is absent
- state transition is invalid

This makes connector catalog rows executable trust contracts, not informal inventory rows.

## Capability Snapshot

Connector capabilities are catalog-backed rows keyed by connector identity and capability key.

Each capability row carries:

- capability key
- capability group
- JSON capability value
- source version lineage
- enablement state
- discovery time

Capability lineage shall match the connector engine version lineage. Stale or mismatched lineage is rejected.

## Remote Metadata Snapshot

Metadata discovery is stored as immutable snapshot records with:

- snapshot identity
- sequence number
- snapshot kind
- snapshot status
- engine version lineage
- object and column counts
- optional catalog hash
- start and completion times
- optional error linkage

Completed snapshots are immutable evidence and shall not be rewritten in place.

## Remote Metadata Object and Column Mapping

Remote metadata objects and columns are first-class catalog rows. They may include:

- remote path
- schema and object names
- normalized domain mapping
- local object mapping
- local schema mapping
- supportability flag

This model is the basis for remote catalog inspection and controlled migration planning.

## Passthrough Policy

Passthrough policy is connector-scoped and explicitly controls:

- query passthrough
- DML passthrough
- DDL passthrough
- admin passthrough
- procedural passthrough
- join-with-local-transaction allowance
- row, byte, and timeout limits
- required capabilities
- audit level

Passthrough is therefore policy-gated, not implied by connector existence.

## Prepared and Transaction Bindings

Remote prepared statements and remote transaction bindings are catalog-tracked surfaces. This allows a connector runtime to support prepared lifecycle, remote transaction mode, heartbeat, and terminal-state evidence without treating the remote engine as opaque.

## Current Proof and Rebuild Boundary

Current code and tests prove:

- remote connector catalog rows
- capability lineage enforcement
- immutable metadata snapshots
- passthrough policy rows
- prepared-statement and remote transaction binding rows

This specification reconstructs the product rule that remote-engine access is a governed connector subsystem, suitable for engines that do not offer native replication.

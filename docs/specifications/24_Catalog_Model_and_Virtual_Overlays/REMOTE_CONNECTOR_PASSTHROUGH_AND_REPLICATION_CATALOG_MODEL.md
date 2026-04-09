# Remote Connector, Passthrough, and Replication Catalog Model

## Status

Reconstructed required specification with substantial current catalog substrate.

## Purpose

This document defines the catalog model for outbound remote connectors, remote metadata snapshots, passthrough policy, remote transaction binding, publication/subscription state, and replication channels.

## Scope

This section governs catalog truth for:

- remote connector definitions
- remote capability discovery
- remote metadata snapshots and mapped objects
- remote schema mapping
- passthrough policy
- remote prepared statements
- remote transaction bindings
- remote execution audit
- remote error inventory
- publications and subscriptions
- replication channel configuration

## Remote Connector Catalog

Current connector catalog rows include:

- connector id
- FDW server id
- FDW id
- connector name
- engine name
- optional engine version text
- endpoint URI
- optional default mapping id
- optional policy id
- connector state
- failure counters
- last probe and last ready timestamps
- module checksum

This is a real current catalog surface, not a future placeholder.

## Capability Discovery Catalog

Connector capabilities are cataloged as key-group-value rows with:

- capability id
- connector id
- capability key
- capability group
- capability value JSON
- optional source-version text
- enabled flag
- discovered time

The engine therefore tracks remote capability inventory explicitly rather than inferring everything at runtime on every request.

## Remote Metadata Snapshot Catalog

Current snapshot catalog rows include:

- snapshot id
- connector id
- snapshot sequence
- snapshot kind
- snapshot status
- optional engine-version text
- object count
- column count
- optional catalog hash
- started and completed timestamps
- optional error id

Associated object and column rows store:

- remote path and schema/object names
- remote object kind
- remote signature
- optional definition JSON
- optional mapped local object or schema ids
- normalized domain ids for columns
- remote type information
- nullability
- default expression text
- type width, precision, scale, charset, collation, and extra JSON

This means metadata-driven proxy and migration behavior is catalog-backed and diffable.

## Schema Mapping Catalog

Current schema mapping rows include:

- mapping name
- remote schema pattern
- local schema id
- mapping mode
- included object kinds
- optional exclusion patterns
- optional rename-rule JSON
- optional last snapshot id

Schema mapping is therefore first-class engine metadata, not an external tool-only concern.

## Passthrough Policy Catalog

Current remote passthrough policy rows include:

- allow query
- allow DML
- allow DDL
- allow admin
- allow procedural
- allow join-local-txn
- max rows
- max bytes
- timeout
- optional required capabilities
- audit level

This is the core catalog control point for safe remote execution.

## Remote Prepared Statement and Transaction Binding Catalog

Prepared statement rows include:

- statement identity
- connector id
- session id
- statement name
- statement fingerprint
- command text
- optional parameter signature
- remote handle
- creation, use, and expiry timestamps

Remote transaction binding rows include:

- connector id
- session id
- local txid
- remote transaction mode
- remote transaction state
- remote transaction token
- begin time
- optional terminal time
- optional last heartbeat
- optional last error id

These rows are necessary for safe proxying and migration because remote execution and remote transaction state must survive beyond a single local function call.

## Execution Audit and Error Catalog

Remote execution audit rows record at least:

- request identity
- connector id
- session id
- optional txid
- operation class
- statement fingerprint
- prepared vs direct execution
- transaction mode
- execution status
- rows returned and affected
- bytes in and out
- latency
- started and finished timestamps
- optional error id

Remote error rows record:

- error class
- optional remote code
- mapped local code
- message text
- first seen and last seen times
- occurrence count
- open vs closed state

This gives proxy and migration behavior a catalog-backed audit trail instead of a transient log-only trail.

## Publication and Subscription Catalog

Publication rows define:

- publication name
- owner
- publish insert/update/delete/truncate flags
- publish-via-partition-root flag

Publication table rows attach:

- table id
- optional column-list id
- optional where-expression SBLR id

Subscription rows define:

- subscription name
- owner
- optional connection-info id
- enabled state
- optional slot name
- sync-commit
- copy-data
- create-slot
- refresh-on-start

These rows show that migration and replication intent is already represented in canonical engine metadata.

## Replication Channel Catalog

Replication channel rows define:

- channel name
- direction
- state
- mode version
- optional publication id
- optional subscription id
- optional source and target server ids
- DDL policy
- conflict policy
- retry limits and backoff
- lag warning and critical thresholds
- batch size limits
- split-brain fence enablement
- split-brain detect window

This is a strong code-backed substrate for migration and replication policy, even if not every runtime lane is equally mature today.

## Canonical Rule For Non-Native Replication Systems

For engines that do not expose a natural replication mechanism or durable remote transaction forcing compatible with ScratchBird's MGA model, ScratchBird must rely on:

- remote metadata snapshots
- passthrough policy
- remote transaction binding
- publication/subscription metadata
- replication channel policy
- audit and retry state

This specification therefore treats proxy and migration as policy-driven engine behavior, not as a thin best-effort connector.

## Partial-Implementation Boundary

Current code strongly proves the catalog substrate.

It does not, by itself, prove that every runtime path for migration, replay, split-brain handling, and remote transactional coordination is fully mature across every supported connector and engine family.

Therefore this document is required specification with substantial current catalog implementation and remaining runtime maturation lanes.

## Required reconstructed migration and cutover records

The rebuilt specification requires the catalog model to grow explicit migration
and cutover records wherever non-native replication or copy-driven promotion is
claimed.

These records are required even where the current code substrate is only partly
implemented.

### Migration session record

A migration session record must track at minimum:

- migration session id
- connector id
- source identity anchor
- target identity anchor
- capability class
- phase state
- started and updated timestamps
- current barrier class
- last validation result
- quarantine or promotion reason

### Cutover fence record

A cutover fence record must track at minimum:

- fence id
- migration session id
- route-change scope
- fence state
- requested by
- requested time
- validation evidence reference
- release or promotion time

### Validation evidence record

A validation evidence record must track at minimum:

- evidence id
- migration session id
- comparison class
- source metric or signature
- target metric or signature
- result state
- captured time
- optional failure reason

## Catalog rule for non-native engines

For engines that do not provide natural replication or durable remote
transaction forcing compatible with ScratchBird's MGA model, the catalog must be
able to represent:

- source identity anchors
- copy windows
- replay batches
- validation outcomes
- quarantine and cutover state

Without these records, the engine may support bounded proxying but may not claim
full migration or cutover authority.

## Fail-closed publication rule

Publication, subscription, and replication channel rows are not enough on their
own to justify transparent migration claims.

If explicit migration-session, validation, and cutover-fence state cannot be
proven, the engine must:

- remain in bounded proxy or copy mode
- refuse automatic promotion
- preserve audit and retry state

# Cluster Fabric Event, Error, and Session Redaction Model

## Scope

This file defines the persisted cluster-fabric row contract for:

- fabric session identity
- fabric events
- fabric errors
- redaction boundaries for sensitive control-plane material

This file is authoritative for the durable evidence and identity substrate
behind manager-mediated cluster or remote-control activity.

## Current code-backed authority

Current code-backed recovery proves all of the following:

1. cluster-fabric sessions persist effective identity
2. cluster-fabric events are durable catalog rows
3. cluster-fabric errors are durable catalog rows
4. error retrieval already redacts token and endpoint material

Canonical rule:

- the cluster-fabric error lane is not free-form log text
- it is persisted, queryable, and redaction-governed state

## Session identity contract

The persisted session family must remain able to bind:

- effective user
- effective role, when present
- effective group, when present
- effective schema
- search-path profile, when present
- open and close timing
- last activity timing

Canonical rule:

- manager-mediated or fabric-mediated activity is attributable to effective
  principal identity
- that identity is durable evidence, not transient process memory only

## Event contract

The persisted event family must remain able to record, at minimum:

- link identity
- optional session identity
- optional task identity
- event class
- event state
- event time
- bounded event detail

Event rows are auditable operational history. They are distinct from link policy
rows and distinct from error rows.

## Error contract

The persisted error family must remain able to record, at minimum:

- link identity
- optional session identity
- optional task identity
- error class
- refusal or failure code
- bounded error detail
- event time

## Redaction rules

The persisted retrieval path must remain redaction-aware.

At minimum, the public inspection path must not expose:

- raw DBBT material
- raw LPREFACE payloads
- raw bearer or token-auth material
- raw endpoint credentials
- unredacted embedded credentials in endpoint text

Canonical rule:

- the catalog may retain enough bounded evidence to diagnose control-plane
  failures
- public inspection and support-bundle paths must redact sensitive fragments

## Relationship to support bundles and operator tooling

Support-bundle, admin-SQL, and manager-inspection surfaces may summarize or
query these rows, but they may not bypass the redaction rules in this file.

## MGA boundary

Session, event, and error rows remain ordinary MGA-governed catalog state:

- transaction-scoped
- commit-visible by snapshot rules
- restart-visible by durable row state

They are not reconstructed from external control-plane logs.

## Reconstructed-required behavior

The lost-spec rebuild requires later remote-management and cluster-control
surfaces to reuse these durable row families rather than creating a shadow
event store outside the catalog.

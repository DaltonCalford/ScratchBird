# Transaction Lineage and Provenance Model

## Purpose

This file defines the authoritative lineage chain for each transaction and the provenance fields that must be recorded to support visibility, replay, and forensic analysis.

ScratchBird is always in a transaction. Every command executes inside a transaction context. The lineage model therefore begins with transaction creation and ends only at terminal publication.

## Canonical Lineage Event Order

### `TX_BEGIN`

`TX_BEGIN` is mandatory and shall be the first lineage event for every transaction.

Exactly one `TX_BEGIN` shall exist for a transaction.

A duplicate `TX_BEGIN` is a rejected malformed lineage chain.

### `CONTEXT_UPDATE`

`CONTEXT_UPDATE` records transaction-local runtime context that affects provenance, replay interpretation, or catalog visibility.

`CONTEXT_UPDATE` may appear zero or more times after `TX_BEGIN` and before the terminal event.

### `DDL_EFFECT`

`DDL_EFFECT` records transaction-local schema effects that must be bound to the transaction's committed schema epoch if the transaction commits.

`DDL_EFFECT` may appear zero or more times after `TX_BEGIN` and before the terminal event.

### Terminal Event

Exactly one terminal event shall close the lineage chain.

The terminal event shall be `TX_COMMIT` or `TX_ROLLBACK`.

No lineage event may appear after the terminal event.

## Provenance Fields

Every transaction lineage chain shall carry the stable transaction identity.

Every committed transaction lineage chain shall carry the committed publication order required for durable interpretation of commit order.

Every replayable transaction lineage chain shall carry the committed schema epoch UUID required for replay interpretation.

Every replayable transaction lineage chain shall carry the forensic snapshot capsule UUID required for replay interpretation.

## Normative Append Rules

The engine shall create `TX_BEGIN` before recording any other lineage event.

The engine shall append `CONTEXT_UPDATE` and `DDL_EFFECT` only while the transaction remains active.

The engine shall freeze the lineage chain before terminal publication.

The engine shall durably publish the terminal lineage state before reporting commit or rollback success.

## Rejection Rules

A lineage chain without `TX_BEGIN` is invalid.

A lineage chain with more than one `TX_BEGIN` is invalid.

A lineage chain with more than one terminal event is invalid.

A lineage chain with events after a terminal event is invalid.

A committed transaction without the required committed publication order is invalid.

A replayable committed transaction without the required schema-epoch or forensic-capsule binding is invalid.

## Interaction with Commit and Rollback

`COMMIT` shall append or finalize `TX_COMMIT` and durably publish it before acknowledgement.

`ROLLBACK` shall append or finalize `TX_ROLLBACK` and durably publish it before acknowledgement.

After either terminal event is acknowledged, the connection immediately continues in a new transaction context.

## Interaction with Savepoints

Savepoints do not create a new top-level lineage chain.

Savepoint rollback may invalidate transaction-local work, but it shall not create a second `TX_BEGIN`.

If a lineage event becomes invalid because a savepoint rollback removes the underlying effect, that lineage event shall not survive into the frozen terminal chain.

## Explicit Non-Goals

This file does not define external audit export formats.

This file does not define WAL lineage reconstruction.

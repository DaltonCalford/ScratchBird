# Transaction Map Layout

## Purpose

This file defines the authoritative durable layout and validation contract for the transaction map. The transaction map is the durable source of truth for MGA terminal transaction state. It is not a WAL-derived view.

## Fixed Placement

Bootstrap page `4` is the live transaction-map bootstrap root.

The transaction map shall remain reachable from the fixed bootstrap placement defined by section `06`.

## Transaction Map Header

The transaction map root shall expose the following durable header fields.

`first_tip_page_id` identifies the first transaction-map page in the durable chain.

`tip_page_count` records the count of transaction-map pages currently published.

`next_txid` records the next transaction identifier to allocate.

`oldest_active_txid` records the lowest transaction identifier that remains active.

`latest_completed_txid` records the highest transaction identifier known to have reached a terminal state.

## Transaction Entry Fields

Every durable transaction entry shall expose the following fields.

`state` records the authoritative MGA state for the transaction.

`commit_time` records the durable commit timestamp when the state is committed.

`commit_seqno` records the durable committed publication order when the state is committed.

## Canonical States

The transaction map shall represent at least the following canonical states.

`ACTIVE`

`COMMITTED`

`ROLLED_BACK`

`PREPARED`

No other state may be published unless this file is updated first.

`ABORTED` is reserved for non-transaction abort/cancel surfaces and must not be
published as a durable transaction-map or TIP/CLOG terminal transaction state.

## Publication Rules

Beginning a transaction reserves an `ACTIVE` entry.

Committing a transaction publishes `COMMITTED` and the required durable `commit_seqno`.

Rolling back a transaction publishes `ROLLED_BACK`.

Preparing a transaction publishes `PREPARED` only when the required durable prepared-state evidence also exists.

A transaction shall not become visible as committed to future transactions until its durable `COMMITTED` entry is published according to the durability rules of section `08` and section `35`.

## Validation Rules

The header shall be structurally coherent with the published transaction pages.

`next_txid` shall not point backward.

`oldest_active_txid` shall not reference a transaction outside the supported durable inventory.

`latest_completed_txid` shall not contradict the known terminal transaction inventory.

A `COMMITTED` entry shall not omit `commit_seqno`.

A duplicate `commit_seqno` shall be treated as reopen drift or corruption according to the classification rules below.

A `PREPARED` entry shall not exist without the required durable prepared-state evidence.

## Canonical Repairable Classes

### `REPAIRABLE_REOPEN_DRIFT_MISSING_COMMIT_SEQNO`

A transaction appears committed on reopen, but the durable committed publication order required for that entry is missing. This class is repairable only if current supported normalization can derive one unique result without inventing new visibility.

### `REPAIRABLE_REOPEN_DRIFT_DUPLICATE_COMMIT_SEQNO`

Two or more committed entries reuse the same durable committed publication order. This class is repairable only if current supported normalization can resolve the contradiction without inventing new visibility.

### `STARTUP_REPAIR_REQUIRED_HEADER_NORMALIZATION`

The transaction-map header and entry inventory disagree in a way that current supported normalization can reconcile.

## Canonical Fail-Closed Corruption Classes

### `CORRUPTION_PREPARED_WITHOUT_DURABLE_PREPARED_RECORD`

A transaction entry is published as `PREPARED`, but the required durable prepared-state evidence is missing.

### `CORRUPTION_UNSUPPORTED_HEADER_ENTRY_CONTRADICTION`

The transaction-map header and entry inventory disagree in a way the current supported repair rules cannot reconcile.

### `CORRUPTION_UNSUPPORTED_COMMIT_PUBLICATION_STATE`

A committed entry requires commit visibility that the durable fields do not support under the current MGA model.

## Startup Normalization Algorithm

1. Validate the fixed bootstrap root.
2. Load the durable transaction-map header.
3. Scan the published transaction-map pages.
4. Validate every entry state and required fields.
5. Classify any contradiction into a repairable class or a fail-closed corruption class.
6. Apply only supported normalization rules.
7. Refuse startup if any contradiction remains outside the repairable classes.

## Explicit Non-Goals

This file does not define WAL sequence numbers.

This file does not define redo-log replay ordering.

This file does not define donor-log reconciliation.

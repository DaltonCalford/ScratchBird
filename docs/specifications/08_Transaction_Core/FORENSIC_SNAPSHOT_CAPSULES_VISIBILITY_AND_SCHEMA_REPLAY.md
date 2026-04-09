# Forensic Snapshot Capsules Visibility and Schema Replay

## Purpose

This file defines how transaction lineage binds to forensic snapshot capsules and committed schema epochs, and how replay consumers must use that binding. The contract is current-state authoritative. It is not a planning placeholder.

## Core Rule

A replayable transaction shall bind to the committed schema epoch and forensic snapshot capsule required to interpret that transaction correctly.

The schema epoch and forensic snapshot capsule are part of transaction provenance. They are not advisory metadata.

## Owning Components

`CatalogManager` owns durable catalog rows for schema epochs, forensic capsules, and transaction-linked provenance rows.

`ConnectionContext` owns runtime lineage capture and replay-session binding.

`TransactionManager` owns the transaction identity that the provenance rows attach to.

## Visibility Rules

Committed forensic snapshot capsules are visible according to committed catalog visibility rules.

Committed schema epochs are visible according to committed catalog visibility rules.

Uncommitted current-transaction provenance remains local to the current transaction context until commit.

A future transaction shall not observe a schema epoch or forensic capsule as committed before the transaction that publishes it commits.

## Required Durable Fields

Each replayable transaction record shall durably retain the transaction identity required to locate its lineage rows.

Each replayable transaction record shall durably retain the committed schema epoch UUID required for correct replay interpretation.

Each replayable transaction record shall durably retain the forensic snapshot capsule UUID required for correct replay interpretation.

## Normative Commit-Binding Rules

Before commit acknowledgement, the engine shall freeze the current transaction's lineage payload.

Before commit acknowledgement, the engine shall freeze the transaction's schema epoch binding.

Before commit acknowledgement, the engine shall freeze the transaction's forensic snapshot capsule binding.

Commit success shall not be reported until the transaction's terminal lineage state and replay-binding state cross the durability fence required by the active durability mode.

## Replay-Binding Algorithm

1. Load the transaction's durable lineage and provenance rows.
2. Resolve the referenced committed schema epoch.
3. Resolve the referenced forensic snapshot capsule.
4. Confirm that both resolved objects are committed and structurally valid.
5. Bind replay interpretation to that schema epoch and that forensic snapshot capsule.
6. Refuse replay if any required binding is missing, contradictory, or uncommitted.

## Refusal Cases

Replay shall be refused if the referenced schema epoch UUID does not resolve.

Replay shall be refused if the referenced forensic snapshot capsule UUID does not resolve.

Replay shall be refused if either resolved object is present only as uncommitted state.

Replay shall be refused if the lineage row and the replay-binding row disagree about the transaction's terminal identity.

Replay shall be refused if the schema epoch required for correct object interpretation is missing.

## DDL Interaction Rules

DDL is transaction-scoped and follows the same terminal publication rules as DML.

If a transaction commits DDL, the committed schema epoch bound to that transaction is the schema epoch that replay shall use.

A future transaction shall not observe committed DDL effects through replay unless the committed schema epoch that describes those effects is also durably published.

## Explicit Non-Goals

This file does not define backup retention policy.

This file does not define external archive packaging.

This file does not define WAL-based historical replay.

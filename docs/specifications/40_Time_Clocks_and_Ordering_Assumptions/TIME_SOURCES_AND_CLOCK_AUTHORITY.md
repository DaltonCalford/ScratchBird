# Time Sources and Clock Authority

## Purpose

This file defines where ScratchBird may use clocks and where clocks are not authoritative.

## Authoritative rules

Local process clocks may be used for diagnostics, metrics, timeout measurement, artifact stamping, and other annotation surfaces.

Local process clocks are not authoritative for transaction correctness, commit visibility, rollback visibility, schema publication, or replay correctness.

Transaction ordering truth remains anchored to MGA publication, transaction inventory, committed publication order, and other transaction-core structures owned by section `08`.

Schema publication truth remains anchored to committed schema epochs and metadata visibility rules owned by sections `24` and `37`.

Replay-binding truth remains anchored to lineage, schema epoch, forensic capsule, and restore or replay evidence rather than to wall-clock leadership.

## Explicit exclusions

There is no logical-clock subsystem guarantee in the current engine.

There is no distributed clock synchronization contract in the current engine.

There is no timestamp-driven serializability or timestamp-as-correctness model in the current engine.

# Replication Channel Cursor Conflict and Migration Audit Model

## Purpose

Define the catalog-backed replication and migration runtime surfaces that support channel state, apply progress, conflict handling, retry queues, and operator audit.

## Replication Channel

A replication channel row carries:

- channel identity
- name
- direction
- state
- mode version
- optional publication and subscription identities
- optional source and target server identities
- DDL policy
- conflict policy
- retry policy
- lag thresholds
- batching thresholds
- split-brain fencing controls
- creator identity

## Channel Members and Origins

Channels have explicit members and origin rows. This permits publisher, subscriber, and peer modeling without overloading a single endpoint record.

## Cursor and Progress Surfaces

Replication cursor rows carry:

- source commit sequence
- applied commit sequence
- lag
- heartbeat time
- last error linkage

Origin progress rows track per-origin progress for the target member.

## Transaction Batch and Apply Log

Replication transaction batches and apply logs are distinct evidence families:

- received or validated batch state
- source commit sequencing
- batch checksum
- apply ordering
- applied row counts
- DDL count
- apply timing

## Conflict and Retry

Conflict rows and retry-queue rows are catalog-backed, not transient-only structures.

Conflict handling records:

- conflict kind
- source and target payload
- resolution state
- resolution timing

Retry handling records:

- queued or running state
- exhaustion or dead-letter state
- retry timing and counters

## Error Redaction

Replication and remote-engine error text is redacted before it becomes catalog-visible. Sensitive credentials and endpoint details do not survive into the observable catalog row in raw form.

## Status Projection

The sys-catalog status surfaces project:

- migration status
- migration audit summary
- replication channel status
- replication conflict queue
- replication cursor status
- shard migration progress

These views are the canonical operator inspection path for migration and replication runtime health.

## Authority Boundary

Replication channels are an explicit subsystem. They are not assumed to exist for every connector or migration case. For engines without a natural replication model, remote connector snapshot and passthrough lanes remain valid independent migration paths.

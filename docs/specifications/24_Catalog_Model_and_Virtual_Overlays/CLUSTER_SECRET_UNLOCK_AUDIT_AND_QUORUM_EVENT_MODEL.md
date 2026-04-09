# Cluster Secret Unlock Audit and Quorum Event Model

## Scope

This file defines the canonical persisted evidence model for:

- cluster-secret unlock attempts
- quorum-evaluation outcomes
- shard collection progress
- break-glass and recovery-class secret access events

## Governing rule

Cluster-secret unlock is an auditable state transition, not a transient
in-memory detail.

Canonical rule:

- unlock attempts, quorum outcomes, and shard-collection posture must be
  representable as durable rows
- public inspection may summarize those rows, but may not replace them

## Current code-backed substrate

Current code-backed recovery already proves:

- encryption profile rows
- encryption key rows
- encryption key-shard rows
- bootstrap unlock policy rows
- security-quorum runtime semantics

This file defines the persisted audit and event layer that must sit beside that
substrate.

## Unlock-attempt row family

The canonical unlock-attempt family must remain able to record, at minimum:

- attempt identity
- database identity
- active key identity
- profile identity
- request time
- completion time, when present
- unlock result class
- quorum decision class
- required shard threshold
- collected shard count
- break-glass flag
- caller or actor identity

## Quorum-event row family

The canonical quorum-event family must remain able to record:

- event identity
- security-quorum failure mode
- security-quorum decision
- remote confirmation posture
- cache posture
- event time
- bounded detail

## Shard-collection row family

The canonical shard-collection family must remain able to record:

- attempt identity
- shard identity
- holder identity
- collection result
- collection time
- source class
- refusal or failure code, when present

Canonical rule:

- shard collection progress is auditable row state
- it is not hidden inside opaque control-plane logs

## Break-glass and recovery rule

If policy permits break-glass or recovery-class unlock behavior, the resulting
audit event must remain explicitly marked as such.

Canonical rule:

- break-glass is never silent
- break-glass events remain durable audit evidence

## Redaction rule

Audit and event rows may retain enough bounded detail for diagnosis, but public
inspection and support-bundle surfaces must not expose:

- raw shard material
- raw reconstructed secret material
- raw token or credential values used during collection

## MGA boundary

Unlock audit and quorum-event rows are ordinary MGA-governed catalog state:

- transaction-scoped
- commit-visible
- restart-visible

They are not reconstructed from external security logs.

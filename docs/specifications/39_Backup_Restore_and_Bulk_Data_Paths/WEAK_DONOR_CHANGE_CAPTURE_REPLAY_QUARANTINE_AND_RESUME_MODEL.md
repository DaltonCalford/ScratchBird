Status: reconstructed_required

# Weak Donor Change Capture Replay Quarantine and Resume Model

## Purpose

This document defines how change-capture units from weak or non-replicating donors are replayed, quarantined, resumed, and audited.

## Canonical Rule

Weak-donor replay is a managed migration stream, not an ordinary transactional replication stream. Replay units therefore require explicit quarantine and resume semantics.

## Replay Unit State Machine

The canonical replay states are:

- `CAPTURED`
- `ORDERED`
- `APPLY_PENDING`
- `APPLIED`
- `QUARANTINED`
- `RESUME_READY`
- `ABANDONED`

## Quarantine Rule

A replay unit shall enter quarantine when:

- ordering evidence is incomplete
- idempotency identity is missing or inconsistent
- target apply result is ambiguous
- donor-source coverage gap makes safe continuation impossible without review

## Resume Rule

Resume is allowed only when:

- the replay unit’s identity is intact
- ordering position is still known
- prior apply state can be classified as applied, not applied, or safely idempotent
- any required operator override is recorded

## Batch Continuity Rule

Quarantine of one replay unit shall not silently reorder later units. The migration controller shall either:

- preserve later units behind the quarantined unit
- or split the stream into explicitly independent ordered partitions

## Audit Requirements

The migration audit shall preserve:

- replay-unit identity
- capture source
- ordering basis
- quarantine reason
- resume decision
- final disposition

## Non-Guarantees

This file does not promise automatic recovery from every weak-donor replay fault. It defines deterministic quarantine and resume boundaries.

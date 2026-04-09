Status: reconstructed_required

# Non-Transactional Donor Replay Order and Idempotency Model

## Purpose

This document defines how ScratchBird replays extracted changes from donors that cannot provide authoritative transaction boundaries.

## Canonical Rule

For non-transactional or transaction-weak donors, replay order and idempotency must be made explicit by the migration system. They cannot be inferred from nonexistent donor commit guarantees.

## Replay Unit

The canonical replay unit shall preserve:

- source object identity
- extracted change identity or batch identity
- observed source ordering marker if any
- target replay ordinal
- idempotency key

## Replay Ordering Rule

Replay ordering shall be determined by one of:

- source-provided stable ordering marker
- extraction-batch ordinal plus intra-batch deterministic order
- proxy-observed session order where coverage is proven sufficient

If no stable ordering basis exists, the replay unit shall be marked as non-orderable and automatic replay beyond snapshot-only mode is non-conforming.

## Idempotency Rule

Every replayable unit shall have a deterministic idempotency key so repeated apply attempts do not create unbounded duplicate effects on the target side.

## Partial Failure Rule

If replay fails mid-stream:

- completed replay units shall remain attributable by idempotency key
- remaining replay units shall stay pending or quarantined
- restart shall not assume donor-side transaction rollback semantics

## Cutover Interaction

Final cutover shall verify that all replay units admitted for the cutover window are either:

- applied exactly once according to idempotency tracking
- explicitly quarantined and included in the residual uncertainty classification

## Non-Guarantees

This file does not claim non-transactional donors can emulate true transactional replication. It defines the minimum replay discipline for such donors.

# Fail Closed And Degraded Mode Rules

Status: current_authority

## 1. Scope

This file defines when ScratchBird Alpha must fail closed and when it may continue in a degraded but correct mode.

## 2. Fail-Closed Writeback Rules

The engine must fail closed for safe-mode durability whenever an open writeback incident fences publication.

Required behavior:

1. transaction publication must refuse to proceed when write admission is fenced.
2. terminal commit publication must refuse to ACK when write admission is fenced.
3. sync failure on the primary database file must fail the operation.
4. sync failure on a registered durable tablespace must fail the operation.
5. sync failure on an active durability-participating shadow filespace must fail the operation.

## 3. Ordered-Publication Rules

The engine must fail closed if it cannot maintain ordered publication.

Required behavior:

1. TIP/CLOG/header publication must not be treated as durable until the forced-write fence completes.
2. post-terminal marker publication must not be assumed durable without its final fence.
3. restart must conservatively reconcile any ambiguity; it must not invent success from missing durable evidence.

## 4. Prepared Transaction Rules

The engine must fail closed when PREPARED durability evidence is inconsistent.

Required behavior:

1. catalog-first prepared records must exist before PREPARED TIP/CLOG is accepted as valid.
2. PREPARED TIP without durable catalog evidence is corruption.
3. stale prepared catalog rows may be cleaned only through explicit startup normalization rules.

## 5. Shadow And Mirroring Rules

Shadow-routing rules are strict when a shadow is active in the durability contract.

Required behavior:

1. mirror write failure blocks the current caller.
2. shadow sync failure blocks the current caller.
3. a shadow may be promoted operationally, but promotion does not change the anti-WAL durability model.

## 6. Derivative Evidence Rules

The following lanes are derivative and do not force correctness failure by themselves:

1. `wal_after` export.
2. remote archive database delivery.
3. forensic bundle generation.
4. sweep statistics emission.

Those lanes may fail or backlog without redefining commit truth.

However:

1. if local evidence-before-prune rules require a local immutable artifact, prune must remain blocked until the local prerequisite succeeds.
2. derivative downstream delivery still remains non-authoritative even when prune is blocked for a local prerequisite.

## 7. Degraded Concurrency Rules

The engine may continue correctly while exposing higher contention if the only issue is structural lock pressure.

Examples:

1. B-tree page-lock coupling causing reader/writer waits.
2. explicit table reservation conflicts.

These are degraded-availability conditions, not durability-truth failures.

## 8. Non-Degraded Incorrectness Rules

The engine must never degrade into these incorrect states:

1. acknowledging safe-mode commit before the forced-write fence.
2. treating derivative logs as authoritative recovery truth.
3. treating reader locks as the main MGA visibility mechanism.
4. permitting PREPARED restart truth without durable catalog evidence.

## 9. Certification Requirements

Implementation is conforming only if tests prove:

1. writeback incidents fence commit and publication.
2. shadow durability failures fail closed in safe modes.
3. derivative export failures remain non-authoritative.
4. structural lock pressure does not get misclassified as durability correctness.

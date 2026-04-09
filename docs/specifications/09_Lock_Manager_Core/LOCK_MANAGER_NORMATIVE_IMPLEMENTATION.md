# Lock Manager Normative Implementation

Status: current_authority

## 1. Scope

This file defines the current lock-manager implementation contract for ScratchBird Alpha.

The lock manager is subordinate to Firebird-style MGA semantics. It is not an independent source of truth.

## 2. Authoritative Rule

Firebird-style MGA semantics govern all lock behavior.

Therefore:

1. ordinary row visibility is version-based, not lock-based.
2. ordinary tuple blocking is write-write only.
3. structural or administrative locks are secondary implementation devices.
4. no PostgreSQL-derived helper may override the MGA rule set.

## 3. Current Lock Targets

Current lock targets are:

1. database
2. table
3. page
4. tuple

Target meaning is authoritative as follows:

1. tuple means ordinary row-write conflict coordination.
2. page means structural coordination only.
3. table means explicit administrative reservation only.
4. database means engine-level coordination only.

## 4. Current Lock Modes

Current code contains a broader lock-mode vocabulary derived from PostgreSQL-style naming.

That vocabulary is implementation scaffolding only.

Normative rule:

- mode names do not change the fact that Firebird-style MGA semantics remain authoritative.

## 5. Tuple Special Case

Tuple-target conflicts are unconditional for overlapping tuple locks.

Because the current engine only acquires tuple row-exclusive locks for write operations, the tuple path is currently the ordinary write-write conflict gate.

This is the current acceptable implementation path.

## 6. Current Call-Site Ownership

Current call-site ownership is:

1. StorageEngine acquireTupleLock owns ordinary tuple write-conflict acquisition for update and delete.
2. B-tree code owns page lock coupling for index traversal and structure changes.
3. ConnectionContext table reservations own explicit table-lock acquisition.
4. prepared-transaction publication snapshots granted locks into durable catalog evidence.

A limited implementer must not invent additional ordinary tuple reader locks outside these surfaces.

## 7. Wait, No-Wait, And Restart

The lock manager currently supports wait, no-wait, timeout, deadlock detection, and higher-layer read-consistency restart handling.

Those features are acceptable only when they preserve MGA semantics.

In particular:

1. a tuple write conflict may wait, fail immediately, or trigger statement restart depending on policy.
2. an ordinary read must not be forced into tuple-lock participation.

## 8. Structural Page Locks

Current B-tree code uses page lock coupling.

This is only a physical structure protection device.

Normative limits:

1. it is not the ordinary MGA row-lock model.
2. it cannot be used to justify reader participation in visibility locking.
3. it cannot be allowed to compromise Firebird-style MGA semantics merely because the helper machinery resembles PostgreSQL.
4. where current code still causes reader blocking during structural page operations, that behavior is bounded current implementation detail and remains subject to MGA-first review.

## 9. Table Reservations

Explicit table reservations are optional administrative controls.

They are not the default concurrency mechanism for user DML visibility.

## 10. Prepared Transaction Persistence

Before prepared transaction state becomes durably published, the engine snapshots granted locks into catalog evidence.

That evidence exists so restart can distinguish legitimate prepared ownership from corruption.

## 11. Non-Guarantees

The current implementation does not guarantee:

1. that every structural coordination path is already minimized to the ideal Firebird-style MGA shape.
2. that all PostgreSQL-derived helper vocabulary has been fully removed from internal code.

However, neither of those gaps changes the normative rule that Firebird-style MGA semantics are authoritative.

## 12. Conformance Rule

Any borrowed PostgreSQL-style locking benefit is acceptable only if it does not compromise Firebird-style MGA locking.

If a borrowed behavior does compromise MGA semantics, the behavior is non-conforming and must be changed.

## 13. Certification Requirements

Implementation is conforming only if tests prove:

1. tuple conflicts are triggered on competing write attempts.
2. ordinary reads do not need tuple locks.
3. prepared transactions durably preserve granted-lock evidence before prepared publication.
4. structural page-lock behavior does not become the authoritative concurrency model.
5. borrowed helper machinery does not compromise Firebird-style MGA semantics.

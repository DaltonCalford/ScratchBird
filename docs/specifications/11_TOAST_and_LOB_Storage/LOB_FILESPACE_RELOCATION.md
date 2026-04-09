# LOB Filespace Relocation

## Purpose

This file defines the current relocation boundary for section `11`.

## Current authoritative truth

Section `11` does not currently define a standalone oversized-value relocation subsystem.

The only current relocation-adjacent behavior owned here is MGA-safe retirement and deferred cleanup of old TOAST values during heap and storage-engine update and delete flows.

The current oversized-value lifecycle families shared with the lifecycle layer are `heap_toast` and `hash_overflow`.

## Current runtime behavior

Heap update and delete paths may defer TOAST cleanup to preserve MGA visibility.

Storage-engine cleanup orchestration routes old oversized-value cleanup through `OversizedValueLifecycleInput`.

Deferred cleanup does not constitute a standalone relocate, resume, abort, or validate contract.

## Cross-section ownership

Section `02` owns filespace ownership and relocation authority.

Section `08` owns transaction visibility and commit/rollback semantics relevant to deferred cleanup.

Section `10` owns reclaim ordering and deferred physical cleanup.

## Fail-Closed Boundary

Standalone LOB relocate operations are unsupported.

Standalone LOB relocation progress tracking is unsupported.

Standalone LOB relocation resume or abort operations are unsupported.

Standalone LOB relocation validation commands are unsupported.

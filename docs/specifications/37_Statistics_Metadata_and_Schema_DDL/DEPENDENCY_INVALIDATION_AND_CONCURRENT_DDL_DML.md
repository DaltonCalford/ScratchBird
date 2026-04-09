# Dependency Invalidation and Concurrent DDL/DML

## Purpose

This file defines how dependency change, invalidation, and concurrent schema/data activity interact.

## Dependency authority

The durable dependency graph is authoritative. Dependency refresh occurs through the catalog manager primitives that replace or clear dependency rows as part of the same transaction that performs the schema mutation.

## Invalidation rules

A dependency-changing transaction must:

1. update canonical definition rows when required
2. update dependency rows transactionally
3. stage invalidation intent locally
4. publish committed invalidation only at commit boundary
5. discard invalidation intent on rollback

## Concurrent DDL and DML rules

### Bound work

An already-bound statement may continue against the metadata definition it was bound to until one of these happens:

- it reaches a lock conflict that requires waiting or restart
- it hits an invalidation check that proves the binding is stale
- it is rejected by a fail-closed online-`DDL` rule

### New bind work

A new bind or rebind must use the current transaction-visible committed schema epoch plus the owning transaction's local overlay.

### Unsupported combinations

This specification does not promise unrestricted concurrent online `DDL`. If the engine cannot prove correctness for a concurrent `DDL`/`DML` combination, it must block, restart, or refuse rather than guess.

## Metadata lock integration

Metadata invalidation does not replace the lock manager. Locking, waiting, timeout, deadlock, restart, and victim rollback continue to be governed by the transaction core and lock manager.

A deadlock victim rollback retires:

- uncommitted schema mutation
- uncommitted dependency change
- uncommitted invalidation intent

## Parser and cache consumers

Parser caches, planner artifacts, result caches, and permission caches must treat committed schema epoch change and dependency change as revalidation triggers. A consumer that cannot prove it is aligned with the current committed anchor must discard and rebuild.

## Refusal rules

The system must fail closed if it cannot determine:

- whether a concurrent reader is still bound to a valid committed definition
- whether a dependency-affecting schema mutation was committed or rolled back
- whether a cache consumer has enough information to revalidate after schema publication

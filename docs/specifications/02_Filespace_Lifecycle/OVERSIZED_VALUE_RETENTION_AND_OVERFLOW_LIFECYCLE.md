# Oversized Value Retention and Overflow Lifecycle

Status: current_authority

## Purpose

Define the current oversized-value lifecycle contract across heap TOAST retention, deferred cleanup, rewrite sequencing, and hash overflow-page cleanup.

## Current authority

Current oversized-value lifecycle truth is split across HeapPage, StorageEngine, ToastManager and ToastVisibility, and HashIndex.

Current code proves:
- heap oversized values are externalized through TOAST-first paths
- HeapPage exposes policy inputs for deferred cleanup, but StorageEngine is the current higher-level decision surface for old-TOAST cleanup during tuple mutation
- old oversized payload cleanup remains MGA-safe and savepoint-sensitive
- hash overflow allocation, compaction, unlink, and free behavior are family-local to HashIndex and are not governed by the heap TOAST path
- publish-before-reuse and reclaim legality remain required across both families even though the physical implementations differ

## Required rules

1. oversized-value cleanup must remain MGA-safe and savepoint-safe
2. old oversized payloads must not be reclaimed before the owning rewrite or rollback boundary is safe
3. heap TOAST retention and hash overflow cleanup may have different physical implementations, but both must stay fail-closed on reclaim legality
4. no caller may guess reclaim legality and mutate oversized-value state directly
5. missing family classification must fail closed rather than reusing pages or unlinking overflow state on guesswork

## Current family split

- heap and TOAST family: StorageEngine and HeapPage coordinate deferred old-value cleanup and TOAST pointer rewrite behavior
- hash overflow family: HashIndex owns overflow-page allocation, compaction, unlink, and free behavior

## Non-goals

- full TOAST physical page layout
- non-hash index overflow families
- parser or SQL dialect lowering rules
- remote archive or backup policy beyond reclaim and rewrite boundaries

## Non-guarantees

- no unified engine-wide oversized-value lifecycle classifier is claimed here
- no claim is made that heap TOAST and hash overflow already share one machine-readable decision vocabulary in code
- no claim is made here for non-hash overflow families

# PostgreSQL DML Write-Path Audit

## Architectural Summary

PostgreSQL is the strongest donor for reducing index churn on updates without giving up transactional correctness. Its write path is still heap-first, but it has several mature mechanisms for avoiding unnecessary secondary index work and for classifying index families by what they can safely promise.

## Insert Optimizations

- Exact B-tree writes are conventional, but GIN has a dedicated `fast-update` path that stores pending entries on list pages and merges them later. This makes bulk insertion of many keys much cheaper than retail insertion into the main tree.
- BRIN does not store tuple pointers. It stores page-range summaries, so insert-time work is a summary update, not one pointer insertion per tuple.
- GIN compresses posting lists and promotes them to posting trees only when they outgrow inline storage, which reduces small-key overhead.

## Update/Delete Optimizations

- `HOT` is the central PostgreSQL write-path idea. If an update does not change any columns referenced by non-summarizing indexes and the new tuple can stay on the same page, PostgreSQL skips creation of new exact index entries.
- HOT chains allow one index entry to represent multiple row versions on the same page. Later pruning and defragmentation reclaim space without a full-table vacuum.
- `heapam.c` also contains bottom-up index-deletion logic to keep version churn under control for index cleanup workloads.
- Deletes and obsolete versions are reclaimed in stages: prune locally first, vacuum globally later.

## Index Maintenance Optimizations

- B-tree maintenance distinguishes between exact tuple-addressing behavior and later cleanup behavior.
- GIN uses a pending-list bulk path plus posting-list compression.
- BRIN explicitly chooses lossy page-range pruning over tuple-exact maintenance.
- PostgreSQL's core strength is that each access method has an explicit contract about trust, lossy behavior, and recheck expectations.

## Reliability And Publication Pattern

- Heap changes and index changes are WAL-managed, but the more important design lesson is the explicit split between foreground write, page-local pruning, and vacuum-time retirement.
- Visibility metadata controls whether an index-only answer is trusted without consulting the heap.

## Best Borrow Candidates For ScratchBird

- A HOT-like fast path for updates that do not change exact indexed values.
- Explicit family contracts: exact, lossy, summary, recheck-required, and bulk-maintained.
- Pending-list bulk insertion for inverted or many-keys-per-row families.
- Bottom-up cleanup for version-heavy exact indexes.

## Local Source Anchors

- `src/backend/access/heap/README.HOT`
- `src/backend/access/heap/heapam.c`
- `src/backend/access/nbtree/README`
- `src/backend/access/gin/README`
- `src/backend/access/brin/README`
- `src/backend/access/transam/README`

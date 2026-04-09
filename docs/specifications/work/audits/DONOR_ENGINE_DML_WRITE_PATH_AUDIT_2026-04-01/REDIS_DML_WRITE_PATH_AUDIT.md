# Redis DML Write-Path Audit

## Architectural Summary

Redis is not a donor for relational storage truth or exact secondary indexing, but it is an excellent donor for hot-path object layout decisions. Its strongest idea is to keep small values in very compact encodings and promote them to heavier structures only when thresholds are crossed.

## Insert Optimizations

- Sets can start as `intset` or `listpack` before upgrading to hash-table-backed representation.
- Hashes and sorted sets also use compact encodings for small payloads before switching to larger structures.
- `rax.c` shows path compression in radix trees, reducing pointer and node overhead for ordered key spaces.

## Update/Delete Optimizations

- Redis is very willing to separate logical removal from the cost of reclaiming memory immediately.
- `UNLINK` and lazy free style behavior are the important lesson here: do not always make the foreground delete path pay full deallocation cost.

## Index Maintenance Optimizations

- Redis does not offer the relational-style secondary-index lessons that PostgreSQL or MySQL do.
- Its value is in compact transient structures and threshold-based promotion.
- In a ScratchBird context, this applies to hot temporary structures such as side-write maps, pending posting lists, duplicate trackers, or small exact-index delta buffers.

## Reliability And Publication Pattern

- `aof.c` exposes the durability tradeoff surface directly through append-only file and rewrite mechanics.
- The key donor lesson is not the persistence model itself. It is the explicit willingness to choose compact, cheap mutable structures first and to repair or rewrite in the background later.

## Best Borrow Candidates For ScratchBird

- Adaptive encoding thresholds for temporary write-path structures.
- Lazy free or deferred reclamation for large delete-side memory releases.
- Compressed radix-like representations where ordered temporary key tracking matters.

## Local Source Anchors

- `src/db.c`
- `src/t_hash.c`
- `src/t_set.c`
- `src/t_zset.c`
- `src/rax.c`
- `src/aof.c`

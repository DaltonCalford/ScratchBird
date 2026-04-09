# OpenSearch DML Write-Path Audit

## Architectural Summary

OpenSearch is a donor for separating durability from query visibility and for keeping recovery metadata tiny and explicit. Its base storage model is Lucene segments plus shard translog, so it is not a relational transaction donor. But its flush, merge, refresh, and checkpoint discipline is extremely relevant for ScratchBird's heavier secondary families.

## Insert Optimizations

- Foreground writes hit the shard engine and Lucene writer while durability is handled through the translog.
- The translog is append-oriented and uses small checkpoint metadata that is rewritten on fsync.
- Reader refresh is managed separately from durability, which prevents every durable write from forcing immediate search visibility.

## Update/Delete Optimizations

- Soft deletes and the live version map let OpenSearch represent updates and removals without synchronously compacting everything on the hot path.
- Merge debt is handled in the background, not by forcing every write to re-pack published segments.

## Index Maintenance Optimizations

- Merge scheduling is treated as a core part of sustained write performance.
- `InternalEngine` explicitly watches for significant merges and can trigger flush-after-merge to free transient disk pressure.
- Refresh management is also tuned to avoid excessive segment creation during heavy indexing.

## Reliability And Publication Pattern

- `Translog.java` documents a translog file plus an atomic single-block checkpoint file containing generation and fsynced offset.
- Generations roll when size or term changes require separation of history.
- This is a very strong donor pattern for "small, atomic, high-value recovery metadata."

## Best Borrow Candidates For ScratchBird

- Separate durability publication from query visibility for families that do not need immediate exact foreground visibility.
- Use tiny atomic checkpoint metadata for generation transitions.
- Trigger maintenance actions from merge debt or transient-disk thresholds instead of waiting for global pain.

## Local Source Anchors

- `server/src/main/java/org/opensearch/index/translog/Translog.java`
- `server/src/main/java/org/opensearch/index/engine/InternalEngine.java`
- `server/src/main/java/org/opensearch/index/shard/IndexShard.java`

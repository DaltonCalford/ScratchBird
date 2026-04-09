# Cassandra DML Write-Path Audit

## Architectural Summary

Cassandra is a donor for immutable write publication, not for transactional visibility truth. Its strongest ideas are append-friendly ingestion, immutable component publication, per-disk flush parallelism, and storage-attached index lifecycles that travel with data components instead of requiring immediate row-by-row secondary maintenance.

## Insert Optimizations

- Writes append to the commit log and update the memtable, which keeps the foreground path sequential and cheap.
- `CommitLog.java` supports multiple sync modes and segment managers, reflecting deliberate control over durability versus latency.
- `ColumnFamilyStore.java` uses per-disk flush executors so memtable flush can parallelize by storage location.

## Update/Delete Optimizations

- Updates and deletes are absorbed into the same append-and-flush model.
- Tombstones and overwritten states are retired during compaction rather than requiring synchronous physical cleanup on the foreground path.
- This keeps write latency predictable, though it moves debt into compaction.

## Index Maintenance Optimizations

- Storage-Attached Indexing is the most relevant donor concept here: index lifecycle tracks SSTable lifecycle.
- Index work is bound to immutable components, which makes publication and retirement simpler than row-by-row global mutable structures.
- Compaction is also the place where older index-attached state can be consolidated or retired.

## Reliability And Publication Pattern

- Commit log provides immediate recovery coverage.
- Immutable SSTables and their attached structures become the published read surface after flush.
- Segments and files are archived or discarded only after covered data is durable elsewhere.

## Best Borrow Candidates For ScratchBird

- Immutable-generation publish for summary, search, text, and ANN families.
- Per-disk or per-device flush parallelism for heavy background maintenance lanes.
- Storage-attached lifecycle for non-exact secondary structures.

## Local Source Anchors

- `src/java/org/apache/cassandra/db/commitlog/CommitLog.java`
- `src/java/org/apache/cassandra/db/ColumnFamilyStore.java`
- `src/java/org/apache/cassandra/db/compaction/CompactionManager.java`
- `src/java/org/apache/cassandra/index/sai/StorageAttachedIndex.java`

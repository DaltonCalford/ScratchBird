# InfluxDB DML Write-Path Audit

## Architectural Summary

InfluxDB 3 Core is a strong donor for time-chunked buffering and object-store-oriented durability staging. The important lesson is that fast ingest can be achieved by making writes durable quickly in a cheap WAL path, then promoting them into larger, more query-friendly persisted units later.

## Insert Optimizations

- The WAL buffers writes in memory and persists them as individual files quickly.
- The WAL comments are explicit that this makes writes durable until larger Parquet files and related snapshot/index files can be produced.
- Table buffers group data by time chunk, which creates a natural unit for later snapshot or persistence work.

## Update/Delete Optimizations

- Mutable table chunks can move into snapshotting state before final persistence, which prevents large storage costs from sitting on the foreground path.
- Catalog state distinguishes soft-deleted from hard-deleted schema resources, which is a useful reminder that logical deletion and physical retirement should be separate states.

## Index Maintenance Optimizations

- The write buffer and persister separate hot mutable state from durable query-serving state.
- Snapshots and checkpoints allow larger persisted artifacts to be written in batches.
- Startup can load from checkpoints or fall back to snapshots, which keeps the persistence path resilient.

## Reliability And Publication Pattern

- `WalFileNotifier` bridges durable WAL persistence to queryable in-memory buffers.
- Snapshot and checkpoint flows make it clear when data is merely durable, when it is queryable in memory, and when it has been promoted to larger persisted form.
- This layered publish model is valuable for ScratchBird summary and archival families.

## Best Borrow Candidates For ScratchBird

- Time or range chunking for append-heavy summary families.
- Fast durable staging followed by larger batch publication.
- Checkpoint-plus-snapshot fallback for family-specific persisted artifacts.

## Local Source Anchors

- `influxdb3_wal/src/lib.rs`
- `influxdb3_write/src/write_buffer/mod.rs`
- `influxdb3_write/src/write_buffer/table_buffer.rs`
- `influxdb3_write/src/persister.rs`
- `influxdb3_catalog/src/catalog.rs`

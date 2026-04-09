# InfluxDB Audit

## Architectural Summary

InfluxDB 3 is a donor for time-series rewrite, chunk pruning, and metadata-driven query routing. It is not trying to invent a separate industrial SQL optimizer; instead it delegates SQL and InfluxQL planning to DataFusion/IOx layers and focuses on chunk selection and time-series metadata.

## Planning Flow

1. SQL planning routes through `SqlQueryPlanner`.
2. InfluxQL first rewrites statements into a constrained single-database/single-retention-policy form.
3. The rewritten logical plan is turned into a physical plan by the underlying DataFusion-based context.
4. Query executor asks the write buffer and persisted-file layers for chunks that satisfy the filter and table request.

## How InfluxDB Uses Indexes and Metadata

The important “index” structures are not general-purpose secondary AMs. They are:

- table index snapshots
- table index caches
- persisted-file metadata
- chunk statistics
- distinct and last-value caches
- time/partition organization

QueryableBuffer returns in-memory chunks with statistics. Persisted-file layers add parquet-backed chunks. Query filters are turned into chunk filters so entire chunks or files can be skipped before scan.

## Consistency and Publication

InfluxDB uses:

- persisted snapshots
- snapshot sequence numbers
- buffer plus persisted-file union
- table index snapshots that can add or remove files over time

So the donor value is:

- chunk/file publication and pruning
- metadata-first filtering
- unifying in-memory and persisted data in query planning

## What ScratchBird Should Borrow

- Chunk-level statistics and pruning as a native planning surface
- Query rewrite before planning when dialect-specific semantics need normalization
- Buffer plus persisted-data union planning instead of pretending only one storage layer exists
- Cache-backed special accelerators for common workloads such as last/distinct

## ScratchBird Comparison Hooks

- Compare ScratchBird summary, columnstore, and time-series chunk planning to InfluxDB’s chunk-filter and table-index model.
- Use InfluxDB as a donor for metadata pruning and hybrid buffer/persisted planning, not for broad secondary-index family competition.

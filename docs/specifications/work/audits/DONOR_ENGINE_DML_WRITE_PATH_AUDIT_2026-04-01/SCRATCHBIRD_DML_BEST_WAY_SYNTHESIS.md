# ScratchBird DML Best-Way Synthesis

## Executive Conclusion

ScratchBird should not pick one donor. The strongest path is a hybrid:

- Firebird for visibility ownership, savepoint/backout discipline, and exact index truth.
- PostgreSQL for selective update-time index suppression, AM-specific maintenance rules, and lossy-versus-exact index contracts.
- MySQL for cold-page secondary-index deferral and purge eligibility discipline.
- MongoDB for online index build workflow: scan, bulk build, intercept concurrent writes, drain, validate, publish.
- Cassandra, ClickHouse, Milvus, and InfluxDB for immutable-generation publication, background consolidation, and family-specific ingest lanes.
- DuckDB for checkpoint-time delta merge instead of forcing all index work into the hot commit path.
- OpenSearch for tiny atomic checkpoint metadata around a heavier mutable write surface.
- Neo4j for batched index-apply orchestration.
- Redis for adaptive memory layout and lazy foreground-free avoidance.

ScratchBird already has the harder foundation:

- non-destructive transaction lineage
- always-in-transaction execution
- cluster-native UUID identity
- sweep/archive capability
- multiple index families
- parser/emulation separation

Because that foundation already exists, ScratchBird does not need donor WAL truth, donor oplog truth, or donor LSM truth. It needs donor maintenance tactics around its own lineage truth.

## What The Donors Agree On

Across very different engines, the same write-path lessons repeat.

1. Foreground commit should do only the work that must be synchronous for correctness.
2. Expensive cleanup should be deferred behind a safe visibility horizon.
3. Index maintenance must be family-specific. Exact B-tree rules are not summary-index rules, and neither are ANN or inverted-index rules.
4. Bulk paths should not look like retail paths. Sorted/batched build and shadow publication are consistently faster and safer.
5. Publication is a first-class concept. Donors separate "constructed" from "visible" with a clear barrier.
6. Delete handling is cheapest when logical deletion is foreground and physical reclamation is deferred.
7. Metadata for crash recovery should be tiny, explicit, and cheap to validate.
8. Batching is used everywhere: row groups, parts, memtables, side-write drains, sync packs, and combined index-update jobs.

## Best-Way Concept Path For ScratchBird

### 1. Keep ScratchBird's row-version lineage as the only truth

ScratchBird should keep its non-destructive row/version chain as the single semantic authority for update, delete, replication, ETL, archive, and conflict avoidance. This is where Firebird is the right donor. PostgreSQL, MySQL, OpenSearch, and DuckDB all rely on auxiliary state to make writes fast, but ScratchBird should not move the truth out of its lineage model.

### 2. Add a HOT-like update path for exact families

For updates that do not change values referenced by exact tuple-addressing index families:

- do not create new exact secondary index entries
- keep the new version page-local when possible
- redirect or prune dead intermediate versions later

This is the clearest PostgreSQL lesson. ScratchBird's lineage model is already more powerful than PostgreSQL's tuple-chain model, so the missing optimization is not semantic support. The missing optimization is a deliberately cheap "same indexed value" fast path.

### 3. Introduce family-specific maintenance lanes

ScratchBird should stop thinking about "index maintenance" as one thing. It should split write handling into at least these lanes:

- exact tuple-aware families
- summary/range families
- inverted/text families
- ANN/vector families
- clustered/locality families

Required rule:

- exact families must remain transactionally exact against visible lineage
- summary families may lag and be repaired or summarized later
- ANN families should use generation publish, not per-row exact mutation on the hot path
- inverted families need pending-list or batch-drain behavior, not only retail insert

This is the combined lesson from PostgreSQL, ClickHouse, Cassandra, Milvus, MongoDB, and OpenSearch.

### 4. Add a cold-page secondary-index delta buffer

ScratchBird should adopt the MySQL idea, but in a more controlled way than classic change buffering:

- only for eligible non-unique exact secondary structures
- only when the target leaf/page is cold or absent
- record a tiny per-page delta entry instead of forcing immediate random I/O
- merge on page load, background maintenance, checkpoint, or sweep
- cap backlog hard and expose backlog metrics

This should not become a general-purpose hidden write cache. It should be a narrow, observable optimization for a specific pain point: random write amplification from cold exact secondary pages.

### 5. Use shadow-build, drain, validate, publish for large index work

ScratchBird should standardize MongoDB's hybrid-build lesson for all costly rebuild or initial-build flows:

1. scan or replay the source lineage
2. bulk-build a shadow structure
3. intercept concurrent writes into a side log
4. drain the side log in one or more passes
5. re-check remaining constraints under a short exclusive barrier
6. atomically publish the new generation

This should apply to:

- parser-emulated donor catalogs
- exact index rebuilds
- inverted/text rebuilds
- ANN/vector rebuilds
- summary-family regeneration

### 6. Adopt immutable-generation publication for heavy families

For summary, text, search, and vector families, ScratchBird should prefer immutable publish units:

- parts
- segments
- generations
- checkpoints

Foreground DML should append or buffer into the current mutable lane, and background workers should publish immutable generations after validation. This is the shared lesson from Cassandra SSTables, ClickHouse parts, Milvus growing-to-sealed segments, InfluxDB snapshot/parquet flow, and OpenSearch/Lucene segments.

### 7. Separate commit-critical work from maintenance-critical work

ScratchBird should draw a hard line:

- commit-critical: lineage row write, transaction stamp, exact uniqueness/constraint checks that must hold now, commit metadata
- maintenance-critical: page defragmentation, exact-family delta merge, summary refresh, ANN build, text posting consolidation, vacuum-like reclaim

Foreground work should be bounded. Maintenance work should be observable, resumable, and backlog-driven.

### 8. Add checkpoint-time delta merge

DuckDB's best idea here is not its checkpoint model itself. It is the willingness to keep an index delta during checkpoint and merge it back after the checkpoint succeeds. ScratchBird should use the same pattern:

- if background work is already publishing a durable checkpoint or generation boundary
- allow a temporary delta for affected index families
- merge that delta only after the durable boundary is safely installed

This reduces foreground stalls during heavy checkpoint or sweep windows.

### 9. Make delete reclamation explicitly horizon-driven

Deletes and obsolete versions should move through deterministic states:

1. logically deleted in visible lineage
2. no longer visible to any active horizon
3. eligible for exact-index cleanup
4. eligible for page reclaim
5. eligible for archive-only retention or physical retirement

Firebird and MySQL are the best donors for the correctness side. Cassandra, ClickHouse, and InfluxDB are the best donors for "background retirement is a first-class subsystem, not a side effect."

### 10. Batch index-apply work across transactions

Neo4j shows a simple but valuable lesson: even if each transaction logically produces its own index updates, the physical apply should often run in combined batches. ScratchBird should add:

- per-family update aggregators
- batched apply jobs
- optional single-writer batched modes for locality-friendly exact trees

This reduces lock churn, latch churn, and cache misses when many small transactions touch the same families.

### 11. Use tiny atomic recovery metadata

OpenSearch and DuckDB both reinforce the same rule: recovery metadata should be tiny and cheap to validate. ScratchBird should prefer:

- a small atomic checkpoint marker
- explicit generation IDs
- simple "completed versus incomplete" recovery decisions

ScratchBird does not need donor translogs as truth, but it should absolutely borrow the discipline of tiny atomic publication metadata.

### 12. Use adaptive in-memory representations

Redis is the outlier donor, but its lesson is still strong:

- small sets should not pay large-structure costs
- small hash-like payloads should stay compact until thresholds are crossed
- deletes should not always synchronously free everything

ScratchBird should use adaptive encodings in hot transient paths:

- pending exact-index deltas
- small side-write tables
- temporary duplicate/skipped-record trackers
- small in-memory posting lists

## Recommended ScratchBird Design Moves

### Priority 1: foreground DML

- Implement a HOT-like exact-family no-churn update path.
- Add per-family maintenance classification: exact, summary, inverted, ANN, clustered.
- Add batched exact-index apply queues.
- Add hard metrics for maintenance backlog by family.

### Priority 2: secondary maintenance

- Add a narrow cold-page secondary delta buffer for eligible exact secondaries.
- Make delete reclamation horizon-driven and observable.
- Add checkpoint-time delta merge for index families that can safely use it.

### Priority 3: build and rebuild workflows

- Standardize shadow-build, side-log, drain, validate, publish.
- Make immutable-generation publish the default for ANN, text, and summary families.
- Add validation and corruption-check tooling before publish.

### Priority 4: storage-shape optimization

- Introduce compact-versus-wide internal staging formats for small versus bulk insert batches.
- Add row-group/page-range summaries for families that benefit from pruning.
- Use adaptive in-memory encodings for temporary write-path structures.

## What ScratchBird Should Not Copy

- PostgreSQL WAL or visibility-map assumptions as platform truth.
- Firebird's tight monolithic ownership as the only extension model.
- MySQL redo and mini-transaction architecture as the core design.
- Cassandra compaction semantics as the visibility truth.
- ClickHouse or OpenSearch visibility timing as transactional commit timing.
- MongoDB oplog or replica-set internals as literal implementation.
- Redis single-threaded in-memory assumptions for base relational storage.

## Final Recommendation

The best ScratchBird write path is:

- Firebird-style lineage truth
- PostgreSQL-style exact-family update suppression
- MySQL-style cold-page secondary deferral
- MongoDB-style shadow-build and staged publish
- Cassandra/ClickHouse/Milvus/InfluxDB-style immutable-generation publication for heavy families
- DuckDB-style checkpoint delta merge
- OpenSearch-style tiny atomic checkpoint metadata
- Neo4j-style batched index-apply orchestration
- Redis-style compact transient encodings

That combination fits ScratchBird better than any single donor because it respects what ScratchBird already is: a clustered database environment with non-destructive lineage, not a traditional single-engine heap-plus-WAL system.

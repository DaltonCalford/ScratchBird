# Test Contract - 18_Index_Framework

## Added Tests
- savepoint rollback removes visible index effects while permitting deferred
  dead-candidate cleanup
- restart-normalized aborted transactions do not leave user-visible index
  matches
- online maintenance survives checkpoint interruption with exactly one legal
  visible generation
- same-key exact update suppression preserves one live exact key and still
  enforces uniqueness
- commit-group batch apply preserves commit order and fails atomically on
  exact-family apply errors
- cold-page exact-secondary delta buffering merges before the first read that
  requires the deferred locality
- GIN and ranked-text pending lanes expose one coherent read view across
  mutable and sealed layers
- shadow-build side-log drain and cutover guard survive restart and publish
  exactly one legal visible index generation
- heavy-family generation publication preserves old generations for older
  snapshots
- hot-leaf mitigation reduces right-edge contention on monotonic insert loads

## Gate Criteria
- Must pass required tests before advancing stage.

## Update 2026-03-28: audited current proof obligations

Current section `18` proof is centered on:
- registry-backed family exposure and capability lookup through `IndexFactory`
- `CatalogManager::createIndex(...)` for:
  - key columns
  - include columns
  - expression indexes
  - partial indexes
  - tablespace-aware root allocation
  - dependency registration
- executor fail-closed generic routing for unsupported or specialized families:
  - `BRIN` block-range only
  - `GIN` specialized operators only
  - `FULLTEXT` and related inverted families specialized text search only
  - `HNSW` and vector families k-NN only
  - `HASH` no generic range scan
  - `COLUMNSTORE` specialized scan only
- GC cleanup-family classification:
  - `BRIN` summary cleanup
  - `HNSW` approximate cleanup
  - all other current runtime classes exact cleanup by default

Still-open proof gaps:
- generic online or concurrent build behavior
- universal rebalance, relocate, and health-scan operator behavior

Beta 1 required proof additions from the new optimization authority:
- exact-family redirect versus stable-head selection
- reclaim-driven cleanup debt emission and page-local compaction
- durable `index_page_delta` merge and refusal rules
- durable `index_build_plan`, `index_build_progress`, and
  `index_build_cutover_guard` restart behavior
- immutable generation manifest publication and merge-debt handling

Update 2026-03-30:
- shared-backend named-family parity is now directly covered by lane-A
  create/open evidence plus lane-B planner and statistics proof, so `D05` is
  no longer an open Beta 1 parity gap

Beta 2 required proof additions:
- deterministic key normalization for every newly admitted Beta 2 datatype
- create-time refusal for disallowed family or datatype pairings
- scalar-path extraction proof for document, geo, and vector families that only
  become exact-indexable through extracted scalar expressions
- hash-only enforcement for opaque PostgreSQL catalog payload wrappers
- ANN-only enforcement for vector families and spatial-only enforcement for
  geo families
- parser and catalog admission proof for `SPATIAL`, `VECTOR`, `COLUMNAR`,
  `MONGODB_COLUMN`, `YBGIN`, `MILVUS_*`, and `CLICKHOUSE_*`
- named-family metrics packet publication for every Beta 2 emulation index
- `EXPLAIN` rendering proof that donor-visible family names survive shared
  runtime lowering
- `CLICKHOUSE_SET` granule overflow behavior and pruning proof
- `CLICKHOUSE_TOKENBF_V1` and `CLICKHOUSE_SPARSE_GRAMS` false-positive bounded
  filter proof
- `CLICKHOUSE_TEXT` three-stream persistence and merge proof
- `CLICKHOUSE_HYPOTHESIS` fail-closed predicate-shape proof
- `CLICKHOUSE_VECTOR_SIMILARITY` donor-parameter validation and ANN costing
  proof
- `YBGIN` donor access-method identity plus explicit no-fast-update boundary
  proof
- analytical segment metadata publication and pruning proof
- fixed vector quantum disclosure and late-materialization proof
- projection, summary, and cube accelerator competition proof
- acceleration freshness or coverage refusal proof
- columnstore implementation-closure proof for segment sealing, compaction
  debt, OLTP interference limits, and benchmark-governed refresh behavior

## Update 2026-03-28: contradiction-derived proof obligations

Section `18` now carries five explicit proof-obligation buckets:
- `D01` runtime-class and named-family authority
- `D02` planner taxonomy and exactness closure
- `D03` metadata authority closure
- `D04` maintenance boundary closure
- `D05` shared-backend family proof closure

No section `18` gate is complete while unresolved contradiction buckets remain
unless the affected canonical files keep the corresponding non-guarantees
explicit.

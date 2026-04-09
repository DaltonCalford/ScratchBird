# ScratchBird MGA DML DDL Index Optimization Audit

Status: preliminary_local_audit_refined_with_web_research

## Purpose

This audit converts the local donor-engine write-path findings and the April 1, 2026 vendor and paper research bundle into a ScratchBird-specific optimization program for:

- `INSERT`
- `UPDATE`
- `DELETE`
- index build, rebuild, and maintenance
- online `DDL`
- bulk ingest and background consolidation

The goal is not donor parity. The goal is to determine which techniques fit ScratchBird's MGA, lineage, parser boundary, and multi-family index framework, and to reject or narrow the ones that do not.

## Non-negotiable ScratchBird constraints

The following are treated as hard boundaries for every recommendation in this audit:

1. ScratchBird is always inside a transaction.
2. MGA row-version lineage is the truth.
3. Indexes remain candidate finders, not visibility authorities.
4. Cleanup is subordinate to heap reclaim legality.
5. Parser and wire-protocol behavior remain outside the engine.
6. `DDL` publishes through committed schema epochs, not out-of-band metadata flips.
7. Heavy-family speedups may not replace exact-family correctness rules.

## ScratchBird baseline already in place

The optimization program starts from real substrate that already exists in ScratchBird.

- broad index family registry and canonical runtime taxonomy already exist
- bounded online schema change classes already exist
- GIN pending-list machinery already exists
- shadow-index version metadata and promotion APIs already exist
- B-tree bulk loading already exists
- family-local dead-entry cleanup hooks already exist
- bloom rebuild and per-family maintenance hooks already exist
- columnar and ANN paths already perform MGA-aware visibility checks

## ScratchBird implementation anchors

Use these search-key anchors when discussing implementation changes:

- `docs/specifications/08_Transaction_Core/MGA_RECORD_STATE_AND_PUBLICATION_MODEL.md` search `Index cleanup hook rule`
- `docs/specifications/18_Index_Framework/INDEX_ARCHITECTURE.md` search `MGA-first rule`
- `docs/specifications/18_Index_Framework/INDEX_MGA_PUBLICATION_AND_RECLAIM.md` search `Publication order`
- `docs/specifications/18_Index_Framework/INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md` search `Canonical Runtime Matrix`
- `docs/specifications/37_Statistics_Metadata_and_Schema_DDL/ONLINE_SCHEMA_CHANGE_AND_BACKFILL_MODEL.md` search `Eligibility classes`
- `src/core/connection_context.cpp` search `beginNewTransaction(` and `Failed to persist transactional DDL lineage/schema epoch`
- `src/sblr/executor.cpp` search `classifySchemaChangeClassForSql(`
- `src/sblr/executor.cpp` search `// Both in index - delete old, insert new`
- `src/core/gin_index.cpp` search `GIN_PENDING_LIST_THRESHOLD`
- `src/core/btree.cpp` search `auto BTree::bulkLoad(`
- `src/core/catalog_manager.cpp` search `createShadowIndex(` and `promoteShadowIndex(`
- `src/core/hash_index.cpp` search `Status HashIndex::removeDeadEntries(`
- `src/core/columnstore.cpp` search `isValueVisible(`
- `src/core/hnsw_index.cpp` search `is_node_visible`

## What the local donor audit said before web refinement

The local donor-clone audit produced twelve durable lessons:

1. Firebird is the best donor for lineage-owned exactness, savepoint-aware backout, and cleanup legality.
2. PostgreSQL is the best donor for same-key update suppression, pending lanes, and access-method-specific contracts.
3. MySQL is the best donor for cold-page secondary deferral, but also the strongest warning about long-term maintenance complexity.
4. MongoDB is the best donor for online build and staged publish.
5. Cassandra, ClickHouse, Milvus, InfluxDB, and OpenSearch are the best donors for immutable publication units and background consolidation.
6. DuckDB is the best donor for checkpoint-bound reconciliation of temporary delta state.
7. Neo4j is the cleanest donor for combined index-apply work.
8. Redis is the best donor for compact transient structures and deferred free.

The web research mostly reinforced those conclusions, but it added stronger choices around:

- metadata-MVCC and asynchronous schema evolution
- invisible candidate indexes and resumable index operations
- automatic compaction and merge-debt scheduling
- monotonic-key hotspot mitigation
- latch-free or message-buffered tree alternatives for future phases

## Audit method

Each optimization below follows the same order:

1. local donor-only preliminary result
2. web-research refinement
3. ScratchBird fit assessment
4. step-by-step implementation shape
5. alternatives with pros and cons
6. recommendation

## OPT-01: Exact-family same-key update suppression

`Current ScratchBird state`: missing in the audited executor update path; the audited expression and partial path still removes old entries and inserts new ones.

`Local preliminary result`: PostgreSQL `HOT` is the clearest donor. Firebird confirms that version churn and exact index truth must remain under one owner.

`Web refinement`: BzTree and Bw-tree research strengthen the case for reducing exact-page churn, but they do not require replacing ScratchBird's page-based exact structures immediately.

`Fit with ScratchBird`: very high. ScratchBird already retains lineage and already treats indexes as candidates. That means the missing optimization is operational, not semantic.

`Recommended implementation`:

1. classify every indexed column reference by family into `exact`, `summary`, `inverted`, `ann`, or `other`
2. detect update statements where no `exact` indexed value changes
3. if the new visible head can remain page-local, preserve the old exact secondary candidate and create a redirect or stable-head continuation
4. if the new version cannot remain page-local, use stable-head indirection so the exact secondary entry can still avoid churn where safe
5. continue to propagate changes to summary families and any other families that summarize or seal by range
6. queue old-version retirement into exact cleanup debt rather than forcing immediate delete-plus-insert churn

`Alternative A`: stable-head exact entry

Pros:
- works even when the new row version moves off-page
- maps well to row UUID and lineage ownership
- cleaner for clustered replication and archive replay

Cons:
- every exact hit pays an extra level of indirection
- requires careful proof for unique conflicts against the current visible head

`Alternative B`: page-local HOT redirect chain

Pros:
- fastest hot-path read behavior
- lowest write amplification when the row stays on-page
- aligns with PostgreSQL's strongest proven tactic

Cons:
- degrades once updates spill across pages
- requires more aggressive page-local pruning logic

`Recommendation`: use a hybrid. Prefer page-local redirect chains when the new version fits on the same page; fall back to stable-head exact entries when it does not. This gives ScratchBird a broader optimization envelope than PostgreSQL without weakening MGA.

## OPT-02: Exact cleanup queues and reclaim-driven compaction

`Current ScratchBird state`: partial. Family-local dead-entry cleanup exists, and B-tree and hash families already rebuild or compact structures during cleanup.

`Local preliminary result`: Firebird, PostgreSQL, and MySQL all agree that exact cleanup must wait for reclaim legality and should not be paid entirely on the foreground path.

`Web refinement`: Azure SQL automatic index compaction adds a useful scheduler model: act on recently modified pages with low overhead instead of rebuilding whole structures.

`Fit with ScratchBird`: very high. This is a direct extension of the existing MGA reclaim rules.

`Recommended implementation`:

1. emit reclaim-proof work items keyed by `logical_index_id`, page or bucket identity, and reclaim horizon
2. group work by family and physical locality
3. run cleanup bottom-up on the affected page or bucket only
4. compact the page or posting structure if reclaim leaves meaningful empty space
5. refresh bloom filters, page-density metrics, and cleanup-debt counters
6. leave pages marked `repair_required` or `rebuild_required` if structural proof fails during cleanup

`Alternative A`: sweep-only cleanup

Pros:
- simplest implementation
- lowest foreground disruption

Cons:
- cleanup debt can spike badly
- misses locality opportunities
- allows write amplification and bloat to grow longer than necessary

`Alternative B`: opportunistic on-read cleanup

Pros:
- exploits locality immediately
- reduces future debt on hot structures

Cons:
- adds tail latency to reads
- harder to reason about fairness

`Alternative C`: background debt queues

Pros:
- best control over backlog and scheduling
- aligns with current MGA reclaim gates
- easiest to observe and tune

Cons:
- needs one more scheduler and metrics surface

`Recommendation`: background debt queues should be the primary path. Opportunistic on-read cleanup can be used only as a bounded assist for obviously safe micro-compaction.

## OPT-03: Narrow cold-page secondary delta buffer

`Current ScratchBird state`: missing.

`Local preliminary result`: MySQL change buffering showed the right pain point to attack: random reads of cold secondary pages for non-unique exact secondaries. MariaDB showed the danger of letting that mechanism become too broad and too permanent.

`Web refinement`: TokuDB fractal-tree indexing shows the more aggressive alternative, where updates are message-buffered inside the tree itself. That is useful as a comparison point, but too invasive for a first pass.

`Fit with ScratchBird`: medium to high if tightly scoped.

`Recommended implementation`:

1. admit the optimization only for non-unique exact secondary families
2. trigger it only when the target leaf or bucket is cold or absent from the buffer pool
3. write a small durable delta record keyed by logical index identity plus target page fence
4. merge the delta on page read, background maintenance, checkpoint, or reclaim-driven sweep
5. expose backlog size, merge age, and merge failure counters
6. fail closed by falling back to ordinary foreground maintenance whenever the buffer or page-state proof is ambiguous

`Alternative A`: classic per-page delta buffer

Pros:
- narrowly solves cold-page random I/O
- easiest to graft onto current exact structures

Cons:
- still needs careful crash and merge proof
- can become hard to reason about if allowed for too many families

`Alternative B`: write-optimized message-buffer tree

Pros:
- stronger write-amplification reduction
- better for sustained heavy-write workloads

Cons:
- much larger architectural change
- far harder to align with MGA exactness and uniqueness

`Recommendation`: ship Alternative A only, with hard scope limits, hard backlog limits, and explicit willingness to remove it if the ongoing correctness cost gets too high.

## OPT-04: Pending lanes for inverted and many-key families

`Current ScratchBird state`: partial. GIN pending-list machinery already exists.

`Local preliminary result`: PostgreSQL GIN and MongoDB bulk-build flows both show that many-key-per-row families should not pay retail insertion cost on every write.

`Web refinement`: OpenSearch and Lucene reinforce the need to keep mutable write buffers separate from published read segments for ranked search. SAP HANA's delta-merge model reinforces that the mutable lane should be explicitly merged, not forgotten.

`Fit with ScratchBird`: very high.

`Recommended implementation`:

1. keep `GIN_TEXT` as a mutable pending-list plus main-tree boolean family
2. treat ranked `INVERTED_TEXT` as immutable mini-segments plus periodic merge
3. make reads evaluate one coherent view across mutable and sealed layers
4. merge based on term count, pending-byte budget, or read-side penalty
5. route delete and update compensation through the same pending lane rather than forcing full-segment rewrite

`Alternative A`: immediate retail insert into the published structure

Pros:
- simplest correctness surface

Cons:
- worst write amplification
- poor fit for many keys per row

`Alternative B`: one shared pending lane for all text families

Pros:
- simpler implementation

Cons:
- boolean and ranked retrieval have different publication and scoring needs

`Alternative C`: split boolean and ranked families

Pros:
- aligns physical design to family semantics
- keeps the boolean path simple and the ranked path scalable

Cons:
- more code paths

`Recommendation`: use Alternative C. ScratchBird's current family taxonomy already supports it.

## OPT-05: Standardized shadow-build, side-log, validate, publish

`Current ScratchBird state`: partial. Shadow-index version metadata, visibility windows, and create or promote APIs already exist, but a universal online build contract is still bounded and not fully closed.

`Local preliminary result`: MongoDB is the clearest donor for scan, bulk build, intercept concurrent writes, drain, validate, and publish.

`Web refinement`: SQL Server resumable index operations and MongoDB resumable phases both show that restart-safe state capture is worth standardizing. Oracle automatic indexing adds a strong publish discipline: candidate structures stay invisible until they prove value.

`Fit with ScratchBird`: very high.

`Recommended implementation`:

1. create the shadow index or generation in `BUILDING`
2. perform a sorted lineage scan or family-native bulk build
3. intercept concurrent writes into a side log keyed by logical index identity
4. drain side-log work in at least two phases: permissive drain and final exclusive drain
5. record durable validation and cutover-guard state
6. publish by setting `valid_from_xid` on the new version and `retired_xid` on the old version
7. leave the old version queryable for older transactions until retirement proof is complete
8. support pause, resume, and restart from durable phase state

`Alternative A`: offline rebuild

Pros:
- easiest correctness proof

Cons:
- unacceptable for the long term
- leaves DDL and maintenance windows too expensive

`Alternative B`: online shadow-build without resumability

Pros:
- better than offline
- simpler than full resumability

Cons:
- throws away large build progress on failure

`Alternative C`: resumable shadow-build with side log

Pros:
- best fit for large exact, ANN, and text rebuilds
- aligns with existing shadow-index metadata

Cons:
- needs durable phase bookkeeping

`Recommendation`: Alternative C should become the canonical model for costly build and rebuild work.

## OPT-06: Immutable generation publication for heavy families

`Current ScratchBird state`: partial. The family taxonomy already distinguishes immutable-generation families from mutable exact families, but the implementation surface is still uneven.

`Local preliminary result`: Cassandra, ClickHouse, Milvus, InfluxDB, DuckDB, and OpenSearch all prove that heavy families should publish in validated units, not by constantly mutating the read surface.

`Web refinement`: SAP HANA adds the strongest merge-policy lesson: delta merge should be cost-based, because merge work itself is expensive.

`Fit with ScratchBird`: high for `BRIN`, `COLUMNSTORE`, ranked text, vector families, and any summary or filter family.

`Recommended implementation`:

1. keep a mutable write lane for each heavy family
2. seal mutable state by policy: bytes, rows, time, or memory pressure
3. build one immutable generation or part
4. validate generation-local metadata and corruption checks before publish
5. flip a tiny durable publication marker
6. retire older generations only after visibility and dependency proof
7. schedule merges from explicit debt counters instead of one fixed cadence

`Alternative A`: fully mutable heavy-family structures

Pros:
- simpler query view

Cons:
- poor sustained write behavior
- expensive foreground maintenance

`Alternative B`: immutable micro-generations plus merge

Pros:
- best sustained ingest behavior
- clean restart and validation boundaries

Cons:
- requires merge scheduling and read fan-in control

`Recommendation`: Alternative B is the correct long-term model for heavy families.

## OPT-07: Checkpoint or sweep-time delta merge with tiny publication markers

`Current ScratchBird state`: partial. ScratchBird already has small shadow or checkpoint metadata patterns and family-local bloom rebuild hooks, but no unified delta-after-boundary contract.

`Local preliminary result`: DuckDB proved that temporary index delta can be reconciled after the durable boundary instead of before it.

`Web refinement`: SQL Server ADR and OpenSearch checkpoint practices reinforce the value of very small durable progress markers rather than large recovery logs for maintenance state.

`Fit with ScratchBird`: high if limited to non-commit-critical structures.

`Recommended implementation`:

1. identify maintenance work that is expensive but not commit-critical
2. allow that work to accumulate in a temporary delta structure during checkpoint or sweep windows
3. publish a tiny durable marker describing whether the new boundary completed
4. after the boundary is durable, merge the delta back into the main family structure
5. if restart finds an incomplete boundary, discard or replay only the family-local delta, never infer completion

`Alternative A`: block boundary advancement until maintenance is fully applied

Pros:
- simplest reasoning

Cons:
- worst latency spikes

`Alternative B`: boundary-first then delta merge

Pros:
- smoother checkpoint or sweep
- better control of long maintenance debt

Cons:
- requires one more restart contract

`Recommendation`: use Alternative B only for non-unique, non-commit-critical family work. Do not use it for uniqueness proof or exact foreground visibility.

## OPT-08: Batched index-apply and locality-aware writers

`Current ScratchBird state`: missing as a generic contract.

`Local preliminary result`: Neo4j's `IndexUpdatesWorkSync` is the cleanest donor. It explicitly combines index updates from multiple transactions into one larger apply job.

`Web refinement`: monotonic-key contention guidance and latch-free research make the value of locality-aware combined apply even clearer.

`Fit with ScratchBird`: very high.

`Recommended implementation`:

1. capture per-transaction index deltas by family and logical index identity
2. at commit-group time, coalesce deltas by family and physical locality
3. sort exact-family deltas by target leaf or bucket
4. apply one combined job for contiguous work where correctness requires commit-bound maintenance
5. use asynchronous combined apply only for lag-tolerant heavy families
6. expose queue depth, combined-job size, and apply stall metrics

`Alternative A`: fully synchronous per-transaction apply

Pros:
- trivial semantics

Cons:
- worst cache and latch behavior

`Alternative B`: commit-group combined apply

Pros:
- preserves exact correctness
- lowers repeated page churn

Cons:
- needs careful integration with commit publication

`Alternative C`: async combined apply for all families

Pros:
- best raw throughput

Cons:
- violates immediate exact-family expectations

`Recommendation`: use Alternative B for exact families and Alternative C only for explicitly lag-tolerant families.

## OPT-09: Hot-leaf and monotonic-key contention mitigation

`Current ScratchBird state`: missing as a visible policy.

`Local preliminary result`: local donor clones did not give one dominant answer, but several engines implicitly avoid pounding one exact page forever.

`Web refinement`: SQL Server's last-page `PAGELATCH_EX` guidance makes the problem explicit. BzTree and Bw-tree show the structural replacement alternative.

`Fit with ScratchBird`: medium to high.

`Recommended implementation`:

1. detect hot-leaf pressure from page-latch retries, split frequency, and queue depth
2. pre-split or reserve rightmost-leaf slack for monotonic workloads
3. prefer batch writer mode on hot contiguous ranges
4. optionally bucket purely synthetic monotonic keys when no ordered semantics depend on a single global tail
5. keep latch-free tree replacement as a future investigation, not a phase-one prerequisite

`Alternative A`: page-based B-tree plus hot-leaf mitigation

Pros:
- least disruptive
- reuses current exact-tree substrate

Cons:
- not the maximum theoretical concurrency

`Alternative B`: latch-free delta-chain tree

Pros:
- stronger write concurrency ceiling
- good future path if hot-leaf pressure dominates

Cons:
- high rewrite cost
- harder recovery and maintenance model

`Recommendation`: use Alternative A first. Re-evaluate Alternative B only after the batch-apply and same-key suppression changes land.

## OPT-10: Direct-path bulk ingest lanes

`Current ScratchBird state`: partial. B-tree bulk loading exists, but the broader bulk-load and `COPY` execution contract is still target-state only.

`Local preliminary result`: ClickHouse, Cassandra, and InfluxDB all show the value of append-friendly publish units. DuckDB shows the value of locality-preserving batch write. The current ScratchBird B-tree bulk loader already proves part of this path.

`Web refinement`: Oracle direct-path insert reinforces that bulk and retail paths should be different. However, donor `NOLOGGING` behavior is not acceptable as ScratchBird truth.

`Fit with ScratchBird`: high.

`Recommended implementation`:

1. define three ingest lanes:
2. `retail micro-batch` for ordinary OLTP inserts
3. `sorted exact bulk` for large exact index builds or `COPY`-style load
4. `shadow load then cutover` for table-scale transformations
5. stamp lineage, UUID identity, and transactional ownership in every lane
6. build exact structures in sorted order where possible
7. build heavy families as immutable generations and publish them after validation

`Alternative A`: retail insert loop only

Pros:
- simplest execution surface

Cons:
- poor large-load behavior

`Alternative B`: sorted bulk exact load

Pros:
- immediate benefit for B-tree and related exact families

Cons:
- requires dedicated planner and executor path

`Alternative C`: shadow load and cutover

Pros:
- best for transformative or very large loads

Cons:
- highest orchestration cost

`Recommendation`: ship both Alternative B and Alternative C. The exact choice should depend on whether the operation is additive or transformative.

## OPT-11: Online DDL, metadata MVCC, invisible indexes, and resumable backfill

`Current ScratchBird state`: partial but stronger than the previous audit assumed. ScratchBird already has schema-epoch publication, plan rows, cutover guards, and bounded `METADATA_ONLY` plus `EXPAND_BACKFILL_CUTOVER` classes.

`Local preliminary result`: MongoDB shadow publish and donor rebuild workflows suggested the shape, but the local donor-clone audit alone did not fully answer the metadata-versioning question.

`Web refinement`: CockroachDB, F1, and MD-MVCC show the same durable lesson: online schema evolution works best when the system is explicit about intermediate states, versioned metadata, and resumable backfill. Oracle automatic indexing adds a valuable `invisible first` rule.

`Fit with ScratchBird`: very high because the schema-epoch substrate already exists.

`Recommended implementation`:

1. preserve the existing three-class model: `METADATA_ONLY`, `EXPAND_BACKFILL_CUTOVER`, `REWRITE_REQUIRED`
2. widen the `METADATA_ONLY` set only when stored-row shape truly does not change
3. keep backfill progress durable and resumable from row UUID or deterministic key anchors
4. bind compiled work to committed schema epoch plus local overlay
5. add invisible or candidate index states for advisor-driven testing and phased adoption
6. support pause and resume for long backfills and long shadow-index builds
7. use one shared cutover-guard and validation-manifest contract across both schema change and index publication work

`Alternative A`: keep current bounded Beta1 surface unchanged

Pros:
- safest short-term

Cons:
- leaves too many operational wins unavailable

`Alternative B`: expand using existing schema-epoch substrate

Pros:
- strongest fit to current implementation
- avoids a second metadata model

Cons:
- still needs careful phased proof per new operation family

`Alternative C`: replace current model with donor-style async DDL machinery

Pros:
- none strong enough to justify replacement

Cons:
- unnecessary churn
- risks violating current transactional DDL guarantees

`Recommendation`: Alternative B only.

## OPT-12: Debt-driven maintenance scheduler, auto compaction, adaptive transient structures

`Current ScratchBird state`: partial. There are rebuild hooks, metrics hooks, and family-local maintenance paths, but no single visible debt scheduler for all write-path debt classes.

`Local preliminary result`: OpenSearch, Redis, and donor background-maintenance engines all argued for explicit backlog handling and compact temp structures.

`Web refinement`: Azure SQL automatic compaction, SAP HANA delta merge, and Oracle automatic indexing all push toward the same operational rule: maintenance should be threshold-driven, load-aware, and evidence-backed.

`Fit with ScratchBird`: very high.

`Recommended implementation`:

1. create one maintenance debt ledger keyed by family, logical index, table, and shard or partition
2. track at minimum:
3. pending exact cleanup bytes
4. pending inverted keys
5. pending generation merges
6. pending side-log drain bytes
7. page density and fragmentation debt
8. hot-leaf contention debt
9. schedule work from thresholds and workload budgets, not one static cadence
10. keep temporary structures compact at small sizes, then widen only after thresholds are crossed
11. use lazy free for large transient-memory teardown where foreground stalls would be visible

`Alternative A`: periodic maintenance only

Pros:
- simple

Cons:
- blind to workload and debt shape

`Alternative B`: debt-driven scheduler

Pros:
- best sustained operational behavior
- easiest to make observable

Cons:
- requires richer metrics and admission policy

`Recommendation`: Alternative B.

## Structural alternatives for future investigation

These are real options, but they should not block the first optimization wave.

### Page-based exact trees plus targeted deferral

Pros:
- best fit to current ScratchBird implementation
- easiest MGA proof
- leverages existing B-tree, hash, cleanup, and shadow-index code

Cons:
- may not reach the absolute top concurrency ceiling of more radical trees

### Latch-free exact trees such as Bw-tree or BzTree

Pros:
- better ceiling under intense concurrency
- attractive if rightmost-leaf pressure remains dominant after earlier fixes

Cons:
- high rewrite cost
- more difficult maintenance, recovery, and observability story

### Message-buffered trees such as fractal-tree style designs

Pros:
- excellent sustained write behavior
- naturally reduces random I/O

Cons:
- easiest to over-apply
- harder to align with exact uniqueness and MGA reclaim rules

`Recommendation`: stay on page-based exact trees for now. Add the latch-free and message-buffered options to a later empirical investigation only after the first wave is benchmarked.

## Priority order

`P0`:

1. exact-family same-key suppression
2. exact cleanup debt queues
3. standardized shadow-build and resumable publish
4. immutable heavy-family publication
5. batched index-apply
6. online DDL expansion on top of current schema-epoch substrate

`P1`:

1. narrow cold-page secondary delta buffer
2. hot-leaf mitigation
3. direct-path bulk ingest lanes
4. debt-driven maintenance scheduler and adaptive transient structures

`P2`:

1. checkpoint or sweep-time delta merge for non-commit-critical family work
2. structural replacement studies for exact trees

## Final opinion

The highest-value choices are the ones that use ScratchBird's current strengths rather than trying to replace them.

- ScratchBird should become faster by doing less exact-index churn, not by weakening MGA.
- ScratchBird should do more work in staged publish and resumable background lanes, not by moving truth into a donor WAL or translog model.
- ScratchBird should expand the current schema-epoch and shadow-index substrate, not replace it.
- ScratchBird should treat exact, summary, text, and ANN families as different maintenance classes, not as one generic "index" behavior.

If only one short list is taken from this audit, it should be:

1. implement same-key exact update suppression
2. add commit-group batch apply
3. standardize side-log shadow-build and resumable publish
4. make heavy families generation-published by default
5. extend online DDL and invisible-index support on the current schema-epoch model

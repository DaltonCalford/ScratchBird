# ScratchBird Index Lane F Findings

Lane: F

Topic: MGA lifecycle and garbage-collection correctness across all index families

Status: First-pass findings

Date: 2026-03-14

## 1. Scope and Lane Objective

Lane F defines the MGA, publication, retirement, and garbage-collection contract that must hold across every ScratchBird index family, not only the ordered families covered in Batch 1.

This lane treats the following as in scope:

- mutable exact and range families: `B-tree`, `Hash`
- write-optimized and generational families: `LSM`, `Columnstore`
- summary and lossy pruning families: `BRIN`, bitmap-family structures
- generalized and spatial families: `GiST`, `SP-GiST`, `R-tree`
- inverted and text families: `GIN`
- ANN and vector families: `HNSW`
- catalog-level shadow build, promotion, retirement, and physical reclaim

This lane does not restate operator semantics, recheck semantics, or family access rules already established in Batch 1 except where those rules interact directly with lifecycle correctness.

The objective is to produce one implementation-oriented contract for:

- what makes an index entry, page, segment, posting list, graph node, or full generation visible
- when logical deletion is allowed
- when physical reclamation is allowed
- how shadow build and publish interact with long-lived snapshots
- which optimizer metrics and costing penalties must reflect lifecycle debt

Primary conclusion:

- ScratchBird already has the beginnings of the right model, including `logical_index_id`, `valid_from_xid`, `retired_xid`, shadow promotion, per-entry `xmin` and `xmax` fields in many families, and exported reclaim horizons in `TransactionManager`
- correctness is currently weakened by family-local GC rules that mix `current_xid`, `oldest active`, and `oldest interesting` tests without a single engine-level reclaim contract

## 2. ScratchBird Current-State Baseline

### 2.1 Control-plane baseline

ScratchBird already has an index lifecycle control plane in `CatalogManager`.

Observed baseline:

- catalog metadata exposes `logical_index_id`, `state`, `valid_from_xid`, and `retired_xid`
- `getVisibleIndexVersion` already implements generation visibility with the rule:
  `visible(g, reader_xid) := g.valid_from_xid <= reader_xid AND (g.retired_xid = 0 OR reader_xid < g.retired_xid)`
- `createShadowIndex` and `promoteShadowIndex` already implement shadow-build cutover
- `promoteShadowIndex` retires the previously active generation at the current transaction XID
- `gcRetiredIndexes` physically drops retired generations when `retired_xid < getOldestXid()`

Implication:

- ScratchBird already behaves like a generation-publishing system at the catalog layer
- the gap is that runtime family GC does not consistently use the same retirement horizon as the catalog

### 2.2 Transaction and GC baseline

`TransactionManager::captureReclaimHorizons` already exports the ingredients needed for a correct index reclaim contract:

- `oldest_interesting_xid`
- `oldest_active_xid`
- `oldest_snapshot_xid`
- `heap_reclaim_horizon`
- `toast_reclaim_horizon`

`GarbageCollector` already passes lifecycle-aware dead-entry candidates through `removeDeadEntriesWithLifecycle`, which means the engine has a natural choke point for unifying reclaim behavior.

Current weakness:

- the engine exports horizon data centrally, but family implementations do not consume one common index horizon

### 2.3 Batch 1 carry-forward baseline

Completed Batch 1 findings already established the family capability surface that Lane F must preserve:

- Lane A: ordered exact and range families require MGA-safe reclaim and family-aware planner contracts
- Lane B: summary, bitmap, and columnar families need publish-by-generation and delayed reclaim after snapshot drain
- Lane C: generalized and spatial families need lower-bound correctness, recheck discipline, and generation-safe build/publish
- Lane D: inverted and text families need pending-state visibility, segment or posting lifecycle rules, and deferred physical cleanup

Lane F therefore treats lifecycle as a cross-lane contract, not a family-local cleanup detail.

### 2.4 Family-by-family current-state findings

`B-tree`

- stores `xmin` and `xmax` per entry and uses transaction visibility for reads
- `gcCompact` currently evaluates reclaim using `current_xid`, not a captured global reclaim horizon
- page compaction is therefore not provably safe under long-lived snapshots

`Hash`

- stores `he_xmin` and `he_xmax`
- read path applies visibility filtering
- `gcCompact` can physically remove any delete-marked entry or invalid TID without checking a safe reclaim horizon
- Bloom rebuild logic uses a different rule based on `oldest interesting`
- current cleanup semantics are internally inconsistent

`BRIN`

- summary ranges can be marked dead and physically removed
- current GC compares `brn_xmax` against `oldest_active_xid`
- summary replacement and summary removal are not yet governed by one engine-wide publication contract

`Bitmap`

- runtime stores visibility metadata per bitmap entry and applies transaction visibility on reads
- lifecycle behavior is stronger than older comments imply, but reclaim rules are not yet specified as a family contract

`GiST`

- leaf and internal entries carry version metadata
- cleanup already uses a dead-TID check plus an `oldest interesting` style horizon
- this is one of the closest families to the desired contract, but it still uses family-local horizon selection

`SP-GiST`

- leaves carry version metadata and the read path honors visibility
- dead-entry cleanup rewrites leaf pages based on dead TIDs without a matching reclaim-horizon test
- current rewrite logic can outrun the oldest reader

`R-tree`

- delete path is logical first and comments expect later physical removal
- one cleanup path uses an `oldest interesting` style test
- `gcCompact` currently delegates to a full clear of the tree, which is not a valid online GC behavior

`GIN`

- keeps a pending list plus main posting structure
- searches already pay the pending-list tax
- `gcCompact` is effectively a no-op while `removeDeadEntries` does the real cleanup work
- the family needs an explicit split between pending-list maintenance, posting cleanup, and generation publication

`HNSW`

- nodes are logically deleted with `node_xmax`
- searches filter by node visibility
- cleanup marks garbage but does not yet provide safe physical reclaim, link repair, or generation replacement

`LSM`

- read path merges memtables and immutable runs with simplified XID tests
- in-memory compaction eagerly drops tombstones and deleted entries
- on-disk compaction uses an `oldest interesting` style horizon
- memtable and SSTable compaction do not yet share one MGA-safe reclaim rule

`Columnstore`

- value and segment metadata already carry visibility information and generation hints
- the simple index variant already uses dual meta pages and generation selection
- GC compaction remains incomplete and the reclaim contract is not yet unified with catalog publication

Planner baseline:

- the optimizer still treats only `B-tree` and `LSM` as first-class base access paths
- lifecycle debt is not currently represented in statistics or costing

## 3. Donor-Engine Research Synthesis

### Firebird

Firebird is the strongest donor for MGA-safe reclaim semantics.

Takeaways:

- garbage collection is anchored to the oldest active snapshot, not the worker's current transaction
- once the oldest active snapshot can see a version, older backversions can be removed
- intermediate versions can also be pruned when adjacent versions share the same oldest-visible snapshot boundary
- index creation and background maintenance are allowed to participate in GC work

ScratchBird implication:

- adopt Firebird's rule that reclaim legality is determined by the oldest reader that could still need the version, not by whichever worker happens to be compacting a page

### PostgreSQL

PostgreSQL is the strongest donor for family-specific cleanup under one engine maintenance framework.

Takeaways:

- core access methods expose common maintenance hooks such as bulk delete and vacuum cleanup
- `GIN` separates write-optimized pending state from the main searchable structure and accepts search overhead until maintenance catches up
- `BRIN` treats summarization as asynchronous maintenance and allows ranges to remain unsummarized until vacuum or targeted summarization runs

ScratchBird implication:

- adopt one engine-level lifecycle interface but let each family implement family-specific cleanup, summarization, and rewrite behavior behind that interface

### MySQL

MySQL InnoDB is the strongest donor for delete-mark now, purge later.

Takeaways:

- clustered and secondary index entries are delete-marked before physical removal
- purge operates against a safe lower-bound read view, not foreground query state
- secondary index removal is gated on the clustered record no longer having any visible non-deleted version that still depends on the secondary entry

ScratchBird implication:

- adapt the delete-mark plus purge-view pattern for mutable families, with heap or owning-record validation required before physical unlink

### Cassandra Storage-Attached Indexing

Cassandra SAI is the strongest donor for immutable view publication across compaction.

Takeaways:

- readers consume an atomic index view
- compaction replaces old SSTable-backed index components by installing a new immutable view
- obsolete structures are released only after the new view is installed

ScratchBird implication:

- immutable or segment-backed families should publish by atomic generation swap, never by in-place partial mutation that lets readers see a torn view

### OpenSearch

OpenSearch is a useful donor for publish-after-refresh and metadata-gated segment visibility.

Takeaways:

- refresh is the point at which persisted changes become visible to search
- old lookup maps are dropped only after a searcher can see the new state
- uploaded segment files are not query-visible until the corresponding metadata publication step completes

ScratchBird implication:

- generation visibility must be metadata-gated, and reclaim must wait until the replacement generation is actually query-visible

### Milvus

Milvus is a useful donor for snapshot-safe sealed-segment rules.

Takeaways:

- point-in-time snapshots capture sealed segments and their indexes
- growing segments are excluded until they are flushed and sealed
- restore can reuse sealed segment and index artifacts without rebuilding from logical rows

ScratchBird implication:

- segment-oriented families should distinguish mutable ingest state from published, sealed, queryable generations

## 4. Primary Literature and Official-Document Synthesis

The local official documentation across donors converges on a small set of lifecycle laws.

### 4.1 Visibility law

Visibility is monotonic with respect to a snapshot boundary:

- if snapshot `S` can see version `V`, any newer snapshot can also see `V`
- therefore reclaim must key off the oldest snapshot that can still see `V`

This is the core MGA rule that ScratchBird must centralize.

### 4.2 Publication law

Published state and built state are not the same:

- a structure may be physically present but not yet query-visible
- query visibility begins only after an explicit publish event such as refresh, metadata swap, or sealed-segment registration

This directly supports ScratchBird's existing `valid_from_xid` and shadow-promotion model.

### 4.3 Reclaim law

Logical death and physical death are different phases:

- mutable families first mark entries deleted
- immutable or segment families first retire the generation
- physical reclaim occurs only after the oldest reader boundary moves past the delete or retire point

### 4.4 Maintenance debt law

Deferred maintenance is correct but not free:

- pending lists, unsummarized ranges, dead postings, dead graph nodes, and stale generations all translate into search overhead and memory or storage debt
- therefore lifecycle state must be visible to the optimizer and observability layer

### 4.5 Derived rule set for ScratchBird

From the donor documents and Batch 1 findings, the correct first-pass rule set is:

- one publication boundary per queryable index generation
- one reclaim horizon shared across all families, with family-specific dependency refinements
- no physical reclaim based on `current_xid`
- no generation drop until catalog, reader, and family-specific dependencies all drain
- no in-place graph or summary rewrite that can expose a partially reclaimed structure to concurrent readers

## 5. MGA and Lifecycle Correctness Packet

### 5.1 Engine-wide horizon formulas

Lane F recommends the following engine-level symbols:

- `X_now`: current transaction XID at the time maintenance begins
- `H_active`: `oldest_active_xid`, with `X_now` used if no older active transaction exists
- `H_snapshot`: `oldest_snapshot_xid`, with `X_now` used if no older statement or snapshot reader exists
- `H_heap`: `heap_reclaim_horizon`
- `H_catalog`: `min(H_active, H_snapshot)`
- `H_family(f)`: additional family dependency horizon, defaulting to `X_now` when the family has no stronger requirement

Recommended index reclaim horizon:

`H_index_reclaim(f) := min(H_heap, H_catalog, H_family(f))`

Interpretation:

- `H_index_reclaim` is the oldest XID boundary that no active reader or dependent structure can cross safely
- no index artifact retired or delete-marked at XID `R` may be physically removed unless `R < H_index_reclaim(f)`

This replaces the current mix of `current_xid`, `oldest_active_xid`, and `getOldestXid()` calls inside family-local GC routines.

### 5.2 Visibility formulas

Mutable entry visibility:

`visible(entry, reader) := create_visible(entry.xmin, reader) AND NOT delete_visible(entry.xmax, reader)`

Mutable entry reclaimability:

`reclaimable(entry, f) := entry.xmax != 0 AND entry.xmax < H_index_reclaim(f) AND owner_confirms_dead(entry)`

Where:

- `owner_confirms_dead(entry)` means heap, row-version, segment, or posting-owner validation proves no visible logical owner still depends on the entry

Generation visibility using existing catalog fields:

`visible(generation, reader_xid) := generation.valid_from_xid <= reader_xid AND (generation.retired_xid = 0 OR reader_xid < generation.retired_xid)`

Generation reclaimability:

`reclaimable(generation, f) := generation.retired_xid != 0 AND generation.retired_xid < H_index_reclaim(f) AND dependents_drained(generation, f)`

Summary artifact reclaimability:

`reclaimable(summary, f) := summary.retired_xid < H_index_reclaim(f) AND (replacement_summary_published(summary.interval) OR source_interval_proven_empty(summary.interval))`

Graph-node reclaimability:

`reclaimable(node, HNSW) := node.xmax != 0 AND node.xmax < H_index_reclaim(HNSW) AND replacement_graph_or_link_repair_published(node)`

### 5.3 Lifecycle states

ScratchBird can keep the current catalog enum in the short term, but the contract needs the following conceptual states:

- `BUILDING`: structure exists but must not answer user queries
- `VALIDATING`: build complete enough for structural checks, still not query-visible
- `PUBLISHED`: metadata cutover complete; new readers may use it
- `RETIRING`: successor generation is published; older readers may still need the old one
- `RECLAIMABLE`: reader and dependency horizons have moved past the retire point
- `FAILED`: build or validation failed and the artifact must never become query-visible

Mapping to today's catalog terms:

- current `BUILDING` stays `BUILDING`
- current `ACTIVE` corresponds to `PUBLISHED`
- current `RETIRED` corresponds to `RETIRING`
- current `FAILED` stays `FAILED`
- current `INACTIVE` should not be used as a substitute for `RECLAIMABLE`; reclamation needs an explicit safe-to-drop condition

### 5.4 Publication rules

Rule 1:

- build artifacts are never query-visible before the publish record is durable

Rule 2:

- promotion sets the replacement generation's `valid_from_xid = X_publish`
- the previous active generation receives `retired_xid = X_publish`

Rule 3:

- readers must resolve visible generations from catalog metadata, not from partially rewritten runtime structures

Rule 4:

- physical generation drop is legal only when `retired_xid < H_index_reclaim(f)`

### 5.5 Reclamation rules by family class

Mutable page-entry families:

- `B-tree`, `Hash`, `GiST`, `SP-GiST`, `R-tree`, `GIN` postings, `HNSW` nodes
- must use delete-mark first, physical unlink later
- page compaction must receive a captured lifecycle snapshot including `H_index_reclaim(f)`

Immutable or generational families:

- `LSM` runs, `Columnstore` segments, future sealed bitmap or inverted segments
- publish by generation install
- retire old generations by metadata
- reclaim only after generation retire XID is older than `H_index_reclaim(f)`

Summary families:

- `BRIN`, bitmap summaries, zonemap-like structures
- cannot drop an old summary until a replacement summary or a proof of source emptiness exists
- summary maintenance debt must be query-visible to the planner

### 5.6 Immediate correctness defects exposed by this packet

Priority defects:

- `B-tree` GC is not safe while it uses `current_xid` for physical compaction decisions
- `Hash` GC is not safe while it removes any delete-marked entry without a reclaim horizon
- `SP-GiST` cleanup is not safe while dead-TID membership alone can trigger page rewrite
- `R-tree` `gcCompact` is not a valid online GC implementation while it clears the tree
- `LSM` memtable compaction is not MGA-safe while it can drop tombstones before the shared reclaim horizon
- `HNSW` has no complete reclaim story while deleted nodes can outlive or outrun graph repair

## 6. Optimizer Metrics Packet

Lifecycle correctness needs planner-visible metrics because deferred cleanup changes search cost materially.

Recommended engine-level metrics:

- `queryable_generation_count`
- `retired_generation_count`
- `failed_generation_count`
- `publish_lag_xids := X_now - last_publish_xid`
- `reclaim_lag_xids := X_now - H_index_reclaim(f)`
- `vacuum_debt_bytes`
- `dead_fraction`
- `stale_entry_fraction`
- `maintenance_backlog_ops`

Recommended family-specific metrics:

- `pending_list_fraction` for `GIN`
- `unsummarized_range_fraction` for `BRIN`
- `overflow_chain_depth` for `Hash`
- `page_garbage_fraction` for `B-tree`, `GiST`, `SP-GiST`, `R-tree`
- `run_count`, `level_count`, and `tombstone_fraction` for `LSM`
- `deleted_node_fraction` and `orphan_link_fraction` for `HNSW`
- `stale_segment_fraction` and `sealed_generation_count` for `Columnstore`
- `visibility_recheck_fraction` for lossy or collision-prone families

Required metric semantics:

- metrics must be collected from the published generation visible to queries
- background build artifacts may export separate diagnostics but must not pollute optimizer inputs until published
- reclaim debt metrics must be monotonic between maintenance cycles so the planner can trust them

Immediate optimizer implication:

- lifecycle health becomes part of access-path quality, not just a maintenance dashboard

## 7. Access-Path and Costing Packet

### 7.1 Access-path gating

Every access path must expose:

- `queryable_state`
- `requires_recheck`
- `ordered_output`
- `lossy_behavior`
- `maintenance_debt_profile`

Enumeration rule:

- the planner may only enumerate a family when at least one published generation is queryable
- retired-but-not-reclaimed generations may remain physically present, but they are not planner-visible for new readers

### 7.2 Costing extensions

Recommended first-pass penalty model:

`cost_lifecycle_penalty(f) := C_dead * dead_fraction + C_backlog * maintenance_backlog_ops + C_reclaim * log2(1 + reclaim_lag_xids) + C_family * family_fragmentation(f)`

Examples:

- `GIN` search cost must include pending-list scan cost
- `BRIN` pruning cost must increase with unsummarized ranges
- `Hash` equality probe cost must increase with overflow depth and stale entry fraction
- `LSM` cost must increase with run count, tombstone fraction, and compaction debt
- `HNSW` search cost must increase with deleted-node fraction and recheck rate

### 7.3 Access-path contracts by family class

Ordered mutable families:

- `B-tree` and future ordered `LSM` paths may only claim stable ordered output from published generations
- index-only eligibility is reduced when visibility or stale-entry debt forces frequent heap confirmation

Equality and lossy families:

- `Hash`, bitmap, `BRIN`, and many inverted paths must expose a `recheck` component explicitly
- lifecycle debt increases both probe CPU and false-positive cleanup cost

Generalized and ANN families:

- `GiST`, `SP-GiST`, `R-tree`, `GIN`, and `HNSW` need family-specific debt metrics before they can become first-class planner paths
- until those metrics exist, the planner should either avoid them for costing-sensitive choices or apply conservative penalties

### 7.4 Current planner gap

The planner currently enumerates only a subset of families and uses hard-coded structural inputs in several cost calls.

Lane F requirement:

- lifecycle-aware planning cannot rely on hard-coded heights, levels, or cleanup assumptions
- it must consume family statistics produced by the published generation and its maintenance subsystem

## 8. ScratchBird Contract Draft

### 8.1 Engine contract

ScratchBird should define one shared lifecycle snapshot structure captured at maintenance start:

- `publish_xid`
- `oldest_active_xid`
- `oldest_snapshot_xid`
- `heap_reclaim_horizon`
- `index_reclaim_horizon`
- family dependency fields where needed

All index maintenance entry points should consume this captured snapshot instead of re-reading transaction state ad hoc during page traversal.

### 8.2 Catalog contract

- `CatalogManager` remains the source of truth for generation visibility
- shadow build is the only legal way to replace a queryable generation for whole-index rebuild or reindex operations
- retired generations remain readable to pre-retire snapshots and physically retained until the shared reclaim horizon moves past `retired_xid`
- `gcRetiredIndexes` must switch from a bare `getOldestXid()` test to the shared index reclaim horizon

### 8.3 Family registration contract

Each family must register:

- publication model: `MUTABLE_IN_PLACE`, `IMMUTABLE_GENERATION`, or `SUMMARY_GENERATION`
- reclaim authority: heap-confirmed, segment-confirmed, posting-confirmed, or graph-confirmed
- required family dependency horizon
- maintenance debt metrics exported to stats and optimizer

### 8.4 Family-specific contract deltas

`B-tree`

- replace `current_xid` reclaim tests with `H_index_reclaim(BTREE)`
- allow physical page compaction only for entries confirmed dead by heap visibility rules

`Hash`

- never physically remove delete-marked entries solely because `he_xmax != 0`
- compact buckets and free overflow pages only when every removed entry is reclaimable under the shared horizon

`BRIN`

- distinguish `summary obsolete` from `summary reclaimable`
- require replacement-summary publication or interval-emptiness proof before physical removal

`Bitmap`

- align documentation and implementation around persisted visibility metadata
- define whether pruning is entry-level, page-level, or generation-level and gate it with the shared horizon

`GiST`

- preserve current dead-TID plus horizon discipline
- route horizon choice through the shared lifecycle API instead of family-local `oldest interesting` reads

`SP-GiST`

- add a reclaim-horizon gate before leaf-page rewrite removes logically deleted entries

`R-tree`

- ban clear-the-tree behavior from online `gcCompact`
- require page-local prune, or full replacement by shadow build plus generation retirement

`GIN`

- define separate maintenance phases for pending-list cleanup, posting-list cleanup, and posting-tree cleanup
- keep searches correct while maintenance debt exists, but surface the debt to costing

`HNSW`

- treat node delete as logical first
- make physical node reclamation legal only after replacement links or a rebuilt published graph exists
- prefer generation rebuild over in-place destructive node unlinking

`LSM`

- use transaction-manager visibility and the shared horizon for memtable compaction and SSTable compaction
- tombstones may be dropped only when they are older than the reclaim horizon and no older run or reader still depends on them

`Columnstore`

- standardize dual-meta or generation-swap publication for every columnar family variant
- reclaim segments only after replacement metadata is published and the retire point is older than the shared horizon

### 8.5 Crash and restart contract

- crash before publish: build artifact is discarded or resumed, never made query-visible implicitly
- crash after publish but before old-generation reclaim: both generations remain durable; readers resolve visibility from catalog metadata
- crash during maintenance cleanup: cleanup is idempotent because logical delete or retire is durable before physical reclaim

## 9. Validation and Benchmark Packet

### 9.1 Correctness validation

Required tests:

- long-lived snapshot holds old generation visible while new generation is published
- background GC runs concurrently with readers across all families and never removes still-visible entries
- shadow build, publish, restart, and retire sequences survive crash injection at every boundary
- heap truth and index truth remain equivalent under delete, update, vacuum, and reindex races

### 9.2 Family-specific validation

- `B-tree` and `Hash`: bucket or page compaction under long snapshot and repeated delete-insert churn
- `BRIN` and bitmap: summary replacement, unsummarized-range drain, and stale-summary non-removal before replacement
- `GiST`, `SP-GiST`, and `R-tree`: dead-entry cleanup with concurrent readers and mandatory heap recheck
- `GIN`: pending-list growth, cleanup thresholds, and query slowdown under maintenance debt
- `HNSW`: recall, connectivity, and delete correctness after logical delete, background rebuild, and graph publish
- `LSM`: tombstone safety across memtable flush, compaction, restart, and old-reader overlap
- `Columnstore`: dual-generation publish and reclaim with snapshot readers pinned on older metadata

### 9.3 Benchmark packet

Recommended benchmark axes:

- `publish_latency`
- `retire_to_reclaim_latency`
- `vacuum_debt_bytes`
- `search_penalty_per_dead_fraction`
- `planner_choice_stability_under_debt`
- `space_amplification`
- `write_amplification`
- `ANN recall under delete debt`

Required benchmark scenarios:

- read-mostly steady state
- write-heavy with delayed maintenance
- mixed OLTP delete-update churn
- long snapshot plus background cleanup
- build and reindex while queries remain online

## 10. Adopt/Adapt/Reject/Defer Matrix

| Pattern | Source | Decision | ScratchBird rationale |
| --- | --- | --- | --- |
| Oldest-reader reclaim horizon | Firebird | Adopt | Matches MGA semantics and fixes current family-local `current_xid` cleanup errors. |
| Intermediate version pruning after common visibility marker | Firebird | Defer | Valuable space optimization, but only after a unified horizon API exists and version ownership is explicit. |
| Common maintenance hooks with family-specific cleanup | PostgreSQL | Adopt | Fits ScratchBird's existing GC dispatch point and avoids one-size-fits-all cleanup. |
| Pending-list plus deferred cleanup for inverted families | PostgreSQL `GIN` | Adapt | Keep the concept, but require planner metrics and publish-state tracking. |
| Asynchronous summarization for summary families | PostgreSQL `BRIN` | Adopt | Unsummarized or stale summary state is acceptable if query-visible and costed correctly. |
| Delete-mark now, purge later using a safe reader boundary | MySQL InnoDB | Adapt | Correct pattern for mutable families, but ScratchBird must bind purge to MGA horizons and heap truth rather than clustered-record undo chains. |
| Atomic immutable view swap during compaction | Cassandra SAI | Adopt | Best fit for `LSM`, `Columnstore`, and future sealed-segment families. |
| Refresh-gated visibility and metadata-gated segment publication | OpenSearch | Adapt | ScratchBird already has catalog generation metadata; use that instead of searcher refresh, but keep the same publication discipline. |
| Snapshot includes only sealed published segments | Milvus | Adopt | Good match for segment-backed families and online backup or checkpoint semantics. |
| Eager physical deletion during foreground cleanup | Multiple anti-patterns | Reject | Violates MGA safety whenever an older reader can still need the artifact. |
| Full-structure clear as online GC | Current `R-tree` behavior | Reject | Not compatible with concurrent query correctness. |
| Planner ignorance of lifecycle debt | Current ScratchBird baseline | Reject | Deferred cleanup without costing will produce unstable and misleading access-path choices. |

## 11. Open Questions and Integration Dependencies

Open questions:

- should catalog generation visibility continue to use transaction XID only, or should it be keyed to an explicit statement snapshot token
- should `oldest_interesting_xid` participate directly in `H_index_reclaim`, or only through `heap_reclaim_horizon`
- does every family need an explicit `VALIDATING` catalog state, or can validation remain build-local until publish
- what is the authoritative `owner_confirms_dead` signal for each family: heap, posting owner, summary source interval, or graph replacement metadata
- should `HNSW` reclaim use incremental link repair, full shadow rebuild, or both depending on delete fraction
- can `LSM` guarantee ordered output for planner purposes before lifecycle metrics and duplicate-resolution contracts are formalized

Integration dependencies:

- `TransactionManager` must expose one stable index lifecycle snapshot API
- `CatalogManager` must consume that API for retired-generation GC
- `GarbageCollector` must pass captured horizons to every family cleanup path
- per-family runtimes must register lifecycle model and exported maintenance metrics
- statistics storage must persist lifecycle debt counters
- optimizer enumeration and costing must become family-aware beyond `B-tree` and `LSM`
- crash recovery must understand build artifacts, published generations, and retired-but-not-yet-reclaimed storage

## 12. Recommended Next-Step Specification Tasks

1. Write the engine-wide index lifecycle specification that defines `H_index_reclaim`, captured lifecycle snapshots, publication rules, and the authoritative state machine.
2. Write the catalog and shadow-build specification update that remaps current `ACTIVE` and `RETIRED` behavior onto explicit publish and retire semantics.
3. Write the mutable-family cleanup specification for `B-tree`, `Hash`, `GiST`, `SP-GiST`, and `R-tree`, including `owner_confirms_dead` requirements and banned online behaviors.
4. Write the generational-family specification for `LSM`, `Columnstore`, and future sealed-segment families, including publish, retire, reclaim, and crash-restart rules.
5. Write the summary-family specification for `BRIN`, bitmap, and zonemap-style structures, including replacement-summary requirements and unsummarized-range observability.
6. Write the inverted and ANN maintenance specification for `GIN` and `HNSW`, separating pending-state visibility, cleanup triggers, and reclamation legality.
7. Write the optimizer statistics and costing specification that introduces lifecycle debt metrics and path penalties for all families.
8. Write the validation and benchmark specification that standardizes long-snapshot GC tests, crash injection points, debt benchmarks, and family-specific correctness probes.

First-pass recommendation:

- treat the unified reclaim horizon and the ban on `current_xid`-based physical deletion as the immediate blocking contract for all subsequent index-family specification work

# ScratchBird Index Research Lane C Findings

Status: first pass

Date: 2026-03-14

This findings packet synthesizes ScratchBird live code and tests, then donor evidence from PostgreSQL, MongoDB, and Redis. Web research was not required for this pass. The goal is not to restate donor behavior verbatim; it is to extract the contract, optimizer, and lifecycle rules ScratchBird needs for a production-grade Lane C design.

## 1. Scope and lane objective

Lane C covers ScratchBird index families whose search semantics are not a simple total-order probe:

- `RTREE`: dedicated spatial search over bounding regions.
- `GIST`: generalized search tree over conservative summaries.
- `SPGIST`: space-partitioned generalized search tree over disjoint routing decisions.

Lane C objective:

- Define a durable family contract for generalized-search and spatial indexes that is compatible with ScratchBird's MGA lifecycle model.
- Turn the current scaffolding for `GiST`, `SPGiST`, and `RTree` into a catalog-driven, planner-visible, validator-backed feature set.
- Separate three concerns that are currently blurred together:
  - family method semantics,
  - operator-class semantics,
  - geometry or decomposition encoding.

In scope for this pass:

- family-level search contracts,
- opclass requirements,
- MGA visibility and maintenance rules,
- optimizer statistics and costing surfaces,
- validation and benchmark expectations,
- adopt/adapt/reject decisions from donor engines.

Out of scope for this pass:

- full SQL grammar design for every operator family,
- extension packaging details,
- ANN/vector search,
- full-text ranking,
- rich spherical geography semantics beyond the contract hooks needed to support them later.

Primary finding:

- ScratchBird already has meaningful storage and API scaffolding for all three families, but it does not yet have a production-ready Lane C contract. The main missing pieces are authoritative catalog-driven opclass binding, operator-to-strategy routing, lifecycle-safe maintenance rules, and planner/costing integration that reflects lossy search and generalized predicates.

## 2. ScratchBird current-state baseline

Current-state baseline should be read as "implemented scaffolding plus important gaps", not "ready contract".

| Area | Observed baseline | Contract implication |
| --- | --- | --- |
| Family registry | ScratchBird registers many index families, including `GIST`, `SPGIST`, `RTREE`, and several geo aliases. | Method names already exist and should remain stable, but registry presence must not be mistaken for full runtime support. |
| Catalog surface | Catalog metadata already includes index method information, per-column opclass identifiers, and index-parameter state. | The catalog surface is promising and should become authoritative rather than decorative. |
| Parser and DDL | DDL and docs describe richer opclass behavior than the live runtime currently enforces end to end. | Lane C must reconcile advertised syntax with actual catalog binding and execution semantics. |
| GiST runtime | `GiST` supports page structures, xmin/xmax versioning, splits, k-NN hooks, and operator-class APIs. However, the default built-in opclass is an equality-only stub, and runtime create/open paths currently resolve opclass `0` from an in-memory registry rather than an authoritative catalog selection. | GiST is structurally present but functionally under-bound. Lane C must make catalog-selected opclass resolution mandatory. |
| GiST search routing | Generic GiST searches are currently routed as `OVERLAPS` instead of deriving strategy from the query operator family. | This is the clearest sign that current GiST behavior is not yet a true generalized-search contract. |
| SP-GiST runtime | `SPGiST` supports inner and leaf page structure, xmin/xmax visibility, and an extensible operator-class API. The default built-in opclass is again a simple equality-like placeholder. | SP-GiST plumbing exists, but production behavior still depends on real opclass registration and operator routing. |
| R-tree runtime | `RTREE` exists as a dedicated implementation and is wired through DML and search paths. Current behavior assumes a fixed serialized 2D bounding-box key shape. | ScratchBird already has a dedicated spatial method, but it is specialized and should be contracted honestly as such. |
| Test model | Integration tests prove that custom GiST and SP-GiST opclasses can be registered and exercised. | Tests show the extension seam is real. They do not prove durable catalog-driven runtime behavior. |
| Planner and executor | Executor routing is family-aware, but current search descriptors are too weak to represent the full operator strategy, lossiness, and distance semantics expected by GiST and SP-GiST. | Planner and executor need a real search-descriptor contract instead of family-specific shortcuts. |
| Documentation and compatibility SQL | Existing docs and compatibility scripts claim broader GiST/SP-GiST coverage than the live runtime currently guarantees. | Lane C needs a spec-first cleanup so outward promises match actual capability gates. |

Most important baseline conclusion:

- ScratchBird should treat its current `GiST` and `SPGiST` code as a strong internal prototype, not as a finished contract.
- ScratchBird should treat current `RTREE` as a usable specialized spatial baseline, not as proof that the generalized-search problem is solved.

Concrete baseline gaps that materially affect the spec:

- GiST and SP-GiST default operator classes are stubs and cannot be the basis of claimed broad operator-family support.
- Current create/open flows do not reliably honor catalog-selected opclasses.
- Current generic GiST search routing collapses strategy to `OVERLAPS`.
- Current R-tree key contract appears fixed to a 2D rectangle encoding.
- Current dedicated R-tree compaction behavior is too blunt to be accepted as an MGA-safe online maintenance contract.

## 3. Donor-engine research synthesis

PostgreSQL is the primary donor for Lane C. MongoDB and Redis are useful secondary donors for spatial publication, approximate filtering, and key-explosion control, but neither is a direct generalized-search donor in the GiST/SP-GiST sense.

| Donor | Strongest lessons | Limits of direct reuse | Lane C value |
| --- | --- | --- | --- |
| PostgreSQL | Mature GiST/SP-GiST contracts, support-function validation, lossy recheck semantics, k-NN lower-bound search, split and concurrency rules, null handling patterns, and official documentation that matches production behavior. | PostgreSQL assumes its own storage, concurrency, and catalog machinery; ScratchBird must translate the ideas into MGA-native publish and reclamation rules. | Primary donor for generalized-search family contract. |
| MongoDB | Spatial indexes are published as cell or hash coverings over a sorted-key substrate; planner logic expands coverings into interval bounds; index versions and key-count caps are explicit. Wildcard indexing shows how generalized search can be modeled as path/value decomposition rather than page-level summaries. | MongoDB does not offer a GiST/SP-GiST-style pluggable summary tree. Its lessons are about publication strategy, validation, and planner interval expansion, not direct family design. | Secondary donor for geo-cell decomposition, versioned index formats, and key explosion limits. |
| Redis | Spatial search is implemented as approximate geohash-box prefilter plus exact post-filter on a sorted set. Complexity claims are straightforward and operationally honest. | Redis is not a generalized-search tree donor. It trades structural generality for simplicity and approximate prefiltering. | Useful contrast donor for "cheap approximate bound plus exact recheck" patterns and cost framing. |

Synthesis across donors:

- The generalized-search contract should be opclass-driven, validator-backed, and explicit about lossiness.
- Spatial publication can use either conservative page summaries or decomposed cell coverings, but the contract must state which one is in use because they have different amplification and maintenance behavior.
- Approximate matches are acceptable only when exact recheck is explicit and never skipped.
- Versioned index formats and hard caps on key explosion are not optional for non-trivial spatial publication.
- Planner costing must account for false positives, overlap amplification, and publication amplification; a simple "index probe is cheap" assumption is wrong for this lane.

## 4. Primary literature and official-document synthesis

The classic R-tree, GiST, and SP-GiST literature, together with production documentation from mature systems, converge on a small set of stable laws. Lane C should elevate these laws into ScratchBird contract text.

| Law | Synthesis | ScratchBird implication |
| --- | --- | --- |
| Conservative summary law | In summary-tree families, every internal summary must over-approximate all visible descendants reachable through that entry. | GiST and dedicated R-tree entries must never become narrower than any visible descendant. Summary staleness may hurt performance, but summary under-approximation is a correctness bug. |
| Safe pruning law | A branch may be pruned only when the family method can prove the query cannot match any visible descendant in that branch. | `consistent`, `inner_consistent`, and dedicated R-tree predicate checks must be one-sided safe. False positives are allowed. False negatives are not. |
| Honest lossy-match law | Lossy internal or leaf filtering is acceptable only if the executor receives an explicit recheck signal. | ScratchBird must carry `needs_recheck` or equivalent through executor and planner costing. |
| Lower-bound distance law | Ordered search over generalized trees is correct only if page and branch distances are lower bounds on the true result distance. | GiST k-NN and any future SP-GiST ordered search must reject opclasses that cannot produce non-overestimating lower bounds. |
| Split-quality law | Search trees remain useful only if split rules control overlap, coverage inflation, and occupancy skew. | ScratchBird needs split metrics and validator thresholds instead of accepting any mechanically valid split. |
| Format-validation law | Extensible families require support-function signature checks and format-version checks at create/open time. | Current "register a runtime opclass and hope it matches" behavior is not sufficient. |
| Publication-amplification law | Spatial coverings or generalized decompositions can emit many physical keys per logical row, so build and DML rules need explicit caps and cost visibility. | ScratchBird should distinguish summary-tree families from decomposed-covering techniques and expose amplification metrics for both. |

The literature-specific design reading for Lane C is:

- R-tree contributes subtree-choice and split heuristics based on bounding-region enlargement, overlap, and area or volume growth.
- GiST contributes the generalized support-function contract: search consistency, union, penalty, split, equivalence, optional compression, optional ordered-distance semantics, and explicit recheck.
- SP-GiST contributes a different generalized-search pattern: route by disjoint space partition, reconstruct values from prefixes and labels when needed, and surface special cases such as high-duplication "all the same" pages.
- Official documentation from mature systems adds an important operational lesson: extensibility is safe only when opclass validation, planner mapping, and exactness rules are first-class metadata, not informal conventions.

## 5. MGA and lifecycle correctness packet

Lane C must remain MGA-native. It must not depend on WAL-style correctness assumptions. The core rule is that index state is published and reclaimed by generation and visibility rules, not by "redo later" assumptions.

Required invariants:

| Invariant | Required rule | Immediate consequence |
| --- | --- | --- |
| Visibility versioning | Every searchable leaf entry, posting entry, or equivalent physical record must carry `xmin` and `xmax` visibility state. | Lane C should preserve the existing versioned entry pattern already present in GiST and SP-GiST. |
| Snapshot-safe build publish | A build or rebuild must construct a shadow tree and publish a new root or metadata generation atomically. Readers on old snapshots must continue to see the prior generation until their visibility horizon closes. | Online create, reindex, and major split repair cannot mutate the only visible root in place. |
| Logical delete before physical reclaim | Delete is a visibility change first. Physical removal is allowed only when the global oldest-active snapshot is newer than the entry's `xmax`. | "Compaction" must not discard entries that are still visible to any snapshot. |
| Conservative ancestor repair | If a leaf entry is inserted or migrated, every visible ancestor on the published path must remain a conservative summary. | Parent repair may lag only in the direction of over-approximation. |
| Split publication safety | A split must never make an existing visible descendant unreachable. | ScratchBird needs a right-link, sibling, or generation-publish rule strong enough to avoid lost branches during concurrent or interleaved readers. |
| Opclass determinism | Support functions must be deterministic for a given opclass version and serialized datum format. | Lane C needs persisted opclass version identifiers and open-time compatibility checks. |
| Recheck honesty | If a family returns a candidate through a lossy path, that fact must survive to executor qualification. | Planner costing and executor filtering both depend on this bit. |
| Null policy explicitness | Null handling must be part of the family contract, not an emergent side effect. | SP-GiST should adopt a dedicated null-partition rule; GiST and R-tree should either define a null bucket or reject null keys explicitly. |

Lifecycle packet by operation:

- Build:
  - Use a stable build snapshot.
  - Populate a shadow structure.
  - Derive metrics during build: height, fill, split counts, overlap ratios, publication amplification.
  - Publish by metadata generation swap.
- Insert:
  - Choose a branch using family-specific rules.
  - Create a new visible entry version.
  - Repair ancestor summaries conservatively.
  - If a split occurs, publish child pages before parent redirect becomes authoritative.
- Delete:
  - Mark logical death using `xmax`.
  - Avoid eager page rewrites whose only effect is reclaim.
  - Reclaim only after the oldest active snapshot passes the death horizon.
- Vacuum or GC:
  - Remove only reclaimable dead entries.
  - Repair summaries if dead-entry removal materially narrows the conservative bound.
  - Preserve older generations until no snapshot can observe them.
- Reindex:
  - Build a new generation from visible rows.
  - Swap atomically.
  - Retire the old generation after the visibility horizon passes.
- Drop:
  - Retire metadata first.
  - Reclaim physical pages only after no active snapshot can reach them.

Immediate baseline correctness concern:

- The current dedicated R-tree compaction path appears equivalent to clearing the tree. That is only lifecycle-correct for drop or truncate semantics. It is not acceptable as online compaction or vacuum behavior under MGA.

## 6. Optimizer metrics packet

Lane C needs family-aware statistics because generalized-search and spatial costs are dominated by overlap, lossiness, and publication shape, not just tuple counts.

Recommended persistent metrics:

| Metric | Formula or definition | Family use |
| --- | --- | --- |
| `tree_height` | Root-to-leaf level count. | All families. |
| `internal_pages`, `leaf_pages` | Physical page counts by role. | All families. |
| `live_fill` | `live_entries / max(entry_capacity, 1)` averaged over pages. | All families. |
| `dead_fraction` | `dead_entries / max(total_entries, 1)` | All families; feeds vacuum urgency and cost inflation. |
| `split_rate` | `splits / max(dml_ops, 1)` over a stats window. | All families; signals write amplification. |
| `recheck_ratio` | `rechecked_candidates / max(total_candidates, 1)` | GiST, SP-GiST, approximate spatial paths. |
| `false_positive_ratio` | `1 - exact_hits / max(total_candidates, 1)` | GiST, SP-GiST, approximate spatial paths. |
| `coverage_ratio` | `sum(volume(child_summary_i)) / max(volume(parent_summary), eps)` averaged over internal pages. | GiST and R-tree; values well above `1` indicate overlap pressure. |
| `pair_overlap_ratio` | `sum(volume(intersection(child_i, child_j))) / max(volume(parent_summary), eps)` averaged over internal pages. | GiST and R-tree; strongest signal for branch explosion. |
| `penalty_mean` | `avg(volume(union(summary, key)) - volume(summary))` during insert. | GiST and R-tree. |
| `branch_skew` | `max(branch_hits) / max(avg(branch_hits), 1)` over SP-GiST inner nodes. | SP-GiST. |
| `all_the_same_fraction` | `all_the_same_inner_pages / max(inner_pages, 1)` | SP-GiST; signals weak discrimination. |
| `avg_depth` | Mean leaf depth. | SP-GiST; important when the tree is non-balanced. |
| `publication_amplification` | `physical_entries_written / max(logical_rows_indexed, 1)` | R-tree variants with decomposition, future geo-cell publication, wildcard-like patterns. |
| `lb_violation_count` | Count of cases where a reported lower bound exceeded the exact distance. Must remain `0`. | Any ordered search. |

Heuristics:

- If `recheck_ratio` is high and `pair_overlap_ratio` is also high, treat the index as structurally lossy rather than merely predicate-lossy and bias costing upward.
- If `all_the_same_fraction` rises, SP-GiST pruning is collapsing and the planner should degrade toward scan-like expectations.
- If `publication_amplification` exceeds a configured threshold, build and insert cost must scale superlinearly because one logical row is no longer close to one physical entry.
- If `dead_fraction` is high, branch visit cost should be inflated even before vacuum because dead entries still consume comparisons and page reads.

Statistics collection timing:

- At create and reindex completion.
- Periodically during maintenance.
- Opportunistically during insert or split hot paths only if counters are cheap.
- During validation runs to calibrate planner weights.

## 7. Access-path and costing packet

Lane C needs a single generic costing skeleton with family-specific selectivity and amplification terms.

Generic cost model:

`C_total = C_root + C_internal + C_leaf + C_candidate + C_recheck + C_heap + C_order`

Where:

- `C_internal = pages_internal_visited * c_page_internal`
- `C_leaf = pages_leaf_visited * c_page_leaf`
- `C_candidate = N_candidate * c_tuple_test`
- `C_recheck = N_recheck * c_exact_predicate`
- `C_heap = N_heap * c_heap_fetch`
- `C_order = N_order * c_priority_queue_or_sort`

Core estimation terms:

- `N_candidate = N_rows * sel_cover * overlap_amp * dead_amp`
- `N_recheck = N_candidate * recheck_ratio`
- `N_heap = N_candidate * (1 - index_only_fraction)`
- `overlap_amp = max(1, 1 + w_overlap * pair_overlap_ratio + w_cover * max(coverage_ratio - 1, 0))`
- `dead_amp = 1 + w_dead * dead_fraction`

Family-specific access paths:

| Access path | Preconditions | Main cost drivers | Contract consequence |
| --- | --- | --- | --- |
| R-tree overlap or containment scan | Built-in rectangle or region semantics, conservative internal bounds, exact leaf predicate or recheck flag. | Overlap amplification, leaf candidate count, heap recheck. | `RTREE` must advertise which predicates are exact at the leaf and which require recheck. |
| R-tree nearest search | Lower-bound distance on internal summaries. | Priority-queue visits, lower-bound tightness, exact distance recheck. | No nearest-path support without a proven non-overestimating lower bound. |
| GiST predicate scan | Query operator maps to a stable strategy number supported by the selected opclass. | Opclass selectivity, pair overlap, recheck ratio. | Executor must pass strategy, query datum, and recheck capability; hardcoded `OVERLAPS` is not sufficient. |
| GiST ordered search | Opclass provides lower-bound distance semantics. | Priority queue growth, distance lower-bound tightness, recheck and reorder rate. | Ordered search must be separately advertised in opclass metadata. |
| SP-GiST partitioned lookup | Opclass `choose` and `inner_consistent` prune branches by partition logic. | Average depth, branch skew, all-the-same pages, recheck ratio. | Planner must know whether the partition is highly discriminative or effectively degenerate. |
| SP-GiST prefix or trie scan | Prefix reconstruction and label routing are defined by opclass config. | Depth, long-value chaining, leaf exactness. | Index-only or reconstructed-key behavior must be declared explicitly. |

Insertion heuristics that should be contractual:

- GiST and R-tree subtree choice:
  - `penalty(entry, key) = volume(union(summary_entry, key)) - volume(summary_entry)`
  - choose the entry with minimum penalty,
  - break ties by smaller summary volume,
  - break remaining ties by higher free capacity or lower historical split pressure.
- Split scoring:
  - `split_score = w_overlap * overlap_after + w_cover * coverage_after + w_skew * imbalance_after`
  - reject or penalize splits that improve occupancy but sharply increase overlap.
- SP-GiST routing:
  - `choose` should prefer deterministic branch selection,
  - `picksplit` should avoid single-branch collapse,
  - if duplication forces a same-branch collapse, mark the page as such so costing degrades appropriately.

Planner safety rule:

- If a query operator cannot be mapped to a validated strategy in the selected opclass, or if family stats are absent and no conservative fallback estimate exists, the planner should not choose the Lane C path.

## 8. ScratchBird contract draft

The contract should deliberately distinguish family method, opclass, and key format.

### 8.1 Family identities

| Family | v1 contract position |
| --- | --- |
| `RTREE` | Keep as a specialized built-in spatial family. Do not pretend it is already a general opclass-driven tree. |
| `GIST` | Make fully opclass-driven. This is the primary generalized-summary family. |
| `SPGIST` | Make fully opclass-driven. This is the primary disjoint-partition family. |

### 8.2 Catalog and metadata contract

For every indexed key column, persist:

- `opclass_id`
- `opclass_version`
- `strategy_map_version`
- `storage_format_version`
- `family_options_version`

Root or metapage state must mirror the same identifiers so open-time mismatch detection is local and cheap.

Open-time contract:

- Resolve the selected opclass from catalog metadata, not from a default in-memory slot.
- Refuse open if family, opclass version, or storage format do not match.
- Allow read-only degraded open only if an explicit compatibility rule says so.

Create-time contract:

- If the user omits an opclass and the key type has exactly one default opclass for the family, bind it explicitly in metadata.
- If multiple plausible opclasses exist, force explicit user selection.
- Validate support-function presence and signature before first page is written.

### 8.3 Search descriptor contract

ScratchBird needs a family-neutral search descriptor at least as strong as:

- family method,
- column ordinal,
- strategy number,
- serialized query datum,
- optional order-by strategy,
- optional distance limit,
- `allow_lossy`,
- `want_recheck_flag`,
- `want_distance`,
- snapshot or visibility context.

Contract consequence:

- Generic GiST search may not default to `OVERLAPS`.
- Every searchable operator must map to a stable strategy number in the bound opclass.
- Result tuples must carry `needs_recheck` when required.

### 8.4 GiST support-function contract

Mandatory GiST support:

- `consistent`
- `union`
- `penalty`
- `picksplit`
- `same`

Optional GiST support:

- `compress`
- `decompress`
- `distance`
- `fetch`
- `options`
- bulk-build or sort-support hooks

GiST contract notes:

- `consistent` may be lossy but must never produce a false negative.
- `distance`, when present, must never overestimate.
- `fetch` is required for index-only behavior if compressed leaf keys are lossy.
- `same` must define semantic equality on the summary domain, not raw-byte coincidence only.

### 8.5 SP-GiST support-function contract

Mandatory SP-GiST support:

- `config`
- `choose`
- `picksplit`
- `inner_consistent`
- `leaf_consistent`

Optional SP-GiST support:

- `compress`
- `options`
- ordered-search distance outputs from `inner_consistent` and `leaf_consistent`

SP-GiST contract notes:

- `config` must declare prefix, label, and leaf reconstruction behavior.
- Null semantics should be handled by a dedicated null partition rather than left to every opclass.
- A same-branch collapse mode must be represented explicitly so validator and planner can see it.
- Index-only behavior requires exact value return or exact reconstructability.

### 8.6 Dedicated R-tree contract

Recommended `RTREE` v1 contract:

- Built-in only.
- Scope limited to rectangle or bounding-region keys over a declared dimensionality, with 2D as the initial production target.
- Required predicates:
  - `overlaps`
  - `contains`
  - `contained_by`
  - `equal`
- Optional ordered search:
  - `distance`
  - `nearest`

R-tree contract notes:

- Internal entries store conservative bounding regions.
- Subtree choice uses minimum enlargement.
- Split policy minimizes overlap, then coverage inflation, then occupancy skew.
- Online compaction must be generational or page-local vacuum, not full-tree clear.

### 8.7 Planner and executor contract

- Planner maps SQL operators to family strategy numbers through bound opclass metadata.
- Executor receives a typed search descriptor and returns candidate TIDs plus exactness metadata.
- Index-only scans are permitted only when the family and opclass declare exact return semantics.
- Ordered search is a separate capability bit and must not be inferred from ordinary predicate support.

### 8.8 Maintenance and validation contract

- Every Lane C family must provide structural validation.
- Every opclass must provide support-function validation.
- Every build path must publish stats needed for costing.
- Every maintenance path must be MGA-safe by construction.

## 9. Validation and benchmark packet

Validation work items:

| Validation class | Minimum check set |
| --- | --- |
| Support-function validator | Arity, signature, strategy coverage, distance lower-bound capability, return-data capability, reconstruction capability. |
| Catalog/open validator | Family/opclass/storage-format/version match, no silent fallback to opclass `0`, no missing strategy map. |
| Structural validator | Reachability from root, parent-child consistency, conservative summaries, page role correctness, sibling or generation linkage correctness, depth sanity. |
| Visibility validator | `xmin` and `xmax` legality, snapshot-visible scan equivalence to heap truth, no reclaimed-live entries. |
| Exactness validator | Candidate set must contain all exact matches; any false positive must be marked for recheck if leaf test is lossy. |
| Ordered-search validator | Lower-bound distance never exceeds exact distance; final ordering is exact after recheck. |
| Maintenance validator | Insert, delete, split, vacuum, reindex, and drop preserve visibility and reachability invariants. |
| Corruption detector | Unknown opclass version, malformed key encoding, impossible bounding boxes, invalid partition labels, metric outlier thresholds. |

Benchmark packet:

| Benchmark | Data shape | Metrics |
| --- | --- | --- |
| Spatial overlap scan | Uniform and clustered rectangles, mixed sizes, skewed aspect ratios. | Build time, page count, pair overlap, false-positive rate, p50/p95 latency. |
| Spatial nearest search | 2D points and boxes with uniform and hotspot distributions. | Queue growth, lower-bound tightness, visited pages, latency, exact reorder cost. |
| SP-GiST partition benchmark | Quad-tree and k-d-like point workloads, plus high-duplication edge cases. | Depth, branch skew, all-the-same fraction, lookup latency, insert split rate. |
| Prefix or trie benchmark | Text prefixes with common roots and long tails. | Depth, compression or reconstruction cost, prefix selectivity accuracy, index-only viability. |
| Mixed DML MGA workload | Concurrent-like insert/delete/update patterns across long-lived snapshots. | Visibility correctness, dead fraction growth, reclaim lag, maintenance overhead. |
| Publication-amplification benchmark | Multi-cell or decomposed-key publication shapes. | Physical entries per logical row, write amplification, planner misestimate rate. |

Validation acceptance bar:

- No false negatives under any validated operator strategy.
- `lb_violation_count = 0` for ordered paths.
- Build, reindex, and vacuum preserve snapshot equivalence.
- Planner chooses Lane C paths only when stats and strategy mappings are valid.

## 10. Adopt/adapt/reject/defer matrix

| Idea | Source | Decision | Reason | ScratchBird action |
| --- | --- | --- | --- | --- |
| Catalog-driven opclass binding | PostgreSQL | Adopt | Extensibility is unsafe without authoritative metadata. | Make catalog selection authoritative at create, open, and search time. |
| Support-function validators | PostgreSQL | Adopt | Prevents invalid opclasses from reaching storage. | Add create/open validation gates for GiST and SP-GiST. |
| Lossy search with explicit recheck | PostgreSQL, Redis | Adopt | Essential for generalized-search practicality. | Carry `needs_recheck` through executor and planner. |
| Lower-bound k-NN contract | PostgreSQL | Adopt | This is the correct foundation for ordered generalized search. | Gate ordered paths on validated lower-bound semantics. |
| Dedicated null partition for SP-GiST | PostgreSQL | Adopt | Simplifies opclass authoring and improves contract clarity. | Move null handling into family core. |
| Same-branch collapse visibility for SP-GiST | PostgreSQL | Adopt | Planner and validator need to know when partition discrimination collapsed. | Persist or expose `all_the_same`-style state. |
| Dedicated built-in `RTREE` method | ScratchBird baseline plus R-tree literature | Adapt | ScratchBird already has a specialized method and can ship it earlier than a full GiST-box replacement. | Keep `RTREE`, but state its narrower built-in scope honestly. |
| Geo-cell covering with key-count caps | MongoDB | Adapt | Useful for future geo adjuncts and decomposed publication, but not the core GiST/SP-GiST contract. | Reuse for future spherical or covering-based opclasses. |
| Approximate box prefilter plus exact distance filter | Redis | Adapt | Good operational pattern for some spatial predicates. | Allow as an implementation option when exactness metadata is preserved. |
| Wildcard path index as Lane C family | MongoDB | Defer | Valuable generalized-search pattern, but it is a different product slice than R-tree/GiST/SP-GiST. | Keep out of Lane C v1 scope. |
| Sorted-set geohash as primary spatial storage | Redis | Reject | Too specialized and not aligned with the existing ScratchBird tree-family direction. | Use only as conceptual inspiration for approximation, not as the core design. |
| Replacing `RTREE` with GiST-only spatial support immediately | PostgreSQL-style box GiST | Reject for v1 | Too disruptive given ScratchBird's existing dedicated R-tree implementation. | Keep both, then reevaluate after planner and opclass maturity. |
| Bulk-build and sort-support optimization | PostgreSQL | Defer | Valuable, but correctness and contract binding come first. | Specify after family semantics and validators are complete. |

## 11. Open questions and integration dependencies

- Should `RTREE` remain a permanent first-class method, or become transitional syntax over a future validated spatial opclass family?
- What is the authoritative ScratchBird geometry encoding for point, box, and polygon-like values, and how is dimensionality represented in metadata?
- Does Lane C require multicolumn GiST/SP-GiST in v1, or should v1 be single-key-column plus included columns only?
- Which SQL operators are in the first strategy map for each family, and how are they surfaced through parser, binder, and planner?
- How will parser v3 expose per-column opclass selection and family options so the catalog state is not inferred indirectly?
- What is the bootstrap model for built-in opclasses: static registry, catalog bootstrap rows, or extension-owned registration?
- Does ScratchBird require index-only scans for any Lane C family in v1, or can that wait on `fetch` or exact reconstruction support?
- How are family metrics collected and refreshed without putting too much work on hot insert paths?
- What MGA-safe online maintenance mechanism replaces the current R-tree clear-style compaction behavior?
- Which existing documentation and compatibility tests must be narrowed immediately so they stop overstating current GiST/SP-GiST capability?

## 12. Recommended next-step specification tasks

1. Write the Lane C family contract spec that defines `RTREE`, `GIST`, and `SPGIST` separately and names the authoritative metadata each one must persist.
2. Write the opclass catalog and validation spec covering support-function signatures, versioning, open-time compatibility, and strategy-map metadata.
3. Write the MGA lifecycle spec for generalized-search families: build publish, split publication, delete, vacuum, reindex, and drop.
4. Write the planner and executor strategy-routing spec so SQL operators map to family strategies without hardcoded search fallbacks.
5. Write the statistics and costing spec using the metrics in this packet, including overlap, recheck, amplification, and dead-entry inflation terms.
6. Write the dedicated `RTREE` v1 scope spec stating supported key formats, predicates, dimensionality rules, and maintenance behavior honestly.
7. Write the validation and benchmark spec with structural, visibility, exactness, and ordered-search acceptance criteria.
8. Reconcile outward-facing docs and compatibility tests with the actual staged Lane C rollout so the product surface stops promising unsupported generalized-search behavior.

First-pass recommendation:

- Ship Lane C as two connected but distinct deliverables:
  - generalized-search core for `GIST` and `SPGIST`,
  - specialized spatial contract for `RTREE`.
- Use PostgreSQL as the primary contract donor, but keep MongoDB and Redis lessons in the spec as secondary patterns for amplification control and approximate-filter economics.

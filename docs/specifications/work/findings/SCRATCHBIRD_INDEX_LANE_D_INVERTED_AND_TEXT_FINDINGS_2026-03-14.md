# ScratchBird Index Research Lane D: Inverted and Text Families Findings

## 1. Scope and lane objective

This findings pass covers ScratchBird Lane D for inverted and text-search families:

- `GIN`
- `FULLTEXT`
- `INVERTED`
- text-search routed families currently exposed as donor-shaped aliases, including `SPARSE_INVERTED`, `SPARSE_WAND`, `NGRAM`, `MONGODB_WILDCARD`, `NEO4J_TEXT`, `CASSANDRA_SASI`, and `CASSANDRA_SAI`

The lane objective is not "add one more text index." The objective is to define a coherent ScratchBird contract for:

- boolean and containment-style text predicates
- ranked text retrieval
- phrase and proximity search
- analyzer-driven token pipelines
- routed donor-family semantics that remain MGA-correct inside ScratchBird

The target outcome is a contract with two stable layers:

- a filter-first access method for exact or candidate-producing token lookups
- a score-first access method for ranked document retrieval

Everything else should be policy, parser, or aliasing on top of those two layers rather than separate storage engines.

Out of scope for this pass:

- ANN or vector retrieval
- non-text B-tree and hash families except where they interact with text planning
- parser syntax expansion beyond what is needed to define the contract boundary

This pass used ScratchBird code first, then donor-engine source and documentation from PostgreSQL, OpenSearch, MongoDB, and Cassandra. Local evidence was sufficient for the first-pass findings.

## 2. ScratchBird current-state baseline

### 2.1 Implemented runtime families

ScratchBird currently has three separate text-related runtime paths:

1. `GinIndex`
   - page-based inverted structure with pending-list support
   - posting lists and posting trees
   - per-entry `xmin` and `xmax`
   - explicit MGA-oriented comments and TIP-style visibility intent

2. `FullTextIndex`
   - wrapper around `GinIndex`
   - `TSVector` and `TSQuery` integration
   - query classification into `NEED_ALL`, `NEED_ANY`, and `NEED_RECHECK`
   - current exactness model is "candidate set first, executor recheck later" for complex queries

3. `InvertedIndex`
   - segment-oriented posting structure
   - tokenization, optional stemming and stop-word filtering
   - BM25-like ranking
   - document statistics and merge-oriented metadata

This is already enough machinery to support Lane D, but it is not a coherent contract yet.

### 2.2 Current routing is inconsistent

The implementation does not treat the family labels consistently:

- `GIN` maps to the GIN runtime.
- `FULLTEXT` and most donor-routed text families map to the inverted runtime.
- lifecycle and migration code still treats several of those same families, including `FULLTEXT`, `INVERTED`, `MONGODB_WILDCARD`, `NEO4J_TEXT`, and `CASSANDRA_SAI`, as if they were GIN-backed.

That means the user-visible family name, the opened runtime, and the lifecycle assumptions do not currently line up. This is the most important baseline finding because it makes correctness and maintenance rules ambiguous.

### 2.3 GIN baseline

The current GIN implementation already contains useful contract material:

- pending list threshold: `1000`
- posting list threshold before tree promotion: `64`
- metapage tracks pending-list head, tail, page count, unique key count, and indexed tuple count
- posting entries carry version-window fields (`xmin`, `xmax`)
- merge, cleanup, and search paths already exist

The current page-capacity formulas are explicit and usable in future costing:

- max pending entries per page: `(page_size - 128) / 72`
- max posting entries per page: `(page_size - 80) / 26`
- max posting-tree internal entries: `(page_size - 92) / 14`
- max posting-tree leaf TIDs: `(page_size - 88) / 26`

Those formulas are implementation-specific, but they are strong evidence that the GIN path is already designed as a real storage access method rather than a stub.

### 2.4 Full-text wrapper baseline

`FullTextIndex` is thin but important:

- `TSVector` storage already supports quoted lexemes, positions, and weights
- `TSQuery` parsing already supports `&`, `|`, `!`, parenthesized composition, and phrase distance
- query classification already marks complex predicates as requiring recheck

The existing selectivity heuristic in the `tsvector` GIN operator class is:

- if stats are missing: `0.1`
- single-term query: `min(avg_tids_per_key / num_tuples, 1.0)`
- pure `AND`: `pow(key_selectivity, query_lexemes * 0.5)`
- pure `OR`: `1 - pow(1 - key_selectivity, query_lexemes)`
- mixed or complex query: `min(pow(key_selectivity, query_lexemes * 0.25), 0.5)`

That heuristic is crude, but it is already a usable baseline for planner integration.

### 2.5 Inverted baseline

`InvertedIndex` exposes a different model:

- segment-level metadata for document count, term count, token count, and average document length
- optional features for positions, offsets, payloads, stemming, and stop words
- segment rotation threshold at `64 MiB` of posting bytes
- merge trigger factor of `10`

The scoring path is BM25-like:

- `idf = ln((N - df + 0.5) / (df + 0.5) + 1)`
- `score += idf * ((k1 + 1) * tf / (tf + k1 * (1 - b + b * dl / avgdl)))`
- current defaults are effectively `k1 = 1.2`, `b = 0.75`

However, the current implementation hard-codes `tf = 1.0` during scoring, so ranking does not presently use real within-document term frequency.

### 2.6 MGA correctness gap in the inverted path

The largest current defect in Lane D is that the inverted runtime does not yet use transaction visibility inputs for search and delete:

- `search(... current_xid ...)` ignores `current_xid`
- `remove(... current_xid ...)` ignores `current_xid`

That means the scored text path is not currently MGA-correct. It behaves like a search engine segment system layered beside the transaction system rather than inside it.

### 2.7 Planner and executor baseline

The planner does not currently expose a first-class text access path family. The system mostly relies on:

- generic index costing
- executor-side full-text routing
- a special path for `TSMATCH` over a `FULLTEXT` index

This creates three practical limitations:

- text family selection is not cost-based in a robust way
- donor-routed families do not all share the same planner contract
- ranked retrieval and boolean token filtering are not separated as different access-path shapes

### 2.8 Baseline conclusion

ScratchBird already has enough code to define Lane D, but it is split across incompatible assumptions:

- GIN is transaction-aware but not the dominant routed full-text runtime
- inverted search is feature-rich but not transaction-correct
- lifecycle code assumes one runtime while factory and executor code may open another

Lane D should therefore be treated as a contract unification effort first and an implementation expansion effort second.

## 3. Donor-engine research synthesis

### 3.1 PostgreSQL

PostgreSQL GIN contributes the clearest model for boolean and candidate-producing text access:

- keys and posting lists are first-class storage objects
- the operator class owns key extraction and query extraction
- exactness is explicit through a `recheck` contract
- `triConsistent` style three-valued reasoning allows pruning without pretending the index always proves truth
- pending-list maintenance exists as a write-optimization layer, not a separate semantics layer

Most importantly, PostgreSQL does not confuse "candidate production" with "final predicate truth." That separation is directly useful for ScratchBird.

### 3.2 OpenSearch

OpenSearch contributes the clearest model for ranked retrieval:

- analyzer choice is explicit and split between index-time and search-time roles
- BM25 is the default scoring baseline
- phrase-oriented features depend on position storage
- immutable segment publication and background merge are standard operational patterns

OpenSearch also demonstrates what ScratchBird must not import directly:

- refresh-based search visibility is not a substitute for MGA visibility
- acknowledged write visibility and search visibility are intentionally decoupled there, which is incompatible with a transactional engine unless adapted

### 3.3 MongoDB

MongoDB contributes useful policy constraints:

- text indexes are sparse by default with respect to documents that produce no tokens
- per-field weights are part of the contract
- wildcard text routing is a policy layer over key extraction, not a new storage theorem
- the planner insists on a controlled text-index selection rule rather than combining many text indexes loosely

MongoDB also illustrates a caution: local per-document scoring without robust corpus-wide statistics is operationally simpler, but it weakens ranking quality and should not be ScratchBird's primary model if BM25-style corpus metrics are available.

### 3.4 Cassandra

Cassandra contributes useful storage lifecycle patterns:

- immutable per-segment or per-SSTable publication
- explicit observability around build state and queryability
- analyzer-driven text options
- substring and prefix families as optional modes with real write-amplification costs

The main donor lesson is structural rather than semantic: immutable publication plus merge works well, but in ScratchBird the publish boundary must be snapshot-safe, not refresh-safe.

### 3.5 Synthesis

The donor engines converge on five stable ideas:

1. Boolean token access and ranked retrieval should not be treated as the same access path.
2. Recheck is a contract feature, not an implementation embarrassment.
3. Phrase and proximity semantics require positions or an explicit downgrade.
4. Analyzer choice is durable schema state.
5. Segment merge and deferred maintenance are allowed only if reader-visible semantics remain correct.

For ScratchBird, the correct synthesis is:

- borrow PostgreSQL-style candidate and recheck discipline for filter-first access
- borrow search-engine segment and scoring techniques for ranked access
- borrow MongoDB and Cassandra policy layers only as aliases or option bundles
- reject any donor visibility model that bypasses MGA rules

## 4. Primary literature and official-document synthesis

The primary literature and official documentation point in the same direction even though they come from different communities.

### 4.1 GIN and generalized inverted access

Foundational GIN literature and official documentation treat an inverted index as a two-stage structure:

- extract queryable keys from a composite value
- use the index to find candidate row identifiers
- let an exact consistency function or executor recheck settle the final truth when the key abstraction is lossy

This matters for ScratchBird because text search is inherently lossy once normalization, stop-word dropping, stemming, weighting, or phrase composition are involved. Lane D should therefore expose lossiness and recheck explicitly rather than implying all text index hits are exact.

### 4.2 Ranking literature

The classical probabilistic retrieval line behind BM25 yields a stable ranking packet:

- corpus size `N`
- document frequency `df`
- within-document frequency `tf`
- document length `dl`
- corpus average length `avgdl`

The practical implication is straightforward: ranked retrieval is not just "boolean filtering plus sorting." If ScratchBird keeps a scored path, it must maintain corpus and document statistics as first-class metadata.

### 4.3 Phrase and position literature

Official documentation and long-standing text-search practice agree on one rule: phrase and proximity semantics require positional evidence. If positions are not stored, the system must do one of the following:

- reject the query shape for that index
- downgrade to term-only candidate production with mandatory recheck against base data
- rewrite to a weaker predicate only when the syntax contract explicitly allows that downgrade

Silent downgrade is not acceptable for Lane D.

### 4.4 Segment publication literature

Search-engine and storage literature both support immutable-segment publication and background merge because they simplify read concurrency and write amplification. ScratchBird can adopt that structure, but only with a transactional publication fence:

- build in private generation state
- validate visibility coverage
- publish atomically
- retain old generations until no active snapshot can reference them

### 4.5 Literature-level implication for ScratchBird

The literature does not support one undifferentiated "full-text index" abstraction. It supports:

- a key-extraction access method with possible recheck
- a ranking-oriented retrieval path with corpus statistics
- explicit handling of phrase and analyzer semantics
- explicit lifecycle fencing

That is the right shape for the ScratchBird contract.

## 5. MGA and lifecycle correctness packet

### 5.1 Non-negotiable invariants

Lane D must obey these invariants:

1. Search visibility is determined by MGA rules, not by refresh timing.
2. No index path may return a row version invisible to the reader snapshot.
3. Delete and update must be represented as logical visibility changes before physical reclamation.
4. Merge, pending-list cleanup, and rebuild must preserve snapshot correctness across publication boundaries.
5. Donor-shaped aliases may change token policy or scoring policy, but they may not change transaction semantics.

### 5.2 Minimal visibility model

For a posting entry or document reference `p`, visibility should be defined as:

`visible(p, snapshot) := created_visible(p.xmin, snapshot) AND NOT deleted_visible(p.xmax, snapshot)`

If a segment-style structure uses generations instead of per-posting version windows, the equivalent rule is:

`visible(p, snapshot) := generation_visible(p.gen_min, p.gen_max, snapshot) AND tuple_visible(p.tid, snapshot)`

The second form is acceptable only if tuple revalidation is always preserved and generation retirement is snapshot-safe.

### 5.3 Required lifecycle states

Every Lane D structure should expose these states:

- `BUILDING`
- `QUERYABLE`
- `MERGING`
- `RETIRED_PENDING_SNAPSHOT_DRAIN`
- `RECLAIMABLE`

Current ScratchBird code already hints at cleanup and merge concepts, but those states are not yet expressed as a single contract.

### 5.4 Blocking current-state issues

The following issues block a correct Lane D implementation:

1. The scored inverted runtime ignores the transaction identifier passed into search and delete paths.
2. The catalog and migration layer still assumes GIN behavior for some families that are actually routed to the inverted runtime.
3. `FULLTEXT` currently behaves as a routing label more than a stable storage contract.
4. Complex `TSQuery` evaluation still depends on residual executor recheck, but planner and lifecycle contracts do not surface that clearly.

### 5.5 MGA packet requirements

The contract should require the following metadata:

- tuple or row identity
- version window or generation visibility range
- analyzer version identifier
- normalization policy identifier
- per-segment live/dead accounting
- publication generation

### 5.6 Physical reclamation rule

No posting or document record may be physically removed until both conditions hold:

- it is not visible to any active snapshot
- every dependent access structure has published a replacement generation or confirmed absence

This is the key point where search-engine segment practice must be adapted for ScratchBird.

### 5.7 Recheck contract

The index contract must surface:

- `exact = true` when the access method proves the predicate
- `exact = false` with `requires_recheck = true` when the access method only produces candidates

Recommended rule:

- pure token membership predicates may be exact if analyzer normalization is fully aligned
- phrase, proximity, weight-sensitive, or negation-heavy predicates should default to candidate plus recheck unless the access method proves them exactly

## 6. Optimizer metrics packet

### 6.1 Current state

Current ScratchBird text statistics are incomplete:

- the GIN `tsvector` operator class has a heuristic selectivity estimator
- the general statistics and cost model are not text-family aware
- the inverted runtime maintains useful corpus metadata, but the planner does not consume it as a first-class packet

### 6.2 Required metrics

Lane D should define a stable metrics packet with at least:

- `N_live_docs`
- `N_terms`
- `N_tokens`
- `avgdl`
- `df(term)`
- `posting_bytes(term)`
- `pending_entry_count`
- `segment_count_live`
- `segment_count_merging`
- `dead_doc_fraction`
- `recheck_ratio`
- `phrase_position_hit_rate`
- `topk_default`

### 6.3 Base selectivity formulas

Recommended first-pass formulas:

- term selectivity: `sel(t) = min(df(t) / N_live_docs, 1.0)`
- conjunction: `sel(AND(t1..tk)) ~= max(min_i(sel(ti)) * overlap_damp, product_i(sel(ti)))`
- disjunction: `sel(OR(t1..tk)) ~= 1 - product_i(1 - sel(ti))`
- phrase: `sel(PHRASE) ~= sel(AND(terms)) * position_hit_rate`
- negation-heavy query: `sel(NOT q)` should not be costed from the index alone; route through residual filter costing

A practical first-pass dampener is:

- `overlap_damp = pow(0.5, k - 1)`

That keeps conjunction estimates from collapsing unrealistically for correlated text terms.

### 6.4 Posting fanout and recheck estimates

Useful planning estimates:

- expected posting fanout for OR: `fanout_or = sum(df(ti))`
- expected candidate count: `cand = ceil(N_live_docs * sel(query))`
- expected rechecks: `rechecks = cand * recheck_ratio`
- expected visible hits: `hits = cand * visibility_survival_rate`

For phrase search:

- `position_hit_rate` should be collected empirically per analyzer family and query length bucket

### 6.5 Ranking metrics

The scored path needs additional statistics:

- `avg_tf(term)` or a bucketed proxy
- score cutoff distribution for recent `TOP K` workloads
- field weight metadata when fielded scoring is enabled

Without at least approximate `tf` and `df`, ranked access becomes little more than filtered scan plus unstable scoring.

### 6.6 Packet update timing

Metrics should update at:

- commit-time for cheap counters
- merge-time for consolidated corpus statistics
- analyze-time for sampled planner histograms

The planner should tolerate mild staleness, but visibility counters must remain transaction-correct.

## 7. Access-path and costing packet

### 7.1 Required access paths

Lane D should expose four planner-visible path shapes:

1. `GIN_FILTER_SCAN`
   - token membership or containment
   - returns exact hits or candidates

2. `TEXT_BITMAP_SCAN`
   - OR-heavy or wide boolean token predicates
   - accumulates candidate row identifiers cheaply

3. `TEXT_SCORE_SCAN`
   - ranked retrieval with `ORDER BY SCORE`, `LIMIT K`, or score-threshold predicates
   - may terminate early only when ranking contract allows it

4. `TEXT_RECHECK_SCAN`
   - residual phrase, weight, negation, or analyzer-sensitive predicate evaluation on candidate hits

### 7.2 First-pass costing formulas

Recommended cost components:

- `posting_pages = ceil(posting_bytes_touched / page_size)`
- `cand = ceil(N_live_docs * sel(query))`
- `rechecks = cand * recheck_ratio`
- `heap_fetches = cand * heap_survival_rate`

Filter-first cost:

`cost_filter = posting_pages * random_page_cost + cand * cpu_index_tuple_cost + rechecks * (cpu_operator_cost + cpu_visibility_cost) + heap_fetches * heap_fetch_cost`

Score-first cost:

`cost_score = posting_pages * random_page_cost + cand * (cpu_index_tuple_cost + cpu_score_cost) + rechecks * cpu_visibility_cost + heap_fetches * heap_fetch_cost`

Bitmap cost:

`cost_bitmap = posting_pages * random_page_cost + fanout_or * cpu_index_tuple_cost + bitmap_words * cpu_bitmap_cost + rechecks * (cpu_operator_cost + cpu_visibility_cost)`

Phrase cost:

`cost_phrase = cost_filter + cand * avg_positions_per_candidate * cpu_position_check_cost`

### 7.3 Path-choice heuristics

Recommended planner heuristics:

- choose `GIN_FILTER_SCAN` when the predicate is boolean, ranking is not required, and `cand << table_rows`
- choose `TEXT_BITMAP_SCAN` when OR fanout is large but a bitmap can compress candidate identity effectively
- choose `TEXT_SCORE_SCAN` when ranking is requested or `LIMIT K` is small relative to `cand`
- force `TEXT_RECHECK_SCAN` when query classification indicates `requires_recheck = true`
- fall back to sequential scan when stop-word stripping or high `df` collapses selectivity

### 7.4 Query-shape rules

Recommended rule table:

- single term, no ranking: `GIN_FILTER_SCAN`
- pure `AND` of terms, no ranking: `GIN_FILTER_SCAN`
- pure `OR` of many terms: `TEXT_BITMAP_SCAN`
- phrase or proximity with stored positions: `GIN_FILTER_SCAN` or `TEXT_SCORE_SCAN` plus `TEXT_RECHECK_SCAN`
- phrase without stored positions: no exact index proof; either reject or candidate plus base-data recheck
- ranked `TOP K`: `TEXT_SCORE_SCAN`
- negation-heavy predicate: candidate path plus mandatory residual filter

### 7.5 Present implementation gap

Current ScratchBird executor routing for text search is useful as a proof of feasibility, but it is not yet a planner contract. Lane D needs explicit access-path nodes and costing hooks rather than ad hoc executor shortcuts.

## 8. ScratchBird contract draft

### 8.1 Canonical family model

ScratchBird should standardize Lane D as:

- `GIN_TEXT`
  - boolean and containment-oriented token access
  - may return exact hits or candidates with recheck

- `INVERTED_TEXT`
  - ranked and analyzer-driven document retrieval
  - may also serve plain token search when ranking is enabled or preferred

- logical aliases
  - `FULLTEXT`
  - wildcard text families
  - donor-specific analyzer families

Aliases should compile to one of the two canonical physical families plus option bundles. They should not define separate storage semantics.

### 8.2 Family responsibilities

`GIN_TEXT` owns:

- token key extraction
- pending-list write optimization
- posting-list and posting-tree layout
- exact versus candidate classification
- efficient boolean access

`INVERTED_TEXT` owns:

- analyzer pipeline
- scored retrieval
- segment publication and merge
- optional positions, offsets, and payloads
- top-`K` retrieval behavior

### 8.3 `FULLTEXT` contract

`FULLTEXT` should become a policy alias, not an on-disk storage identity. Recommended lowering:

- boolean `tsvector` containment and `@@` forms without ranking intent -> `GIN_TEXT`
- free-text ranked retrieval, wildcard text, or field-weighted search -> `INVERTED_TEXT`

If a single parser surface must support both, the planner should lower it based on query semantics rather than family label alone.

### 8.4 Minimal schema contract

Every Lane D index should carry:

- analyzer identifier
- search analyzer identifier, if distinct
- normalization and stop-word policy
- stemming policy
- positions enabled flag
- offsets enabled flag
- scoring model identifier
- field-weight metadata, if any
- MGA visibility mode
- pending-list or segment merge configuration

### 8.5 Minimal query contract

At planning time, the query packet should expose:

- token set
- boolean structure
- phrase or proximity requirements
- ranking requested or not
- expected recheck requirement
- field restriction or wildcard scope

### 8.6 Exactness contract

Every Lane D access path must declare:

- `proof_level = EXACT | CANDIDATE`
- `requires_recheck = true | false`

Recommended defaults:

- exact only for predicates the access method can prove under the index's stored metadata
- candidate for phrase, weight-sensitive, analyzer-mismatch, or negation-heavy predicates unless a stronger proof exists

### 8.7 Publication contract

Recommended publication rule:

1. build postings or segments in a private generation
2. validate analyzer identity and visibility coverage
3. atomically publish the new generation
4. retain replaced generations until snapshot drain completes
5. reclaim old generations only after visibility safety is proven

### 8.8 Backward-compatibility implication

The current split between `FullTextIndex` and `InvertedIndex` suggests that one of them should become an adapter layer:

- either retain `FullTextIndex` only as a `GIN_TEXT` operator adapter
- or absorb it into a generic text operator-class layer and stop treating it as a separate family

The current three-way split is not maintainable as a long-term contract.

## 9. Validation and benchmark packet

### 9.1 Correctness gates

Lane D should ship with mandatory correctness gates:

- committed insert becomes searchable at the correct snapshot boundary
- uncommitted insert is never returned to another snapshot
- delete and update are not visible after the delete version becomes visible
- phrase queries never return false positives without `requires_recheck = true`
- analyzer mismatch is either rejected or explicitly rechecked
- merge and rebuild preserve snapshot-stable results

### 9.2 Differential truth testing

Every indexed query shape should be checked against a base-data truth evaluator:

- term
- `AND`
- `OR`
- phrase
- proximity
- negation
- wildcard text scope
- field-weighted scoring

Required assertion:

- `indexed_visible_results == heap_truth_results` after applying any mandatory recheck

### 9.3 Benchmark classes

Recommended benchmark families:

1. write-heavy pending-list and merge pressure
2. boolean term lookup on small and large `df`
3. ranked `TOP K`
4. phrase and proximity search with positions on and off
5. wildcard and path-expansion queries
6. substring or n-gram style routing
7. concurrent insert, update, delete, and query under mixed snapshots

### 9.4 Metrics to record

Record at least:

- p50, p95, p99 latency
- posting pages touched
- candidate count
- recheck count
- heap fetch count
- score computation count
- merge debt
- pending-list depth
- stale-hit count
- false-positive count before recheck
- false-negative count after recheck

### 9.5 Success thresholds

Minimum required thresholds:

- stale-hit count: `0`
- false-negative count: `0`
- false-positive count after required recheck: `0`
- ranked path must demonstrate stable score ordering for a fixed snapshot and statistics state

### 9.6 Performance heuristics

Useful first-pass targets:

- recheck ratio should stay low for common boolean term predicates
- score-first retrieval should beat filter-plus-sort for small `K` on large candidate sets
- pending-list or merge debt should not cause unbounded query latency spikes

## 10. Adopt/adapt/reject/defer matrix

| Pattern | Source family | Decision | Reason |
| --- | --- | --- | --- |
| Explicit `recheck` contract | PostgreSQL GIN | Adopt | Best fit for lossy token extraction under MGA. |
| Operator-class style key and query extraction | PostgreSQL GIN | Adopt | Clean boundary between storage and text semantics. |
| Pending-list write optimization | PostgreSQL GIN | Adapt | Useful, but cleanup must obey ScratchBird snapshot fences. |
| Three-valued consistency pruning | PostgreSQL GIN | Adapt | Valuable for complex `TSQuery`, but can follow after basic recheck contract lands. |
| BM25 baseline with `k1 = 1.2`, `b = 0.75` | Search-engine practice | Adopt | Strong default for ranked retrieval if true `tf` and `df` are maintained. |
| Separate index analyzer and search analyzer | OpenSearch style | Adopt | Necessary to make analyzer policy explicit and testable. |
| Refresh-based visibility | OpenSearch style | Reject | Conflicts with MGA correctness. |
| Immutable segment publication and background merge | OpenSearch and Cassandra style | Adapt | Good structure if publication and retirement are snapshot-safe. |
| Sparse text indexing for token-less rows | MongoDB style | Adapt | Good default for text families, but contract must define whether empty text is searchable as absence. |
| Field weights | MongoDB style | Adapt | Useful for ranked retrieval; should stay out of boolean exactness logic. |
| Exactly-one text index planning rule per text predicate scope | MongoDB style | Adopt | Prevents ambiguous routed text plans. |
| Local per-document scoring without robust corpus stats | MongoDB style | Reject as primary | Too weak for the ranked path when corpus stats are already available. |
| Storage-attached observability for build/queryable state | Cassandra SAI style | Adopt | Helpful for lifecycle diagnostics and validation. |
| Substring `CONTAINS` as default text behavior | Cassandra SASI style | Reject as default | Write amplification is too high for the default lane contract. |
| Prefix or substring modes as explicit optional families | Cassandra SASI style | Defer | Reasonable after the main boolean and ranked contracts stabilize. |
| Donor family names as storage identities | Multiple donors | Reject | ScratchBird should keep donor names as aliases or option bundles only. |

## 11. Open questions and integration dependencies

1. Should `FULLTEXT` lower to `GIN_TEXT`, `INVERTED_TEXT`, or both based on query shape?
2. Does ScratchBird want per-posting version windows in the scored path, generation fences plus tuple revalidation, or a hybrid?
3. Should `FullTextIndex` remain as an adapter over `GinIndex`, or should it dissolve into an operator-class layer?
4. What planner node names and result-shape contracts should be exposed for ranked versus boolean text access?
5. Where should analyzer definitions live in catalog metadata, and how are analyzer upgrades versioned?
6. Are field weights column-local, expression-local, or document-schema-local?
7. How should wildcard text families define path extraction, include/exclude rules, and path-depth caps under a common ScratchBird contract?
8. Which lifecycle component owns merge scheduling, snapshot-drain tracking, and old-generation reclamation?
9. How should phrase and proximity exactness be represented when positions are absent but the query syntax is still accepted?
10. What compatibility behavior is required for existing catalogs that already store text families under today's inconsistent routing?

## 12. Recommended next-step specification tasks

1. Write a canonical Lane D taxonomy spec defining `GIN_TEXT`, `INVERTED_TEXT`, and alias lowering rules.
2. Write an MGA visibility spec for text postings, segment generations, publication, retirement, and reclamation.
3. Write a planner spec for `GIN_FILTER_SCAN`, `TEXT_BITMAP_SCAN`, `TEXT_SCORE_SCAN`, and `TEXT_RECHECK_SCAN`.
4. Write a text statistics spec covering `df`, `tf`, `avgdl`, recheck ratio, phrase hit rate, and merge debt metrics.
5. Write an analyzer and normalization spec covering index-time analyzer, search-time analyzer, stop words, stemming, and versioning.
6. Write a query exactness spec defining `proof_level`, `requires_recheck`, and the allowed downgrade rules for phrase and analyzer-sensitive predicates.
7. Write a donor-alias mapping spec for wildcard, routed text, and analyzer-shaped families so donor names remain policy-only.
8. Write a backward-compatibility and migration spec that resolves the current runtime mismatch between factory routing and lifecycle assumptions.
9. Write a validation gate spec with differential truth tests, snapshot correctness tests, and ranked benchmark suites.
10. Write an implementation sequencing note that lands correctness first: visibility, routing unification, planner integration, then scoring refinement.

## Summary recommendation

ScratchBird should not continue treating Lane D as one loose bucket of "full-text-like" indexes. The correct design is:

- one MGA-correct boolean or candidate-producing token access family
- one MGA-correct ranked inverted family
- donor-shaped names only as aliases and policy bundles

The immediate engineering priority is not new syntax or new analyzers. It is to unify routing and lifecycle semantics so that every text-family path becomes transaction-correct, planner-visible, and explicit about exactness versus recheck.

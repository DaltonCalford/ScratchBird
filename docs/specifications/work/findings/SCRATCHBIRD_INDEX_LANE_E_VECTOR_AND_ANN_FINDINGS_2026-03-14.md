# ScratchBird Index Research Lane E Findings

Status: first pass

Date: 2026-03-14

This findings packet synthesizes ScratchBird live code and tests first, then donor evidence from Milvus, Neo4j, Redis, and OpenSearch official material where the local OpenSearch core clone did not expose the k-NN plugin internals. The goal is not to preserve every compatibility label as an independent implementation. The goal is to define an honest, MGA-safe, optimizer-visible contract for ScratchBird vector and ANN indexing.

## 1. Scope and lane objective

Lane E covers ScratchBird vector and ANN families:

- exact vector baselines: `VECTOR_FLAT`, `VECTOR_BIN_FLAT`
- graph ANN: `HNSW`
- partitioned or quantized labels: `IVF`, `IVF_FLAT`, `BIN_IVF_FLAT`, `IVF_PQ`, `IVF_SQ8`, `IVF_SQ8_HYBRID`, `RHNSW_PQ`, `RHNSW_SQ`
- routed or compatibility labels: `DISKANN`, `SCANN`, `ANNOY`, `NSG`, `GPU_CAGRA`, `NEO4J_VECTOR`

Lane E objective:

- identify which labels correspond to real physical runtimes and which are routed aliases
- define the lifecycle, metadata, optimizer, and validation contract needed for production-grade vector indexing
- determine what ScratchBird should adopt directly, adapt carefully, reject, or defer
- turn the current vector surface from parser-first compatibility into an implementation-honest family contract

In scope for this pass:

- ScratchBird live runtime, parser, executor, catalog, and tests
- donor-engine lessons from Milvus, Neo4j, Redis, and OpenSearch
- primary literature for HNSW, DiskANN, FreshDiskANN, and ScaNN
- MGA rules, maintenance, costing, and benchmark expectations

Out of scope for this pass:

- embedding-model selection
- distributed ANN or cluster-routing semantics
- GPU execution details beyond first-pass family gating
- SQL surface polish beyond what is needed to make the optimizer and contract coherent

Primary finding:

- ScratchBird already has a broad canonical specification surface for vector indexes, but the live engine currently exposes one meaningful ANN runtime prototype, `HNSW`, plus a wide set of routed labels. Lane E should therefore begin by making the family contract honest before multiplying family count.

## 2. ScratchBird current-state baseline

Current-state baseline should be read as "implemented surface plus material gaps", not "finished family contract".

| Area | Observed baseline | Contract implication |
| --- | --- | --- |
| Canonical spec surface | Section 18 already defines shared MGA rules plus dedicated specs for `HNSW`, `VECTOR_FLAT`, `IVF`, `IVF` variants, and `SCANN`. | The target contract already exists at the specification level; the implementation gap is now the main issue. |
| Family registry | The index registry maps many vector labels, including `IVF`, `VECTOR_FLAT`, `IVF_PQ`, `IVF_SQ8`, `RHNSW_PQ`, `RHNSW_SQ`, `DISKANN`, `SCANN`, `GPU_CAGRA`, and `NEO4J_VECTOR`, to the same runtime class. | ScratchBird currently has one effective vector runtime family, not many. Catalog labels must not be mistaken for separate implementations. |
| Parser surface | The parser accepts a wide range of vector and ANN family names. | Compatibility naming is ahead of physical implementation. |
| DDL option validation | The executor validates rich option sets for HNSW, IVF, SCANN, DISKANN, Neo4j-style vector labels, and quantized variants. | Accepted options look broader than the durable behavior actually provided by the runtime. |
| Parameter persistence | Persisted index parameters currently cover Bloom metadata and a separate array-uniqueness setting. Vector-family options are not durably persisted as authoritative index metadata. | `CREATE INDEX` options for vector families are not yet a real contract. |
| Runtime creation | HNSW create/open logic uses hardcoded defaults for metric, `M`, `ef_construction`, and `ef_search`, while dimensions come from column metadata rather than from a persisted family packet. | Current vector DDL is validator-only in many cases; runtime behavior is still default-driven. |
| Query surface | A low-level `VECTOR ANN QUERY INDEX ... METRIC ... TOPK ... EF_SEARCH` surface exists. | This is a diagnostic or executor-adjacent surface, not a usable relational optimizer contract. |
| DML routing | Inserts and removals for HNSW and most routed vector labels use the same runtime. | Alias families are currently implemented as HNSW routing, not as dedicated families with distinct storage and cost behavior. |
| Quantization support | A vector quantization module exists with SQ, PQ, OPQ, and binary modes. | Quantization capability exists in isolation, but it is not yet integrated into a real vector-family storage contract. |
| Optimizer integration | Vector families are not first-class access paths in the normal relational optimizer. | Lane E needs dedicated path applicability, stats, and costing rules. |

Most important baseline conclusion:

- ScratchBird should treat current `HNSW` code as a strong prototype, not as a finished Lane E contract.
- ScratchBird should treat current routed vector labels as compatibility surface, not as evidence of independent runtimes.
- ScratchBird should not claim durable family-specific vector behavior until the metadata, runtime, and optimizer all reflect the same contract.

Material baseline gaps that affect the specification immediately:

- HNSW node storage writes raw float payload bytes, while search-time decoding expects the structured vector-binary format with a type and dimension header. This is a correctness bug, not a tuning issue.
- Entry-point and max-layer discovery do not fully align with the multi-page sibling-chain layout, so larger indexes can start from non-authoritative graph state.
- The level-0 search loop stops when the result heap reaches `ef`, which is shallower than the canonical HNSW stopping rule based on candidate distance versus worst current result.
- Metric naming is inconsistent across surfaces:
  - DDL validation uses `L2`, `COSINE`, `INNER_PRODUCT`
  - the low-level query surface uses `DOT`
  - runtime and tests expose `DOT_PRODUCT` and `MANHATTAN`
- Quantized and routed labels such as `RHNSW_PQ`, `RHNSW_SQ`, `IVF_PQ`, and `IVF_SQ8` are mostly surface claims today, not distinct persisted runtime modes.
- Current user-visible results still depend on heap visibility recheck, which is correct for MGA but also shows that current vector search is not yet a planner-native path with formal candidate semantics.

Net baseline:

- ScratchBird already has enough code and canonical spec material to define a real first-wave vector design.
- ScratchBird does not yet have enough durable metadata, runtime separation, or optimizer integration to claim a complete Lane E implementation.

## 3. Donor-engine research synthesis

Milvus, Neo4j, Redis, and OpenSearch converge on a useful pattern: separate family classes honestly, separate build knobs from search knobs, version metadata explicitly, and treat disk or quantized ANN as a different resource and lifecycle class rather than as a label swap.

| Donor | Strongest lessons | Limits of direct reuse | Lane E value |
| --- | --- | --- | --- |
| Milvus | Clear separation of build-time and search-time parameters; centralized type-aware validation; explicit capability flags such as training required, disk-aware, mmap-aware, and multi-vector support; disk ANN treated as a separate resource class. | Milvus depends on Knowhere and its own segment/load architecture, so ScratchBird cannot copy the machinery directly. | Primary donor for capability matrices, parameter staging, and honest family differentiation. |
| Neo4j | Versioned vector-index contract; explicit validators for dimensions, similarity function, quantization, and HNSW bounds; query-time dimensionality validation; quantization introduced as a versioned capability rather than a free-form alias. | Neo4j delegates much of the storage/runtime behavior to Lucene KNN internals and procedure-style query entry points. | Primary donor for metadata versioning, open-time validation, and feature gating. |
| OpenSearch | Official method docs distinguish HNSW and IVF surfaces cleanly; IVF training is explicit; filtering strategy is documented; disk-based vector search is modeled as a compression and rerank policy rather than as "just another HNSW knob". | The local OpenSearch core clone did not provide the k-NN plugin internals for code-level inspection, so this donor is doc-led in this pass. | Strong donor for access-path framing, filter placement, and disk/rescore contract design. |
| Redis | Operationally honest vector-set contract with explicit HNSW parameters, optional quantization, filtered search effort controls, and a full exact "truth" mode for recall measurement. | Redis is not MGA-based and reclaims deletes immediately, so its lifecycle rules do not transfer directly. | Strong donor for benchmark design, filter-effort thinking, and exact-versus-approx instrumentation. |

Cross-donor synthesis:

- Build parameters and search parameters must be stored separately.
- Family capability must be explicit:
  - exact versus approximate
  - train-free versus train-required
  - memory-resident versus disk-assisted
  - raw versus quantized payload
- Disk ANN is a separate cost and maintenance class, not a synonym for graph ANN.
- Quantization should be a persisted capability or version field, not an untracked label.
- Approximation may affect candidate generation and coarse ranking, but exact rerank and visibility semantics must remain explicit.
- Filtered ANN needs an honest contract for whether filtering happens before traversal, during traversal, after traversal, or by oversampling plus rerank.

## 4. Primary literature and official-document synthesis

The HNSW, DiskANN, FreshDiskANN, and ScaNN literature, together with official OpenSearch documentation, converges on a stable set of laws that ScratchBird should elevate into normative Lane E text.

| Law | Synthesis | ScratchBird implication |
| --- | --- | --- |
| Hierarchical-routing law | "Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs" shows that search quality depends on a sparse upper-layer routing structure plus a denser base layer. | HNSW metadata must persist authoritative entry-point and max-layer state across the full graph, not just one page view. |
| Candidate-frontier stopping law | Canonical HNSW search stops when the best remaining candidate cannot improve the worst result in the bounded result heap. | ScratchBird must use the canonical stopping criterion; stopping merely because `top_k` reached `ef` undercuts recall. |
| Probabilistic-level law | HNSW level assignment follows an exponential tail. A useful first-order heuristic is `P(level >= l) ~= M^(-l)`. | Planner stats and validation should track whether observed level distribution drifts materially from expectation after churn or corruption. |
| Partition-training law | IVF-style methods depend on trained partitions or centroids. OpenSearch documentation states that IVF requires training before normal use. | ScratchBird must model IVF and IVF-derived families as train-and-publish families with explicit artifact metadata. |
| Approximation-separation law | Approximate methods accelerate candidate generation, not truth itself. Official ANN docs consistently describe a latency-recall tradeoff, not a relaxed correctness model. | ScratchBird must keep MGA visibility, exact predicate recheck, and exact final-distance semantics separate from ANN candidate generation. |
| Quantization-contract law | "Accelerating Large-Scale Inference with Anisotropic Vector Quantization" and production systems using quantized HNSW show that compressed codes alter distance evaluation behavior and rerank needs. | Quantized families need explicit payload-mode metadata, codebook artifacts, and rerank policy; they cannot be silent aliases of raw-vector HNSW. |
| Disk-latency law | "DiskANN: Fast Accurate Billion-point Nearest Neighbor Search on a Single Node" and official disk-based vector docs show that disk-assisted ANN is governed by random-read count, cache locality, and rerank oversampling. | ScratchBird must cost disk ANN in terms of I/O and cache behavior, not in-memory graph heuristics alone. |
| Freshness law | "FreshDiskANN: A Fast and Accurate Graph-Based ANN Index for Streaming Similarity Search" shows that online mutation in disk-resident ANN requires dedicated freshness machinery. | ScratchBird should not promise MGA-safe online DISKANN behavior until it has a real delta-and-shadow design. |
| Filter-placement law | Official OpenSearch filtering guidance and Redis filter-effort design both show that filter placement materially affects recall and latency. | ScratchBird ANN paths need filter-aware costing and must state whether filtering is pre-traversal, in-traversal, or post-traversal. |

Practical literature-derived heuristics for first-pass specification text:

- HNSW search work can be modeled initially as:
  - `Nvisit ~= a0 + a1 * ef_search + a2 * log2(max(Nlive, 2))`
- IVF scan work can be modeled initially as:
  - `avg_list_len = Nlive / max(nlist, 1)`
  - `Nscan ~= nprobe * avg_list_len`
- Quantized or partitioned methods require exact rerank when approximate code distance is not itself the exposed user-visible score:
  - `Nrerank = min(reorder_k, Ncandidate)`
- Disk-assisted ANN should expose:
  - `C_io ~= Nio * c_random_read`

These formulas are calibration heuristics, not correctness invariants.

## 5. MGA and lifecycle correctness packet

Lane E must remain MGA-native. Approximation may affect which candidates are explored first or which coarse structures are scanned. Approximation must never weaken visibility rules or allow physically convenient but snapshot-incorrect deletion.

Required invariants:

| Invariant | Required rule | Immediate consequence |
| --- | --- | --- |
| Visibility truth source | Every ANN result is a candidate row version and must pass MGA visibility. | Vector indexes may accelerate candidate generation but may not replace heap or version truth. |
| Append-only update rule | Vector index update is logically `insert new row version, tombstone old row version, reclaim later`. | ANN families must not physically unlink older versions at delete time. |
| Publish-by-generation build rule | Build, retrain, and major reorganization must publish a new generation atomically. | IVF retraining, HNSW graph rebuild, and quantizer replacement should use shadow publish rather than in-place mutation of the only visible structure. |
| Artifact immutability rule | Search must use the exact centroid, codebook, graph-format, and metric contract of the published generation. | Training artifacts must be versioned catalog state, not session-local defaults. |
| Exactness-after-candidate rule | If a family is lossy or approximate, the executor must know that rerank or exact distance confirmation is still required. | ANN access paths need an explicit `needs_rerank` or equivalent flag. |
| Reclaim-after-horizon rule | Physical reclaim may occur only after the oldest active snapshot is newer than the entry death horizon. | Redis-style immediate removal is not acceptable under ScratchBird MGA. |
| Structural-honesty rule | Graph or partition degradation may lower quality and raise cost, but it must not make visible rows unreachable forever once a rebuild is required by contract. | Lane E needs quality-debt metrics and rebuild triggers, not just page-level GC. |

Lifecycle packet by family:

| Family | Insert/update posture | Delete posture | Rebuild or retrain posture |
| --- | --- | --- | --- |
| `VECTOR_FLAT` | Straight append of exact vector entry per visible row version. | Tombstone old version; reclaim after MGA horizon. | Rebuild mainly for compaction or format upgrade. |
| `HNSW` | Online insert is acceptable if the graph update uses the published parameter packet. | Mark dead; do not physically unlink from the graph in the fast path. | Use shadow rebuild when delete debt, orphaning, or recall drift exceeds threshold. |
| `RHNSW_*` | Same as HNSW, but only if the quantized payload format is persisted and validated. | Same tombstone rule as HNSW. | Rebuild required when codebooks or quantization metadata change. |
| `IVF_*` | Insert assigns the row version to the current centroid generation and stores payload according to the variant. | Tombstone old entry inside its posting list or list segment. | Retraining should build a shadow centroid generation and republish. |
| `SCANN` | Treat as train-and-publish unless a real online delta path exists. | Tombstone compatible entries; avoid in-place retraining. | Rebuild or retrain as a new published generation. |
| `DISKANN` | Defer online mutation promises until there is an explicit log-structured or shadow-merge design. | Same MGA tombstone rule if exposed later. | First viable contract should be build-and-publish, not in-place freshness claims. |

Recommended first-pass maintenance thresholds:

- `dead_fraction = dead_vectors / max(total_vectors, 1)`
- `deletion_debt = dead_vectors / max(live_vectors, 1)`
- `centroid_drift = avg(dist(x, centroid(x))) / max(baseline_quant_error, eps)`
- `graph_rebuild_required` when any of the following hold:
  - `dead_fraction > 0.20`
  - `candidate_amplification_p95` rises materially above the build baseline
  - sampled `recall_estimate_at_k` falls below target by more than `epsilon`
- `ivf_retrain_required` when:
  - `centroid_drift > 1.25`
  - list skew exceeds policy
  - or mutation count since training exceeds a configured fraction of `Nlive`

These thresholds are initial operational heuristics and should be calibrated by benchmark evidence.

Immediate lifecycle conclusion:

- HNSW is the only vector family whose live mutation story is meaningfully prototyped today.
- Even there, ScratchBird still needs a formal delete-debt, rebuild, and graph-quality contract before the family is production-honest.

## 6. Optimizer metrics packet

Lane E needs family-aware statistics because ANN cost is driven by candidate inflation, dead-entry debt, graph or partition skew, and rerank effort, not just row count.

Recommended persistent metrics:

| Metric | Formula or definition | Use |
| --- | --- | --- |
| `live_vectors` | Count of live visible vector entries in the current generation. | All families. |
| `dead_vectors` | Count of tombstoned but not yet reclaimed vector entries. | All families. |
| `dead_fraction` | `dead_vectors / max(live_vectors + dead_vectors, 1)` | Maintenance urgency and cost inflation. |
| `vector_dim` | Persisted dimension for the family generation. | Applicability validation. |
| `metric_id` | Canonical metric enum for the family generation. | Applicability and score semantics. |
| `candidate_count_p50`, `candidate_count_p95` | Sampled candidate counts examined per query before final qualification. | Generic ANN costing. |
| `distance_evals_p50`, `distance_evals_p95` | Sampled distance computations per query. | CPU costing. |
| `heap_fetch_ratio` | `heap_fetches / max(candidates_returned, 1)` | Recheck and rerank cost. |
| `recheck_ratio` | `rechecked_candidates / max(candidates_returned, 1)` | Lossiness visibility. |
| `recall_estimate_at_k` | `|approx_topk ∩ truth_topk| / k` on sampled truth runs. | Planner confidence and health. |
| `candidate_amplification` | `candidates_examined / max(k, 1)` | Distinguishes efficient ANN from degraded ANN. |
| `generation_age_ops` | Number of row mutations since build or retrain publish. | Freshness penalty. |
| `generation_age_time` | Time since build or retrain publish. | Operational visibility. |

HNSW and routed-graph metrics:

| Metric | Formula or definition | Use |
| --- | --- | --- |
| `graph_pages` | Total pages storing HNSW nodes. | Storage footprint and cache residency. |
| `max_level` | Highest published layer. | Search-entry behavior. |
| `avg_level` | Mean node level. | Detects level-distribution drift. |
| `avg_degree_l0` | Mean base-layer neighbor count. | Search breadth proxy. |
| `avg_degree_upper` | Mean upper-layer neighbor count. | Routing quality proxy. |
| `orphan_fraction` | Fraction of nodes with below-threshold reachable degree or poor connectivity. | Rebuild urgency. |
| `visited_nodes_p50`, `visited_nodes_p95` | Sampled visited-node counts during search. | HNSW cost model. |

IVF and partitioned-family metrics:

| Metric | Formula or definition | Use |
| --- | --- | --- |
| `nlist` | Published coarse partition count. | Applicability and cost. |
| `nprobe_default` | Published default list-probe count. | Default effort and cost. |
| `avg_list_len` | `live_vectors / max(nlist, 1)` | First-order scan estimate. |
| `max_list_len` | Length of largest live list. | Skew penalty. |
| `list_cv` | `stddev(list_len) / max(avg_list_len, 1)` | Skew inflation. |
| `training_sample_size` | Rows used for the current training generation. | Quality auditability. |
| `centroid_drift` | Current drift relative to training-time distortion baseline. | Retrain urgency. |
| `code_bytes_per_vector` | Average compressed payload size. | Memory and rerank cost. |
| `rerank_ratio` | `reranked_candidates / max(candidates_examined, 1)` | Exact rerank burden. |

SCANN and disk-family metrics:

| Metric | Formula or definition | Use |
| --- | --- | --- |
| `reorder_k_default` | Published exact-rerank bound. | Score accuracy and cost. |
| `partition_skew` | Relative imbalance among coarse partitions. | Partition cost inflation. |
| `aq_code_bytes` | Bytes of anisotropic or quantized code per vector. | Storage and CPU cost. |
| `search_list_default` | Default disk or beam-style search list width. | DISKANN effort. |
| `ssd_reads_per_query` | Sampled random read count. | Disk ANN cost. |
| `cache_hit_ratio` | `cache_hits / max(cache_hits + cache_misses, 1)` | Disk-path viability. |
| `resident_fraction` | Fraction of hot graph or code data resident in memory. | Path selection. |

Recommended optimizer heuristics:

- Inflate ANN cost by stale or degraded structure:
  - `degrade_mult = 1 + w_dead * dead_fraction + w_age * freshness_penalty + w_skew * skew_penalty`
- Inflate IVF scan work by list skew:
  - `Nscan_adj = Nscan * (1 + list_cv^2)`
- Penalize lossy or approximate families when filters are selective but not natively supported:
  - `post_filter_mult ~= 1 / max(filter_selectivity, eps)`
- If `recall_estimate_at_k` is absent, the optimizer should either:
  - require an explicit ANN opt-in mode, or
  - apply conservative path penalties until evidence exists

## 7. Access-path and costing packet

Lane E needs first-class vector access paths rather than a generic executor escape hatch.

Recommended path families:

| Access path | Preconditions | Main knobs | Candidate semantics |
| --- | --- | --- | --- |
| `VECTOR_EXACT_SCAN` | Metric supported, vector dimension compatible, exact scan or exact rerank required. | `k`, optional radius, optional batch size. | Exact top-k or exact range candidates. |
| `HNSW_KNN_SCAN` | Active HNSW generation, metric supported, explicit `k` or radius, graph stats present or conservative defaults. | `ef_search`, optional oversample factor. | Approximate candidate generation followed by MGA visibility and exact qualification. |
| `IVF_KNN_SCAN` | Active trained centroid generation, metric supported, explicit `k` or radius. | `nprobe`, optional `reorder_k`. | Coarse partition probe plus rerank. |
| `SCANN_KNN_SCAN` | Active trained quantizer or partition generation, explicit rerank policy. | `num_leaves_to_search`, `reorder_k`. | Partitioned approximate codes plus exact rerank. |
| `DISKANN_KNN_SCAN` | Active disk-aware generation with cache policy and I/O stats. | `search_list`, cache or oversample policy. | Disk-assisted approximate traversal plus exact rerank. |

Applicability rules:

- ANN paths should require:
  - a recognized distance expression or vector search operator
  - explicit `LIMIT k`, `TOP K`, or a bounded radius predicate
  - dimension and metric compatibility
  - an `active` family generation
- ANN paths should not be chosen when:
  - full total ordering is required without a bound
  - family metadata is incomplete or stale enough that recall cannot be bounded operationally
  - the query requires exact results and no exact-rerank plan is available

Generic cost model:

- `C_total = C_start + C_candidates + C_distance + C_filter + C_heap + C_rerank + C_io`
- `C_candidates = Nvisit * c_node`
- `C_distance = Ndist * c_dist`
- `C_filter = Ncand * c_filter`
- `C_heap = Nheap * c_heap_fetch`
- `C_rerank = Nrerank * c_exact_distance`
- `C_io = Nio * c_random_read`

HNSW first-pass cost model:

- `Nvisit ~= a0 + a1 * ef_search + a2 * log2(max(Nlive, 2))`
- `Ndist ~= b0 + b1 * Nvisit`
- `Nvisit_adj = Nvisit * (1 + w_dead * dead_fraction + w_orphan * orphan_fraction)`
- `C_hnsw = C_entry + Nvisit_adj * c_node + Ndist * c_dist + Nheap * c_heap_fetch + Nrerank * c_exact_distance`
- Initial recall heuristic for planner calibration only:
  - `Recall(k) ~= 1 - exp(-beta * ef_search / max(M, 1))`

IVF first-pass cost model:

- `avg_list_len = Nlive / max(nlist, 1)`
- `Nscan = nprobe * avg_list_len`
- `Nscan_adj = Nscan * (1 + list_cv^2)`
- `C_ivf = C_centroids + nprobe * c_list_open + Nscan_adj * c_code_distance + Nrerank * c_exact_distance + Nheap * c_heap_fetch`

SCANN first-pass cost model:

- `C_scann = C_partitions + Ncodes * c_aq_distance + Nrerank * c_exact_distance + Nheap * c_heap_fetch`
- `Nrerank = min(reorder_k, Ncandidate_kept)`

DISKANN first-pass cost model:

- `C_diskann = C_cache + Nio * c_random_read + Ndist * c_dist + Nrerank * c_exact_distance + Nheap * c_heap_fetch`
- `Nio` should be inflated if `resident_fraction` or `cache_hit_ratio` falls below policy.

Filter-aware costing rules:

- If the family supports prefilter or in-traversal filtering, model filtered candidate generation directly.
- If filtering is post-traversal only, inflate work:
  - `Ncand_eff ~= Ncand / max(filter_selectivity, eps)`
- If `Ncand_eff` exceeds the exact scan candidate count by policy, the planner should prefer `VECTOR_EXACT_SCAN`.

Immediate costing conclusion:

- ScratchBird should not expose `SCANN` or `DISKANN` as planner-visible paths until it has dedicated metrics for partitioned quantization or disk I/O behavior.
- HNSW and exact vector scan are the only plausible first-wave planner-visible Lane E paths.

## 8. ScratchBird contract draft

The contract should distinguish physical family, routed compatibility label, payload mode, and search defaults.

Recommended first-wave physical families:

| Physical family | Contract status | Required persisted fields |
| --- | --- | --- |
| `VECTOR_FLAT` | First-wave exact baseline. | `format_version`, `metric`, `vector_dim`, payload format, optional binary flag. |
| `HNSW` | First-wave approximate graph family. | `format_version`, `metric`, `vector_dim`, `M`, `M0`, `ef_construction`, `ef_search_default`, `seed`, graph layout version. |
| `IVF` | First-wave partition family if training artifacts are implemented durably. | `format_version`, `metric`, `vector_dim`, `nlist`, `nprobe_default`, centroid artifact id, optional quantizer mode. |

Recommended routed-label policy:

| Label or group | Recommended contract |
| --- | --- |
| `IVF_FLAT`, `BIN_IVF_FLAT` | Accept only if they map to a real `IVF` physical family with persisted payload mode and metric compatibility. |
| `IVF_PQ`, `IVF_SQ8`, `IVF_SQ8_HYBRID` | Accept only when quantizer mode, codebook artifact, rerank policy, and payload bytes are all persisted and enforced. Otherwise reject rather than silently route. |
| `RHNSW_PQ`, `RHNSW_SQ` | Treat as HNSW physical submodes only when compressed payload behavior is real and open-time validation exists. |
| `NEO4J_VECTOR` | Keep as a dialect surface that maps to supported physical family modes and persists the actual physical family chosen. |
| `SCANN`, `DISKANN`, `ANNOY`, `NSG`, `GPU_CAGRA` | Parseable compatibility names may remain, but create-time should reject or mark unsupported until dedicated metadata, runtime, and costing exist. |

Required persisted metadata packet for all Lane E families:

- `family_id`
- `family_mode`
- `format_version`
- `metric`
- `vector_dim`
- `build_knobs`
- `search_defaults`
- `payload_mode`
- `artifact_generation`
- `training_artifact_id` when applicable
- `quality_baseline` summary at build time
- `state` and maintenance counters

Contract rules ScratchBird should adopt immediately:

- A vector-family `CREATE INDEX` option is part of the contract only if it is:
  - validated,
  - persisted,
  - reopened correctly,
  - and reflected in search behavior
- If any of those steps is missing, ScratchBird should reject the option or reject the family mode rather than silently ignore it.
- External metric names should be canonicalized:
  - store `l2`, `cosine`, `inner_product`
  - accept parser synonyms such as `dot` only as aliases to the canonical stored value
- Vector payload encoding must be canonical and uniform across:
  - insert
  - search
  - reopen
  - tests
  - rebuild

Recommended query contract:

- The relational surface should be driven by vector distance predicates or ordering, not by a low-level index-id query form.
- The low-level `VECTOR ANN QUERY` surface may remain as a diagnostic or executor-adjacent facility, but it should not be the planner-facing contract.
- Planner-visible vector search should carry:
  - metric
  - `k` or radius
  - search effort (`ef_search`, `nprobe`, `reorder_k`, `search_list`)
  - expected candidate count
  - whether exact rerank occurs in the access method or executor

Immediate implementation implications from the draft contract:

- Fix the HNSW vector payload encoding mismatch before treating current HNSW behavior as authoritative.
- Move HNSW entry-point and max-layer authority into published metadata or a fully authoritative graph walk.
- Persist family-specific vector options in catalog metadata instead of validating and discarding them.
- Reject unsupported routed labels until the physical runtime and metadata exist.

## 9. Validation and benchmark packet

Lane E requires both correctness validation and search-quality benchmarking. Exact vector scan is the ground-truth baseline.

Required benchmark baselines:

- `VECTOR_FLAT` exact scan for truth top-k
- the candidate ANN family under test
- identical metric, dimension, and filter semantics across both paths

Required result metrics:

| Metric | Formula or definition |
| --- | --- |
| `Recall@K` | `|approx_topk ∩ truth_topk| / K` |
| `NDCG@K` | Optional ranking-quality metric when order within the top-k matters. |
| `Latency p50/p95/p99` | End-to-end search latency by path and workload. |
| `Build throughput` | Indexed rows per second during build. |
| `Bytes per live vector` | `storage_bytes / max(live_vectors, 1)` |
| `Candidate amplification` | `candidates_examined / max(K, 1)` |
| `Write amplification` | `physical_writes / max(logical_dml_ops, 1)` |
| `Deletion debt` | `dead_vectors / max(live_vectors, 1)` |
| `Rerank burden` | `reranked_candidates / max(candidates_examined, 1)` |

Required workload classes:

- clustered embeddings
- near-uniform random vectors
- high-duplicate or near-duplicate vectors
- large-dimension embeddings, including the 1536-dimension case already visible in current tests
- filtered top-k search with selective and non-selective predicates
- churn-heavy insert/update/delete workloads
- stale-training workloads for IVF-like families
- degraded-state workloads with high tombstone fraction

Required correctness and degradation tests:

- dimension mismatch must fail cleanly
- metric mismatch must fail cleanly
- MGA visibility under concurrent readers and deletes must remain correct
- HNSW rebuild must remove dead entries without dropping still-visible versions
- IVF retrain publish must not expose mixed centroid generations
- exact-versus-approx truth comparison must be available for sampled health runs
- corrupted or incompatible family metadata must force `invalid` or rebuild-required state

Recommended benchmark discipline:

- report truth and approximate results together
- record the exact family packet used for the run
- collect structure metrics at the start and end of the run
- benchmark both healthy and degraded states
- treat recall regression and visibility regression differently:
  - recall regression is a quality failure
  - visibility regression is a correctness failure

Redis contributes the key operational idea here:

- ScratchBird should have a truth-mode benchmark workflow for ANN families so recall and candidate amplification are measured continuously rather than guessed.

## 10. Adopt/adapt/reject/defer matrix

| Item | Decision | Reason | Contract implication |
| --- | --- | --- | --- |
| Exact `VECTOR_FLAT` baseline | Adopt | Every ANN family needs a truth baseline for correctness and benchmarking. | Make `VECTOR_FLAT` the required truth path for Lane E validation and small-`k` fallback planning. |
| HNSW layered graph with explicit `M`, `ef_construction`, and `ef_search` | Adopt | The current runtime and literature both support this as the natural first-wave ANN family. | Make HNSW the first production ANN family once metadata persistence and correctness gaps are fixed. |
| IVF build-time versus search-time separation | Adopt | Milvus and OpenSearch both treat training and probing as separate contract phases. | Define `IVF` as a train-and-publish family with explicit centroid artifacts. |
| Versioned vector metadata and open-time validation | Adopt | Neo4j shows this is necessary for safe feature evolution. | Add `format_version`, payload mode, and artifact version checks to Lane E open paths. |
| Quantization as a persisted family mode | Adapt | Quantization is useful but only if its payload and rerank semantics are explicit. | Model PQ and SQ as persisted submodes, not free labels. |
| Filter-aware ANN costing and rerank policy | Adapt | OpenSearch and Redis show that filter placement changes both recall and cost. | Add prefilter, in-traversal, and post-filter contract flags. |
| Truth-mode recall measurement | Adapt | Redis provides a clean operational precedent. | Add sampled exact-truth runs to validation and health scans. |
| Silent routing of every vector label to one HNSW runtime | Reject | It hides real runtime and maintenance behavior. | Either persist the actual physical family chosen or reject unsupported labels. |
| Immediate physical delete | Reject | It violates MGA lifecycle rules. | Use tombstones plus reclaim-after-horizon for all Lane E families. |
| Using low-level `VECTOR ANN QUERY` as the optimizer contract | Reject | It is too narrow and index-id-centric. | Move planner-visible vector access to normal predicate and ordering semantics. |
| `SCANN` dedicated runtime in first wave | Defer | ScratchBird lacks anisotropic quantization, partitioned rerank, and metadata support. | Keep the label parseable if needed, but reject create-time activation until dedicated runtime exists. |
| `DISKANN` dedicated runtime in first wave | Defer | ScratchBird lacks disk-aware traversal, cache metrics, and a freshness design compatible with MGA. | Treat DISKANN as future work behind a dedicated spec and storage design. |
| `ANNOY`, `NSG`, `GPU_CAGRA` first-wave exposure | Defer | No dedicated runtime or costing exists today, and they add breadth faster than they add value. | Keep them out of the first production contract. |

## 11. Open questions and integration dependencies

Open questions:

- What is the authoritative persisted metadata structure for Lane E family packets?
- Should exact rerank use heap-fetched vectors, index-stored raw vectors, or both depending on payload mode?
- Which vector datum encoding is canonical across heap, index payload, and function calls?
- What is the planner-visible SQL or SBLR representation for vector distance order-by, top-k, and bounded-radius search?
- Does ScratchBird want first-wave binary-vector support, or should `VECTOR_BIN_FLAT` and binary IVF variants wait until the float contract is stable?
- What is the exact policy for post-filter oversampling when ANN traversal cannot apply the filter directly?
- How should sampled `recall_estimate_at_k` be stored and refreshed:
  - `ANALYZE`
  - health scan
  - benchmark-only
  - or some combination
- What is the first supported score contract:
  - raw distance
  - normalized similarity
  - or family-specific internal score plus exact final score
- Should `SCANN` be specified as a future separate physical family or as a later `IVF`-derived subfamily?
- Can `NEO4J_VECTOR` remain purely a dialect surface, or does compatibility pressure require a dedicated published mode matrix?

Integration dependencies:

- catalog metadata extension for persisted Lane E family packets
- canonical vector type and binary encoding rules
- optimizer access-path enumeration and costing support for vector search
- executor support for rerank flags and candidate semantics
- MGA GC and maintenance hooks for tombstone-heavy ANN families
- health-scan and `ANALYZE` support for quality metrics and sampled truth runs
- maintenance procedures for shadow rebuild, retrain, and generation publish

Immediate dependency conclusion:

- Lane E is blocked more by metadata, lifecycle, and optimizer integration than by raw distance-function availability.

## 12. Recommended next-step specification tasks

1. Write a Lane E contract addendum that distinguishes supported physical families from compatibility labels and states which labels must reject at create time.
2. Extend the index-metadata specification so vector families persist `metric`, `vector_dim`, build knobs, search defaults, payload mode, and artifact generation.
3. Write a dedicated vector access-path and costing specification covering `VECTOR_EXACT_SCAN`, `HNSW_KNN_SCAN`, and future `IVF_KNN_SCAN`.
4. Reconcile the HNSW spec with live runtime gaps:
   - canonical vector payload encoding
   - authoritative entry-point and max-layer state
   - canonical stopping rule
   - graph-quality maintenance triggers
5. Draft an IVF specification update that makes training artifacts, publish-by-generation, and rerank policy mandatory.
6. Draft a shared quantization packet for PQ and SQ modes so `IVF_*` and `RHNSW_*` variants use one metadata vocabulary.
7. Write a Lane E validation and benchmark specification that mandates truth-mode comparison, degraded-state tests, and quality metrics.
8. Add explicit Alpha or first-wave gating text for `SCANN`, `DISKANN`, `ANNOY`, `NSG`, and `GPU_CAGRA`.
9. Define metric-name canonicalization so external DDL, internal runtime enums, and query surfaces all store one authoritative value.
10. Write an optimizer-integration note that states when ANN paths are legal, when exact scan should win, and how filter placement changes costing.

Recommended delivery order:

- first, metadata and HNSW contract honesty
- second, exact-scan and HNSW planner integration
- third, IVF training and rerank specification
- fourth, deferred-family gating and future-family placeholders

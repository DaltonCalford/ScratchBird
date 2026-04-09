# Inverted Text and Ranking Planner Spec

## Purpose
Define the planner contract for:

- `GIN_TEXT`
- `INVERTED_TEXT`
- `FULLTEXT` and routed text aliases

## Hard Invariants
1. ScratchBird must distinguish boolean token access from ranked inverted
   retrieval.
2. `FULLTEXT` is an alias-lowering surface, not a separate storage identity.
3. Phrase, analyzer-sensitive, and rank-sensitive paths must declare explicit
   recheck or score semantics.
4. Ranked text paths are not planner-visible without corpus statistics.

## Canonical Paths
- `GIN_FILTER_SCAN`
- `TEXT_BITMAP_SCAN`
- `TEXT_SCORE_SCAN`
- `TEXT_RECHECK_SCAN`

## Family Model

### `GIN_TEXT`
- boolean and containment-oriented token access
- optional pending-list write optimization
- may return exact hits or candidates with recheck

### `INVERTED_TEXT`
- analyzer-driven ranked retrieval
- immutable segment publication and merge
- optional positions, offsets, and payloads

### Alias lowering
- boolean containment and token predicates -> `GIN_TEXT`
- ranked, wildcard, field-weighted, or top-`K` retrieval -> `INVERTED_TEXT`

## Query Packet
Planner-visible text queries must expose:

- token set
- boolean structure
- phrase or proximity requirements
- ranking requested or not
- field restriction or wildcard scope
- analyzer identity
- expected recheck requirement
- continuation or search-after position when applicable
- collector specialization request when a non-generic top-`K`, filter-only, or
  aggregation-heavy pipeline is requested

## Metrics Packet
- `term_df`
- `avg_postings_per_term`
- `pending_list_fraction`
- `phrase_hit_rate`
- `score_rows_est`
- `merge_debt`
- `stale_hit_ratio`
- `recheck_ratio_est`
- `collector_early_stop_gain`
- `mutable_overlay_fraction`

## Exactness Contract
Every Lane D path must declare:

- `proof_level = EXACT | CANDIDATE`
- `requires_recheck = true | false`

Defaults:

- boolean token predicates may be exact or candidate depending on analyzer and
  postings
- phrase, proximity, and analyzer-mismatch cases are candidate paths unless a
  stronger proof exists
- maintenance or publication states that weaken exactness must force candidate
  classification unless a stronger family contract explicitly proves otherwise

## Costing

### Boolean filter
`cost_text_filter = C_term * term_count + C_posting * posting_pages_touched + C_recheck * recheck_ratio_est + C_pending * pending_list_fraction`

### Ranked search
`cost_text_rank = C_term * term_count + C_posting * posting_pages_touched + C_score * score_rows_est + C_topk * K + C_merge * merge_debt`

### Phrase and proximity
`cost_phrase = cost_text_filter + C_positions * position_checks_est + C_phrase_recheck * phrase_hit_rate`

## Planner Selection Rules
1. Use `GIN_FILTER_SCAN` for boolean and containment predicates.
2. Use `TEXT_SCORE_SCAN` only when ranking or score-ordered output is required.
3. `TEXT_BITMAP_SCAN` is legal when the text access method returns a candidate
   bitmap or posting-derived row-id set.
4. Planner must not treat ranked retrieval as a free extension of boolean
   access; score cost and corpus stats are mandatory.
5. Query rewrite for text families must happen before final access-path
   comparison so analyzer choice, wildcard scope, and collector specialization
   are frozen as planner-visible state.
6. Collector specialization for filter-only, top-`K`, aggregation-heavy, and
   continuation workloads must remain explicit in runtime-plan evidence.

## Donor-Derived Requirements
This document incorporates the normalized inverted-text and ranking
requirements traced in
`../../planning/SPECIFICATIONS_WORK_PLANNING/INDEX_OPTIMIZER_REFERENCE_TRACE_MATRIX_2026-03-16.md`.

## BM25 Baseline
First-wave ranked paths should use:

- `k1 = 1.2`
- `b = 0.75`

unless a later authoritative scoring profile supersedes it.

## Cross-Section References
- `GIN_SPEC.md`
- `FULLTEXT_SPEC.md`
- `FULLTEXT_RANKING_MODES.md`
- `INVERTED_SPEC.md`
- `INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md`

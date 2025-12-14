# TODO: Optimizer Usage of Stats/Costs

Goal: Specify how collected stats feed the planner/optimizer for index/plan selection.

Requirements:
- Cost model: define base CPU/IO costs, selectivity estimation per predicate/type, and per-index-type cost functions.
- Join order/strategy selection using stats (cardinality estimates, histograms, correlation).
- Index selection rules per index type (btree/hash/gist/gin/rtree/bitmap/columnstore/fulltext).
- Replan/adaptive behavior (if any) and parameter handling.
- Controls: enable/disable certain indexes/strategies; planner debug output for diagnostics.

Work Items:
- Document cost formulas and selectivity estimation inputs (tie to `index_stats_research_todo.md`).
- Implement planner hooks to consume stats and prefer appropriate index types.
- Add planner debugging/tracing to inspect decisions.
- Tests: planner chooses expected indexes/plans given stats; regressions when stats are stale/missing.

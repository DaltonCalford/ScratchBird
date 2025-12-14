# TODO: Index Statistics & Usage (All Index Types)

Goal: Define best practices to collect and use statistics per index type, and feed them into the optimizer/planner.

Index Types to cover:
- B-tree
- Hash
- GiST
- GiN
- R-tree (spatial)
- Bitmap
- Columnstore
- Fulltext/LSM (if applicable)

Requirements:
- Collection: per-index statistics (cardinality, distinct counts, histograms, null fraction, correlation, page counts, bloat/fragmentation, depth/fanout, split/merge rates, pending list size (GiN), bucket load (hash), node utilization (rtree/gist), dictionary/encoding metrics (bitmap/columnstore)).
- Trigger points: manual ANALYZE, auto-analyze thresholds (tuple churn), background maintenance hooks.
- Storage: persist stats in catalog; versioned per index/table; track last analyze timestamp.
- Planner use: cost model inputs per index type; selectivity estimates; index choice rules; join order impact.
- Controls: enable/disable auto-analyze; per-index thresholds; sampling rates; lightweight vs full analyze.
- Visibility: catalog views for stats and last-run info; accessible to privileged roles.

Work Items:
- Define stat schema per index type and persist in catalog.
- Implement collectors (shared where possible) and per-index-type calculators.
- Integrate stats into planner cost/sel estimation per index type.
- Add auto-analyze triggers and manual ANALYZE support.
- Tests: stats collection per index type, planner decisions influenced by stats, thresholds and controls.

# B-tree Compressed Page Search Acceleration

## Purpose
Define the next-generation in-page search protocol for prefix-compressed B-tree
pages so hot lookup cost is bounded and not dominated by full linear decode.

## Scope
- restart-anchor layout
- writer responsibilities for compressed pages
- binary search over restart anchors
- bounded decode inside restart blocks

## Hard Invariants
1. Search acceleration must preserve exact key ordering under prefix and suffix
   truncation.
2. The slot array remains the authoritative logical order.
3. Search must not require full-page sequential reconstruction as the steady
   state hot path.

## Restart-Anchor Model
ScratchBird reuses the page's jump-table region as a restart-anchor array.

Each restart anchor stores:
- slot ordinal
- byte offset of the anchor node
- full reconstructed key prefix for that anchor block

Required header semantics:
- `jump_interval` = restart interval in slots
- `jump_count` = number of restart anchors
- `jump_size` = bytes per restart anchor entry

## Writer Rules
Page writers must:
1. regenerate restart anchors whenever slot order or compression changes
2. ensure the first anchor in a page reconstructs without predecessor state
3. choose a restart interval validated by page occupancy and average key width

## Search Algorithm
1. binary search the restart-anchor array to find the best candidate block
2. reconstruct the first full key for that block
3. linearly decode within the block until the target key is found or the block
   bound is crossed
4. if target exceeds page high key, use right-link chase per the concurrency
   protocol

Maximum decode work per probe is bounded by one restart block, not the full
page.

## Metrics
Required metrics:
- `avg_decoded_keys_per_probe`
- `restart_density`
- `compression_bytes_saved`
- `probe_fallback_linear_pages`

## Acceptance Criteria
- page-local search is no longer full linear decode
- search cost grows approximately logarithmically with page node count
- restart-anchor search remains correct under suffix truncation

## Cross-Section References
- `BTREE_PIVOT_TUPLE_AND_SEPARATOR_KEYS.md`
- `BTREE_CONCURRENCY_AND_SPLIT_TOLERANT_DESCENT.md`
- `INDEX_METRICS_AND_COSTING.md`

## Legacy Mapping
| Historical source | Material preserved here |
| --- | --- |
| `specifications_old/indexes/BTREE_SPEC.md` | binary-search intent refined into explicit compressed-page search mechanics |

## Gap Closure Mapping
- `SB-BTR-002`

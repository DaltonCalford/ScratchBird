# B-tree Pivot Tuple and Separator Keys

## Purpose
Define the minimal-separator, suffix-truncation, and child-range contract for
internal B-tree nodes.

## Scope
- minimal separator computation
- suffix truncation for pivot tuples
- internal child-range invariants
- separator validation and rebuild rules

## Hard Invariants
1. Every internal separator must be sufficient to distinguish the left child's
   high range from the right child's low range.
2. Internal tuples store only the shortest stable separator needed for routing.
3. Separator truncation must never change logical ordering.

## Minimal Separator Rule
For child ranges `L` and `R`, the chosen separator `S` must satisfy:
- every key in `L` is `< S`
- every key in `R` is `>= S`
- no shorter prefix than `S` satisfies both conditions

The engine must derive `S` from the left child maximum and right child minimum,
not by blindly promoting the full right minimum key.

## Suffix Truncation
Internal pivot tuples store:
- retained prefix bytes
- `separator_suffix_trunc`
- remaining suffix bytes

Writers must maximize truncation subject to the minimal separator rule.

## Fence and Child-Range Interaction
For internal page slot `i`:
- lower bound comes from the predecessor separator or `-infinity`
- upper bound comes from separator `i`
- the rightmost child is bounded by page high key or `+infinity`

Validation must prove each child falls inside its advertised range.

## Rebuild and Validation Rules
1. any split, merge, redistribution, or bulk build must recompute minimal
   separators
2. diagnostic validation must check child bounds against all separators
3. internal-node rebuild tools must report average separator length and bytes
   saved by truncation

## Acceptance Criteria
- internal separators are materially shorter on wide-key workloads
- child-range validation is deterministic
- fan-out and split-rate improvement is measurable

## Cross-Section References
- `BTREE_COMPRESSED_PAGE_SEARCH_ACCELERATION.md`
- `BTREE_CONCURRENCY_AND_SPLIT_TOLERANT_DESCENT.md`
- `INDEX_METRICS_AND_COSTING.md`

## Legacy Mapping
| Historical source | Material preserved here |
| --- | --- |
| `specifications_old/indexes/BTREE_SPEC.md` | generic separator promotion refined into minimal pivot discipline |

## Gap Closure Mapping
- `SB-BTR-003`

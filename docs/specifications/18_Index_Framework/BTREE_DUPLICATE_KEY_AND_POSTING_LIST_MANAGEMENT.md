# B-tree Duplicate Key and Posting-List Management

## Purpose
Define duplicate-heavy key handling, posting-list growth, and visibility-aware
cleanup for B-tree leaf pages.

## Scope
- duplicate coalescing
- inline and overflow posting thresholds
- posting-list compression
- duplicate cleanup under MGA churn

## Hard Invariants
1. Duplicate handling may reduce storage overhead but may not change visible key
   semantics.
2. Unique-index enforcement still uses snapshot-visible conflict rules.
3. Posting-list cleanup must remove only dead or invisible version references.

## Duplicate Entry Model
Leaf storage uses one logical key entry with one or more version references.

Posting modes:
- `INLINE`
- `DELTA_COMPRESSED`
- `CONTINUATION_GROUP`

The engine promotes a duplicate set through these modes as it grows.

## Thresholds
Required settings:
- `btree.posting_inline_max_tids`
- `btree.posting_compress_min_tids`
- `btree.posting_continuation_max_bytes`
- `btree.duplicate_dedup_trigger_pct`

Defaults are profile-controlled and must be benchmark-visible.

## Compression Rules
Posting lists store:
1. one full anchor TID
2. delta-encoded page/slot/version triples for subsequent entries

Compression must preserve deterministic ordering by encoded TID.

## Split Policy
When a duplicate group exceeds inline space:
1. attempt dedup and compression
2. if still oversized, emit continuation groups before splitting the whole page
3. split only after continuation options are exhausted or scan cost becomes
   excessive

## Cleanup Rules
Duplicate cleanup may:
- drop dead TIDs from a posting group
- collapse continuation groups back into inline groups
- remove the logical key only when all TIDs are dead

## Metrics
Required metrics:
- duplicate pressure ratio
- posting compression bytes saved
- continuation group count
- duplicate cleanup removals

## Acceptance Criteria
- duplicate-heavy workloads materially delay whole-page splits
- posting-list overhead declines after compression
- duplicate scan cost remains bounded

## Cross-Section References
- `BTREE_MGA_VERSION_CHURN_MANAGEMENT.md`
- `INDEX_VERSION_SEMANTICS_AND_DEAD_ENTRY_LIFECYCLE.md`
- `INDEX_METRICS_AND_COSTING.md`

## Legacy Mapping
| Historical source | Material preserved here |
| --- | --- |
| `specifications_old/indexes/BTREE_SPEC.md` | duplicate key handling extended into explicit posting-list policy |

## Gap Closure Mapping
- `SB-BTR-006`

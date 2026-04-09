# B-tree Page Deletion, Merge, and Reclamation

## Purpose
Define the conservative, scan-safe deletion, merge, and page reuse policy for
the hardened B-tree.

## Scope
- empty-page deletion
- non-empty merge gating
- parent separator deletion
- reclaim quarantine and reuse horizons

## Hard Invariants
1. Compaction is distinct from structural merge.
2. A page may not be reused until it is unreachable and its reclaim epoch is
   older than the stable scan and snapshot horizons.
3. Active scans may not miss or duplicate keys because of page deletion.

## Maintenance Modes
- `COMPACT_ONLY`
- `EMPTY_PAGE_DELETE`
- `REDISTRIBUTE_ONLY`
- `NON_EMPTY_MERGE`

`NON_EMPTY_MERGE` is disabled by default until all hardening gates pass.

## Empty Page Deletion
A page is eligible only if:
- it has no live entries
- it has no active scan pin
- its parent cleanup intent is durably recorded
- the sibling/right-link redirection is publishable without ambiguity

## Non-Empty Merge Gating
Non-empty merge requires:
- both siblings below merge threshold
- no scan blocker or long-lived maintenance pin
- reclaim quarantine support enabled
- full validation after merge publication

If any precondition fails, the engine must prefer compaction or redistribution.

## Reclaim Quarantine
Freed pages enter a quarantine queue with:
- `page_gpid`
- `delete_epoch`
- `stable_reclaim_epoch_required`
- `smo_uuid`

Allocator reuse is allowed only after the stable reclaim epoch passes and no
restart intent references the page.

## Parent Cleanup
Parent entry deletion must be:
1. durably linked to the child-deletion intent
2. restart-repairable
3. validated against sibling/fence correctness before page reuse

## Diagnostics
Required counters:
- pages pending reclaim
- reclaimed pages by level
- merges skipped by scan-safety risk
- redistribution count versus true merge count

## Acceptance Criteria
- no scan misses or duplicates due to merge or reclaim
- freed pages are unreachable before reuse
- parent cleanup is deterministic and restart-safe

## Cross-Section References
- `BTREE_STRUCTURAL_MODIFICATION_DURABILITY_PROTOCOL.md`
- `BTREE_CONCURRENCY_AND_SPLIT_TOLERANT_DESCENT.md`
- `BTREE_VERIFICATION_AND_HARDENING_FRAMEWORK.md`

## Legacy Mapping
| Historical source | Material preserved here |
| --- | --- |
| `specifications_old/indexes/BTREE_SPEC.md` | merge/rebalance baseline narrowed into conservative deletion and reclaim rules |
| `specifications_old/indexes/INDEX_GC_PROTOCOL.md` | unreachable-page lifecycle aligned with MGA cleanup horizons |

## Gap Closure Mapping
- `SB-BTR-005`

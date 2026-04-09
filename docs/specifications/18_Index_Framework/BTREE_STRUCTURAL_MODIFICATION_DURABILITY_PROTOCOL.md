# B-tree Structural Modification Durability Protocol

## Purpose
Define the crash-safe structural modification protocol for B-tree split, merge,
root change, and page reclamation without promoting general-purpose WAL to core
recovery truth.

## Scope
- split, merge, and root publication state machines
- durable structural intent records
- restart reconciliation and repair rules
- page dependency ordering for multi-page SMO

## Hard Invariants
1. Heap/version visibility remains MGA-based; B-tree structural durability does
   not replace section 08 or 10 recovery truth.
2. Every multi-page B-tree structural operation must be reconstructable from the
   metapage, page headers, and SMO intent records after crash.
3. No page becomes reusable until the metapage and page-level intent state prove
   it unreachable.
4. The structural protocol is not general WAL; it is a bounded B-tree-local
   intent and dependency system.

## Durability Model
ScratchBird uses a hybrid model:
1. careful-write dependency ordering for page images
2. bounded metapage-resident SMO intent records for incomplete operation repair

Every SMO intent record contains:
- `smo_uuid`
- `smo_type` (`split`, `merge`, `root_change`, `bulk_publish`)
- `phase`
- `index_uuid`
- `left_gpid`
- `right_gpid`
- `parent_gpid`
- `root_before`
- `root_after`
- `publication_seq`
- `reclaim_epoch`

## Split State Machine
Required phases:
1. `PREPARE_RIGHT`
2. `LINK_RIGHT`
3. `PUBLISH_PARENT`
4. `CLEAR_INTENT`

Ordering rules:
1. persist intent record before exposing the new right page
2. write and flush the right page before left-page sibling/high-key update
3. write and flush the left page before parent separator publication
4. clear the intent only after parent publication and metapage counters are
   durable

## Merge and Deletion State Machine
Required phases:
1. `MARK_DELETE_CANDIDATE`
2. `REDIRECT_RIGHTLINKS`
3. `DELETE_PARENT_SEPARATOR`
4. `QUARANTINE_FREE`
5. `CLEAR_INTENT`

Ordering rules:
1. the victim page is never freed before parent cleanup is durably recorded
2. the victim enters quarantine before allocator reuse eligibility
3. restart may resume or roll forward parent cleanup, but it may not restore a
   page to allocation if a parent can still reference it

## Root Change Publication
Required phases:
1. `ALLOCATE_NEW_ROOT`
2. `INSTALL_CHILD_LINKS`
3. `PUBLISH_META_ROOT`
4. `RETIRE_OLD_ROOT`
5. `CLEAR_INTENT`

Rules:
1. metapage root publication increments `root_publication_seq`
2. readers use only the metapage-published root
3. old root retirement is separate from new root publication

## Restart Reconciliation
On startup:
1. load metapage and all active SMO intent records
2. for each intent, inspect page headers, sibling links, and parent references
3. deterministically choose one legal repair path:
   - complete publication
   - complete unlink and quarantine
   - roll forward root publication
4. increment `restart_repaired_pages` and emit diagnostics

Required repair outcomes:
- no parent references a quarantined page
- no right link points at an uninitialized page
- metapage root and page graph converge to one tree

## Error Classes
- `BTREE_SMO_INTENT_CORRUPT`
- `BTREE_SMO_PHASE_INVALID`
- `BTREE_SMO_RESTART_REPAIR_FAILED`
- `BTREE_ROOT_PUBLICATION_CONFLICT`

## Acceptance Criteria
- crash at any SMO phase leaves the tree restart-repairable
- no freed page remains reachable after restart
- repair counts and incomplete-SMO counts are observable

## Cross-Section References
- `BTREE_PERSISTENT_METADATA_AND_ROOT_MANAGEMENT.md`
- `BTREE_PAGE_DELETION_MERGE_AND_RECLAMATION.md`
- `BTREE_BULK_BUILD_AND_REBUILD_PROTOCOL.md`
- `../31_Conformance_Performance_and_Reliability_Gates/BTREE_HARDENING_AND_CRASH_SAFE_INDEX_GATES.md`

## Legacy Mapping
| Historical source | Material preserved here |
| --- | --- |
| `specifications_old/indexes/BTREE_SPEC.md` | split/merge/root behavior elevated into explicit SMO durability states |
| `specifications_old/indexes/INDEX_GC_PROTOCOL.md` | restart-safe unreachable-page handling refined for B-tree pages |

## Gap Closure Mapping
- `SB-BTR-001`

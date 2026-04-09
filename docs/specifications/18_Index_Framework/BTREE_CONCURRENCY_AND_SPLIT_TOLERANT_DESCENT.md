# B-tree Concurrency and Split-Tolerant Descent

Status: current_authority

## 1. Purpose

Define B-tree search, insert, split publication, and scan behavior under
concurrent structural change while preserving MGA-first visibility rules.

## 2. Scope

- fence and high-key semantics
- right-link chase rules
- latch modes and descent protocol
- concurrent split visibility to readers and writers
- MGA boundaries for index scans

## 3. Hard invariants

1. readers must tolerate concurrent splits without restarting from the root
2. high key and right sibling define the authoritative rightward escape path
3. page merges or deletions may not invalidate an active correct scan
4. structural page coordination must not become the ordinary row-visibility
   mechanism

## 4. Fence and right-link rules

Each page advertises:

- low bound inherited from its parent or predecessor
- exclusive high key
- right sibling pointer

If a search key is `>= high_key`, the reader must chase `right_sibling` until a
page whose bound contains the key is found.

## 5. Latch modes

- `SEARCH_SHARED`
- `WRITE_INTENT`
- `WRITE_EXCLUSIVE`
- `MAINTENANCE_EXCLUSIVE`

Rules:

1. readers descend with `SEARCH_SHARED`
2. writers may descend optimistically and upgrade only near the target page
3. parent and child need not both remain latched once the child's fence bounds
   prove the search key is contained
4. `MAINTENANCE_EXCLUSIVE` is reserved for deletion or merge phases gated by the
   reclamation spec

## 6. Descent protocol

1. latch current page shared
2. determine candidate child using separator rules
3. latch candidate child shared
4. if child fence bounds are valid for the search key, release parent and
   continue
5. if search key exceeds child high key, release parent if still held and chase
   right sibling at the child level

## 7. Split visibility

Writers must publish splits so that readers see one of two legal states:

1. the pre-split page still contains the key range
2. the left page's high key redirects the reader to the right page

Readers must never observe a state where the key range is in neither page.

## 8. MGA scan boundary

B-tree scan correctness still depends on MGA row-version visibility after the
structure lookup.

Required rule:

1. structural descent finds candidate row references
2. row acceptance still requires MGA visibility checks
3. sibling chase during split handling does not alter MGA visibility semantics

## 9. Worked split example

1. reader descends toward key `K`
2. writer splits the target page before the reader reaches the final node
3. reader inspects the high key and discovers that `K` belongs to the right
   sibling
4. reader follows the right sibling without restarting from the root
5. reader reaches a candidate row reference
6. reader then still applies MGA row-version visibility to the referenced row

This means split tolerance is structural correctness, not a replacement for MGA
visibility.

## 10. Merge preconditions

Structural merge is not a normal write-path action. It requires:

- no active scan pin or deletion blocker on the victim path
- satisfied reclaim epoch
- durable structural-modification intent record

## 11. Telemetry

Required contention metrics:

- latch wait by level
- split retry count
- right-link chase count
- hot upper-level page ids

## 12. Acceptance criteria

- concurrent split and search workloads do not require root restart
- upper-level contention drops relative to strict full-path coupling
- scans remain correct during concurrent split publication
- structural page coordination does not redefine MGA row-visibility rules

## 13. Cross-section references

- `BTREE_STRUCTURAL_MODIFICATION_DURABILITY_PROTOCOL.md`
- `BTREE_PAGE_DELETION_MERGE_AND_RECLAMATION.md`
- `INDEX_CONCURRENCY_AND_VISIBILITY.md`

## 14. Legacy mapping

| Historical source | Material preserved here |
| --- | --- |
| `specifications_old/indexes/BTREE_SPEC.md` | latch coupling baseline refined into split-tolerant descent |

## 15. Gap closure mapping

- `SB-BTR-004`

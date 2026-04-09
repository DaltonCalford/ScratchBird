# Index MGA Publication and Reclaim

Status: current_authority

## Purpose

This document defines how index families publish candidate entries, tolerate historical versions, and clean dead entries under the MGA model.

## Publication order

For any statement that changes indexed data, the ordered contract is:

1. materialize the new heap version or delete state with transaction stamp
2. preserve lineage to the earlier version as required by heap semantics
3. publish index candidate entries for the new state according to the family rules
4. commit the transaction in transaction inventory using forced-write / ordered-write rules
5. allow readers to re-check candidate hits against MGA visibility
6. retain older index entries until the corresponding heap versions become reclaimable
7. only then allow family-local dead-entry cleanup

## Reader contract

A reader that finds a candidate through an index shall:

1. fetch the referenced row-version candidate or row head
2. evaluate visibility using current transaction context
3. follow backversion lineage if the candidate head is not visible to the reader
4. reject candidates that are dead, rolled back, or otherwise invisible to the reader

## Dead-entry lifecycle

An index entry moves through these states:

- `live_candidate`
- `historically_retained_candidate`
- `cleanup_eligible_candidate`
- `physically_removed`

Transition to `cleanup_eligible_candidate` requires proven heap reclaim eligibility. Transition to `physically_removed` requires successful family-local structural cleanup.

## Family-local cleanup

Each family shall implement cleanup only after heap truth authorizes it. Cleanup may take the form of:

- physical entry deletion
- posting-list rewrite
- page compaction
- tombstone absorption
- deferred rebuild of an affected page or segment

## Split and sibling tolerance

Ordered families such as B-tree classes shall tolerate concurrent structural change during descent and cleanup. Split-tolerant sibling chase is a structural rule only; final row acceptance still depends on MGA visibility.

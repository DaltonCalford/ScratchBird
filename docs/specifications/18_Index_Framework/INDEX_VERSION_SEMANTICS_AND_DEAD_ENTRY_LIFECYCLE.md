# Index Version Semantics and Dead Entry Lifecycle

Status: current_authority

## Purpose

This document defines how index entries coexist with MGA version chains and how dead entries move from historical retention to physical removal.

## Version-semantics rule

An index entry may reference:

- the visible head candidate of a version chain
- a specific historical version where the family stores version-addressed entries
- a posting structure containing multiple candidate version references

Regardless of encoding, the entry remains a candidate only. Visibility is resolved by MGA rules against transaction state and lineage.

## Historical coexistence

During ordinary updates, the system may temporarily retain both old and new candidate entries. This is legal and expected under MGA. The old entry does not become dead merely because a newer entry exists.

## Dead-entry eligibility

An entry becomes cleanup-eligible only when:

- the referenced heap version is reclaimable
- the family-local structure can remove or compact the entry safely
- required derivative evidence has already been emitted where configured

## Cleanup phases

Family-local cleanup shall occur in these phases:

1. identify cleanup-eligible entries from heap reclaim proof
2. revalidate family structure before mutation
3. delete or compact the entry
4. repair or rebalance family-local structure if required
5. refresh family metrics and cleanup debt counters

## Failure handling

If structure validation fails during cleanup:

- stop destructive cleanup on the affected page or segment
- classify the target as `repair_required`, `rebuild_required`, or `containment_required`
- retain historical entries until a verified repair or rebuild path completes

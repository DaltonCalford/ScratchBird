# Temporary Memory and Spill Boundary

Status: current_authority_beta1

## Purpose

Define the Beta 1 lifecycle boundary between executor scratch, temporary pages,
spill workfiles, and persistent page residency. This file makes larger-than-
memory execution a first-class controlled path rather than an ad hoc fallback.

## Temporary and spill matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| temp table backed execution | beta1_required | temp tables and spill workfiles are page-backed memory owners under one controller | not durable user state |
| memory pressure to spill transition | beta1_required | spillable operators transition through explicit pressure and page-state steps | not a hidden overflow path |
| spill artifact lifecycle | beta1_required | spill artifacts remain temporary runtime state with resume metadata and reclaim rules | not archival storage |
| transparent spill for all operators | fail_closed | only operators declared spillable in their owning spec may use this path | not universal spill coverage |

## Canonical rules

1. Spillable temporary memory shall allocate through `SbPageBackedArena`.
2. Spill artifacts are controlled by the same residency manager that governs
   persistent and resident-index pages.
3. If an operator or subsystem is not declared spillable, it remains fail
   closed.
4. Cache eviction is not spill.
5. Resident vector demotion is not spill.
6. JIT artifact persistence is not spill.

## Cross-domain separation

The following must remain distinct:

- executor scratch
- spillable temporary pages
- temp-table state
- spill workfiles
- result cache entries
- statement cache entries
- translation cache entries
- permission cache entries
- resident vector state

Pressure in one class does not authorize silent reclassification into another
class.

## Temporary page states

Spillable temporary pages shall use these states:

1. `RESIDENT_CLEAN`
2. `RESIDENT_DIRTY`
3. `SPILL_CANDIDATE`
4. `SPILL_WRITING`
5. `SPILLED`
6. `RESTORE_PENDING`
7. `RESTORING`
8. `RECLAIMABLE`

## Admission ordering

Where a runtime path supports spill:

1. reserve the operator grant
2. consume resident scratch budget
3. switch the operator to page-backed temporary allocation
4. mark eligible page groups `SPILL_CANDIDATE`
5. write spill workfiles and metadata
6. preserve MGA correctness and statement semantics
7. emit spill-state observability

Where a runtime path does not support spill:

1. do not silently create an undocumented workfile path
2. refuse, degrade, or quarantine according to the owning subsystem contract

## Required spill metadata

Each spill group shall persist resume metadata containing:

- statement UUID
- operator UUID
- spill group UUID
- schema-root UUID
- page count
- workfile locator
- logical row-group boundaries
- checksum or hash
- reclaim state

## Sample operator transition

```cpp
void maybeSpill(HashAggregateState& state) {
  if (!state.supports_spill) {
    throw MemoryLimitExceeded("HASH_AGG not spillable");
  }
  auto pages = state.temp_pages.selectSpillCandidates();
  for (auto& page : pages) {
    writeSpillPage(page);
    page.state = TempPageState::SPILLED;
  }
  state.metrics.spill_groups++;
}
```

## Restore rules

1. Restore occurs only on demand.
2. Restore bytes are charged before the page becomes visible to the operator.
3. Restore may re-spill older temporary pages if the operator remains over
   budget.
4. Spill workfiles are deleted only after the page group reaches
   `RECLAIMABLE`.

## Explicit non-guarantees

- no universal automatic spill coverage
- no mature spill-cost optimizer guarantee
- no promise that spill paths preserve performance parity with in-memory execution

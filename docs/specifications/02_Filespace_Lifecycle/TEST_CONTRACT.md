# 02 Filespace Lifecycle Test Contract

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-30

## Current status

This section's test contract must cover both the current substrate and the full
Beta 1 operator lifecycle required by this section. Missing runtime coverage is
implementation drift, not permission to narrow the contract.

Targeted current proof now exists for the lane-A placement surface:
- `tests/unit/test_tablespace_autoextend.cpp` proves requested-tablespace
  allocation and autoextend behavior through `allocatePageInTablespace`
- `tests/unit/test_storage_engine.cpp` proves storage-engine insert publication
  into a custom-tablespace heap page

## Required coverage

1. Primary database bootstrap layout
   - database creation initializes the fixed bootstrap-page map
   - bootstrap-page validation rejects incompatible layout
2. Secondary tablespace header compatibility
   - current tablespace header decode works
   - legacy `TablespaceHeaderV1` decode works
   - unsupported header versions fail closed
3. Tablespace allocation and autoextend
   - `allocatePageInTablespace` allocates inside the requested tablespace
   - inconsistent FSM state is rejected
   - autoextend expands capacity only through the canonical allocation path
4. Live relocation resolution
   - tuples without migration evidence resolve to the source tablespace
   - tuples with exact migration evidence resolve to the target tablespace
   - bloom negatives do not falsely route to the target tablespace
5. Stable row identity during movement
   - moved rows preserve CTID/back-version/row-UUID invariants required by
     current heap semantics
6. Movement-sensitive transaction ordering
   - all-filespace publication/fencing logic prevents unsafe visibility during
     migration-sensitive work
7. Attach/detach operator lifecycle
   - attach validates header/UUID/page-size/path and reaches live dispatch
   - detach refuses primary and enforces emptiness or explicit `FORCE`
     migration
8. Durable lifecycle history
   - create/attach/migrate/cutover/shrink/split/detach publish durable history
   - refusal/rollback reason remains visible after restart
9. Shrink/compaction
   - shrink cannot truncate past live placement
   - header/FSM/file-size publication order is fail closed
10. Partition split and cutover
   - split publishes explicit pending/cutover states
   - cutover fence switches metadata and resolver truth atomically
11. Exact gate closure for non-heap storage-mode movement semantics when those
    families are claimed in scope

## Non-blocking expansion candidates

- A section-local gate bundle that groups all section `02` placement and
  relocation evidence
- Operator-visible diagnostics and metrics for migration progress and refusal
  reasons
- Test closure for multi-file tablespace inventory if that feature is intended
  to be fully supported

## Suggestions

- Keep this test contract fail closed: no lifecycle claim is complete until the
  matching tests or gates exist.
- When missing lifecycle features are implemented, add their tests here before
  closing the corresponding work-plan ticket.
- Reuse section `05`, `10`, and `18` gate work when validating placement,
  reclaim, and relocation side effects.

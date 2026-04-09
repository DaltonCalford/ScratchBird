# Spec Outline - 10_GC_and_Sweep

## Purpose
Define the Beta 1 sweep, reclaim, retention, and maintenance-publication
model.

## Authoritative Files
- `GC_SWEEP_ALGORITHM.md`
- `GC_SWEEP_AND_VERSION_RECLAMATION_HARDENING.md`
- `RECLAIM_LEGALITY_AUTHORITY_AND_DECISION_VOCABULARY.md`
- `SWEEP_CURSOR_PERSISTENCE_AND_RESTART_RESUMPTION.md`
- `RECLAIM_ELIGIBILITY_AND_PUBLICATION_ORDERING.md`
- `ACTIVE_SWEEP_WORKER_MODES_AND_POLICY.md`
- `SWEEP_AUDIT_EXPORT_AND_SHADOW_CAPTURE.md`
- `SWEEP_ARCHIVE_AND_RETENTION_POLICY.md`
- `DEPENDENCIES.md`
- `DECISION_RECORD.md`
- `TEST_CONTRACT.md`

## Scope
- captured-horizon sweep runs
- restart-resumable progress
- heap and index reclaim ordering
- shared reclaim legality decision vocabulary
- retained-evidence and shadow-capture publication
- retention and archive-boundary semantics
- repair-aware compaction and cleanup debt

## Invariants
1. reclaim truth is derived from transaction truth and captured horizons
2. cursor persistence is advisory progress, not authority
3. index cleanup may not outrun heap proof
4. restart either resumes safely or rewinds deterministically

## Dependencies
- Upstream: `08`, `09`, `05`, `24`.
- Downstream: `18`, `20`, `31`.

## Completeness Criteria
1. sweep phases and cursor persistence are explicit
2. restart resume and rewind rules are explicit
3. heap-index publication ordering is explicit
4. shared reclaim legality decision vocabulary is explicit
5. retained-evidence and retention boundaries are explicit
6. observability and gate evidence are named

## Implementation closure boundary
- Reclaim legality authority is already canonically owned here by
  `RECLAIM_LEGALITY_AUTHORITY_AND_DECISION_VOCABULARY.md`. Future work is code
  alignment, not canonical placement.
- Retained-evidence, shadow-capture, and retention-policy files are part of
  the authoritative section `10` surface and must not be treated as optional
  sidecars.
- Full archive-transfer depth remains an explicit runtime-boundary statement,
  not an implied promise hidden behind retention metadata.

## Code-backed audit addendum (2026-03-27)

### Status
`current_authority_with_reconstructed_expansion`

### Main finding
The section outline is directionally right but materially incomplete relative to the now-audited section `10` surface. The authoritative set is broader than sweep phases and cursor persistence alone: it also includes policy lanes, retained evidence handoff, shadow capture, retention metadata, decision drift, and dependency fan-out.

### Current status
- `GC_SWEEP_ALGORITHM.md`, `GC_SWEEP_AND_VERSION_RECLAMATION_HARDENING.md`, `SWEEP_CURSOR_PERSISTENCE_AND_RESTART_RESUMPTION.md`, `ACTIVE_SWEEP_WORKER_MODES_AND_POLICY.md`, `SWEEP_AUDIT_EXPORT_AND_SHADOW_CAPTURE.md`, `SWEEP_ARCHIVE_AND_RETENTION_POLICY.md`, `TEST_CONTRACT.md`, `DEPENDENCIES.md`, and `DECISION_RECORD.md` now all have current-authority-with-reconstructed-expansion status.
- The strongest proven area is restart-safe sweep plus evidence-before-prune behavior.
- The weakest proven area is full archive-transfer depth; the code proves retained metadata more strongly than an archive subsystem.

### Remaining implementation drift
- The runtime still realizes multiple worker roles through a narrower concrete
  worker substrate in places.
- The runtime still distributes reclaim-decision substrate across `HeapPage`,
  `SweepManager`, and `StorageEngine` while the shared section-owned contract is
  being aligned.
- The code proves retained metadata more strongly than a full archive-transfer
  subsystem.

### Suggestions
- Keep the authoritative-file set broad enough to include retained-evidence and
  retention-policy documents, not just the core sweep or reclaim docs.
- Keep section `10` completeness criteria split into:
  - core sweep correctness
  - retained-evidence correctness
  - retention or archive scope honesty
  - gate and test closure

# 10_GC_and_Sweep

## Purpose
Canonical specification area for 10_GC_and_Sweep.

## Status
Authoritative - approved for implementation.

## Files
- Canonical documents for this section are listed in the file index below.
- Use this README as the section navigation root.
- Do not use legacy material as implementation authority.

## Links
- Back to root index: [../README.md](../README.md)

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [ACTIVE_SWEEP_WORKER_MODES_AND_POLICY.md](ACTIVE_SWEEP_WORKER_MODES_AND_POLICY.md)
- [BETA2_ARCHIVE_TIER_ILM_AND_LEGAL_HOLD_MODEL.md](BETA2_ARCHIVE_TIER_ILM_AND_LEGAL_HOLD_MODEL.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [GC_SWEEP_ALGORITHM.md](GC_SWEEP_ALGORITHM.md)
- [GC_SWEEP_AND_VERSION_RECLAMATION_HARDENING.md](GC_SWEEP_AND_VERSION_RECLAMATION_HARDENING.md)
- [RECLAIM_ELIGIBILITY_AND_PUBLICATION_ORDERING.md](RECLAIM_ELIGIBILITY_AND_PUBLICATION_ORDERING.md)
- [RECLAIM_LEGALITY_AUTHORITY_AND_DECISION_VOCABULARY.md](RECLAIM_LEGALITY_AUTHORITY_AND_DECISION_VOCABULARY.md)
- `SECTION_CLOSURE_MATRIX.csv`
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [SWEEP_ARCHIVE_AND_RETENTION_POLICY.md](SWEEP_ARCHIVE_AND_RETENTION_POLICY.md)
- [SWEEP_AUDIT_EXPORT_AND_SHADOW_CAPTURE.md](SWEEP_AUDIT_EXPORT_AND_SHADOW_CAPTURE.md)
- [SWEEP_CURSOR_PERSISTENCE_AND_RESTART_RESUMPTION.md](SWEEP_CURSOR_PERSISTENCE_AND_RESTART_RESUMPTION.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Maintenance
- Update file list with `../skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`.

## Current audit state (`2026-03-27`)
- Restart-safe sweep, cursor persistence, evidence-before-prune, page-audit findings, logical shadow capture, and derivative `wal_after` behavior are code-backed.
- Retention metadata is code-backed.
- Full archive-transfer depth is not yet code-backed to the same level.
- Reclaim-legality authority is canonically owned by
  `RECLAIM_LEGALITY_AUTHORITY_AND_DECISION_VOCABULARY.md`; current distributed
  heap, sweep, and storage-engine call sites are implementation drift against
  that contract.

## Primary audit lookup anchors
- `src/core/heap_page.cpp` search `HeapPage::scanVersionMaturity` for the
  shared reclaim-maturity classifier.
- `src/core/garbage_collector.cpp` search `GarbageCollector::cleanPage` for
  GC-driven reclaim execution.
- `src/core/sweep_manager.cpp` search
  `SweepManager::persistSweepProgressState` for cursor persistence and lane
  binding.

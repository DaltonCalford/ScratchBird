# ScratchBird Platform Memory Management Audit

Status: preliminary_local_audit_refined_with_web_research

This audit package combines the local donor-repo memory review with the April 2, 2026 web-research bundle and turns it into a ScratchBird-specific platform memory strategy.

## File Set

- `PRELIMINARY_FULL_MEMORY_MANAGEMENT_AUDIT.md`
- `LOCAL_DONOR_MEMORY_MODEL_MATRIX.csv`
- `MEMORY_MANAGEMENT_DECISION_MATRIX.csv`

## Inputs

- ScratchBird canonical authority under `docs/specifications/33_Memory_Management/`
- ScratchBird implementation anchors in `include/scratchbird/` and `src/`
- downloaded vendor-doc manifest `docs/reference/workspace_library/technical_specs/MEMORY_MANAGEMENT_WEB_SOURCES_20260402.md`
- downloaded paper manifest `docs/reference/workspace_library/whitepapers/MEMORY_MANAGEMENT_WHITEPAPERS_20260402.md`

## Reading Order

1. `PRELIMINARY_FULL_MEMORY_MANAGEMENT_AUDIT.md`
2. `MEMORY_MANAGEMENT_DECISION_MATRIX.csv`
3. `LOCAL_DONOR_MEMORY_MODEL_MATRIX.csv`

## Main Conclusion

The best model is not a copy of one donor engine and not a generic allocator swap.

The strongest ScratchBird path is:

- keep the existing Section 33 domain model, but turn it into a hard hierarchical budget tree with live breakers
- make lifetime-bound arenas and typed allocators the default execution substrate
- unify persistent-page residency and spillable temporary pages under one control plane
- separate page cache, object heaps, resident indexes, and JIT code memory while still charging them into one global tree
- add feedback-based grants, soft or hard pressure transitions, and a background memory-debt scheduler
- keep the general-purpose allocator pluggable and subordinate to ScratchBird-owned arenas and page managers

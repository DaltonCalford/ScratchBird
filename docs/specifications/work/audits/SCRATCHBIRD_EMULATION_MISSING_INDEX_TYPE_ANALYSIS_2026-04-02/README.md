# ScratchBird Emulation Missing Index Type Analysis 2026-04-02

This audit package classifies the current raw missing-index packet against
actual ScratchBird and donor-engine source so the emulation backlog only
contains real persisted index-family work.

## Files

- `MISSING_INDEX_TYPE_ANALYSIS.md`
  - narrative findings, packet defects, and recommended Beta 2 work order
- `MISSING_INDEX_TYPE_DECISION_MATRIX.csv`
  - row-level classification for every raw missing-index packet row

## Scope

- local-source-only analysis
- persisted index families and donor-visible index surfaces
- exclusion of planner hints, routing abstractions, access-path labels, and
  property flags when they are not real persisted families

## Primary Inputs

- `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/MISSING_DONOR_INDEX_SURFACES.csv`
- `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/MISSING_DONOR_INDEX_TOKEN_ROLLUP.csv`
- `docs/reference/reference_library/emulation_1_to_1_engine_reference_packets_2026-04-02/CROSS_ENGINE_INDEX_SUPPORT_MATRIX.csv`
- `include/scratchbird/parser/ast_v3.h`
- `src/parser/parser_v3.cpp`
- `src/core/index_factory.cpp`
- `src/optimizer/index_family_lowering.cpp`


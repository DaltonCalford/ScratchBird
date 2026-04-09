# Canonical Spec Stage Inventory 2026-04-03

## Purpose

Provide a definitive stage assignment for the canonical ScratchBird specification set using the explicit stage markers in canon plus the user rule that any spec with no explicit later-stage marker defaults to `Beta 1`.

## Files

- [CANONICAL_STAGE_INVENTORY_REPORT.md](CANONICAL_STAGE_INVENTORY_REPORT.md)
  - human-readable rollup, classification rule, bridge files, and section counts
- [CANONICAL_IMPLEMENTATION_STAGE_CLASSIFICATION.csv](CANONICAL_IMPLEMENTATION_STAGE_CLASSIFICATION.csv)
  - full implementation-driving canonical inventory with one stage classification per artifact
- [BETA1_IMPLEMENTATION_SCOPE.csv](BETA1_IMPLEMENTATION_SCOPE.csv)
  - definitive Beta 1 implementation-driving inventory, including shared Beta 1/Beta 2 bridge files
- [BETA2_IMPLEMENTATION_SCOPE.csv](BETA2_IMPLEMENTATION_SCOPE.csv)
  - definitive Beta 2 implementation-driving inventory, including shared Beta 1/Beta 2 bridge files
- [BETA3_IMPLEMENTATION_SCOPE.csv](BETA3_IMPLEMENTATION_SCOPE.csv)
  - definitive Beta 3 implementation-driving inventory
- [SECTION_STAGE_COUNTS.csv](SECTION_STAGE_COUNTS.csv)
  - counts by stage and canonical section

## Classification Rule

1. explicit `BETA3_*` marker -> `Beta 3`
2. explicit `BETA2_*` or `*_BETA2_*` marker -> `Beta 2`
3. explicit cross-stage cloud files with both `Beta 1` and `Beta 2` scope -> included in both stage inventories
4. no explicit later-stage marker -> `Beta 1`

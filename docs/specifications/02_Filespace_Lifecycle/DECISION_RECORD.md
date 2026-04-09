# 02 Filespace Lifecycle Decision Record

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status

The following decisions are supported by reviewed code and by the explicit Beta
1 lifecycle expansion adopted for this section.

## Decisions

1. The implementation term is `tablespace`, not `filespace`.
   - Section `02` may keep `filespace` as a documentation label, but canonical
     implementation guidance should use `tablespace` when describing live code
     surfaces.
2. Tablespace `0` is the primary database file.
   - The primary `.sbdb` file is governed by `DatabaseHeader` plus the fixed
     bootstrap-page map.
3. Custom tablespaces use their own versioned tablespace header.
   - Secondary `.sbts` files are governed by `TablespaceHeaderV1` or
     `TablespaceHeader` decoding, not by the primary-file bootstrap header.
4. Global placement identity is GPID-based.
   - The current model uses a `16`-bit tablespace identifier plus `48`-bit page
     number, preserving stable location identity across filespaces.
5. Live relocation truth is currently tuple-aware redirection.
   - Reviewed migration logic redirects reads through source/target tablespace
     resolution using exact and bloom-backed migration evidence.
6. Row identity must survive relocation.
   - Stable slot identity, CTID, back-version linkage, and row UUID fields
     remain the authority for row movement semantics.
7. Beta 1 requires one explicit operator lifecycle for create, attach, detach,
   migrate, shrink, split, cutover, and durable lifecycle history.
   - Current code gaps in those surfaces are implementation drift, not a reason
     to weaken the section contract.

## Drift and contradictions

- Older prose implied a generic filespace lifecycle subsystem with attach,
  detach, shadow-copy shrink, and partition split already defined. Those
  surfaces are now explicit Beta 1 required behavior, but current code closure
  remains incomplete.
- Older prose implied the primary file carried a filespace header block. The
  code instead uses a database-header bootstrap authority for the primary file.
- Older prose implied a generic row-mapping journal. The reviewed code proves
  movement-aware tuple and migration logic, but not that separate generic
  subsystem.

## Suggestions

- Preserve these decisions as fail-closed defaults while implementing the
  broader Beta 1 lifecycle.
- Drive shrink, split, attach/detach, and cutover through one tablespace
  lifecycle state machine instead of adding more distributed special cases.
- Treat current missing code closure as implementation drift, not as authority
  to re-narrow the canon.

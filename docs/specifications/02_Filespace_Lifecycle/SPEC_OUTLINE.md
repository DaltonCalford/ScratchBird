# 02 Filespace Lifecycle Specification Outline

## Status
- Section status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status

This section covers the current filespace/tablespace substrate plus the
explicit Beta 1 lifecycle behavior that must be closed in this lane, in this
order:

1. durable file and header layout
2. bootstrap and page-allocation behavior
3. explicit operator lifecycle and durable history
4. live relocation, split, and cutover boundaries
5. storage-mode and oversized-value consequences
6. section-local dependency and test closure

## Implementation depth by topic

- Primary database file bootstrap layout: implemented and code-backed
- Secondary tablespace file header layout: implemented and code-backed
- Tablespace allocation and autoextend: implemented and code-backed
- Online tuple relocation during migration: implemented and code-backed
- Stable row identity across relocation: implemented and code-backed
- Attach/detach operations: Beta 1 required behavior with partial substrate and
  incomplete live dispatch closure
- Shadow-copy shrink or compaction: Beta 1 required behavior with incomplete
  runtime closure
- Partition split orchestration: Beta 1 required behavior with resolver/cutover
  substrate but incomplete orchestrator closure
- Full operator-visible filespace DDL surface: Beta 1 required behavior with
  partial parser/catalog/executor substrate

## Canonical subsection roles

- `FILESPACE_FILE_LAYOUT.md`: durable file and header authority
- `FILESPACE_OPERATIONS.md`: current substrate plus Beta 1 lifecycle operations
- `TABLESPACE_DDL_AND_OPERATOR_LIFECYCLE_MODEL.md`: explicit operator
  state-machine contract
- `PARTITION_BOUNDARY_SPLIT_AND_OBJECT_RELOCATION.md`: relocation, split, and
  cutover authority
- `TABLE_STORAGE_MODES_AND_ROW_MOVEMENT_MODEL.md`: stable identity,
  movement legality, and storage-mode maturity
- `TABLE_STORAGE_AND_ACCESS_METHOD_ARCHITECTURE.md`: access-method and
  family-level storage authority
- `OVERSIZED_VALUE_RETENTION_AND_OVERFLOW_LIFECYCLE.md`: oversized-value
  retention and overflow authority
- `DEPENDENCIES.md`: subsystem coupling and required upstream/downstream links
- `TEST_CONTRACT.md`: required gate and audit closure for this section

## Suggestions

- Keep each section file narrow, but let it state required Beta 1 behavior when
  current code is only partial.
- When a feature is only partially proven, say so directly and record the
  implementation drift instead of weakening the product contract by implication.
- Use section `02` as the source authority for placement and relocation
  semantics that later specs can reference instead of re-describing.

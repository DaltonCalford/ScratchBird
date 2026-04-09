# Index Relocation Cleanup Hook Registry

Status: current_authority
Section owner: `18_Index_Framework`

## Current authority

ScratchBird does not currently prove one engine-owned relocation cleanup hook
registry. The current implementation authority is family-owned cleanup and
rewrite logic in the concrete index families plus the maintenance paths that
call them.

## Current code-backed ownership matrix

| Family or surface | Current authority | Notes |
| --- | --- | --- |
| `BTree` migration rewrite | `src/core/btree.cpp` | `BTree::updateTIDsAfterMigration` is the current family-owned relocation rewrite hook |
| `BTree` reclaim constraint | `src/core/btree.cpp` | reclaim remains narrower than full structural rewrite closure |
| `HashIndex` migration rewrite | `src/core/hash_index.cpp` | `HashIndex::updateTIDsAfterMigration` is the current family-owned relocation rewrite hook |
| `HashIndex` overflow cleanup | `src/core/hash_index.cpp` | empty overflow unlink and free remain family-owned cleanup logic |
| `ColumnstoreIndex` migration rewrite | `src/core/columnstore.cpp` | current participation remains partial and family-owned |
| unified hook registry | fail closed | no current central registry authority is proven |

## Canonical rule

Section `18` must describe relocation cleanup in terms of the current
family-owned hook surfaces. It must not claim a central hook registry until one
is explicitly implemented and audited.

## Runtime decision rule

1. Relocation or cleanup orchestration determines the target index family.
2. The orchestration path dispatches only to the family-owned hook proved for
   that family.
3. Unknown family, missing family hook, or unproven family breadth rejects fail
   closed.
4. `ColumnstoreIndex` remains bounded to the currently audited partial rewrite
   path and must not be described as full cleanup closure.

## Current fail-closed boundaries

- no engine-owned central relocation cleanup registry is current authority
- no generic plugin-style hook discovery surface is current authority
- no claim that every index family shares one normalized cleanup callback shape
- no claim that columnstore currently matches B-tree or hash relocation breadth

## Cross-section references

- `INDEX_ROLLBACK_REWRITE_FAMILY_CLOSURE.md`
- `INDEX_MGA_PUBLICATION_AND_RECLAIM.md`
- `../02_Filespace_Lifecycle/PARTITION_BOUNDARY_SPLIT_AND_OBJECT_RELOCATION.md`
- `../10_GC_and_Sweep/RECLAIM_LEGALITY_AUTHORITY_AND_DECISION_VOCABULARY.md`

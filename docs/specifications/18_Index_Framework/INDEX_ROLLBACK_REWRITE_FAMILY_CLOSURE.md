# Index Rollback Rewrite Family Closure

Status: current_authority
Section owner: `18_Index_Framework`

## Current authority

ScratchBird currently proves rollback-sensitive rewrite behavior only through
family-owned code paths and explicit guarded constraints in those families.
There is no current family-neutral capability registry that turns rewrite
support into one shared runtime table.

## Current family support matrix

| Family | Operation | Current state | Current authority |
| --- | --- | --- | --- |
| `btree` | migration `TID` rewrite | `supported_with_constraints` | `src/core/btree.cpp` |
| `btree` | separator-key or structural merge rewrite | `unsupported` or fail-closed constrained | `src/core/btree.cpp` guarded disabled paths |
| `btree` | reclaim cleanup | `supported_with_constraints` | leaf-oriented reclaim behavior remains narrower than full structural closure |
| `hash` | migration `TID` rewrite | `supported` for current audited path | `src/core/hash_index.cpp` |
| `hash` | overflow cleanup | `supported` for current audited path | `src/core/hash_index.cpp` |
| `columnstore` | migration rewrite | `partial` | `src/core/columnstore.cpp` |
| unknown family or unknown operation | any | fail closed | no generic capability table is current authority |

## Canonical rule

Section `18` must describe rollback-sensitive rewrite support as the sum of the
current family-owned behaviors above. It must not claim one shared capability
registry or uniform rewrite closure unless that surface is explicitly
implemented and audited.

## Runtime decision rule

1. Determine the concrete index family.
2. Determine whether the requested rewrite is one of the audited operations for
   that family.
3. Execute only the proven family-owned path.
4. Reject unknown family, unknown operation, or unproven breadth fail closed.
5. Treat `partial` as distinct from `supported`; `partial` is not promotion to
   general rewrite closure.

## MGA and reclaim rule

Index rewrite and cleanup remain subordinate to heap visibility, MGA maturity,
and reclaim legality. Index-local rewrite capability does not override heap
publication or reclaim safety rules.

## Current fail-closed boundaries

- no family-neutral rewrite capability registry is current authority
- no claim of full B-tree structural rewrite closure
- no claim that columnstore currently provides complete rollback rewrite closure
- no claim that warnings or TODO-style guards are equivalent to supported

## Cross-section references

- `INDEX_MGA_PUBLICATION_AND_RECLAIM.md`
- `INDEX_RELOCATION_CLEANUP_HOOK_REGISTRY.md`
- `BTREE_PAGE_DELETION_MERGE_AND_RECLAMATION.md`
- `HASH_SPEC.md`
- `COLUMNSTORE_SPEC.md`
- `../10_GC_and_Sweep/RECLAIM_LEGALITY_AUTHORITY_AND_DECISION_VOCABULARY.md`

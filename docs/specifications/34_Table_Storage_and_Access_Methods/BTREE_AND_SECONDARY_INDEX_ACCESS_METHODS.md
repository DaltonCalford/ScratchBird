# BTree and Secondary Index Access Methods

This file owns the bounded relationship between primary storage and secondary access paths.

## Secondary access matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| B-tree access | current_bounded | B-tree path semantics are current where section 18 proves them | not a claim of universal plan dominance |
| visibility recheck and heap handoff | current_bounded | secondary access remains bounded by heap visibility and MGA truth | not index-only visibility folklore unless explicitly proven |
| non-B-tree secondary families | current_bounded | admitted named families now have deterministic create/open or scan-routing truth through section 18 registry authority and shared-runtime lowering where implemented | not equal planner, maintenance, or maturity claims across all families |
| unified secondary-access contract | fail_closed | no universal cross-family performance or maintenance guarantee is implied | not a stable abstract access-method ABI |

## Canonical rules

1. Section 34 may summarize secondary path boundaries, but section 18 remains the deep authority for individual families.
2. Secondary lookup does not bypass MGA visibility truth.
3. Any family not explicitly current remains fail-closed at this section level.

## Explicit non-guarantees

- no universal index-only execution claim
- no equal maintenance cost or correctness maturity across families
- no stable extension ABI for new access methods

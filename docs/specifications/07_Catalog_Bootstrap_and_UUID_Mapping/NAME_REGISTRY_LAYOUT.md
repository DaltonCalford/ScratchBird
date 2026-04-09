# Name Registry Layout

## Status
`current_authority_with_reconstructed_expansion`

## Last code-audit date
`2026-03-27`

## Purpose
Define the name-authority layout that current code actually implements and remove older claims that imply a separate special physical name-registry page family.

## Current implementation status
The current implementation proves a catalog-row-based name authority:
- the catalog root contains an `object_name_page` pointer
- bootstrap allocates an `object_name` table page as the core name registry root
- bootstrap and open fail closed when that pointer is missing
- current name authority is centered on canonical `object_name` rows, not on a standalone special physical page type dedicated only to name registry storage

## Implementation code map
- `ScratchBird/src/core/catalog_manager.cpp:5377`: catalog-root payload includes `object_name_page`
- `ScratchBird/src/core/catalog_manager.cpp:10966`: bootstrap allocates the `object_name` table page as the core name registry
- `ScratchBird/src/core/catalog_manager.cpp:21345`: catalog-root persistence writes `object_name_page`
- `ScratchBird/src/core/catalog_manager.cpp:21684`: catalog-root read path reloads `object_name_page`
- `ScratchBird/src/core/catalog_manager.cpp:21841`: bootstrap or open fails closed when `object_name` root is absent
- `ScratchBird/src/core/catalog_manager.cpp:22033`: later read path repeats the same fail-closed requirement
- `ScratchBird/include/scratchbird/core/catalog_manager.h:13485`: runtime keeps `object_name_table_page_` as catalog-owned canonical state
- `ScratchBird/src/core/catalog_manager.cpp:5454`: comments identify `object_name` rows as the authoritative default-language name registry rows
- `ScratchBird/src/core/catalog_manager.cpp:22165`: current code comments still acknowledge a transitional boundary where canonical database rows remain authoritative until object-name or schema bootstrap has settled

## Known contradictions and drift
- the old prose read too much like a standalone physical name-registry subsystem; current code proves a catalog-table root instead
- this pass proves canonical default-language name authority, but it does not fully prove the broader multi-language, alias, or namespace-resolution policy surface
- the specification must not imply a dedicated `PAGE_TYPE_NAME_REGISTRY` family unless code later proves one

## Non-blocking expansion candidates
- close the authoritative semantics for aliasing, multi-language naming, and reserved-namespace policy in the downstream catalog sections
- add dedicated corruption and missing-root tests for `object_name_page` bootstrap failure paths
- publish a machine-readable name-authority matrix that distinguishes catalog default-name truth from any future alias or localization layers

# Test Contract - 07_Catalog_Bootstrap_and_UUID_Mapping

## Status
`current_authority_with_reconstructed_expansion`

## Last code-audit date
`2026-03-30`

## Directly evidenced tests in this audit pass
- `ScratchBird/tests/unit/test_vnext_bootstrap_contract.cpp:24`: fixed catalog-root bootstrap page constant remains page `2`
- `ScratchBird/tests/unit/test_catalog_database_bootstrap.cpp:373`: database
  identity row persists without duplicate bootstrap records
- `ScratchBird/tests/unit/test_catalog_database_bootstrap.cpp:477`: canonical
  fixed schema tree and expanded catalog-root inventory materialize on create

## Implementation behaviors that require explicit gate coverage
The code reviewed in this audit requires dedicated tests or gates for:
- catalog-root materialization from the page-`2` bootstrap shell into a fully populated catalog root
- bootstrap-state transitions across `UNINITIALIZED`, `INITIALIZED`, and `LOCKED`
- fail-closed missing-`object_name_page` bootstrap and open paths
- deterministic bootstrap schema-tree seeding of `43` nodes
- database UUID persistence across create and reopen
- stable heap `row_uuid` preservation across version-chain creation and rewrite paths

## Current test-gap assessment
This pass did not prove a complete dedicated section `07` gate suite.

The strongest remaining test gaps are:
- no section-level corruption matrix for catalog-root pointer inventory fields
- no dedicated test file was confirmed in this pass for `object_name_page` fail-closed behavior
- no dedicated gate was confirmed for bootstrap-state invalid-transition handling
- no dedicated section-level UUID identity gate inventory was confirmed, even though implementation evidence is strong

## Required next work-plan items
- add a section `07` gate matrix covering catalog-root materialization, missing `object_name_page`, and bootstrap-state transitions
- add explicit database UUID create/open persistence tests to the section `07` contract inventory
- add stable row-UUID preservation tests to the section `07` contract inventory

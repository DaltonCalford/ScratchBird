# 07_Catalog_Bootstrap_and_UUID_Mapping

## Purpose
Define the authoritative bootstrap materialization rules for the fixed catalog root page and the UUID identity rules used for durable internal identity.

## Status
Authoritative - current_authority_with_reconstructed_expansion as of `2026-03-30`.

## Current implementation summary
The current code proves the following section `07` contracts:
- page `2` is the fixed catalog root location, but fixed placement belongs to section `06` while section `07` owns the catalog-root payload and bootstrap materialization rules
- `CatalogManager` owns real bootstrap-state transitions and real catalog-root materialization rather than a tiny static table list written once at create time
- the catalog root persists a broad engine-owned pointer inventory, including the canonical `object_name` table root
- `object_name` is the current canonical default-language name authority; this section is not backed by a standalone special physical "name registry page" subsystem
- durable internal identity is UUIDv7-based through `core::ID`, database header UUIDs, heap row UUIDs, and widespread catalog object identifiers
- package `02` lane A proved that adding catalog-managed configuration and
  listener-topology families does not displace database UUID identity or fixed
  bootstrap-root authority

## Primary code anchors
- `include/scratchbird/core/ondisk.h`
- `include/scratchbird/core/uuidv7.h`
- `include/scratchbird/core/catalog_manager.h`
- `src/core/uuidv7.cpp`
- `src/core/database.cpp`
- `src/core/heap_page.cpp`
- `src/core/catalog_manager.cpp`

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [CATALOG_BOOTSTRAP_LAYOUT.md](CATALOG_BOOTSTRAP_LAYOUT.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [NAME_REGISTRY_LAYOUT.md](NAME_REGISTRY_LAYOUT.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [UUID_IDENTITY_AND_COLLISION_RULES.md](UUID_IDENTITY_AND_COLLISION_RULES.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

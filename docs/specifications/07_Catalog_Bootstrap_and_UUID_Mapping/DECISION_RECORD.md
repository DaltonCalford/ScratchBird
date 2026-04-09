# Decision Record - 07_Catalog_Bootstrap_and_UUID_Mapping

## Status
Authoritative decision record with code-backed `partial` validation as of `2026-03-27`.

## Decisions confirmed by code
- fixed page `2` remains the catalog-root location, but section `06` owns placement while section `07` owns payload materialization semantics
- catalog bootstrap is owned by `CatalogManager`, not by a tiny static list embedded only in `Database::create`
- bootstrap state is a real runtime concept with `UNINITIALIZED`, `INITIALIZED`, and `LOCKED`
- canonical default-language name authority currently lives in the `object_name` catalog table root referenced by the catalog root
- durable internal identity uses UUIDv7 through `core::ID`, persisted database UUIDs, stable heap row UUIDs, and broad catalog object identifiers
- deterministic bootstrap schema seeding is real and materially larger than older prose implied

## Drift corrected in this decision pass
- removed the implication that name registry is already a separate special physical subsystem
- narrowed UUID collision language to what code currently proves
- separated fixed bootstrap-page placement from catalog-owned payload materialization

## Open decisions still requiring future resolution
- whether the project actually wants centralized distributed UUID collision governance or treats UUIDv7 uniqueness plus local constraints as sufficient
- whether aliasing, localization, or namespace overlays should remain catalog-row concerns or grow into a separately modeled authority layer
- whether the catalog root should expose a machine-readable pointer manifest as part of the authoritative on-disk contract

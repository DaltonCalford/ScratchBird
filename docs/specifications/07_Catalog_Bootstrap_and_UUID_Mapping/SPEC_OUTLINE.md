# Spec Outline - 07_Catalog_Bootstrap_and_UUID_Mapping

## Purpose
Define the durable bootstrap contract for catalog-root materialization and the UUID identity rules that the engine actually uses today.

## Scope
This section covers:
- the catalog-owned payload and initialization rules for fixed page `2`
- bootstrap-state transitions used while catalog bootstrap is incomplete or locked
- the canonical `object_name` root and the name-authority boundary it currently proves
- UUIDv7-based durable identity for the database, heap rows, and catalog objects

## Out of scope
This section does not define:
- the fixed bootstrap page map itself; that belongs to section `06`
- the full catalog model, virtual overlays, or broad schema semantics; those belong to section `24`
- a general distributed collision-resolution service for UUIDs; this audit did not prove one
- a standalone special physical page family for name registry storage; current code does not prove one

## Canonical section responsibilities
- page `2` remains the fixed `PAGE_TYPE_CATALOG_ROOT` location, but `CatalogManager` owns the payload schema, initialization, and fail-closed validation
- bootstrap state is a real catalog-managed state machine with `UNINITIALIZED`, `INITIALIZED`, and `LOCKED`
- the catalog root must persist the `object_name` root page and fail closed when that pointer is absent
- the engine uses UUIDv7 for durable internal identity through `core::ID`
- heap row versioning preserves stable `row_uuid` identity across back-version creation and rewrite paths

## Main implementation drift corrected in this audit
- the previous prose understated how much real state now lives behind catalog-root materialization
- the previous prose blurred `Database::create` writing a bootstrap shell with `CatalogManager` later materializing the full catalog root and bootstrap tables
- the previous prose treated name registry as more physically separate than current code proves
- the previous prose was too aggressive about centralized distributed UUID collision governance

## Failure semantics
- catalog bootstrap must fail closed when the catalog root is missing required pages, especially `object_name`
- bootstrap-state transitions must reject invalid state changes
- database-open logic depends on a valid database UUID and the fixed page-`2` bootstrap contract

## Test direction
See `TEST_CONTRACT.md`.

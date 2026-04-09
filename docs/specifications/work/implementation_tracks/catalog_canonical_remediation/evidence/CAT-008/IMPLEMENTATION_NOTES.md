# Implementation Notes

Status: `Completed`

## Scope
- Enforce CAT-008 parent-local uniqueness semantics for child objects.
- Enforce canonical object-name uniqueness key at default language scope.
- Enforce catalog parentage contracts during canonical object/object_name sync.
- Add deterministic test coverage for low-capability implementation handoff.

## Code Changes
- `include/scratchbird/core/catalog_manager.h`
  - Changed table-trigger name map key from global string key to `(table_id, normalized_name)` key.

- `src/core/catalog_manager.cpp`
  - Added trigger name key helper + unscoped trigger lookup helper with ambiguity rejection.
  - Added table-parent collision helper using `IdentifierUtils::namesConflict(...)` semantics.
  - Updated `createTrigger` to enforce same-table uniqueness and return `DUPLICATE_OBJECT` with `NAME_COLLISION` context.
  - Updated `dropTrigger`, `getTriggerByName`, and `enableTrigger` to reject ambiguous unscoped trigger names.
  - Removed table-trigger/database-trigger cross-scope collision rejection (scope now parent-based).
  - Updated trigger rename flow to enforce table-scoped collisions and keep map state consistent.
  - Added read-time trigger-catalog corruption check for duplicate names within same table scope.
  - Added default-language `object_name` uniqueness enforcement on `(parent_object_id, object_type, canonical_name_text, language='default')`.
  - Added parentage guardrails in canonical sync:
    - `INVALID_PARENT_SCOPE` context on missing required parent UUIDs.
    - `PARENT_TYPE_MISMATCH` context on invalid parent object type.

- `tests/unit/test_catalog_parentage_and_name_uniqueness.cpp` (new)
  - Added CAT-008 tests for:
    - duplicate trigger same table => fail
    - same trigger name on different tables => success
    - unscoped trigger lookup ambiguity => fail
    - duplicate index same table => fail; same name different table => success

## Notes
- Failure-class strings are emitted in error context (`NAME_COLLISION`, `INVALID_PARENT_SCOPE`, `PARENT_TYPE_MISMATCH`) while using existing engine status codes (`DUPLICATE_OBJECT`, `INVALID_ARGUMENT`, `WRONG_OBJECT_TYPE`).

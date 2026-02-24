# UDR-CAT-005 Audit: Redaction + Write-Only Guarantees

Date: 2026-02-23
Owner: agent-security
Task: `UDR-CAT-005`
Gate: `UDR-CAT-GATE-05`

## Scope Implemented

1. Enforced write-only credential view for user-mapping reads.
2. Added runtime-only accessor for resolved credentials used by executor dispatch.
3. Added remote diagnostic message redaction before persisting remote error catalog rows.

## Code Touchpoints

- `include/scratchbird/core/catalog_manager.h`
  - Added `getUserMappingForRuntime(...)`.
  - Documented write-only behavior of `getUserMapping(...)`.
- `src/core/catalog_manager.cpp`
  - Added write-only marker helper and application on `getUserMapping(...)`.
  - Added `getUserMappingForRuntime(...)` implementation.
  - Added regex-based secret/endpoint redaction for remote diagnostic text.
  - Applied redaction in `upsertRemoteErrorCatalogEntry(...)`.
- `src/sblr/executor.cpp`
  - Runtime mapping resolution paths use `getUserMappingForRuntime(...)` where credentials are required.
- `tests/unit/test_catalog_remote_connector_extension_contract.cpp`
  - Added assertions for write-only mapping view and runtime credential view.
  - Added assertions that persisted remote error text is sanitized.
- `tests/unit/test_catalog_persistence_phase_b.cpp`
  - Added runtime accessor coverage across restart without asserting secret persistence text.

## Evidence (Commands + Result)

1. `cmake --build build --target scratchbird_tests -j8`
   - Result: PASS
2. `build/tests/scratchbird_tests --gtest_filter='CatalogRemoteConnectorExtensionContractTest.*'`
   - Result: PASS (1/1)
3. `build/tests/scratchbird_tests --gtest_filter='CatalogPersistencePhaseBTest.*'`
   - Result: PASS (2/2)
4. `build/tests/scratchbird_tests --gtest_filter='SBLRVNextExecutorDispatchContractTest.*'`
   - Result: PASS (22/22)

## Notes

- Full-tree build (`cmake --build build -j8`) is currently blocked by unrelated existing FDW compile errors:
  - `src/fdw/firebird_adapter.cpp`: missing `sb_socket_*` symbols.
  - `src/fdw/mysql_adapter.cpp`: missing `sb_socket_*` symbols.
- This blocker is external to `UDR-CAT-005` changes and does not affect the targeted gate evidence above.


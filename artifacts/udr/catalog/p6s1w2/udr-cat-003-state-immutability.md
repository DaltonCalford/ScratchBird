# UDR-CAT-003 State + Immutability Enforcement
Last-Modified: 2026-02-23

## Scope
Close `UDR-CAT-003`:
1. Connector state-machine transition enforcement.
2. Snapshot immutability and terminal delete-block.
3. Execution audit append-only enforcement.
4. Remote transaction binding terminal invariants (including local terminal-state coupling).

## Implementation Evidence
1. Connector state transition validator:
   - `src/core/catalog_manager.cpp:748`
   - `src/core/catalog_manager.cpp:78775`
2. Snapshot terminal immutability and delete protection:
   - `src/core/catalog_manager.cpp:79297`
   - `src/core/catalog_manager.cpp:79456`
3. Execution audit append-only enforcement (update + delete blocked):
   - `src/core/catalog_manager.cpp:80941`
   - `src/core/catalog_manager.cpp:81095`
4. Remote transaction binding terminal invariants:
   - terminal-state helper: `src/core/catalog_manager.cpp:832`
   - terminal-time/state consistency checks: `src/core/catalog_manager.cpp:80635`
   - local terminal transaction requires terminal remote binding: `src/core/catalog_manager.cpp:80683`
   - terminal binding row immutability: `src/core/catalog_manager.cpp:80722`
   - local runtime tx cannot close with open remote bindings: `src/core/catalog_manager.cpp:61106`

## Test Evidence
1. Contract tests added/updated:
   - `tests/unit/test_catalog_remote_connector_extension_contract.cpp:125`
   - `tests/unit/test_catalog_remote_connector_extension_contract.cpp:238`
   - `tests/unit/test_catalog_remote_connector_extension_contract.cpp:395`
   - `tests/unit/test_catalog_remote_connector_extension_contract.cpp:490`
   - `tests/unit/test_catalog_remote_connector_extension_contract.cpp:529`
2. Focused execution:
   - command:
     - `build/tests/scratchbird_tests --gtest_filter='CatalogRemoteConnectorExtensionContractTest.RemoteConnectorExtensionCatalogContracts:CatalogDatabaseBootstrapTest.RemoteConnectorLifecycleMaterializesDynamicOverlaySchema:CatalogDatabaseBootstrapTest.RemoteConnectorRenameMovesDynamicOverlaySchema:CatalogVirtualOverlayConformanceContractTest.VirtualOverlayConformance:SBLRV3OpcodeIdentity.MapsExpandedStatementFamilies'`
   - log:
     - `artifacts/udr/catalog/p6s1w2/udr-foundation-focused-tests.log:2`
     - `artifacts/udr/catalog/p6s1w2/udr-foundation-focused-tests.log:388`
     - `artifacts/udr/catalog/p6s1w2/udr-foundation-focused-tests.log:771`
     - `artifacts/udr/catalog/p6s1w2/udr-foundation-focused-tests.log:1157`
     - `artifacts/udr/catalog/p6s1w2/udr-foundation-focused-tests.log:1544`
     - `artifacts/udr/catalog/p6s1w2/udr-foundation-focused-tests.log:1554`

## Gate Decision
1. `UDR-CAT-GATE-03`: PASS.
2. `UDR-CAT-004` remains valid to execute next for lineage/attestation closure.

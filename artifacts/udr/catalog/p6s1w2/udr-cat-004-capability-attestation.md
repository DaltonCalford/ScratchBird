# UDR-CAT-004 Capability Lineage + Attestation Checks
Last-Modified: 2026-02-23

## Scope
Close `UDR-CAT-004` using the current catalog schema fields (`module_checksum`, `engine_version_text`, `source_version_text`, `discovered_time`) with strict runtime validation.

## Enforcement Added
1. Connector attestation minimums for `READY/DEGRADED`:
   - requires non-zero `module_checksum`.
   - requires non-empty `engine_version_text`.
   - evidence: `src/core/catalog_manager.cpp:78677`
2. Capability lineage requirements:
   - enabled capability requires non-empty `source_version_text`.
   - enabled capability source version must match connector engine version (when present).
   - `READY/DEGRADED` connectors cannot carry disabled capability lineage rows.
   - evidence: `src/core/catalog_manager.cpp:78999`
3. Lineage monotonicity:
   - `discovered_time` cannot move backwards.
   - if source/version or capability payload changes, `discovered_time` must advance.
   - evidence: `src/core/catalog_manager.cpp:79050`

## Test Evidence
1. Negative-path contract coverage:
   - missing attestation checksum: `tests/unit/test_catalog_remote_connector_extension_contract.cpp:132`
   - missing engine version text: `tests/unit/test_catalog_remote_connector_extension_contract.cpp:141`
   - missing capability lineage: `tests/unit/test_catalog_remote_connector_extension_contract.cpp:164`
   - capability lineage mismatch: `tests/unit/test_catalog_remote_connector_extension_contract.cpp:174`
   - stale discovered_time regression: `tests/unit/test_catalog_remote_connector_extension_contract.cpp:183`
2. Baseline fixtures updated for attestation-ready connector inserts:
   - `tests/unit/test_catalog_database_bootstrap.cpp:1091`
   - `tests/unit/test_catalog_database_bootstrap.cpp:1163`
   - `tests/unit/test_catalog_virtual_overlay_conformance_contract.cpp:124`
3. Focused gate run (all passing):
   - `artifacts/udr/catalog/p6s1w2/udr-foundation-focused-tests.log:2`
   - `artifacts/udr/catalog/p6s1w2/udr-foundation-focused-tests.log:1157`
   - `artifacts/udr/catalog/p6s1w2/udr-foundation-focused-tests.log:1554`

## Gate Decision
1. `UDR-CAT-GATE-04`: PASS (runtime lineage + attestation checks in place on current schema contract).
2. Next required slice: `UDR-CAT-005` (correlation + secret redaction/write-only controls).

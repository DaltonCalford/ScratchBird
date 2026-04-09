# Test Contract - 06_Fixed_Bootstrap_Page_Map

## Current code-backed tests
Canonical bootstrap constant contract:
- `tests/unit/test_vnext_bootstrap_contract.cpp:18`
- `tests/unit/test_vnext_bootstrap_contract.cpp:20`
- `tests/unit/test_vnext_bootstrap_contract.cpp:27`

Bootstrap FSM root creation and readback:
- `tests/unit/test_page_management.cpp:52`
- `tests/unit/test_page_management.cpp:65`

Page-zero format and compatibility validation:
- `tests/unit/test_database_format_compatibility.cpp:77`
- `tests/unit/test_database_format_compatibility.cpp:92`

Restore validation keeps bootstrap-derived page-size truth aligned:
- `tests/unit/test_restore_validation_rehearsal.cpp:50`
- `tests/unit/test_restore_validation_rehearsal.cpp:158`
- `tests/unit/test_restore_validation_rehearsal.cpp:159`

Corruption and boundary pressure on bootstrap-adjacent paths:
- `tests/unit/test_error_paths.cpp:29`
- `tests/unit/test_page_management_edge_cases.cpp:248`
- `tests/unit/test_page_management_edge_cases.cpp:300`

System-state control page is directly exercised by runtime-oriented tests:
- `tests/unit/test_executor_transaction_payload.cpp:334`
- `tests/unit/test_executor_transaction_payload.cpp:346`
- `tests/unit/test_garbage_collector.cpp:1264`
- `tests/unit/test_garbage_collector.cpp:1298`

## Required negative cases
The bootstrap contract must fail closed when:
- page `0` has invalid magic
- page `0` has invalid page size
- canonical bootstrap pages `1..5` have wrong page ids
- canonical bootstrap pages `1..5` have wrong page types
- page-zero compatibility fields are unsupported
- the main-file bootstrap map is truncated or unreadable

## Current evidence gaps
This section still retains broader expansion candidates, but the previously
missing dedicated corruption-matrix gate is now present.
`tests/unit/test_storage_recovery_gate_contract.cpp` explicitly corrupts the
canonical bootstrap pages `1..5` by wrong page type or wrong page id and also
truncates the fixed bootstrap map across every supported page size.

The remaining highest-value next tests are:
- one separation test proving tablespace-local page `0..1` cannot be mistaken for the main database bootstrap map
- one reserved-page contract test proving page `5` stays inert until a future owning feature is defined

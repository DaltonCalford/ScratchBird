# Repo Test Suite Registration and Execution Model

Status: current_authority_with_reconstructed_expansion

## 1. Scope

This file defines how the current `ScratchBird/tests` tree is actually registered and executed.

The resident file tree is larger than the active aggregate test inventory. The authoritative registration surface is `tests/CMakeLists.txt`, plus dedicated conformance shell entrypoints under `tests/conformance`.

## 2. Resident test-tree inventory buckets

Current resident top-level buckets under `ScratchBird/tests` include at least:

- `compatibility` (`56891` files)
- `conformance` (`3465` files)
- `unit` (`454` files)
- `integration` (`75` files)
- `stress` (`10` files)
- `v3` (`10` files)
- `benchmark` (`8` files)
- `sql` (`7` files)
- `mocks` (`5` files)
- `sequential` (`3` files)
- `tsan` (`3` files)
- `deprecated` (`2` files)
- `helgrind` (`2` files)
- `standalone` (`2` files)
- `compliance` (`1` file)
- `fuzz` (`1` file)
- `git` (`1` file)
- `manual` (`1` file)

The `tests/` root also contains loose harness files and several root-level
legacy test sources outside the main directory-backed families.

Resident presence does not by itself mean active CTest registration.
The file-volume dominance of `compatibility` and `conformance` is therefore not
equivalent to aggregate GoogleTest-discovered execution breadth.

## 3. Platform split

### 3.1 Windows path

On Windows, the suite uses a reduced portable subset and builds a single `scratchbird_tests` target with a narrow source list. This is an intentionally reduced lane.

### 3.2 Non-Windows path

On non-Windows platforms, the suite builds a larger aggregate binary and multiple dedicated targets.

## 4. Aggregate binary model

### 4.1 Aggregate source globs

The non-Windows aggregate `scratchbird_tests` target globs from:

- `unit/*.cpp`
- `unit/types/*.cpp`
- `unit/domains/*.cpp`
- `integration/*.cpp`
- `benchmark/*.cpp`

### 4.2 Aggregate exclusions

The aggregate target then excludes many files for reasons including:

- deprecated location
- standalone `main()` ownership
- API refactor drift
- dedicated-target ownership
- linux-only gating
- parser boundary isolation
- external-client ownership
- quarantine or manual repair state

Therefore, the aggregate binary is curated, not a literal build of every file under the globs.

## 5. Aggregate discovery and label model

The aggregate GoogleTest discovery model currently defines these principal label families:

- `unit`
- `smoke`
- `cross_os_runtime`
- `parser` by dialect
- `stress`
- `performance`
- `integration`
- `linux_only`
- `quarantine`

The shell runner maps those labels into higher-level modes such as `quick`, `ci`, `all`, `portable`, and `windows_portable`.

## 6. Dedicated binary families

The current dedicated-binary registration surface includes the following target groups.

### 6.1 Sequencing and scheduler

- `scratchbird_tests_sequential`
- `test_scheduler_contracts`

### 6.2 Parser and parser-agent boundaries

- `test_firebird_parser_contracts`
- `test_firebird_parser_agent_contracts`
- `test_firebird_parser_boundary_contracts`
- `test_dialect_compiler_udr_contracts`
- `test_emulated_parser_boundary_contracts`
- `test_postgresql_parser`
- `test_mysql_query_compiler`

### 6.3 Protocol, IPC, and conformance

- `test_ipc_server`
- `test_wire_protocol`
- `test_sbwp_frame_conformance`
- `test_pg_frame_conformance`
- `test_mysql_frame_conformance`
- `test_firebird_frame_conformance`
- `test_protocol_frame_conformance`
- `test_transaction_truth_native`
- `test_engine_ipc_session_handler`
- `test_ipc_integration_harness`
- `test_v3_derived_table`
- `test_copy_1gb`

### 6.4 Memory, fuzz, and race-oriented lanes

- `test_memory_safety`
- `helgrind_races`
- `multithreaded_stress`
- `auth_rate_limit_stress`
- `auth_plugin_payload_fuzz`
- `auth_provider_fail_closed_stress`
- `auth_plugin_enterprise_perf`
- `auth_plugin_enterprise_soak`
- `operational_reliability_soak`

### 6.5 Type, range, and columnstore lanes

- `test_text_search_types`
- `test_range_types`
- `test_temporal_range_types`
- `test_range_operators`
- `test_network_types`
- `test_columnstore_rle`
- `test_columnstore_dict`
- `test_columnstore_bitpack`
- `test_columnstore_predicate`
- `test_columnstore_segments`
- `test_columnstore_comprehensive`
- `test_columnstore_simple_e2e`
- `test_columnstore_load_simple`
- `test_columnstore_persistence`

### 6.6 Security, domain, authentication, and relational-integrity lanes

- `test_security_phase3_3`
- `test_security_phase3_5_rls_dml`
- `test_auth_plugin_enterprise_matrix`
- `test_auth_plugin_enterprise_fail_closed`
- `test_check_constraints`
- `test_domain_integrity`
- `test_domain_encryption`
- `test_domain_security`
- `test_domain_validation`
- `test_domain_quality`
- `test_domain_e2e_scenarios`
- `test_foreign_keys`
- `test_composite_fk`
- `test_auth_bootstrap_claim`
- `test_auth_mfa_challenge_flow`
- `test_auth_plugin_admission`
- `test_auth_plugin_ident`
- `test_auth_plugin_kerberos`
- `test_auth_plugin_ldap`
- `test_auth_plugin_manager`
- `test_auth_plugin_p2_admission`
- `test_auth_plugin_pam`
- `test_auth_plugin_radius`
- `test_auth_plugin_registry_negotiation`
- `test_auth_plugin_v1a_methods`
- `test_auth_policy_protocol_parity`
- `test_auth_provider_defaults`
- `test_manager_proxy_mcp`

### 6.7 Index and storage correction lanes

- `test_gist_mvcc`
- `test_brin_mvcc`
- `test_brin_dml`
- `test_concurrent_page_access`
- `test_buffer_pool_exhaustion`
- `test_index_dml_integration`
- `test_gin_dml`
- `test_heap_page_ownership`
- `test_hnsw_gc`
- `test_fulltext_gc`
- `test_brin_gc`
- `test_rtree_gc`
- `test_gin_gc`
- `test_shadow_index_rebuild`

### 6.8 Miscellaneous contract utilities

- `test_git_config_parser`
- `test_sblr_type_opcodes`
- `test_fdw_protocol_adapter_factory`
- `test_udr_connector_factory`

## 7. Conformance shell lanes outside aggregate gtest discovery

Current code-backed shell or data-driven conformance entrypoints include:

- `tests/conformance/public_beta/run_required_public_beta_gate.sh`
- `tests/conformance/security/run_security_parity_matrix.sh`
- `tests/conformance/v3_native_inet/sql/09_security_default_access_public.sql`
- `tests/conformance/v3_native_inet/sql/10_security_ownership_alter_owner.sql`
- `tests/conformance/v3_native_inet/sql/11_security_grants_dml_execute_view.sql`
- `tests/conformance/v3_native_inet/sql/12_security_show_visibility.sql`
- `tests/conformance/v3_native_inet/sql/13_security_row_level_security.sql`
- `tests/conformance/v3_native_inet/sql/14_security_column_level.sql`
- `tests/conformance/v3_native_inet/sql/15_security_domain_masking.sql`

These conformance lanes are authoritative even though they are not ordinary aggregate gtest registration.

## 8. Sequential and optional lanes

### 8.1 Sequential lane

`SequentialTestSuite` runs `scratchbird_tests_sequential` as a single non-parallel CTest lane for tests that conflict on shared singletons or other global resources.

### 8.2 Optional external-client lane

External-client tests exist behind:

- `SCRATCHBIRD_ENABLE_CLIENT_TESTS=ON`

These are not part of the default portable aggregate lane.

## 9. Compatibility lane relationship

Compatibility is not registered through the same simple aggregate GoogleTest path. It has its own vendored-snapshot, conversion, and CTest-list machinery under `tests/compatibility`.

## 10. Required execution interpretation

A correct implementer or operator must distinguish:

- resident test files
- aggregate GoogleTest-discovered inventory
- dedicated `add_test` binaries
- sequential-only lanes
- shell-driven conformance lanes
- optional external-client lanes
- compatibility lanes
- manual or deprecated resident content

## 11. Security and management gate interpretation

Security, authorization, manager-proxy, and remote-management proof is currently split across:

- aggregate or dedicated GoogleTest binaries
- shell-driven conformance lanes
- protocol and native-inet SQL expected-output lanes

Therefore, no section `31` security gate may assume that a single `ctest` aggregate run alone proves the full security or management contract.

## 12. Non-authority and rejection rules

- resident presence does not imply maintained release authority
- benchmark presence does not imply gating authority unless explicitly promoted in section `31`
- compatibility or shell-driven evidence cannot be silently collapsed into generic aggregate gtest proof
- reconstructed remote-management queue and heartbeat gates remain required canon even where current repo entrypoints are still incomplete

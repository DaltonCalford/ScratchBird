# ScratchBird Test Tree Family Breakdown and Label Taxonomy

## Purpose

Define the repository-local ScratchBird test tree, its family layout, and the active label taxonomy used by CTest.

## Top-Level Test Families

The current repository test tree includes direct directory-backed families:

- `unit`
- `integration`
- `benchmark`
- `compliance`

It also includes execution families that are proven by runners or exclusion
policy rather than by a top-level directory:

- `smoke`
- `portable`
- `windows_portable`
- `linux_only`
- `stress`
- `performance`
- `quarantine`
- `compatibility`
- `conformance`
- `sequential`
- `standalone`
- `manual`
- `deprecated`
- `tsan`
- `helgrind`
- `sql`

These are real repository lanes and should be described as distinct evidence families rather than one undifferentiated “test suite”.

## Current directory-backed inventory

Current code-backed directories include:

- `tests/unit`
- `tests/unit/types`
- `tests/unit/domains`
- `tests/integration`
- `tests/benchmark`
- `tests/compliance`
- `tests/compatibility`
- `tests/conformance`
- `tests/stress`
- `tests/sequential`
- `tests/standalone`
- `tests/sql`
- `tests/fuzz`
- `tests/tsan`
- `tests/helgrind`
- `tests/manual`
- `tests/deprecated`
- `tests/git`
- `tests/v3`

Current recovered top-level file counts are:

- `compatibility`: `56891`
- `conformance`: `3465`
- `unit`: `454`
- `integration`: `75`
- `stress`: `10`
- `v3`: `10`
- `benchmark`: `8`
- `sql`: `7`
- `mocks`: `5`
- `sequential`: `3`
- `tsan`: `3`
- `deprecated`: `2`
- `helgrind`: `2`
- `standalone`: `2`

The test tree also contains loose top-level harness files plus a small number of
root-level legacy test sources outside the directory-backed families.

Representative current benchmark files include:

- `benchmark_suite.cpp`
- `test_auth_plugin_enterprise_perf.cpp`
- `test_btree_proof_corpus.cpp`
- `test_cache_buffer_benchmark.cpp`
- `test_front_door_mode_benchmark.cpp`
- `test_optimizer_cost_calibration.cpp`
- `test_parser_v3_benchmark.cpp`
- `test_sblr_jit_performance.cpp`

Representative current compliance shell contracts include:

- `test_vnext_scope_scan_contract.sh`

That compliance lane is not a GoogleTest lane. It is a shell contract lane with
its own exit-code and report-content assertions.

## Active Label Taxonomy

The CMake test registration currently uses a large label vocabulary, including:

- general execution class:
  - `unit`
  - `integration`
  - `stress`
  - `performance`
  - `soak`
  - `fuzz`
  - `sequential`
  - `tsan`
- protocol and client families:
  - `protocol`
  - `wire`
  - `ipc`
  - `client`
  - `postgresql`
  - `mysql`
  - `firebird`
- parser and compiler families:
  - `parser`
  - `parser_agent`
  - `compiler_udr`
  - `v3`
  - `native`
  - `emulated`
- storage and memory families:
  - `memory`
  - `buffer_pool`
  - `storage`
  - `columnstore`
  - `lsm`
- security families:
  - `security`
  - `auth_plugin`
  - `enterprise`
- index families:
  - `index`
  - `gist`
  - `brin`
  - `gin`
  - `hnsw`
  - `fulltext`
  - `rtree`
- evidence and release classes:
  - `conformance`
  - `compatibility`
  - `public_beta`
  - `hard_gate`
  - `critical`
  - `quarantine`
  - `disabled`

## Active Versus Scaffolded Coverage

The test tree intentionally contains a mixture of:

- active registered tests
- discoverable gtest families
- commented or scaffolded add-test blocks
- placeholder or deferred families

A test file existing under `tests/` does not by itself mean it participates in the default build or aggregate CTest run.

## Important Family Examples

Examples of active, explicitly registered families include:

- frame conformance tests for native and emulated protocols
- transaction truth and MGA restart tests
- security phase 3 column permissions and RLS DML enforcement
- domain security, encryption, validation, quality, and end-to-end scenarios
- columnstore, BRIN, GiST, GIN, HNSW, R-tree, and shadow-rebuild lanes
- auth plugin matrix, fail-closed, stress, soak, and benchmark lanes
- compatibility and public-beta conformance runners

Representative current code-backed examples in the tree include:

- security and auth:
  - `test_auth_plugin_manager.cpp`
  - `test_auth_plugin_registry_negotiation.cpp`
  - `test_auth_mfa_challenge_flow.cpp`
  - `test_data_masking.cpp`
  - `test_security_phase3_4_rls.cpp`
- MGA and transaction:
  - `test_mga_back_versioning.cpp`
  - `test_transaction_manager.cpp`
  - `test_multi_index_mga.cpp`
  - `test_toast_crash_recovery_mga.cpp`
- optimizer and planning:
  - `test_index_advisor.cpp`
  - `test_optimizer_vnext_plan_selection.cpp`
  - `test_query_planner_integration.cpp`
- JIT and SBLR:
  - `test_sblr_jit_runtime_selector.cpp`
  - `test_sblr_jit_llvm_provider.cpp`
  - `test_sblr_v3_payload_codec.cpp`
- index families:
  - `test_brin_index.cpp`
  - `test_hash_index.cpp`
  - `test_rtree.cpp`
  - `test_hnsw_index.cpp`
  - `test_gin_basic.cpp`

## Shell-lane taxonomy

`tests/run_tests.sh` proves these repo-local selector lanes:

- `smoke`
- `portable`
- `windows_portable`
- `linux_only`
- `unit`
- `integration`
- `stress`
- `performance`
- `quarantine`
- `quick`
- `ci`
- `all`

These selectors are execution lanes over labels and exclusions.
They are not one-to-one with directory names.

## Interpretation Rule

Any documentation of “the full ScratchBird test suite” shall distinguish:

- file-tree presence
- CMake registration
- active CTest discoverability
- shell-lane selection
- standalone compliance execution
- hard-gate inclusion

Those axes are not identical.

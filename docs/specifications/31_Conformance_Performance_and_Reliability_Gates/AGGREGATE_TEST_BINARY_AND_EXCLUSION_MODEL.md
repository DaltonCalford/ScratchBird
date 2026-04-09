# Aggregate Test Binary and Exclusion Model

## Purpose

This document defines how the in-repo aggregate GoogleTest binary is built, what
it intentionally includes, what it intentionally excludes, and how those
exclusions relate to dedicated lanes.

## Current code-backed authority

The current aggregate test-binary authority is `tests/CMakeLists.txt`.

The build entrypoint used by `tests/run_tests.sh` is:
- `cmake --build <build_dir> --target scratchbird_test_binaries`

The aggregate GoogleTest executable is:
- `scratchbird_tests`

On Windows, the aggregate binary is intentionally reduced to a portable subset.
On non-Windows builds, the aggregate binary is produced from globbed unit,
integration, and benchmark sources and then filtered through explicit exclusion
rules.

The aggregate binary therefore represents a curated execution lane, not a raw
file-tree projection of every `.cpp` file under `tests/`.

## Windows aggregate lane

The current Windows aggregate lane includes a small fixed source set and labels
it as:
- `unit`
- `smoke`
- `integration`

This is a portability lane, not full test-matrix proof.

The Windows subset exists because:
- the wider Linux-oriented matrix pulls in POSIX-only APIs
- runtime-DLL discovery and test discovery differ from Linux
- some auth and hashing paths are unavailable in the Windows CI environment

## Non-Windows aggregate source discovery

The current non-Windows aggregate lane begins by globbing:
- `unit/*.cpp`
- `unit/types/*.cpp`
- `unit/domains/*.cpp`
- `integration/*.cpp`
- `benchmark/*.cpp`

The resulting set is then explicitly filtered.

This means aggregate admission is policy-driven in two phases:
- discover widely
- narrow explicitly

## Canonical exclusion classes

The exclusion rules are part of the current implementation authority and shall
be documented under these classes:
- `deprecated`
- `standalone_main`
- `api_refactor_blocked`
- `dedicated_target`
- `sequential_lane_only`
- `external_client_or_parser_split`
- `windows_portable_subset_only`
- `manual_or_investigation`

## Current major exclusion reasons

The current code-backed exclusion reasons include:
- deprecated tests are excluded entirely
- tests with standalone `main()` are excluded from the aggregate binary
- tests needing dedicated harnesses or dedicated targets are excluded from the
  aggregate binary
- some integration tests are forced into sequential or dedicated lanes
- some parser-agent or external-client tests are excluded due to API mismatch or
  dedicated-boundary requirements
- some tests remain excluded because of refactored executor or parser APIs

Representative current examples include:
- `test_bytecode_executor.cpp`
- `integration/test_gist_dml.cpp`
- `unit/test_parser.cpp`
- `unit/test_parser_integration.cpp`
- `test_firebird_parser_agent.cpp`
- `test_mysql_parser_agent.cpp`
- `test_postgresql_parser_agent.cpp`
- `test_firebird_parser_boundary_contract.cpp`
- `test_emulated_parser_boundary_contracts.cpp`
- `test_buffer_error_consistency.cpp`
- `test_cache_bounded.cpp`
- `test_clog_checksum.cpp`
- `test_columnstore_bitpack.cpp`
- `test_hot_updates.cpp`
- `test_snapshot_sorted.cpp`
- `test_text_search_phase2.cpp`
- `test_transaction_markers_race.cpp`
- `test_wraparound_detection.cpp`
- `integration/manual_test_planner_integration.cpp`

These exclusions are implementation truth, not documentation drift.

## Important aggregate-binary rule

The aggregate binary is not the full universe of available tests.
It is one execution lane.

Therefore:
- absence from `scratchbird_tests` does not mean the feature lacks tests
- presence in the source tree does not mean the test participates in the
  default aggregate lane
- the canonical test program must document both aggregate and dedicated lanes

## Dedicated-lane classes required by current tree

The source tree already proves or strongly implies dedicated lanes for:
- compatibility suites
- compliance shell scripts
- sequential tests
- standalone tests
- stress suites
- tsan suites
- helgrind suites
- fuzzing inputs
- SQL fixture tests
- parser or protocol boundary tests with dedicated harnesses

It also proves a repo-local shell lane split through `tests/run_tests.sh`, which
selects CTest subsets independently of source-directory layout.

## Aggregate-binary inclusion standard

A test belongs in the aggregate GoogleTest binary only when:
- it is GoogleTest-formatted
- it does not require a dedicated standalone `main()`
- it does not require incompatible external-client harnessing
- it does not require an isolated sequential lane
- it is not explicitly quarantined by the build rules

## Recovered implementation truth

The aggregate lane is currently designed as a converged shared binary with
incremental migration from older standalone tests.

That means the canonical specification shall preserve these truths:
- conversion into GoogleTest is an explicit enablement step
- some tests remain intentionally outside the aggregate lane
- the aggregate lane is not allowed to silently absorb tests that require
  special ordering, process layout, or harnesses

## Required certification consequence

A full conformance claim may not rely only on `scratchbird_tests`.
It must account for:
- aggregate binary execution
- repo-local shell-lane execution
- standalone compliance contract execution
- dedicated target execution
- sequential execution
- stress execution
- compatibility execution
- explicit excluded and quarantined lanes

## Non-guarantees

The aggregate binary does not guarantee:
- execution of every file under `tests/`
- coverage of every platform-specific lane
- coverage of every external compatibility harness
- coverage of every manually-invoked or dedicated standalone test

# Test Directory Family and Dedicated Lane Breakdown

## Purpose

This document records the current source-tree test families and their intended
execution lanes from the actual `ScratchBird/tests` tree.

## Top-level family model

The current repository test tree includes at least these top-level families:
- `unit`
- `integration`
- `benchmark`
- `compatibility`
- `compliance`
- `stress`
- `sequential`
- `standalone`
- `sql`
- `fuzz`
- `tsan`
- `helgrind`
- `manual`
- `deprecated`
- `git`

These are distinct operational lanes, not just folders.

## Unit family

The unit family covers, among other areas:
- auth providers and auth plugins
- audit logging
- backup catalog and execution policy
- bitmap, BRIN, B-tree, and buffer-pool behavior
- bytecode opcodes and parser/compiler contracts
- catalog extension and persistence contracts
- clock, cluster, and control-plane behavior
- compression and encryption
- dependency tracking
- domain and masking behavior
- cost model and optimizer-support surfaces

The unit family is the broadest aggregate-binary lane.

## Integration family

The integration family covers, among other areas:
- index DML, GC, MVCC, and runtime behavior
- cache integration
- constraint and FK behavior
- copy and client flows
- domain end-to-end scenarios
- emulated protocol and client integration
- multi-index MGA behavior
- parser and bytecode integration
- tablespace and shadow rebuild flows
- TOAST crash recovery and garbage collection
- v3 derived-table and view flows

Many integration tests are not run through the aggregate lane without further
harness or exclusion handling.

## Benchmark family

The in-repo benchmark family includes at least:
- auth plugin enterprise performance
- B-tree proof corpus benchmarking
- cache and buffer benchmarking
- front-door mode benchmarking
- optimizer cost calibration
- parser v3 benchmarking
- SBLR JIT performance benchmarking

Recovered concrete benchmark-family examples:
- `benchmark_suite.cpp`
  - timed GoogleTest microbenchmark suite with warmup and stdout result printing
- `test_cache_buffer_benchmark.cpp`
  - scan-resistance and hot-page retention benchmark for `Normal` versus `Sequential` buffer access strategy
- `test_sblr_jit_performance.cpp`
  - VM versus JIT p95 envelope guard for a simple arithmetic statement
- `test_auth_plugin_enterprise_perf.cpp`
  - enterprise auth-provider latency and RSS benchmark against a phase-1-equivalent baseline
- `test_btree_proof_corpus.cpp`
  - B-tree proof corpus benchmark with insert and search average-microsecond outputs
- `test_front_door_mode_benchmark.cpp`
  - front-door proxy or manager overhead benchmark using the native wire path
- `test_optimizer_cost_calibration.cpp`
  - cost-model calibration corpus benchmark with optional CSV evidence export
- `test_parser_v3_benchmark.cpp`
  - parser `v3` compile-loop benchmark across statement classes

The benchmark family is a mixed lane:
- some files are true microbenchmarks
- some files are calibration guards
- some files are performance-envelope regression tests

They must not all be treated as equivalent release benchmark proof.

## Conformance family

The conformance family is a release-significant truth lane, not just extra tests.

Recovered current conformance subfamilies:
- `protocol`
  - deterministic frame-shape and negative-frame conformance
- `transactions`
  - transaction-truth matrix and native transaction truth executable
- `security`
  - parity matrix and shell runner
- `public_beta`
  - required public beta gate shell orchestrator
- `v3_native_inet`
  - native-listener TCP-path parser conformance
- `v3_native_comparative_regression`
  - donor-derived comparative regression for native `v3`

These subfamilies are dedicated-lane truth surfaces and must not be collapsed into “ordinary unit tests.”

This family is part of repo-local benchmark and calibration lanes, distinct from
`ScratchBird-Benchmarks`.

## Compatibility family

The compatibility family is a dedicated external or vendored compatibility lane.
It includes its own:
- list summary
- README
- longer-running compatibility-specific timeouts and result trees

Compatibility execution is governed separately from ordinary integration timeouts.

Recovered external benchmark-project comparative families that complement, but do not replace, the in-repo compatibility lane:
- `index-comparison-tests`
- `engine-differential-tests`
- `performance-tests`
- `stress-tests`
- `tpc-c`
- `tpc-h`

## Compliance family

The compliance family includes explicit shell-driven contract checks, such as
`test_vnext_scope_scan_contract.sh`.

These are specification or contract gates, not ordinary unit tests.

## Sequential family

Sequential tests exist where ordering or shared-resource isolation is required.
Current examples include:
- cache integration sequential
- GiST DML sequential
- job scheduler sequential

A sequential family test is intentionally not interchangeable with aggregate
parallel GoogleTest discovery.

## Standalone family

Standalone tests exist for isolated process or minimal-engine scenarios.
Current examples include:
- `test_clog_standalone.cpp`
- `test_minimal_db.cpp`

Standalone tests are dedicated-lane tests by design.

## SQL family

The SQL family captures SQL fixture and script-driven contract surfaces, such as:
- sequences
- truncate
- UTF-8 identifiers
- views
- parity scripts

These are canonical SQL contract fixtures, not just examples.

## Fuzz, TSAN, and Helgrind families

These families represent specialized execution environments:
- fuzz:
  - malformed payload and robustness inputs
- tsan:
  - race-detection lanes
- helgrind:
  - lock and race diagnostics with suppression configuration

They are required specialized gates and may not be collapsed into ordinary unit
or integration status.

## Stress family

The stress family covers long-duration or high-volume behavior, including:
- auth-provider fail-closed stress
- auth-rate limiting stress
- multithreaded stress
- operational reliability soak
- TOAST concurrency
- columnstore load and batch performance
- LSM stress

Recovered stress-family example with mixed classification:
- `test_columnstore_batch_performance.cpp`
  - validates batch scan, predicate evaluation, and RLE correctness
  - belongs to a stress-supporting lane
  - is not current authority for throughput certification by itself

Other recovered current stress examples:
- `test_auth_plugin_enterprise_soak.cpp`
  - multi-hour simulated reconnect and provider-fault soak
- `test_operational_reliability_soak.cpp`
  - operational governance and support-bundle soak

Stress lanes are intentionally distinct from quick or CI lanes.

## Manual and deprecated families

`manual` holds non-default operator or developer invocation tests.
`deprecated` holds retired or superseded tests and is not active execution
authority.

## Canonical execution-lane rule

Every test-family artifact must say which lane it belongs to:
- aggregate GoogleTest
- dedicated GoogleTest target
- sequential
- standalone
- SQL script
- shell contract
- fuzz
- tsan
- helgrind
- stress
- compatibility
- manual

Without that lane label, the artifact is incomplete.

## Full-cycle reporting rule

A full clean, build, test, and benchmark cycle must distinguish at minimum:
- aggregate correctness pass or fail
- dedicated compatibility lane execution
- dedicated benchmark lane execution
- stress-lane execution
- dedicated conformance lane execution
- microbenchmark or envelope-only outputs
- external benchmark-project matrix outputs

Collapsing these into a single undifferentiated `tests passed` or `benchmarks passed` claim is non-conforming.

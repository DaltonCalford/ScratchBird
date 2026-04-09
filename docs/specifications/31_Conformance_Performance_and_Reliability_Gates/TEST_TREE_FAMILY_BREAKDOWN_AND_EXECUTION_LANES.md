# Test Tree Family Breakdown and Execution Lanes

Status: current_authority

## Purpose

This file defines what the in-repo `ScratchBird/tests` tree currently contains, how those tests are grouped, which lanes are expected to participate in a full clean build and test cycle, and which directories are supporting or non-aggregate lanes.

## Current code-backed authority

The current test tree is organized around these top-level families:

1. `unit/`
- primary high-volume C++ correctness lane
- covers storage, transaction, MGA visibility, catalog, parser, SBLR, JIT, security, protocol, observability, backup, and management contracts
- representative families include:
  - transaction and MGA: `test_transaction_manager.cpp`, `test_mga_back_versioning.cpp`, `test_subtransactions.cpp`
  - indexes: `test_btree_mga_compliance.cpp`, `test_hash_index.cpp`, `test_hnsw_index.cpp`, `test_rtree.cpp`
  - planner and optimizer: `test_cost_model.cpp`, `test_optimizer_vnext_plan_selection.cpp`, `test_query_planner_integration.cpp`
  - SBLR and JIT: `test_sblr_v3_container.cpp`, `test_sblr_jit_llvm_provider.cpp`, `test_sblr_jit_vm_native_equivalence.cpp`
  - security: `test_data_masking.cpp`, `test_auth_plugin_registry_negotiation.cpp`, `test_catalog_security_extension_contract.cpp`

2. `integration/`
- end-to-end feature and subsystem interaction lane
- validates DDL, DML, indexes, columnstore, TOAST, protocol adapters, security flows, and multi-subsystem MGA behavior
- representative files include `test_multi_index_mga.cpp`, `test_toast_crash_recovery_mga.cpp`, `test_firebird_client.cpp`, `test_psql_client.cpp`, `test_mysql_client.cpp`

3. `v3/`
- native V3 parser, protocol, SBLR, executor, dialect, and IPC subtree
- used when validating the native V3 surface independently of broader unit/integration suites

4. `benchmark/`
- in-repo microbenchmark and calibration lane
- includes `benchmark_suite.cpp`, parser and JIT benchmarks, cost calibration, front-door benchmarks, auth plugin performance, and cache or buffer measurements
- this lane is not the same system as the external `ScratchBird-Benchmarks` matrix project

Recovered current benchmark-file examples include:
- `benchmark_suite.cpp`
  - generic timed GoogleTest suite with warmup and stdout-reported throughput metrics
- `test_cache_buffer_benchmark.cpp`
  - scan-resistance and hot-page retention benchmark for `Normal` versus `Sequential` buffer access
- `test_sblr_jit_performance.cpp`
  - VM-versus-JIT p95 envelope benchmark
- `test_auth_plugin_enterprise_perf.cpp`
  - phase-1-equivalent baseline versus enterprise provider-method latency and RSS delta benchmark
- `test_btree_proof_corpus.cpp`
  - B-tree insert and search proof corpus benchmark emitting `BTREE_PROOF_SCENARIO` rows
- `test_front_door_mode_benchmark.cpp`
  - front-door mode overhead benchmark through actual socket and wire-protocol connect or auth flow
- `test_optimizer_cost_calibration.cpp`
  - fixed-seed cost-model calibration benchmark with optional CSV export via environment variable
- `test_parser_v3_benchmark.cpp`
  - parser V3 compile-loop benchmark for simple, complex, DDL, and transaction statements

5. `compatibility/`
- cross-engine compatibility and comparative execution harness
- organized around Firebird, PostgreSQL, MySQL, and ScratchBird result trees plus scripts and result storage
- may run through dedicated scripts or shell orchestration rather than the plain aggregate unit-test path

6. `conformance/`
- protocol, security, transaction, public beta, and comparative regression lanes
- used for maintained conformance and release-significant behavior validation

Recovered current conformance sublanes include:
- `protocol/`
  - deterministic frame-shape conformance for:
    - native `sbwp`
    - PostgreSQL v3 emulation
    - MySQL 8x emulation
    - Firebird remote emulation
  - includes:
    - golden trace checksum index
    - per-protocol frame conformance tests
    - cross-lane negative protocol matrix assertions
- `transactions/`
  - deterministic transaction truth validation
  - includes:
    - `TRANSACTION_TRUTH_MATRIX.md`
    - native truth executable
    - matrix runner with diff artifacts
- `v3_native_inet/`
  - validates the canonical `v3` parser through the native listener TCP path
  - registered as `ConformanceV3NativeParserInet`
- `v3_native_comparative_regression/`
  - frozen donor-derived comparative corpus against donor engines plus ScratchBird native `v3`
  - registered as `ConformanceV3NativeComparativeRegression`
- `public_beta/`
  - shell-driven required public beta gate that orchestrates multiple `ctest` and compatibility-script lanes

7. `stress/`
- soak, concurrency, long-running, and high-volume lanes
- representative files include auth stress, operational reliability soak, columnstore load, LSM stress, and multithreaded runtime tests

Recovered current stress-file examples include:
- `test_auth_plugin_enterprise_soak.cpp`
  - multi-hour simulated mixed-method reconnect and fault-injection soak for enterprise auth providers
- `test_operational_reliability_soak.cpp`
  - operational governance, support-bundle, readiness, and admission-policy soak surface
- `test_columnstore_load.cpp`
- `test_columnstore_load_simple.cpp`
- `test_lsm_tree_stress.cpp`
- `test_multithreaded_stress.cpp`
- `test_toast_concurrency.cpp`

## Concrete in-repo benchmark and stress artifacts recovered from code

### `tests/benchmark/benchmark_suite.cpp`

This file is a self-reporting GoogleTest benchmark suite with a reusable `BenchmarkResult` envelope that prints:
- benchmark name
- iteration count
- total time in milliseconds
- average time in milliseconds
- operations per second
- optional data-size and derived row rate

The current recovered benchmark families in this file include:
- sequential scan style workloads
- filtered scan workloads
- projection scan workloads
- aggregate and analytical workloads
- join and transaction-throughput style workloads declared by the benchmark-suite header

The current benchmark harness behavior includes:
- fixed-seed reproducibility using `std::mt19937{42}`
- warmup before timed iterations
- repeated timed iterations
- stdout printing as the current result surface

This file is a microbenchmark and calibration lane. It is not release-significant correctness proof by itself.

### `tests/benchmark/test_cache_buffer_benchmark.cpp`

This file is authoritative for the current cache and buffer scan-resistance benchmark.

Recovered workload contract:
- creates a database at canonical bootstrap page size `16384`
- allocates `512` pages
- reserves a hot set of `16` pages
- compares:
  - `BufferPool::AccessStrategy::Normal`
  - `BufferPool::AccessStrategy::Sequential`

Recovered emitted metrics:
- `hits`
- `misses`
- `hit_ratio`
- `scan_ms`
- `hot_ms`

Recovered printed benchmark labels:
- `ScanResistance Normal`
- `ScanResistance Sequential`

The intent is not generic “buffer speed.” The lane specifically measures scan resistance and hot-page retention under access-strategy changes.

### `tests/benchmark/test_sblr_jit_performance.cpp`

This file is authoritative for the current SBLR/JIT performance-envelope benchmark.

Recovered workload contract:
- statement: `SELECT 100 + 23`
- `20` samples for VM execution and `20` samples for JIT-preferred execution
- compares:
  - `JitCompileMode::EXPLICIT_ONLY` plus `JitExecutionPolicy::INTERPRETED_ONLY`
  - `JitCompileMode::JIT_ALLOWED` plus `JitExecutionPolicy::PREFER_NATIVE`

Recovered metric contract:
- `vm_p95_us`
- `jit_p95_us`

Recovered current acceptance rule:
- `jit_p95_us` must be less than or equal to `vm_p95_us * 3 + 1`

This lane is an envelope guard, not a promise that JIT is always faster than the interpreter on microstatements.

### `tests/stress/test_columnstore_batch_performance.cpp`

Despite the file name, the current stress artifact is primarily correctness-first with light performance implications.

Recovered validated behaviors:
- RLE compression and decompression round-trip
- predicate evaluation count correctness
- batch scan iterator correctness

Recovered workload details:
- creates a columnstore index with explicit segment size and compression type
- inserts `2048` rows for batch scan validation
- uses MGA transaction identity via the current transaction manager during scan begin

This file must be classified as a stress or correctness-supporting lane, not as a benchmark authority for release-performance claims.

8. `sequential/`
- serialization-sensitive or controlled scheduling lane
- used where deterministic sequential execution is required for meaningful validation

9. `standalone/`
- minimal database and low-dependency test executables
- used to validate storage or bootstrap behavior without requiring the full aggregate environment

10. `fuzz/`, `tsan/`, `helgrind/`
- specialized dynamic-analysis lanes
- `fuzz/` targets malformed payload and parser or auth boundaries
- `tsan/` targets race detection
- `helgrind/` targets concurrency tooling with suppression support

11. `sql/`
- SQL script corpus and file-backed query fixtures
- supports parity and script-driven validation

12. `manual/`, `deprecated/`, `git/`, `mocks/`
- non-primary aggregate lanes
- `manual/` requires operator invocation
- `deprecated/` is reference-only unless explicitly re-promoted
- `git/` and `mocks/` are supporting families rather than broad product-certification lanes

## Full clean build and test cycle lane contract

A full clean build and test cycle is required to account for the following classes even when orchestration differs:

1. compile the engine, test harnesses, protocol fixtures, and maintained compatibility helpers
2. run the aggregate maintained `ctest` lane rooted in `tests/CMakeLists.txt`
3. run any maintained shell-driven or script-driven compatibility and conformance entrypoints that are not naturally expressed as a single `ctest` target
4. emit metrics and gate evidence into the current `tests/results/` tree
5. preserve compatibility outputs under `tests/compatibility/results/` when that lane is enabled
6. preserve benchmark outputs separately from ordinary correctness results

A full clean build and test cycle is not allowed to silently claim coverage for:
- `manual/`
- `deprecated/`
- quarantined or skipped files such as `*.skip`
- non-maintained experimental lanes
unless the orchestration explicitly includes and records them.

## Lane ownership and expectations

| Lane | Primary purpose | Aggregate by default | Notes |
| --- | --- | --- | --- |
| `unit` | contract and component correctness | yes | highest breadth lane |
| `integration` | subsystem interaction | yes | feature and MGA interaction proof |
| `v3` | native V3 and SBLR surface proof | yes or lane-specific | may be split into dedicated executables |
| `benchmark` | microbenchmark and calibration | no release correctness by itself | evidence, not correctness truth |
| `compatibility` | cross-engine comparison | lane-specific | often shell or script orchestrated |
| `conformance` | maintained behavioral gates | lane-specific | release-significant |
| `stress` | soak and high-load proof | optional by stage | non-trivial runtime cost |
| `sequential` | deterministic serialized proof | lane-specific | used where scheduler interference would invalidate the test |
| `standalone` | minimal bootstrap/storage proof | lane-specific | reduced harness dependence |
| `fuzz` / `tsan` / `helgrind` | dynamic analysis | stage-gated | tooling and environment dependent |

## Required operator understanding

- The in-repo test tree is broader than a single `ctest` execution.
- A release-quality full cycle must record which directories were executed, skipped, quarantined, or stage-gated.
- Benchmark outputs are evidence artifacts and must not be confused with functional pass or fail status.
- Compatibility and comparative lanes are maintained but may use shell runners or nested harnesses instead of the base aggregate CMake path.
- Files that contain the words `benchmark` or `performance` are not automatically throughput-certification lanes. Some are correctness-first guards with lightweight timing output.
- The conformance tree also contains shell-driven and fixture-driven lanes whose authority is higher than ordinary microbenchmark output for release or parity claims.

## Explicit non-authority

This file does not redefine the external benchmark project. That ownership remains in section `31` benchmark files covering `ScratchBird-Benchmarks`.

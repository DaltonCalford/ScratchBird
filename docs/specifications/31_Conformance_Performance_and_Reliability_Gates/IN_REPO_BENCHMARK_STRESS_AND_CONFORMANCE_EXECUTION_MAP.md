# In-Repo Benchmark, Stress, and Conformance Execution Map

Status: current_authority

## Purpose

This file defines the concrete in-repo evidence lanes that complement the external `ScratchBird-Benchmarks` project:
- repo-local benchmark and calibration files
- stress and soak files
- conformance and gate scripts
- public-beta orchestration surfaces

## Benchmark lane map

### `tests/benchmark/benchmark_suite.cpp`

Role:
- generic timed GoogleTest benchmark suite

Recovered behavior:
- uses a warmup phase before timed iterations
- reports:
  - benchmark name
  - iteration count
  - total time
  - average time
  - operations per second
  - optional row rate
- covers at least:
  - sequential scan
  - filtered scan
  - projection scan
  - aggregate or join style workloads declared by the suite header

### `tests/benchmark/test_cache_buffer_benchmark.cpp`

Role:
- buffer-pool scan-resistance benchmark

Recovered behavior:
- creates a database using the canonical bootstrap page size `16384`
- allocates `512` pages
- reserves a hot set of `16` pages
- compares:
  - `BufferPool::AccessStrategy::Normal`
  - `BufferPool::AccessStrategy::Sequential`
- reports:
  - `hits`
  - `misses`
  - `hit_ratio`
  - `scan_ms`
  - `hot_ms`

### `tests/benchmark/test_sblr_jit_performance.cpp`

Role:
- JIT envelope benchmark

Recovered behavior:
- statement: `SELECT 100 + 23`
- captures `20` VM samples and `20` JIT-preferred samples
- reports:
  - `vm_p95_us`
  - `jit_p95_us`
- enforces:
  - `jit_p95_us <= vm_p95_us * 3 + 1`

### `tests/benchmark/test_auth_plugin_enterprise_perf.cpp`

Role:
- enterprise auth-provider latency and memory benchmark

Recovered behavior:
- builds a phase-1-equivalent secure baseline
- benchmarks enterprise methods against that baseline
- recovered methods in the current file include:
  - LDAP
  - Kerberos
  - Ident
  - Radius
  - PAM
- reports p50 and p95 latency classes and RSS delta
- uses `10000` iterations in the recovered benchmark body

This file is a benchmark and regression-envelope lane, not a release-correctness truth lane.

### `tests/benchmark/test_btree_proof_corpus.cpp`

Role:
- ordered-index benchmark corpus

Recovered behavior:
- creates a fresh B-tree per scenario
- measures insert and search time
- emits `BTREE_PROOF_SCENARIO` rows containing:
  - label
  - entries
  - searches
  - `insert_avg_us`
  - `search_avg_us`
  - `total_results`

### `tests/benchmark/test_front_door_mode_benchmark.cpp`

Role:
- front-door overhead benchmark

Recovered behavior:
- drives actual socket and native wire-protocol flows
- reserves ephemeral ports
- may spawn the manager binary for front-door mode
- compares connect or auth or query overhead through the front-door path

### `tests/benchmark/test_optimizer_cost_calibration.cpp`

Role:
- optimizer cost-model calibration benchmark

Recovered behavior:
- uses a fixed-seed calibration corpus
- compares profile-driven cost behavior across workload profiles
- may write CSV evidence when:
  - `SB_OPTIMIZER_COST_BENCHMARK_CSV` is set

### `tests/benchmark/test_parser_v3_benchmark.cpp`

Role:
- parser `v3` compile-loop benchmark

Recovered behavior:
- creates a temporary database and `QueryCompilerV3`
- benchmarks:
  - simple selects
  - arithmetic or logical expressions
  - string expressions
  - DDL statements
  - transaction statements
- reports average microseconds per iteration

## Stress and soak lane map

### `tests/stress/test_auth_plugin_enterprise_soak.cpp`

Role:
- enterprise auth-provider stability soak

Recovered behavior:
- simulates `4` hours via `14400` iterations
- injects random disconnects and provider-fault paths
- tracks:
  - disconnect events
  - reconnect events
  - success count
  - deny count
  - unexpected count
  - RSS delta

### `tests/stress/test_operational_reliability_soak.cpp`

Role:
- operational reliability and supportability soak

Recovered behavior:
- exercises:
  - catalog-backed cluster objects
  - workload governance
  - admission policy
  - support bundle generation
  - readiness state surfaces

### `tests/stress/test_columnstore_batch_performance.cpp`

Role:
- stress-supporting correctness lane

Recovered behavior:
- validates:
  - RLE compression or decompression round-trip
  - predicate evaluation count
  - batch scan iterator behavior

This file is not authoritative throughput certification by itself.

## Conformance lane map

### `tests/conformance/protocol/`

Role:
- deterministic frame-shape conformance

Recovered current sublanes:
- native `sbwp`
- PostgreSQL emulation
- MySQL emulation
- Firebird emulation
- negative protocol matrix

Recovered artifacts:
- `GOLDEN_TRACE_INDEX.csv`
- per-protocol frame conformance tests
- protocol README and fixture directories

### `tests/conformance/transactions/`

Role:
- transaction-truth conformance

Recovered artifacts:
- `TRANSACTION_TRUTH_MATRIX.md`
- `test_transaction_truth_native.cpp`
- `run_transaction_truth_matrix.sh`
- evidence targets:
  - `transactions/TRANSACTION_TRUTH_MATRIX.md`
  - `transactions/native_truth_results.txt`
  - `transactions/A55_transaction_truth_run.txt`

### `tests/conformance/v3_native_inet/`

Role:
- validates the canonical `v3` parser through the native listener TCP path

Recovered current semantics:
- positive scripts emit deterministic `ASSERT|...` rows
- negative scripts must fail with expected parser-error substrings
- registered `ctest` name:
  - `ConformanceV3NativeParserInet`

### `tests/conformance/v3_native_comparative_regression/`

Role:
- donor-derived comparative regression for native `v3`

Recovered current semantics:
- runs a frozen donor-derived corpus against:
  - original Firebird
  - original MySQL
  - original PostgreSQL
  - ScratchBird native `v3`
- translated `v3` cases are static on disk
- registered `ctest` name:
  - `ConformanceV3NativeComparativeRegression`

### `tests/conformance/public_beta/run_required_public_beta_gate.sh`

Role:
- shell-driven required public-beta gate orchestrator

Recovered current category names:
- `wire_protocol`
- `transaction_semantics`
- `security_enforcement`
- `end_to_end_sql`
- `modal_nosql`
- `cluster_infra`

Recovered currently named executed steps in the observed portion include:
- compatibility scripts for:
  - PostgreSQL
  - MySQL
  - Firebird
  - ScratchBird native
- protocol frame conformance `ctest` runs
- transaction truth matrix and native transaction truth runs
- MGA durability and failpoint `ctest` runs
- memory-model and scan-resistance `ctest` runs
- native `v3` inet conformance script run

This gate is stronger than a plain aggregate `ctest` pass because it stitches together shell-driven compatibility and conformance entrypoints with dedicated `ctest` cases.

## Full-cycle classification rule

A full clean, build, test, and benchmark cycle must classify in-repo outputs into at least:
- aggregate correctness
- dedicated conformance
- dedicated compatibility
- repo-local benchmark or calibration
- repo-local soak or stress
- external benchmark-project matrix

No automation or report is allowed to collapse all of these into a single undifferentiated success claim.

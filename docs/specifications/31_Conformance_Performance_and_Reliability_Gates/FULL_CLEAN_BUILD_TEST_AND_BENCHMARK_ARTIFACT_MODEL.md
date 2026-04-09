# Full Clean Build Test and Benchmark Artifact Model

Status: current_authority_with_reconstructed_expansion

## 1. Scope

This file defines what a full engineering validation cycle actually means in the current codebase.

A limited implementer must not assume that one command already performs every build, test, compatibility, and benchmark lane.

Current Beta 1 validation is split across:

- repo-local build and CTest execution in `ScratchBird`
- the required public-beta gate in `ScratchBird/tests/conformance/public_beta`
- standalone compliance shell contracts in `ScratchBird/tests/compliance`
- optional compatibility lanes under `ScratchBird/tests/compatibility`
- external benchmark matrix execution in `ScratchBird-Benchmarks`

## 2. Repo-local build and test cycle

### 2.1 Build entrypoint

The repo-local test runner expects a configured build tree at:

- `ScratchBird/build`

It builds test binaries through:

- `cmake --build build --target scratchbird_test_binaries`

The shell entrypoint is:

- `ScratchBird/tests/run_tests.sh`

For Beta 1 release-readiness, the maintained end-to-end orchestrator is:

- `ScratchBird/scripts/run_full_build_test_with_metrics.sh`

### 2.2 Repo-local run modes and concrete selector behavior

`ScratchBird/tests/run_tests.sh` hard-codes these selector modes and `ctest`
invocations:

| Mode | Concrete selector behavior |
| --- | --- |
| `smoke` | `ctest -L smoke --output-on-failure` |
| `portable` | `ctest -L "smoke|unit|integration" -E "quarantine" -LE "linux_only|disabled" --output-on-failure --timeout 60` |
| `windows_portable` | `ctest -L "smoke|unit|integration" -E "quarantine" -LE "linux_only|disabled|stress|performance|tsan" --output-on-failure --timeout 60` |
| `linux_only` | `ctest -L linux_only --output-on-failure --timeout 60` |
| `unit` | `ctest -L unit --output-on-failure --timeout 10` |
| `integration` | `ctest -L integration --output-on-failure --timeout 60` |
| `stress` | `ctest -L stress --output-on-failure --timeout 600` |
| `performance` | `ctest -L performance --output-on-failure --timeout 300` |
| `quarantine` | `ctest -L quarantine --output-on-failure --timeout 60 || true` |
| `quick` | `ctest -L "smoke|unit" --output-on-failure --timeout 10` |
| `ci` | `ctest -L "smoke|unit|integration" -E "quarantine" --output-on-failure --timeout 60` |
| `all` | `ctest -E "quarantine" --output-on-failure --timeout 300` |

### 2.3 Build-precondition and timeout rules

- the runner refuses to execute if `ScratchBird/build` does not exist
- the runner always builds `scratchbird_test_binaries` before dispatching
  `ctest`
- `quarantine` is deliberately non-gating and tolerated with `|| true`
- `stress` and `performance` are opt-in heavier lanes, not implied by `ci` or
  `quick`
- `portable` and `windows_portable` are label-plus-exclusion lanes, not simple
  directory aliases

### 2.4 Output model for repo-local runs

Repo-local runs currently emit:

- built test binaries in the build tree
- `ctest` stdout/stderr
- any per-test artifacts written by the invoked tests

The repo-local runner does not automatically emit benchmark matrix artifacts.

It also does not automatically execute standalone compliance shell contracts
unless those are invoked separately.

## 3. Repo-local test registration boundary

The aggregate repo-local suite is not equal to all files resident under `ScratchBird/tests`.

The active registration surface is defined by:

- `ScratchBird/tests/CMakeLists.txt`

That file builds:

- aggregate GoogleTest binaries
- dedicated test binaries
- sequential test lane
- protocol and transaction conformance targets
- optional external-client targets
- stress, soak, fuzz, and safety targets

It also excludes a large number of resident files from the aggregate binary for reasons such as:

- standalone `main()` structure
- API drift
- dedicated-target ownership
- platform-only behavior
- quarantine or manual lane ownership

This means a full file-tree inventory and an aggregate-binary inventory are not
the same artifact claim.

Current recovered top-level file volume under `ScratchBird/tests` makes that
distinction explicit:

- `compatibility`: `56891` files
- `conformance`: `3465` files
- `unit`: `454` files
- `integration`: `75` files
- `stress`: `10` files
- `v3`: `10` files
- `benchmark`: `8` files
- `sql`: `7` files
- `mocks`: `5` files
- `sequential`: `3` files
- `tsan`: `3` files
- `deprecated`: `2` files
- `helgrind`: `2` files
- `standalone`: `2` files

There are also top-level loose harness files such as:

- `CMakeLists.txt`
- `run_tests.sh`
- `README.md`
- several root-level legacy test sources under `tests/`

## 3.1 Current in-repo benchmark-family examples

The current repo-local benchmark family includes at least:

- `benchmark_suite.cpp`
- `test_auth_plugin_enterprise_perf.cpp`
- `test_btree_proof_corpus.cpp`
- `test_cache_buffer_benchmark.cpp`
- `test_front_door_mode_benchmark.cpp`
- `test_optimizer_cost_calibration.cpp`
- `test_parser_v3_benchmark.cpp`
- `test_sblr_jit_performance.cpp`

These belong to the repository-local evidence set and must not be conflated
with the external benchmark repository.

## 3.2 Current standalone compliance shell example

The current repo-local compliance shell lane includes at least:

- `tests/compliance/test_vnext_scope_scan_contract.sh`

That lane validates a tool contract by:

- creating a temporary git repository
- executing the scope scanner
- checking success and failure exit codes
- checking allowlist behavior
- checking report contents

## 4. Compatibility lane is separate

Compatibility testing is a separate lane under:

- `ScratchBird/tests/compatibility`

It has its own:

- vendored upstream snapshots
- conversion scripts
- curated, expanded, and full ctest list generation
- runtime estimates
- separate result trees

A clean repo-local `ctest` pass does not mean the compatibility lane was exercised unless those compatibility tests were explicitly selected.

## 5. External benchmark cycle is separate

The external benchmark cycle is owned by:

- `ScratchBird-Benchmarks`

The authoritative matrix entrypoint is:

- `scripts/run-benchmark-matrix.sh`

The authoritative single-engine suite entrypoint is:

- `scripts/run-benchmark.sh`

The benchmark repo also exposes a broader umbrella orchestrator:

- `ScratchBird-Benchmarks/run-all-tests.sh`

These benchmark passes emit:

- system-info captures
- raw suite JSON
- per-suite reports when enabled
- matrix summary and consolidated CSV when running the matrix
- pairwise index-comparison artifacts when applicable

The benchmark cycle is not automatically invoked by `ScratchBird/tests/run_tests.sh`.

The Beta 1 release orchestrator can invoke it through:

- `ScratchBird/scripts/run_full_build_test_with_metrics.sh --run-benchmarks`

## 6. Clean validation stages

A full current engineering validation program should be described as these stages, not as one implicit command:

1. clean configure/generate build tree
2. build `scratchbird_test_binaries`
3. run repo-local CTest lane(s)
4. run the required public-beta gate
5. run standalone compliance shell lane(s) when required
6. run compatibility lane(s) when required
7. run external benchmark lane(s) when required
8. interpret artifacts by lane, not as one merged result set

For the Beta 1 release lane, the benchmark engine set is expected to include:

- `firebird`
- `mysql`
- `postgresql`
- `scratchbird`

## 7. Artifact families by stage

### 7.1 Build stage artifacts

- generated build tree
- compiled libraries and executables
- compiled test binaries

### 7.2 Repo-local CTest artifacts

- `ctest` result stream
- test-binary output
- dedicated test-generated files where applicable

### 7.3 Standalone compliance artifacts

- shell exit status
- tool report text
- temporary fixture trees created by the compliance contract
- any allowlist or changed-file report emitted by the exercised tool

### 7.4 Required public-beta artifacts

- gate status ledger
- per-category and per-step logs
- preserved compatibility lane result trees referenced by the gate

### 7.5 Compatibility artifacts

- `RUN_MANIFEST.json`
- `PARSER_BOUNDARY.txt`
- per-engine compatibility result trees
- curated/expanded/full ctest lists
- compatibility runtime-estimate summary

### 7.6 Benchmark artifacts

- `system-info.json`
- suite result JSON
- per-suite comparison text reports
- `matrix-summary.json`
- `.matrix-runs.tsv`
- `matrix-comparison-unified.csv`
- index-comparison pairwise JSON/TXT outputs

## 8. Required interpretation rule

A clean repo-local build and `ctest` pass is not proof that:

- the required public-beta gate passed
- compatibility lanes passed
- standalone compliance shell contracts passed
- benchmark suites ran
- matrix-comparison artifacts exist
- upstream regression baselines were refreshed

Those require explicit execution of their own lanes.

## 9. Non-authority and rejection rules

The following claims are incorrect:

- `tests/run_tests.sh all` implies compatibility coverage
- `tests/run_tests.sh all` implies standalone compliance coverage
- `tests/run_tests.sh all` implies benchmark matrix generation
- every file under `ScratchBird/tests` is part of the aggregate GoogleTest binary
- repo-local CTest output and external benchmark artifacts are one merged artifact family

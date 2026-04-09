# Full Clean, Build, Test, and Benchmark Sequence and Artifact Emission Model

## Scope

This file defines the canonical operator model for a full local cycle that
includes:

- clean
- configure
- build
- `ctest`-driven registered test execution
- dedicated compliance or specialized test lanes
- in-repo benchmark lanes
- external `ScratchBird-Benchmarks` matrix execution

This file is authoritative for the sequence and artifact-emission model. It
does not require that every lane always run in one command.

## Governing split

The full cycle is a composition of related but distinct runners:

1. repo-local configure and build
2. repo-local registered test execution
3. specialized repo-local shell-driven or compatibility lanes
4. repo-local benchmark and stress lanes
5. external benchmark-matrix execution in `ScratchBird-Benchmarks`

Canonical rule:

- these lanes may be orchestrated together
- but their artifacts remain class-distinct and must not be conflated

## Clean and build phase

The canonical clean/build phase includes:

1. clean or recreate the build directory
2. configure with the selected profile
3. build engine and registered test targets

### Emitted build artifact classes

At minimum, the build phase emits or updates:

- configured build tree
- engine binaries or libraries
- aggregate test binary where the active CMake registration uses one
- benchmark binaries where enabled
- `CTestTestfile.cmake` registration metadata
- build logs and generator metadata

## Registered test execution phase

The canonical registered-test phase is the `ctest`-driven lane over the
currently registered test set.

### Important boundary

The aggregate `ctest` lane does not imply that every source file under
`ScratchBird/tests/` executes automatically.

Canonical rule:

- only registered test targets and scripts are in the aggregate lane
- dedicated shell scripts, compatibility lanes, or specialty runners may remain
  outside ordinary aggregate `ctest`

### Emitted registered-test artifacts

At minimum, the registered-test lane emits or updates:

- `ctest` result summary
- per-test pass/fail timing data
- label and test-name mapping
- failure logs or console excerpts
- any structured result files emitted by registered tests

## Specialized test lanes

The full cycle may additionally include dedicated lanes such as:

- compatibility
- compliance
- public-beta gate
- protocol and dialect-specific validation
- security-focused validation

These lanes are part of the overall validation picture but remain lane-distinct
from the aggregate `ctest` run.

### Emitted specialized-test artifact classes

At minimum, these lanes may emit:

- dedicated results directories
- manifest or marker files
- compatibility result rows
- protocol transcripts
- shell-run summaries
- preserved failure logs

## In-repo benchmark and stress phase

The full cycle may include in-repo performance lanes such as:

- microbenchmarks
- parser benchmarks
- optimizer calibration lanes
- JIT latency lanes
- index benchmark corpora
- stress and maintenance evidence runs

These are distinct from the external benchmark matrix.

### Emitted in-repo performance artifact classes

At minimum, these lanes may emit:

- benchmark result rows
- CSV summaries
- calibration baselines
- machine-capture metadata
- timing distributions
- stress evidence artifacts

## External benchmark matrix phase

The `ScratchBird-Benchmarks` project is the external matrix harness.

It is responsible for:

- engine-matrix execution
- machine baseline capture
- consolidated benchmark artifacts
- suite-scoped benchmark result trees

Canonical rule:

- external benchmark-matrix artifacts are not the same as repo-local test
  artifacts
- they must remain distinguishable by root, suite, and engine

## Sequence model

The canonical full sequence is:

1. clean previous build outputs
2. configure build tree
3. build engine and registered test targets
4. execute registered `ctest` lane
5. execute dedicated compliance or compatibility lanes as required
6. execute in-repo benchmark or stress lanes as required
7. execute external benchmark matrix as required
8. consolidate evidence without collapsing lane identity

## Artifact identity rule

Every emitted artifact family must remain attributable to:

- lane class
- runner or orchestrator
- suite or sub-suite
- machine or environment capture, where performance is involved
- run identity or timestamp root

## Fail-closed rules

The full-cycle lane shall not:

1. report a full local cycle while skipping required registered-test execution
   without declaring that omission
2. report repo-local benchmark success as though the external benchmark matrix
   also ran
3. collapse specialized shell-driven lanes into aggregate `ctest` results
   without preserving their distinct artifact roots
4. claim that every file under `ScratchBird/tests/` executed merely because the
   aggregate runner passed

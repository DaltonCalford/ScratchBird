#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
RUN_ID="${RUN_ID:-$(date -u +%Y%m%dT%H%M%SZ)}"
FULL_GATE_DIR="${REPO_ROOT}/tests/results/full_gate/${RUN_ID}"
NORMALIZER="${REPO_ROOT}/scripts/full_run_metrics/normalize_full_run.py"
BENCHMARK_PROJECT_ROOT="${REPO_ROOT}/../ScratchBird-Benchmarks"
BENCHMARKS_ROOT="${BENCHMARK_PROJECT_ROOT}/results"
BENCHMARKS_ROOT_EXPLICIT=0
BENCHMARK_MATRIX_OUTPUT=""
BENCHMARK_ENGINES="${BENCHMARK_ENGINES:-firebird,mysql,postgresql}"
BENCHMARK_SUITES="${BENCHMARK_SUITES:-stress,acid,engine-differential,index-comparison}"
BENCHMARK_STRESS_SCALE="${BENCHMARK_STRESS_SCALE:-small}"

RUN_CONFIGURE=1
RUN_BUILD=1
RUN_CTEST=1
RUN_PUBLIC_BETA=0
RUN_BENCHMARKS=0
HISTORY_LIMIT="${SCRATCHBIRD_FULL_RUN_HISTORY_LIMIT:-5}"

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --run-id <id>             Override run id (default: current UTC timestamp)
  --build-dir <path>        Build directory (default: <repo>/build)
  --skip-configure          Reuse existing build tree
  --skip-build              Skip build step
  --skip-ctest              Skip ctest step
  --run-public-beta         Execute required public beta gate before normalization
  --run-benchmarks          Execute ScratchBird-Benchmarks matrix before normalization
  --benchmarks-root <path>  ScratchBird-Benchmarks results root or matrix run dir
  --benchmark-project-root <path>
                            ScratchBird-Benchmarks repo root
  --benchmark-output <path> Benchmark matrix output directory
  --benchmark-engines <csv> Benchmark matrix engines (default: ${BENCHMARK_ENGINES})
  --benchmark-suites <csv>  Benchmark matrix suites (default: ${BENCHMARK_SUITES})
  --benchmark-stress-scale <scale>
                            Benchmark stress scale env (default: ${BENCHMARK_STRESS_SCALE})
  --history-limit <n>       Number of prior normalized runs to compare (default: 5)
  -h, --help                Show help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --run-id)
      RUN_ID="$2"
      FULL_GATE_DIR="${REPO_ROOT}/tests/results/full_gate/${RUN_ID}"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --skip-configure)
      RUN_CONFIGURE=0
      shift
      ;;
    --skip-build)
      RUN_BUILD=0
      shift
      ;;
    --skip-ctest)
      RUN_CTEST=0
      shift
      ;;
    --run-public-beta)
      RUN_PUBLIC_BETA=1
      shift
      ;;
    --run-benchmarks)
      RUN_BENCHMARKS=1
      shift
      ;;
    --benchmarks-root)
      BENCHMARKS_ROOT="$2"
      BENCHMARKS_ROOT_EXPLICIT=1
      shift 2
      ;;
    --benchmark-project-root)
      BENCHMARK_PROJECT_ROOT="$2"
      shift 2
      ;;
    --benchmark-output)
      BENCHMARK_MATRIX_OUTPUT="$2"
      shift 2
      ;;
    --benchmark-engines)
      BENCHMARK_ENGINES="$2"
      shift 2
      ;;
    --benchmark-suites)
      BENCHMARK_SUITES="$2"
      shift 2
      ;;
    --benchmark-stress-scale)
      BENCHMARK_STRESS_SCALE="$2"
      shift 2
      ;;
    --history-limit)
      HISTORY_LIMIT="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ "${BENCHMARKS_ROOT_EXPLICIT}" != "1" ]]; then
  BENCHMARKS_ROOT="${BENCHMARK_PROJECT_ROOT}/results"
fi

mkdir -p "${FULL_GATE_DIR}"

CONFIGURE_LOG="${FULL_GATE_DIR}/configure.log"
BUILD_LOG="${FULL_GATE_DIR}/build.log"
CTEST_LOG="${FULL_GATE_DIR}/ctest.log"
BENCHMARK_LOG="${FULL_GATE_DIR}/benchmark.log"

configure_exit=""
build_exit=""
ctest_exit=""
benchmark_exit=""

if [[ "${RUN_CONFIGURE}" == "1" ]]; then
  set +e
  cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" >"${CONFIGURE_LOG}" 2>&1
  configure_exit=$?
  set -e
  if [[ "${configure_exit}" -ne 0 ]]; then
    cat > "${FULL_GATE_DIR}/RUN_STATUS.txt" <<EOF
run_dir=tests/results/full_gate/${RUN_ID}
configure_log=tests/results/full_gate/${RUN_ID}/configure.log
build_log=tests/results/full_gate/${RUN_ID}/build.log
ctest_log=tests/results/full_gate/${RUN_ID}/ctest.log
configure_exit=${configure_exit}
EOF
    exit "${configure_exit}"
  fi
fi

if [[ "${RUN_BUILD}" == "1" ]]; then
  set +e
  cmake --build "${BUILD_DIR}" -j"$(nproc)" >"${BUILD_LOG}" 2>&1
  build_exit=$?
  set -e
  if [[ "${build_exit}" -ne 0 ]]; then
    cat > "${FULL_GATE_DIR}/RUN_STATUS.txt" <<EOF
run_dir=tests/results/full_gate/${RUN_ID}
configure_log=tests/results/full_gate/${RUN_ID}/configure.log
build_log=tests/results/full_gate/${RUN_ID}/build.log
ctest_log=tests/results/full_gate/${RUN_ID}/ctest.log
configure_exit=${configure_exit:-0}
build_exit=${build_exit}
EOF
    exit "${build_exit}"
  fi
fi

if [[ "${RUN_CTEST}" == "1" ]]; then
  set +e
  ctest --test-dir "${BUILD_DIR}" --output-on-failure >"${CTEST_LOG}" 2>&1
  ctest_exit=$?
  set -e
else
  ctest_exit=0
fi

cat > "${FULL_GATE_DIR}/RUN_STATUS.txt" <<EOF
run_dir=tests/results/full_gate/${RUN_ID}
configure_log=tests/results/full_gate/${RUN_ID}/configure.log
build_log=tests/results/full_gate/${RUN_ID}/build.log
ctest_log=tests/results/full_gate/${RUN_ID}/ctest.log
configure_exit=${configure_exit:-0}
build_exit=${build_exit:-0}
ctest_exit=${ctest_exit:-0}
EOF

if [[ "${RUN_PUBLIC_BETA}" == "1" ]]; then
  "${REPO_ROOT}/tests/conformance/public_beta/run_required_public_beta_gate.sh"
fi

if [[ "${RUN_BENCHMARKS}" == "1" ]]; then
  if [[ -z "${BENCHMARK_MATRIX_OUTPUT}" ]]; then
    BENCHMARK_MATRIX_OUTPUT="${BENCHMARK_PROJECT_ROOT}/results/matrix-${RUN_ID}"
  fi
  set +e
  STRESS_SCALE="${BENCHMARK_STRESS_SCALE}" \
    "${BENCHMARK_PROJECT_ROOT}/scripts/run-benchmark-matrix.sh" \
    "--engines=${BENCHMARK_ENGINES}" \
    "--suites=${BENCHMARK_SUITES}" \
    "--output=${BENCHMARK_MATRIX_OUTPUT}" \
    --compare >"${BENCHMARK_LOG}" 2>&1
  benchmark_exit=$?
  set -e
  BENCHMARKS_ROOT="${BENCHMARK_MATRIX_OUTPUT}"
else
  benchmark_exit=0
fi

python3 "${NORMALIZER}" \
  --repo-root "${REPO_ROOT}" \
  --run-id "${RUN_ID}" \
  --full-gate-dir "${FULL_GATE_DIR}" \
  --benchmarks-root "${BENCHMARKS_ROOT}" \
  --history-limit "${HISTORY_LIMIT}"

cat > "${FULL_GATE_DIR}/RUN_STATUS.txt" <<EOF
run_dir=tests/results/full_gate/${RUN_ID}
configure_log=tests/results/full_gate/${RUN_ID}/configure.log
build_log=tests/results/full_gate/${RUN_ID}/build.log
ctest_log=tests/results/full_gate/${RUN_ID}/ctest.log
benchmark_log=tests/results/full_gate/${RUN_ID}/benchmark.log
configure_exit=${configure_exit:-0}
build_exit=${build_exit:-0}
ctest_exit=${ctest_exit:-0}
benchmark_exit=${benchmark_exit:-0}
EOF

if [[ "${ctest_exit:-0}" -ne 0 ]]; then
  exit "${ctest_exit}"
fi
exit "${benchmark_exit:-0}"

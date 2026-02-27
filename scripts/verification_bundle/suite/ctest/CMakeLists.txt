cmake_minimum_required(VERSION 3.16)
project(scratchbird_verification_ctest NONE)

enable_testing()

set(VERIFY_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

add_test(
  NAME footprint_metrics
  COMMAND python3 "${VERIFY_ROOT}/scripts/project_footprint.py"
          --config "${VERIFY_ROOT}/configs/projects.yaml"
          --out-dir "${VERIFY_ROOT}/reports"
)

add_test(
  NAME compat_diff
  COMMAND python3 "${VERIFY_ROOT}/scripts/differential_runner.py"
          --engines "${VERIFY_ROOT}/configs/engines.yaml"
          --cases "${VERIFY_ROOT}/cases/case_index.yaml"
          --out-dir "${VERIFY_ROOT}/results/diff"
)

add_test(
  NAME perf_smoke
  COMMAND python3 "${VERIFY_ROOT}/scripts/perf_runner.py"
          --engines "${VERIFY_ROOT}/configs/engines.yaml"
          --config "${VERIFY_ROOT}/configs/perf_workloads.yaml"
          --out-dir "${VERIFY_ROOT}/results/perf"
)


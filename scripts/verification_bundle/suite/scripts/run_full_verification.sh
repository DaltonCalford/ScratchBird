#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="${SB_VERIFY_REPO_ROOT:-${WORKSPACE_ROOT}/repos}"
PYTHON_BIN="${WORKSPACE_ROOT}/.venv/bin/python"

RUN_FOOTPRINT=1
RUN_DIFF=1
RUN_PERF=1
RUN_WIRE=1

usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Options:
  --workspace-root <path>   Verification workspace root
  --repo-root <path>        Clone root
  --python <path>           Python interpreter path
  --skip-footprint          Skip project footprint report
  --skip-diff               Skip differential runner
  --skip-perf               Skip perf runner
  --skip-wire               Skip wire capture parity script
  --help                    Show help
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --workspace-root)
      WORKSPACE_ROOT="$2"
      shift 2
      ;;
    --repo-root)
      REPO_ROOT="$2"
      shift 2
      ;;
    --python)
      PYTHON_BIN="$2"
      shift 2
      ;;
    --skip-footprint)
      RUN_FOOTPRINT=0
      shift
      ;;
    --skip-diff)
      RUN_DIFF=0
      shift
      ;;
    --skip-perf)
      RUN_PERF=0
      shift
      ;;
    --skip-wire)
      RUN_WIRE=0
      shift
      ;;
    --help|-h)
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

if [[ ! -x "${PYTHON_BIN}" ]]; then
  PYTHON_BIN="$(command -v python3)"
fi

mkdir -p "${WORKSPACE_ROOT}/reports" "${WORKSPACE_ROOT}/results"

RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
MANIFEST="${WORKSPACE_ROOT}/reports/verification_manifest_${RUN_ID}.json"
INDEX="${WORKSPACE_ROOT}/reports/verification_index_${RUN_ID}.md"

if [[ "${RUN_FOOTPRINT}" -eq 1 ]]; then
  "${PYTHON_BIN}" "${WORKSPACE_ROOT}/scripts/project_footprint.py" \
    --config "${WORKSPACE_ROOT}/configs/projects.yaml" \
    --out-dir "${WORKSPACE_ROOT}/reports" \
    --workspace-root "${WORKSPACE_ROOT}" \
    --repo-root "${REPO_ROOT}"
fi

if [[ "${RUN_DIFF}" -eq 1 ]]; then
  "${PYTHON_BIN}" "${WORKSPACE_ROOT}/scripts/differential_runner.py" \
    --engines "${WORKSPACE_ROOT}/configs/engines.yaml" \
    --cases "${WORKSPACE_ROOT}/cases/case_index.yaml" \
    --out-dir "${WORKSPACE_ROOT}/results/diff" \
    --workspace-root "${WORKSPACE_ROOT}" \
    --repo-root "${REPO_ROOT}"
fi

if [[ "${RUN_PERF}" -eq 1 ]]; then
  "${PYTHON_BIN}" "${WORKSPACE_ROOT}/scripts/perf_runner.py" \
    --engines "${WORKSPACE_ROOT}/configs/engines.yaml" \
    --config "${WORKSPACE_ROOT}/configs/perf_workloads.yaml" \
    --out-dir "${WORKSPACE_ROOT}/results/perf" \
    --workspace-root "${WORKSPACE_ROOT}" \
    --repo-root "${REPO_ROOT}"
fi

WIRE_RESULT="skipped"
if [[ "${RUN_WIRE}" -eq 1 ]]; then
  WIRE_SCRIPT="${REPO_ROOT}/ScratchBird/scripts/emulation/generate_wire_capture_parity.sh"
  if [[ -x "${WIRE_SCRIPT}" ]]; then
    SCRATCHBIRD_EMU_ARTIFACT_ROOT="${WORKSPACE_ROOT}/results/wire" \
      "${WIRE_SCRIPT}" auto
    WIRE_RESULT="executed"
  else
    WIRE_RESULT="not_available"
  fi
fi

SB_COMMIT=""
if [[ -d "${REPO_ROOT}/ScratchBird/.git" ]]; then
  SB_COMMIT="$(git -C "${REPO_ROOT}/ScratchBird" rev-parse HEAD)"
fi

cat > "${MANIFEST}" <<JSON
{
  "run_id": "${RUN_ID}",
  "workspace_root": "${WORKSPACE_ROOT}",
  "repo_root": "${REPO_ROOT}",
  "python": "${PYTHON_BIN}",
  "scratchbird_commit": "${SB_COMMIT}",
  "executed": {
    "footprint": ${RUN_FOOTPRINT},
    "differential": ${RUN_DIFF},
    "perf": ${RUN_PERF},
    "wire_capture": "${WIRE_RESULT}"
  }
}
JSON

cat > "${INDEX}" <<MD
# Verification Run ${RUN_ID}

- Workspace: `${WORKSPACE_ROOT}`
- Repo root: `${REPO_ROOT}`
- ScratchBird commit: `${SB_COMMIT}`
- Manifest: `${MANIFEST}`

## Outputs

- Footprint reports: `${WORKSPACE_ROOT}/reports/footprint_*`
- Differential results: `${WORKSPACE_ROOT}/results/diff`
- Perf results: `${WORKSPACE_ROOT}/results/perf`
- Wire capture results: `${WORKSPACE_ROOT}/results/wire` (`${WIRE_RESULT}`)
MD

echo "Verification run complete."
echo "Manifest: ${MANIFEST}"
echo "Index:    ${INDEX}"

#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_ID="${RUN_ID:-$(date -u +%Y%m%dT%H%M%SZ)}"
EVID_ROOT="${1:-/home/dcalford/CliWork/local_work/artifacts/audit_5of5_conformance/${RUN_ID}}"
RUN_LOG="${EVID_ROOT}/audit/A55_audit_bundle_run.txt"

mkdir -p "${EVID_ROOT}"/{gates,protocol,transactions,security,audit,ci}

echo "RUN_ID=${RUN_ID}" > "${EVID_ROOT}/audit/RUN_CONTEXT.txt"
echo "REPO_DIR=${REPO_DIR}" >> "${EVID_ROOT}/audit/RUN_CONTEXT.txt"
echo "EVID_ROOT=${EVID_ROOT}" >> "${EVID_ROOT}/audit/RUN_CONTEXT.txt"

# Coverage map generated from tracker status snapshot.
TRACKER="/home/dcalford/CliWork/local_work/docs/planning/AUDIT_5_OF_5_CONFORMANCE_TRACKER_2026-02-26.csv"
cp "${TRACKER}" "${EVID_ROOT}/audit/COVERAGE_MAP.csv"

# Spot checks for required key files.
{
  echo "SPOT_CHECK|protocol_ctest|$([[ -f "${EVID_ROOT}/protocol/A55_protocol_ctest.txt" ]] && echo PRESENT || echo MISSING)"
  echo "SPOT_CHECK|tx_run|$([[ -f "${EVID_ROOT}/transactions/A55_transaction_truth_run.txt" ]] && echo PRESENT || echo MISSING)"
  echo "SPOT_CHECK|sec_run|$([[ -f "${EVID_ROOT}/security/A55_security_parity_run.txt" ]] && echo PRESENT || echo MISSING)"
  echo "SPOT_CHECK|gate_decisions|$([[ -f "${EVID_ROOT}/gates/A55_GATE_DECISIONS.md" ]] && echo PRESENT || echo MISSING)"
} > "${EVID_ROOT}/audit/SPOT_CHECK_REPORT.txt"

# Parser boundary checks from latest lane manifests.
{
  echo "lane,manifest,status,parser_core,parser_mode"
  for lane in scratchbird postgresql mysql firebird; do
    manifest="$(ls -1dt "${REPO_DIR}/tests/compatibility/${lane}/results/ctest/"*/RUN_MANIFEST.json 2>/dev/null | head -n1 || true)"
    if [[ -z "${manifest}" ]]; then
      echo "${lane},,MISSING,,"
      continue
    fi
    parser_core="$(awk -F'"' '/"parser_core"/{print $4; exit}' "${manifest}")"
    parser_mode="$(awk -F'"' '/"parser_mode"/{print $4; exit}' "${manifest}")"
    echo "${lane},${manifest},PRESENT,${parser_core},${parser_mode}"
  done
} > "${EVID_ROOT}/audit/PARSER_BOUNDARY_CHECKS.csv"

# Evidence ledger with checksums.
{
  echo "path,sha256,size_bytes"
  while IFS= read -r f; do
    sha="$(sha256sum "${f}" | awk '{print $1}')"
    size="$(wc -c < "${f}" | tr -d ' ')"
    rel="${f#${EVID_ROOT}/}"
    echo "${rel},${sha},${size}"
  done < <(find "${EVID_ROOT}" -type f | sort)
} > "${EVID_ROOT}/audit/EVIDENCE_LEDGER.csv"

"${REPO_DIR}/scripts/score_5of5_claims_vs_proof.sh" "${EVID_ROOT}" "${EVID_ROOT}/audit/SCORES.json"

{
  echo "A55 audit bundle run"
  echo "timestamp_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "run_id=${RUN_ID}"
  echo "evidence_root=${EVID_ROOT}"
  echo "generated=RUN_CONTEXT.txt,COVERAGE_MAP.csv,SPOT_CHECK_REPORT.txt,PARSER_BOUNDARY_CHECKS.csv,EVIDENCE_LEDGER.csv,SCORES.json"
} > "${RUN_LOG}"

echo "AUDIT_BUNDLE_ROOT=${EVID_ROOT}"
echo "Generated:"
echo "- audit/COVERAGE_MAP.csv"
echo "- audit/EVIDENCE_LEDGER.csv"
echo "- audit/SPOT_CHECK_REPORT.txt"
echo "- audit/PARSER_BOUNDARY_CHECKS.csv"
echo "- audit/A55_audit_bundle_run.txt"
echo "- audit/SCORES.json"

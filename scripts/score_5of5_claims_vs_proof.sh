#!/usr/bin/env bash
set -euo pipefail

EVID_ROOT="${1:?usage: score_5of5_claims_vs_proof.sh <evidence_root>}"
OUT_JSON="${2:-${EVID_ROOT}/audit/SCORES.json}"
mkdir -p "$(dirname "${OUT_JSON}")"

score_wire=0
wire_reason="missing protocol evidence"
if [[ -f "${EVID_ROOT}/protocol/A55_protocol_ctest.txt" ]]; then
  if rg -q "0 tests failed out of 5" "${EVID_ROOT}/protocol/A55_protocol_ctest.txt"; then
    score_wire=5
    wire_reason="all protocol conformance tests passed"
  else
    score_wire=3
    wire_reason="protocol run exists but not fully passing"
  fi
fi

score_tx=0
tx_reason="missing transaction evidence"
if [[ -f "${EVID_ROOT}/transactions/native_truth_results.txt" ]]; then
  native_ok=0
  emu_na=0
  lane_pass=0
  if rg -q "LANE_SUMMARY\|native\|6\|0\|0" "${EVID_ROOT}/transactions/native_truth_results.txt"; then
    native_ok=1
  fi
  if [[ -f "${EVID_ROOT}/transactions/A55_transaction_truth_run.txt" ]]; then
    if rg -q "LANE_STATUS\|postgresql\|PASS" "${EVID_ROOT}/transactions/A55_transaction_truth_run.txt" \
      && rg -q "LANE_STATUS\|mysql\|PASS" "${EVID_ROOT}/transactions/A55_transaction_truth_run.txt" \
      && rg -q "LANE_STATUS\|firebird\|PASS" "${EVID_ROOT}/transactions/A55_transaction_truth_run.txt" \
      && ! rg -q "LANE_STATUS\|.*\|FAIL" "${EVID_ROOT}/transactions/A55_transaction_truth_run.txt"; then
      lane_pass=1
    fi
    if rg -q "LANE_STATUS\|postgresql\|NA|LANE_STATUS\|mysql\|NA|LANE_STATUS\|firebird\|NA" "${EVID_ROOT}/transactions/A55_transaction_truth_run.txt"; then
      emu_na=1
    fi
  fi
  if [[ ${native_ok} -eq 1 && ${lane_pass} -eq 1 ]]; then
    score_tx=5
    tx_reason="native + emulation lanes fully executed"
  elif [[ ${native_ok} -eq 1 ]]; then
    score_tx=3
    tx_reason="native lane proven; emulation lanes NA"
  else
    score_tx=2
    tx_reason="partial transaction proof only"
  fi
fi

score_sec=0
sec_reason="missing security evidence"
if [[ -f "${EVID_ROOT}/security/A55_security_parity_run.txt" ]]; then
  native_ok=0
  emu_na=0
  lane_pass=0
  sec009_ok=0
  if rg -q "SEC_LANE_SUMMARY\|native\|9\|0\|0" "${EVID_ROOT}/security/A55_security_parity_run.txt"; then
    native_ok=1
  fi
  if rg -q "SEC_RESULT\|SEC-009\|PASS\|native_audit_visibility" "${EVID_ROOT}/security/domain_parity_results.txt"; then
    sec009_ok=1
  fi
  if rg -q "SEC_LANE_STATUS\|postgresql\|PASS" "${EVID_ROOT}/security/A55_security_parity_run.txt" \
    && rg -q "SEC_LANE_STATUS\|mysql\|PASS" "${EVID_ROOT}/security/A55_security_parity_run.txt" \
    && rg -q "SEC_LANE_STATUS\|firebird\|PASS" "${EVID_ROOT}/security/A55_security_parity_run.txt" \
    && ! rg -q "SEC_LANE_STATUS\|.*\|FAIL" "${EVID_ROOT}/security/A55_security_parity_run.txt"; then
    lane_pass=1
  fi
  if rg -q "SEC_LANE_STATUS\|postgresql\|NA|SEC_LANE_STATUS\|mysql\|NA|SEC_LANE_STATUS\|firebird\|NA" "${EVID_ROOT}/security/A55_security_parity_run.txt"; then
    emu_na=1
  fi
  if [[ ${native_ok} -eq 1 && ${lane_pass} -eq 1 && ${sec009_ok} -eq 1 ]]; then
    score_sec=5
    sec_reason="native + emulation executed with audit visibility proof"
  elif [[ ${native_ok} -eq 1 ]]; then
    score_sec=3
    sec_reason="native security proof present; emulation lanes NA"
  else
    score_sec=2
    sec_reason="security proof incomplete"
  fi
fi

score_parser=0
parser_reason="missing parser-boundary evidence"
if [[ -f "${EVID_ROOT}/protocol/GOLDEN_TRACE_INDEX.csv" ]] && [[ -f "${EVID_ROOT}/audit/PARSER_BOUNDARY_CHECKS.csv" ]]; then
  if rg -q "scratchbird,.*PRESENT,v3,native_core" "${EVID_ROOT}/audit/PARSER_BOUNDARY_CHECKS.csv" \
    && rg -q "postgresql,.*PRESENT,v3,emulation_surface_only" "${EVID_ROOT}/audit/PARSER_BOUNDARY_CHECKS.csv" \
    && rg -q "mysql,.*PRESENT,v3,emulation_surface_only" "${EVID_ROOT}/audit/PARSER_BOUNDARY_CHECKS.csv" \
    && rg -q "firebird,.*PRESENT,v3,emulation_surface_only" "${EVID_ROOT}/audit/PARSER_BOUNDARY_CHECKS.csv"; then
    score_parser=5
    parser_reason="v3 parser boundary manifests proven for native and all emulation lanes"
  else
    score_parser=3
    parser_reason="partial parser boundary evidence present"
  fi
fi

score_e2e=0
e2e_reason="missing end-to-end evidence"
if [[ -f "${EVID_ROOT}/ci/A55_full_ctest.txt" ]] && rg -q "100% tests passed" "${EVID_ROOT}/ci/A55_full_ctest.txt"; then
  score_e2e=5
  e2e_reason="full ctest conformance bundle passed"
elif [[ -f "${EVID_ROOT}/gates/A55-000_baseline_ctest.txt" ]]; then
  if rg -q "100% tests passed" "${EVID_ROOT}/gates/A55-000_baseline_ctest.txt"; then
    score_e2e=4
    e2e_reason="baseline compatibility/security subset passed"
  else
    score_e2e=2
    e2e_reason="baseline run exists but not all passing"
  fi
fi

overall="INCOMPLETE"
if [[ ${score_wire} -eq 5 && ${score_tx} -ge 5 && ${score_sec} -ge 5 && ${score_parser} -ge 5 && ${score_e2e} -ge 5 ]]; then
  overall="PASS_5_OF_5"
fi

cat > "${OUT_JSON}" <<JSON
{
  "wire_protocol_conformance": {"score": ${score_wire}, "reason": "${wire_reason}"},
  "transaction_semantics_conformance": {"score": ${score_tx}, "reason": "${tx_reason}"},
  "security_runtime_enforcement_conformance": {"score": ${score_sec}, "reason": "${sec_reason}"},
  "parser_boundary_conformance": {"score": ${score_parser}, "reason": "${parser_reason}"},
  "end_to_end_sql_correctness_conformance": {"score": ${score_e2e}, "reason": "${e2e_reason}"},
  "overall": "${overall}"
}
JSON

echo "Wrote ${OUT_JSON}"

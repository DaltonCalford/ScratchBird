# Test Results - HCN-002

- Status: pass
- Date: 2026-02-24

## Checks
- [x] `GAP_TO_TICKET_TRACE.csv` rows map to valid `HCN-*` ticket IDs.
- [x] `ACCEPTANCE_MAPPING.csv` gate IDs exist in `GATE_EVIDENCE_MATRIX.csv`.
- [x] P0 and P1 assignments are present for all mapped gaps.
- [x] Bundle checksums regenerated.

## Validation Commands
- `awk -F, 'NR>1 {print $4}' GAP_TO_TICKET_TRACE.csv | tr '|' '\n' | sort -u`
- `awk -F, 'NR>1 {print $5}' ACCEPTANCE_MAPPING.csv | sort -u`
- `find . -maxdepth 1 -type f ! -name 'CHECKSUMS.sha256' -print0 | sort -z | xargs -0 sha256sum`

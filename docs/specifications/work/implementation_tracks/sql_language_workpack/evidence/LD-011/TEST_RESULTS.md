# Test Results

- ticket_id: LD-011
- status: PASS
- summary: SQL-to-SBLR request envelope and diagnostics trace audits completed
- deterministic_failures: 0
- envelope_rows: 30
- envelope_accept_rows: 18
- envelope_reject_rows: 12
- diagnostic_trace_rows: 25
- diagnostic_fail_rows: 24

## Validation Commands
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-011/SQL_SBLR_ENVELOPE_AUDIT.csv | wc -l
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-011/SQL_SBLR_ENVELOPE_AUDIT.csv | rg ',ACCEPT,' | wc -l
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-011/SQL_SBLR_ENVELOPE_AUDIT.csv | rg ',REJECT,' | wc -l
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-011/DIAGNOSTIC_TRACE_AUDIT.csv | wc -l
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-011/DIAGNOSTIC_TRACE_AUDIT.csv | rg ',false,' | wc -l

## Pass Criteria Evaluation
- SQL family to opcode/payload/result-shape mapping is deterministic: PASS
- Normalization evidence fields (rule set, clause bits, clause-order, alias flags) are covered: PASS
- Verifier failure code and trace-field contract is covered: PASS
- Deterministic replay behavior is represented in diagnostic trace audit: PASS

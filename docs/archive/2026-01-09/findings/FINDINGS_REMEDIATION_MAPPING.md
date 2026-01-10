# FINDINGS_REMEDIATION_MAPPING.md

## Purpose
This document maps existing audit and findings reports to consolidated remediation plans and task IDs.

## Mapping

| Finding | Category | Plan | Task |
|-------|--------|------|------|
| DEADLOCK_FIX_2025_12_30.md | Concurrency | PLAN_19 | GC-B4 |
| FK_DEADLOCK_FIX_2025_12_31.md | Concurrency | PLAN_19 | GC-B2 |
| EXECUTOR_TRANSACTION_TIMEOUT_ANALYSIS_2025_12_30.md | Executor | PLAN_18 | OPT-A1 |
| TEST_TIMEOUT_ANALYSIS_2025_12_29.md | Infra | PLAN_18 / 19 | OPT-A8 / GC-B6 |
| DROPTABLE_DEADLOCK_ANALYSIS_2025_12_30.md | DDL | PLAN_19 | GC-B2 |
| FINAL_TEST_RESULTS_2025_12_31.md | Validation | PLAN_18 / 19 | All |
| SESSION_SUMMARY_2025_12_31.md | Summary | PLAN_18 / 19 | Tracking |

## Notes
- No findings remain unmapped.
- Security-sensitive findings always map to PLAN_18.
- Storage health findings always map to PLAN_19.

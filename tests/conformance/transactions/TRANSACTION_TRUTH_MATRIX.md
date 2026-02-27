# Transaction Truth Matrix (A55-020)

Purpose:
Define deterministic transaction-semantics truth rows that must be proven across native and emulation lanes.

## Scope
- Native ScratchBird lane
- PostgreSQL emulation lane
- MySQL emulation lane
- Firebird emulation lane

## Determinism Rules
1. Each row has a fixed setup, action, and expected output contract.
2. Output comparison uses normalized line-based expected files.
3. Failures must emit lane-specific diff artifacts.
4. Unknown or unsupported row/lane combinations are marked `N/A` with explicit reason.

## Truth Rows
| Row ID | Category | Setup | Action | Expected Deterministic Outcome |
|---|---|---|---|---|
| TX-001 | Commit visibility | Session A inserts row in tx | Session A COMMIT; Session B SELECT | Row visible to Session B |
| TX-002 | Rollback invisibility | Session A inserts row in tx | Session A ROLLBACK; Session B SELECT | Row not visible to Session B |
| TX-003 | Savepoint rollback | Session A updates row twice with savepoint | ROLLBACK TO savepoint; COMMIT | Value equals first update only |
| TX-004 | Error rollback behavior | Session A performs valid DML then invalid DML | Statement error then commit/rollback path | Deterministic rollback or failed-tx semantics per lane contract |
| TX-005 | Isolation baseline | Session A holds uncommitted update | Session B reads row before/after commit | No dirty read before commit; updated value after commit |
| TX-006 | Lock conflict semantics | Session A holds row lock | Session B conflicting update with timeout | Deterministic lock/timeout error code family |

## Expected Output Contract
For each lane:
1. `ROW_RESULT` line format:
   - `ROW_RESULT|<row_id>|<PASS|FAIL|NA>|<normalized_result>`
2. `ROW_ASSERT` line format:
   - `ROW_ASSERT|<row_id>|<assertion_name>|<PASS|FAIL>|<details>`
3. A lane summary line:
   - `LANE_SUMMARY|<lane>|<pass_count>|<fail_count>|<na_count>`

## Required Artifacts (Phase 2)
1. Native lane executable test output.
2. Emulation lane SQL scripts and expected outputs.
3. Matrix runner aggregate report with per-row/per-lane pass/fail.
4. Stable diffs for any mismatch.

## Gate Link
- Gate: `A55-GATE-03`
- Upstream dependency: `A55-015`
- Downstream tickets: `A55-021`, `A55-022`, `A55-023`

# Security Parity Matrix (A55-030)

Purpose:
Define deterministic, lane-comparable security enforcement rows for native and emulation surfaces.

## Scope
- Native ScratchBird lane
- PostgreSQL emulation lane
- MySQL emulation lane
- Firebird emulation lane (where surface is supported)

## Determinism Rules
1. Each row has fixed setup principals, privileges, and expected result.
2. Outputs are normalized into `SEC_RESULT` lines.
3. Unsupported surfaces must emit `NA` with explicit reason.
4. Parity claims must match executable evidence, not inferred behavior.

## Security Rows
| Row ID | Category | Setup | Action | Expected Deterministic Outcome |
|---|---|---|---|---|
| SEC-001 | RLS allow path | policy allows subject on tenant row | SELECT protected rows | only allowed rows returned |
| SEC-002 | RLS deny path | policy denies subject | SELECT/UPDATE denied row | deny result with deterministic error/empty set contract |
| SEC-003 | Column grant allow | subject granted column read | SELECT allowed columns | allowed columns visible |
| SEC-004 | Column grant deny | subject denied column | SELECT denied column | access denied deterministic contract |
| SEC-005 | Domain masking privileged | privileged principal | SELECT masked domain column | clear value visible |
| SEC-006 | Domain masking unprivileged | unprivileged principal | SELECT masked domain column | masked/redacted value returned |
| SEC-007 | Domain encryption policy allow | principal with decrypt policy | SELECT encrypted domain column | decrypted or allowed clear representation |
| SEC-008 | Domain encryption policy deny | principal without decrypt policy | SELECT encrypted domain column | ciphertext/denied representation |
| SEC-009 | Security audit visibility | security action performed | query audit surface | expected audit event visible |

## Expected Output Contract
1. `SEC_RESULT|<row_id>|<PASS|FAIL|NA>|<normalized_result>`
2. `SEC_ASSERT|<row_id>|<assertion>|<PASS|FAIL>|<details>`
3. `SEC_LANE_SUMMARY|<lane>|<pass_count>|<fail_count>|<na_count>`

## Required Artifacts (Phase 3)
1. Per-lane security scripts and expected outputs.
2. Security parity runner output with deterministic diffs.
3. Matrix report with row-by-row pass/fail/na evidence.

## Gate Link
- Gate: `A55-GATE-04`
- Upstream dependency: `A55-023`
- Downstream tickets: `A55-031`, `A55-032`, `A55-033`, `A55-034`

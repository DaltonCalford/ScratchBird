# Implementation Notes

- Scope executed: transaction alias rewrite and reject-mode determinism from canonical section-21 parser specs.
- `TSQL_ALIAS_REWRITE_RESULTS.csv` records accepted alias forms and exact canonical rewrite outputs with feature keys and result shapes.
- `TSQL_ALIAS_REJECTION_RESULTS.csv` records disabled-mode and malformed/ambiguous alias failures.
- Alias-disabled rejection is normalized to the canonical parser code `E_ALIAS_MODE_DISABLED` across both rejection-code columns for compatibility.

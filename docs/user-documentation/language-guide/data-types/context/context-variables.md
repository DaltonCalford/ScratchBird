# Context Variables
Last modified: 2026-02-19

Back links:
- [Context README](README.md)
- [Data Types README](../README.md)

Next in series:
- [Trigger Context](trigger-context.md)

Bare context keywords accepted by native parser v3:
- `CURRENT_USER`
- `SESSION_USER`
- `CURRENT_ROLE`
- `CURRENT_CONNECTION`
- `CURRENT_SESSION`
- `CURRENT_TRANSACTION`
- `NOW`
- `CURRENT_DATE`
- `CURRENT_TIME`
- `CURRENT_TIMESTAMP`

Runtime behavior highlights:
- `NOW`: wall-clock evaluation time
- `CURRENT_TIMESTAMP`: transaction-start anchored when transaction exists, otherwise wall-clock fallback
- identity/session/transaction context values return null-like values when unavailable

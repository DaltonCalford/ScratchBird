# Type Family: Temporal
Last modified: 2026-02-19

Back links:
- [Type Families README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [Text And Binary](text-and-binary.md)
- Next: [JSON, Vector, And Search](json-vector-search.md)

## Parser-Accepted Names
- `DATE`, `TIME`, `TIMESTAMP`, `TIME_TZ`, `TIMESTAMP_TZ`, `INTERVAL`, `DATETIME`, `YEAR`.
- Time-zone suffixes parsed on type forms: `WITH TIME ZONE`, `WITHOUT TIME ZONE`.

## Context-Sensitive Temporal Values
- `CURRENT_TIMESTAMP`: transaction-start anchored when a transaction is active.
- `NOW`: wall-clock value at expression evaluation time.
- `CURRENT_DATE`, `CURRENT_TIME`: runtime-generated date/time context values.

## Arithmetic And Conversion
- Temporal arithmetic supports temporal +/- interval and temporal-temporal subtraction-to-interval.
- Implicit text-to-temporal conversion exists for eligible operator families unless strict mode blocks coercion.

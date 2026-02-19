# Aggregate Functions: Core
Last modified: 2026-02-19

Back links:
- [Aggregate README](README.md)
- [Functions README](../README.md)

Next in series:
- [Statistical And Regression](statistical-and-regression.md)

Core aggregate families mapped in emitter:
- `COUNT`
- `SUM`
- `AVG`
- `MIN`
- `MAX`
- `ARRAY_AGG`

Parser support details:
- supports aggregate call parsing including `FILTER (WHERE ...)`
- supports in-call `ORDER BY` for aggregate forms
- parser currently does not close aggregate DISTINCT capture in function call path (`COUNT(DISTINCT ...)` parser gap)

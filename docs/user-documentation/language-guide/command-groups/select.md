# Command Group: SELECT
Last modified: 2026-02-19

Back links:
- [Language Guide README](../README.md)
- [Command Group Index](README.md)

Series navigation:
- Previous: [DROP](drop.md)
- Next: [SET](set.md)

Native v3 `parseSelect()` command block supports:

- `SELECT [DISTINCT [ON (...)] | ALL] <select_list>`
- Optional row controls: `FIRST`, `SKIP`, `LIMIT`, `OFFSET`, `FETCH`, `ROWS`
- Clause order: `FROM`, `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`
- Set operations: `UNION`, `INTERSECT`, `EXCEPT`
- Locking: `FOR UPDATE`, `FOR SHARE`, `FOR NO KEY UPDATE`, `FOR KEY SHARE`, optional `NOWAIT` and `SKIP LOCKED`
- Planning controls: `PLAN ...`, `OPTIMIZE FOR ... ROWS`

Related expression/query bridge surfaces that can appear in `SELECT` workflows:

- `DOC PATH FILTER ...`
- `TS BUCKET AGG ...`
- `SEARCH DSL ...`
- `VECTOR ANN ...`
- `HYBRID BRIDGE EXCHANGE ...`
- `GRAPH PATH MATCH ...`
- `MATCH GRAPH PATH ...`

DML details are in [DML](../dml/README.md).

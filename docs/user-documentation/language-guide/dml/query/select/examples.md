# DML SELECT: Examples
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DML README](../../README.md)
- [Family README](../README.md)
- [Statement README](README.md)

Series navigation:
- Previous: [Clauses](clauses.md)
- Next: [Runtime](runtime.md)

## Coverage
- Status: Supported

## Syntax
~~~sql
WITH [RECURSIVE] <cte_list> SELECT [DISTINCT|ALL] <select_list> FROM <sources> WHERE <predicate> GROUP BY ... HAVING ... ORDER BY ... LIMIT/OFFSET/FETCH;
~~~

## Notes
- Clause summary: Supports WITH/RECURSIVE CTE entry, FIRST/SKIP, LIMIT/OFFSET, FETCH, ROWS, UNION/INTERSECT/EXCEPT, and lock clauses.
- Runtime note: Core parser/emitter/runtime path is available; subquery-membership IN/NOT IN remains partial.
- Error/contract note: IN(subquery) and NOT IN(subquery) are not fully closed in expression runtime routing.
- Usage rationale: Primary read/query path for relational, analytics, and dimensional projections.

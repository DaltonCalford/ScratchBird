### SELECT Queries

What it is
- The read query language: projections, sources, joins, filters, ordering, limits, and set operations.

Why it matters
- SELECT is the core of data retrieval and optimization. Understanding join parsing and set op precedence helps write predictable queries.

How to use it
- Follow the supported constructs below and use examples as templates. For plan analysis, see EXPLAIN/ANALYZE.

Parsing is implemented in `src/engine/parser_select.cpp` with normalized tokens from the lexer.

Supported constructs:
- WITH [RECURSIVE] CTEs (top-level detection)
- FROM tables, subqueries, table functions; `table@link` notation; optional `LATERAL`
- JOINs: INNER (default), LEFT, RIGHT, FULL, CROSS, NATURAL; ON/USING
- WHERE, GROUP BY, HAVING
- ORDER BY with `ASC|DESC` and `NULLS FIRST|LAST`
- FETCH {FIRST|NEXT} n ROWS ONLY
- Set operations with precedence: `INTERSECT` > `UNION`/`EXCEPT`
- PLAN clause parsing helpers

Join parsing builds a right-deep JoinTree; parenthesized join groups and nested subqueries are supported.

Examples:
```sql
SELECT e.name, d.name
FROM employees e
JOIN departments d ON d.id = e.dept_id
WHERE e.id > 1
ORDER BY e.name NULLS LAST
FETCH FIRST 10 ROWS ONLY;
```

Code anchors: `src/engine/parser_select.cpp`

See also
- [Operators](./sql-operators.md) · [DML](./sql-dml.md) · [EXPLAIN/ANALYZE](./explain-analyze.md)


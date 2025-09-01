### EXPLAIN and EXPLAIN ANALYZE

What it is
- Tools to view query plans and actual execution metrics for tuning.

Why it matters
- Understanding plans and actuals is essential for indexing and query optimization.

How to use it
- Prefix your SELECT with EXPLAIN or EXPLAIN ANALYZE; inspect text/JSON outputs for node kinds, rows, and timing.

Parsing surface: `src/engine/parser_session.cpp` (EXPLAIN [ANALYZE] routing). Planning/execution: `src/engine/query_planner.cpp`, `src/engine/executor*.cpp` and node instrumentation.

Outputs:
- Text and JSON plan formats; logical nodes include SeqScan, Filter, Project, Join, etc.
- EXPLAIN ANALYZE includes actual metrics (rows/time) from instrumentation
- EXPLAIN BYTECODE shows the compiled SBLR bytecode for the query
- EXPLAIN ADAPTIVE shows adaptive specialization statistics and JIT compilation status

Examples:
```sql
-- Standard query plan
EXPLAIN SELECT name FROM employees WHERE id > 1;

-- Plan with execution statistics
EXPLAIN ANALYZE SELECT name FROM employees WHERE id > 1;

-- Show bytecode compilation
EXPLAIN BYTECODE SELECT name FROM employees WHERE id > 1;
-- Output: SBLR bytecode instructions for the query

-- Show adaptive optimization status
EXPLAIN ADAPTIVE SELECT name FROM employees WHERE id > 1;
-- Output: Specialization statistics, JIT compilation status
```

See tests for output shapes: `tests/explain_analyze_tests.cpp`.

See also
- [SELECT](./sql-select.md) · [Indexes](./ddl-indexes.md)


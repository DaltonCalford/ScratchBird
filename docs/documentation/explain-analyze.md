### EXPLAIN and EXPLAIN ANALYZE

Parsing surface: `src/engine/parser_session.cpp` (EXPLAIN [ANALYZE] routing). Planning/execution: `src/engine/query_planner.cpp`, `src/engine/executor*.cpp` and node instrumentation.

Outputs:
- Text and JSON plan formats; logical nodes include SeqScan, Filter, Project, Join, etc.
- EXPLAIN ANALYZE includes actual metrics (rows/time) from instrumentation

Examples:
```sql
EXPLAIN SELECT name FROM employees WHERE id > 1;
EXPLAIN ANALYZE SELECT name FROM employees WHERE id > 1;
```

See tests for output shapes: `tests/explain_analyze_tests.cpp`.


### Developer Tools (PSQL)

What it is
- Utilities for analyzing dependencies, formatting code, profiling performance, and validating syntax of PSQL.

Why it matters
- Keeps procedural code maintainable, fast, and consistent; helps detect issues earlier in CI.

How to use it
- Integrate these utilities in developer workflows or CI to lint, format, and report metrics.

Dependency Analyzer (`src/engine/psql_dev_tools.cpp`):
- analyze_dependencies(name, code): extracts CALLs, function calls (basic regex), table refs (FROM/JOIN, INSERT/UPDATE/DELETE)
- analyze_dependency_graph, find_circular_dependencies (simplified)

Code Formatter:
- Options: normalize whitespace, uppercase keywords, indentation, break long lines
- format_code, format_statement, is_well_formatted; keyword list includes core SQL/PSQL terms

Performance Profiler:
- start_profiling/stop_profiling per procedure; records counts and microsecond timings; statement counts; generate_report

Syntax Validator:
- validate_syntax parses code, reports issues and common problems; generate report string

These utilities are meant for developer workflows and PSQL code hygiene.

See also
- [PSQL runtime](./psql-runtime.md) · [Routines & triggers](./psql-routines-and-triggers.md)


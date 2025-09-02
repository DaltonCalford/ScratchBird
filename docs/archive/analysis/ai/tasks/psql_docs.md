### Task: PSQL runtime documentation authoring

Goal: Author PSQL runtime docs with full feature coverage and Implementation References.

Input:
- `psql_executor.*`, `psql_dev_tools.*`, `ast.h`, `parser_psql.cpp`
- ProjectPlan Phase 8

Steps:
1. Author overview and per-feature sections (execute block, variables, control flow, exceptions, cursors, security, packages, dev tools, debugging).
2. For each feature, add Implementation References to key methods.
3. Add Spec Trace for the corresponding REQ-PSQL-RUNTIME-* IDs.

Output:
- Updated `subprojects/psql/index.md` with detailed sections.

Validation:
- At least 10 anchors spanning executor, dev tools, and parser.

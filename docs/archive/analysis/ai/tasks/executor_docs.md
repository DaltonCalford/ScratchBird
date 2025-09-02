### Task: Executor documentation authoring

Goal: Author executor/operator docs with Implementation References and Spec Trace.

Input:
- `include/scratchbird/engine/executor*.h`, `src/engine/executor/*.cpp`, `src/engine/expr.cpp`
- ProjectPlan Phase 5, 6

Steps:
1. Create operator pages or sections (SeqScan, IndexScan, Filter, Project, Sort, Limit, HashAgg, NLJ, Window subset).
2. For each operator, list inputs/outputs, invariants, memory behavior, spill policy.
3. Add Implementation References to specific `next()` implementations and helpers.
4. Cross-link to optimizer cost entries.

Output:
- Updated `subprojects/query-engine/index.md` and operator subsections.

Validation:
- Each operator has at least one source anchor pointing to its `next()` and construction path.

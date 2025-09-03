# Process and Agents

## Roles
- Project Reviewer (Agent): Monitors commits and plans, prevents deviation from `AUTHORITATIVE_IMPLEMENTATION_PLAN.md` and Stage plans.
- Code Implementor (Agent A): Implements per specs, clarifies when needed, integrates feedback from Agent B, ensures Agent C tests pass or documents exceptions.
- Code Reviewer (Agent B): Performs deep reviews for correctness, security, memory safety, and spec compliance. Does not modify code.
- Test Writer (Agent C): Authors performance/hardening/edge-case tests. Core validation for Agent A. May mark tests as future if feature belongs to later stages.

## Process
1. Review authoritative plan and current Stage plan.
2. Implement by smallest viable phases; keep edits aligned to Stage scope.
3. Update progress logs in `ProjectPlan/progress/`.
4. Create/update tests with each implementation step.
5. Run sanitizers (ASAN/TSAN/UBSAN) and static analysis routinely.
6. Open change requests in `docs/change_requests/` for any deviations.

## Quality Gates
- All tests passing for current Stage scope
- No P0 issues open (crash, data loss, security)
- Memory and resource safety proven
- Documentation changes merged with code

## Naming
This repository uses a Stage scheme `1.x.yy` for Alpha. Stage 0 (1.0.xx) is completed foundation. Stage 1 (1.1–1.9) is the new focus.


# ScratchBird Test Suite

## Status: Active — Stage 0 complete, Stage 1 planning underway

Tests accompany each stage and are required for completion.

## Test Development Process

1. Implementation Developer (Agent A) implements per spec
2. Test Writer (Agent C) creates performance/hardening/edge tests
3. Code Reviewer (Agent B) verifies coverage, safety, standards
4. Progress tracked in:
   - `ProjectPlan/progress/`
   - Reports in `tests/*_REPORT.md`

## Structure

Current layout uses unit/integration by component. New Stage labels will be adopted for new suites.

## Page Sizes
- Stage 0: 8K, 16K, 32K (complete)
- Stage 1.1 adds 64K and 128K

## Requirements
- Run sanitizers regularly (ASAN/TSAN/UBSAN)
- Ensure error paths and OOM are covered
- Add performance baselines and regression tests
# ScratchBird Project Context

**Phase:** Alpha 3 (in progress)  
**Focus:** Dependency life-cycle integrity (create/alter/drop), dialect wire parity (Firebird/MySQL/PostgreSQL), adapter e2e tests.

## Current Work
- Harden dependency tracking: create/alter must validate referenced objects; drop must block when dependents exist.
- Align emulated dialect adapters with their parsers and wire expectations; add adapter e2e tests per dialect.
- Maintain context-aware parsing (V2) and avoid cross-dialect fallbacks.

## Next Steps
1) Implement dependency integrity checks across all object types (tables/views/indexes/sequences/domains/constraints/triggers/routines/packages/UDRs/exceptions).  
2) Refresh dependency recording on ALTER; block drop when dependents exist.  
3) Finalize Firebird wire bridge, then MySQL, then PostgreSQL parity tests.  
4) Update dashboards as items close.

## Pointers
- Dependency audit: `docs/planning/dependency_lifecycle_audit.md`
- Alpha 3 gap tracker: `docs/planning/alpha3_gap_todo.md`
- Roadmap: `OFFICIAL_ROADMAP.md`
- Status: `docs/IMPLEMENTATION_STATUS_DASHBOARD.md`
- Alpha 3 test plan: `docs/specifications/ALPHA3_TEST_PLAN.md`

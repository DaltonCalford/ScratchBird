# ScratchBird Project Status

## Current Status: Stage 0 COMPLETE; Stage 1 Planning In Progress

### Completed (Stage 0 — 1.0.xx)
- 1.0.01–1.0.02: Database Core (create/open, header, base schemas)
- 1.0.03: Page Management (FSM, buffer pool, LRU, dirty)
- 1.0.04: System Catalog (schemas/tables/columns)
- 1.0.05: Storage Engine (heap, tuples, scans)
- 1.0.06: Transaction Foundation (XID, TIP, MVCC, commit/rollback)
- 1.0.07: Basic SQL Parser (baseline statements)

### Documentation Status
- Authoritative plan present: `AUTHORITATIVE_IMPLEMENTATION_PLAN.md`
- Stage 1 plan: `ProjectPlan/ALPHA_STAGE_1_PLAN.md`
- Restructure mapping: `ProjectPlan/PHASE_NUMBERING_RECONCILIATION.md`, `ProjectPlan/STAGE_RESTRUCTURE_REPORT.md`

### Implementation Approach (Stage 1)
1. Embedded-first; no network routing
2. Test-driven with sanitizers (ASAN/TSAN/UBSAN) in CI
3. Expand to 64K/128K in Stage 1.1 (was Beta in earlier docs)
4. Maintain UUID v7 and MGA/MVCC principles

### Stage 1 Next Steps
1. Execute Stage 1.1: 64K/128K, compression, TOAST/LOB
2. Execute Stage 1.2: Advanced SBLR (joins, subqueries, windows)
3. Execute Stage 1.3: Concurrency (multi-threaded buffer pool, locks, deadlock)
4. Execute Stage 1.4: Advanced indexes (bitmap, hash)
5. Execute Stage 1.5–1.7: Embedded API, context-aware parser, sb_isql_a

### Repository Structure
```
.
├── ProjectPlan/
│   ├── progress/
│   ├── reviews/
│   ├── ALPHA_STAGE_1_PLAN.md
│   ├── PHASE_NUMBERING_RECONCILIATION.md
│   └── STAGE_RESTRUCTURE_REPORT.md
├── references/
│   ├── data_types/
│   └── technical_specifications/
├── docs/
├── src/
├── include/
└── tests/
```

### Quality Standards
- Compile clean, no warnings
- Tests pass across all configured page sizes
- Memory/resource safety proven (sanitizers)
- Thread safety verified (TSAN)
- Documentation updated with code changes

---

Last Updated: 2025-01 (Current Session)
Status: Stage 1 Planning In Progress
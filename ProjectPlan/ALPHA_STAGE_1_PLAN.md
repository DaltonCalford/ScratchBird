# Alpha Stage 1 Plan (1.1 – 1.9)

This document defines the Alpha Stage 1 roadmap after Stage 0 (foundation) completion. It introduces Stage-based numbering (1.x.yy) and entry/exit criteria per stage.

## Scope and Goals
- Embedded, local, exclusive-lock database engine suitable for use as a library.
- No network or Y‑Valve routing in Stage 1; parser and engine run in-process.
- SBLR fully expressive for SQL constructs, even if some execution entries defer to later phases.

## Stage 1.1 — Extended Storage
Goal: Support 64KB and 128KB pages, compression, TOAST/LOB.
- Deliverables:
  - Page sizes: 64K, 128K
  - Pluggable compression (LZ4 baseline)
  - TOAST/LOB storage for large attributes
- Entry criteria: Stage 0 complete, storage engine stable on 8K/16K/32K
- Exit criteria: All tests pass across 5 page sizes; compression interoperability documented
- Notes: Update on-disk format docs as needed; avoid breaking 8K/16K/32K compatibility

## Stage 1.2 — Advanced SBLR
Goal: Expand SBLR to handle joins, subqueries, window functions; cover all SQL grammar productions.
- Deliverables:
  - SBLR ops for joins, subqueries, windows, expressions
  - Coverage mapping to SQL grammar (see references)
  - Stubs/entrypoints for unimplemented execution paths
- References:
  - `references/data_types/SCRATCHBIRD_UNIVERSAL_TYPE_SYSTEM.md`
  - `references/technical_specifications/SBLR_BYTECODE_SPECIFICATION.md`
  - `references/technical_specifications/SQL_LANGUAGE_OVERVIEW.md` (BNF: add `SQL_COMPLETE_BNF.md`)
- Exit criteria: Parser can emit SBLR for all tracked constructs; executor accepts or stubs gracefully

## Stage 1.3 — Concurrency
Goal: Multi-threaded buffer pool, lock manager, deadlock detection.
- Deliverables:
  - Thread-safe buffer pool with configurable workers
  - Lock manager (row/table/index) and deadlock detection
  - TSAN-clean test suite for concurrency paths
- Exit criteria: No TSAN/UBSAN issues; deterministic deadlock tests

## Stage 1.4 — Advanced Indexes
Goal: Add bitmap and hash indexes; approach parity with major engines.
- Deliverables:
  - Hash index
  - Bitmap index
  - API for future types; documentation for parity gaps
- Exit criteria: Index creation/use tested across page sizes; performance baselines recorded

## Stage 1.5 — Embedded API
Goal: Stable embedded C/C++ API for applications.
- Deliverables:
  - Session/statement/transaction APIs
  - Error contexts and diagnostic surfaces
  - Versioning and compatibility policy
- Exit criteria: Sample apps compile; ABI tests pass; docs complete

## Stage 1.6 — SBSQL Context-Aware Parser
Goal: Build context-aware parser using the embedded engine.
- Deliverables:
  - Context-aware parsing with symbol resolution
  - Full round-trip: SQL -> SBLR -> Engine
  - Error messages with context
- Exit criteria: Parser test corpus passes; integration with embedded API proven

## Stage 1.7 — sb_isql_a Tool
Goal: CLI that uses the parser and engine to validate embedded workflows.
- Deliverables:
  - `sb_isql_a` executable with scripts and regression suite
  - Non-networked local database operations
- Exit criteria: Full embedded workflows covered; regression tests added to CI

## Stage 1.8 / 1.9 — Catch-up and Hardening
Goal: Close gaps discovered during 1.1–1.7, performance tuning, documentation.
- Deliverables:
  - Outstanding features from earlier stages
  - Performance and stability improvements
  - Documentation and migration notes
- Exit criteria: All identified gaps closed or deferred with rationale; readiness review for Beta

## Testing Strategy
- Unit, integration, and performance tests per stage
- Sanitizers (ASAN, TSAN, UBSAN) in CI
- Requirements Traceability Matrix maintained in `docs/`

## Governance
- Project Reviewer ensures adherence to this plan
- Deviations require documented change requests in `docs/change_requests/`


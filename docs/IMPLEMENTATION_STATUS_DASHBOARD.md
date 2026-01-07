# Implementation Status Dashboard

**Last updated:** 2026-01-06  
**Tests:** `ctest --test-dir build` → 2,007 passed, 42 skipped (network/socket gating, protocol/session pre-reqs).

## Phase Summary (history + todo)
- **Alpha 1 – Engine/Core**  
  - [x] Storage engine, MGA transactions, catalog, indexes, sequences  
  - [x] Bootstrap parser, basic DDL/DML, core tests  
- **Alpha 2 – Parser V2 & Dialects**  
  - [x] Parser v2 (context-aware), ScratchBird dialect  
  - [x] Firebird/MySQL/PostgreSQL dialect parsers  
  - [x] Semantic analyzer v2, SBLR bytecode v2  
- **Alpha 3 – Network & Service**  
  - [x] Network stack, service mode, security (core/enterprise)  
  - [x] Wire adapters (FB/MySQL/PG/native), pooling, FDW/UDR, ODBC/JDBC  
- **Alpha (Completion) – Parser Alignment & Audit Repairs** *(current)*  
  - [x] MySQL/PostgreSQL DML bytecode alignment (SBLR v2 format)  
  - [ ] Firebird DDL/DML alignment (via v2 → SBLR v2 or executor extensions)  
  - [ ] Executor gaps for emulated dialects (see list below)  
  - [ ] Dialect parity + adapter e2e suites per dialect; no cross-dialect fallbacks; Firebird→MySQL→PostgreSQL order  

## Outstanding Detail (Alpha 3 blockers)
1) Full dependency life-cycle enforcement across all object types.  
2) Dialect-specific adapter e2e coverage (Firebird, then MySQL, then PostgreSQL).  

## Plan Progress (Active)
- Plan 02 (UUID Resolution/Rename/Move): complete (resolver cache/view, rename/move across object types, resolver rebuild + test coverage).
- Plan 04 (Emulated parser alignment): in progress (see `docs/planning/PLAN_04_PARSER_BYTECODE_ALIGNMENT_PROGRESS.md`).
- Plan 06–08 (ISQL clients, protocol conformance, test automation): pending start after parser alignment.

## Executor Gaps (Documented for Follow-up)
- DML extras not supported: `ON CONFLICT`, `UPDATE ... FROM`, `DELETE ... USING`, `INSERT ... SELECT`, multi-row `VALUES`, `RETURNING`.
- SELECT limitations: no DISTINCT handling, no expression SELECT-list with FROM, ORDER BY/GROUP BY expressions limited to column refs.
- DML ORDER BY/LIMIT for UPDATE/DELETE not supported.
- JOIN emission not wired for emulated parsers (executor supports join opcodes, but dialect parsers currently skip them).

## Links
- Roadmap: `OFFICIAL_ROADMAP.md`  
- Current context: `PROJECT_CONTEXT.md`  
- Planning: `docs/planning/` (e.g., `alpha3_gap_todo.md`, `dependency_lifecycle_audit.md`)  
- Specs: `docs/specifications/`

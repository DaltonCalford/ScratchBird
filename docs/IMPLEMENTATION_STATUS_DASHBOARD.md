# Implementation Status Dashboard

**Last updated:** 2026-01-18  
**Tests:** `ctest --test-dir build` → 2,296 passed, 18 skipped (gated: TCP/Unix sockets, protocol/session, sweep parser, JSON negative cases, Firebird IPC bridge).

## Phase Summary (history + todo)
- **Alpha 1 – Engine/Core**  
  - [x] Storage engine, MGA transactions, catalog, indexes, sequences  
  - [x] Bootstrap parser, basic DDL/DML, core tests  
- **Alpha 2 – Parser V2 & Dialects**  
  - [x] Parser v2 (context-aware), ScratchBird dialect  
  - [x] Firebird/MySQL/PostgreSQL dialect parsers  
  - [x] Semantic analyzer v2, SBLR bytecode v2  
- **Alpha 3 – Network & Service** *(next)*  
  - [x] Core network stack + IPC  
  - [x] Wire adapters (FB/MySQL/PG/native), pooling, FDW/UDR, ODBC/JDBC  
  - [ ] Listener binaries + parser pools per dialect  
  - [ ] Server auth wiring (HBA/SCRAM/TLS/MFA hooks)  
- **Alpha (Completion) – Parser Alignment & Audit Repairs**  
  - [x] MySQL/PostgreSQL DML bytecode alignment (SBLR v2 format)  
  - [x] Firebird DDL/DML alignment (via v2 → SBLR v2 or executor extensions)  
  - [x] Parser critical findings remediation complete (Phases 0–6)  
  - [x] Full build + full test suite pass (gated network tests only)  

## Outstanding Detail (Alpha 3 blockers)
1) Listener binaries + parser pool lifecycle per dialect (native/FB/PG/MySQL).  
2) Server auth path wiring (HBA/SCRAM/TLS/MFA) and protocol handshake integration.  
3) Dialect‑specific adapter e2e coverage (Firebird → MySQL → PostgreSQL) once listeners land.  

## Plan Progress (Active)
- Plan 02 (UUID Resolution/Rename/Move): complete (resolver cache/view, rename/move across object types, resolver rebuild + test coverage).
- Plan 04 (Emulated parser alignment): complete (see `docs/archive/2026-01-09/planning/PLAN_04_PARSER_BYTECODE_ALIGNMENT_PROGRESS.md`).
- Parser Critical Findings Remediation: complete (`docs/planning/PLAN_ALPHA_PARSER_CRITICAL_FINDINGS_REMEDIATION.md`; full ctest pass 2026-01-18).
- Plan 06–08 (ISQL clients, protocol conformance, test automation): pending after listener work.

## Known Alpha Limitations (Explicit Warnings/Errors)
- TRUNCATE `CASCADE` / `RESTART IDENTITY`: warning + proceed without cascade/restart.
- `SIMILAR TO ... ESCAPE`: warning; ESCAPE ignored.
- COPY `ENCODING BINARY`: unsupported (UTF8/UTF-8 only in Alpha).
- Aggregation with joins/CTE and `SELECT *` aggregation: executor limitation (tracked in core engine plan).
- Dialect guardrails: MySQL partition clauses and `LOCK/UNLOCK TABLES` are explicit errors; non‑dialect DDL rejected by allowlists.

## Links
- Roadmap: `OFFICIAL_ROADMAP.md`  
- Current context: `PROJECT_CONTEXT.md`  
- Planning: `docs/planning/CONSOLIDATED_FINDINGS_REMEDIATION_PLAN.md` and `docs/archive/2026-01-09/planning/`  
- Specs: `docs/specifications/`

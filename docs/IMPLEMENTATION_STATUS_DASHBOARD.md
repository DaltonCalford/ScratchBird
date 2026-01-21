# Implementation Status Dashboard

**Last updated:** 2026-01-20  
**Tests (last run 2026-01-20):** `ctest --test-dir build --output-on-failure` → 2,286 passed, 0 failed, 18 skipped (skip list: FirebirdAdapterBridge, TCP/Unix socket integration, SweepMechanism parsing, ProtocolSession, JSONFunction).

## Phase Summary (history + todo)
- **Alpha 1 – Engine/Core**  
  - [x] Storage engine, MGA transactions, catalog, indexes, sequences  
  - [x] Bootstrap parser, basic DDL/DML, core tests  
- **Alpha 2 – Parser V2 & Dialects**  
  - [x] Parser v2 (context-aware), ScratchBird dialect  
  - [x] Firebird/MySQL/PostgreSQL dialect parsers  
  - [x] Semantic analyzer v2, SBLR bytecode v2  
- **Alpha 3 – Network & Service** *(current)*  
  - [x] Core network stack + IPC  
  - [x] Listener binaries + parser pools per dialect  
  - [x] Wire adapters (FB/MySQL/PG/native), pooling, FDW/UDR  
  - [ ] Driver adapters (ODBC/JDBC) readiness and integration  
  - [ ] Server auth wiring (HBA/SCRAM/TLS/MFA hooks)  
- **Alpha (Completion) – Parser Alignment & Audit Repairs**  
  - [x] MySQL/PostgreSQL DML bytecode alignment (SBLR v2 format)  
  - [x] Firebird DDL/DML alignment (via v2 → SBLR v2 or executor extensions)  
  - [x] Parser critical findings remediation complete (Phases 0–6)  
  - [x] Full build + full test suite pass (gated network tests only)  

## Outstanding Detail (Alpha 3 blockers)
1) Server auth path wiring (HBA/SCRAM/TLS/MFA) and protocol handshake integration.  
2) Driver adapter readiness (ODBC/JDBC) and client integration work.  
3) Dialect‑specific adapter e2e coverage (Firebird → MySQL → PostgreSQL).  

## Plan Progress (Active)
- Plan 02 (UUID Resolution/Rename/Move): complete (resolver cache/view, rename/move across object types, resolver rebuild + test coverage).
- Plan 04 (Emulated parser alignment): complete (see `docs/archive/2026-01-09/planning/PLAN_04_PARSER_BYTECODE_ALIGNMENT_PROGRESS.md`).
- Parser Critical Findings Remediation: complete (`docs/planning/PLAN_ALPHA_PARSER_CRITICAL_FINDINGS_REMEDIATION.md`; full ctest pass 2026-01-20).
- Plan 06–08 (ISQL clients, protocol conformance, test automation): pending; listener work unblocked, next after auth + driver wiring.

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

# ScratchBird Project Context

**Phase:** Alpha completion (Plan 04 in progress)  
**Focus:** Emulated parser bytecode alignment, executor gap closure, and audit 16–23 repairs; follow `docs/planning/PLAN_AUDIT_16_23_REPAIR.md` plus the latest alignment status in `docs/planning/PLAN_04_PARSER_BYTECODE_ALIGNMENT_PROGRESS.md`.

## Key Architecture Notes for AI
- **MGA (Firebird model):** Snapshot by default; readers/writers non-blocking. No WAL for recovery; WAL planned in Beta only for logging/ETL/replication. Backups: transaction-isolated logical dump; shadow/live page copy.  
- **Optional libraries:** Keep minimal deps; OpenSSL is build-time; others (GEOS/PROJ/libxml2/LZ4) are optional. Runtime-load idea captured separately.  
- **Emulated engines:** Strict sandbox; no ScratchBird-only features surfaced (e.g., 128-bit types, advanced domains).  
- **Search path:** Schema/current path with left-to-right resolution; package members hidden by default unless explicitly listed.  
- **Packages:** Package-as-container; internal visibility; external-callable flags; SHOW … IN PACKAGE for listings; resolve name collisions vs schema.proc.  
- **Locks:** Default MGA non-blocking; explicit “WITH LOCK” enables row/table locks; add deadlock/timeout handling.  
- **Triggers:** Before/after for DB/table (SELECT support TBD); ordered by smallint; ensure runtime hooks.  

## Current Work (see `docs/planning/PLAN_AUDIT_16_23_REPAIR.md`)
- Emulated parser alignment to SBLR v2 (MySQL/PostgreSQL DML done; Firebird DDL/DML pending).
- Document and close executor gaps surfaced by parser alignment (ON CONFLICT/RETURNING/UPDATE FROM/DELETE USING, SELECT DISTINCT, etc.).
- Dialect parity and adapter e2e tests (Firebird → MySQL → PostgreSQL order); no ScratchBird fallback.
- Listener/parser binaries + protocol conformance test harnesses (Plan 06–08).

## Pointers
- Next steps: `docs/planning/ALPHA_NEXT_STEPS.md`
- Roadmap: `OFFICIAL_ROADMAP.md`
- Status dashboard: `docs/IMPLEMENTATION_STATUS_DASHBOARD.md`
- Alpha 3 test plan: `docs/specifications/ALPHA3_TEST_PLAN.md`

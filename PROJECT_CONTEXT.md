# ScratchBird Project Context

**Phase:** Alpha 3 (in progress)  
**Focus:** Finish Alpha 3 hardening (dependency lifecycle, triggers/packages, search path, RLS/masking, MGA with optional locks, dialect parity) and follow `docs/planning/ALPHA_NEXT_STEPS.md` as the current work list.

## Key Architecture Notes for AI
- **MGA (Firebird model):** Snapshot by default; readers/writers non-blocking. No WAL for recovery; WAL planned in Beta only for logging/ETL/replication. Backups: transaction-isolated logical dump; shadow/live page copy.  
- **Optional libraries:** Keep minimal deps; OpenSSL is build-time; others (GEOS/PROJ/libxml2/LZ4) are optional. Runtime-load idea captured separately.  
- **Emulated engines:** Strict sandbox; no ScratchBird-only features surfaced (e.g., 128-bit types, advanced domains).  
- **Search path:** Schema/current path with left-to-right resolution; package members hidden by default unless explicitly listed.  
- **Packages:** Package-as-container; internal visibility; external-callable flags; SHOW … IN PACKAGE for listings; resolve name collisions vs schema.proc.  
- **Locks:** Default MGA non-blocking; explicit “WITH LOCK” enables row/table locks; add deadlock/timeout handling.  
- **Triggers:** Before/after for DB/table (SELECT support TBD); ordered by smallint; ensure runtime hooks.  

## Current Work (see `docs/planning/ALPHA_NEXT_STEPS.md`)
- Dependency integrity (create/alter/drop) across all object types.
- Package resolution/visibility; search path enforcement; RLS/masking.
- Dialect parity and adapter e2e tests (Firebird → MySQL → PostgreSQL order); no ScratchBird fallback.
- Tests and runtime wiring for triggers, locks, and dependency blocking.

## Pointers
- Next steps: `docs/planning/ALPHA_NEXT_STEPS.md`
- Roadmap: `OFFICIAL_ROADMAP.md`
- Status dashboard: `docs/IMPLEMENTATION_STATUS_DASHBOARD.md`
- Alpha 3 test plan: `docs/specifications/ALPHA3_TEST_PLAN.md`

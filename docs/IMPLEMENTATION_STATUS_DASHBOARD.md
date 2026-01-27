# Implementation Status Dashboard

**Last updated:** 2026-01-27  
**Tests (last run 2026-01-20):** `ctest --test-dir build --output-on-failure` → 2,286 passed, 0 failed, 18 skipped (skip list: FirebirdAdapterBridge, TCP/Unix socket integration, SweepMechanism parsing, ProtocolSession, JSONFunction).

## Alpha Status (current)

### Core Engine
- **Completed:** storage engine, MGA transactions, scheduler/jobs, constraint enforcement,
  security enforcement wiring, cache/buffer plan.
- **Completed:** tablespace routing defaults + root page allocation; index migration safety
  for SPGIST/BITMAP/COLUMNSTORE/LSM.
- **In Progress:** monitoring parity;
  backup/restore parity across tablespaces/catalogs.
- **Open:** expression/partial index root allocation still uses primary tablespace.
- **Open:** restore uses only first tablespace file path (multi‑file tablespace).
- **Open:** timezones/charsets/collations resource loading + catalog persistence.

### Parser + PSQL
- **Completed:** V2 parser base, dialect parsers, semantic analyzer v2, baseline bytecode.
- **In Progress:** V2 parser completeness (DDL/DML/utility/PSQL surface).
- **Open:** PSQL bytecode emission + executor parity (FOR/CASE/SUSPEND/etc).

### Network & Service
- **Completed:** listener/pool/parser/server process and wire adapters (FB/MySQL/PG/native).
- **In Progress:** dialect parity test suites and remaining auth/config wiring (if any).

## Outstanding Detail (Alpha blockers)
1) Tablespace routing defaults + root page allocation (tablespace‑aware).  
2) Index migration safety for SPGIST/BITMAP/COLUMNSTORE/LSM.  
3) Monitoring parity (remaining MON$ placeholders).  
4) Backup/restore parity across tablespaces/catalogs.  
5) V2 parser completeness + PSQL bytecode/executor parity.  
6) Timezone/charset/collation resource loaders + catalog persistence.  
7) IVF/Zone Maps/inverted GC index gaps and GPID/TID checks.  

## Plan Progress (Active)
- **Alpha Completion Master Plan:** in progress (`planning/ALPHA_COMPLETION_MASTER_PLAN.md`).
- **Engine Core Alpha Completion:** in progress (`planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md`).
- **V2 Parser Completion:** in progress (`planning/PLAN_V2_PARSER_COMPLETION.md`).
- **Resources I18N/Timezone:** open (`planning/RESOURCES_I18N_TIMEZONE_REMEDIATION_PLAN.md`).
- **Index Spec Gap Tracker:** open (`planning/TRACKER_INDEX_SPEC_GAPS.md`).
- **Cache/Buffer Remediation:** done (`planning/CACHE_AND_BUFFER_REMEDIATION_PLAN.md`).

## Known Alpha Limitations (Explicit Warnings/Errors)
- TRUNCATE `CASCADE` / `RESTART IDENTITY`: warning + proceed without cascade/restart.
- `SIMILAR TO ... ESCAPE`: warning; ESCAPE ignored.
- COPY `ENCODING BINARY`: unsupported (UTF8/UTF-8 only in Alpha).
- Aggregation with joins/CTE and `SELECT *` aggregation: executor limitation (tracked in core engine plan).
- Dialect guardrails: MySQL partition clauses and `LOCK/UNLOCK TABLES` are explicit errors; non‑dialect DDL rejected by allowlists.

## Links
- Roadmap: `../OFFICIAL_ROADMAP.md`
- Alpha/Beta scope: `findings/ALPHA_BETA_SCOPE_STATUS.md`
- Alpha plan: `planning/ALPHA_COMPLETION_MASTER_PLAN.md`
- Current context: `../PROJECT_CONTEXT.md`
- Planning: `planning/`
- Specs: `specifications/`

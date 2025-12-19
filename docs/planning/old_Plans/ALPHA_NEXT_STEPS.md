# Alpha Next Steps (Prioritized)

**Scope:** Alpha hardening and remaining Alpha 3 completion items (ScratchBird core; emulated engines stay within native capabilities). Based on recent audits; replaces per-file audit notes.

## Priority P0 (blockers)
- Dependency life-cycle enforcement across all object types: validate on create/alter, refresh deps on alter, block drop when dependents exist (functions/procs/packages/UDRs/exceptions/tables/views/indexes/sequences/domains/constraints/triggers). Add tests.  
- Package resolution rules: package-as-container flag, internal visibility, external-callable flag per member, SHOW … IN PACKAGE listing, dependency links, drop rules, and name resolution vs schema.proc.  
- Search path resolution: session SET current schema/search_path; left-to-right path; full-path B-tree index + UUID hash; emulation sandboxing; packages excluded from default listings.  
- RLS and column masking: confirm/implement row filters; add column-level masking/redaction policies with predicates; enforce security and add tests.  
- Locking with MGA defaults: keep non-blocking snapshot; add explicit “WITH LOCK” row/table locks, conflict matrix, deadlock detection/timeout, admin lock inspection; emulation limited to native semantics.  
- Trigger runtime wiring: verify before/after hooks for DB/table (and SELECT if supported), ordering (smallint, low→high), dependency blocking; add tests.

## Priority P1
- Structured domains: record-like domains with computed fields, EXTRACT/SET, domain-specific casts, catalog storage, dependency enforcement, and tests.  
- 128-bit numerics (ScratchBird-only): add UINT128/FLOAT128, storage/serialization, evaluator ops, casts, index comparators, planner support, dialect fallbacks.  
- First-class cursor handles: DECLARE/OPEN/FETCH/CLOSE, pass handles across routines/triggers, use in FROM, transaction-scoped registry, tests.  
- CDC via transaction stamps: opt-in tables, changes-by-txn-range views/functions, op type, security, and tests.  
- Checkpoint policy: define non-WAL checkpoint triggers/behavior and diagnostics for MGA; align with shadow backup consistency.

## Priority P2
- Scheduler: job catalog, cron/interval schedules, local/cluster (future) control, retries/backoff, status/history, security (needed to close Alpha tooling gaps).  
- Optimizer use of stats: cost/selectivity model tied to per-index stats; planner debug output; controls.  
- Logging/telemetry: query/audit logging scope, redaction, metrics endpoints/access control, retention/rotation.  
- Index stats collection: per index type (btree/hash/gist/gin/rtree/bitmap/columnstore/fulltext) collection triggers, catalog persistence, and planner consumption.

## Notes
- Keep ScratchBird-only features (e.g., 128-bit, advanced domains) out of emulated engines; adapters may expose safe fallbacks only.  
- Reference: `docs/planning/Beta Phase 0 Implementation Plan.md` for upcoming Beta packaging/tooling; Alpha tasks here are independent of packaging.  

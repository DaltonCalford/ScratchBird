# Stage Restructure Report — ScratchBird Database Engine

## Purpose
This report reconciles historical Alpha work with the updated Stage plan and numbering. It replaces mixed sequencing (core, networking, parser extras) with a linear engine-first pipeline. It also introduces the naming of the oversight agent as "Project Reviewer".

## Agent Naming
- Agent role name: Project Reviewer (formerly generic reviewer/monitor)

## High-Level Direction Change
- Focus Alpha on embedded, local, non-network, exclusive-lock engine.
- Defer network/protocol routing until after embedded API is stable.
- Make SBLR broadly capable early, but postpone context-aware parser to Stage 1.6.

## Stage Model
First digit: 1=Alpha, 2=Beta
Second digit: Stage track within Alpha—each Stage is a thematic grouping.
Third and fourth digits: Individual phases within a Stage (e.g., 1.1.01).

### Stage 0 (Completed Foundation)
- 1.0.01–1.0.05: Core foundation work (database create/open, FSM, buffer pool, system catalog, storage engine, transactions, basic parser for initial tests)

### Stage 1 (Planned, Pre-Beta Readiness)
- 1.1: Extended Storage — 64KB/128KB pages, compression, TOAST/LOB
- 1.2: Advanced SBLR — joins, subqueries, window functions; SBLR entries for all SQL BNF constructs per `references/data_types/SCRATCHBIRD_UNIVERSAL_TYPE_SYSTEM.md` and `references/technical_specifications/SQL_LANGUAGE_OVERVIEW.md` (BNF complete spec document to be added)
- 1.3: Concurrency — multi-threaded buffer pool, lock manager, deadlock detection
- 1.4: Advanced Indexes — bitmap, hash, plus parity with PostgreSQL/MySQL/Firebird families
- 1.5: Embedded API — stable C/C++ API for embedded use (local, non-network)
- 1.6: SBSQL Context-Aware Parser — dedicated parser tier using engine as embedded library
- 1.7: `sb_isql_a` — CLI using the parser and engine for full embedded validation
- 1.8/1.9: Catch-up/Hardening — fill gaps, polish, stabilize for Beta

## Mapping: Historical Phases -> New Stage Scheme
| Historical Label | Actual Content | New Stage.Code |
|---|---|---|
| Alpha 1.01.1 | Database Core (create/open, header, base schemas) | 1.0.01–1.0.02 |
| Alpha 1.01.2 | Page Management (FSM, buffer pool, LRU, dirty pages) | 1.0.03 |
| Alpha 1.02 | System Catalog (schemas, tables, columns) | 1.0.04 |
| Alpha 1.03 (aka 1.04) | Storage Engine (heap, tuples, scans, visibility) | 1.0.05 |
| Alpha 1.04 | Transaction Foundation (XID, TIP, MVCC, commit/rollback) | 1.0.06 |
| Alpha 1.05 | Basic SQL Parser (non-join SELECT, CREATE TABLE, INSERT) | 1.0.07 |

Note: The above consolidates the already completed foundation under Stage 0 (1.0.xx). Future work proceeds as Stage 1 (1.1–1.7+).

## Documentation Updates Required
1. README — Reflect Alpha 1.05 complete and Stage 1 planning underway (done).
2. AUTHORITATIVE_IMPLEMENTATION_PLAN.md — Update Beta-scheduled items vs Stage 1; clarify that 64K/128K move to Stage 1.1 (Alpha), and keep Beta criteria unchanged for distributed and protocol parity features.
3. PHASE_NUMBERING_RECONCILIATION.md — Convert to Stage mapping with 1.0.xx for completed foundation.
4. PROJECT_STATUS.md — Mark Stage 0 complete; Stage 1 planning in progress.
5. Add Stage plan files (see below).

## New/Updated Planning Artifacts
- ProjectPlan/ALPHA_STAGE_1_PLAN.md — Detailed 1.1–1.7 breakdown with entry/exit criteria.
- references/technical_specifications/SQL_COMPLETE_BNF.md — Add or link authoritative BNF; existing `SQL_LANGUAGE_OVERVIEW.md` is insufficient.
- references/data_types/SCRATCHBIRD_UNIVERSAL_TYPE_SYSTEM.md — Already present; ensure cross-references from Stage 1.2.

## Risks and Considerations
- Parser and SBLR scope creep: keep context-aware parsing for Stage 1.6.
- Index feature parity is large; prioritize btree, hash, bitmap in 1.4 and document others for 1.8/1.9 if needed.
- Concurrency and deadlock detection need TSAN coverage in CI; plan perf baselines.

## Decision
Adopt the Stage numbering herein and proceed to produce the Stage 1 plan before implementation work begins.


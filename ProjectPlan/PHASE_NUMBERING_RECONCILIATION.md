# Phase Numbering Reconciliation (Stage Format)

## Issue
Phase numbering mixed content across core, parser, and network. We are adopting a Stage-based scheme: 1.x.yy where 1=Alpha, x=Stage, yy=phase within Stage.

## Mapping to Stage 0 (Completed Foundation)
| Old Label | Actual Content | New Stage.Code | Status |
|---|---|---|---|
| Alpha 1.01.1 | Database Core (create/open, header, base schemas) | 1.0.01–1.0.02 | ✅ Complete |
| Alpha 1.01.2 | Page Management (FSM, buffer pool, LRU, dirty) | 1.0.03 | ✅ Complete |
| Alpha 1.02 | System Catalog (schemas/tables/columns) | 1.0.04 | ✅ Complete |
| Alpha 1.03 (aka 1.04) | Storage Engine (heap, tuples, scans) | 1.0.05 | ✅ Complete |
| Alpha 1.04 | Transaction Foundation (XID, TIP, MVCC, C/R) | 1.0.06 | ✅ Complete |
| Alpha 1.05 | Basic SQL Parser (baseline statements) | 1.0.07 | ✅ Complete |

## Stage 1 Overview (Planned)
- 1.1: Extended Storage (64K/128K, compression, TOAST/LOB)
- 1.2: Advanced SBLR (joins, subqueries, window functions)
- 1.3: Concurrency (multi-threaded buffer pool, lock manager, deadlock)
- 1.4: Advanced Indexes (bitmap, hash, parity path)
- 1.5: Embedded API
- 1.6: SBSQL Context-Aware Parser
- 1.7: sb_isql_a embedded CLI
- 1.8/1.9: Catch-up and hardening

## Guidance
- Use Stage codes going forward. Keep historical logs unchanged; reference this mapping.
- Update documents to use Stage 0 for completed foundation and Stage 1 for upcoming work.

## Action Items
1. Adjust `AUTHORITATIVE_IMPLEMENTATION_PLAN.md` to reference Stage 0 completion and Stage 1 plan.
2. Update `PROJECT_STATUS.md` to Stage 0 complete, Stage 1 planning.
3. Ensure tests and CI labels use Stage codes for new work.

## Summary for All Agents
Completed foundation work is consolidated under Stage 0 (1.0.xx). Stage 1 planning is approved.

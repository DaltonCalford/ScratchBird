# Alpha Completion Master Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## Purpose
Provide a single tracked plan for **Alpha completion** by consolidating the
remaining gaps identified in `ALPHA_BETA_SCOPE_STATUS.md` and the current
code‑truth audits.

This plan links to detailed sub‑plans but remains the **primary tracker** for
Alpha scope.

## Scope
- Core engine readiness (embedded/IPC/INET)
- V2 parser + PSQL execution completeness
- Resource loaders and i18n data coverage
- Index migration correctness and advanced index gaps (Alpha‑scoped)

## Sources
- `ScratchBird/docs/findings/ALPHA_BETA_SCOPE_STATUS.md`
- `ScratchBird/docs/specifications/parser/v3/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md`
- `ScratchBird/docs/specifications/parser/v3/planning/PLAN_V2_PARSER_COMPLETION.md`
- `ScratchBird/docs/specifications/parser/v3/planning/RESOURCES_I18N_TIMEZONE_REMEDIATION_PLAN.md`
- `ScratchBird/docs/specifications/parser/v3/planning/TRACKER_INDEX_SPEC_GAPS.md`
- `ScratchBird/docs/specifications/parser/v3/planning/ALPHA_CODE_TRUTH_AUDIT_2026-01-28.md`
- `ScratchBird/docs/specifications/parser/v3/planning/SBLR_TYPE_OPCODE_REMEDIATION_PLAN.md`

## Tracking Table

| Area | Task | Status | Owner | Milestone | References |
| --- | --- | --- | --- | --- | --- |
| Engine core | Tablespace routing defaults + root page allocation | Done |  | Alpha‑Core‑WS2 | `ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` |
| Engine core | Index migration safety (SPGIST/BITMAP/COLUMNSTORE/LSM) | Done |  | Alpha‑Core‑WS3 | `ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` |
| Engine core | Expression/partial index root allocation uses primary tablespace | Done |  | Alpha‑Core‑WS3 | Code: `ScratchBird/src/core/catalog_manager.cpp:7642` |
| Engine core | Monitoring parity (remaining MON$ placeholders) | Done |  | Alpha‑Core‑WS7 | Code: `ScratchBird/src/catalog/sys_catalog.cpp:173` |
| Engine core | Backup/restore parity (all tablespaces/catalogs) | Done |  | Alpha‑Core‑WS8 | Code: `ScratchBird/src/core/backup_manager.cpp:723` |
| Parser/PSQL | V2 parser completeness (DDL/DML/utility/PSQL) | Done |  | Alpha‑Parser‑P1 | `PLAN_V2_PARSER_COMPLETION.md` |
| PSQL runtime | PSQL bytecode emission + executor parity | Done |  | Alpha‑Parser‑P2 | `V2_PARSER_DDL_DML_PSQL_AUDIT.md` |
| SBLR/types | Missing type markers + typed literals | Done |  | Alpha‑Parser‑P3 | `SBLR_TYPE_OPCODE_REMEDIATION_PLAN.md` |
| Resources | Timezones/Charsets/Collations loaders + catalog | Done |  | Alpha‑I18N‑P1 | `RESOURCES_I18N_TIMEZONE_REMEDIATION_PLAN.md` |
| Indexes | Inverted GC, IVF, Zone Maps, GPID/TID checks | Done |  | Alpha‑Index‑P1 | `TRACKER_INDEX_SPEC_GAPS.md` |

## Execution Order (Suggested)
1. **Parser + PSQL end‑to‑end** (unblocks dialect parity and testing)
2. **Tablespace routing defaults** (core correctness)
3. **Index migration safety** (data integrity)
4. **Monitoring parity** (operational readiness)
5. **Backup/restore parity** (disaster recovery)
6. **Resources i18n loaders** (compatibility)
7. **Remaining index gaps** (performance/feature parity)

## Acceptance Criteria
- All rows in the Tracking Table are **Done**.
- `ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` shows all WS items complete.
- `PLAN_V2_PARSER_COMPLETION.md` acceptance checklist fully checked.
- i18n/timezone resources pass baseline conformance checks.
- Full build + `ctest` pass with standard skip list.

## Notes
- Beta scope is tracked separately in:
  `ScratchBird/docs/findings/ALPHA_BETA_SCOPE_STATUS.md`

## Progress Log
- 2026-01-29: Verified expression/partial index root allocation uses tablespace-aware GPIDs and completed full build + `ctest` pass (2490 tests).

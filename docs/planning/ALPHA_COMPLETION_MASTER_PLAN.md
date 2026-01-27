# Alpha Completion Master Plan

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
- `ScratchBird/docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md`
- `ScratchBird/docs/planning/PLAN_V2_PARSER_COMPLETION.md`
- `ScratchBird/docs/planning/RESOURCES_I18N_TIMEZONE_REMEDIATION_PLAN.md`
- `ScratchBird/docs/planning/TRACKER_INDEX_SPEC_GAPS.md`

## Tracking Table

| Area | Task | Status | Owner | Milestone | References |
| --- | --- | --- | --- | --- | --- |
| Engine core | Tablespace routing defaults + root page allocation | Done |  | Alpha‑Core‑WS2 | `ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` |
| Engine core | Index migration safety (SPGIST/BITMAP/COLUMNSTORE/LSM) | Done |  | Alpha‑Core‑WS3 | `ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` |
| Engine core | Expression/partial index root allocation uses primary tablespace | Open |  | Alpha‑Core‑WS3 | Code: `ScratchBird/src/core/catalog_manager.cpp:7513` |
| Engine core | Monitoring parity (remaining MON$ placeholders) | In Progress |  | Alpha‑Core‑WS7 | `ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` |
| Engine core | Backup/restore parity (all tablespaces/catalogs) | In Progress |  | Alpha‑Core‑WS8 | `ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` |
| Engine core | Restore only uses first tablespace file path (multi‑file tablespace) | Open |  | Alpha‑Core‑WS8 | Code: `ScratchBird/src/core/backup_manager.cpp:590` |
| Parser/PSQL | V2 parser completeness (DDL/DML/utility/PSQL) | In Progress |  | Alpha‑Parser‑P1 | `PLAN_V2_PARSER_COMPLETION.md` |
| PSQL runtime | PSQL bytecode emission + executor parity | Open |  | Alpha‑Parser‑P2 | `V2_PARSER_DDL_DML_PSQL_AUDIT.md` |
| Resources | Timezones/Charsets/Collations loaders + catalog | Open |  | Alpha‑I18N‑P1 | `RESOURCES_I18N_TIMEZONE_REMEDIATION_PLAN.md` |
| Indexes | Inverted GC, IVF, Zone Maps, GPID/TID checks | Open |  | Alpha‑Index‑P1 | `TRACKER_INDEX_SPEC_GAPS.md` |

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

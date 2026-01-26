[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Native (V2) - System Catalog Surface

Spec refs:
- `ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`
- `ScratchBird/docs/specifications/catalog/CATALOG_CORRECTION_PLAN.md`

## Catalog namespaces (spec-defined)
- `sys.catalog` (core object metadata)
- `sys.security` (users/roles/privileges)
- `sys.monitor` (runtime monitoring)
- `information_schema` (ANSI-compatible views)

## Implementation status
Status: Partial.

Notes:
- Catalog tables and views are managed by the catalog manager and stored on disk
  via MGA; coverage varies by object type.
- Information schema views are expected but not audited in this pass.
- Use `SHOW` commands (see `10_session_show_set.md`) for object summaries.

Implementation notes:
- GRANT/REVOKE: Fully implemented. Parser supports GRANT/REVOKE for privileges (table, column, schema level) and roles. Executor handles executeGrantPrivilege, executeRevokePrivilege, executeGrantRole, executeRevokeRole with WITH GRANT OPTION support.
- sys.jobs, sys.job_runs, sys.job_dependencies: Implemented and queryable (WS-4 complete)
- Some system tables flagged in `SYSTEM_CATALOG_STRUCTURE.md` may still need population validation.

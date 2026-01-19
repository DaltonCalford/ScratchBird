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

Spec delta:
- `SYSTEM_CATALOG_STRUCTURE.md` still flags GRANT/REVOKE and several system
  tables as not implemented; validate current catalog population against spec.

# Catalog Extract (Generated 2026-02-19)

This directory contains a full extract from a freshly bootstrapped ScratchBird database.

## Files
- `00_bootstrap_schema_nodes.tsv`: Canonical bootstrap schema nodes from source.
- `01_schema_tree.tsv`: Runtime schema tree from catalog API.
- `02_resolved_objects.tsv`: Runtime object resolver output.
- `03_catalog_page_occupancy.tsv`: Catalog page variable, page id, occupancy, and reference counts.
- `04_system_table_alias_map.tsv`: System logical table aliases (`logical -> sys.*record`).
- `05_catalog_outline.txt`: Combined schema/object outline with `[s]` and `[o]` markers.
- `06_system_table_aliases_from_source.tsv`: Source-of-truth alias map from `kSystemTableAliasMap`.
- `07_system_domain_by_column.tsv`: Column-to-domain typing map from `kSystemDomainByColumn`.
- `08_system_domain_by_table_column.tsv`: Conflict override typing map from `kSystemDomainByTableColumn`.
- `09_catalog_family_summary.tsv`: Family-level grouping summary derived from `03_catalog_page_occupancy.tsv`.
- `10_page_var_global_refs.tsv`: Repo-wide references for each catalog page variable (`src/`, `include/`, `tests/`).
- `AUDIT_SUMMARY.md`: Human-readable synthesis of structure, intent, occupancy, and usage findings.

## Notes
- `03_catalog_page_occupancy.tsv` reads low-level heap-page slot counts on a fresh bootstrap database.
- Resolver/bootstrap metadata is present in `01`/`02`/`05` even when low-level slot counts are zero.

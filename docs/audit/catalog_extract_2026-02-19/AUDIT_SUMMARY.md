# Catalog Audit Summary (2026-02-19)

## Scope
- Extract source: fresh bootstrapped database created by `CatalogFullExtractAudit.ExportCatalogStructureAndUsage`.
- Canonical runtime extract files: `00` through `10` in this directory.

## Canonical Catalog Tree (What Actually Exists)
- Use `01_schema_tree.tsv` as canonical schema hierarchy.
- Bootstrap/runtime schema nodes: 37 schemas + root (`38` lines including header).
- Runtime object resolver: `02_resolved_objects.tsv`.
- Combined view: `05_catalog_outline.txt`.

Key point:
- `root.sys.catalog.*` is **not** a runtime schema path in canonical tree.
- Entries like `sys.constraintinfo.on_update` are table-column type override keys, not schemas/objects.

## Intended Storage Semantics (What Each Element Is For)
- System table logical aliases (internal key -> spec-facing system record name):
  - `06_system_table_aliases_from_source.tsv`
  - Source map: `kSystemTableAliasMap` in `src/core/catalog_manager.cpp`.
- System column typing defaults (column name -> domain):
  - `07_system_domain_by_column.tsv`
  - Source map: `kSystemDomainByColumn`.
- Ambiguous per-table column typing overrides:
  - `08_system_domain_by_table_column.tsv`
  - Source map: `kSystemDomainByTableColumn`.

Interpretation:
- These maps define intended payload semantics for catalog records (key IDs, names, timestamps, flags, LOB refs, etc.).
- Do not model these keys as schema nodes.

## Data Presence (Fresh Bootstrap)
- Low-level page-chain scan:
  - `03_catalog_page_occupancy.tsv`
- Result on fresh bootstrap:
  - `191/191` catalog page families report `has_data = no`.
  - `189` page families report `note = ok`.
  - `2` page families report `read_failed_2002` (`database_table_page_`, `object_table_page_`).

Important caveat:
- Resolver/bootstrap metadata is present (`01`/`02`/`05`) despite zero low-level slot counts.
- This indicates runtime catalog state is populated even when low-level `record_count` scan does not show rows in this audit mode.

## Usage (What Code Touches It)
- Per-page variable usage and first references:
  - `03_catalog_page_occupancy.tsv`
  - Columns: `refs_catalog_cpp`, `refs_catalog_h`, `refs_executor_cpp`, `first_refs`
- Repo-wide references (`src/`, `include/`, `tests/`):
  - `10_page_var_global_refs.tsv`
- Family-level usage grouping:
  - `09_catalog_family_summary.tsv`

Observed usage pattern:
- Catalog families are predominantly implemented and referenced in:
  - `src/core/catalog_manager.cpp`
  - `include/scratchbird/core/catalog_manager.h`
- Domain manager additionally touches domain pages.
- No direct page-var references found in `src/sblr/executor.cpp` for this extract.

## Redesign Baseline Rules
- Treat `01_schema_tree.tsv` as authoritative namespace structure.
- Treat `06` + `07` + `08` as authoritative intent for system catalog semantics.
- Treat `03` occupancy as low-level physical signal only; do not use it alone to infer logical feature availability.
- Keep alias/type maps separate from schema tree to avoid path explosion and false `[s]/[o]` nodes.

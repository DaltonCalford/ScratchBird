# Emulated Catalog Analysis: MySQL 8.x

## Purpose
Identify how MySQL catalog and metadata surfaces map to ScratchBird canonical catalog data, and which surfaces are purely virtual overlays.

## Classification
- `canonical`: requires persisted ScratchBird catalog data.
- `virtual`: derived from canonical data.
- `runtime`: derived from runtime state.
- `gated`: exposed only if feature is enabled.

## Primary MySQL Catalog Surfaces
MySQL’s public metadata surfaces are `information_schema.*` and `performance_schema.*`. Internal Data Dictionary (DD) tables are implementation details and do not require separate ScratchBird storage.

## Mapping Table (Information Schema)
| MySQL surface | Purpose | SB source | Storage class | Notes |
| --- | --- | --- | --- | --- |
| information_schema.SCHEMATA | Schemas | `schema` | canonical | Schema registry. |
| information_schema.TABLES | Tables/views | `table`, `view` | canonical | Relation metadata. |
| information_schema.COLUMNS | Column metadata | `column` | canonical | Column definitions. |
| information_schema.STATISTICS | Indexes | `index`, `index_column`, `index_stats` | canonical | Index metadata. |
| information_schema.TABLE_CONSTRAINTS | Constraints | `table_constraint` | canonical | Table constraints. |
| information_schema.KEY_COLUMN_USAGE | Key usage | `fk_constraint`, `index_column` | canonical | FK and PK mapping. |
| information_schema.REFERENTIAL_CONSTRAINTS | FK details | `fk_constraint` | canonical | FK metadata. |
| information_schema.CHECK_CONSTRAINTS | CHECK constraints | `check_constraint` | canonical | Check constraints. |
| information_schema.ROUTINES | Functions/procedures | `function`, `procedure` | canonical | Routine registry. |
| information_schema.PARAMETERS | Routine parameters | `function_param`, `procedure_param` | canonical | Param definitions. |
| information_schema.TRIGGERS | Triggers | `trigger` | canonical | Trigger registry. |
| information_schema.VIEWS | Views | `view` | canonical | View definitions. |
| information_schema.EVENTS | Events | `event` | canonical | Event scheduler metadata. |
| information_schema.PARTITIONS | Partitions | `partition` | canonical | Partition metadata. |
| information_schema.TABLESPACES | Tablespaces | `filespaces` | canonical | Filespace mapping. |
| information_schema.FILES | Files | `filespace_files` | canonical | Filespace files. |
| information_schema.ENGINES | Storage engines | `storage_engine` | virtual | Exposed as supported engine list. |
| information_schema.PLUGINS | Plugins | `sys.plugin` | virtual | Supported plugins list. |
| information_schema.COLLATIONS | Collations | `collation` | canonical | Collation registry. |
| information_schema.CHARACTER_SETS | Charsets | `charset` | canonical | Charset registry. |
| information_schema.USER_PRIVILEGES | Global privileges | `permission` | canonical | Privilege graph. |
| information_schema.SCHEMA_PRIVILEGES | Schema privileges | `permission` | canonical | Privilege graph. |
| information_schema.TABLE_PRIVILEGES | Table privileges | `permission` | canonical | Privilege graph. |
| information_schema.COLUMN_PRIVILEGES | Column privileges | `permission` | canonical | Privilege graph. |

## Mapping Table (Performance Schema)
| MySQL surface | Purpose | SB source | Storage class | Notes |
| --- | --- | --- | --- | --- |
| performance_schema.* | Runtime instrumentation | runtime metrics | runtime | Expose as runtime views mapped to SB metrics/diagnostics tables. |

## Notes
- The MySQL Data Dictionary tables (internal `mysql.*` DD tables) are **not** user-facing; they can be mapped from ScratchBird canonical data without separate storage.
- Any MySQL-specific metadata not supported in alpha is exposed as empty or gated views, with dialect-appropriate errors where required.

## Resolved Decisions
- `information_schema.EVENTS` support is required in Alpha and is backed by `event`.
- Partition metadata coverage for Alpha must include all fields required to populate `information_schema.PARTITIONS` for MySQL 8.x using `partitioned_table`, `partition`, and related stats tables.

## Open Questions
- None.

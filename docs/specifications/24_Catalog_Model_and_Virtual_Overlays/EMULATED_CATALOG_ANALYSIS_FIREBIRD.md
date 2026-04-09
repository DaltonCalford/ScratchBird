# Emulated Catalog Analysis: Firebird 5.x

## Purpose
Identify which Firebird system relations must be backed by canonical ScratchBird catalog data versus which can be exposed as purely virtual overlays derived from existing catalog or runtime state.

## Classification
- `canonical`: requires persisted ScratchBird catalog data.
- `virtual`: derived from canonical data or runtime state; no Firebird-specific storage.
- `runtime`: derived exclusively from runtime state (session/transaction/allocator) and not stored.

## Mapping Summary
The Firebird system relations are persistent in Firebird, but **ScratchBird does not need Firebird-specific physical tables**. Each relation is exposed as a virtual overlay over canonical ScratchBird catalog data (or runtime state). The only question is whether the *underlying canonical data* must be persisted.

## Relation Mapping
| Firebird relation | Purpose | SB source | Storage class | Notes |
| --- | --- | --- | --- | --- |
| RDB$PAGES | System page tracking | allocator/page-metadata | runtime | Virtual view from allocator stats; no Firebird-specific storage. |
| RDB$DATABASE | Database header metadata | `database` | canonical | 1 row per database. |
| RDB$FIELDS | Domain/field definitions | `domain`, `type` | canonical | Domains + types. |
| RDB$INDEX_SEGMENTS | Index key segments | `index_column` | canonical | Column order and expressions. |
| RDB$INDICES | Index definitions | `index` | canonical | Includes index expressions and conditions. |
| RDB$RELATION_FIELDS | Table columns | `column` | canonical | Column metadata. |
| RDB$RELATIONS | Table/view registry | `table`, `view` | canonical | Relation + view metadata. |
| RDB$VIEW_RELATIONS | View dependencies | `dependency` | canonical | Derived from dependency graph. |
| RDB$FORMATS | Record format versions | record-format registry | virtual | Derived from internal format registry. |
| RDB$SECURITY_CLASSES | Security class ACLs | `security_class` | canonical | Backed by security model. |
| RDB$FILES | Database and secondary files | `filespace_files` | canonical | Filespace metadata. |
| RDB$TYPES | Enumerated type metadata | `type` + enum dictionary | virtual | Expose as derived view of enum registry. |
| RDB$TRIGGERS | Trigger metadata | `trigger` | canonical | Trigger definitions. |
| RDB$DEPENDENCIES | Object dependency graph | `dependency` | canonical | Used for invalidation and view dependencies. |
| RDB$FUNCTIONS | Function metadata | `function` | canonical | Includes UDF/UDR entries. |
| RDB$FUNCTION_ARGUMENTS | Function parameters | `function_param` | canonical | Argument definitions. |
| RDB$FILTERS | BLOB filters | `blob_filter` | canonical | Requires canonical table if filters are supported. |
| RDB$TRIGGER_MESSAGES | Trigger message catalog | `trigger_message` | canonical | If trigger messages are supported. |
| RDB$USER_PRIVILEGES | Grants | `permission` | canonical | Privilege graph. |
| RDB$TRANSACTIONS | Limbo/2PC metadata | `prepared_transaction` | virtual | Overlay of 2PC/limbo transactions; empty unless 2PC enabled. |
| RDB$GENERATORS | Sequences/generators | `sequence` | canonical | Sequences. |
| RDB$FIELD_DIMENSIONS | Array dimension metadata | `domain_parameter` | canonical | Arrays stored as domain parameters. |
| RDB$RELATION_CONSTRAINTS | Table constraints | `table_constraint` | canonical | Primary/unique/check/foreign. |
| RDB$REF_CONSTRAINTS | FK constraints | `fk_constraint` | canonical | FK mapping. |
| RDB$CHECK_CONSTRAINTS | CHECK constraints | `check_constraint` | canonical | Expression/definition. |
| RDB$LOG_FILES | Firebird log file config | `sys.config.value` | virtual | SB has no WAL; expose empty or config-derived view. |
| RDB$PROCEDURES | Stored procedures | `procedure` | canonical | Procedure metadata. |
| RDB$PROCEDURE_PARAMETERS | Procedure parameters | `procedure_param` | canonical | Param metadata. |
| RDB$CHARACTER_SETS | Charsets | `charset` | canonical | System registry. |
| RDB$COLLATIONS | Collations | `collation` | canonical | System registry. |
| RDB$EXCEPTIONS | User-defined exceptions | `exception` | canonical | Exception definitions. |
| RDB$ROLES | Role definitions | `role` | canonical | Role registry. |
| RDB$BACKUP_HISTORY | Backup history | `backup_history` | canonical | May also be a view over audit log if desired. |
| RDB$PACKAGES | Package headers/bodies | `package` | canonical | Firebird packages. |
| RDB$AUTH_MAPPING | Auth mapping | `auth_mapping` | canonical | Auth mapping rules. |
| RDB$DB_CREATORS | DB creator list | `db_creator` | canonical | DB creator registry. |
| RDB$PUBLICATIONS | Publication definitions | `publication` | canonical | Publication metadata. |
| RDB$PUBLICATION_TABLES | Publication membership | `publication_table` | canonical | Publication membership. |
| RDB$SCHEMAS | Schema definitions | `schema` | canonical | Schema registry.

## Notes
- Firebird virtual relations `MON$*`, `SEC$*`, `RDB$CONFIG`, `RDB$KEYWORDS`, `RDB$TIME_ZONES` remain virtual overlays (see `FIREBIRD_CATALOG_ONDISK_OBJECTS.md`).
- Even when Firebird marks a relation as persistent, ScratchBird can still expose it as a view over canonical data. No Firebird-specific storage is required.

## Resolved Decisions
- `blob_filter` and `trigger_message` are dedicated canonical tables (see `CATALOG_TABLE_SCHEMA_ENGINE_SPECIFIC.md`).
- `RDB$BACKUP_HISTORY` is backed by persisted `backup_history`; audit events in `audit_log` are supplemental and not a replacement.

## Open Questions
- None.

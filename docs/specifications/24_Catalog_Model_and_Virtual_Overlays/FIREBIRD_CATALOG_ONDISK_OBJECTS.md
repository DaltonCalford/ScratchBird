# Firebird 5.x On-Disk System Objects (Catalog)

## Purpose
Define the canonical ScratchBird backing for every Firebird `rel_persistent` system relation used in Firebird 5.x emulation.

## Canonical Mapping Rules
1. Firebird catalog names are parser-facing overlays only.
2. Backing data is stored in canonical `sb_*` tables (or derived runtime/config overlays where explicitly noted).
3. No Firebird-specific physical catalog table names are created on disk.

## Persistent System Relations (Resolved)
| Firebird Relation | ODS | Canonical ScratchBird Backing | Storage/Exposure |
| --- | --- | --- | --- |
| `RDB$PAGES` | `ODS_8_0` | allocator/page metadata runtime view | virtual overlay |
| `RDB$DATABASE` | `ODS_8_0` | `database` | canonical |
| `RDB$FIELDS` | `ODS_8_0` | `domain`, `type`, `domain_parameter` | canonical |
| `RDB$INDEX_SEGMENTS` | `ODS_8_0` | `index_column` | canonical |
| `RDB$INDICES` | `ODS_8_0` | `index` | canonical |
| `RDB$RELATION_FIELDS` | `ODS_8_0` | `column` | canonical |
| `RDB$RELATIONS` | `ODS_8_0` | `table`, `view`, `object` | canonical |
| `RDB$VIEW_RELATIONS` | `ODS_8_0` | `dependency` | canonical |
| `RDB$FORMATS` | `ODS_8_0` | `object_definition` | canonical |
| `RDB$SECURITY_CLASSES` | `ODS_8_0` | `security_class` | canonical |
| `RDB$FILES` | `ODS_8_0` | `filespace_files` | canonical |
| `RDB$TYPES` | `ODS_8_0` | `type` | canonical |
| `RDB$TRIGGERS` | `ODS_8_0` | `trigger` | canonical |
| `RDB$DEPENDENCIES` | `ODS_8_0` | `dependency` | canonical |
| `RDB$FUNCTIONS` | `ODS_8_0` | `function` | canonical |
| `RDB$FUNCTION_ARGUMENTS` | `ODS_8_0` | `function_param` | canonical |
| `RDB$FILTERS` | `ODS_8_0` | `blob_filter` | canonical |
| `RDB$TRIGGER_MESSAGES` | `ODS_8_0` | `trigger_message` | canonical |
| `RDB$USER_PRIVILEGES` | `ODS_8_0` | `permission`, `object_permission`, `column_permission` | canonical |
| `RDB$TRANSACTIONS` | `ODS_8_0` | `transaction`, `prepared_transaction` | canonical |
| `RDB$GENERATORS` | `ODS_8_0` | `sequence` | canonical |
| `RDB$FIELD_DIMENSIONS` | `ODS_8_0` | `type_modifier` | canonical |
| `RDB$RELATION_CONSTRAINTS` | `ODS_8_0` | `table_constraint` | canonical |
| `RDB$REF_CONSTRAINTS` | `ODS_8_0` | `fk_constraint` | canonical |
| `RDB$CHECK_CONSTRAINTS` | `ODS_8_0` | `check_constraint` | canonical |
| `RDB$LOG_FILES` | `ODS_8_0` | `sys.config.value` (derived) | virtual overlay |
| `RDB$PROCEDURES` | `ODS_8_0` | `procedure` | canonical |
| `RDB$PROCEDURE_PARAMETERS` | `ODS_8_0` | `procedure_param` | canonical |
| `RDB$CHARACTER_SETS` | `ODS_8_0` | `charset` | canonical |
| `RDB$COLLATIONS` | `ODS_8_0` | `collation` | canonical |
| `RDB$EXCEPTIONS` | `ODS_8_0` | `exception` | canonical |
| `RDB$ROLES` | `ODS_9_0` | `role` | canonical |
| `RDB$BACKUP_HISTORY` | `ODS_11_0` | `backup_history` | canonical |
| `RDB$PACKAGES` | `ODS_12_0` | `package`, `package_member` | canonical |
| `RDB$AUTH_MAPPING` | `ODS_12_0` | `auth_mapping` | canonical |
| `RDB$DB_CREATORS` | `ODS_12_0` | `db_creator` | canonical |
| `RDB$PUBLICATIONS` | `ODS_13_0` | `publication` | canonical |
| `RDB$PUBLICATION_TABLES` | `ODS_13_0` | `publication_table` | canonical |
| `RDB$SCHEMAS` | `ODS_14_0` | `schema` | canonical |

## Virtual (Non-Persistent) Firebird Relations
- `MON$*` monitoring relations are runtime overlays.
- `SEC$*` security views are overlays over canonical security tables.
- `RDB$CONFIG`, `RDB$KEYWORDS`, and `RDB$TIME_ZONES` are virtual overlays from canonical config and resource catalogs.

## Deterministic Requirements
1. Every Firebird relation listed above must have a deterministic overlay mapping in parser metadata.
2. Overlay queries must reference only canonical UUID-backed objects in engine calls.
3. Any unsupported column in a Firebird relation must return deterministic dialect-compatible null/default behavior as specified by profile rules.

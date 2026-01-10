# Plan 02 - UUID Resolution, Rename, and Schema Move

## Scope
Implement deterministic UUID <-> path resolution and complete rename/move support across core SQL objects, including table-scoped objects and global domains, with opcode-aware SBLR support for all parsers.

## Priority
P0 (required for dependency tracking, security auditing, schema navigation, and catalog correctness).

## Status (Current)
Completed:
- Resolver cache APIs implemented: getSchemaPath/createSchemaPath/resolveObjectPath/listResolvedObjects, plus object_resolver virtual view.
- EXT_RENAME_OBJECT / EXT_MOVE_OBJECT handlers wired end-to-end (v2 generator + executor).
- Parser integrations:
  - ScratchBird v2: ALTER ... RENAME TO / SET SCHEMA across core object types.
  - PostgreSQL: emits EXT_RENAME_OBJECT / EXT_MOVE_OBJECT.
  - MySQL: RENAME TABLE and ALTER TABLE RENAME TO (schema-qualified maps to MOVE).
  - Firebird: column rename via ALTER [COLUMN] <col> TO <new>, domain rename via ALTER DOMAIN TO.
- SHOW LOCATION/RESOLVED/OBJECTS/SCHEMA_TREE use resolver output.
- Catalog persistence + rename/move disk updates added for sequences, views, triggers, functions, procedures, synonyms, and foreign tables.
- Unqualified name resolution now prefers current schema before search_path (aligned with schema path spec) with unit coverage.
- Rename/move resolver tests for table/column paths (including restart rebuild) added.
- Column rename scans catalog heap page chains (covered by CatalogRenameMoveTest.RenameColumnUpdatesResolver).
- Rename/move resolver tests added for sequences, views, synonyms, foreign tables, functions, procedures, packages, UDRs, and exceptions; cross-type ambiguity resolution is now covered.
- Rename/move resolver tests added for table-scoped index/trigger/constraint objects, with constraint rename persistence coverage.
- Resolver rebuild after restart now covers table-scoped (index/trigger/constraint) and schema-scoped (synonym/foreign table/package/UDR/exception) objects.
- Executor DROP/ALTER TABLE and CREATE/DROP INDEX now resolve schema via resolveSchemaIdForQualifiedName, with unit coverage for current-schema resolution.
- name_is_delimited is now persisted for columns, constraints, synonyms, foreign tables, packages, UDRs, and exceptions; resolver rebuild uses the stored flags with persistence tests for column/constraint.
- getSchema(string) now respects name_is_delimited during path resolution, with unit coverage for delimited schema names.
- Executor CREATE/TRUNCATE TABLE now covered for current-schema resolution via unit tests.
- ALTER TABLE semantic analysis now covers ADD/DROP/ALTER COLUMN plus SET TABLESPACE and ENABLE/DISABLE RLS (constraint operations still error).
- Executor DML + SHOW table resolution now uses schema path resolution (no PUBLIC default).
- Resolver path lookup now honors delimited schema/object names before case-folded lookup.
- Tablespace records load on startup so resolver includes tablespaces after restart.

Outstanding: None.

Emulated parser constraints (by design):
- Firebird parser is limited to Firebird-valid rename semantics (no SET SCHEMA / no object-level rename beyond domain and column).
- MySQL parser is limited to RENAME TABLE / ALTER TABLE RENAME TO (no SET SCHEMA).

## References
- `docs/specifications/DDL_SCHEMAS.md`
- `docs/specifications/DDL_TABLES.md`
- `docs/specifications/DDL_VIEWS.md`
- `docs/specifications/DDL_INDEXES.md`
- `docs/specifications/DDL_SEQUENCES.md`
- `docs/specifications/DDL_FUNCTIONS.md`
- `docs/specifications/DDL_PROCEDURES.md`
- `docs/specifications/DDL_DOMAINS.md`
- `docs/specifications/DDL_TRIGGERS.md`
- `docs/specifications/DDL_ROLES_AND_GROUPS.md`
- `docs/specifications/02_DDL_STATEMENTS_OVERVIEW.md`
- `docs/specifications/SYSTEM_CATALOG_STRUCTURE.md`
- `docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `docs/specifications/Appendix_A_SBLR_BYTECODE.md`
- `docs/archive/2026-01-09/findings/engine_gap_report.md` (UUID resolver gap)

## Order of Implementation
1) Schema path resolution and caches (schema hierarchy support).
2) Unified resolver cache and APIs (UUID <-> path).
3) Rename and move operations in CatalogManager for all object types.
4) SBLR opcodes + executor handlers (rename/move).
5) Parser and compiler integration (ScratchBird v2, Firebird, MySQL, PostgreSQL).
6) SHOW/diagnostic commands updated to use resolver.

## Decision Gates (Resolved)
- Resolver storage: in-memory resolver cache + indexes (hash on UUID, tree index on namespace keys) + virtual resolver view (MON$-style). No on-disk resolver table in alpha.
- Schema path source: derived from SchemaRecord.parent_schema_id; do not add schema_path columns to catalog tables.
- Domain scope: domains are global (database/cluster wide). Treat schema_id for domains as root/zero and ignore schema scope for resolution.
- Name uniqueness:
  - Schema-scoped objects: unique within schema_id.
  - Table-scoped objects (index/trigger/constraint/column): unique within table_id.
  - Schema names: unique within parent_schema_id.
- SBLR encoding: bump SBLR_VERSION to 2; extended opcode encoding uses 16-bit opcodes after EXTENDED_OPCODE (0xFF); add EXT_RENAME_OBJECT and EXT_MOVE_OBJECT as 16-bit extended opcodes.

## Known Stub/Partial Areas
Resolved for Plan 02 scope; no remaining known stubs.

## Required Data/Schema Changes
- No schema_path columns are added to sys.catalog.tables/sys.catalog.views/etc. Schema path is computed.
- Add/maintain in-memory lookup indexes for:
  - schema: (parent_schema_id, name) -> schema_id
  - schema-scoped objects: (schema_id, object_type, name) -> object_id
  - table-scoped objects: (table_id, object_type, name) -> object_id
- Domains must expose dialect_tag and compat_name for resolution; if DomainRecord lacks these fields, extend the record and update read/write converters before implementing resolver logic.
- If name_is_delimited is not persisted in on-disk records, treat it as false on load and document that case-sensitive identifiers are not persisted in alpha.
- Add a virtual system view `sys.catalog.object_resolver` (MON$-style) backed by resolver cache, not persisted on disk.

## Resolver Cache Layout (Concrete)
Add to CatalogManager:
- struct ResolvedObject:
  - ID object_id
  - ObjectType object_type
  - ID schema_id           // zero for global objects
  - ID parent_object_id    // table_id for index/trigger/constraint/column
  - std::string object_name
  - std::string schema_path  // dotted schema path (computed)
  - std::string full_path    // schema_path + "." + object_name (or schema.table.object for table-scoped)
  - std::string dialect_tag  // domains only (empty otherwise)
  - std::string compat_name  // domains only (empty otherwise)
- struct ResolverKey:
  - ID scope_id (schema_id or table_id depending on object_type)
  - ObjectType object_type
  - std::string normalized_name
  - bool name_is_delimited
- Caches:
  - resolver_by_id_: unordered_map<ID, ResolvedObject>
  - resolver_by_name_: std::map<ResolverKey, ID> (tree index for prefix/range scans)
  - schema_name_lookup_: unordered_map<pair<ID, std::string>, ID> using PairHash

## Path Resolution Algorithm (No Ambiguity)
- getSchemaPath(schema_id):
  - Walk parent_schema_id until zero/invalid.
  - Detect cycles (track visited IDs); on cycle -> corruption error.
  - Build components in reverse; join with '.'.
  - Cache into SchemaInfo.full_path.
- resolveSchemaPath(path_type, components, current_schema_id):
  - ABSOLUTE: start from root (zero UUID).
  - CURRENT: start from current_schema_id.
  - PARENT: start from parent of current_schema_id; if no parent -> error.
  - For each component, find child schema with parent_schema_id == current and IdentifierUtils::namesMatch.
  - If any component missing -> NOT_FOUND.
- resolveObjectPath(path):
  - Determine namespace:
    - If object_type is DOMAIN/ROLE/USER/GROUP/TABLESPACE/DATABASE: ignore schema path, resolve globally.
    - If table-scoped (INDEX/TRIGGER/CONSTRAINT/COLUMN): require at least [table, object] components; schema path is optional.
  - For UNQUALIFIED: search in connection search_path (ordered list of schema paths); if ambiguous (found in >1 schema), return AMBIGUOUS error.
  - For ABSOLUTE/CURRENT/PARENT: resolve schema path first, then object name.
  - For table-scoped: resolve table first, then resolve child object within table_id.

## CatalogManager API (Exact)
Implement or add:
- Status getSchemaPath(const ID& schema_id, std::string& path_out, ErrorContext* ctx)
- Status createSchemaPath(const std::string& path, SchemaType type, ID& leaf_schema_id_out, ErrorContext* ctx)
- Status resolveObjectPath(const ObjectPath& path, ObjectType expected_type, const ResolveOptions& opts,
                           ID& object_id_out, ObjectType& type_out, ErrorContext* ctx)
- Status resolveObjectId(const ID& object_id, ResolvedObject& out, ErrorContext* ctx)
- Status listResolvedObjects(const ResolveFilter& filter, std::vector<ResolvedObject>& out, ErrorContext* ctx)

New helper types in core (not parser):
- enum class PathType : uint8_t { UNQUALIFIED=0, CURRENT=1, PARENT=2, ABSOLUTE=3 }
- struct ObjectPath { PathType type; std::vector<std::string> components; }

ResolveOptions:
- bool allow_ambiguity = false
- bool follow_synonyms = false
- std::string dialect_tag = "scratchbird"

## Implementation Notes (Concrete)
- Name normalization for resolver keys:
  - If name_is_delimited is false: normalized_name = IdentifierUtils::toUpper(name).
  - If name_is_delimited is true: normalized_name = name as-is.
- Schema lookup:
  - Replace the current std::tolower() comparison in getSchema(string) with IdentifierUtils::namesMatch and full path resolution.
  - Allow full dotted schema paths in getSchema(string); treat strings with '.' as absolute schema paths.
- Search path:
  - ConnectionContext::search_path entries are treated as schema paths (not simple names).
  - For UNQUALIFIED resolution, resolve each search_path entry to schema_id with resolveSchemaPath before lookup.
- Dialect tag:
  - Add ConnectionContext::dialect_tag (string) with getter/setter.
  - Set dialect_tag in protocol handlers and query compilers (scratchbird, firebird, mysql, postgresql).
  - Resolver uses opts.dialect_tag or ConnectionContext::dialect_tag for domain lookups.
- Object type filtering:
  - If expected_type == ObjectType::UNKNOWN, search all object types in scope; otherwise, only the expected type.
- Domain resolution:
  - First try (domain_name, dialect_tag); if not found, try compat_name == domain_name.
  - If multiple matches by compat_name, return AMBIGUOUS.
- Virtual resolver view:
  - Implement `sys.catalog.object_resolver` as a synthetic system view (similar to Firebird MON$ tables).
  - Source rows from CatalogManager::listResolvedObjects; no on-disk catalog record required.

## Rename/Move Implementation Map (Explicit)
Use updateRecordInHeapPage(...) with matcher by ID to ensure MGA update-in-place semantics.

Schema-scoped objects (schema_id + name):
- TableRecord: table_name, schema_id, last_modified_time
- ViewRecord: view_name, schema_id (add last_modified_time to record if missing)
- SequenceRecord: sequence_name, schema_id (add last_modified_time to record if missing)
- Function/Procedure records: function_name/procedure_name, schema_id
- PackageRecord, UDRRecord, ExceptionRecord, SynonymRecord, ForeignTableRecord: name + schema_id

Table-scoped objects (table_id + name):
- IndexRecord: index_name (schema from parent table)
- TriggerRecord: trigger_name (schema from parent table)
- ConstraintRecord, ColumnRecord: constraint_name/column_name

Global objects:
- DomainRecord: domain_name (schema ignored); enforce dialect_tag/compat_name uniqueness
- RoleRecord, UserRecord, GroupRecord, TablespaceRecord: name only (no schema)

For each rename/move:
- Validate new name with UTF8Utils::validateStorageCapacity.
- Check name conflicts in the appropriate namespace using IdentifierUtils::namesConflict.
- Update in-memory cache and name lookup maps.
- Update resolver cache entry and full_path for any descendants (table -> indexes/triggers/constraints/columns).
- Persist record with updateRecordInHeapPage.
- On failure, rollback cache changes to old values.
- Existing renameColumn and renameTablespace must also update resolver caches and name lookup maps.

Move rules:
- Only schema-scoped objects can move.
- Table move updates schema_id in TableRecord and recomputes full_path for child objects.
- Domains cannot be moved (global).
- Table-scoped objects cannot move directly (move the parent table).

## SBLR Encoding (Opcode-Aware)
Bump SBLR_VERSION to 2 and extend extended-opcode encoding:
- `include/scratchbird/sblr/opcodes.h`: set `SBLR_VERSION = 2`.
- Encoding rules:
  - Version 1: `0xFF` + `uint8` ext opcode (legacy).
  - Version 2+: `0xFF` + `uint16` ext opcode (little-endian).
- Add new `enum class ExtendedOpcode : uint16_t` and update helpers to write/read 16-bit extended opcodes.
- Define new 16-bit extended opcodes:
  - EXT_RENAME_OBJECT = 0x0100
  - EXT_MOVE_OBJECT = 0x0101
- All compilers/parsers must emit SBLR_VERSION=2 in the bytecode header.

Payload encoding (both extended opcodes):
- uint8 flags
  - bit0: HAS_UUID
  - bit1: IF_EXISTS
- uint8 object_type (CatalogManager::ObjectType values)
- if HAS_UUID: 16-byte UUID
- ObjectPath payload:
  - uint8 path_type (PathType values)
  - uint8 component_count
  - for each component: uint16 length + bytes (UTF-8)
- For EXT_RENAME_OBJECT:
  - uint16 new_name_len + bytes
- For EXT_MOVE_OBJECT:
  - ObjectPath payload for target schema (components are schema only)
  - uint16 new_name_len + bytes (0 length means keep name)

Executor behavior:
- If HAS_UUID: use object_id directly; skip path resolution.
- Else: resolve using CatalogManager::resolveObjectPath with PathType and components.
- If IF_EXISTS and not found: no-op.
- When `Opcode::EXTENDED_OPCODE` is encountered, read 16-bit ext opcode when version >=2; read 8-bit ext opcode for version 1.

## Parser/Compiler Integration (Exact File Targets)
ScratchBird v2:
- `src/parser/parser_v2.cpp`:
  - Add ALTER TABLE ... SET SCHEMA parsing.
  - Add ALTER VIEW/INDEX/SEQUENCE/DOMAIN/TRIGGER/FUNCTION/PROCEDURE/PACKAGE/SCHEMA parsing for RENAME TO and SET SCHEMA.
  - Produce new AST nodes for Rename/Move (or reuse generic AlterObjectStmt).
- `src/sblr/semantic_analyzer_v2.cpp`:
  - Implement analysis for rename/move statements.
  - Resolve object UUID when possible; store in resolved node.
- `src/sblr/bytecode_generator_v2.cpp`:
  - Emit EXT_RENAME_OBJECT / EXT_MOVE_OBJECT (16-bit extended opcodes) with path payload.
  - Set HAS_UUID when resolved UUID is available.

Firebird:
- `src/parser/firebird/firebird_parser.cpp`:
  - Map ALTER TABLE/VIEW/PROCEDURE/FUNCTION/DOMAIN/SEQUENCE/TRIGGER RENAME TO, SET SCHEMA to new AST nodes.
- Firebird compiler uses v2 semantic analyzer; ensure rename/move nodes are covered.

MySQL:
- `src/parser/mysql/mysql_parser.cpp`:
  - Implement RENAME TABLE and ALTER TABLE RENAME TO.
  - Emit EXT_RENAME_OBJECT / EXT_MOVE_OBJECT directly (MySQL parser is direct SBLR).

PostgreSQL:
- `src/parser/postgresql/pg_parser_ddl.cpp`:
  - Implement ALTER ... RENAME TO and ALTER ... SET SCHEMA.
  - Emit EXT_RENAME_OBJECT / EXT_MOVE_OBJECT directly.

## Executor Updates (Exact)
- `src/sblr/executor.cpp`:
  - Add handlers for EXT_RENAME_OBJECT and EXT_MOVE_OBJECT in executeExtendedOpcode dispatch.
  - Replace hard-coded PUBLIC schema for rename/move and SHOW LOCATION/RESOLVED with resolver API.
  - Use ConnectionContext search_path/current_schema for unqualified names.

## Resolver Integration for SHOW/Diagnostics
Expose resolver cache as a virtual system view (MON$-style):
- `sys.catalog.object_resolver` (virtual, read-only, not persisted)
- Columns: object_id, object_type, schema_path, full_path, object_name, dialect_tag, compat_name

Update these commands to use resolver cache (and/or the virtual view) instead of manual schema/table scans:
- EXT_SHOW_LOCATION
- EXT_SHOW_RESOLVED
- EXT_SHOW_OBJECTS
- EXT_SHOW_SCHEMA_TREE (use full schema hierarchy; no "TODO" placeholder)

Output columns for SHOW LOCATION/RESOLVED:
- object_id (UUID)
- object_type
- schema_path
- full_path
- object_name

## Completion Checklist (Developer)
- [ ] Schema path resolution implemented and cached.
- [ ] Resolver cache built on startup and maintained on create/drop/rename/move.
- [ ] rename/move implemented for all schema-scoped and table-scoped objects.
- [ ] Global domains resolve by name + dialect_tag/compat_name.
- [ ] EXT_RENAME_OBJECT and EXT_MOVE_OBJECT implemented end-to-end (16-bit extended opcodes).
- [ ] ScratchBird v2, Firebird, MySQL, PostgreSQL parsers emit new opcodes.
- [ ] SHOW LOCATION/RESOLVED use resolver cache.
- [ ] `sys.catalog.object_resolver` virtual view exposed and matches resolver output.

## Completion Checklist (Auditor)
- [ ] UUID -> path returns correct schema path and full_path for all object types.
- [ ] Path -> UUID resolution is correct for unqualified, current, parent, and absolute paths.
- [ ] Rename/move does not change UUIDs and does not break dependency graph.
- [ ] Table-scoped objects resolve under correct parent table namespace.
- [ ] Ambiguous search path resolution returns explicit error.
- [ ] Parser outputs are consistent across dialects (SBLR payload matches spec).

## Testing Requirements
- Unit tests for schema path resolution (hierarchy, current/parent/absolute).
- Resolver tests for each object type (table, view, sequence, index, trigger, column, domain).
- Rename/move tests per object type with conflict and IF EXISTS cases.
- SBLR opcode tests for rename/move across parsers.
- Restart tests: resolver cache rebuild matches on-disk catalog after rename/move.
- Virtual view tests: `SELECT * FROM sys.catalog.object_resolver` returns expected rows and matches resolver API.

## Concrete Test Cases
- Create schemas: app, app.sales, app.sales.international. Validate getSchemaPath and resolveObjectPath for .sales and ..sales.
- Create two schemas in search_path with same table name; unqualified resolve returns AMBIGUOUS.
- Rename table with indexes/triggers/constraints; verify full_path updates for all children.
- Move table to new schema; verify child objects resolve via new schema path.
- Rename domain (global); resolve by dialect_tag and compat_name.
- MySQL: RENAME TABLE old TO new; verify UUID unchanged and SHOW LOCATION returns new path.
- Firebird: ALTER TABLE t RENAME TO t2; verify resolver and executor.
- PostgreSQL: ALTER VIEW v SET SCHEMA s2; verify resolver and executor.
- Restart DB; ensure resolver cache reflects renamed/moved objects.
- Query `sys.catalog.object_resolver`; verify rows and paths for renamed/moved objects.

## Acceptance Criteria
- Resolver returns correct object_id/type/path in all cases.
- Rename/move preserves UUID and dependencies across all object types.
- All parsers generate opcode-correct SBLR for rename/move.
- SHOW LOCATION/RESOLVED always matches resolver output.
- `sys.catalog.object_resolver` results match resolver API output.

## Checkpoints (Low-Capability AI)
- Checkpoint A: Schema path resolution and getSchemaPath/createSchemaPath tests pass.
- Checkpoint B: Resolver caches and resolveObjectPath tests pass.
- Checkpoint C: CatalogManager rename/move for core objects pass tests.
- Checkpoint D: SBLR opcodes + executor handlers pass unit tests.
- Checkpoint E: Parser coverage (v2/firebird/mysql/postgresql) passes integration tests.

## Common Failure Patterns
- Resolver uses schema_name only (ignores parent_schema_id) leading to wrong path in hierarchy.
- Resolver caches updated but on-disk records not updated (restart breaks behavior).
- Table rename updates table name but not child object full_path (indexes/triggers/constraints).
- Schema move updates object record but leaves name lookup maps stale.
- Executor resolves names using PUBLIC instead of search_path/current_schema.
- Parsers emit rename ops without the correct opcode payload shape.
- Extended opcode width mismatch (8-bit vs 16-bit) causing invalid bytecode or executor crashes.

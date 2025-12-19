# Plan 02 - UUID Resolution, Rename, and Schema Move

## Scope
Implement unified UUID ↔ name/path resolver and object rename/move support across object types, including global (non-schema) domains.

## Priority
P0 (foundation for security, auditing, and catalog usability).

## References
- `docs/specifications/DDL_SCHEMAS.md`
- `docs/specifications/DDL_TABLES.md`
- `docs/specifications/DDL_VIEWS.md`
- `docs/specifications/DDL_SEQUENCES.md`
- `docs/specifications/SYSTEM_CATALOG_STRUCTURE.md`
- `docs/findings/engine_gap_report.md` (UUID resolver gap)

## Order of Implementation
1) Unified resolver view/indexes in catalog.
2) Resolver API in CatalogManager.
3) Object rename for tables/views/sequences/etc.
4) Schema move support (SET SCHEMA or MOVE) for schema-scoped objects.
5) Update SHOW commands to use resolver API.

## Implementation Tasks
- Add unified resolver view across catalog tables (id, schema_path, full_path, name, type, dialect_tag, compat_name where applicable).
- Back resolver with hash index on UUID and B-tree on schema/name.
- Implement CatalogManager API: resolve UUID → (path, name, type), and reverse.
- Implement renameTable and rename for other object types.
- Implement object move to new schema (with optional rename) for schema-scoped objects only.
- Update SBLR executor to handle RENAME TABLE and SET SCHEMA.

## Required Data/Schema Changes
- Unified resolver view across all catalog tables with columns: object_id, schema_path, full_path, object_name, object_type.
- Hash index on object_id; B-tree indexes on schema_path + object_name.


## Completion Checklist (Developer)
- [ ] Unified resolver view exists with indexes.
- [ ] CatalogManager exposes resolver APIs (UUID→path and path→UUID).
- [ ] Rename implemented for all core object types.
- [ ] Move-to-schema implemented for all schema-scoped object types (domains excluded).
- [ ] SBLR executor uses resolver for SHOW LOCATION/RESOLVED.

## Completion Checklist (Auditor)
- [ ] Resolver returns correct type/path for all object types.
- [ ] Rename does not change UUIDs and preserves dependencies.
- [ ] Schema move updates full paths without breaking references.
- [ ] SHOW LOCATION/RESOLVED consistent with resolver view.

## Testing Requirements
- Unit tests for resolver API (UUID→path and path→UUID).
- Rename tests for global domains; rename/move tests for schema-scoped objects.
- Dependency graph remains intact after rename/move.

## Acceptance Criteria
- Resolver returns correct schema path/name/type for all object types.
- Rename/move does not change UUIDs and does not break dependencies (domains are rename-only).
- RENAME TABLE and SET SCHEMA execute end-to-end via SBLR.

## Implementation Notes (Concrete)
- **Resolver view schema** (example): `sb_object_resolver(object_id, schema_path, full_path, object_name, object_type, dialect_tag, compat_name)`.
- **Indexes**: hash index on `object_id`; B-tree on `(schema_path, object_name)` and `(object_name, dialect_tag)` for domains.
- **CatalogManager APIs**:
  - `resolveObject(const ID& object_id, ResolvedObject& out)`
  - `resolveObject(const std::string& schema_path, const std::string& name, ObjectType type, ID& out)`
  - `renameObject(const ID& object_id, const std::string& new_name)`
  - `moveObject(const ID& object_id, const ID& new_schema_id, std::optional<std::string> new_name)`
- **SBLR executor**: implement handlers for `RENAME TABLE` and `SET SCHEMA` that call CatalogManager.
- **Dependency updates**: update resolver view + dependency graph after rename/move.

## Expanded API/Schema Details
- **Resolver view sources** (expected UNION): `sb_tables`, `sb_views`, `sb_indexes`, `sb_sequences`, `sb_functions`, `sb_procedures`, `sb_packages`, `sb_domains`, `sb_triggers`, `sb_roles` (as applicable).
- **Domain resolver behavior**:
  - `schema_path` = empty string for domains.
  - `full_path` = `domain_name`.
  - Resolution requires `(object_name, dialect_tag)` or `(compat_name)` when provided.
- **Resolver columns**:
  - `object_id` (UUID)
  - `schema_path` (text)
  - `full_path` (text, `schema_path.object_name`)
  - `object_name` (text)
  - `object_type` (enum or text)
  - `dialect_tag` (text, null for non-domain objects)
  - `compat_name` (text, null for non-domain objects)
- **Object rename APIs** (CatalogManager):
  - `renameTable(const ID& table_id, const std::string& new_name, ErrorContext* ctx)`
  - `renameView(...)`, `renameSequence(...)`, `renameIndex(...)`, `renameFunction(...)`, `renameProcedure(...)`, `renamePackage(...)`, `renameDomain(...)`, `renameTrigger(...)`
- **Move-to-schema API** (CatalogManager):
  - `moveObject(const ID& object_id, const ID& schema_id, std::optional<std::string> new_name, ErrorContext* ctx)` (not valid for domains)
- **SBLR opcodes/handlers**:
  - Ensure `ALTER TABLE ... RENAME TO` and `ALTER ... SET SCHEMA` bytecode exist and are routed in `Executor`.
  - Ensure `ALTER DOMAIN ... RENAME TO` is supported; no `SET SCHEMA` for domains.

## Full Implementation Detail (No Ambiguity)
- **Resolver view DDL (example)**:
  - `CREATE VIEW sb_object_resolver AS` UNION ALL of each catalog table, with columns: `object_id`, `schema_path`, `full_path`, `object_name`, `object_type`, `dialect_tag`, `compat_name`.
  - `schema_path` = resolved hierarchical path; `full_path` = `schema_path || '.' || object_name` (for domains, `schema_path` is empty and `full_path` is `domain_name`).
- **Indexing**:
  - Hash index on `object_id` for O(1) UUID lookups.
  - B-tree index on `(schema_path, object_name)` for name resolution.
  - B-tree index on `(object_name, dialect_tag)` for domain lookups.
- **Rename/move invariants**:
  - UUIDs never change.
  - Dependencies update to new name/path; dependency edges remain intact.
  - Search path resolution uses resolver view for SHOW LOCATION/RESOLVED.
- **Executor behavior**:
  - Implement `ALTER TABLE ... RENAME TO` using `CatalogManager::renameTable`.
  - Implement `ALTER ... SET SCHEMA` for tables, views, sequences, functions, procedures, packages, triggers (domains are global).

## Concrete View + Index DDL (Example)
- `CREATE VIEW sb_object_resolver AS
   SELECT table_id AS object_id, schema_path, schema_path || '.' || table_name AS full_path,
          table_name AS object_name, 'TABLE' AS object_type, NULL AS dialect_tag, NULL AS compat_name FROM sb_tables
   UNION ALL
   SELECT view_id, schema_path, schema_path || '.' || name, name, 'VIEW', NULL, NULL FROM sb_views
   UNION ALL
   SELECT sequence_id, schema_path, schema_path || '.' || name, name, 'SEQUENCE', NULL, NULL FROM sb_sequences
   UNION ALL
   SELECT function_id, schema_path, schema_path || '.' || name, name, 'FUNCTION', NULL, NULL FROM sb_functions
   UNION ALL
   SELECT domain_id, '' AS schema_path, domain_name AS full_path, domain_name AS object_name,
          'DOMAIN' AS object_type, dialect_tag, compat_name FROM sb_domains;`
- `CREATE INDEX sb_object_resolver_uuid_hash ON sb_object_resolver USING HASH (object_id);`
- `CREATE INDEX sb_object_resolver_name_btree ON sb_object_resolver (schema_path, object_name);`
- `CREATE INDEX sb_object_resolver_domain_btree ON sb_object_resolver (object_name, dialect_tag);`

## Full Catalog DDL (Required)
```sql
-- Core object tables must include UUID primary keys and schema references
CREATE TABLE sb_tables (
  table_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  table_name TEXT NOT NULL,
  schema_path TEXT NOT NULL,
  owner_id UUID NOT NULL
);

CREATE TABLE sb_views (
  view_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  schema_path TEXT NOT NULL,
  owner_id UUID NOT NULL,
  definition TEXT NOT NULL
);

CREATE TABLE sb_sequences (
  sequence_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  schema_path TEXT NOT NULL,
  owner_id UUID NOT NULL
);

CREATE TABLE sb_functions (
  function_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  schema_path TEXT NOT NULL,
  owner_id UUID NOT NULL
);

CREATE TABLE sb_procedures (
  procedure_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  schema_path TEXT NOT NULL,
  owner_id UUID NOT NULL
);

CREATE TABLE sb_packages (
  package_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  schema_path TEXT NOT NULL,
  owner_id UUID NOT NULL
);

CREATE TABLE sb_domains (
  domain_id UUID PRIMARY KEY,
  domain_name TEXT NOT NULL,
  dialect_tag TEXT NOT NULL,
  compat_name TEXT,
  owner_id UUID NOT NULL
);

CREATE TABLE sb_triggers (
  trigger_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  schema_path TEXT NOT NULL,
  owner_id UUID NOT NULL
);

CREATE TABLE sb_indexes (
  index_id UUID PRIMARY KEY,
  table_id UUID NOT NULL,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  schema_path TEXT NOT NULL,
  owner_id UUID NOT NULL
);
```

## Concrete Test Cases
- **UUID resolution**: lookup by UUID for each object type returns correct schema path/name/type.
- **Rename table**: rename table and verify UUID unchanged, dependencies unchanged, SHOW LOCATION updated.
- **Move to schema**: move view and sequence to new schema, verify full_path changed, resolution works.

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.

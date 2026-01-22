# Optional Alpha Specification: Object Name Registry (Materialized View)

## 1. Purpose
Provide a fast, security-aware name resolution registry without making names
the canonical source of truth. All catalog objects remain UUID-first; this
registry is a materialized index for lookup and discovery.

## 2. Scope
- **Optional Alpha** feature.
- **Core engine** catalog only (no parser or wire-protocol changes required).
- **Read-mostly** registry used by tooling and metadata queries.

## 3. Rationale
- UUIDs are authoritative in the engine; names are for humans and tooling.
- A single registry reduces repeated name resolution across many catalog tables.
- Security can be enforced by UUID privileges while still allowing name lookup.

## 4. Data Model

### 4.1 Base Table (Materialized Index)
`sys.object_registry_base` is the materialized registry maintained by catalog DDL.

**Columns (minimum set):**
- `object_id` UUID (primary key)
- `object_type` INT16 (catalog object type code)
- `schema_id` UUID (nullable for non-schema objects)
- `name` TEXT (original name as stored)
- `normalized_name` TEXT (case-folded name for lookup)
- `full_path` TEXT (canonical schema path + name, not search-path result)
- `namespace_group` INT16 (see Section 5)
- `owner_id` UUID
- `is_system` BOOLEAN
- `is_emulated` BOOLEAN
- `is_temp` BOOLEAN
- `created_at` TIMESTAMP
- `updated_at` TIMESTAMP

**Indexes (recommended):**
- PK on `object_id`
- Unique on (`schema_id`, `namespace_group`, `normalized_name`)
- Unique on (`full_path`, `namespace_group`)
- Lookup index on (`object_type`, `normalized_name`)

### 4.2 Security-Filtered View
`sys.object_registry` is a view over `sys.object_registry_base` that filters
rows by UUID-based privilege rules.

**Rule:** the view returns only objects the caller can see (ownership or
effective privilege), using the standard permission checker by `object_id`.

### 4.3 Session Overlay (Optional)
`sys.object_registry_temp` can hold session-scoped temp objects. The
`sys.object_registry` view unions the temp overlay for the caller’s session.

## 5. Namespace Rules
`namespace_group` separates object types that share a naming scope.

**Suggested groups (example mapping):**
- `RELATION` (tables, views, materialized views)
- `ROUTINE` (procedures, functions)
- `TYPE` (domains, user-defined types)
- `SECURITY` (roles, users, groups)
- `SCHEMA` (schemas)
- `STORAGE` (tablespaces)
- `INDEX` (indexes)

The exact mapping should follow catalog rules in
`docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`.

## 6. Maintenance Rules
The registry is updated in the same catalog transaction as the DDL change:
- CREATE: insert one row
- ALTER RENAME / MOVE: update name, normalized_name, full_path
- DROP: delete row
- Schema rename: update full_path for all child objects in that schema
- Object-type changes (if allowed): update object_type/namespace_group

**Bootstrap:** initial catalog creation populates the registry for all system
objects and schemas.

**Resync:** provide an admin-only rebuild path that re-scans canonical catalog
tables and replaces the registry (for recovery from drift).

## 7. Security Rules
- The registry must not leak names for objects the caller cannot see.
- `sys.object_registry_base` is internal; only `sys.object_registry` is exposed.
- Visibility checks must use UUID-based privileges (not name-based).

## 8. Query Examples

**Find a table by schema + name (case-insensitive):**
```sql
SELECT object_id
FROM sys.object_registry
WHERE namespace_group = 'RELATION'
  AND schema_id = :schema_id
  AND normalized_name = UPPER(:name);
```

**Resolve a fully-qualified path:**
```sql
SELECT object_id, object_type
FROM sys.object_registry
WHERE namespace_group = 'RELATION'
  AND full_path = 'users.sales.orders';
```

## 9. Dependencies
- `docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`
- `docs/specifications/catalog/SCHEMA_PATH_RESOLUTION.md`
- `docs/specifications/Security Design Specification/03_AUTHORIZATION_MODEL.md`
- `docs/specifications/core/ENGINE_CORE_UNIFIED_SPEC.md`

## 10. Out of Scope
- Replacing per-object catalog tables as the source of truth.
- Changing parser or wire protocol behavior.
- Altering privilege semantics.

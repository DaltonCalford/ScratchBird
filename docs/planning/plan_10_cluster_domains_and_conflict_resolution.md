# Plan 10 - Cluster Domains and Conflict Resolution

## Scope
Define and implement cluster-wide domains with dialect tags, compatibility names, conflict detection, and resolution. Domains are global (not schema-scoped) and replicated across cluster members.

## Priority
P0 (core metadata consistency and cross-node correctness).

## References
- `docs/specifications/DDL_DOMAINS.md`
- `docs/specifications/03_TYPES_AND_DOMAINS.md`
- `docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md`
- `docs/specifications/draft_security_architecture_specification.md`
- `docs/findings/engine_gap_report.md` (domains + cluster gaps)
- `docs/planning/plan_12_domain_runtime_and_type_system.md`

## Order of Implementation
1) Catalog schema changes for cluster domains and conflict tracking.
2) CatalogManager domain APIs (create/alter/drop/resolve/rebind).
3) Conflict detection and reconciliation on join/rejoin.
4) Dependency gating for shadow domains.
5) SHOW DOMAIN + resolver integration.
6) Tests (cluster, conflict, rebind, permissions).

## Concrete Code Touchpoints (Exact Files + Functions)
- `include/scratchbird/core/catalog_manager.h`:
  - Extend `DomainInfo` with dialect/compat/origin/state fields.
  - Add APIs: `resolveDomain`, `rebindDomain`, `resolveDomainConflict`.
- `src/core/catalog_manager.cpp`:
  - Add `DomainRecord` fields for dialect/compat/hash/origin/state.
  - Implement create/alter/drop with conflict detection + collision tables.
- `src/sblr/executor.cpp`:
  - Implement `EXT_CREATE_DOMAIN`, `EXT_ALTER_DOMAIN`, `EXT_DROP_DOMAIN`, `EXT_REBIND_DOMAIN`, `EXT_RESOLVE_DOMAIN_CONFLICT`.
- `src/core/domain_manager.cpp`:
  - Remove in-memory-only logic; use catalog tables (see Plan 12).
- Dependency gating:
  - `CatalogManager::createTable` and `addColumn` paths (block SHADOW domains).
- SHOW DOMAIN:
  - `Executor::executeShowDomain()`

## Implementation Tasks
- Implement cluster-wide domain catalog tables with conflict/history/alias support.
- Add domain detail tables (constraints, fields, enum values, variant types, policy options) for runtime enforcement.
- Extend DomainRecord with dialect/compat/state/origin/hash fields.
- Enforce CREATE/ALTER/DROP DOMAIN privileges (cluster-wide).
- Implement ALTER DOMAIN full validation and cross-node pending status/reporting.
- Implement conflict detection during CREATE/ALTER and join/rejoin reconciliation.
- Implement shadow domain gating (existing dependencies allowed, new usage blocked).
- Implement domain rebind (repoint dependencies to new domain UUID).
- Add cluster epoch bump for domain changes to invalidate caches/plans.
- Add SBLR opcodes for ALTER/DROP/REBIND/RESOLVE and include domain metadata in CREATE.

## Required Data/Schema Changes
- `sys.cluster.configuration.domains` (global domains, no schema_id).
- `sys.cluster.configuration.domain_constraints`, `sys.cluster.configuration.domain_fields`, `sys.cluster.configuration.domain_enum_values`,
  `sys.cluster.configuration.domain_enum_options`, `sys.cluster.configuration.domain_variant_types`.
- `sys.cluster.configuration.domain_security`, `sys.cluster.configuration.domain_integrity`, `sys.cluster.configuration.domain_validation`,
  `sys.cluster.configuration.domain_quality`.
- `sys.cluster.configuration.domain_validation_reports` (per-node validation results and violating row keys).
- `sys.cluster.configuration.domain_collisions` and `sys.cluster.configuration.domain_collision_members`.
- `sys.cluster.configuration.domain_history` (audit/epoch).
- `sys.cluster.configuration.domain_aliases` (optional name mapping).
- Indexes on `(domain_name, dialect_tag)` and `compat_name`.

## Completion Checklist (Developer)
- [ ] Cluster-wide domain tables exist with indexes and on-disk persistence.
- [ ] Domain DDL requires explicit privileges.
- [ ] Conflict detection creates collision entries and shadow domains.
- [ ] UUIDv7 ordering determines canonical domain.
- [ ] Shadow domains block new dependencies.
- [ ] Rebind updates all dependent objects and dependency records.
- [ ] Domain changes bump cluster epoch and invalidate caches.
- [ ] SHOW DOMAIN exposes dialect_tag, compat_name, domain_kind, state, collision info.

## Completion Checklist (Auditor)
- [ ] No schema-scoped domains remain; all domains are global.
- [ ] Conflicts are recorded with deterministic canonical selection.
- [ ] Domain rebind honors cast/storage rules and is fully audited.
- [ ] Cluster join reconciliation merges non-conflicting domains and flags conflicts.

## Testing Requirements
- Cluster replication tests for domain DDL (CREATE/ALTER/DROP).
- ALTER DOMAIN validation tests (local fail with PK report; cluster pending until all nodes confirm).
- Join/rejoin conflict tests (same name + dialect across nodes).
- Shadow gating tests (new usage blocked, old usage allowed).
- Rebind tests with cast-allowed vs storage-mismatch cases.
- Permission tests for CREATE/ALTER/DROP DOMAIN and USAGE grants.
- SHOW DOMAIN tests for dialect/compat/state visibility.

## Acceptance Criteria
- Domains are global and identical across cluster members after reconciliation.
- Name+dialect conflicts are detected and resolved deterministically.
- Shadow domains are blocked from new dependencies but remain valid for existing usage.
- Rebind works when casts exist or storage matches; otherwise it fails with clear error.

## Implementation Notes (Concrete)
- **Global domains**: `domain_name` is global; no `schema_id` or `schema_path`.
- **Dialect tag**: `dialect_tag` required (scratchbird/firebird/mysql/postgres).
- **Compatibility name**: `compat_name` optional and used for cross-dialect resolution.
- **Domain kind**: store `domain_kind` (BASIC/RECORD/ENUM/SET/VARIANT) and `parent_domain_id` for inheritance.
- **Element types**: store `element_type_oid`/`element_domain_id` for SET/ARRAY domains.
- **Runtime enforcement**: see `docs/planning/plan_12_domain_runtime_and_type_system.md` for constraint evaluation, casting, and advanced domain type semantics.
- **Canonical ordering**: compare UUIDv7 timestamps; tie-break with full UUID bytes.
- **Conflict key**: `domain_name|dialect_tag` and `compat_name|*` (if compat provided).
- **State**:
  - `ACTIVE`: can be used for new dependencies.
  - `SHADOW`: only existing dependencies allowed.
  - `DEPRECATED`: allowed but warned; no new usage by default.
  - `PENDING_VALIDATE`: local validation passed; awaiting cluster validation reports.
  - `DROPPED`: retained only for history.

## Expanded API/Schema Details
- **CatalogManager domain APIs**:
  - `createDomain(const DomainInfo& in, ID& out_id, ErrorContext* ctx)`
  - `alterDomain(const ID& domain_id, const DomainAlterSpec& spec, ErrorContext* ctx)`
  - `dropDomain(const ID& domain_id, ErrorContext* ctx)` (RESTRICT only)
  - `resolveDomain(const std::string& name, const std::string& dialect_tag, const std::string& compat_name, ID& out_id, ErrorContext* ctx)`
  - `rebindDomain(const ID& old_id, const ID& new_id, RebindMode mode, ErrorContext* ctx)`
  - `resolveDomainConflict(const ID& collision_id, const ConflictResolution& action, ErrorContext* ctx)`
- **Dependency gating**:
  - On column/type usage, fail if target domain is `SHADOW` or `DEPRECATED` (configurable).
- **SBLR requirements**:
  - `EXT_CREATE_DOMAIN` payload includes: `domain_id`, `domain_name`, `dialect_tag`, `compat_name`, `domain_kind`, `parent_domain_id`, `owner_id`, `base_type_oid`, `default_expr_oid`, `check_expr_oid`, `cast_map_oid`, `element_type_oid`, `element_domain_id`, `collation_id`, `storage_hash`, `definition_hash`, `not_null`, `origin_node_id`, `origin_cluster_id`.
  - `EXT_ALTER_DOMAIN` for rename/default/check/compat updates.
  - `EXT_DROP_DOMAIN` (RESTRICT only).
  - `EXT_REBIND_DOMAIN` (old_id -> new_id with mode).
  - `EXT_RESOLVE_DOMAIN_CONFLICT` (collision_id + action).
  - `SBLR_TYPE_DOMAIN` must carry domain UUID.

## Full Implementation Detail (No Ambiguity)
### Catalog Schema Placement (Required)
- **Authoritative cluster tables**: `sys.cluster.configuration` schema path.
- **User-facing catalog**: views/synonyms in `sys.catalog` (no `sb_` prefix).
- **Naming rule**: `sb_` prefixes are documentation-only; do not create physical tables/views with `sb_` prefixes.
- **Example**: `sys.catalog.domains` -> `sys.cluster.configuration.domains`.

### Catalog DDL (Required)
```sql
CREATE TABLE sys.cluster.configuration.domains (
  domain_id UUID PRIMARY KEY,
  domain_name TEXT NOT NULL,
  dialect_tag TEXT NOT NULL,
  compat_name TEXT,
  domain_kind SMALLINT NOT NULL,
  parent_domain_id UUID,
  base_type_oid OID NOT NULL,
  default_expr_oid OID,
  check_expr_oid OID,
  cast_map_oid OID,
  element_type_oid OID,
  element_domain_id UUID,
  collation_id INTEGER,
  storage_hash BINARY(32) NOT NULL,
  definition_hash BINARY(32) NOT NULL,
  not_null SMALLINT NOT NULL,
  owner_id UUID NOT NULL,
  domain_state SMALLINT NOT NULL,
  canonical_domain_id UUID,
  collision_id UUID,
  origin_node_id UUID NOT NULL,
  origin_cluster_id UUID NOT NULL,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL,
  is_valid SMALLINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_constraints (
  constraint_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  constraint_name TEXT,
  constraint_type SMALLINT NOT NULL,
  expr_oid OID,
  is_enforced SMALLINT NOT NULL,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_fields (
  field_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  field_position SMALLINT NOT NULL,
  field_name TEXT NOT NULL,
  field_type_oid OID NOT NULL,
  field_domain_id UUID,
  not_null SMALLINT NOT NULL,
  default_expr_oid OID,
  check_expr_oid OID,
  collation_id INTEGER,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_enum_values (
  enum_value_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  value_label TEXT NOT NULL,
  value_position INTEGER NOT NULL,
  is_active SMALLINT NOT NULL,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_enum_options (
  domain_id UUID PRIMARY KEY,
  wrap SMALLINT NOT NULL,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_variant_types (
  variant_type_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  type_oid OID NOT NULL,
  created_time BIGINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_security (
  domain_id UUID PRIMARY KEY,
  mask_function TEXT,
  mask_type TEXT,
  audit_access SMALLINT NOT NULL,
  require_permission TEXT,
  encryption TEXT,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_integrity (
  domain_id UUID PRIMARY KEY,
  unique_across_database SMALLINT NOT NULL,
  case_insensitive SMALLINT NOT NULL,
  normalize_function TEXT,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_validation (
  domain_id UUID PRIMARY KEY,
  validate_function TEXT,
  on_violation SMALLINT NOT NULL,
  error_message TEXT,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_quality (
  domain_id UUID PRIMARY KEY,
  parse_function TEXT,
  standardize_function TEXT,
  enrich_function TEXT,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_collisions (
  collision_id UUID PRIMARY KEY,
  conflict_key TEXT NOT NULL,
  conflict_type SMALLINT NOT NULL,
  canonical_domain_id UUID NOT NULL,
  created_time BIGINT NOT NULL,
  resolved_time BIGINT,
  resolved_by UUID,
  resolution_action SMALLINT,
  status SMALLINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_collision_members (
  collision_id UUID NOT NULL,
  domain_id UUID NOT NULL,
  is_canonical SMALLINT NOT NULL,
  created_time BIGINT NOT NULL,
  PRIMARY KEY (collision_id, domain_id)
);

CREATE TABLE sys.cluster.configuration.domain_history (
  history_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  action SMALLINT NOT NULL,
  old_definition_oid OID,
  new_definition_oid OID,
  changed_by UUID NOT NULL,
  changed_time BIGINT NOT NULL,
  cluster_epoch BIGINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_validation_reports (
  report_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  node_id UUID NOT NULL,
  status SMALLINT NOT NULL,          -- 0=PASS, 1=FAIL
  table_id UUID,                     -- table with violations (NULL if PASS)
  pk_values_oid OID,                 -- serialized primary key values for violating rows
  detail_text_oid OID,               -- optional human-readable summary
  created_time BIGINT NOT NULL
);

CREATE TABLE sys.cluster.configuration.domain_aliases (
  alias_id UUID PRIMARY KEY,
  alias_name TEXT NOT NULL,
  dialect_tag TEXT NOT NULL,
  compat_name TEXT,
  domain_id UUID NOT NULL,
  created_time BIGINT NOT NULL,
  is_active SMALLINT NOT NULL
);

CREATE VIEW sys.catalog.domains AS
  SELECT * FROM sys.cluster.configuration.domains;
CREATE VIEW sys.catalog.domain_constraints AS
  SELECT * FROM sys.cluster.configuration.domain_constraints;
CREATE VIEW sys.catalog.domain_fields AS
  SELECT * FROM sys.cluster.configuration.domain_fields;
CREATE VIEW sys.catalog.domain_enum_values AS
  SELECT * FROM sys.cluster.configuration.domain_enum_values;
CREATE VIEW sys.catalog.domain_enum_options AS
  SELECT * FROM sys.cluster.configuration.domain_enum_options;
CREATE VIEW sys.catalog.domain_variant_types AS
  SELECT * FROM sys.cluster.configuration.domain_variant_types;
CREATE VIEW sys.catalog.domain_security AS
  SELECT * FROM sys.cluster.configuration.domain_security;
CREATE VIEW sys.catalog.domain_integrity AS
  SELECT * FROM sys.cluster.configuration.domain_integrity;
CREATE VIEW sys.catalog.domain_validation AS
  SELECT * FROM sys.cluster.configuration.domain_validation;
CREATE VIEW sys.catalog.domain_quality AS
  SELECT * FROM sys.cluster.configuration.domain_quality;
CREATE VIEW sys.catalog.domain_collisions AS
  SELECT * FROM sys.cluster.configuration.domain_collisions;
CREATE VIEW sys.catalog.domain_collision_members AS
  SELECT * FROM sys.cluster.configuration.domain_collision_members;
CREATE VIEW sys.catalog.domain_history AS
  SELECT * FROM sys.cluster.configuration.domain_history;
CREATE VIEW sys.catalog.domain_validation_reports AS
  SELECT * FROM sys.cluster.configuration.domain_validation_reports;
CREATE VIEW sys.catalog.domain_aliases AS
  SELECT * FROM sys.cluster.configuration.domain_aliases;
```

### Conflict Algorithm (Deterministic)
1) **Create/Alter in cluster**:
   - Resolve `(domain_name, dialect_tag)` and `compat_name`.
   - If identical `definition_hash`, return existing domain.
   - If conflict:
     - Insert domain as `SHADOW`.
     - Create/update `sys.cluster.configuration.domain_collisions`.
     - Canonical = smallest UUIDv7 timestamp (tie-break by UUID bytes).
     - Block new dependencies on non-canonical domains.
   - If ALTER passes local validation and cluster mode is enabled, set `domain_state = PENDING_VALIDATE` and wait for
     `sys.cluster.configuration.domain_validation_reports` from all members before activating.
2) **Join/Rejoin reconciliation**:
   - For each local domain:
     - If no cluster match: promote to `ACTIVE` cluster domain.
     - If conflict: create collision entry and set canonical via UUIDv7 ordering.
3) **Usage gating**:
   - If domain_state is `SHADOW`, reject new dependencies with hint to canonical domain.
4) **Resolution actions**:
   - `RENAME`: change domain_name to unique value; clear collision.
   - `REBIND`: repoint all references to canonical domain (with cast/storage checks).
   - `DROP_SHADOW`: only if no dependencies remain.

### Rebind Rules
- Allowed if `storage_hash` matches or cast exists in `cast_map_oid`.
- Updates all domain references in columns, defaults, checks, procedures/functions, and dependencies.
- Records change in `sys.cluster.configuration.domain_history` and bumps cluster epoch.

## Concrete Test Cases
- Create same `domain_name` in two nodes with different definitions; verify collision record and canonical selection.
- Create same `domain_name` with same definition; verify idempotent merge.
- Use `compat_name` to resolve domain and ensure correct UUID returned.
- Shadow domain blocks new column creation; existing columns still work.
- Rebind with cast succeeds; rebind without cast fails.

## Common Failure Patterns
- Domain resolution ignores dialect_tag or compat_name.
- Conflict detection only checks name, missing compat_name conflicts.
- Shadow domains still allow new dependencies.
- Rebind updates only some reference types (columns but not procedure parameters).
- No cluster epoch bump after domain changes, leading to stale plans.

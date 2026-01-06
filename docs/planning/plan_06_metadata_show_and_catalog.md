# Plan 06 - Metadata, SHOW Commands, and Catalog Completeness

## Scope
Complete metadata visibility, SHOW commands, catalog tables for core object types, and runtime monitoring views needed by emulated engines (MON$/pg_stat/performance_schema).

## Priority
P1 (admin usability and tooling).

## Status (Current)
Completed:
- SHOW TRIGGER/PROCEDURE/FUNCTION/VIEW/COMMENTS/DEPENDENCIES/PACKAGE now pull from catalog caches and honor body redaction when source is missing.
- SHOW outputs now include object comments where available.
- SHOW DOMAIN/GRANTS/CHECKS now use catalog lookups (constraints/permissions/domain manager) with redaction hooks.
- Metadata visibility hooks now enforce owner/superuser/privilege-based redaction for SHOW TABLES/COLUMNS/INDEXES/CREATE TABLE/TABLE, SHOW GENERATOR, SHOW COMMENTS/DEPENDENCIES, and SHOW SCHEMA/SCHEMA TREE/RESOLVED/OBJECTS/LOCATION (restricted enumeration for schemas/tables/objects).
- Virtual catalog routing initialized at database open and executor now resolves virtual schemas (information_schema/pg_catalog/mysql/etc) via VirtualCatalogRouter.
- information_schema expanded to include domains, sequences, routines, triggers, stats, and privilege views with catalog-backed data.
- Firebird RDB$FIELDS now maps domain and column metadata from ScratchBird domains/columns.
- pg_catalog and mysql.* virtual catalogs now return real core metadata for key tables (namespace/class/type/attr/user/proc).
- Runtime monitoring views now populate pg_stat_activity, pg_locks, MON$ATTACHMENTS/TRANSACTIONS/STATEMENTS, and MySQL information_schema/performance_schema processlist/threads/events_statements_*/events_transactions_current/events_transactions_history_long/events_waits_current/events_waits_history_long/metadata_locks.
- pg_catalog helper functions (format_type, obj_description, col_description, shobj_description) now resolve core metadata comments/types.
- Added targeted virtual catalog unit coverage for pg_catalog and MySQL performance_schema processlist/threads/events_statements/events_transactions/events_waits/history_long/metadata_locks.
- MySQL performance_schema history_long tables now include completed transactions and lock waits (in-memory ring buffer).
- MySQL performance_schema digest summary tables now track statement digests (global/account/user/host) with histogram buckets and quantile estimation.

Partial / Outstanding:
- Metadata visibility for unsupported object types (packages/triggers/UDR/etc.) remains best-effort until dedicated permission object types exist.
- MySQL information_schema/performance_schema emulation beyond processlist/threads/events_statements_*/events_statements_summary_by_*/events_statements_histogram_*/events_transactions_current/events_transactions_history_long/events_waits_current/events_waits_history_long/metadata_locks remains pending (plugins, other performance_schema tables, etc.).

## References
- `docs/specifications/SYSTEM_CATALOG_STRUCTURE.md`
- `docs/specifications/DDL_TRIGGERS.md`
- `docs/specifications/DDL_PROCEDURES.md`
- `docs/specifications/DDL_FUNCTIONS.md`
- `docs/specifications/DDL_DOMAINS.md`
- `docs/specifications/DDL_PACKAGES.md`
- `docs/findings/engine_gap_report.md` (SHOW stubs)
- `docs/planning/plan_02_uuid_resolution_and_rename_move.md` (resolver view)
- `docs/planning/plan_16_attachment_transaction_model.md` (runtime attachments/transactions views)

## Order of Implementation
1) Catalog tables for triggers/procedures/functions/domains/comments/dependencies/packages.
2) SHOW command implementation using catalog tables.
3) Runtime monitoring views (attachments/transactions/locks).
4) Metadata visibility controls (redaction levels).

## Concrete Code Touchpoints (Exact Files + Functions)
- Catalog tables:
  - `src/core/catalog_manager.cpp` (catalog table definitions + record structs)
  - `include/scratchbird/core/catalog_manager.h` (Info structs + APIs)
  - `src/catalog/virtual_catalog.cpp` (virtual view exposure)
- `src/catalog/firebird_catalog.cpp` (RDB$ mapping)
- `src/catalog/virtual_catalog.cpp` (runtime monitoring views)
- SHOW handlers:
  - `src/sblr/executor.cpp`:
    - `executeShowTrigger()` (stub)
    - `executeShowProcedure()` (stub)
    - `executeShowFunction()` (stub)
    - `executeShowDomain()` (stub)
    - `executeShowGrants()` (stub)
    - `executeShowChecks()` (stub)
    - `executeShowComments()` (stub)
    - `executeShowDependencies()` (stub)
    - `executeShowPackage()` (stub)
    - `executeShowSchemaTree()` / `executeShowResolved()` / `executeShowObjects()` (must use resolver view)
- Parser and bytecode:
  - `src/parser/parser_v2.cpp` (SHOW parsing)
  - `src/sblr/bytecode_generator_v2.cpp` (EXT_SHOW_* opcodes)
- CLI:
  - `src/cli/sb_isql.cpp` (client-side SHOW glue; should call SQL SHOW commands)

## Implementation Tasks
- Add catalog tables for missing object types and wire DDL to populate them.
- Implement SHOW TRIGGER/PROCEDURE/FUNCTION/DOMAIN/COMMENTS/DEPENDENCIES/PACKAGE/GRANTS/CHECKS with real queries.
- Add metadata visibility policy hooks (redaction, restricted enumeration).
- Implement information_schema DOMAINS/USER_DEFINED_TYPES and Firebird RDB$FIELDS domain mapping.
- Implement pg_catalog/mysql catalogs with core metadata (pg_namespace/pg_class/pg_attribute/pg_type/pg_enum/pg_proc/mysql.user/mysql.proc).
- Implement runtime monitoring views required by emulated engines:
  - Firebird MON$ATTACHMENTS / MON$TRANSACTIONS.
  - PostgreSQL pg_stat_activity / pg_stat_database.
  - MySQL performance_schema threads/events_statements/events_transactions/events_waits/metadata_locks tables.

## Required Data/Schema Changes
- Add catalog tables for triggers, procedures, functions, domains, domain collision/history/aliases, domain constraints/fields/enum/variant/options, domain validation reports, comments, dependencies, packages.
- Add indexes required for SHOW queries and UUID resolver view (domain name + dialect + compat indexes).
- Add runtime virtual views (no on-disk storage) to expose attachments and transactions (Plan 16).

## Completion Checklist (Developer)
- [ ] Catalog tables exist for all SHOW targets.
- [ ] SHOW commands return real metadata.
- [ ] Metadata visibility policies are enforced.

## Completion Checklist (Auditor)
- [ ] SHOW commands reflect catalog state accurately.
- [ ] Restricted metadata mode hides object names/paths as configured.

## Testing Requirements
- Catalog DDL/metadata tests for each object type.
- SHOW command tests for each object type.
- Metadata visibility tests across roles.
- Update/add tests in:
  - `tests/unit/test_show_set_commands.cpp`
  - `tests/unit/test_catalog_manager.cpp`
  - `tests/unit/domains/test_domain_manager.cpp`
  - `tests/unit/test_runtime_monitor_views.cpp`
  - `tests/unit/test_virtual_catalogs.cpp`

## Acceptance Criteria
- All SHOW commands return real metadata.
- Metadata redaction/enumeration policy is enforced per security configuration.

## Implementation Notes (Concrete)
- **Catalog tables**:
  - `sys.catalog.triggers`, `sys.catalog.procedures`, `sys.catalog.functions`, `sys.catalog.comments`, `sys.catalog.dependencies`, `sys.catalog.packages`.
  - Domain tables live in `sys.cluster.configuration` with user-facing `sys.catalog` views (see schema placement below).
- **Indexes**: add UUID hash indexes and name/path B-tree indexes for SHOW queries.
- **SHOW handlers**: route all SHOW commands to catalog queries (no stub rows).
- **SHOW DOMAIN**: include `dialect_tag`, `compat_name`, `domain_kind`, `parent_domain_id`, `default`, `not_null`, `constraints`, `domain_state`, and any `collision_id`/`canonical_domain_id` fields.
- **Information schema**: populate `information_schema.domains` and `information_schema.user_defined_types` from `sys.catalog.domains` and `sys.catalog.domain_*` views.
- **Firebird catalog**: map `RDB$FIELDS` and `RDB$RELATION_FIELDS` domain metadata to `sys.catalog.domains` and `sys.catalog.domain_*` views.
- **Redaction**: enforce metadata visibility policy before returning rows.
- **Runtime monitoring**: implement `sys.runtime.attachments` and `sys.runtime.transactions` (Plan 16) and map to emulated views.

## Redaction Policy (Pending Plan 03)
- Finalize once role/privilege enforcement and permission cache hooks land (Plan 03).
- Candidate rules to lock in at that point:
  - Object names/paths appear only when caller has USAGE/SELECT/EXECUTE or is owner; otherwise apply restricted enumeration rules.
  - SHOW returns input/output + comments; body is redacted unless owner/superuser or explicit VIEW DEFINITION/EXECUTE privilege grants definition access.
  - Redacted bodies return literal `Redacted` with a reason code for audit/debugging.

## Full Implementation Detail (No Ambiguity)
### 1) Catalog Tables
- Define columns, types, and indexes for each metadata table (UUID id, schema_id for schema-scoped objects, owner_id, body OIDs).
- Ensure every object type has a UUID primary key; domains are global and do not include schema_id.

### 2) SHOW Handler Replacements
- Replace stub results in `src/sblr/executor.cpp`:
  - `executeShowTrigger()` -> query `sys.catalog.triggers` + `sys.catalog.tables` for table name.
  - `executeShowProcedure()` -> query `sys.catalog.procedures` + `sys.catalog.procedure_params`.
  - `executeShowFunction()` -> query `sys.catalog.functions` + `sys.catalog.procedure_params`.
  - `executeShowDomain()` -> query `sys.catalog.domains` + related `sys.catalog.domain_*` views.
  - `executeShowGrants()` -> query `sys.security.permissions`/`sys.security.column_permissions`.
  - `executeShowChecks()` -> query `sys.catalog.constraints` with CHECK type.
  - `executeShowComments()` -> query `sys.catalog.comments`.
  - `executeShowDependencies()` -> query `sys.catalog.dependencies`.
  - `executeShowPackage()` -> query `sys.catalog.packages`.
- Use resolver cache for:
  - `executeShowSchemaTree()`, `executeShowResolved()`, `executeShowObjects()` via `sys.catalog.object_resolver` view (Plan 02).

### 3) Information Schema / Emulated Catalog
- `src/catalog/virtual_catalog.cpp`:
  - Add virtual view definitions for domains + user-defined types.
- `src/catalog/firebird_catalog.cpp`:
  - Map `RDB$FIELDS` and `RDB$RELATION_FIELDS` to domain metadata.
- `src/catalog/virtual_catalog.cpp`:
  - Add runtime views for attachments/transactions and reuse in MON$/pg_stat/performance_schema definitions.

## Concrete Index DDL (Example)
- `CREATE INDEX triggers_name_idx ON sys.catalog.triggers(schema_id, name);`
- `CREATE INDEX procedures_name_idx ON sys.catalog.procedures(schema_id, name);`
- `CREATE INDEX functions_name_idx ON sys.catalog.functions(schema_id, name);`
- `CREATE INDEX domains_name_idx ON sys.cluster.configuration.domains(domain_name, dialect_tag);`
- `CREATE INDEX domains_compat_idx ON sys.cluster.configuration.domains(compat_name);`
- `CREATE INDEX domain_constraints_domain_idx ON sys.cluster.configuration.domain_constraints(domain_id);`
- `CREATE INDEX domain_fields_domain_idx ON sys.cluster.configuration.domain_fields(domain_id, field_position);`
- `CREATE INDEX domain_enum_values_domain_idx ON sys.cluster.configuration.domain_enum_values(domain_id, value_position);`
- `CREATE INDEX domain_variant_types_domain_idx ON sys.cluster.configuration.domain_variant_types(domain_id);`
- `CREATE INDEX domain_validation_reports_domain_idx ON sys.cluster.configuration.domain_validation_reports(domain_id);`
- `CREATE INDEX packages_name_idx ON sys.catalog.packages(schema_id, name);`
- `CREATE INDEX comments_object_idx ON sys.catalog.comments(object_id);`
- `CREATE INDEX dependencies_dep_idx ON sys.catalog.dependencies(dependent_id);`

## Catalog Schema Placement (Required)
- **Authoritative cluster domain tables**: `sys.cluster.configuration` schema path.
- **User-facing catalog**: `sys.catalog` views/synonyms without `sb_` prefix.
- **Other catalog tables**: stored in `sys.catalog` with canonical names (no `sb_` prefix).
- **Naming rule**: `sb_` prefixes are documentation-only; do not create physical tables/views with `sb_` prefixes.
- **Emulated catalogs**: views/synonyms under `remote.emulated.<dialect>.<server>.<db>` that point to `sys.catalog.*`, `sys.cluster.configuration.*`, `sys.security.*`, and `sys.runtime.*`.

## Full Catalog DDL (Required)
```sql
CREATE TABLE sys.catalog.triggers (
  trigger_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  table_id UUID NOT NULL,
  name TEXT NOT NULL,
  timing TEXT NOT NULL,
  events TEXT NOT NULL,
  body_oid OID,
  owner_id UUID NOT NULL
);

CREATE TABLE sys.catalog.procedures (
  procedure_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  arg_sig TEXT,
  body_oid OID,
  owner_id UUID NOT NULL
);

CREATE TABLE sys.catalog.functions (
  function_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  return_type TEXT NOT NULL,
  arg_sig TEXT,
  body_oid OID,
  owner_id UUID NOT NULL
);

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

CREATE TABLE sys.cluster.configuration.domain_validation_reports (
  report_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  node_id UUID NOT NULL,
  status SMALLINT NOT NULL,
  table_id UUID,
  pk_values_oid OID,
  detail_text_oid OID,
  created_time BIGINT NOT NULL
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

CREATE TABLE sys.cluster.configuration.domain_aliases (
  alias_id UUID PRIMARY KEY,
  alias_name TEXT NOT NULL,
  dialect_tag TEXT NOT NULL,
  compat_name TEXT,
  domain_id UUID NOT NULL,
  created_time BIGINT NOT NULL,
  is_active SMALLINT NOT NULL
);

CREATE TABLE sys.catalog.comments (
  comment_id UUID PRIMARY KEY,
  object_id UUID NOT NULL,
  owner_id UUID NOT NULL,
  comment_text TEXT NOT NULL
);

CREATE TABLE sys.catalog.dependencies (
  dependency_id UUID PRIMARY KEY,
  dependent_id UUID NOT NULL,
  referenced_id UUID NOT NULL,
  dependency_type TEXT NOT NULL
);

CREATE TABLE sys.catalog.packages (
  package_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  header_oid OID,
  body_oid OID,
  owner_id UUID NOT NULL
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
CREATE VIEW sys.catalog.domain_validation_reports AS
  SELECT * FROM sys.cluster.configuration.domain_validation_reports;
CREATE VIEW sys.catalog.domain_collisions AS
  SELECT * FROM sys.cluster.configuration.domain_collisions;
CREATE VIEW sys.catalog.domain_collision_members AS
  SELECT * FROM sys.cluster.configuration.domain_collision_members;
CREATE VIEW sys.catalog.domain_history AS
  SELECT * FROM sys.cluster.configuration.domain_history;
CREATE VIEW sys.catalog.domain_aliases AS
  SELECT * FROM sys.cluster.configuration.domain_aliases;
```

## Concrete Test Cases
- Create each object type and verify corresponding SHOW output.
- SHOW DOMAIN includes domain_kind/parent/default/constraints and matches catalog.
- information_schema.domains matches `sys.catalog.domains`/`sys.catalog.domain_*` contents.
- Firebird RDB$FIELDS exposes domain metadata for emulated catalogs.
- Verify restricted metadata mode hides object names for low-privileged users.

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.

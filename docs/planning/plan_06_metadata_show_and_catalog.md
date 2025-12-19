# Plan 06 - Metadata, SHOW Commands, and Catalog Completeness

## Scope
Complete metadata visibility, SHOW commands, and catalog tables for core object types.

## Priority
P1 (admin usability and tooling).

## References
- `docs/specifications/SYSTEM_CATALOG_STRUCTURE.md`
- `docs/specifications/DDL_TRIGGERS.md`
- `docs/specifications/DDL_PROCEDURES.md`
- `docs/specifications/DDL_FUNCTIONS.md`
- `docs/specifications/DDL_DOMAINS.md`
- `docs/specifications/DDL_PACKAGES.md`
- `docs/findings/engine_gap_report.md` (SHOW stubs)

## Order of Implementation
1) Catalog tables for triggers/procedures/functions/domains/comments/dependencies/packages.
2) SHOW command implementation using catalog tables.
3) Metadata visibility controls (redaction levels).

## Implementation Tasks
- Add catalog tables for missing object types and wire DDL to populate them.
- Implement SHOW TRIGGER/PROCEDURE/FUNCTION/DOMAIN/COMMENTS/DEPENDENCIES/PACKAGE/GRANTS/CHECKS with real queries.
- Add metadata visibility policy hooks (redaction, restricted enumeration).
- Implement information_schema DOMAINS/USER_DEFINED_TYPES and Firebird RDB$FIELDS domain mapping.

## Required Data/Schema Changes
- Add catalog tables for triggers, procedures, functions, domains, domain collision/history/aliases, domain constraints/fields/enum/variant/options, domain validation reports, comments, dependencies, packages.
- Add indexes required for SHOW queries and UUID resolver view (domain name + dialect + compat indexes).

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

## Acceptance Criteria
- All SHOW commands return real metadata.
- Metadata redaction/enumeration policy is enforced per security configuration.

## Implementation Notes (Concrete)
- **Catalog tables**: implement `sb_triggers`, `sb_procedures`, `sb_functions`, `sb_domains`, `sb_domain_constraints`, `sb_domain_fields`, `sb_domain_enum_values`, `sb_domain_enum_options`, `sb_domain_variant_types`, `sb_domain_security`, `sb_domain_integrity`, `sb_domain_validation`, `sb_domain_quality`, `sb_domain_validation_reports`, `sb_domain_collisions`, `sb_domain_history`, `sb_domain_aliases`, `sb_comments`, `sb_dependencies`, `sb_packages`.
- **Indexes**: add UUID hash indexes and name/path B-tree indexes for SHOW queries.
- **SHOW handlers**: route all SHOW commands to catalog queries (no stub rows).
- **SHOW DOMAIN**: include `dialect_tag`, `compat_name`, `domain_kind`, `parent_domain_id`, `default`, `not_null`, `constraints`, `domain_state`, and any `collision_id`/`canonical_domain_id` fields.
- **Information schema**: populate `information_schema.domains` and `information_schema.user_defined_types` from `sb_domains` and `sb_domain_*` tables.
- **Firebird catalog**: map `RDB$FIELDS` and `RDB$RELATION_FIELDS` domain metadata to `sb_domains` and `sb_domain_*` tables.
- **Redaction**: enforce metadata visibility policy before returning rows.

## Expanded API/Schema Details
- **Table examples**:
  - `sb_triggers(trigger_id, schema_id, table_id, name, timing, events, body_oid, owner_id)`
  - `sb_procedures(procedure_id, schema_id, name, arg_sig, body_oid, owner_id)`
  - `sb_functions(function_id, schema_id, name, return_type, arg_sig, body_oid, owner_id)`
  - `sb_domains(domain_id, domain_name, dialect_tag, compat_name, domain_kind, parent_domain_id, base_type_oid, element_type_oid, element_domain_id, default_expr_oid, check_expr_oid, cast_map_oid, owner_id, domain_state, canonical_domain_id, collision_id)`
  - `sb_domain_constraints(domain_id, constraint_name, constraint_type, expr_oid)`
  - `sb_domain_fields(domain_id, field_position, field_name, field_type_oid, field_domain_id, default_expr_oid)`
  - `sb_domain_enum_values(domain_id, value_label, value_position)`
  - `sb_domain_variant_types(domain_id, type_oid)`
  - `sb_domain_security(domain_id, mask_function, audit_access, require_permission, encryption)`
  - `sb_domain_integrity(domain_id, unique_across_database, normalize_function)`
  - `sb_domain_validation(domain_id, validate_function, on_violation, error_message)`
  - `sb_domain_quality(domain_id, parse_function, standardize_function, enrich_function)`
  - `sb_domain_validation_reports(domain_id, node_id, status, table_id, pk_values_oid)`
  - `sb_comments(comment_id, object_id, owner_id, comment_text)`
  - `sb_dependencies(dependency_id, dependent_id, referenced_id, dependency_type)`
  - `sb_packages(package_id, schema_id, name, header_oid, body_oid, owner_id)`
- **SHOW handlers**: use CatalogManager queries (no stub rows), including `SHOW DEPENDENCIES` and `SHOW COMMENTS`.

## Full Implementation Detail (No Ambiguity)
- **Catalog DDL**:
  - Define columns, types, and indexes for each metadata table (UUID id, schema_id for schema-scoped objects, owner_id, body OIDs).
  - Ensure every object type has a UUID primary key; domains are global and do not include schema_id.
- **SHOW mapping**:
  - SHOW TRIGGER → sb_triggers
  - SHOW PROCEDURE → sb_procedures
  - SHOW FUNCTION → sb_functions
  - SHOW DOMAIN → sb_domains
  - SHOW COMMENTS → sb_comments
  - SHOW DEPENDENCIES → sb_dependencies
  - SHOW PACKAGE → sb_packages
- **Visibility policy**:
  - Metadata redaction enforced prior to returning rows based on user/role policy.

## Concrete Index DDL (Example)
- `CREATE INDEX sb_triggers_name_idx ON sb_triggers(schema_id, name);`
- `CREATE INDEX sb_procedures_name_idx ON sb_procedures(schema_id, name);`
- `CREATE INDEX sb_functions_name_idx ON sb_functions(schema_id, name);`
- `CREATE INDEX sb_domains_name_idx ON sb_domains(domain_name, dialect_tag);`
- `CREATE INDEX sb_domains_compat_idx ON sb_domains(compat_name);`
- `CREATE INDEX sb_domain_constraints_domain_idx ON sb_domain_constraints(domain_id);`
- `CREATE INDEX sb_domain_fields_domain_idx ON sb_domain_fields(domain_id, field_position);`
- `CREATE INDEX sb_domain_enum_values_domain_idx ON sb_domain_enum_values(domain_id, value_position);`
- `CREATE INDEX sb_domain_variant_types_domain_idx ON sb_domain_variant_types(domain_id);`
- `CREATE INDEX sb_domain_validation_reports_domain_idx ON sb_domain_validation_reports(domain_id);`
- `CREATE INDEX sb_packages_name_idx ON sb_packages(schema_id, name);`
- `CREATE INDEX sb_comments_object_idx ON sb_comments(object_id);`
- `CREATE INDEX sb_dependencies_dep_idx ON sb_dependencies(dependent_id);`

## Full Catalog DDL (Required)
```sql
CREATE TABLE sb_triggers (
  trigger_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  table_id UUID NOT NULL,
  name TEXT NOT NULL,
  timing TEXT NOT NULL,
  events TEXT NOT NULL,
  body_oid OID,
  owner_id UUID NOT NULL
);

CREATE TABLE sb_procedures (
  procedure_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  arg_sig TEXT,
  body_oid OID,
  owner_id UUID NOT NULL
);

CREATE TABLE sb_functions (
  function_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  return_type TEXT NOT NULL,
  arg_sig TEXT,
  body_oid OID,
  owner_id UUID NOT NULL
);

CREATE TABLE sb_domains (
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

CREATE TABLE sb_domain_constraints (
  constraint_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  constraint_name TEXT,
  constraint_type SMALLINT NOT NULL,
  expr_oid OID,
  is_enforced SMALLINT NOT NULL,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sb_domain_fields (
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

CREATE TABLE sb_domain_enum_values (
  enum_value_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  value_label TEXT NOT NULL,
  value_position INTEGER NOT NULL,
  is_active SMALLINT NOT NULL,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sb_domain_enum_options (
  domain_id UUID PRIMARY KEY,
  wrap SMALLINT NOT NULL,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sb_domain_variant_types (
  variant_type_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  type_oid OID NOT NULL,
  created_time BIGINT NOT NULL
);

CREATE TABLE sb_domain_security (
  domain_id UUID PRIMARY KEY,
  mask_function TEXT,
  mask_type TEXT,
  audit_access SMALLINT NOT NULL,
  require_permission TEXT,
  encryption TEXT,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sb_domain_integrity (
  domain_id UUID PRIMARY KEY,
  unique_across_database SMALLINT NOT NULL,
  case_insensitive SMALLINT NOT NULL,
  normalize_function TEXT,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sb_domain_validation (
  domain_id UUID PRIMARY KEY,
  validate_function TEXT,
  on_violation SMALLINT NOT NULL,
  error_message TEXT,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sb_domain_quality (
  domain_id UUID PRIMARY KEY,
  parse_function TEXT,
  standardize_function TEXT,
  enrich_function TEXT,
  created_time BIGINT NOT NULL,
  last_modified_time BIGINT NOT NULL
);

CREATE TABLE sb_domain_validation_reports (
  report_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  node_id UUID NOT NULL,
  status SMALLINT NOT NULL,
  table_id UUID,
  pk_values_oid OID,
  detail_text_oid OID,
  created_time BIGINT NOT NULL
);

CREATE TABLE sb_domain_collisions (
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

CREATE TABLE sb_domain_collision_members (
  collision_id UUID NOT NULL,
  domain_id UUID NOT NULL,
  is_canonical SMALLINT NOT NULL,
  created_time BIGINT NOT NULL,
  PRIMARY KEY (collision_id, domain_id)
);

CREATE TABLE sb_domain_history (
  history_id UUID PRIMARY KEY,
  domain_id UUID NOT NULL,
  action SMALLINT NOT NULL,
  old_definition_oid OID,
  new_definition_oid OID,
  changed_by UUID NOT NULL,
  changed_time BIGINT NOT NULL,
  cluster_epoch BIGINT NOT NULL
);

CREATE TABLE sb_domain_aliases (
  alias_id UUID PRIMARY KEY,
  alias_name TEXT NOT NULL,
  dialect_tag TEXT NOT NULL,
  compat_name TEXT,
  domain_id UUID NOT NULL,
  created_time BIGINT NOT NULL,
  is_active SMALLINT NOT NULL
);

CREATE TABLE sb_comments (
  comment_id UUID PRIMARY KEY,
  object_id UUID NOT NULL,
  owner_id UUID NOT NULL,
  comment_text TEXT NOT NULL
);

CREATE TABLE sb_dependencies (
  dependency_id UUID PRIMARY KEY,
  dependent_id UUID NOT NULL,
  referenced_id UUID NOT NULL,
  dependency_type TEXT NOT NULL
);

CREATE TABLE sb_packages (
  package_id UUID PRIMARY KEY,
  schema_id UUID NOT NULL,
  name TEXT NOT NULL,
  header_oid OID,
  body_oid OID,
  owner_id UUID NOT NULL
);
```

## Concrete Test Cases
- Create each object type and verify corresponding SHOW output.
- SHOW DOMAIN includes domain_kind/parent/default/constraints and matches catalog.
- information_schema.domains matches sb_domains/sb_domain_* contents.
- Firebird RDB$FIELDS exposes domain metadata for emulated catalogs.
- Verify restricted metadata mode hides object names for low-privileged users.

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.

# Plan 12 - Domain Runtime and Type System Integration

## Scope
Complete end-to-end domain functionality: unified catalog definitions, type resolution, runtime verification, casting rules, array/sub-domain semantics, and named element access (RECORD/ENUM/SET/VARIANT). This plan covers runtime behavior and type-system integration; cluster conflict handling is in `docs/archive/2026-01-04/planning/plan_10_cluster_domains_and_conflict_resolution.md`.

## Priority
P0 (domains are core to data correctness and dialect compatibility).

## References
- `docs/specifications/DDL_DOMAINS.md`
- `docs/specifications/03_TYPES_AND_DOMAINS.md`
- `docs/specifications/SYSTEM_CATALOG_STRUCTURE.md`
- `docs/archive/2026-01-09/findings/domain_support_gaps.md`
- `docs/archive/2026-01-04/planning/plan_10_cluster_domains_and_conflict_resolution.md`

## Order of Implementation
1) Unify domain catalog and type descriptor storage.
2) Extend semantic resolution and SBLR type encoding to carry domain IDs.
3) Implement domain DDL execution (CREATE/ALTER/DROP) with advanced domain types.
4) Enforce domain constraints/defaults in DML and parameter binding.
5) Implement domain casting rules and inheritance validation.
6) Implement named element access for RECORD and ENUM/SET operations.
7) Add compatibility catalog exposure and SHOW DOMAIN details.
8) Tests (SQL + runtime + restart persistence).

## Concrete Code Touchpoints (Exact Files + Functions)
- `src/core/domain_manager.cpp` / `include/scratchbird/core/domain_manager.h`:
  - Replace in-memory domain store with runtime wrapper over `CatalogManager` tables.
- `src/core/catalog_manager.cpp` / `include/scratchbird/core/catalog_manager.h`:
  - Domain DDL create/alter/drop methods and domain table records.
- `src/sblr/executor.cpp`:
  - DDL executor paths for domain opcodes.
  - DML paths (INSERT/UPDATE) for domain constraint enforcement.
- `src/sblr/semantic_analyzer_v2.cpp`:
  - Type resolution includes domain_id.
- `src/sblr/bytecode_generator_v2.cpp`:
  - `TYPE_DOMAIN` and `TYPE_ARRAY` emission for domain types.
- `include/scratchbird/core/types.h`:
  - Add `TypeDescriptor` struct and serialization helpers.
- `src/core/type_system.cpp` / `src/core/type_extractor.cpp`:
  - Decode/encode TypeDescriptor and map domain->base types.
- Tests:
  - `tests/unit/domains/test_domain_manager.cpp`
  - `tests/unit/domains/test_enum_domain.cpp`
  - `tests/unit/domains/test_set_domain.cpp`
  - `tests/unit/domains/test_variant_domain.cpp`

## Implementation Tasks
- Replace or refactor `DomainManager` to be a runtime wrapper over `CatalogManager` domain tables.
- Add `TypeDescriptor` (domain-aware, array-aware) and persist it for columns/parameters/domains.
- Implement SBLR `TYPE_DOMAIN` and `TYPE_ARRAY` descriptors; update parsers and bytecode generator.
- Implement executor handling for domain DDL and type descriptors.
- Implement ALTER DOMAIN full validation with violation reporting and cluster pending status.
- Enforce domain constraints in INSERT/UPDATE and default application rules.
- Implement domain inheritance and prevent inheritance cycles.
- Implement domain casting rules (domain->base, base->domain, domain->domain).
- Implement RECORD field access and ENUM/SET/VARIANT runtime operations.

## Required Data/Schema Changes
- Use the domain tables defined in Plan 10/Plan 06 (authoritative tables live in `sys.cluster.configuration`):
  - `sys.cluster.configuration.domains`, `sys.cluster.configuration.domain_constraints`, `sys.cluster.configuration.domain_fields`
  - `sys.cluster.configuration.domain_enum_values`, `sys.cluster.configuration.domain_enum_options`, `sys.cluster.configuration.domain_variant_types`
  - `sys.cluster.configuration.domain_security`, `sys.cluster.configuration.domain_integrity`
  - `sys.cluster.configuration.domain_validation`, `sys.cluster.configuration.domain_quality`
  - `sys.cluster.configuration.domain_history`, `sys.cluster.configuration.domain_collisions`, `sys.cluster.configuration.domain_collision_members`
  - `sys.cluster.configuration.domain_aliases`
- Add validation reporting for ALTER DOMAIN:
  - `sys.cluster.configuration.domain_validation_reports` (per-node validation results + violating rows)
- User-facing catalog views live in `sys.catalog` without the `sb_` prefix.
- Extend column/parameter records to persist full type descriptors:
  - Add `type_descriptor_oid` (preferred) and `domain_id` fallback fields where needed.

## Completion Checklist (Developer)
- [ ] Domain definitions are loaded from `sys.cluster.configuration.domain_*` tables (no in-memory-only domains).
- [ ] `TypeDescriptor` persists and round-trips for columns/parameters/domains.
- [ ] Domain types resolve to base types while retaining domain_id.
- [ ] Domain constraints/defaults are enforced on INSERT/UPDATE/parameter binding.
- [ ] ALTER DOMAIN performs full validation and reports table_id + primary-key values.
- [ ] Cluster-wide ALTER DOMAIN uses pending status until all members confirm validation.
- [ ] Domain inheritance merges constraints and rejects cycles.
- [ ] Domain casts are checked and enforced (including storage hash checks).
- [ ] RECORD/ENUM/SET/VARIANT runtime operations work and are callable from SQL.

## Completion Checklist (Auditor)
- [ ] Domain DDL and usage survives restart (catalog persistence verified).
- [ ] Domain constraints are enforced consistently in DML and procedural code.
- [ ] ALTER DOMAIN failure reports include table_id + primary-key values.
- [ ] Cluster pending status is cleared only after all members confirm validation.
- [ ] Domain arrays and sub-domain (domain-in-domain) usage works or errors clearly.
- [ ] Named element access for RECORD domains returns correct field values.
- [ ] Domain cast failures are deterministic and explain why.

## Testing Requirements
- SQL tests for CREATE/ALTER/DROP DOMAIN and column usage.
- DML tests for domain constraints, defaults, and inheritance.
- ALTER DOMAIN validation tests (violations reported with table_id/PK).
- Cluster validation tests (pending status until all nodes confirm).
- Array tests for domain[] and SET OF domain.
- RECORD tests for dot access and EXTRACT(field FROM record).
- ENUM tests for ordering and wrap option.
- CAST tests for domain<->base and domain<->domain.
- Restart tests to verify persistence of domain definitions and usage.

## Acceptance Criteria
- Domains behave as first-class types with correct constraints, defaults, and casts.
- Array/sub-domain semantics are consistent across DDL, DML, and runtime.
- RECORD/ENUM/SET/VARIANT operations are usable from SQL and enforced at runtime.

## Implementation Notes (Concrete)
- **DomainManager**: convert to `DomainRuntime` that loads from `CatalogManager` and caches by domain_id.
- **TypeDescriptor**: a single serialized representation for base types, arrays, and domain references.
- **Column storage**: prefer `type_descriptor_oid` for new columns; use legacy fields only when descriptor is empty.
- **Constraint evaluation**: reuse `evaluatePolicyExpression()` by compiling domain CHECK expressions to SBLR bytecode.
- **Default precedence**: column DEFAULT overrides domain DEFAULT; domain DEFAULT applies only when column DEFAULT is absent.
- **Array constraint scope**: constraints may be defined as element-level or array-level. For simple arrays, follow Firebird dialect behavior.
- **Depth limit**: maximum nested domain/record depth is 50 levels (reject deeper nesting).

## Expanded API/Schema Details
- **TypeDescriptor struct** (serialize to TOAST):
  - `data_type` (DataType enum)
  - `precision`, `scale`, `length`
  - `with_time_zone`
  - `is_array`, `array_rank`, `array_size`
  - `domain_id` (optional)
  - `element_type` (DataType) and `element_domain_id` (optional)
- **CatalogManager APIs**:
  - `getDomainDefinition(domain_id, DomainDefinition& out)`
  - `listDomainConstraints(domain_id, vector<DomainConstraint>& out)`
  - `listDomainFields(domain_id, vector<DomainField>& out)`
  - `listDomainEnumValues(domain_id, vector<EnumValue>& out)`
  - `listDomainVariantTypes(domain_id, vector<TypeDescriptor>& out)`
- **DomainRuntime APIs**:
  - `resolveDomainByName(name, dialect_tag, compat_name, DomainDefinition& out)`
  - `validateValue(domain_id, TypedValue value)`
  - `applyDefault(domain_id, TypedValue& value)`
  - `castValue(domain_id, TypedValue value, target_domain_id)`
- **Validation report APIs**:
  - `validateDomainChange(domain_id, DomainAlterSpec, DomainValidationReport& out)`
  - `persistDomainValidationReport(report)`
  - `listDomainValidationReports(domain_id, vector<DomainValidationReport>& out)`

## Full Implementation Detail (No Ambiguity)
### 1) TypeDescriptor and Persistence
- Add `TypeDescriptor` struct in `include/scratchbird/core/types.h` (or new header) and serialize it to a TOAST blob.
- Add `type_descriptor_oid` to:
  - `ColumnRecord` (src/core/catalog_manager.cpp)
  - Procedure/function parameter records
  - Domain field records (`sys.cluster.configuration.domain_fields`)
- When `type_descriptor_oid != 0`, use it instead of `data_type/type_precision/type_scale` fields.

### 2) Semantic Resolution
- Extend `ResolvedType` to include:
  - `domain_id` (optional)
  - `element_domain_id` (optional)
- In `SemanticAnalyzerV2::resolveTypeName`:
  - If name resolves to domain, set `resolved.domain_id` and set `resolved.data_type` to domain base type (from catalog), not `UNKNOWN`.
  - If `type_name.is_array`, carry array flags and resolve element domain if needed.

### 3) SBLR Type Encoding
- Add new opcodes:
  - `TYPE_DOMAIN` (payload: domain UUID)
  - `TYPE_ARRAY` (payload: element type descriptor + optional array size)
- Update bytecode generator:
  - For domain types, emit `TYPE_DOMAIN` instead of primitive `TYPE_*`.
  - For arrays, emit `TYPE_ARRAY` wrapping element descriptor (primitive or domain).
- Update executor to decode `TYPE_DOMAIN`/`TYPE_ARRAY` and build `TypeDescriptor` for catalog insertion.

### 4) Domain DDL Execution
-- Implement `EXT_CREATE_DOMAIN` in executor:
  - Insert into `sys.cluster.configuration.domains` with `domain_kind`, `parent_domain_id`, `base_type_oid`, `default_expr_oid`, `check_expr_oid`, `element_type_oid`, `element_domain_id`.
  - Insert per-domain rows into `sys.cluster.configuration.domain_constraints`, `sys.cluster.configuration.domain_fields`,
    `sys.cluster.configuration.domain_enum_values`, `sys.cluster.configuration.domain_variant_types`, and policy tables.
-- Implement `ALTER DOMAIN` actions:
  - Rename, default change, add/drop/rename constraint, update policy blocks.
  - Record history in `sys.cluster.configuration.domain_history`.
- ALTER validation flow:
  - Evaluate all dependent objects (tables/views/functions/procedures/constraints).
  - For tables, scan rows and collect violating PK values.
  - If violations exist, fail ALTER and return a report with `table_id` and PK values.
  - If in cluster and local validation passes, set `domain_state = PENDING_VALIDATE` until all nodes report success.
  - Persist validation outcomes in `sys.cluster.configuration.domain_validation_reports` for auditing and cross-node reconciliation.
  - `pk_values_oid` payload format: JSON array of objects `{table_id, pk_columns:[column_id], pk_values:[typed_literal]}`; `typed_literal` uses the same literal serialization as CHECK/DEFAULT bytecode literals.
- Implement `DROP DOMAIN` as RESTRICT-only (fail if dependencies exist).

### 5) Runtime Verification and Defaults
- When writing a row (INSERT/UPDATE):
  - If column has `domain_id`, validate value with domain constraints before column CHECK constraints.
  - Apply domain DEFAULT if column value is NULL and column DEFAULT is absent.
- For array domains:
  - Element-level constraints validate each element on assignment.
  - Array-level constraints evaluate against the full array value.
- When binding parameters or variables of domain type:
  - Validate input values against domain constraints immediately.

### 6) Inheritance and Sub-Domains
- Enforce no inheritance cycles (`parent_domain_id` cannot reach itself).
- Enforce max nested depth of 50 when resolving inherited domains and nested record fields.
- Inheritance merges:
  - NOT NULL = true if any ancestor is NOT NULL.
  - DEFAULT = nearest child default (child overrides parent).
  - CHECK constraints = union of all ancestor constraints.
- Record fields may reference domains via `field_domain_id`; resolve those during validation.

### 7) Casting Rules
- Domain -> base type: allowed if storage hash matches base type descriptor.
- Base -> domain: allowed if cast exists AND constraints pass.
- Domain -> domain: allowed if compatible base type (or cast exists) AND target constraints pass.
- Store cast rules in `sys.cluster.configuration.domains.cast_map_oid` as a serialized map of target_type -> cast_fn.

### 8) Named Elements and Advanced Types
- RECORD:
  - Support `ROW(...)::domain` construction and `record.field`/`EXTRACT(field FROM record)` access.
  - Implement `DomainRuntime::extractField()` using `sys.cluster.configuration.domain_fields` definitions.
- ENUM:
  - Implement enum ordering, `SET NEXT VALUE`, and `GET VALUE/POSITION FOR` ops via `sys.cluster.configuration.domain_enum_values`.
- SET:
  - Store SET values as ARRAY with uniqueness enforced; implement `@>` and `&&` operators.
- VARIANT:
  - Store variant type tag and value; implement `EXTRACT(DATATYPE FROM ...)` and `IS OF TYPE`.

## Concrete Test Cases
- CREATE DOMAIN (BASIC/RECORD/ENUM/SET/VARIANT) then SHOW DOMAIN returns full definition.
- Column using domain enforces CHECK and DEFAULT on INSERT.
- Domain inheritance: child gets parent constraints; cycle creation rejected.
- ARRAY of domain: each element validated against domain constraints.
- RECORD: `address.city` returns correct value and type.
- ENUM wrap option enabled/disabled behavior.
- CAST(domain AS base) and CAST(base AS domain) behave as specified.

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

## Order of Implementation
1) Catalog schema changes for cluster domains and conflict tracking.
2) CatalogManager domain APIs (create/alter/drop/resolve/rebind).
3) Conflict detection and reconciliation on join/rejoin.
4) Dependency gating for shadow domains.
5) SHOW DOMAIN + resolver integration.
6) Tests (cluster, conflict, rebind, permissions).

## Implementation Tasks
- Implement cluster-wide domain catalog tables with conflict/history/alias support.
- Extend DomainRecord with dialect/compat/state/origin/hash fields.
- Enforce CREATE/ALTER/DROP DOMAIN privileges (cluster-wide).
- Implement conflict detection during CREATE/ALTER and join/rejoin reconciliation.
- Implement shadow domain gating (existing dependencies allowed, new usage blocked).
- Implement domain rebind (repoint dependencies to new domain UUID).
- Add cluster epoch bump for domain changes to invalidate caches/plans.
- Add SBLR opcodes for ALTER/DROP/REBIND/RESOLVE and include domain metadata in CREATE.

## Required Data/Schema Changes
- `sb_domains` (global domains, no schema_id).
- `sb_domain_collisions` and `sb_domain_collision_members`.
- `sb_domain_history` (audit/epoch).
- `sb_domain_aliases` (optional name mapping).
- Indexes on `(domain_name, dialect_tag)` and `compat_name`.

## Completion Checklist (Developer)
- [ ] Cluster-wide domain tables exist with indexes and on-disk persistence.
- [ ] Domain DDL requires explicit privileges.
- [ ] Conflict detection creates collision entries and shadow domains.
- [ ] UUIDv7 ordering determines canonical domain.
- [ ] Shadow domains block new dependencies.
- [ ] Rebind updates all dependent objects and dependency records.
- [ ] Domain changes bump cluster epoch and invalidate caches.
- [ ] SHOW DOMAIN exposes dialect_tag, compat_name, state, collision info.

## Completion Checklist (Auditor)
- [ ] No schema-scoped domains remain; all domains are global.
- [ ] Conflicts are recorded with deterministic canonical selection.
- [ ] Domain rebind honors cast/storage rules and is fully audited.
- [ ] Cluster join reconciliation merges non-conflicting domains and flags conflicts.

## Testing Requirements
- Cluster replication tests for domain DDL (CREATE/ALTER/DROP).
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
- **Canonical ordering**: compare UUIDv7 timestamps; tie-break with full UUID bytes.
- **Conflict key**: `domain_name|dialect_tag` and `compat_name|*` (if compat provided).
- **State**:
  - `ACTIVE`: can be used for new dependencies.
  - `SHADOW`: only existing dependencies allowed.
  - `DEPRECATED`: allowed but warned; no new usage by default.
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
  - `EXT_CREATE_DOMAIN` payload includes: `domain_id`, `domain_name`, `dialect_tag`, `compat_name`, `owner_id`, `base_type_oid`, `default_expr_oid`, `check_expr_oid`, `cast_map_oid`, `storage_hash`, `definition_hash`, `not_null`, `origin_node_id`, `origin_cluster_id`.
  - `EXT_ALTER_DOMAIN` for rename/default/check/compat updates.
  - `EXT_DROP_DOMAIN` (RESTRICT only).
  - `EXT_REBIND_DOMAIN` (old_id → new_id with mode).
  - `EXT_RESOLVE_DOMAIN_CONFLICT` (collision_id + action).
  - `SBLR_TYPE_DOMAIN` must carry domain UUID.

## Full Implementation Detail (No Ambiguity)
### Catalog DDL (Required)
```sql
CREATE TABLE sb_domains (
  domain_id UUID PRIMARY KEY,
  domain_name TEXT NOT NULL,
  dialect_tag TEXT NOT NULL,
  compat_name TEXT,
  base_type_oid OID NOT NULL,
  default_expr_oid OID,
  check_expr_oid OID,
  cast_map_oid OID,
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
```

### Conflict Algorithm (Deterministic)
1) **Create/Alter in cluster**:
   - Resolve `(domain_name, dialect_tag)` and `compat_name`.
   - If identical `definition_hash`, return existing domain.
   - If conflict:
     - Insert domain as `SHADOW`.
     - Create/update `sb_domain_collisions`.
     - Canonical = smallest UUIDv7 timestamp (tie-break by UUID bytes).
     - Block new dependencies on non-canonical domains.
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
- Records change in `sb_domain_history` and bumps cluster epoch.

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

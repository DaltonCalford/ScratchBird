# Canonical Schema Tree

## Purpose
Define the canonical parent-child schema hierarchy for ScratchBird Alpha so schema layout is deterministic, parser-safe, and implementable without guesswork.

## Scope
- Database object to schema-root relationship.
- Fixed system schema tree.
- Emulation, local, and NoSQL schema branches.
- Naming isolation and uniqueness rules.
- Visibility and ownership boundaries.

## Non-Negotiable Invariants
1. The database object is the logical parent of the root schema.
2. The root schema UUID is exactly equal to the database UUID.
3. The engine stores and resolves object identity by UUID only.
4. Schema names are user-layer identifiers; UUIDs are engine-layer identifiers.
5. Parser dialect differences do not change physical catalog hierarchy.
6. No SQL parser logic is implemented in engine internals.

## Canonical Hierarchy
The following tree is mandatory for every new database. Names are unquoted lowercase identifiers unless noted.

```text
database:<db_uuid>
`-- root  (schema_uuid = <db_uuid>)
    |-- sys
    |   |-- information
    |   |   |-- views
    |   |   |   |-- system
    |   |   |   |-- code
    |   |   |   |-- runtime
    |   |   |   `-- cluster
    |   |   `-- emulation
    |   |-- security
    |   |   |-- catalog
    |   |   |   |-- principals
    |   |   |   |-- authorization
    |   |   |   |-- mapping
    |   |   |   |-- auth
    |   |   |   |-- pki
    |   |   |   |-- crypto
    |   |   |   |-- sessions
    |   |   |   `-- audit
    |   |   `-- overlays
    |   |-- system
    |   |   `-- catalog
    |   |       |-- core
    |   |       |-- relations
    |   |       |-- types
    |   |       |-- code
    |   |       |-- index
    |   |       |-- storage
    |   |       |-- integration
    |   |       |-- replication
    |   |       |-- engine_specific
    |   |       |-- cluster
    |   |       |   `-- routing
    |   |       |-- olap
    |   |       |-- text_search
    |   |       `-- admin
    |   |-- monitor
    |   |   `-- catalog
    |   |       |-- runtime
    |   |       |-- metrics
    |   |       `-- incident
    |   |-- node
    |   |   `-- catalog
    |   |       |-- identity
    |   |       |-- lifecycle
    |   |       `-- capacity
    |   |-- config
    |   |   `-- catalog
    |   `-- jobs
    |       `-- catalog
    |           `-- scheduler
    |-- users
    |   |-- public
    |   |-- roles
    |   |   `-- <role_name>
    |   |-- groups
    |   |   `-- <group_name>
    |   |-- <user_name>
    |   |-- <workgroup_name>
    |   |   `-- <user_name>
    |   `-- <cluster_group_path...>
    |-- remote
    |   |-- emulation
    |   |   |-- firebird
    |   |   |-- postgresql
    |   |   |-- mysql
    |   |   |-- cassandra
    |   |   |-- mongodb
    |   |   |-- neo4j
    |   |   |-- redis
    |   |   `-- milvus
    |   |-- fdw
    |   `-- links
    |-- local
    |   |-- instances
    |   `-- links
    `-- nosql
        |-- cassandra
        |-- mongodb
        |-- neo4j
        |-- redis
        `-- milvus
```

## Required Dynamic Subtree Patterns
The following path templates are created on demand by parser-admin operations.

1. Emulated SQL engine database
`root.remote.emulation.<dialect>.<server_alias>.databases.<emulated_db_name>`

2. Emulated SQL engine system catalogs
`root.remote.emulation.<dialect>.<server_alias>.databases.<emulated_db_name>.catalogs`

3. Local host-side external link
`root.local.links.<engine>.<link_name>`

4. Remote host-side external link
`root.remote.links.<engine>.<link_name>`

5. User home schema
`root.users.<user_name>`

6. Role home schema
`root.users.roles.<role_name>`

7. Group home schema
`root.users.groups.<group_name>`

8. Workgroup user home schema
`root.users.<workgroup_name>.<user_name>`

9. Cluster user home schema (extended grouping)
`root.users.<cluster_scope_path...>.<user_name>`

## Fixed Branch Semantics
1. `root.sys`
- Reserved for internal control-plane metadata and system-managed objects.
- Direct user DDL in `root.sys` is denied except for privileged maintenance operations.
- All catalog object placement under `root.sys` is normative in `CATALOG_OBJECT_SCHEMA_BRANCH_ASSIGNMENT.md`.

2. `root.users`
- Reserved for user-visible native objects and user home scoping.
- `root.users.public` is the default native schema target unless overridden by session settings.
- Reserved first-level tokens in this branch: `public`, `roles`, `groups`.
- Personal and grouped home schemas are resolved under this branch.
- Workgroup and cluster deployments may add additional user grouping levels under `root.users`.

3. `root.remote`
- Reserved for emulation overlays and remote integration metadata.
- Emulated database creation always allocates under `root.remote.emulation`.

4. `root.local`
- Reserved for same-host instance links and local foreign endpoints.
- No direct internet-exposed endpoint metadata is stored here.

5. `root.nosql`
- Reserved for NoSQL emulation and NoSQL adapter metadata paths.
- SQL parser visibility to this branch is profile-controlled.

6. `root.sys.information`
- Contains read-only metadata views and compatibility overlays.
- Direct writes to objects in this branch are prohibited.

7. `root.sys.system.catalog.*`
- Contains authoritative on-disk catalog tables.
- Sub-branches isolate object families (core, relations, types, index, storage, integration, cluster, and others).

8. `root.sys.monitor.catalog.*`
- Contains persisted monitoring, metrics, incident, and runtime-attribution tables.
- Visibility to non-native parsers is profile-gated.

## Name Isolation Rules
1. Parent-local uniqueness
- Within a single parent schema UUID, child names must be unique under normalization rules.

2. Native normalization
- Unquoted names are case-insensitive.
- Quoted names are case-sensitive and exact.
- If `"myTable"` exists, all unquoted or differently cased variants that normalize to `mytable` are blocked in the same parent.

3. Delimiter rules
- Period (`.`) and space are forbidden in unquoted identifiers.
- Delimited identifiers may include period and space.

4. Cross-parent allowance
- Same child name may exist under different parents.

5. Reserved names in `root.users`
- `public`, `roles`, and `groups` are reserved first-level names and cannot be used as personal user schema names.
- Workgroup or cluster labels equal to reserved names are invalid.

## Schema Type Tags
Every schema row must include `schema_type` with one of these values:
- `SYSTEM_FIXED`
- `SYSTEM_DYNAMIC`
- `USER`
- `EMULATION_ROOT`
- `EMULATION_DYNAMIC`
- `LOCAL_LINK`
- `REMOTE_LINK`
- `NOSQL`

## Ownership and ACL Defaults
1. System fixed schemas
- Owner: `SYSTEM`
- ACL baseline: system-only write, policy-governed read.

2. User schemas
- Owner: creator principal or mapped group principal.
- ACL baseline: owner full control, inherited grants per security policy.

3. Emulation dynamic schemas
- Owner: emulation service principal for that dialect profile.
- ACL baseline: parser-gated surface plus security policy filters.

## Prohibited Layouts
The following layouts are invalid and must fail validation:
1. Root schema not equal to database UUID.
2. Any missing fixed first-level branch (`sys`, `users`, `remote`, `local`, `nosql`).
3. Emulated database path created outside `root.remote.emulation.<dialect>`.
4. User-created schemas directly under `root.sys`.
5. Dialect-specific physical catalog duplication outside canonical catalog storage.

## Validation Queries (Normative)
Implementations must provide equivalent internal checks. Native SQL examples:

```sql
-- Root UUID invariant
SELECT 1
FROM "schema" s
JOIN "database" d ON d.database_id = s.database_id
WHERE s.schema_name = 'root'
  AND s.parent_schema_id IS NULL
  AND s.schema_id = d.database_id;

-- Required first-level children
SELECT child.schema_name
FROM "schema" root
JOIN "schema" child ON child.parent_schema_id = root.schema_id
WHERE root.schema_name = 'root'
  AND child.schema_name IN ('sys','users','remote','local','nosql');
```

## Test Contract Addendum
These tests are mandatory in addition to section-level contracts:
1. Bootstrap tree completeness test.
2. Root UUID equality test.
3. Name normalization collision test (`mytable`, `MYTABLE`, `"myTable"` scenarios).
4. Emulated database path placement test per supported dialect.
5. ACL default assignment test for each branch type.
6. Catalog branch assignment test for every object listed in `CATALOG_OBJECT_SCHEMA_BRANCH_ASSIGNMENT.md`.
7. Session home-schema assignment tests:
- personal home schema (`root.users.<user_name>`)
- role home schema (`root.users.roles.<role_name>`)
- group home schema (`root.users.groups.<group_name>`)
- workgroup home schema (`root.users.<workgroup>.<user_name>`)
- default fallback (`root.users.public`)
8. Search-path precedence tests for duplicate object names across schemas.

## Legacy Mapping Summary
- Legacy roots that used `app` or incomplete `remote` branches map to this canonical tree using migration rules in `docs/specifications/work/migration_notes/LEGACY_SCHEMA_TREE_DELTA.md`.

## Resolved Review Decisions
1. Canonical personal schema token is normalized login name (`root.users.<user_name>`); object identity and collision protection remain UUID-based in catalog internals.

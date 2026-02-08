# Catalog Review and System Domains Proposal
Date: 2026-02-07
Scope: ScratchBird system catalog layout (current + beta/planned) and proposal for system domains to replace raw base datatypes in catalog column definitions.

## Sources Reviewed
- `docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`
- `docs/specifications/catalog/CATALOG_CORRECTION_PLAN.md`
- `src/core/catalog_manager.cpp`
- `docs/specifications/types/DDL_DOMAINS_COMPREHENSIVE.md`
- `docs/specifications/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md`
- `docs/specifications/types/03_TYPES_AND_DOMAINS.md`
- `docs/specifications/types/README.md`
- `docs/specifications/storage/TOAST_LOB_STORAGE.md`
- `docs/specifications/storage/HEAP_TOAST_INTEGRATION.md`
- `docs/specifications/storage/ON_DISK_FORMAT.md`
- `docs/specifications/storage/STORAGE_ENGINE_PAGE_MANAGEMENT.md`
- `docs/specifications/udr_connectors/postgresql_udr/SPECIFICATION.md`
- `docs/archive/2026-01-04/planning/plan_10_cluster_domains_and_conflict_resolution.md`

## Current Catalog Snapshot (What Exists Today)
The current authoritative catalog layout is documented in `SYSTEM_CATALOG_STRUCTURE.md` and largely mirrors the on-disk structs in `src/core/catalog_manager.cpp`.

Key observations:
- Catalog root page contains >60 table pointers (schemas, tables, columns, indexes, security, jobs, UDRs, FDW, etc.).
- Schema hierarchy is recursive: parent pointer (`parent_schema_id`) + local name. Full path is derived, not stored.
- On-disk records heavily use a small set of base datatypes (IDs/UUIDs, fixed-size name buffers, u8/u16/u32/u64, and TOAST OIDs).

### Base Datatypes Used by On-Disk Records
(From `SYSTEM_CATALOG_STRUCTURE.md` record structs)

- `ID` (UUID) and `ID[16]`
- `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`
- `int32_t`, `int64_t`
- `float`, `double`
- `char[6]`, `char[16]`, `char[32]`, `char[64]`, `char[128]`, `char[256]`, `char[512]`, `char[1024]`

These map cleanly to a system domain set (see proposal below).

## Beta/Proposed Catalog Additions
### From `SYSTEM_CATALOG_STRUCTURE.md` (Planned Migration Catalogs)
- Tablespace migration state (`TablespaceMigrationRecord`)
- Shard migration state (`ShardMigrationRecord`) [Beta, Cluster]
- Shard map versions (`ShardMapVersionRecord`) [Beta, Cluster]

### From `CATALOG_CORRECTION_PLAN.md`
- Explicit dependency table
- Comment table
- Object definitions table
- Expanded security tables
- Synonyms / FDW / server registry / UDR engines/modules
- Migration history, dormant/prepared transaction tables

### From Cluster Domain Plan (Archived)
- `sys.cluster.configuration.*` tables
- `sys.catalog.*` views layered on `sys.cluster.configuration.*`

This is currently archived but is the only detailed spec for domain conflict/rebind structures and cluster-side domain metadata. It should be reconciled explicitly with the main catalog layout before beta.

## Conflicts and Drift (Must Resolve Before Final Catalog Design)
1. **Schema hierarchy path drift**
   - `SYSTEM_CATALOG_STRUCTURE.md` uses `/remote/emulation/<dialect>` and `/users/public`.
   - `CATALOG_CORRECTION_PLAN.md` uses `/emulation/<dialect>` and `public` under root.
   - Decision needed: canonical root for emulation schemas and default `public` location.

2. **Security schema naming drift**
   - `SYSTEM_CATALOG_STRUCTURE.md` shows `/sys/sec/sec_users`.
   - `CATALOG_CORRECTION_PLAN.md` shows `/sys/sec/users`.
   - Need a single canonical path; the recursive schema model makes this a permanent API decision.

3. **CatalogRootPage field mismatch**
   - `src/core/catalog_manager.cpp` includes `default_privileges_page`.
   - `SYSTEM_CATALOG_STRUCTURE.md` omits `default_privileges_page` in the root page definition.
   - Recommendation: update spec to include it or remove from implementation.

4. **Domain catalog placement mismatch**
   - `SYSTEM_CATALOG_STRUCTURE.md` places domains in `sys.domains`.
   - `DDL_DOMAINS_COMPREHENSIVE.md` references `sys.catalog.domains`.
   - Archived cluster plan uses `sys.cluster.configuration.domains` with `sys.catalog.domains` view.
   - Decision needed: final authoritative path for domain metadata (and whether `sys.catalog` is a view-only namespace).

5. **Catalog root page IDs**
   - `SYSTEM_CATALOG_STRUCTURE.md` claims Catalog Root Page is Page 3.
   - `src/core/database.cpp` shows a system catalog bootstrap page at Page 1.
   - `ON_DISK_FORMAT.md` says DatabaseHeader points to `system_catalog_page` (usually 1).
   - Clarify: which page is the bootstrap vs catalog root and update specs accordingly.

6. **UUID version requirements**
   - `ON_DISK_FORMAT.md` mandates UUID v7 for all on-disk UUIDs.
   - Catalog structs use `ID`/UUID without documenting v7 enforcement.
   - System domains should encode v7 as the canonical UUID variant used by catalog columns.

7. **TOAST OID vs UUID usage**
   - `DATA_TYPE_PERSISTENCE_AND_CASTS.md` + `TOAST_LOB_STORAGE.md` define TOAST pointers using UUIDs and varlen payloads.
   - Catalog columns use `*_oid` as `uint32` references.
   - Confirm whether catalog TOAST references remain 32-bit OIDs or need to migrate to UUID-based references.

## Owner Responses (Applied Decisions)
1. **Identifiers and UUIDs**
   - All surrogate key identifiers are UUID v7.
   - `ID[16]` is treated as UUID v7.
   - UUID v7 is the default for UUID unless explicitly stated otherwise.
   - Catalog identifiers must use `SBDB$KEY_*` domains; `SBDB$LOB_REF` is only for TOAST/LOB references.

2. **Domain naming and visibility**
   - Use purpose-specific domain names for identifiers (e.g., `SBDB$KEY_TABLE`, `SBDB$KEY_COLUMN`) even if they map to the same base UUID v7.
   - All system domains/types must use the `SBDB$` prefix to avoid collisions with user domains.

3. **Schema hierarchy and emulation**
   - `public` lives under `users` (e.g., `users.public`).
   - Role and group schemas are under `users.roles.{rolename}` and `users.groups.{groupname}`.
   - General users may be under `users.{username}` but should be rare.
   - Remote schema path is `remote/<server>/<dialect>/<database>`.
   - Emulated servers are local and live under `emulated` at root.

4. **Security schema paths**
   - Canonical paths: `sys.sec.users`, `sys.sec.groups`, `sys.sec.roles`, etc.

5. **Catalog root page**
   - Catalog root page is page 1 (bootstrap + root pointer page).

6. **Domain catalog placement**
   - Canonical store: `sys.cluster.configuration.domains`.
   - `sys.catalog.domains` should exist as a view or synonym.

## Additional Notes From Type/Storage/UDR Specs
1. **Canonical on-disk encoding is type-driven**
   - `ON_DISK_FORMAT.md` + `DATA_TYPE_PERSISTENCE_AND_CASTS.md` describe byte-level layouts (endianness, fixed/varlen markers, TOAST indirection).
   - The catalog should not introduce alternate encodings; it should refer to the same domains that map to these canonical encodings.

2. **TOAST pointer structure is UUID-based**
   - `TOAST_LOB_STORAGE.md` defines TOAST pointers as UUID-based references (plus inline thresholds and varlen framing).
   - If catalog columns keep `*_oid` as `uint32`, then TOAST pointer usage must be confined to the heap/varlen layer only.
   - If catalog columns can reference TOAST blobs directly (e.g., long definitions), then the catalog should use a UUID-based domain instead of legacy OID/`SBDB$U32` references.

3. **UUID v7 should be explicit in domains**
   - `ON_DISK_FORMAT.md` requires v7 UUIDs.
   - Recommend using `SBDB$UUID_V7` for all catalog identifiers that are stored on disk.

4. **Database header vs catalog root page**
   - `STORAGE_ENGINE_PAGE_MANAGEMENT.md` + `ON_DISK_FORMAT.md` indicate the database header points to `system_catalog_page` (usually page 1).
   - `SYSTEM_CATALOG_STRUCTURE.md` refers to a catalog root page at page 3.
   - Clarify which is the bootstrap page and which contains the table pointer array; update both docs and code to a single canonical description.

5. **FDW/UDR catalog coverage**
   - `udr_connectors/postgresql_udr/SPECIFICATION.md` implies `sys.foreign_servers`, `sys.user_mappings`, and `sys.udr_*` tables need stable option payloads and owner/ACL domains.
   - Ensure catalog columns for FDW/UDR options use a consistent domain (likely a TOAST-backed `SBDB$LOB_REF` or UUID-based large object reference).

## System Domain Proposal (Replace Raw Catalog Types)
Goal: all catalog columns should reference system domains instead of raw datatypes. These domains should be defined before catalog load (engine-resident).

### Naming Scheme
Use Firebird-style system domains with `SBDB$` prefix. For identifier domains, use purpose-specific names (e.g., `SBDB$KEY_TABLE`, `SBDB$KEY_COLUMN`) that map to UUID v7. **All catalog identifiers and foreign keys must use a `SBDB$KEY_<OBJECT>` domain that names the target object type**, so relationships are visible from the column type alone. Keep a visual distinction between system and user-defined domains.

### Core Domain Set (Recommended)
| Domain Name | Base Type | Constraints / Semantics | Typical Usage |
| --- | --- | --- | --- |
| `SBDB$UUID_V7` | `UUID` | UUID v7 only (time-ordered) | All on-disk identifiers |
| `SBDB$KEY_<OBJECT>` | `UUID` | UUID v7 | Generic pattern for every catalog object id and FK (e.g., `SBDB$KEY_TABLE`, `SBDB$KEY_INDEX`, `SBDB$KEY_VIEW`, `SBDB$KEY_TRIGGER`, `SBDB$KEY_SEQUENCE`, `SBDB$KEY_PROC`, `SBDB$KEY_FUNC`, `SBDB$KEY_SCHEMA`, `SBDB$KEY_COLUMN`, `SBDB$KEY_DOMAIN`, `SBDB$KEY_TABLESPACE`, `SBDB$KEY_USER`, `SBDB$KEY_ROLE`, `SBDB$KEY_GROUP`, `SBDB$KEY_SERVER`, `SBDB$KEY_UDR_MODULE`, `SBDB$KEY_UDR_ENGINE`, `SBDB$KEY_POLICY`, `SBDB$KEY_RULE`, `SBDB$KEY_JOB`, `SBDB$KEY_SESSION`, `SBDB$KEY_TXN`) |
| `SBDB$KEY_TABLE` | `UUID` | UUID v7 | Table identifiers |
| `SBDB$KEY_COLUMN` | `UUID` | UUID v7 | Column identifiers |
| `SBDB$KEY_SCHEMA` | `UUID` | UUID v7 | Schema identifiers |
| `SBDB$KEY_DOMAIN` | `UUID` | UUID v7 | Domain identifiers |
| `SBDB$KEY_INDEX` | `UUID` | UUID v7 | Index identifiers |
| `SBDB$KEY_TABLESPACE` | `UUID` | UUID v7 | Tablespace identifiers |
| `SBDB$KEY_USER` | `UUID` | UUID v7 | User identifiers |
| `SBDB$KEY_ROLE` | `UUID` | UUID v7 | Role identifiers |
| `SBDB$KEY_GROUP` | `UUID` | UUID v7 | Group identifiers |
| `SBDB$KEY_SERVER` | `UUID` | UUID v7 | FDW/foreign server identifiers |
| `SBDB$KEY_OBJECT` | `UUID` | UUID v7 | Polymorphic object references with an object type discriminator |
| `SBDB$KEY_PRINCIPAL` | `UUID` | UUID v7 | Polymorphic user/role/group references with a type discriminator |
| `SBDB$KEY_TXN` | `UUID` | UUID v7 | Transaction identifiers (time-ordered) |
| `SBDB$KEY_COLLATION` | `UUID` | UUID v7 | Collation identifiers |
| `SBDB$KEY_CHARSET` | `UUID` | UUID v7 | Character set identifiers |
| `SBDB$KEY_TIMEZONE` | `UUID` | UUID v7 | Timezone identifiers |
| `SBDB$NAME` | `VARCHAR(128)` | Identifier (UTF-8, case-insensitive collation) | all identifiers |
| `SBDB$NAME_64` | `VARCHAR(64)` | Short non-identifier labels | object type names, short codes |
| `SBDB$NAME_256` | `VARCHAR(256)` | Mid paths | cluster IDs, etc. |
| `SBDB$NAME_512` | `VARCHAR(512)` | Long names | object names, hostnames |
| `SBDB$NAME_1024` | `VARCHAR(1024)` | Paths | UDR library paths |
| `SBDB$BOOL` | `BOOLEAN` | 0/1 | flags currently stored as `uint8_t` |
| `SBDB$U8` | `UINT8` | 0..255 | enum storage |
| `SBDB$U16` | `UINT16` | 0..65535 | small numeric codes |
| `SBDB$U32` | `UINT32` | 32-bit unsigned | counters |
| `SBDB$U64` | `UINT64` | 64-bit unsigned | timestamps, XIDs, row counts |
| `SBDB$I32` | `INT32` | 32-bit signed | error codes, offsets |
| `SBDB$I64` | `INT64` | 64-bit signed | row deltas |
| `SBDB$PAGE_ID` | `UINT32` | page number | catalog page pointers |
| `SBDB$F32` | `FLOAT` | IEEE-754 | performance metrics |
| `SBDB$F64` | `DOUBLE` | IEEE-754 | performance metrics |
| `SBDB$LOB_REF` | `UUID` | UUID v7 | TOAST/LOB reference (replaces all `*_oid` fields) |
| `SBDB$TIME_US` | `UINT64` | microseconds since epoch | created/modified timestamps |
| `SBDB$SQLSTATE` | `CHAR(5)` | SQLSTATE code | error catalog, audit |
| `SBDB$HASH256` | `BINARY(32)` | 32-byte hash | audit chain |

Note: these domains should be visible as system domains in `sys.domains` (or `sys.catalog.domains` if you choose view indirection).

### Enum and Bitmask Domains (Recommended)
Instead of raw `uint8_t`/`uint16_t` for enums and flags, define explicit domains:
- `SBDB$OBJTYPE` (object type enum)
- `SBDB$SCHEMA_TYPE`
- `SBDB$INDEX_TYPE`
- `SBDB$TABLE_TYPE`
- `SBDB$POLICY_TYPE`
- `SBDB$SECURITY_FLAGS`
- `SBDB$PERMISSIONS_MASK` (u16/u32)

## Canonical SBDB$KEY_* Domain Registry (System Objects)
All catalog identifiers and foreign keys must use a purpose-specific `SBDB$KEY_<OBJECT>` domain that names the target object type. The list below is the authoritative registry for system object identifiers.

| SBDB$KEY Domain | Object Type / Notes |
| --- | --- |
| `SBDB$KEY_SCHEMA` | Schema identifiers |
| `SBDB$KEY_TABLE` | Table identifiers |
| `SBDB$KEY_COLUMN` | Column identifiers |
| `SBDB$KEY_INDEX` | Index identifiers (including logical index IDs) |
| `SBDB$KEY_CONSTRAINT` | Constraint identifiers |
| `SBDB$KEY_SEQUENCE` | Sequence identifiers |
| `SBDB$KEY_VIEW` | View identifiers |
| `SBDB$KEY_TRIGGER` | Trigger identifiers |
| `SBDB$KEY_PERMISSION` | Permission record identifiers |
| `SBDB$KEY_COLUMN_PERMISSION` | Column permission record identifiers |
| `SBDB$KEY_OBJECT_PERMISSION` | Object permission record identifiers |
| `SBDB$KEY_POLICY` | Row-level security policy identifiers |
| `SBDB$KEY_STATISTIC` | Statistics record identifiers |
| `SBDB$KEY_DEPENDENCY` | Dependency record identifiers |
| `SBDB$KEY_TIMEZONE` | Timezone identifiers (convert from numeric IDs) |
| `SBDB$KEY_CHARSET` | Character set identifiers (convert from numeric IDs) |
| `SBDB$KEY_COLLATION` | Collation identifiers (convert from numeric IDs) |
| `SBDB$KEY_COMMENT` | Comment identifiers |
| `SBDB$KEY_OBJECT_DEFINITION` | Object definition identifiers |
| `SBDB$KEY_USER` | User identifiers |
| `SBDB$KEY_ROLE` | Role identifiers |
| `SBDB$KEY_GROUP` | Group identifiers |
| `SBDB$KEY_ROLE_MEMBERSHIP` | Role membership identifiers |
| `SBDB$KEY_GROUP_MEMBERSHIP` | Group membership identifiers |
| `SBDB$KEY_GROUP_MAPPING` | External group mapping identifiers |
| `SBDB$KEY_AUTHKEY` | Authentication key identifiers |
| `SBDB$KEY_SESSION` | Session identifiers |
| `SBDB$KEY_AUDIT_EVENT` | Audit log event identifiers |
| `SBDB$KEY_PROCEDURE` | Procedure/function identifiers |
| `SBDB$KEY_PROC_PARAM` | Procedure/function parameter identifiers |
| `SBDB$KEY_DOMAIN` | Domain identifiers |
| `SBDB$KEY_UDR` | UDR identifiers |
| `SBDB$KEY_PACKAGE` | Package identifiers |
| `SBDB$KEY_EXCEPTION` | Exception identifiers |
| `SBDB$KEY_EMULATION_TYPE` | Emulation type identifiers |
| `SBDB$KEY_EMULATION_SERVER` | Emulation server identifiers |
| `SBDB$KEY_EMULATED_DB` | Emulated database identifiers |
| `SBDB$KEY_SYNONYM` | Synonym identifiers |
| `SBDB$KEY_FOREIGN_SERVER` | Foreign server identifiers |
| `SBDB$KEY_FOREIGN_TABLE` | Foreign table identifiers |
| `SBDB$KEY_USER_MAPPING` | User mapping identifiers |
| `SBDB$KEY_SERVER_REGISTRY` | Cluster server registry identifiers |
| `SBDB$KEY_UDR_ENGINE` | UDR engine identifiers |
| `SBDB$KEY_UDR_MODULE` | UDR module identifiers |
| `SBDB$KEY_FOREIGN_KEY` | Foreign key identifiers |
| `SBDB$KEY_MIGRATION_HISTORY` | Tablespace migration history identifiers |
| `SBDB$KEY_MIGRATION` | Migration job identifiers |
| `SBDB$KEY_DORMANT_TXN` | Dormant transaction identifiers |
| `SBDB$KEY_PREPARED_TXN` | Prepared transaction identifiers |
| `SBDB$KEY_TABLESPACE` | Tablespace identifiers |
| `SBDB$KEY_SHARD` | Shard identifiers |
| `SBDB$KEY_SHARD_MIGRATION` | Shard migration identifiers |
| `SBDB$KEY_SHARD_MAP_VERSION` | Shard map version identifiers |
| `SBDB$KEY_ATTACHMENT` | Client attachment identifiers |
| `SBDB$KEY_SERVER_INSTANCE` | Server instance identifiers |
| `SBDB$KEY_DATABASE` | Database identifiers |
| `SBDB$KEY_TXN` | Transaction identifiers (time-ordered) |
| `SBDB$KEY_OBJECT` | Polymorphic object references (must be paired with `SBDB$OBJTYPE`) |
| `SBDB$KEY_PRINCIPAL` | Polymorphic user/role/group references (must be paired with type indicator) |

## Mapping Guidance (Representative Examples)
Below are representative table mappings. The intent is to apply the same mapping across all catalog tables.

### `sys.schemas`
- `schema_id` -> `SBDB$KEY_SCHEMA`
- `parent_schema_id` -> `SBDB$KEY_SCHEMA`
- `schema_name` -> `SBDB$NAME`
- `owner_id` -> `SBDB$KEY_USER`
- `default_tablespace_id` -> `SBDB$KEY_TABLESPACE`
- `permissions` -> `SBDB$PERMISSIONS_MASK`
- `default_charset` -> `SBDB$KEY_CHARSET`
- `default_collation_id` -> `SBDB$KEY_COLLATION`
- `acl_oid` -> `SBDB$LOB_REF`
- `created_time` / `last_modified_time` -> `SBDB$TIME_US`
- `is_valid` -> `SBDB$BOOL`

### `sys.tables`
- `table_id` -> `SBDB$KEY_TABLE`
- `schema_id` -> `SBDB$KEY_SCHEMA`
- `owner_id` -> `SBDB$KEY_USER`
- `table_name` -> `SBDB$NAME`
- `root_gpid` -> `SBDB$U64`
- `column_count` -> `SBDB$U32`
- `row_count` -> `SBDB$U64`
- `table_type` -> `SBDB$TABLE_TYPE`
- `tablespace_id` -> `SBDB$KEY_TABLESPACE`
- `default_charset` -> `SBDB$KEY_CHARSET`
- `default_collation_id` -> `SBDB$KEY_COLLATION`
- `storage_params_oid` -> `SBDB$LOB_REF`
- `policy_epoch` -> `SBDB$U64`
- `created_time` / `last_modified_time` -> `SBDB$TIME_US`
- `is_valid` -> `SBDB$BOOL`

### `sys.columns`
- `column_id` -> `SBDB$KEY_COLUMN`
- `table_id` -> `SBDB$KEY_TABLE`
- `domain_id` -> `SBDB$KEY_DOMAIN`
- `column_name` -> `SBDB$NAME`
- `ordinal` -> `SBDB$U16`
- `data_type` -> `SBDB$U16` (eventually `SBDB$DATA_TYPE`) 
- `type_precision` / `type_scale` -> `SBDB$U32`
- `charset` -> `SBDB$KEY_CHARSET`
- `collation_id` -> `SBDB$KEY_COLLATION`
- `default_value_oid` / `check_expr_oid` -> `SBDB$LOB_REF`
- `created_time` -> `SBDB$TIME_US`
- `is_valid` -> `SBDB$BOOL`

### `sys.domains`
- `domain_id` -> `SBDB$KEY_DOMAIN`
- `schema_id` -> `SBDB$KEY_SCHEMA`
- `parent_domain_id` -> `SBDB$KEY_DOMAIN`
- `domain_name` -> `SBDB$NAME`
- `base_type` -> `SBDB$DATA_TYPE` (domain over base datatypes)
- `precision`, `scale` -> `SBDB$U32`
- `nullable` -> `SBDB$BOOL`
- `default_value` -> `SBDB$TEXT_256` (or OID-backed) 
- `constraints_oid` / `fields_oid` / etc -> `SBDB$LOB_REF`

### `sys.audit_log`
- `event_id` -> `SBDB$KEY_AUDIT_EVENT`
- `timestamp` -> `SBDB$TIME_US`
- `event_type` -> `SBDB$U16`
- `success` -> `SBDB$BOOL`
- `session_id` -> `SBDB$KEY_SESSION`
- `user_id` -> `SBDB$KEY_USER`
- `role_id` -> `SBDB$KEY_ROLE`
- `object_id` -> `SBDB$KEY_OBJECT` (pair with `SBDB$OBJTYPE`)
- `username`, `target_username` -> `SBDB$NAME`
- `object_name` -> `SBDB$NAME_512`
- `details_oid` -> `SBDB$LOB_REF`
- `hash_prev`, `hash_curr` -> `SBDB$HASH256`

## Comprehensive Catalog Domain Map (All Columns)
This section maps every column in `SYSTEM_CATALOG_STRUCTURE.md` to its required `SBDB$` system domain. Reserved/padding bytes are omitted.

### Structure (CatalogRootPage)
**Record**: `CatalogRootPage`

| Column | Domain | Notes |
| --- | --- | --- |
| `header` | `SBDB$U32` |  |
| `schema_count` | `SBDB$U32` |  |
| `table_count` | `SBDB$U32` |  |
| `schemas_page` | `SBDB$PAGE_ID` |  |
| `tables_page` | `SBDB$PAGE_ID` |  |
| `columns_page` | `SBDB$PAGE_ID` |  |
| `indexes_page` | `SBDB$PAGE_ID` |  |
| `constraints_page` | `SBDB$PAGE_ID` |  |
| `sequences_page` | `SBDB$PAGE_ID` |  |
| `views_page` | `SBDB$PAGE_ID` |  |
| `triggers_page` | `SBDB$PAGE_ID` |  |
| `permissions_page` | `SBDB$PAGE_ID` |  |
| `statistics_page` | `SBDB$PAGE_ID` |  |
| `collations_page` | `SBDB$PAGE_ID` |  |
| `timezones_page` | `SBDB$PAGE_ID` |  |
| `charsets_page` | `SBDB$PAGE_ID` |  |
| `collation_defs_page` | `SBDB$PAGE_ID` |  |
| `dependencies_page` | `SBDB$PAGE_ID` |  |
| `comments_page` | `SBDB$PAGE_ID` |  |
| `object_definitions_page` | `SBDB$PAGE_ID` |  |
| `jobs_page` | `SBDB$PAGE_ID` |  |
| `job_runs_page` | `SBDB$PAGE_ID` |  |
| `job_dependencies_page` | `SBDB$PAGE_ID` |  |
| `job_secrets_page` | `SBDB$PAGE_ID` |  |
| `users_page` | `SBDB$PAGE_ID` |  |
| `roles_page` | `SBDB$PAGE_ID` |  |
| `groups_page` | `SBDB$PAGE_ID` |  |
| `role_members_page` | `SBDB$PAGE_ID` |  |
| `group_members_page` | `SBDB$PAGE_ID` |  |
| `group_mappings_page` | `SBDB$PAGE_ID` |  |
| `procedures_page` | `SBDB$PAGE_ID` |  |
| `proc_params_page` | `SBDB$PAGE_ID` |  |
| `domains_page` | `SBDB$PAGE_ID` |  |
| `udr_page` | `SBDB$PAGE_ID` |  |
| `exceptions_page` | `SBDB$PAGE_ID` |  |
| `packages_page` | `SBDB$PAGE_ID` |  |
| `emulation_types_page` | `SBDB$PAGE_ID` |  |
| `emulation_servers_page` | `SBDB$PAGE_ID` |  |
| `emulated_dbs_page` | `SBDB$PAGE_ID` |  |
| `tablespaces_page` | `SBDB$PAGE_ID` |  |
| `tablespace_files_page` | `SBDB$PAGE_ID` |  |
| `extensions_page` | `SBDB$PAGE_ID` |  |
| `foreign_keys_page` | `SBDB$PAGE_ID` |  |
| `synonyms_page` | `SBDB$PAGE_ID` |  |
| `foreign_servers_page` | `SBDB$PAGE_ID` |  |
| `foreign_tables_page` | `SBDB$PAGE_ID` |  |
| `user_mappings_page` | `SBDB$PAGE_ID` |  |
| `server_registry_page` | `SBDB$PAGE_ID` |  |
| `udr_engines_page` | `SBDB$PAGE_ID` |  |
| `udr_modules_page` | `SBDB$PAGE_ID` |  |
| `migration_history_page` | `SBDB$PAGE_ID` |  |
| `dormant_transactions_page` | `SBDB$PAGE_ID` |  |
| `prepared_transactions_page` | `SBDB$PAGE_ID` |  |
| `encryption_keys_page` | `SBDB$PAGE_ID` |  |
| `authkeys_page` | `SBDB$PAGE_ID` |  |
| `sessions_page` | `SBDB$PAGE_ID` |  |
| `audit_log_page` | `SBDB$PAGE_ID` |  |
| `security_policy_epoch_page` | `SBDB$PAGE_ID` |  |
| `policy_toast_table_id` | `SBDB$KEY_TABLE` |  |
| `column_permissions_page` | `SBDB$PAGE_ID` |  |
| `object_permissions_page` | `SBDB$PAGE_ID` |  |
| `policies_page` | `SBDB$PAGE_ID` |  |

### Schemas Table (`schemas_page`)
**Record**: `SchemaRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `parent_schema_id` | `SBDB$KEY_SCHEMA` |  |
| `schema_name` | `SBDB$NAME` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `default_tablespace_id` | `SBDB$KEY_TABLESPACE` | Convert from uint16 |
| `permissions` | `SBDB$PERMISSIONS_MASK` |  |
| `default_charset` | `SBDB$KEY_CHARSET` | Convert from uint16 |
| `name_is_delimited` | `SBDB$BOOL` |  |
| `default_collation_id` | `SBDB$U32` |  |
| `acl_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Tables Table (`tables_page`)
**Record**: `TableInfo`

| Column | Domain | Notes |
| --- | --- | --- |
| `table_id` | `SBDB$KEY_TABLE` |  |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `table_name` | `SBDB$NAME` |  |
| `name_is_delimited` | `SBDB$BOOL` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `root_page` | `SBDB$PAGE_ID` |  |
| `column_count` | `SBDB$U32` |  |
| `row_count` | `SBDB$U64` |  |
| `table_type` | `SBDB$U32` |  |
| `temp_metadata_scope` | `SBDB$U32` |  |
| `temp_data_scope` | `SBDB$U32` |  |
| `temp_on_commit` | `SBDB$U32` |  |
| `creating_session_id` | `SBDB$KEY_SESSION` |  |
| `creating_transaction_id` | `SBDB$U64` |  |
| `temp_parent_table_id` | `SBDB$KEY_TABLE` |  |
| `temp_schema_id` | `SBDB$KEY_SCHEMA` |  |
| `has_toast` | `SBDB$U32` |  |
| `toast_table_id` | `SBDB$UUID_V7` |  |
| `tablespace_id` | `SBDB$KEY_TABLESPACE` | Convert from uint16 |
| `default_charset` | `SBDB$KEY_CHARSET` | Convert from uint16 |
| `default_collation_id` | `SBDB$U32` |  |
| `storage_params_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `policy_epoch` | `SBDB$U64` |  |
| `migration_in_progress` | `SBDB$U32` |  |
| `migration_id` | `SBDB$KEY_MIGRATION` |  |
| `migration_xid` | `SBDB$KEY_TXN` | Convert from uint64 |
| `migration_target_ts` | `SBDB$U16` |  |
| `migration_phase` | `SBDB$U8` | Define enum domain if needed |
| `rls_enabled` | `SBDB$U32` |  |
| `rls_forced` | `SBDB$U32` |  |

### Columns Table (`columns_page`)
**Record**: `ColumnRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `table_id` | `SBDB$KEY_TABLE` |  |
| `column_id` | `SBDB$KEY_COLUMN` |  |
| `column_name` | `SBDB$NAME` |  |
| `ordinal` | `SBDB$U16` |  |
| `data_type` | `SBDB$U16` |  |
| `type_precision` | `SBDB$U32` |  |
| `type_scale` | `SBDB$U32` |  |
| `max_length` | `SBDB$U32` |  |
| `domain_id` | `SBDB$KEY_DOMAIN` |  |
| `is_array` | `SBDB$BOOL` |  |
| `array_size` | `SBDB$U32` |  |
| `nullable` | `SBDB$U8` | Define enum domain if needed |
| `has_default` | `SBDB$U8` | Define enum domain if needed |
| `is_primary_key` | `SBDB$BOOL` |  |
| `is_unique` | `SBDB$BOOL` |  |
| `is_foreign_key` | `SBDB$BOOL` |  |
| `is_generated` | `SBDB$BOOL` |  |
| `storage_type` | `SBDB$U8` | Define enum domain if needed |
| `with_timezone` | `SBDB$U8` | Define enum domain if needed |
| `name_is_delimited` | `SBDB$BOOL` |  |
| `charset` | `SBDB$KEY_CHARSET` | Convert from uint16 |
| `timezone_hint` | `SBDB$KEY_TIMEZONE` | Convert from uint16 |
| `collation_id` | `SBDB$U32` |  |
| `default_value` | `SBDB$NAME` |  |
| `default_value_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `check_expr_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Indexes Table (`indexes_page`)
**Record**: `IndexInfo`

| Column | Domain | Notes |
| --- | --- | --- |
| `index_id` | `SBDB$KEY_INDEX` |  |
| `table_id` | `SBDB$KEY_TABLE` |  |
| `index_name` | `SBDB$NAME` |  |
| `name_is_delimited` | `SBDB$BOOL` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `root_page` | `SBDB$PAGE_ID` |  |
| `tablespace_id` | `SBDB$KEY_TABLESPACE` | Convert from uint16 |
| `index_type` | `SBDB$U32` |  |
| `is_unique` | `SBDB$U32` |  |
| `index_params_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `collation_id` | `SBDB$U32` |  |
| `rtree_max_entries` | `SBDB$U32` |  |
| `is_expression_index` | `SBDB$U32` |  |
| `is_partial_index` | `SBDB$U32` |  |
| `expression_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `predicate_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `predicate_string` | `SBDB$U32` |  |
| `dependency_id` | `SBDB$KEY_DEPENDENCY` |  |
| `logical_index_id` | `SBDB$KEY_INDEX` |  |
| `state` | `SBDB$U8` | Define enum domain if needed |
| `valid_from_xid` | `SBDB$KEY_TXN` | Convert from uint64 |
| `retired_xid` | `SBDB$KEY_TXN` | Convert from uint64 |
| `build_started_time` | `SBDB$TIME_US` |  |
| `build_completed_time` | `SBDB$TIME_US` |  |

### Index Versions Table (`index_versions_page`)
**Record**: `IndexVersionInfo`

| Column | Domain | Notes |
| --- | --- | --- |
| `logical_index_id` | `SBDB$KEY_INDEX` |  |
| `index_id` | `SBDB$KEY_INDEX` |  |
| `state` | `SBDB$U32` |  |
| `valid_from_xid` | `SBDB$KEY_TXN` | Convert from uint64 |
| `retired_xid` | `SBDB$KEY_TXN` | Convert from uint64 |
| `build_started_time` | `SBDB$TIME_US` |  |
| `build_completed_time` | `SBDB$TIME_US` |  |
| `created_time` | `SBDB$TIME_US` |  |

### Constraints Table (`constraints_page`)
**Record**: `ConstraintInfo`

| Column | Domain | Notes |
| --- | --- | --- |
| `constraint_id` | `SBDB$KEY_CONSTRAINT` |  |
| `constraint_name` | `SBDB$NAME` |  |
| `name_is_delimited` | `SBDB$BOOL` |  |
| `table_id` | `SBDB$KEY_TABLE` |  |
| `constraint_type` | `SBDB$U32` |  |
| `check_expression` | `SBDB$U32` |  |
| `check_expr_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `referenced_table_id` | `SBDB$KEY_TABLE` |  |
| `on_delete` | `SBDB$U32` |  |
| `on_update` | `SBDB$U32` |  |
| `match_type` | `SBDB$U32` |  |
| `exclusion_operator` | `SBDB$U32` |  |
| `index_method` | `SBDB$U32` |  |
| `is_deferrable` | `SBDB$U32` |  |
| `initially_deferred` | `SBDB$U32` |  |
| `is_enabled` | `SBDB$U32` |  |
| `is_validated` | `SBDB$BOOL` |  |
| `is_system_generated` | `SBDB$U32` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `validated_time` | `SBDB$TIME_US` |  |

### Sequences Table (`sequences_page`)
**Record**: `SequenceRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `sequence_id` | `SBDB$KEY_SEQUENCE` |  |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `sequence_name` | `SBDB$NAME` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `owned_by_table_id` | `SBDB$KEY_TABLE` |  |
| `owned_by_column_id` | `SBDB$KEY_COLUMN` |  |
| `current_value` | `SBDB$I64` |  |
| `increment_by` | `SBDB$I64` |  |
| `min_value` | `SBDB$I64` |  |
| `max_value` | `SBDB$I64` |  |
| `start_value` | `SBDB$I64` |  |
| `cache_size` | `SBDB$I64` |  |
| `cycle` | `SBDB$U8` | Define enum domain if needed |
| `name_is_delimited` | `SBDB$BOOL` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Views Table (`views_page`)
**Record**: `ViewRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `view_id` | `SBDB$KEY_VIEW` |  |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `view_name` | `SBDB$NAME` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `materialized_table_id` | `SBDB$KEY_TABLE` |  |
| `change_log_table_id` | `SBDB$KEY_TABLE` |  |
| `definition_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `columns_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `base_table_ids_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `name_is_delimited` | `SBDB$BOOL` |  |
| `check_option` | `SBDB$U8` | Define enum domain if needed |
| `is_materialized` | `SBDB$BOOL` |  |
| `refresh_strategy` | `SBDB$U8` | Define enum domain if needed |
| `refresh_on_commit` | `SBDB$U8` | Define enum domain if needed |
| `supports_concurrent` | `SBDB$U8` | Define enum domain if needed |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `last_refreshed` | `SBDB$U64` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Triggers Table (`triggers_page`)
**Record**: `TriggerRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `trigger_id` | `SBDB$KEY_TRIGGER` |  |
| `table_id` | `SBDB$KEY_TABLE` |  |
| `trigger_name` | `SBDB$NAME` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `scope` | `SBDB$U8` | Define enum domain if needed |
| `name_is_delimited` | `SBDB$BOOL` |  |
| `trigger_timing` | `SBDB$U8` | Define enum domain if needed |
| `trigger_event` | `SBDB$U8` | Define enum domain if needed |
| `granularity` | `SBDB$U8` | Define enum domain if needed |
| `enabled` | `SBDB$U8` | Define enum domain if needed |
| `position` | `SBDB$I32` |  |
| `condition_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `action_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `old_alias_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `new_alias_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Permissions Table (`permissions_page`)
**Record**: `PermissionRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `permission_id` | `SBDB$KEY_PERMISSION` |  |
| `object_id` | `SBDB$KEY_OBJECT` | Pair with SBDB$OBJTYPE |
| `object_type` | `SBDB$OBJTYPE` | Define enum domain if needed |
| `grantee_id` | `SBDB$KEY_PRINCIPAL` | User/role/group (SBDB$KEY_PRINCIPAL) |
| `grantee_type` | `SBDB$U8` | Define enum domain if needed |
| `privileges` | `SBDB$U32` |  |
| `grant_option` | `SBDB$U8` | Define enum domain if needed |
| `grantor_id` | `SBDB$KEY_USER` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Statistics Table (`statistics_page`)
**Record**: `StatisticRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `statistic_id` | `SBDB$KEY_STATISTIC` |  |
| `table_id` | `SBDB$KEY_TABLE` |  |
| `column_id` | `SBDB$KEY_COLUMN` |  |
| `data_type` | `SBDB$U16` |  |
| `num_rows` | `SBDB$U64` |  |
| `num_nulls` | `SBDB$U64` |  |
| `null_fraction` | `SBDB$F32` |  |
| `num_distinct` | `SBDB$U64` |  |
| `avg_width` | `SBDB$F32` |  |
| `mcv_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `histogram_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `histogram_type` | `SBDB$U8` | Define enum domain if needed |
| `histogram_bucket_count` | `SBDB$U32` |  |
| `last_analyzed_time` | `SBDB$TIME_US` |  |
| `sample_size` | `SBDB$U64` |  |
| `sample_rate` | `SBDB$F32` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Timezones Table (`timezones_page`)
**Record**: `TimezoneRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `timezone_id` | `SBDB$KEY_TIMEZONE` | Convert from uint16 |
| `name` | `SBDB$NAME` |  |
| `abbreviation` | `SBDB$NAME_64` |  |
| `std_offset_minutes` | `SBDB$I32` |  |
| `observes_dst` | `SBDB$BOOL` |  |
| `dst_start_month` | `SBDB$U8` | Define enum domain if needed |
| `dst_start_week` | `SBDB$U8` | Define enum domain if needed |
| `dst_start_day` | `SBDB$U8` | Define enum domain if needed |
| `dst_start_hour` | `SBDB$U8` | Define enum domain if needed |
| `dst_end_month` | `SBDB$U8` | Define enum domain if needed |
| `dst_end_week` | `SBDB$U8` | Define enum domain if needed |
| `dst_end_day` | `SBDB$U8` | Define enum domain if needed |
| `dst_end_hour` | `SBDB$U8` | Define enum domain if needed |
| `dst_offset_minutes` | `SBDB$I32` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Character Sets Table (`charsets_page`)
**Record**: `CharsetRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `charset_id` | `SBDB$KEY_CHARSET` | Convert from uint16 |
| `name` | `SBDB$NAME` |  |
| `description` | `SBDB$NAME` |  |
| `min_bytes` | `SBDB$U8` | Define enum domain if needed |
| `max_bytes` | `SBDB$U8` | Define enum domain if needed |
| `variable_width` | `SBDB$BOOL` |  |
| `default_collation_id` | `SBDB$U32` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Collations Table (`collation_defs_page`)
**Record**: `CollationRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `collation_id` | `SBDB$U32` |  |
| `name` | `SBDB$NAME` |  |
| `charset_id` | `SBDB$KEY_CHARSET` | Convert from uint16 |
| `collation_type` | `SBDB$U8` | Define enum domain if needed |
| `strength` | `SBDB$U8` | Define enum domain if needed |
| `pad_space` | `SBDB$BOOL` |  |
| `is_default` | `SBDB$BOOL` |  |
| `locale` | `SBDB$NAME_64` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Comments Table (`comments_page`)
**Record**: `CommentRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `comment_id` | `SBDB$KEY_COMMENT` |  |
| `object_id` | `SBDB$KEY_OBJECT` | Pair with SBDB$OBJTYPE |
| `object_type` | `SBDB$OBJTYPE` | Define enum domain if needed |
| `owner_id` | `SBDB$KEY_USER` |  |
| `comment_text_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Object Definitions Table (`object_definitions_page`)
**Record**: `ObjectDefinitionRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `object_id` | `SBDB$KEY_OBJECT` | Pair with SBDB$OBJTYPE |
| `object_type` | `SBDB$OBJTYPE` | Define enum domain if needed |
| `ddl_text_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `bytecode_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Users Table (`users_page`)
**Record**: `UserRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `user_id` | `SBDB$KEY_USER` |  |
| `username` | `SBDB$NAME` |  |
| `password_hash_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `user_metadata_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `default_schema_id` | `SBDB$KEY_SCHEMA` |  |
| `is_active` | `SBDB$BOOL` |  |
| `is_superuser` | `SBDB$BOOL` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_login_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Roles Table (`roles_page`)
**Record**: `RoleRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `role_id` | `SBDB$KEY_ROLE` |  |
| `role_name` | `SBDB$NAME` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `default_schema_id` | `SBDB$KEY_SCHEMA` |  |
| `role_metadata_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `is_active` | `SBDB$BOOL` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Groups Table (`groups_page`)
**Record**: `GroupRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `group_id` | `SBDB$KEY_GROUP` |  |
| `group_name` | `SBDB$NAME` |  |
| `external_id` | `SBDB$NAME_512` |  |
| `group_type` | `SBDB$U8` | Define enum domain if needed |
| `default_schema_id` | `SBDB$KEY_SCHEMA` |  |
| `group_metadata_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Role Memberships Table (`role_members_page`)
**Record**: `RoleMembershipRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `membership_id` | `SBDB$KEY_MEMBERSHIP` |  |
| `user_id` | `SBDB$KEY_USER` |  |
| `role_id` | `SBDB$KEY_ROLE` |  |
| `granted_by` | `SBDB$KEY_USER` |  |
| `with_admin_option` | `SBDB$U8` | Define enum domain if needed |
| `granted_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Group Memberships Table (`group_members_page`)
**Record**: `GroupMembershipRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `membership_id` | `SBDB$KEY_MEMBERSHIP` |  |
| `user_id` | `SBDB$KEY_USER` |  |
| `member_type` | `SBDB$U8` | Define enum domain if needed |
| `group_id` | `SBDB$KEY_GROUP` |  |
| `granted_by` | `SBDB$KEY_USER` |  |
| `granted_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Group Mappings Table (`group_mappings_page`)
**Record**: `GroupMappingRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `mapping_id` | `SBDB$KEY_MAPPING` |  |
| `external_group_name` | `SBDB$NAME` |  |
| `auth_method` | `SBDB$U8` | Define enum domain if needed |
| `auto_create_users` | `SBDB$U8` | Define enum domain if needed |
| `internal_group_id` | `SBDB$UUID_V7` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Auth Keys Table (`authkeys_page`)
**Record**: `AuthKeyRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `authkey_id` | `SBDB$KEY_AUTHKEY` |  |
| `issuer` | `SBDB$NAME` |  |
| `valid_from` | `SBDB$TIME_US` |  |
| `valid_to` | `SBDB$TIME_US` |  |
| `usage_limit` | `SBDB$U32` |  |
| `usage_count` | `SBDB$U32` |  |
| `status` | `SBDB$U8` | Define enum domain if needed |
| `usage_type` | `SBDB$U8` | Define enum domain if needed |
| `role_scope_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `group_scope_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Sessions Table (`sessions_page`)
**Record**: `SessionRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `session_id` | `SBDB$KEY_SESSION` |  |
| `user_id` | `SBDB$KEY_USER` |  |
| `authkey_id` | `SBDB$KEY_AUTHKEY` |  |
| `emulation_mode` | `SBDB$NAME_64` |  |
| `login_time` | `SBDB$TIME_US` |  |
| `last_activity_time` | `SBDB$TIME_US` |  |
| `current_schema_id` | `SBDB$KEY_SCHEMA` |  |
| `policy_epoch_global` | `SBDB$U64` |  |
| `policy_epoch_table` | `SBDB$U64` |  |
| `is_expired` | `SBDB$BOOL` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Audit Log Table (`audit_log_page`)
**Record**: `AuditLogRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `event_id` | `SBDB$U64` |  |
| `timestamp` | `SBDB$TIME_US` |  |
| `event_type` | `SBDB$U16` |  |
| `success` | `SBDB$BOOL` |  |
| `session_id` | `SBDB$KEY_SESSION` |  |
| `authkey_id` | `SBDB$KEY_AUTHKEY` |  |
| `user_id` | `SBDB$KEY_USER` |  |
| `role_id` | `SBDB$KEY_ROLE` |  |
| `target_user_id` | `SBDB$KEY_USER` |  |
| `object_id` | `SBDB$KEY_OBJECT` | Pair with SBDB$OBJTYPE |
| `username` | `SBDB$NAME` |  |
| `target_username` | `SBDB$NAME` |  |
| `object_type` | `SBDB$OBJTYPE` |  |
| `object_name` | `SBDB$NAME` |  |
| `details_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `hash_prev` | `SBDB$HASH256` |  |
| `hash_curr` | `SBDB$HASH256` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Security Policy Epoch Table (`security_policy_epoch_page`)
**Record**: `SecurityPolicyEpochRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `global_epoch` | `SBDB$U64` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Procedures Table (`procedures_page`)
**Record**: `ProcedureRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `procedure_id` | `SBDB$KEY_PROCEDURE` |  |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `procedure_name` | `SBDB$NAME` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `procedure_type` | `SBDB$U8` | Define enum domain if needed |
| `is_selectable` | `SBDB$BOOL` |  |
| `language` | `SBDB$U8` | Define enum domain if needed |
| `sql_security` | `SBDB$U8` | Define enum domain if needed |
| `name_is_delimited` | `SBDB$BOOL` |  |
| `body_redacted` | `SBDB$BOOL` |  |
| `deterministic` | `SBDB$BOOL` |  |
| `parameter_count` | `SBDB$U32` |  |
| `return_type_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `body_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `bytecode_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Procedure Parameters Table (`proc_params_page`)
**Record**: `ProcedureParameterRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `parameter_id` | `SBDB$KEY_PROC_PARAM` |  |
| `procedure_id` | `SBDB$KEY_PROCEDURE` |  |
| `parameter_name` | `SBDB$NAME` |  |
| `parameter_position` | `SBDB$U16` |  |
| `parameter_mode` | `SBDB$U8` | Define enum domain if needed |
| `data_type_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `default_value_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `is_valid` | `SBDB$BOOL` |  |

### Domains Table (`domains_page`)
**Record**: `DomainRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `domain_id` | `SBDB$KEY_DOMAIN` |  |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `domain_name` | `SBDB$NAME` |  |
| `domain_type` | `SBDB$U8` | Define enum domain if needed |
| `base_type` | `SBDB$U16` |  |
| `precision` | `SBDB$U32` |  |
| `scale` | `SBDB$U32` |  |
| `nullable` | `SBDB$U8` | Define enum domain if needed |
| `default_value` | `SBDB$NAME` |  |
| `parent_domain_id` | `SBDB$UUID_V7` |  |
| `is_valid` | `SBDB$BOOL` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `constraints_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `fields_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `enum_values_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `security_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `integrity_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `validation_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `quality_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `set_element_type` | `SBDB$U16` |  |
| `dialect_tag` | `SBDB$NAME_64` |  |
| `compat_name` | `SBDB$NAME` |  |

### UDR Table (`udr_page`)
**Record**: `UDRRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `udr_id` | `SBDB$KEY_UDR` |  |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `udr_name` | `SBDB$NAME` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `library_path` | `SBDB$NAME_1024` |  |
| `entry_point` | `SBDB$NAME_1024` |  |
| `udr_type` | `SBDB$U8` | Define enum domain if needed |
| `name_is_delimited` | `SBDB$BOOL` |  |
| `signature_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Packages Table (`packages_page`)
**Record**: `PackageRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `package_id` | `SBDB$KEY_PACKAGE` |  |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `package_name` | `SBDB$NAME` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `package_header_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `package_body_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |
| `name_is_delimited` | `SBDB$BOOL` |  |

### Exceptions Table (`exceptions_page`)
**Record**: `ExceptionRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `exception_id` | `SBDB$KEY_EXCEPTION` |  |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `name` | `SBDB$NAME` |  |
| `message_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `owner_id` | `SBDB$KEY_USER` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |
| `name_is_delimited` | `SBDB$BOOL` |  |

### Emulation Types Table (`emulation_types_page`)
**Record**: `EmulationTypeRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `emulation_type_id` | `SBDB$KEY_EMULATION_TYPE` |  |
| `emulation_name` | `SBDB$NAME` |  |
| `version_major` | `SBDB$U8` | Define enum domain if needed |
| `version_minor` | `SBDB$U8` | Define enum domain if needed |
| `mapping_rules_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Emulation Servers Table (`emulation_servers_page`)
**Record**: `EmulationServerRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `server_id` | `SBDB$KEY_SERVER` |  |
| `server_name` | `SBDB$NAME` |  |
| `emulation_type_id` | `SBDB$KEY_EMULATION_TYPE` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `server_config_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `is_active` | `SBDB$BOOL` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Emulated Databases Table (`emulated_dbs_page`)
**Record**: `EmulatedDatabaseRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `emulated_db_id` | `SBDB$KEY_EMULATED_DB` |  |
| `database_name` | `SBDB$NAME` |  |
| `server_id` | `SBDB$KEY_SERVER` |  |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `db_metadata_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `is_active` | `SBDB$BOOL` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Synonyms Table (`synonyms_page`)
**Record**: `SynonymRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `synonym_id` | `SBDB$KEY_SYNONYM` |  |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `synonym_name` | `SBDB$NAME` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `target_path_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `target_type` | `SBDB$U8` | Define enum domain if needed |
| `is_public` | `SBDB$BOOL` |  |
| `name_is_delimited` | `SBDB$BOOL` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Foreign Servers Table (`foreign_servers_page`)
**Record**: `ForeignServerRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `server_id` | `SBDB$KEY_SERVER` |  |
| `server_name` | `SBDB$NAME` |  |
| `server_type` | `SBDB$NAME` |  |
| `host` | `SBDB$NAME_512` |  |
| `port` | `SBDB$U16` |  |
| `connection_options_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `owner_id` | `SBDB$KEY_USER` |  |
| `is_active` | `SBDB$BOOL` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Foreign Tables Table (`foreign_tables_page`)
**Record**: `ForeignTableRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `foreign_table_id` | `SBDB$KEY_FOREIGN_TABLE` |  |
| `schema_id` | `SBDB$KEY_SCHEMA` |  |
| `table_name` | `SBDB$NAME` |  |
| `foreign_server_id` | `SBDB$KEY_FOREIGN_SERVER` |  |
| `remote_schema` | `SBDB$NAME` |  |
| `remote_table` | `SBDB$NAME` |  |
| `owner_id` | `SBDB$KEY_USER` |  |
| `column_mapping_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |
| `name_is_delimited` | `SBDB$BOOL` |  |

### User Mappings Table (`user_mappings_page`)
**Record**: `UserMappingRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `mapping_id` | `SBDB$KEY_MAPPING` |  |
| `user_id` | `SBDB$KEY_USER` |  |
| `foreign_server_id` | `SBDB$KEY_FOREIGN_SERVER` |  |
| `remote_user` | `SBDB$NAME_256` |  |
| `remote_credentials_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Server Registry Table (`server_registry_page`)
**Record**: `ServerRegistryRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `server_id` | `SBDB$KEY_SERVER` |  |
| `server_name` | `SBDB$NAME` |  |
| `host` | `SBDB$NAME_512` |  |
| `port` | `SBDB$U16` |  |
| `role` | `SBDB$U8` | Define enum domain if needed |
| `state` | `SBDB$U8` | Define enum domain if needed |
| `last_heartbeat` | `SBDB$TIME_US` |  |
| `last_xid` | `SBDB$KEY_TXN` | Convert from uint64 |
| `replication_lag_ms` | `SBDB$U64` |  |
| `cluster_id` | `SBDB$NAME_256` |  |
| `server_version` | `SBDB$NAME_256` |  |
| `metadata_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### UDR Engines Table (`udr_engines_page`)
**Record**: `UDREngineRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `engine_id` | `SBDB$KEY_UDR_ENGINE` |  |
| `engine_name` | `SBDB$NAME` |  |
| `engine_type` | `SBDB$U8` | Define enum domain if needed |
| `is_active` | `SBDB$BOOL` |  |
| `is_default` | `SBDB$BOOL` |  |
| `plugin_path` | `SBDB$NAME_1024` |  |
| `config_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### UDR Modules Table (`udr_modules_page`)
**Record**: `UDRModuleRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `module_id` | `SBDB$KEY_UDR_MODULE` |  |
| `module_name` | `SBDB$NAME` |  |
| `engine_id` | `SBDB$KEY_UDR_ENGINE` |  |
| `library_path` | `SBDB$NAME_1024` |  |
| `checksum` | `SBDB$NAME_256` |  |
| `entry_point` | `SBDB$NAME_1024` |  |
| `dependencies_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `is_loaded` | `SBDB$BOOL` |  |
| `is_validated` | `SBDB$BOOL` |  |
| `loaded_count` | `SBDB$U64` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `last_modified_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Foreign Keys Table (`foreign_keys_page`)
**Record**: `ForeignKeyRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `fk_id` | `SBDB$KEY_FOREIGN_KEY` |  |
| `fk_name` | `SBDB$NAME` |  |
| `child_table_id` | `SBDB$UUID_V7` |  |
| `parent_table_id` | `SBDB$UUID_V7` |  |
| `child_columns` | `SBDB$NAME` |  |
| `parent_columns` | `SBDB$NAME` |  |
| `on_delete` | `SBDB$U8` | Define enum domain if needed |
| `on_update` | `SBDB$U8` | Define enum domain if needed |
| `match_type` | `SBDB$U8` | Define enum domain if needed |
| `is_enabled` | `SBDB$BOOL` |  |
| `created_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Migration History Table (`migration_history_page`)
**Record**: `MigrationHistoryInfo`

| Column | Domain | Notes |
| --- | --- | --- |
| `history_id` | `SBDB$KEY_MIGRATION_HISTORY` |  |
| `migration_id` | `SBDB$KEY_MIGRATION` |  |
| `table_id` | `SBDB$KEY_TABLE` |  |
| `source_tablespace` | `SBDB$KEY_TABLESPACE` | Convert from uint16 |
| `target_tablespace` | `SBDB$KEY_TABLESPACE` | Convert from uint16 |
| `final_phase` | `SBDB$U32` |  |
| `migration_xid` | `SBDB$KEY_TXN` | Convert from uint64 |
| `total_pages` | `SBDB$U32` |  |
| `pages_copied` | `SBDB$U32` |  |
| `start_time` | `SBDB$TIME_US` |  |
| `end_time` | `SBDB$TIME_US` |  |
| `catch_up_iterations` | `SBDB$U32` |  |
| `total_bytes_copied` | `SBDB$U64` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Dormant Transactions Table (`dormant_transactions_page`)
**Record**: `DormantTransactionRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `dormant_id` | `SBDB$KEY_DORMANT_TXN` |  |
| `attachment_id` | `SBDB$KEY_ATTACHMENT` |  |
| `proc_id` | `SBDB$U32` |  |
| `txn_id` | `SBDB$KEY_TXN` | Convert from uint64 |
| `session_id` | `SBDB$KEY_SESSION` |  |
| `user_id` | `SBDB$KEY_USER` |  |
| `session_user_id` | `SBDB$KEY_USER` |  |
| `role_id` | `SBDB$KEY_ROLE` |  |
| `isolation_level` | `SBDB$U8` | Define enum domain if needed |
| `access_mode` | `SBDB$U8` | Define enum domain if needed |
| `wait_mode` | `SBDB$U8` | Define enum domain if needed |
| `autocommit_mode` | `SBDB$U8` | Define enum domain if needed |
| `lock_timeout_seconds` | `SBDB$U32` |  |
| `current_schema_id` | `SBDB$KEY_SCHEMA` |  |
| `session_settings_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `last_statement_oid` | `SBDB$LOB_REF` | Replace legacy OID with UUID v7 |
| `last_statement_hash` | `SBDB$U64` |  |
| `last_statement_type` | `SBDB$U8` | Define enum domain if needed |
| `last_statement_status` | `SBDB$U8` | Define enum domain if needed |
| `state` | `SBDB$U8` | Define enum domain if needed |
| `start_time` | `SBDB$TIME_US` |  |
| `last_activity_time` | `SBDB$TIME_US` |  |
| `dormant_since` | `SBDB$TIME_US` |  |
| `lease_expires_at` | `SBDB$TIME_US` |  |
| `last_statement_time` | `SBDB$TIME_US` |  |
| `last_rows_affected` | `SBDB$I64` |  |
| `last_error_code` | `SBDB$U32` |  |
| `last_sqlstate` | `SBDB$SQLSTATE` |  |
| `server_instance_id` | `SBDB$KEY_SERVER_INSTANCE` |  |
| `is_valid` | `SBDB$BOOL` |  |

### Prepared Transactions Table (`prepared_transactions_page`)
**Record**: `PreparedTransactionRecord`

| Column | Domain | Notes |
| --- | --- | --- |
| `prepared_id` | `SBDB$KEY_PREPARED_TXN` |  |
| `txn_id` | `SBDB$KEY_TXN` | Convert from uint64 |
| `owner_id` | `SBDB$KEY_USER` |  |
| `database_id` | `SBDB$KEY_DATABASE` |  |
| `gid` | `SBDB$NAME_256` |  |
| `prepared_time` | `SBDB$TIME_US` |  |
| `is_valid` | `SBDB$BOOL` |  |

## Decisions Required (Before Finalizing)
1. Finalize the system vs user domain naming convention (Firebird-style prefix or alternative).

## Recommended Next Step
Once these decisions are confirmed, I can generate a complete catalog DDL draft where every column uses the system domains above, and the recursive schema paths are aligned to a single canonical hierarchy.

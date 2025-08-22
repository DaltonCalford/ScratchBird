# Phase 4 — Catalog Persistence and Bootstrap ✅ **COMPLETED**

## Status: **FULLY IMPLEMENTED**

#### Scope and goals
- Materialize the persistent system catalog (SDB$*) and system domains in the database.
- Execute bootstrap SQL inside the engine to seed fixed UUIDs/IDs, default schemas, domains, compatibility views (RDB$*/MON$*), and meta.
- Provide transactional DDL surfaces to create/alter/drop objects by writing to SDB$* under Phase 3 transaction semantics.
- Exit: CREATE/ALTER/DROP objects persist; catalog versioning recorded; compatibility views present and queryable; engine boots on fresh and pre-bootstrapped DBs.

#### Prior state assumed
- Engine core: file manager, pager, buffer cache; checksums; WAL (logical) + recovery.
- Storage: heap (root/data/overflow), allocator (PIP), TIP and transactions with RC/RR snapshots.
- Index: B-Tree V1 with DDL/executor integration.
- Parser: DDL/DML features largely in place; executor stubs available.

#### Architecture overview
- Catalog uses normal heap relations with fixed UUIDs; persisted in default page space.
- `CatalogManager` module:
  - Bootstrap detection and creation of SDB$ objects
  - Typed CRUD over SDB$ rows for DDL executor
  - Read caches (name→oid, oid→descriptor) invalidated on DDL commit
- `BootstrapExecutor` runs idempotent scripts for initial objects and compatibility views.

#### SDB$ schema (core)
- SDB$SCHEMA(oid, name, owner_oid, parent_oid, created_at, flags)
- SDB$OBJECT(oid, schema_oid, name, type, owner_oid, created_at, flags, comment)
- SDB$RELATION(oid, rel_id, heap_root_page, persistence, relkind, options)
- SDB$COLUMN(oid, relation_oid, name, ordinal, type_oid, attnotnull, default_expr, collation_oid, storage)
- SDB$INDEX(oid, relation_oid, name, method, unique, partial_predicate, include_cols, options, root_page)
- SDB$INDEX_KEY(index_oid, ordinal, column_oid, opclass_oid)
- SDB$SEQUENCE(oid, name, start, increment, minvalue, maxvalue, cache, cycle, last_value)
- SDB$CONSTRAINT(oid, relation_oid, name, kind, details)
- SDB$DOMAIN, SDB$COLLATION, SDB$ROUTINE, SDB$ROUTINE_ARG, SDB$ROLE, SDB$USER (minimal Phase 4 fields)
- SDB$PROPERTY(key, value)
- SDB$CATALOG_VERSION(version, bootstrap_completed, upgraded_from)

Notes: UUIDs stored as 16-byte binary; fixed UUIDs for core objects; compatibility views map to Firebird-like shapes.

#### System domains
- Define standard domains (SDB$OID, SDB$NAME, SDB$IDENT, timestamps, booleans) and persist under SDB$DOMAIN.

#### ODS/header integration
- Header clumplets: catalog_root_schema_oid (UUID), catalog_version (int32), optional root pointers.
- Well-known generators: object_id, relation_id, index_id; tie to SDB$SEQUENCE.

#### Bootstrap sequence (idempotent)
1. Detect bootstrap: absence of SDB$CATALOG_VERSION → uninitialized; else read version.
2. Create SDB$* relations + B-Tree indexes on critical keys.
3. Seed fixed UUIDs for root schemas/objects within one transaction.
4. Insert system domains and SDB$CATALOG_VERSION v1.
5. Execute bootstrap SQL to create compatibility views and `System` sub-schemas; seed `Users/Roles/Remote` scaffolding.
6. Commit and verify via CatalogManager reads.

#### CatalogManager APIs
- is_bootstrapped(), current_version(), bootstrap_if_needed()
- DDL helpers:
  - create_schema(name, owner, parent) → schema_oid
  - create_relation(schema, name, columns[], options) → relation_oid, heap_root_page
  - alter_relation_add_column(relation_oid, column_spec)
  - create_index(relation_oid, index_spec) → index_oid
  - drop_object(oid)
  - lookup_{schema,object,relation,column,index}(…)
- Implementation: marshal SDB$ rows via HeapRelation; use B-Tree for lookups; enforce name-uniqueness and not-null basics.

#### Minimal DDL executor (Phase 4)
- Wire parser DDL to CatalogManager: CREATE SCHEMA/TABLE/INDEX/SEQUENCE; ALTER TABLE ADD COLUMN; DROP TABLE/INDEX/SEQUENCE.
- CREATE TABLE: allocate heap, insert SDB$ rows, build indexes (offline ok).
- DROP TABLE: free storage via allocator; remove dependent indexes; invalidate caches.
- Execute within normal transactions; no global DDL locks yet.

#### Compatibility views (RDB$*/MON$*)
- Bootstrap SQL to create views: RDB$RELATIONS, RDB$FIELDS, RDB$INDICES, etc.
- MON$ placeholders for basic counters; real monitoring later.

#### Fixed UUIDs and well-known IDs
- Code constants for root schemas and SDB$ relations; stable across runs; enable idempotent bootstrap.

#### WAL/recovery/upgrades
- WAL: reuse logical heap records for SDB$ changes; reserve catalog op types for future.
- Recovery: idempotent steps and/or staged commits; `SDB$CATALOG_VERSION` flags progress.
- Upgrade framework: `upgrade_if_needed()` with versioned scripts; Phase 4 ships v1 only.

#### Tooling
- CLI `catalog_inspect`: dump counts, sample rows, catalog version, root schema OIDs.
- isql meta: `SHOW CATALOG`; shortcuts to list relations/columns/indexes via views.

#### Concurrency
- DDL is transactional. Lock `{schema,name,type}` namespace to avoid dupes (use LockManager). Conflicts fail fast.

#### Testing
- Unit: marshal/unmarshal SDB$ rows.
- Bootstrap: fresh DB creates catalog; reopen is a no-op; crash mid-bootstrap resumes idempotently.
- DDL: create/alter/drop visible in compatibility views; allocator frees on drop.
- Concurrency: concurrent create same name—one winner.
- Perf sanity: create 1K tables/10K cols within reasonable bounds.

#### Milestones
- M1: SDB$ schema + encoders + read APIs + tests.
- M2: Bootstrap creation + fixed UUID seeding + views.
- M3: CREATE SCHEMA/TABLE/INDEX/SEQUENCE end-to-end.
- M4: ALTER ADD COLUMN; DROP object paths; allocator integration.
- M5: Tooling + docs.

#### Exit criteria
- Engine boots fresh/existing; SDB$ present; catalog_version=v1.
- RDB$*/MON$* views selectable; DDL persists and visible per isolation semantics.
- CI green for bootstrap, DDL, concurrency, idempotency.

#### Risks/mitigations
- Idempotency bugs → fixed UUIDs, progress markers, step validation.
- Cache incoherence → transaction-scoped caches and post-commit invalidation.
- Crash during bootstrap → staged/atomic steps; safe re-entry.

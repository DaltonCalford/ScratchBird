# Specification: Catalog Bootstrap Sequence

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:11920`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:4502`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:350`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_database_bootstrap.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_type_schema_contract.cpp:1`

## Synopsis

This specification defines the deterministic sequence for initializing the ScratchBird catalog system during database creation. It covers the phased creation of the schema tree, catalog table initialization, and bootstrap invariants that ensure consistent database topology across all deployments.

## Scope

### In Scope

- Database creation and catalog initialization phases
- Schema tree bootstrap order and dependencies
- Catalog table creation sequence
- Bootstrap invariants and validation rules
- Error handling during bootstrap

### Out of Scope

- Individual catalog table schemas (see `catalog_table_layouts.md`)
- UUID generation rules (see `uuid_mapping.md`)
- Object identity resolution (see `object_identity_rules.md`)
- Virtual catalog overlays (see virtual catalog specifications)

## Background

The ScratchBird catalog bootstrap creates the foundational metadata structures required for database operation. Unlike PostgreSQL's template database approach or Firebird's hard-coded system table OIDs, ScratchBird uses a deterministic bootstrap sequence that generates stable UUID-based identifiers while maintaining topological consistency.

The bootstrap process must create:
1. The root schema hierarchy
2. System catalog tables (on-disk storage for metadata)
3. Bootstrap records for database identity
4. Schema tree structure for namespacing

## Specification

### Bootstrap Phases

The bootstrap algorithm runs in 7 distinct phases. A failure in any phase aborts database creation.

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Phase 0   │───▶│   Phase 1   │───▶│   Phase 2   │───▶│   Phase 3   │
│ Preconditions│    │ Root Creation│    │ Fixed First │    │ Fixed Second│
│             │    │             │    │ Level Branches│   │ Level Branches│
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
                                                              │
       ┌──────────────────────────────────────────────────────┘
       ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Phase 4   │───▶│   Phase 5   │───▶│   Phase 6   │
│   Security  │    │   Emulation │    │   Metadata  │
│   Subtree   │    │ Dialect Roots│   │   Commit    │
└─────────────┘    └─────────────┘    └─────────────┘
```

### Phase 0 - Preconditions

**Source:** `src/core/catalog_manager.cpp:11920`

```cpp
// Verify catalog pages are writable
Status status = pm->allocatePage(root_page_id, ctx);
if (status != Status::OK) {
    return status;
}

// Verify database_uuid is valid and non-zero
if (isZeroUuid(database_uuid)) {
    return Status::INVALID_ARGUMENT;
}

// Verify root schema does not already exist
if (schemaExists(ID{})) {
    return Status::ALREADY_EXISTS;
}
```

**Requirements:**
- Catalog pages must be writable
- `database_uuid` must be valid UUIDv7 (non-zero)
- Root schema must not already exist (idempotency check)

### Phase 1 - Root Creation

**Source:** `src/core/catalog_manager.cpp:11977`

Create the root schema with special invariants:

| Field | Value | Invariant |
|-------|-------|-----------|
| `schema_id` | `database_uuid` | `SCHEMA_INV_001` |
| `parent_schema_id` | Zero UUID (NULL) | `SCHEMA_INV_002` |
| `schema_name` | `'root'` | Fixed name |
| `schema_type` | `SYSTEM_FIXED` | Non-modifiable |
| `owner` | `SYSTEM` | Internal principal |

```cpp
SchemaInfo root_schema;
root_schema.schema_id = database_uuid;  // Root UUID = database UUID
root_schema.parent_schema_id = ID{};    // Zero UUID = no parent
root_schema.schema_name = "root";
root_schema.schema_type = SchemaType::SYSTEM;
root_schema.owner_id = SYSTEM_OWNER_ID;
```

**Postconditions:**
- Root schema persisted and fsync'd
- Bootstrap transaction committed before proceeding

### Phase 2 - Fixed First-Level Branches

**Source:** `src/core/catalog_manager.cpp:12008`

Create children under `root` in exact order:

1. `sys` - System management schemas
2. `users` - User home directories
3. `remote` - Remote server mounts
4. `local` - Local instance storage
5. `nosql` - NoSQL emulation schemas

```cpp
const std::vector<BootstrapSchemaNode> kBootstrapSchemas = {
    // Phase 2: First-level branches (order matters)
    {"sys", "root", SchemaType::SYSTEM},
    {"users", "root", SchemaType::SYSTEM},
    {"remote", "root", SchemaType::SYSTEM},
    {"local", "root", SchemaType::SYSTEM},
    {"nosql", "root", SchemaType::SYSTEM},
    // ... additional nodes
};
```

**Invariant:** `SCHEMA_INV_003` - All 5 fixed first-level branches must exist exactly once.

### Phase 3 - Fixed Second-Level Branches

Create schema hierarchy under first-level branches:

**Under `root.sys` (in order):**
| Schema | Purpose |
|--------|---------|
| `information` | Information schema views |
| `security` | Security subsystem metadata |
| `system` | Core system tables |
| `monitor` | Monitoring and metrics |
| `config` | Configuration storage |
| `jobs` | Job scheduler metadata |

**Under `root.users`:**
| Schema | Purpose |
|--------|---------|
| `public` | Default public schema |
| `roles` | Role definitions |
| `groups` | Group definitions |

**Under `root.remote`:**
| Schema | Purpose |
|--------|---------|
| `emulation` | Emulated engine schemas |
| `fdw` | Foreign data wrappers |
| `links` | Server links |

**Under `root.local`:**
| Schema | Purpose |
|--------|---------|
| `instances` | Local instances |
| `links` | Local links |

**Under `root.nosql`:**
| Schema | Purpose |
|--------|---------|
| `cassandra` | Cassandra emulation |
| `mongodb` | MongoDB emulation |
| `neo4j` | Neo4j emulation |
| `redis` | Redis emulation |
| `milvus` | Milvus emulation |

### Phase 4 - Security Subtree

**Under `root.sys.security` (in order):**
1. `users` - User account definitions
2. `roles` - Role definitions
3. `groups` - Group mappings
4. `auth` - Authentication policies

### Phase 5 - Emulation Dialect Roots

**Source:** `src/core/catalog_manager.cpp:11977-12008`

Under `root.remote.emulation`, create one child per supported dialect:

1. `firebird`
2. `postgresql`
3. `mysql`
4. `cassandra`
5. `mongodb`
6. `neo4j`
7. `redis`
8. `milvus`

**Important:** All dialect roots are created even if disabled. Profile gating controls parser visibility, not tree existence. This ensures deterministic checksum stability.

**Invariant:** `SCHEMA_INV_006` - Emulation roots only under `root.remote.emulation`.

### Phase 6 - Bootstrap Metadata Commit

**Source:** `src/core/catalog_manager.cpp:4502`

Write bootstrap completion record to `CatalogRootPage`:

```cpp
struct CatalogRootPage {
    PageHeader header;
    uint32_t schema_count;
    uint32_t table_count;
    
    // Core catalog table page pointers
    uint32_t schemas_page;
    uint32_t tables_page;
    uint32_t columns_page;
    uint32_t indexes_page;
    // ... additional page pointers
};
```

**Bootstrap completion record:**
- Bootstrap version
- Schema hash/checksum
- Timestamp
- Creator principal

### Bootstrap Catalog Table Creation Order

After schema tree creation, catalog tables are created in dependency order:

| Order | Table | Purpose | Dependencies |
|-------|-------|---------|--------------|
| 1 | `database` | Database identity | None |
| 2 | `schema` | Schema registry | database |
| 3 | `object` | Object registry | schema |
| 4 | `object_name` | Name registry | object |
| 5 | `type` | Type system | object |
| 6 | `domain` | Domain registry | type |
| 7 | `table` | Table metadata | schema |
| 8 | `column` | Column metadata | table |
| 9 | `index` | Index registry | table |
| 10 | `constraint` | Constraints | table, column |

## Invariants

| ID | Invariant | Enforcement Phase | Failure Action |
|----|-----------|-------------------|----------------|
| `SCHEMA_INV_001` | `root.schema_id == database_uuid` | Phase 1 | Abort bootstrap |
| `SCHEMA_INV_002` | `root.parent_schema_id IS NULL` | Phase 1 | Abort bootstrap |
| `SCHEMA_INV_003` | All 5 fixed first-level branches exist exactly once | Phase 2 | Abort bootstrap |
| `SCHEMA_INV_004` | Fixed branch names are non-delimited lowercase identifiers | Phases 2-5 | Abort bootstrap |
| `SCHEMA_INV_005` | No user-owned schema under `root.sys` | Post-bootstrap | Fail startup |
| `SCHEMA_INV_006` | Emulation roots only under `root.remote.emulation` | Phase 5 | Abort bootstrap |
| `SCHEMA_INV_007` | Branch creation order checksum equals canonical checksum | Phase 6 | Abort bootstrap |

## Bootstrap Checksum

For deterministic verification, compute:

```
1. Build ordered list of full schema paths in creation order
2. Concatenate as newline-separated UTF-8 text
3. Compute SHA-256
4. Store as bootstrap_schema_checksum
```

**Canonical path list (37 entries):**
```
root
root.sys
root.users
root.remote
root.local
root.nosql
root.sys.information
root.sys.security
root.sys.system
root.sys.monitor
root.sys.config
root.sys.jobs
root.users.public
root.users.roles
root.users.groups
root.remote.emulation
root.remote.fdw
root.remote.links
root.local.instances
root.local.links
root.nosql.cassandra
root.nosql.mongodb
root.nosql.neo4j
root.nosql.redis
root.nosql.milvus
root.sys.security.users
root.sys.security.roles
root.sys.security.groups
root.sys.security.auth
root.remote.emulation.firebird
root.remote.emulation.postgresql
root.remote.emulation.mysql
root.remote.emulation.cassandra
root.remote.emulation.mongodb
root.remote.emulation.neo4j
root.remote.emulation.redis
root.remote.emulation.milvus
```

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `CATALOG_BOOTSTRAP_FAILED` | Any required table/index cannot be created | Database creation fails; cleanup partial state |
| `SCHEMA_INV_001` | Root UUID mismatch | Abort bootstrap |
| `SCHEMA_INV_007` | Checksum mismatch | Log warning; continue in repair mode if enabled |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_catalog_database_bootstrap.cpp` | Bootstrap sequence and invariants |
| `tests/unit/test_catalog_type_schema_contract.cpp` | Type system bootstrap |
| `tests/unit/test_catalog_family_matrix_contract.cpp` | Schema tree structure |

## Migration Notes

- Bootstrap is one-time operation per database
- Repair mode (`catalog_repair_mode = true`) can recreate missing fixed nodes
- No automatic repair for invalid root UUID (requires operator intervention)

## Related Specifications

- `uuid_mapping.md` - UUID generation and identity rules
- `object_identity_rules.md` - Object identification and resolution
- `catalog_table_layouts.md` - Individual catalog table schemas

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Bootstrap | Initial database creation and catalog initialization |
| Fixed Schema | System-created schema that cannot be deleted |
| Schema Tree | Hierarchical namespace structure |
| SYSTEM_FIXED | Schema type for non-modifiable system schemas |

### References

- `docs/specifications/07_Catalog_Bootstrap_and_UUID_Mapping/CATALOG_BOOTSTRAP_LAYOUT.md`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SCHEMA_BOOTSTRAP_ORDER_AND_INVARIANTS.md`

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |

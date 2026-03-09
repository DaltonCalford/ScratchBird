# Specification: Catalog System Overview

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:350`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/catalog/sys_catalog.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_database_bootstrap.cpp:1`

## Synopsis

This specification provides a comprehensive overview of the ScratchBird Catalog System, which manages all database metadata including schemas, tables, columns, indexes, constraints, and database objects. The catalog implements a hybrid storage model combining on-disk system tables, virtual views, and in-memory caching.

## Scope

### In Scope

- Catalog system architecture and components
- System catalog vs user catalog distinction
- Catalog storage model (on-disk, virtual, in-memory)
- Catalog manager responsibilities
- Catalog table categories and organization
- UUID-based identity system
- Metadata caching architecture

### Out of Scope

- Individual catalog table schemas (see specific object type specs)
- Bootstrap sequence details (see `bootstrap_sequence.md`)
- DDL implementation internals (see `ddl_operations.md`)
- Specific index type implementations (see index specs)

## Background

The ScratchBird catalog system is responsible for storing and managing all database metadata. Unlike PostgreSQL (which uses template databases) or Firebird (which uses hard-coded system tables), ScratchBird uses a deterministic bootstrap sequence with UUID-based identifiers.

### Key Design Principles

1. **UUIDv7 Primary Keys**: All objects use time-ordered UUIDv7 for global uniqueness
2. **Hybrid Storage**: On-disk tables, virtual views, and in-memory caches
3. **Firebird-Style Identifiers**: Case-insensitive (uppercase) unless quoted
4. **Soft Delete**: MGA (Multi-Generational Architecture) with is_valid flags
5. **TOAST Support**: Large metadata stored in TOAST tables

## Specification

### Catalog Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                    ScratchBird Catalog System                        │
├─────────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │
│  │  In-Memory   │  │   Virtual    │  │   On-Disk    │              │
│  │    Cache     │  │    Views     │  │    Tables    │              │
│  │  (relcache)  │  │   (sys.*)    │  │  (sb_*)      │              │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘              │
│         │                 │                 │                      │
│         └─────────────────┼─────────────────┘                      │
│                           ▼                                        │
│              ┌────────────────────────┐                           │
│              │    Catalog Manager     │                           │
│              │  (CatalogManager class) │                           │
│              └────────────────────────┘                           │
└─────────────────────────────────────────────────────────────────────┘
```

### Catalog Manager Components

**Source:** `include/scratchbird/core/catalog_manager.h:350`

```cpp
class CatalogManager {
    // Core identity management
    - Schema management (SchemaInfo)
    - Table management (TableInfo)
    - Column management (ColumnInfo)
    
    // Index and constraint management
    - Index management (IndexInfo, 28+ index types)
    - Constraint management (ConstraintInfo)
    
    // Object management
    - Sequence management (SequenceInfo)
    - View management (ViewInfo)
    - Trigger management (TriggerInfo)
    - Function/Procedure management (FunctionInfo, ProcedureInfo)
    
    // Type system
    - Domain management (DomainInfo)
    - Character set management (CharsetInfo)
    - Collation management (CollationCatalogInfo)
    - Timezone management (TimezoneInfo)
    
    // Security
    - User management (UserInfo)
    - Role management (RoleInfo)
    - Permission management (PermissionInfo)
    
    // Advanced features
    - Partition management
    - Replication management
    - Job scheduler management
    - Remote connector management
};
```

### System Catalog vs User Catalog

#### System Catalog (sb_* tables)

| Table Category | Tables | Description |
|----------------|--------|-------------|
| Core Identity | sb_database, sb_schema, sb_object, sb_object_name | Database and schema registry |
| Table Metadata | sb_tables, sb_columns | Table and column definitions |
| Indexes | sb_indexes, sb_index_columns | Index metadata |
| Constraints | sb_constraints, sb_foreign_keys | Constraint definitions |
| Types | sb_types, sb_domains, sb_type_modifiers | Type system |
| Security | sb_users, sb_roles, sb_permissions | Security metadata |
| Sequences | sb_sequences | Sequence generators |
| Views | sb_views | View definitions |
| Triggers | sb_triggers | Trigger definitions |
| Procedures | sb_procedures, sb_functions | Stored routines |
| Character Sets | sb_charsets, sb_collations | i18n support |
| Timezones | sb_timezones, sb_timezone_transitions | Timezone data |

#### Virtual Catalog (sys.* views)

**Source:** `src/catalog/sys_catalog.cpp:435`

| View | Description |
|------|-------------|
| sys.schemas | Schema registry view |
| sys.tables | Table metadata view |
| sys.columns | Column metadata view |
| sys.indexes | Index registry view |
| sys.index_columns | Index column mapping |
| sys.constraints | Constraint definitions |
| sys.foreign_keys | Foreign key relationships |
| sys.domains | Domain definitions |
| sys.sessions | Active sessions |
| sys.transactions | Transaction status |
| sys.locks | Lock status |
| sys.jobs | Job scheduler |
| sys.performance | Performance metrics |

### Storage Classes

| Class | Description | Persistence | Examples |
|-------|-------------|-------------|----------|
| `on-disk` | Persisted system tables | Database lifetime | sb_tables, sb_columns |
| `virtual` | Computed views | Session lifetime | sys.tables, sys.columns |
| `in-memory` | Runtime structures | Process lifetime | relcache, prepared_statement cache |

### Catalog Page Organization

**Source:** `src/core/catalog_manager.cpp:4502`

```cpp
struct CatalogRootPage {
    PageHeader header;
    uint32_t schema_count;
    uint32_t table_count;
    
    // Core catalog table page pointers
    uint32_t schemas_page;        // sb_schema
    uint32_t tables_page;         // sb_tables
    uint32_t columns_page;        // sb_columns
    uint32_t indexes_page;        // sb_indexes
    uint32_t constraints_page;    // sb_constraints
    uint32_t sequences_page;      // sb_sequences
    uint32_t views_page;          // sb_views
    uint32_t triggers_page;       // sb_triggers
    uint32_t permissions_page;    // sb_permissions
    // ... 150+ additional catalog page pointers
    
    uint8_t reserved[2920];       // Padding for 4KB page
};
```

### Catalog Table Categories

#### Category 1: Core Identity (Foundation)

| Table | Purpose | Records |
|-------|---------|---------|
| sb_database | Database identity | 1 per database |
| sb_schema | Schema registry | N schemas |
| sb_object | Object registry | N objects |
| sb_object_name | Name resolution | N names |

#### Category 2: Table Metadata (Core)

| Table | Purpose | Relationship |
|-------|---------|--------------|
| sb_tables | Table definitions | 1 per table |
| sb_columns | Column definitions | N per table |
| sb_indexes | Index definitions | N per table |
| sb_constraints | Constraint definitions | N per table |

#### Category 3: Type System

| Table | Purpose | Examples |
|-------|---------|----------|
| sb_types | Base types | INTEGER, VARCHAR, VECTOR |
| sb_domains | User-defined domains | Email, PhoneNumber |
| sb_type_modifiers | Type parameters | VARCHAR(100), DECIMAL(10,2) |
| sb_charsets | Character sets | UTF8, LATIN1 |
| sb_collations | Collations | utf8_general_ci |

#### Category 4: Programmability

| Table | Purpose | Features |
|-------|---------|----------|
| sb_procedures | Stored procedures | PSQL bytecode |
| sb_functions | Stored functions | Return types |
| sb_triggers | Triggers | Timing, events |
| sb_sequences | Generators | Auto-increment |
| sb_views | Views | SELECT definitions |

#### Category 5: Security

| Table | Purpose | Features |
|-------|---------|----------|
| sb_users | User accounts | Authentication |
| sb_roles | Roles | Permission groups |
| sb_permissions | ACL entries | Grant/revoke |
| sb_policies | Row-level security | RLS policies |

### Catalog Access Patterns

#### Read Patterns

```
1. Cache Lookup (fastest)
   - Check relcache for table metadata
   - Check syscache for type info
   
2. Virtual View Query
   - Query sys.* views for runtime state
   - Join on-disk tables with runtime data
   
3. On-Disk Table Scan
   - Full scan for catalog browsing
   - Index scan for specific lookups
```

#### Write Patterns

```
1. DDL Operations
   - CREATE: Insert new catalog records
   - ALTER: Update existing records
   - DROP: Set is_valid = 0 (soft delete)
   
2. Metadata Updates
   - Statistics: UPDATE sb_statistic
   - Row counts: UPDATE sb_tables
   - Index state: UPDATE sb_indexes
```

### UUID-Based Identity System

**Source:** `include/scratchbird/core/catalog_manager.h:76`

```cpp
using ID = UuidV7Bytes;  // 16-byte UUIDv7

// Zero UUID semantics
constexpr ID ZERO_UUID = {};  // 00000000-0000-0000-0000-000000000000

// Context-dependent meanings:
// - parent_schema_id = ZERO_UUID → Root schema
// - owner_id = ZERO_UUID → SYSTEM owner
// - tablespace_id = ZERO_UUID → Default tablespace
// - toast_table_id = ZERO_UUID → No TOAST table
```

### Catalog Caching Architecture

```
┌─────────────────────────────────────────────────────┐
│                 Session Cache                        │
│  ┌──────────────┐ ┌──────────────┐ ┌─────────────┐ │
│  │  Table Cache │ │  Type Cache  │ │ Schema Cache│ │
│  │  (relcache)  │ │  (syscache)  │ │             │ │
│  └──────────────┘ └──────────────┘ └─────────────┘ │
└─────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────┐
│              Catalog Manager (shared)                │
│         ┌─────────────────────────┐                 │
│         │   Catalog Tables Mutex  │                 │
│         └─────────────────────────┘                 │
└─────────────────────────────────────────────────────┘
```

## Invariants

| ID | Invariant | Enforcement |
|----|-----------|-------------|
| `CAT_INV_001` | Root schema ID equals database UUID | Bootstrap validation |
| `CAT_INV_002` | All catalog tables use PAGE_TYPE_HEAP | Page header check |
| `CAT_INV_003` | Record sizes are 8-byte aligned | sizeof() compile-time check |
| `CAT_INV_004` | UUIDv7 format for all object IDs | isUuidV7Local() validation |
| `CAT_INV_005` | is_valid flag consistent with MGA | Transaction visibility |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `CATALOG_NOT_FOUND` | Catalog table doesn't exist | Bootstrap repair |
| `CATALOG_CORRUPTED` | Checksum/page validation failed | Recovery mode |
| `DUPLICATE_OBJECT` | Object already exists | Rename or drop existing |
| `INVALID_UUID` | UUID format/version invalid | Reject operation |

## Related Specifications

- [bootstrap_sequence.md](./bootstrap_sequence.md) - Catalog initialization
- [uuid_mapping.md](./uuid_mapping.md) - UUID identity system
- [catalog_table_layouts.md](./catalog_table_layouts.md) - Table schemas
- [metadata_caching.md](./metadata_caching.md) - Cache system

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Catalog | Complete set of system tables storing metadata |
| MGA | Multi-Generational Architecture (versioning) |
| TOAST | The Oversized-Attribute Storage Technique |
| relcache | Relation metadata cache |
| syscache | System catalog cache |
| UUIDv7 | Time-ordered UUID format (RFC 9562) |
| Soft Delete | is_valid flag marking instead of physical deletion |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |

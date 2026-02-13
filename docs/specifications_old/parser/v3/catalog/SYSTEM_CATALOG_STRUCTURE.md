# ScratchBird System Catalog Structure

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

## Complete Schema Layout and Metadata Tables

**Last Updated**: January 28, 2026
**Status**: Authoritative (V3)

---

## Table of Contents

1. [Overview](#overview)
2. [Database Bootstrap](#database-bootstrap)
3. [System Schemas](#system-schemas)
4. [Catalog Root Page](#catalog-root-page)
5. [System Tables](#system-tables)
6. [In-Memory Structures](#in-memory-structures)
7. [Implementation Requirements](#implementation-requirements-v3)
8. [System Views](#system-views-required)
9. [Online Migration Catalogs](#online-migration-catalogs-authoritative)

---

## Overview

ScratchBird's system catalog is a metadata repository that tracks all database objects including schemas, tables, columns, indexes, constraints, sequences, views, triggers, permissions, and statistics.

### Storage Architecture

- **Root Catalog Page**: Page 1 (`CATALOG_ROOT_PAGE`) - Points to all system tables
- **System Tables**: Heap-based storage for metadata
- **In-Memory Cache**: Thread-safe caches for performance
- **TOAST References**: Large objects stored externally (ACLs, expressions, etc.)

### Character Set & Encoding

All identifiers use:
- **Encoding**: UTF-8 only
- **Character Limit**: 128 characters (SQL:2016 §5.2)
- **Storage Limit**: 512 bytes (128 chars × 4 bytes max per UTF-8 char)
- **Validation**: Character boundaries, no split multi-byte sequences

### Type Conventions (Authoritative)

- All `*_id` fields and catalog object identifiers are **UUID v7**.
- All `*_oid` fields are **UUID v7** TOAST/LOB references.
- `UUID_ZERO` denotes the all-zero UUID (used for “no reference”).
- `UuidV7Bytes` is the 16-byte binary UUID v7 representation.

---

## Database Bootstrap

### Initial Database Creation

When `Database::create()` is called, the following pages are initialized:

| Page # | Type | Contents |
|--------|------|----------|
| 0 | Header | Database metadata, magic number, UUID |
| 1 | System Catalog Bootstrap | Legacy base schema entries (SystemCatalogEntry) |
| 2 | FSM | Free Space Map |
| 3 | Catalog Root | System catalog root page (see below) |
| 4+ | Data | Available for allocation |

## System Schemas

### Default Schemas Created

ScratchBird bootstraps an 18-schema hierarchy used by native and emulated
dialects. The layout defines the canonical bootstrap roots for security,
monitoring, and emulation:

```
root
├── sys
│   ├── sec
│   │   ├── srv
│   │   ├── users
│   │   ├── roles
│   │   └── groups
│   ├── mon
│   └── agents
├── app
├── users
│   └── public
├── remote
│   └── emulation
│       ├── mysql
│       ├── postgresql
│       ├── mssql
│       └── firebird
```

**Note**: The `public` schema is created by default under `/users` and is
the default schema for normal users. Emulation schemas live only under
`/remote/emulation` (no separate `/emulated` root).

---

## Catalog Root Page

**Page ID**: 1 (fixed)
**Page Type**: `PAGE_TYPE_CATALOG_ROOT`

### Structure (CatalogRootPage)

```cpp
struct CatalogRootPage {
    PageHeader header;
    uint32_t schema_count;
    uint32_t table_count;

    // Core catalog tables
    uint32_t schemas_page;
    uint32_t tables_page;
    uint32_t columns_page;
    uint32_t indexes_page;
    uint32_t index_versions_page;
    uint32_t constraints_page;
    uint32_t sequences_page;
    uint32_t views_page;
    uint32_t triggers_page;
    uint32_t permissions_page;
    uint32_t statistics_page;
    uint32_t collations_page;     // Legacy collations page
    uint32_t timezones_page;
    uint32_t charsets_page;
    uint32_t collation_defs_page;

    // Dependencies, comments, DDL definitions + scheduler
    uint32_t dependencies_page;
    uint32_t comments_page;
    uint32_t object_definitions_page;
    uint32_t jobs_page;
    uint32_t job_runs_page;
    uint32_t job_dependencies_page;
    uint32_t job_secrets_page;

    // Security tables
    uint32_t users_page;
    uint32_t roles_page;
    uint32_t groups_page;
    uint32_t role_members_page;
    uint32_t group_members_page;
    uint32_t group_mappings_page;

    // Stored code tables
    uint32_t procedures_page;
    uint32_t proc_params_page;
    uint32_t domains_page;
    uint32_t udr_page;
    uint32_t exceptions_page;
    uint32_t packages_page;

    // Emulation tables
    uint32_t emulation_types_page;
    uint32_t emulation_servers_page;
    uint32_t emulated_dbs_page;

    // Tablespaces and extensions
    uint32_t tablespaces_page;
    uint32_t tablespace_files_page;
    uint32_t extensions_page;
    uint32_t foreign_keys_page;

    // Phase B: Synonyms, FDW, registry, UDR engines/modules
    uint32_t synonyms_page;
    uint32_t foreign_servers_page;
    uint32_t foreign_tables_page;
    uint32_t user_mappings_page;
    uint32_t server_registry_page;
    uint32_t udr_engines_page;
    uint32_t udr_modules_page;

    // Migration and transaction state
    uint32_t migration_history_page;
    uint32_t dormant_transactions_page;
    uint32_t prepared_transactions_page;

    // Security/audit
    uint32_t encryption_keys_page;
    uint32_t authkeys_page;
    uint32_t sessions_page;
    uint32_t audit_log_page;
    uint32_t security_policy_epoch_page;

    UuidV7Bytes policy_toast_table_id;     // UUID for policy expression TOAST storage
    uint32_t column_permissions_page;
    uint32_t object_permissions_page;
    uint32_t policies_page;

    uint8_t reserved[3760];       // Padding for 4KB page (336 bytes used)
};
```

**Size**: 4KB struct; stored in a normal database page (unused bytes remain if page size > 4KB).

**Note:** `index_versions_page` is required and stores index history/version rows.

---

## System Tables

### 1. Schemas Table (`schemas_page`)

Stores database schema definitions.

#### Disk Structure (SchemaRecord)

```cpp
struct SchemaRecord {
    UuidV7Bytes schema_id;
    UuidV7Bytes parent_schema_id;              // Parent schema UUID (zero UUID for root)
    char schema_name[512];            // UTF-8, max 128 characters
    UuidV7Bytes owner_id;                      // Owner UUID reference
    UuidV7Bytes default_tablespace_id;   // Default tablespace for new tables
    uint16_t permissions;             // Bitmask of schema permissions
    uint16_t default_charset;         // CharacterSet enum (0 = inherit)
    uint8_t name_is_delimited;        // 1 if quoted identifier
    uint8_t reserved;
    UuidV7Bytes default_collation_id;    // Collation ID (0 = inherit)
    UuidV7Bytes acl_oid;                 // TOAST reference for ACL (implemented)
    uint64_t created_time;            // Unix timestamp (microseconds)
    uint64_t last_modified_time;      // Unix timestamp (microseconds)
    uint32_t is_valid;                // 1 = valid, 0 = deleted
    uint32_t padding;                 // Alignment
};
```

**Size**: 600 bytes per record (packed)

#### SchemaType Enum

```cpp
enum class SchemaType : uint8_t {
    SYSTEM = 0,          // /sys/*
    USER_HOME = 1,       // /users/{username}/*
    REMOTE_NATIVE = 2,   // /remote/scratchbird/*
    REMOTE_EMULATED = 3, // /remote/emulation/*
    PUBLIC = 4,          // /users/public
    APPLICATION = 5      // User-created schemas
};
```

#### In-Memory Structure (SchemaInfo)

```cpp
struct SchemaInfo {
    UuidV7Bytes schema_id;
    UuidV7Bytes parent_schema_id;
    std::string schema_name;
    bool name_is_delimited = false;
    std::string full_path;
    SchemaType schema_type = SchemaType::APPLICATION;
    UuidV7Bytes owner_id;
    UuidV7Bytes default_tablespace_id = UUID_ZERO;
    uint16_t permissions = 0;
    uint16_t default_charset = 0;
    uint16_t reserved = 0;
    UuidV7Bytes default_collation_id = UUID_ZERO;
    UuidV7Bytes acl_oid = UUID_ZERO;
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

**Cache**: `std::unordered_map<UuidV7Bytes, SchemaInfo> schema_cache_`

---

### 2. Tables Table (`tables_page`)

Stores table metadata.

#### Disk Structure (TableRecord)

```cpp
enum class TableType : uint8_t {
    HEAP = 0,              // Regular heap table
    INDEX = 1,             // Index-organized table
    TEMPORARY = 2,         // Temporary table
    EXTERNAL = 3,          // External table
    MATERIALIZED_VIEW = 4, // Materialized view
    TOAST = 5              // TOAST table
};

enum class TempMetadataScope : uint8_t {
    NONE = 0,
    GLOBAL = 1,
    SESSION = 2
};

enum class TempDataScope : uint8_t {
    NONE = 0,
    SESSION = 1,
    TRANSACTION = 2
};

enum class TempOnCommitAction : uint8_t {
    NONE = 0,
    DELETE_ROWS = 1,
    PRESERVE_ROWS = 2,
    DROP = 3
};

struct TableRecord {
    UuidV7Bytes table_id;
    UuidV7Bytes schema_id;
    char table_name[512];             // UTF-8, max 128 characters
    UuidV7Bytes owner_id;                      // Owner UUID reference
    uint32_t root_page;
    uint32_t column_count;
    uint64_t row_count;
    uint8_t table_type;               // TableType enum
    uint8_t has_toast;                // 1 if table has TOAST
    uint8_t rls_enabled;              // Row-level security enabled
    uint8_t rls_forced;               // Force RLS for table owners
    uint8_t temp_metadata_scope;      // TempMetadataScope enum
    uint8_t temp_data_scope;          // TempDataScope enum
    uint8_t temp_on_commit;           // TempOnCommitAction enum
    uint8_t temp_flags;               // Reserved for temp table flags
    UuidV7Bytes tablespace_id;           // Tablespace ID (0 = default)
    uint16_t default_charset;         // CharacterSet enum (0 = inherit)
    uint8_t name_is_delimited;        // 1 if quoted identifier
    uint8_t reserved1;
    UuidV7Bytes default_collation_id;    // Collation ID (0 = inherit)
    UuidV7Bytes storage_params_oid;      // TOAST reference for storage parameters
    UuidV7Bytes creating_session_id;           // Session UUID for session-scoped temp metadata
    UuidV7Bytes creating_transaction_id; // Transaction ID for temp metadata
    UuidV7Bytes temp_parent_table_id;          // Internal temp instance parent table
    UuidV7Bytes temp_schema_id;                // Session-local temp schema
    uint64_t created_time;
    uint64_t last_modified_time;
    uint64_t policy_epoch;            // Security policy epoch
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: 686 bytes per record (packed)

#### In-Memory Structure (TableInfo)

```cpp
struct TableInfo {
    UuidV7Bytes table_id;
    UuidV7Bytes schema_id;
    std::string table_name;
    bool name_is_delimited = false;
    UuidV7Bytes owner_id;
    uint32_t root_page = 0;
    uint32_t column_count = 0;
    uint64_t row_count = 0;
    TableType table_type = TableType::HEAP;
    TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
    TempDataScope temp_data_scope = TempDataScope::NONE;
    TempOnCommitAction temp_on_commit = TempOnCommitAction::NONE;
    UuidV7Bytes creating_session_id{};
    UuidV7Bytes creating_transaction_id = UUID_ZERO;
    UuidV7Bytes temp_parent_table_id{};
    UuidV7Bytes temp_schema_id{};
    bool has_toast = false;
    UuidV7Bytes toast_table_id;
    UuidV7Bytes tablespace_id = UUID_ZERO;
    uint16_t default_charset = 0;
    UuidV7Bytes default_collation_id = UUID_ZERO;
    UuidV7Bytes storage_params_oid = UUID_ZERO;
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
    uint64_t policy_epoch = 0;

    // ONLINE migration (IMPLEMENTED)
    bool migration_in_progress = false;
    UuidV7Bytes migration_id;
    uint64_t migration_xid = 0;
    uint16_t migration_target_ts = 0;
    uint8_t migration_phase = 0;

    bool rls_enabled = false;
    bool rls_forced = false;
};
```

**Cache**: `std::unordered_map<UuidV7Bytes, TableInfo> table_cache_`

---

### 3. Columns Table (`columns_page`)

Stores column definitions for all tables.

#### Disk Structure (ColumnRecord)

```cpp
struct ColumnRecord {
    UuidV7Bytes table_id;                      // Parent table ID
    UuidV7Bytes column_id;                     // UUIDv7 unique identifier
    char column_name[512];            // UTF-8, max 128 characters
    uint16_t ordinal;                 // Column position in table (0-based)
    uint16_t data_type;               // Type code
    uint32_t type_precision;          // DECIMAL precision, VARCHAR length, VECTOR dimensions
    uint32_t type_scale;              // DECIMAL scale
    uint32_t max_length;              // Legacy field (use type_precision)
    UuidV7Bytes domain_id;                     // Domain ID (zero if not domain-based)
    uint8_t is_array;                 // 1 if column stores array values
    uint32_t array_size;              // Fixed array size (0 = unspecified)
    uint8_t nullable;                 // 1 = NULL allowed
    uint8_t has_default;              // 1 = has default value
    uint8_t is_primary_key;           // 1 = part of primary key
    uint8_t is_unique;                // 1 = unique constraint
    uint8_t is_foreign_key;           // 1 = foreign key
    uint8_t is_generated;             // 1 = generated column
    uint8_t storage_type;             // TOAST storage strategy
    uint8_t with_timezone;            // TIMESTAMP: 1 = WITH TIME ZONE
    uint8_t name_is_delimited;        // 1 if quoted identifier
    uint16_t charset;                 // Character set (0 = inherit)
    uint16_t timezone_hint;           // Timezone ID for display (0 = connection default)
    UuidV7Bytes collation_id;            // Collation ID (0 = inherit)
    char default_value[128];          // Serialized default (inline)
    UuidV7Bytes default_value_oid;       // TOAST reference for large defaults
    UuidV7Bytes check_expr_oid;          // TOAST reference for check expressions
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: 750 bytes per record (packed)

#### In-Memory Structure (ColumnInfo)

```cpp
enum class GeneratedColumnType : uint8_t {
    NOT_GENERATED = 0,
    STORED = 1,
    VIRTUAL = 2
};

struct ColumnInfo {
    UuidV7Bytes table_id;
    UuidV7Bytes column_id;
    std::string column_name;
    uint16_t ordinal = 0;
    uint16_t data_type = 0;
    uint32_t type_precision = 0;
    uint32_t type_scale = 0;
    uint32_t max_length = 0;          // Legacy
    UuidV7Bytes domain_id{};
    bool is_array = false;
    uint32_t array_size = 0;
    bool nullable = true;
    bool has_default = false;
    bool is_primary_key = false;
    bool is_unique = false;
    bool is_foreign_key = false;
    bool is_generated = false;
    GeneratedColumnType generated_type = GeneratedColumnType::NOT_GENERATED;
    std::string generation_expression;
    UuidV7Bytes generation_expr_oid = UUID_ZERO;
    std::vector<uint16_t> dependent_columns;
    bool is_identity = false;
    bool identity_always = true;
    UuidV7Bytes identity_sequence_id;
    uint8_t storage_type = 0;
    bool with_timezone = false;
    bool name_is_delimited = false;
    uint16_t charset = 0;
    uint16_t timezone_hint = 0;
    UuidV7Bytes collation_id = UUID_ZERO;
    std::string default_value;
    std::string default_expr;
    UuidV7Bytes default_value_oid = UUID_ZERO;
    std::string check_expr;
    UuidV7Bytes check_expr_oid = UUID_ZERO;
    uint64_t created_time = 0;
};
```

**Cache**: `std::unordered_map<UuidV7Bytes, std::vector<ColumnInfo>> column_cache_` (keyed by table_id)

---

### 4. Indexes Table (`indexes_page`)

Stores index metadata.

#### Disk Structure (IndexRecord)

```cpp
enum class IndexType : uint8_t {
    BTREE = 0,        // ✅ B-tree index
    HASH = 1,         // ✅ Hash index
    VECTOR = 2,       // ⚠️ HNSW vector similarity (TID updates incomplete)
    FULLTEXT = 3,     // ❌ Full-text search (NOT IMPLEMENTED)
    GIN = 4,          // ⚠️ Generalized Inverted Index (PARTIAL)
    GIST = 5,         // ⚠️ Generalized Search Tree (PARTIAL)
    BRIN = 6,         // ⚠️ Block Range Index (PARTIAL)
    RTREE = 7,        // ⚠️ R-tree spatial index (TID updates incomplete)
    SPGIST = 8,       // ⚠️ Space-Partitioned GiST (PARTIAL)
    BITMAP = 9,       // ✅ Bitmap index
    COLUMNSTORE = 10, // ✅ Columnstore index
    LSM = 11          // ✅ LSM-Tree
};

struct IndexRecord {
    UuidV7Bytes index_id;
    UuidV7Bytes table_id;
    char index_name[512];             // UTF-8, max 128 characters
    UuidV7Bytes owner_id;                      // Owner UUID reference
    uint32_t root_page;
    uint8_t index_type;               // IndexType enum
    uint8_t is_unique;
    uint16_t column_count;
    UuidV7Bytes column_ids[16];                // Max 16 columns
    uint16_t include_column_count;
    UuidV7Bytes include_column_ids[16];
    UuidV7Bytes index_params_oid;        // TOAST reference for index parameters
    UuidV7Bytes expression_oid;          // TOAST reference for expression tree(s)
    UuidV7Bytes predicate_oid;           // TOAST reference for WHERE predicate
    uint64_t created_time;
    uint32_t is_valid;

    // Shadow rebuild / versioning (current implementation)
    UuidV7Bytes logical_index_id;              // Stable logical index UUID
    uint8_t state;                    // 0=BUILDING, 1=ACTIVE, 2=RETIRED, 3=FAILED, 4=INACTIVE
    uint8_t name_is_delimited;        // 1 if quoted identifier
    uint8_t padding1[6];
    uint64_t valid_from_xid;          // XID when new txns can use this index
    uint64_t retired_xid;             // XID after which no new txns use this index
    uint64_t build_started_time;
    uint64_t build_completed_time;
};
```

**Size**: 1162 bytes per record (packed)

#### In-Memory Structure (IndexInfo)

```cpp
struct IndexInfo {
    UuidV7Bytes index_id;
    UuidV7Bytes table_id;
    std::string index_name;
    bool name_is_delimited = false;
    UuidV7Bytes owner_id;
    uint32_t root_page = 0;
    UuidV7Bytes tablespace_id = UUID_ZERO;
    IndexType index_type = IndexType::BTREE;
    bool is_unique = false;
    std::vector<UuidV7Bytes> column_ids;
    std::vector<UuidV7Bytes> include_column_ids;
    UuidV7Bytes index_params_oid = UUID_ZERO;
    uint64_t created_time = 0;
    UuidV7Bytes collation_id = 101;      // Default: utf8_general_ci

    // R-tree parameters
    uint32_t rtree_max_entries = 50;

    // Expression & Filtered Indexes (IMPLEMENTED)
    bool is_expression_index = false;
    bool is_partial_index = false;
    UuidV7Bytes expression_oid = UUID_ZERO;
    UuidV7Bytes predicate_oid = UUID_ZERO;
    std::vector<std::string> expression_strings;
    std::string predicate_string;
    std::vector<uint8_t> expression_data;   // Inline serialization
    std::vector<uint8_t> predicate_data;    // Inline serialization

    UuidV7Bytes dependency_id;                 // Dependency: index -> table

    // Shadow rebuild / versioning (current implementation)
    UuidV7Bytes logical_index_id;
    uint8_t state = 1;
    uint64_t valid_from_xid = 0;
    uint64_t retired_xid = 0;
    uint64_t build_started_time = 0;
    uint64_t build_completed_time = 0;
};
```

**Cache**: `std::unordered_map<UuidV7Bytes, IndexInfo> index_cache_`

---

### 5. Index Versions Table (`index_versions_page`)

Design target for shadow index rebuild + swap. The current implementation
stores versioning fields directly on `IndexRecord`; this table will become
the canonical source once wired and persisted.

#### Disk Structure (IndexVersionRecord)

```cpp
enum class IndexVersionState : uint8_t {
    BUILDING = 0,
    ACTIVE = 1,
    RETIRED = 2,
    FAILED = 3
};

struct IndexVersionRecord {
    UuidV7Bytes logical_index_id;             // Stable index identity (logical name)
    UuidV7Bytes index_id;                     // Physical index instance ID
    uint8_t state;                   // IndexVersionState
    uint8_t reserved[7];             // Padding for alignment
    uint64_t valid_from_xid;         // XID at which new txns can use this index
    uint64_t retired_xid;            // XID after which no new txns use this index
    uint64_t build_started_time;
    uint64_t build_completed_time;
    uint64_t created_time;
};
```

**Size**: ~80 bytes per record

#### In-Memory Structure (IndexVersionInfo)

```cpp
struct IndexVersionInfo {
    UuidV7Bytes logical_index_id;
    UuidV7Bytes index_id;
    IndexVersionState state = IndexVersionState::BUILDING;
    uint64_t valid_from_xid = 0;
    uint64_t retired_xid = 0;
    uint64_t build_started_time = 0;
    uint64_t build_completed_time = 0;
    uint64_t created_time = 0;
};
```

**Cache**: `std::unordered_map<UuidV7Bytes, std::vector<IndexVersionInfo>> index_versions_cache_`

---

### 6. Constraints Table (`constraints_page`)

**Status**: ✅ **Catalog persistence implemented** (enforcement handled outside catalog)

#### Disk Structure (ConstraintRecord)

```cpp
enum class CatalogConstraintType : uint8_t {
    PRIMARY_KEY = 0,
    FOREIGN_KEY = 1,
    UNIQUE = 2,
    CHECK = 3,
    NOT_NULL = 4,
    DEFAULT = 5,
    EXCLUSION = 6
};

struct ConstraintRecord {
    UuidV7Bytes constraint_id;
    UuidV7Bytes table_id;
    char constraint_name[512];       // UTF-8, max 128 characters
    UuidV7Bytes owner_id;                     // Owner UUID reference
    uint8_t constraint_type;         // CatalogConstraintType enum
    uint8_t is_deferrable;           // Can be deferred to end of transaction
    uint8_t initially_deferred;      // Initially deferred or immediate
    uint8_t is_enabled;              // Enabled flag
    uint8_t is_validated;            // Validated flag
    uint8_t is_system_generated;     // System-generated name flag
    uint8_t on_delete;               // FKAction enum
    uint8_t on_update;               // FKAction enum
    uint8_t match_type;              // FKMatchType enum
    uint16_t column_count;           // Number of columns involved
    UuidV7Bytes column_ids[16];               // Columns involved in constraint (max 16)
    UuidV7Bytes referenced_table_id;          // For foreign keys
    uint16_t referenced_column_count;
    uint8_t name_is_delimited;       // 1 if quoted identifier
    uint8_t reserved;
    UuidV7Bytes referenced_column_ids[16];    // Referenced columns for FK
    UuidV7Bytes check_expr_oid;         // TOAST reference for check expression
    UuidV7Bytes exclusion_operator_oid; // TOAST reference for exclusion operator
    UuidV7Bytes index_method_oid;       // TOAST reference for exclusion index method
    uint64_t created_time;
    uint64_t validated_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: 1,139 bytes per record (packed)

#### In-Memory Structure (ConstraintInfo)

```cpp
struct ConstraintInfo {
    UuidV7Bytes constraint_id;
    std::string constraint_name;
    bool name_is_delimited = false;
    UuidV7Bytes table_id;
    ConstraintType constraint_type;
    std::vector<std::string> column_names;
    std::string check_expression;
    UuidV7Bytes check_expr_oid = UUID_ZERO;
    UuidV7Bytes referenced_table_id;
    std::vector<std::string> referenced_columns;
    FKAction on_delete = FKAction::NO_ACTION;
    FKAction on_update = FKAction::NO_ACTION;
    FKMatchType match_type = FKMatchType::SIMPLE;
    std::string exclusion_operator;
    std::string index_method;
    bool is_deferrable = false;
    bool initially_deferred = false;
    bool is_enabled = true;
    bool is_validated = true;
    bool is_system_generated = false;
    UuidV7Bytes owner_id;
    uint64_t created_time = 0;
    uint64_t validated_time = 0;
};
```

**Cache**: `std::unordered_map<UuidV7Bytes, ConstraintInfo> constraints_cache_`

---

### 7. Sequences Table (`sequences_page`)

**Status**: ✅ **FULLY IMPLEMENTED** (November 3, 2025)

#### Disk Structure (SequenceRecord)

```cpp
struct SequenceRecord {
    UuidV7Bytes sequence_id;
    UuidV7Bytes schema_id;
    char sequence_name[512];        // UTF-8, max 128 characters
    UuidV7Bytes owner_id;                    // Owner UUID reference
    UuidV7Bytes owned_by_table_id;           // Optional owned-by table ID
    UuidV7Bytes owned_by_column_id;          // Optional owned-by column ID
    int64_t current_value;
    int64_t increment_by;
    int64_t min_value;
    int64_t max_value;
    int64_t start_value;
    int64_t cache_size;
    uint8_t cycle;                  // 1 if cycle, 0 if no cycle
    uint8_t name_is_delimited;      // 1 if quoted identifier
    uint8_t reserved[6];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: 672 bytes per record (packed)

#### In-Memory Structure (SequenceInfo + SequenceState)

```cpp
struct SequenceInfo {
    UuidV7Bytes sequence_id;
    UuidV7Bytes schema_id;
    std::string name;
    bool name_is_delimited = false;
    UuidV7Bytes owner_id;
    UuidV7Bytes owned_by_table_id{};
    UuidV7Bytes owned_by_column_id{};
    int64_t current_value;
    int64_t increment_by;
    int64_t min_value;
    int64_t max_value;
    int64_t start_value;
    int64_t cache_size;
    bool cycle;
    uint64_t created_time;
    uint64_t last_modified_time;
    TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
    UuidV7Bytes creating_session_id{};
    UuidV7Bytes creating_transaction_id = UUID_ZERO;
};

// Runtime state (atomic operations)
struct SequenceState {
    UuidV7Bytes sequence_id;
    UuidV7Bytes schema_id;
    std::string name;
    bool name_is_delimited = false;
    UuidV7Bytes owner_id;
    UuidV7Bytes owned_by_table_id{};
    UuidV7Bytes owned_by_column_id{};
    std::atomic<int64_t> current_value;  // Thread-safe atomic
    int64_t increment_by;
    int64_t min_value;
    int64_t max_value;
    int64_t start_value = 0;
    int64_t cache_size = 1;
    bool cycle;
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
    TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
    UuidV7Bytes creating_session_id{};
    UuidV7Bytes creating_transaction_id = UUID_ZERO;
    std::mutex config_mutex;             // Protect ALTER SEQUENCE changes
};
```

**Cache**:
- `std::unordered_map<UuidV7Bytes, std::shared_ptr<SequenceState>> sequence_cache_`
- `std::unordered_map<std::string, UuidV7Bytes> sequence_name_to_id_`

**DDL Support**:
- ✅ CREATE SEQUENCE
- ✅ ALTER SEQUENCE
- ✅ DROP SEQUENCE
- ✅ NEXTVAL(), CURRVAL(), SETVAL()

---

### 8. Views Table (`views_page`)

**Status**: ✅ **Catalog persistence implemented**

#### Disk Structure (ViewRecord)

```cpp
struct ViewRecord {
    UuidV7Bytes view_id;                       // UUIDv7 unique identifier
    UuidV7Bytes schema_id;                     // Parent schema ID
    char view_name[512];              // UTF-8, max 128 characters
    UuidV7Bytes owner_id;                      // Owner UUID reference
    UuidV7Bytes materialized_table_id;         // Physical table backing materialized view
    UuidV7Bytes change_log_table_id;           // Change log table for fast refresh (0 if none)
    UuidV7Bytes definition_oid;          // TOAST reference for view definition SQL
    UuidV7Bytes columns_oid;             // TOAST reference for explicit column list
    UuidV7Bytes base_table_ids_oid;      // TOAST reference for base table IDs (MV)
    uint8_t name_is_delimited;        // 1 if quoted identifier
    uint8_t check_option;             // 1 if WITH CHECK OPTION
    uint8_t is_materialized;          // 1 if materialized view
    uint8_t refresh_strategy;         // MVRefreshStrategy enum
    uint8_t refresh_on_commit;        // 1 if refresh on commit
    uint8_t supports_concurrent;      // 1 if concurrent refresh supported
    uint8_t reserved[2];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint64_t last_refreshed;          // For materialized views
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: 644 bytes per record (packed)

#### In-Memory Structure (ViewInfo)

```cpp
struct ViewInfo {
    UuidV7Bytes view_id;
    UuidV7Bytes schema_id;
    std::string name;
    bool name_is_delimited = false;
    UuidV7Bytes owner_id;
    std::string definition;           // SELECT query text
    bool check_option;
    std::vector<std::string> column_names;  // Optional explicit columns
    uint64_t created_time;
    uint64_t last_modified_time;
    TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
    UuidV7Bytes creating_session_id{};
    UuidV7Bytes creating_transaction_id = UUID_ZERO;

    // Materialized view fields
    bool materialized;
    UuidV7Bytes materialized_table_id;
    uint64_t last_refresh_time;
    MVRefreshStrategy refresh_strategy;
    bool refresh_on_commit;
    std::vector<UuidV7Bytes> base_table_ids;
    UuidV7Bytes change_log_table_id;
    bool supports_concurrent;
};
```

**Cache**:
- `std::unordered_map<UuidV7Bytes, ViewInfo> view_cache_`
- `std::unordered_map<std::string, UuidV7Bytes> view_name_to_id_`

**Catalog Notes**:
- View definitions and column lists are persisted via TOAST OIDs.
- Materialized view metadata is persisted on disk even if execution support is staged.

---

### 9. Triggers Table (`triggers_page`)

**Status**: ✅ **Catalog persistence implemented**

#### Disk Structure (TriggerRecord)

```cpp
struct TriggerRecord {
    UuidV7Bytes trigger_id;
    UuidV7Bytes table_id;                    // Zero for database triggers
    char trigger_name[512];           // UTF-8, max 128 characters
    UuidV7Bytes owner_id;                      // Owner UUID reference
    uint8_t scope;                    // 0=TABLE trigger, 1=DATABASE trigger
    uint8_t name_is_delimited;        // 1 if quoted identifier
    uint8_t trigger_timing;           // Table triggers: BEFORE/AFTER
    uint8_t trigger_event;            // Table: event mask, DB: DatabaseTriggerEvent
    uint8_t granularity;              // Table: FOR_EACH_ROW/FOR_EACH_STATEMENT
    uint8_t enabled;                  // 1 if enabled/active
    uint8_t reserved[2];
    int32_t position;                 // DB trigger position (0 for table triggers)
    UuidV7Bytes condition_oid;           // TOAST reference for WHEN condition
    UuidV7Bytes action_oid;              // TOAST reference for trigger action or procedure name
    UuidV7Bytes old_alias_oid;           // TOAST reference for OLD TABLE alias (statement triggers)
    UuidV7Bytes new_alias_oid;           // TOAST reference for NEW TABLE alias (statement triggers)
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: 612 bytes per record (packed)

---

### 10. Permissions Table (`permissions_page`)

**Status**: ✅ **Catalog persistence implemented**

#### Disk Structure (PermissionRecord)

```cpp
struct PermissionRecord {
    UuidV7Bytes permission_id;
    UuidV7Bytes object_id;         // Schema, table, view, sequence ID
    uint8_t object_type;  // ObjectType enum
    UuidV7Bytes grantee_id;        // User, Role, or Group UUID
    uint8_t grantee_type; // USER=0, ROLE=1, GROUP=2, PUBLIC=3
    uint32_t privileges;  // Bitmask of Privilege enum
    uint8_t grant_option; // 1 if WITH GRANT OPTION
    UuidV7Bytes grantor_id;        // User UUID of grantor
    uint8_t reserved[6];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: 93 bytes per record (packed)

#### In-Memory Structure (PermissionInfo)

```cpp
struct PermissionInfo {
    UuidV7Bytes permission_id;
    UuidV7Bytes object_id;
    PermissionObjectType object_type;
    UuidV7Bytes grantee_id;
    GranteeType grantee_type;
    uint32_t privileges;
    bool grant_option = false;
    UuidV7Bytes grantor_id;
    uint64_t created_time = 0;
};
```

**Cache**: No dedicated cache (lookups scan the catalog heap page).

---

### Column Permissions Table (`column_permissions_page`)

#### Disk Structure (ColumnPermissionRecord)

```cpp
struct ColumnPermissionRecord {
    UuidV7Bytes permission_id;
    UuidV7Bytes table_id;
    char column_name[128];
    UuidV7Bytes grantee_id;        // User, Role, Group, or PUBLIC UUID
    uint8_t grantee_type; // USER=1, ROLE=2, GROUP=3, PUBLIC=4
    uint32_t privileges;  // SELECT/UPDATE/INSERT/REFERENCES bitmask
    uint8_t grant_option;
    UuidV7Bytes grantor_id;
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: 214 bytes per record (packed)

#### In-Memory Structure (ColumnPermissionInfo)

```cpp
struct ColumnPermissionInfo {
    UuidV7Bytes permission_id;
    UuidV7Bytes table_id;
    std::string column_name;
    UuidV7Bytes grantee_id;
    GranteeType grantee_type;
    uint32_t privileges;
    bool grant_option = false;
    UuidV7Bytes grantor_id;
    uint64_t created_time = 0;
};
```

**Cache**: No dedicated cache (lookups scan the catalog heap page).

---

### Row-Level Security Policies (`policies_page`)

#### Disk Structure (PolicyRecord)

```cpp
struct PolicyRecord {
    UuidV7Bytes policy_id;
    UuidV7Bytes table_id;
    char policy_name[64];
    uint8_t policy_type;      // ALL=0, SELECT=1, INSERT=2, UPDATE=3, DELETE=4
    UuidV7Bytes roles_oid;       // TOAST reference for role UUID list
    UuidV7Bytes using_expr_oid;  // TOAST reference for USING expression
    UuidV7Bytes with_check_expr_oid; // TOAST reference for WITH CHECK expression
    uint8_t is_enabled;
    uint64_t created_time;
    uint64_t modified_time;
    uint32_t is_valid;
    uint8_t padding[3];
};
```

**Size**: 133 bytes per record (packed)

#### In-Memory Structure (PolicyInfo)

```cpp
struct PolicyInfo {
    UuidV7Bytes policy_id;
    UuidV7Bytes table_id;
    std::string policy_name;
    PolicyType policy_type;
    std::vector<UuidV7Bytes> role_ids;    // Empty = all roles
    std::string using_expr;
    std::string with_check_expr;
    bool is_enabled = true;
    uint64_t created_time = 0;
    uint64_t modified_time = 0;
};
```

**Cache**: `std::unordered_map<UuidV7Bytes, PolicyInfo> policy_cache_`

---

### Object Permissions Table (`object_permissions_page`)

#### Disk Structure (ObjectPermissionRecord)

```cpp
struct ObjectPermissionRecord {
    UuidV7Bytes permission_id;
    UuidV7Bytes object_id;
    uint8_t object_type;    // 1=PROCEDURE, 2=FUNCTION, 3=VIEW, 4=TABLE, 5=SEQUENCE
    UuidV7Bytes grantee_id;
    uint8_t grantee_type;   // 1=USER, 2=ROLE, 3=GROUP
    uint32_t permissions;   // EXECUTE/SELECT/INSERT/UPDATE/DELETE/USAGE
    uint8_t grant_option;
    UuidV7Bytes grantor_id;
    uint64_t created_time;
    uint8_t is_valid;
    uint8_t padding[6];
};
```

**Size**: 86 bytes per record (packed)

#### In-Memory Structure (ObjectPermissionInfo)

```cpp
struct ObjectPermissionInfo {
    UuidV7Bytes permission_id;
    UuidV7Bytes object_id;
    ObjectType object_type;
    UuidV7Bytes grantee_id;
    GranteeType grantee_type;
    uint32_t permissions;
    bool grant_option = false;
    UuidV7Bytes grantor_id;
    uint64_t created_time = 0;
};
```

**Cache**: `std::unordered_map<UuidV7Bytes, std::vector<ObjectPermissionInfo>> object_permissions_cache_`

---

### 11. Statistics Table (`statistics_page`)

**Status**: ⚠️ **PARTIALLY IMPLEMENTED** (catalog persistence in place)

#### Disk Structure (StatisticRecord)

```cpp
struct StatisticRecord {
    UuidV7Bytes statistic_id;
    UuidV7Bytes table_id;
    UuidV7Bytes column_id;
    uint16_t data_type;           // DataType enum value
    uint16_t reserved1;
    uint64_t num_rows;
    uint64_t num_nulls;
    float null_fraction;
    uint64_t num_distinct;
    float avg_width;
    UuidV7Bytes mcv_oid;             // TOAST reference for MCV list (JSON)
    UuidV7Bytes histogram_oid;       // TOAST reference for histogram (JSON)
    uint8_t histogram_type;       // 0=equal_height, 1=equal_width, 255=none
    uint8_t padding1[3];
    uint32_t histogram_bucket_count;
    uint64_t last_analyzed_time;
    uint64_t sample_size;
    float sample_rate;
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding2;
};
```

**Size**: 144 bytes per record (packed)

#### In-Memory Structure (StatisticInfo)

```cpp
struct StatisticInfo {
    UuidV7Bytes statistic_id;
    UuidV7Bytes table_id;
    UuidV7Bytes column_id;
    uint16_t data_type = 0;
    uint16_t reserved1 = 0;
    uint64_t num_rows = 0;
    uint64_t num_nulls = 0;
    float null_fraction = 0.0f;
    uint64_t num_distinct = 0;
    float avg_width = 0.0f;
    UuidV7Bytes mcv_oid = UUID_ZERO;
    UuidV7Bytes histogram_oid = UUID_ZERO;
    uint8_t histogram_type = 0;
    uint32_t histogram_bucket_count = 0;
    uint64_t last_analyzed_time = 0;
    uint64_t sample_size = 0;
    float sample_rate = 0.0f;
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

---

### Dependencies Table (`dependencies_page`)

**Status**: ✅ **IMPLEMENTED** (Phase 1.4 catalog corrections)

#### Disk Structure (DependencyRecord)

```cpp
struct DependencyRecord {
    UuidV7Bytes dependency_id;           // Unique dependency record ID
    UuidV7Bytes dependent_object_id;     // Object that depends on something
    uint8_t dependent_type;     // VIEW, TRIGGER, FK, PROCEDURE, etc.
    uint8_t reserved1[7];
    UuidV7Bytes referenced_object_id;    // Object being depended upon
    uint8_t referenced_type;    // TABLE, VIEW, SEQUENCE, etc.
    uint8_t dependency_type;    // NORMAL, AUTO, INTERNAL, PIN
    uint8_t reserved2[6];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Cache**: `std::unordered_map<UuidV7Bytes, DependencyInfo> dependency_cache_`

---

### 12. Timezones Table (`timezones_page`)

**Status**: ⚠️ **PARTIALLY IMPLEMENTED** (structure defined, minimal usage)

#### Disk Structure (TimezoneRecord)

```cpp
struct TimezoneRecord {
    UuidV7Bytes timezone_id;             // Unique timezone ID
    char name[64];                    // e.g., "America/New_York"
    char abbreviation[16];            // e.g., "EST", "PST"
    int32_t std_offset_minutes;       // Standard offset from GMT
    uint8_t observes_dst;             // 1 = observes DST
    uint8_t reserved1;
    uint16_t reserved2;
    // DST transition rules
    uint8_t dst_start_month;          // Month DST starts (1-12)
    uint8_t dst_start_week;           // Week of month (1-5)
    uint8_t dst_start_day;            // Day of week (0-6)
    uint8_t dst_start_hour;           // Hour DST starts (0-23)
    uint8_t dst_end_month;
    uint8_t dst_end_week;
    uint8_t dst_end_day;
    uint8_t dst_end_hour;
    int32_t dst_offset_minutes;       // Additional DST offset
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~136 bytes per record

**Cache**: ❌ NOT IMPLEMENTED

**Usage**: Minimal (TIMESTAMP WITH TIME ZONE support incomplete)

---

### 13. Character Sets Table (`charsets_page`)

**Status**: ⚠️ **PARTIALLY IMPLEMENTED** (pg_charset)

#### Disk Structure (CharsetRecord)

```cpp
struct CharsetRecord {
    UuidV7Bytes charset_id;              // Matches CharacterSet enum
    char name[64];                    // e.g., "utf8", "latin1"
    char description[128];            // Human-readable description
    uint8_t min_bytes;                // Minimum bytes per character
    uint8_t max_bytes;                // Maximum bytes per character
    uint8_t variable_width;           // 1 = variable, 0 = fixed
    uint8_t reserved1;
    UuidV7Bytes default_collation_id;    // Default collation
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~232 bytes per record

**Cache**: ❌ NOT IMPLEMENTED

**Usage**: UTF-8 is the only supported charset

---

### 14. Collations Table (`collation_defs_page`)

**Status**: ⚠️ **PARTIALLY IMPLEMENTED** (pg_collation)

#### Disk Structure (CollationRecord)

```cpp
struct CollationRecord {
    UuidV7Bytes collation_id;
    char name[128];                   // e.g., "utf8_general_ci"
    UuidV7Bytes charset_id;              // Associated character set
    uint8_t collation_type;           // CollationType enum
    uint8_t strength;                 // CollationStrength enum
    uint8_t pad_space;                // 1 = PAD SPACE, 0 = NO PAD
    uint8_t is_default;               // 1 = default for charset
    uint16_t reserved;
    char locale[32];                  // Locale string (e.g., "en_US")
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~208 bytes per record

**Cache**: ❌ NOT IMPLEMENTED

**Usage**: Default collation (utf8_general_ci) used, but not fully configurable

---

### 15. Comments Table (`comments_page`)

#### Disk Structure (CommentRecord)

```cpp
struct CommentRecord {
    UuidV7Bytes comment_id;
    UuidV7Bytes object_id;               // Object being commented
    uint8_t object_type;        // TABLE, COLUMN, VIEW, etc.
    uint8_t reserved[7];
    UuidV7Bytes owner_id;                // Owner UUID reference
    UuidV7Bytes comment_text_oid;  // TOAST reference - comment text
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 16. Object Definitions Table (`object_definitions_page`)

#### Disk Structure (ObjectDefinitionRecord)

```cpp
struct ObjectDefinitionRecord {
    UuidV7Bytes object_id;
    uint8_t object_type;        // CatalogManager::ObjectType
    uint8_t reserved[7];
    UuidV7Bytes ddl_text_oid;      // TOAST reference for original DDL SQL
    UuidV7Bytes bytecode_oid;      // TOAST reference for compiled SBLR bytecode
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 17. Users Table (`users_page`)

#### Disk Structure (UserRecord)

```cpp
struct UserRecord {
    UuidV7Bytes user_id;
    char username[512];
    UuidV7Bytes password_hash_oid; // TOAST reference
    UuidV7Bytes user_metadata_oid; // TOAST reference
    UuidV7Bytes default_schema_id;
    uint8_t is_active;
    uint8_t is_superuser;
    uint8_t reserved[6];
    uint64_t created_time;
    uint64_t last_login_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 18. Roles Table (`roles_page`)

#### Disk Structure (RoleRecord)

```cpp
struct RoleRecord {
    UuidV7Bytes role_id;
    char role_name[512];
    UuidV7Bytes owner_id;
    UuidV7Bytes default_schema_id;
    UuidV7Bytes role_metadata_oid; // TOAST reference
    uint8_t is_active;
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 19. Groups Table (`groups_page`)

#### Disk Structure (GroupRecord)

```cpp
struct GroupRecord {
    UuidV7Bytes group_id;
    char group_name[512];
    char external_id[512];      // AD/LDAP group ID (empty if local)
    uint8_t group_type;         // LOCAL, AD, LDAP
    uint8_t reserved[7];
    UuidV7Bytes default_schema_id;
    UuidV7Bytes group_metadata_oid; // TOAST reference
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 20. Role Memberships Table (`role_members_page`)

#### Disk Structure (RoleMembershipRecord)

```cpp
struct RoleMembershipRecord {
    UuidV7Bytes membership_id;
    UuidV7Bytes user_id;
    UuidV7Bytes role_id;
    UuidV7Bytes granted_by;
    uint8_t with_admin_option;
    uint8_t reserved[7];
    uint64_t granted_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 21. Group Memberships Table (`group_members_page`)

#### Disk Structure (GroupMembershipRecord)

```cpp
struct GroupMembershipRecord {
    UuidV7Bytes membership_id;
    UuidV7Bytes user_id;           // User or Group UUID (for nesting)
    uint8_t member_type;  // USER=0, GROUP=1
    uint8_t reserved1[7];
    UuidV7Bytes group_id;
    UuidV7Bytes granted_by;
    uint64_t granted_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 22. Group Mappings Table (`group_mappings_page`)

#### Disk Structure (GroupMappingRecord)

```cpp
struct GroupMappingRecord {
    UuidV7Bytes mapping_id;
    char external_group_name[512]; // LDAP DN, Kerberos principal, AD SID
    uint8_t auth_method;           // LDAP=1, KERBEROS=2, AD=3
    uint8_t auto_create_users;     // 1 = auto-create users on first login
    uint8_t reserved[6];
    UuidV7Bytes internal_group_id;
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 23. Auth Keys Table (`authkeys_page`)

#### Disk Structure (AuthKeyRecord)

```cpp
struct AuthKeyRecord {
    UuidV7Bytes authkey_id;
    char issuer[256];
    uint64_t valid_from;
    uint64_t valid_to;
    uint32_t usage_limit;
    uint32_t usage_count;
    uint8_t status;             // AuthKeyStatus enum
    uint8_t usage_type;         // AuthKeyUsage enum
    uint8_t reserved[6];
    UuidV7Bytes role_scope_oid;    // TOAST reference for role scope UUID list
    UuidV7Bytes group_scope_oid;   // TOAST reference for group scope UUID list
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 24. Sessions Table (`sessions_page`)

#### Disk Structure (SessionRecord)

```cpp
struct SessionRecord {
    UuidV7Bytes session_id;
    UuidV7Bytes user_id;
    UuidV7Bytes authkey_id;
    char emulation_mode[64];
    uint64_t login_time;
    uint64_t last_activity_time;
    UuidV7Bytes current_schema_id;
    uint64_t policy_epoch_global;
    uint64_t policy_epoch_table;
    uint8_t is_expired;
    uint8_t reserved[7];
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 25. Audit Log Table (`audit_log_page`)

#### Disk Structure (AuditLogRecord)

```cpp
struct AuditLogRecord {
    UuidV7Bytes event_id;
    uint64_t timestamp;
    uint16_t event_type;
    uint8_t success;
    uint8_t reserved1;
    UuidV7Bytes session_id;
    UuidV7Bytes authkey_id;
    UuidV7Bytes user_id;
    UuidV7Bytes role_id;
    UuidV7Bytes target_user_id;
    UuidV7Bytes object_id;
    char username[128];
    char target_username[128];
    char object_type[64];
    char object_name[512];
    UuidV7Bytes details_oid;       // TOAST reference for details JSON
    uint8_t hash_prev[32];
    uint8_t hash_curr[32];
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 26. Security Policy Epoch Table (`security_policy_epoch_page`)

#### Disk Structure (SecurityPolicyEpochRecord)

```cpp
struct SecurityPolicyEpochRecord {
    uint64_t global_epoch;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 27. Procedures Table (`procedures_page`)

#### Disk Structure (ProcedureRecord)

```cpp
struct ProcedureRecord {
    UuidV7Bytes procedure_id;
    UuidV7Bytes schema_id;
    char procedure_name[512];
    UuidV7Bytes owner_id;
    uint8_t procedure_type;     // PROCEDURE vs FUNCTION
    uint8_t is_selectable;      // 1 if has SUSPEND
    uint8_t language;           // PSQL, SQL, UDR, etc.
    uint8_t sql_security;       // 0=DEFINER, 1=INVOKER
    uint8_t name_is_delimited;  // 1 if quoted identifier
    uint8_t body_redacted;      // 1 if body intentionally not stored
    uint8_t deterministic;      // 0/1
    uint8_t reserved;
    uint32_t parameter_count;
    UuidV7Bytes return_type_oid;   // TOAST reference for return type definition
    UuidV7Bytes body_oid;          // TOAST reference for procedure/function body
    UuidV7Bytes bytecode_oid;      // TOAST reference for compiled SBLR bytecode
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 28. Procedure Parameters Table (`proc_params_page`)

#### Disk Structure (ProcedureParameterRecord)

```cpp
struct ProcedureParameterRecord {
    UuidV7Bytes parameter_id;
    UuidV7Bytes procedure_id;
    char parameter_name[512];
    uint16_t parameter_position; // 1-based
    uint8_t parameter_mode;      // IN, OUT, INOUT
    uint8_t reserved[5];
    UuidV7Bytes data_type_oid;      // TOAST reference for data type definition
    UuidV7Bytes default_value_oid;  // TOAST reference for default value expression
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 29. Domains Table (`domains_page`)

Domains are persisted by `DomainManager` in the catalog heap page. The on-disk
record layout is defined in `ScratchBird/src/core/domain_manager.cpp`:

```cpp
struct DomainRecord {
    UuidV7Bytes domain_id;
    UuidV7Bytes schema_id;
    char domain_name[128];
    uint8_t domain_type;         // DomainType enum
    uint16_t base_type;          // DataType enum
    uint32_t precision;
    uint32_t scale;
    uint8_t nullable;
    char default_value[256];
    UuidV7Bytes parent_domain_id;         // For inheritance
    uint8_t is_valid;
    uint64_t created_time;
    uint64_t last_modified_time;
    UuidV7Bytes constraints_oid;    // TOAST reference
    UuidV7Bytes fields_oid;         // TOAST reference
    UuidV7Bytes enum_values_oid;    // TOAST reference
    UuidV7Bytes security_oid;       // TOAST reference
    UuidV7Bytes integrity_oid;      // TOAST reference
    UuidV7Bytes validation_oid;     // TOAST reference
    UuidV7Bytes quality_oid;        // TOAST reference
    uint16_t set_element_type;   // For SET domains
    char dialect_tag[32];
    char compat_name[128];
    uint16_t reserved;
};
```

---

### 30. UDR Table (`udr_page`)

#### Disk Structure (UDRRecord)

```cpp
struct UDRRecord {
    UuidV7Bytes udr_id;
    UuidV7Bytes schema_id;
    char udr_name[512];
    UuidV7Bytes owner_id;
    char library_path[1024];
    char entry_point[512];
    uint8_t udr_type;           // FUNCTION, PROCEDURE, TRIGGER
    uint8_t name_is_delimited;  // 1 if quoted identifier
    uint8_t reserved[6];
    UuidV7Bytes signature_oid;     // TOAST reference
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 31. Packages Table (`packages_page`)

#### Disk Structure (PackageRecord)

```cpp
struct PackageRecord {
    UuidV7Bytes package_id;
    UuidV7Bytes schema_id;
    char package_name[512];
    UuidV7Bytes owner_id;
    UuidV7Bytes package_header_oid; // TOAST reference
    UuidV7Bytes package_body_oid;   // TOAST reference
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint8_t name_is_delimited;
    uint8_t padding[3];
};
```

---

### 32. Exceptions Table (`exceptions_page`)

#### Disk Structure (ExceptionRecord)

```cpp
struct ExceptionRecord {
    UuidV7Bytes exception_id;
    UuidV7Bytes schema_id;
    char name[512];
    UuidV7Bytes message_oid;       // TOAST reference for message text
    UuidV7Bytes owner_id;
    uint64_t created_time;
    uint64_t last_modified_time;
    uint8_t is_valid;
    uint8_t name_is_delimited;
    uint8_t padding[6];
};
```

---

### 33. Emulation Types Table (`emulation_types_page`)

#### Disk Structure (EmulationTypeRecord)

```cpp
struct EmulationTypeRecord {
    UuidV7Bytes emulation_type_id;
    char emulation_name[64];    // "mysql", "postgres", "mssql", "firebird"
    uint8_t version_major;
    uint8_t version_minor;
    uint16_t reserved;
    UuidV7Bytes mapping_rules_oid; // TOAST reference - JSON mapping rules
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 34. Emulation Servers Table (`emulation_servers_page`)

#### Disk Structure (EmulationServerRecord)

```cpp
struct EmulationServerRecord {
    UuidV7Bytes server_id;
    char server_name[512];
    UuidV7Bytes emulation_type_id;
    UuidV7Bytes owner_id;
    UuidV7Bytes server_config_oid; // TOAST reference - JSON server configuration
    uint8_t is_active;
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 35. Emulated Databases Table (`emulated_dbs_page`)

#### Disk Structure (EmulatedDatabaseRecord)

```cpp
struct EmulatedDatabaseRecord {
    UuidV7Bytes emulated_db_id;
    char database_name[512];
    UuidV7Bytes server_id;
    UuidV7Bytes schema_id;
    UuidV7Bytes owner_id;
    UuidV7Bytes db_metadata_oid;   // TOAST reference - JSON database metadata
    uint8_t is_active;
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 36. Synonyms Table (`synonyms_page`)

#### Disk Structure (SynonymRecord)

```cpp
struct SynonymRecord {
    UuidV7Bytes synonym_id;
    UuidV7Bytes schema_id;
    char synonym_name[512];
    UuidV7Bytes owner_id;
    UuidV7Bytes target_path_oid;  // TOAST reference for target path
    uint8_t target_type;       // ObjectType enum
    uint8_t is_public;
    uint8_t name_is_delimited;
    uint8_t reserved[5];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 37. Foreign Servers Table (`foreign_servers_page`)

#### Disk Structure (ForeignServerRecord)

```cpp
struct ForeignServerRecord {
    UuidV7Bytes server_id;
    char server_name[512];
    char server_type[128];
    char host[512];
    uint16_t port;
    uint16_t reserved0;
    UuidV7Bytes connection_options_oid; // TOAST reference for JSON options
    UuidV7Bytes owner_id;
    uint8_t is_active;
    uint8_t reserved1[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 38. Foreign Tables Table (`foreign_tables_page`)

#### Disk Structure (ForeignTableRecord)

```cpp
struct ForeignTableRecord {
    UuidV7Bytes foreign_table_id;
    UuidV7Bytes schema_id;
    char table_name[512];
    UuidV7Bytes foreign_server_id;
    char remote_schema[512];
    char remote_table[512];
    UuidV7Bytes owner_id;
    UuidV7Bytes column_mapping_oid; // TOAST reference for column mapping JSON
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint8_t name_is_delimited;
    uint8_t padding[3];
};
```

---

### 39. User Mappings Table (`user_mappings_page`)

#### Disk Structure (UserMappingRecord)

```cpp
struct UserMappingRecord {
    UuidV7Bytes mapping_id;
    UuidV7Bytes user_id;
    UuidV7Bytes foreign_server_id;
    char remote_user[512];
    UuidV7Bytes remote_credentials_oid; // TOAST reference for encrypted credentials
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 40. Server Registry Table (`server_registry_page`)

#### Disk Structure (ServerRegistryRecord)

```cpp
struct ServerRegistryRecord {
    UuidV7Bytes server_id;
    char server_name[512];
    char host[512];
    uint16_t port;
    uint8_t role;                 // ServerRole enum
    uint8_t state;                // ServerState enum
    uint32_t reserved0;
    uint64_t last_heartbeat;
    uint64_t last_xid;
    uint64_t replication_lag_ms;
    char cluster_id[256];
    char server_version[128];
    UuidV7Bytes metadata_oid;        // TOAST reference for JSON metadata
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 41. UDR Engines Table (`udr_engines_page`)

#### Disk Structure (UDREngineRecord)

```cpp
struct UDREngineRecord {
    UuidV7Bytes engine_id;
    char engine_name[512];
    uint8_t engine_type;          // UDREngineType enum
    uint8_t is_active;
    uint8_t is_default;
    uint8_t reserved0;
    char plugin_path[1024];
    UuidV7Bytes config_oid;          // TOAST reference for JSON config
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 42. UDR Modules Table (`udr_modules_page`)

#### Disk Structure (UDRModuleRecord)

```cpp
struct UDRModuleRecord {
    UuidV7Bytes module_id;
    char module_name[512];
    UuidV7Bytes engine_id;
    char library_path[1024];
    char checksum[128];
    char entry_point[512];
    UuidV7Bytes dependencies_oid;    // TOAST reference for dependency list
    uint8_t is_loaded;
    uint8_t is_validated;
    uint8_t reserved0[6];
    uint64_t loaded_count;
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 43. Foreign Keys Table (`foreign_keys_page`)

#### Disk Structure (ForeignKeyRecord)

```cpp
struct ForeignKeyRecord {
    UuidV7Bytes fk_id;
    char fk_name[512];
    UuidV7Bytes child_table_id;
    UuidV7Bytes parent_table_id;
    char child_columns[1024];
    char parent_columns[1024];
    uint8_t on_delete;
    uint8_t on_update;
    uint8_t match_type;
    uint8_t is_enabled;
    uint8_t reserved[4];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 44. Migration History Table (`migration_history_page`)

#### Disk Structure (MigrationHistoryInfo)

```cpp
struct MigrationHistoryInfo {
    UuidV7Bytes history_id;
    UuidV7Bytes migration_id;
    UuidV7Bytes table_id;
    uint16_t source_tablespace;
    uint16_t target_tablespace;
    MigrationPhase final_phase;
    uint64_t migration_xid;
    uint32_t total_pages;
    uint32_t pages_copied;
    uint64_t start_time;
    uint64_t end_time;
    uint32_t catch_up_iterations;
    uint64_t total_bytes_copied;
    uint8_t is_valid;
    uint8_t padding[7];
};
```

---

### 45. Dormant Transactions Table (`dormant_transactions_page`)

#### Disk Structure (DormantTransactionRecord)

```cpp
struct DormantTransactionRecord {
    UuidV7Bytes dormant_id;
    UuidV7Bytes attachment_id;
    UuidV7Bytes proc_id;
    uint32_t reserved0;
    UuidV7Bytes txn_id;
    UuidV7Bytes session_id;
    UuidV7Bytes user_id;
    UuidV7Bytes session_user_id;
    UuidV7Bytes role_id;
    uint8_t isolation_level;
    uint8_t access_mode;
    uint8_t wait_mode;
    uint8_t autocommit_mode;
    uint32_t lock_timeout_seconds;
    UuidV7Bytes current_schema_id;
    UuidV7Bytes session_settings_oid;
    UuidV7Bytes last_statement_oid;
    uint64_t last_statement_hash;
    uint8_t last_statement_type;
    uint8_t last_statement_status;
    uint8_t state;
    uint8_t reserved1;
    uint64_t start_time;
    uint64_t last_activity_time;
    uint64_t dormant_since;
    uint64_t lease_expires_at;
    uint64_t last_statement_time;
    int64_t last_rows_affected;
    uint32_t last_error_code;
    char last_sqlstate[6];
    UuidV7Bytes server_instance_id;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

### 46. Prepared Transactions Table (`prepared_transactions_page`)

#### Disk Structure (PreparedTransactionRecord)

```cpp
struct PreparedTransactionRecord {
    UuidV7Bytes prepared_id;
    UuidV7Bytes txn_id;
    UuidV7Bytes owner_id;
    UuidV7Bytes database_id;
    char gid[512];
    uint64_t prepared_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

## In-Memory Structures

### Catalog Manager Caches

Catalog uses a `std::recursive_mutex` for core operations plus per-cache
mutexes for hot paths. The list below is representative (not exhaustive):

```cpp
class CatalogManager {
private:
    // Schema cache
    std::unordered_map<UuidV7Bytes, SchemaInfo> schema_cache_;
    uint32_t schema_count_ = 0;

    // Table cache
    std::unordered_map<UuidV7Bytes, TableInfo> table_cache_;
    uint32_t table_count_ = 0;

    // Column cache (keyed by table_id)
    std::unordered_map<UuidV7Bytes, std::vector<ColumnInfo>> column_cache_;

    // Index cache
    std::unordered_map<UuidV7Bytes, IndexInfo> index_cache_;

    // Index version cache (logical index -> versions)
    std::unordered_map<UuidV7Bytes, std::vector<IndexVersionInfo>> index_versions_cache_;

    // Constraints, dependencies, comments
    std::unordered_map<UuidV7Bytes, ConstraintInfo> constraints_cache_;
    std::unordered_map<UuidV7Bytes, DependencyInfo> dependency_cache_;
    std::unordered_map<UuidV7Bytes, CommentInfo> comment_cache_;

    // Sequence cache (thread-safe atomic operations)
    std::unordered_map<UuidV7Bytes, std::shared_ptr<SequenceState>> sequence_cache_;
    std::unordered_map<std::string, UuidV7Bytes> sequence_name_to_id_;
    std::mutex sequence_cache_mutex_;
    std::mutex sequence_name_mutex_;

    // View cache
    std::unordered_map<UuidV7Bytes, ViewInfo> view_cache_;
    std::unordered_map<std::string, UuidV7Bytes> view_name_to_id_;
    std::mutex view_cache_mutex_;

    // TRUNCATE job tracking
    std::unordered_map<uint64_t, std::shared_ptr<TruncateJob>> truncate_jobs_;
    std::mutex truncate_jobs_mutex_;
    uint64_t next_job_id_ = 1;
};
```

### Cache Load Strategy

On `Database::open()` the catalog root page is read and all catalog tables
are loaded into their respective caches. Dependencies and comments are
best-effort and will not fail database open on load errors.

---

## Implementation Requirements (V3)

Catalog persistence is defined above. The following parser/executor behaviors
MUST be implemented to make the catalog authoritative:

### Constraints (Enforcement)
- ALTER TABLE ADD CONSTRAINT
- PRIMARY KEY enforcement
- FOREIGN KEY enforcement (CASCADE/RESTRICT)
- UNIQUE constraints
- CHECK constraints
- NOT NULL constraints
- DEFAULT constraints

### Security/Permissions
- GRANT/REVOKE statements
- Role management DDL
- User/group management DDL
- Row-level security DDL + enforcement

### Triggers
- CREATE/DROP TRIGGER
- Trigger execution
- BEFORE/AFTER/INSTEAD OF timing

### Advanced DDL
- ALTER TABLE (all sub-forms)
- ALTER COLUMN
- DROP COLUMN
- RENAME TABLE/COLUMN
- CREATE TYPE (domains, composite, enum, set, range)
- CREATE SCHEMA / DROP SCHEMA

### Views
- CREATE/DROP views
- Query expansion
- Updatable views
- Materialized views + refresh
- WITH CHECK OPTION enforcement

### Indexes
- All 28 core index types MUST be fully implemented.
- Index version history MUST be persisted in `index_versions_page`.
- Full-text index type is core (see index specs for tokenizer and operator bindings).

### Statistics
- ANALYZE statement
- Automatic statistics gathering
- Histogram bounds
- Most common values (MCVs)
- Query planner must consume these statistics deterministically.

### Character Sets & Collations
- UTF-8 encoding
- Collation-aware comparisons
- Multiple character sets
- Collation configuration and runtime load via SBCL weights

---

## System Views (Required)

Implement in-memory or materialized views for metadata queries:
- `information_schema.tables`
- `information_schema.columns`
- `information_schema.views`
- `information_schema.sequences`
- `pg_catalog.pg_class`
- `pg_catalog.pg_attribute`
- `pg_catalog.pg_index`
- `pg_catalog.pg_constraint`

---

## Online Migration Catalogs (Authoritative)

### Tablespace Migration State (Authoritative)

Tracks online tablespace migrations (see `/docs/specifications/parser/v3/storage/TABLESPACE_ONLINE_MIGRATION.md`).

```cpp
struct TablespaceMigrationRecord {
    UuidV7Bytes table_id;                      // Table UUID
    UuidV7Bytes source_tablespace_id;
    UuidV7Bytes target_tablespace_id;
    UuidV7Bytes shadow_table_id;               // Shadow table UUID
    UuidV7Bytes delta_log_id;                  // Delta log UUID
    uint8_t state;                    // PREPARE|COPY|CATCH_UP|CUTOVER|CLEANUP|DONE|FAILED|CANCELED
    uint64_t migration_start_xid;
    uint64_t cutover_xid;
    uint64_t rows_copied;
    uint64_t rows_total_est;
    double rows_per_sec;
    uint64_t eta_seconds;
    uint64_t last_lag_ms;
    uint64_t last_lag_sample_at;
    uint8_t throttle_state;           // NONE|LAG|IO|MANUAL
    uint32_t throttle_sleep_ms;
    uint64_t last_progress_at;
    int32_t last_error_code;
    uint64_t created_time;
    uint64_t updated_time;
    uint32_t is_valid;
};
```

### Shard Migration State (Cluster)

Tracks cross-node shard migration and rebalancing (see `SBCLUSTER-11-SHARD-MIGRATION-AND-REBALANCING.md`).

```cpp
struct ShardMigrationRecord {
    UuidV7Bytes migration_id;                  // Migration UUID
    UuidV7Bytes source_shard_id;               // Source shard UUID
    UuidV7Bytes target_shard_id;               // Target shard UUID
    uint8_t state;                    // PREPARE|COPY|CATCH_UP|CUTOVER|CLEANUP|DONE|FAILED|CANCELED
    uint64_t migration_start_xid;
    uint64_t cutover_xid;
    uint64_t bytes_copied;
    uint64_t rows_copied;
    double bytes_per_sec;
    double rows_per_sec;
    uint64_t eta_seconds;
    uint64_t last_lag_ms;
    uint64_t last_lag_sample_at;
    uint8_t throttle_state;           // NONE|LAG|IO|MANUAL
    uint32_t throttle_sleep_ms;
    uint64_t last_progress_at;
    int32_t last_error_code;
    uint64_t created_time;
    uint64_t updated_time;
    uint32_t is_valid;
};
```

### Shard Map Versions (Cluster)

Immutable shard map versions used for routing.

```cpp
struct ShardMapVersionRecord {
    uint64_t map_version;
    UuidV7Bytes map_uuid;
    uint64_t created_time;
    UuidV7Bytes created_by;
    UuidV7Bytes signature_oid;           // Optional TOAST reference
    uint32_t is_valid;
};
```

---

## Summary

### Current State

- **Fully Implemented**: Schemas, Tables, Columns, Sequences, Views (DDL + query)
- **Partially Implemented**: Statistics, Timezones, Charsets, Collations, Index coverage (GIN/GiST/SP-GiST/BRIN/Full-text/HNSW/R-tree)
- **Execution Gaps**: Constraint enforcement, permissions DDL, trigger execution, index versioning table, most ALTER operations
- **Constraint Enforcement**: Defined in `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md` and MUST be implemented.

### Size Estimates (16KB pages)

| Table | Record Size | Records/Page | Notes |
|-------|-------------|--------------|-------|
| Schemas | 600 bytes | ~27 | Low volume (bootstrap + user schemas) |
| Tables | 686 bytes | ~23 | Medium volume |
| Columns | 750 bytes | ~21 | High volume |
| Indexes | 1162 bytes | ~14 | Medium volume |
| Index Versions | ~80 bytes | ~200 | Low volume (required) |
| Constraints | 1,139 bytes | ~14 | Catalog persistence in place |
| Sequences | 672 bytes | ~24 | Low volume |
| Views | 644 bytes | ~25 | Low volume |
| Triggers | 612 bytes | ~26 | Low volume |
| Permissions | 93 bytes | ~171 | Low volume |
| Statistics | 144 bytes | ~111 | Low usage |

### Recommendations Priority

1. **Critical**: Implement constraint enforcement
2. **High**: Enhance view execution (updatable views, MV refresh)
3. **High**: Move index versioning into `index_versions_page`
4. **Medium**: Implement GRANT/REVOKE
5. **Medium**: Complete ALTER TABLE operations
6. **Low**: Trigger support
7. **Low**: Multi-database support

---

**End of Document**

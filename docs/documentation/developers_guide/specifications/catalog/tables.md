# Specification: Table Metadata

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:423`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:390`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:4883`

## Synopsis

This specification defines table metadata storage, including the TableInfo structure, table types, temporary table semantics, and the sb_tables catalog table layout.

## Scope

### In Scope

- Table metadata structures (TableInfo)
- Table types and their semantics
- Temporary table scopes and lifecycle
- Table-to-TOAST relationships
- Tablespace assignments
- Row-level security flags
- Online migration tracking

### Out of Scope

- Column metadata (see `columns.md`)
- Index metadata (see `indexes.md`)
- Physical heap page layout (see storage specs)

## Specification

### Table Types

**Source:** `include/scratchbird/core/catalog_manager.h:390`

```cpp
enum class TableType : uint8_t {
    HEAP = 0,              // Regular heap table
    INDEX = 1,             // Index-organized table
    TEMPORARY = 2,         // Temporary table
    EXTERNAL = 3,          // External table (FDW)
    MATERIALIZED_VIEW = 4, // Materialized view
    TOAST = 5              // TOAST storage table
};
```

**Table Type Characteristics:**

| Type | Storage | Persistence | Use Case |
|------|---------|-------------|----------|
| HEAP | Heap pages | Permanent | Standard tables |
| INDEX | Index structure | Permanent | IOT (Index-Organized Table) |
| TEMPORARY | Heap pages | Session/Transaction | Temp data |
| EXTERNAL | External | Session | FDW tables |
| MATERIALIZED_VIEW | Heap pages | Until REFRESH | Cached query results |
| TOAST | TOAST pages | Permanent | Large value storage |

### Temporary Table Scopes

**Source:** `include/scratchbird/core/catalog_manager.h:400`

```cpp
// Metadata scope - where table definition is visible
enum class TempMetadataScope : uint8_t {
    NONE = 0,       // Not temporary
    GLOBAL = 1,     // Visible to all sessions
    SESSION = 2     // Visible only to creating session
};

// Data scope - where table data is visible
enum class TempDataScope : uint8_t {
    NONE = 0,       // Not temporary
    SESSION = 1,    // Data persists for session
    TRANSACTION = 2 // Data cleared on commit
};

// ON COMMIT action
enum class TempOnCommitAction : uint8_t {
    NONE = 0,         // Not applicable
    DELETE_ROWS = 1,  // Delete all rows
    PRESERVE_ROWS = 2,// Keep rows
    DROP = 3          // Drop table entirely
};
```

### TableInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:423`

```cpp
struct TableInfo {
    // Identity
    ID table_id;                    // UUIDv7 unique identifier
    ID schema_id;                   // Containing schema
    std::string table_name;         // Table name
    bool name_is_delimited = false; // Quoted identifier flag
    ID owner_id;                    // Owner UUID
    
    // Storage
    GPID root_gpid = 0;             // Root page of table data
    uint32_t column_count = 0;      // Number of columns
    uint64_t row_count = 0;         // Estimated row count
    uint16_t tablespace_id = 0;     // Tablespace (0 = primary)
    ID tablespace_uuid{};           // Tablespace UUID reference
    
    // Table type
    TableType table_type = TableType::HEAP;
    
    // Temporary table attributes
    TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
    TempDataScope temp_data_scope = TempDataScope::NONE;
    TempOnCommitAction temp_on_commit = TempOnCommitAction::NONE;
    ID creating_session_id{};       // Session UUID for temp tables
    uint64_t creating_transaction_id = 0;
    ID temp_parent_table_id{};      // Internal temp instance parent
    ID temp_schema_id{};            // Session-local temp schema
    
    // TOAST relationship
    bool has_toast = false;
    ID toast_table_id;              // UUID of TOAST table
    
    // Defaults
    uint16_t default_charset = 0;
    ID default_charset_uuid{};
    uint32_t default_collation_id = 0;
    ID storage_params_oid{};        // TOAST for storage params
    
    // Timestamps
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
    
    // Security
    uint64_t policy_epoch = 0;      // Security policy epoch
    bool rls_enabled = false;       // Row-level security enabled
    bool rls_forced = false;        // Force RLS for table owners
    
    // Migration state (Sprint 4 Task 5.4.1)
    bool migration_in_progress = false;
    ID migration_id;
    uint64_t migration_xid = 0;
    uint16_t migration_target_ts = 0;
    uint8_t migration_phase = 0;
};
```

**Field Descriptions:**

| Field | Type | Description |
|-------|------|-------------|
| table_id | ID | UUIDv7 primary key |
| schema_id | ID | Parent schema reference |
| table_name | string | Up to 128 UTF-8 chars |
| name_is_delimited | bool | Quoted = case-sensitive |
| owner_id | ID | Owning user UUID |
| root_gpid | GPID | First data page |
| column_count | uint32 | Number of columns |
| row_count | uint64 | Statistics estimate |
| tablespace_id | uint16 | 0 = primary tablespace |
| table_type | TableType | HEAP, INDEX, etc. |
| has_toast | bool | Has overflow table |
| toast_table_id | ID | TOAST table reference |
| rls_enabled | bool | Row-level security |
| policy_epoch | uint64 | Security version |

### sb_tables Catalog Table

**Source:** `src/core/catalog_manager.cpp:4883`

```cpp
struct TableRecord {
    // Primary key
    ID table_id;
    
    // Schema reference
    ID schema_id;
    char table_name[512];
    ID owner_id;
    
    // Physical storage
    uint64_t root_gpid;
    uint32_t column_count;
    uint64_t row_count;
    
    // Type flags (packed)
    uint8_t table_type;
    uint8_t has_toast;
    uint8_t rls_enabled;
    uint8_t rls_forced;
    uint8_t temp_metadata_scope;
    uint8_t temp_data_scope;
    uint8_t temp_on_commit;
    uint8_t temp_flags;
    uint8_t name_is_delimited;
    
    // Storage references
    ID tablespace_id;
    ID default_charset_id;
    uint32_t default_collation_id;
    ID storage_params_oid;
    
    // Temporary table context
    ID creating_session_id;
    uint64_t creating_transaction_id;
    ID temp_parent_table_id;
    ID temp_schema_id;
    
    // TOAST relationship
    ID toast_table_id;
    
    // Timestamps and versioning
    uint64_t created_time;
    uint64_t last_modified_time;
    uint64_t policy_epoch;
    
    // MGA soft delete
    uint32_t is_valid;
    uint32_t padding;
};
```

### Table Create Options

```cpp
struct TableCreateOptions {
    TableType table_type = TableType::HEAP;
    TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
    TempDataScope temp_data_scope = TempDataScope::NONE;
    TempOnCommitAction temp_on_commit = TempOnCommitAction::NONE;
    ID creating_session_id{};
    uint64_t creating_transaction_id = 0;
    ID temp_parent_table_id{};
    ID temp_schema_id{};
    bool force_table_id = false;   // Use forced_table_id
    ID forced_table_id{};
};
```

### Temporary Table Lifecycle

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   CREATE    │────▶│    USE      │────▶│   CLEANUP   │
│  TEMP TABLE │     │  (Session)  │     │             │
└─────────────┘     └─────────────┘     └─────────────┘
                                              │
       ┌──────────────────────────────────────┼──────────┐
       │                                      │          │
       ▼                                      ▼          ▼
┌─────────────┐                      ┌─────────────┐ ┌─────────┐
│ ON COMMIT   │                      │   DROP      │ │ Session │
│  DROP       │                      │   TABLE     │ │  End    │
└─────────────┘                      └─────────────┘ └─────────┘
```

**Lifecycle Rules:**

| Scope | Data Visibility | Cleanup Trigger |
|-------|-----------------|-----------------|
| GLOBAL | All sessions | DROP TABLE |
| SESSION | Creating session only | Session end or DROP |
| TRANSACTION | Creating transaction | Transaction end |

### TOAST Table Relationship

```
┌─────────────────┐         ┌─────────────────┐
│   User Table    │◄────────│   TOAST Table   │
│  (main data)    │  1:N    │ (large values)  │
└─────────────────┘         └─────────────────┘
        │
        │ has_toast = true
        │ toast_table_id = {uuid}
        ▼
┌─────────────────┐
│  TOAST Pointer  │
│  in main row    │
└─────────────────┘
```

**TOAST Trigger Conditions:**
- Row size > 2KB (configurable)
- Individual column > 2KB
- Variable-length columns (TEXT, BYTEA, JSON)

### Row-Level Security (RLS)

```cpp
// RLS policy enforcement flags
struct TableRLSInfo {
    bool enabled;           // RLS active for this table
    bool forced;            // Apply to table owner too
    ID policy_id;           // Reference to RLS policy
    uint64_t epoch;         // Policy version for cache invalidation
};
```

**RLS Enforcement:**
1. If `rls_enabled = false`: No policy checks
2. If `rls_enabled = true, rls_forced = false`: Skip for owner
3. If `rls_enabled = true, rls_forced = true`: Always check

## Algorithms

### Algorithm: Create Table

```
Input:  Schema ID, table name, column definitions, options
Output: Table ID

1. Validate table name uniqueness in schema
2. Generate UUIDv7 for table_id
3. Allocate root GPID for table data
4. If has variable-length columns:
   a. Create TOAST table
   b. Set has_toast = true
   c. Set toast_table_id
5. Create sb_tables record
6. Create sb_columns records for each column
7. If temporary:
   a. Set temp_metadata_scope, temp_data_scope
   b. Set creating_session_id
8. Commit transaction
9. Return table_id
```

### Algorithm: Drop Table

```
Input:  Table ID, cascade flag
Output: Success/Failure

1. Verify table exists and is valid
2. Check for dependent objects:
   a. Foreign keys referencing this table
   b. Views using this table
   c. Triggers on this table
3. If dependencies exist and !cascade:
   a. Return ERROR_DEPENDENCY_EXISTS
4. If cascade:
   a. Drop dependent objects recursively
5. Mark table as invalid (is_valid = 0)
   - MGA: existing transactions can still see it
6. Free data pages (deferred if transactions active)
7. If has TOAST table:
   a. Drop TOAST table
8. Commit transaction
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|--------------|
| `TABLE_INV_001` | table_id is valid UUIDv7 | isUuidV7Local() check |
| `TABLE_INV_002` | schema_id references valid schema | Foreign key check |
| `TABLE_INV_003` | root_gpid is valid or 0 (for views) | GPID validation |
| `TABLE_INV_004` | column_count matches actual columns | Consistency check |
| `TABLE_INV_005` | TOAST table exists iff has_toast = true | Referential integrity |
| `TABLE_INV_006` | Temporary tables have valid session_id | Session validation |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `TABLE_EXISTS` | Name conflict in schema | Choose different name |
| `INVALID_TABLE_TYPE` | Unsupported table type | Use valid type |
| `TOAST_CREATE_FAILED` | TOAST table creation failed | Retry or abort |
| `DEPENDENCY_EXISTS` | Objects depend on table | Use CASCADE or drop dependents |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_catalog_tables.cpp` | Table CRUD operations |
| `tests/unit/test_temp_tables.cpp` | Temporary table lifecycle |
| `tests/unit/test_toast_tables.cpp` | TOAST relationship |
| `tests/unit/test_rls_tables.cpp` | Row-level security |

## Related Specifications

- [columns.md](./columns.md) - Column metadata
- [indexes.md](./indexes.md) - Index metadata
- [constraints.md](./constraints.md) - Table constraints
- [ddl_operations.md](./ddl_operations.md) - CREATE TABLE implementation

## Appendix

### Table Record Size Calculation

| Field | Size | Cumulative |
|-------|------|------------|
| Header (record_id, etc.) | 48 | 48 |
| table_id | 16 | 64 |
| schema_id | 16 | 80 |
| table_name | 512 | 592 |
| owner_id | 16 | 608 |
| root_gpid | 8 | 616 |
| column_count | 4 | 620 |
| row_count | 8 | 628 |
| Type flags (9 bytes) | 9 | 637 |
| Padding | 3 | 640 |
| tablespace_id | 16 | 656 |
| default_charset_id | 16 | 672 |
| default_collation_id | 4 | 676 |
| storage_params_oid | 16 | 692 |
| Temporary fields (48 bytes) | 48 | 740 |
| toast_table_id | 16 | 756 |
| Timestamps (24 bytes) | 24 | 780 |
| policy_epoch | 8 | 788 |
| is_valid | 4 | 792 |
| padding | 4 | 796 |

**Total: ~800 bytes per table record**

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |

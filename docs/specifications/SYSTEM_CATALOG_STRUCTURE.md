# ScratchBird System Catalog Structure
## Complete Schema Layout and Metadata Tables

**Last Updated**: November 7, 2025
**Status**: Current Implementation

---

## Table of Contents

1. [Overview](#overview)
2. [Database Bootstrap](#database-bootstrap)
3. [System Schemas](#system-schemas)
4. [Catalog Root Page](#catalog-root-page)
5. [System Tables](#system-tables)
6. [In-Memory Structures](#in-memory-structures)
7. [Missing/Deferred Features](#missing-deferred-features)
8. [Recommendations](#recommendations)

---

## Overview

ScratchBird's system catalog is a metadata repository that tracks all database objects including schemas, tables, columns, indexes, constraints, sequences, views, triggers, permissions, and statistics.

### Storage Architecture

- **Root Catalog Page**: Page 0 - Points to all system tables
- **System Tables**: Heap-based storage for metadata
- **In-Memory Cache**: Thread-safe caches for performance
- **TOAST References**: Large objects stored externally (ACLs, expressions, etc.)

### Character Set & Encoding

All identifiers use:
- **Encoding**: UTF-8 only
- **Character Limit**: 128 characters (SQL:2016 §5.2)
- **Storage Limit**: 512 bytes (128 chars × 4 bytes max per UTF-8 char)
- **Validation**: Character boundaries, no split multi-byte sequences

---

## Database Bootstrap

### Initial Database Creation

When `Database::create()` is called, the following pages are initialized:

| Page # | Type | Contents |
|--------|------|----------|
| 0 | Header | Database metadata, magic number, UUID |
| 1 | Catalog Root | System catalog page (see below) |
| 2 | FSM | Free Space Map |
| 3+ | Data | Available for allocation |

### Default Schemas Created

8 base schemas are created during bootstrap (hard-coded UUIDs):

| Schema Name | Purpose | Parent | UUID Slot |
|-------------|---------|--------|-----------|
| `[root]` | Root schema, parent of all | (none) | 0 |
| `[sys]` | System objects | [root] | 1 |
| `[sec]` | Security objects | [root] | 2 |
| `[agents]` | Agent-related objects | [root] | 3 |
| `[app]` | Application objects | [root] | 4 |
| `[remote]` | Remote/distributed objects | [root] | 5 |
| `[users]` | User management | [root] | 6 |
| `[roles]` | Role management | [root] | 7 |

**Note**: The `PUBLIC` schema is NOT created by default. User tables currently go into the first available schema (typically `[sys]`).

---

## Catalog Root Page

**Page ID**: 1 (fixed)
**Page Type**: `PAGE_TYPE_CATALOG`

### Structure (CatalogRootPage)

```cpp
struct CatalogRootPage {
    PageHeader header;                // Standard page header
    uint32_t schema_count;            // Number of schemas
    uint32_t table_count;             // Number of tables
    uint32_t schemas_page;            // Page containing schemas table
    uint32_t tables_page;             // Page containing tables table
    uint32_t columns_page;            // Page containing columns table
    uint32_t indexes_page;            // Page containing indexes table
    uint32_t constraints_page;        // Page containing constraints table
    uint32_t sequences_page;          // ✅ Page containing sequences table
    uint32_t views_page;              // ✅ Page containing views table
    uint32_t triggers_page;           // Page containing triggers table
    uint32_t permissions_page;        // Page containing permissions table
    uint32_t statistics_page;         // Page containing statistics table
    uint32_t collations_page;         // Legacy collations page
    uint32_t timezones_page;          // Page containing timezones table
    uint32_t charsets_page;           // Page containing character sets (pg_charset)
    uint32_t collation_defs_page;     // Page containing collations (pg_collation)
    uint8_t reserved[4024];           // Padding to 16KB
};
```

**Size**: Exactly 16KB (default page size)

---

## System Tables

### 1. Schemas Table (`schemas_page`)

Stores database schema definitions.

#### Disk Structure (SchemaRecord)

```cpp
struct SchemaRecord {
    ID schema_id;                     // UUIDv7 unique identifier
    char schema_name[512];            // UTF-8, max 128 characters
    char owner[512];                  // UTF-8, max 128 characters
    uint16_t default_tablespace_id;   // Default tablespace for new tables
    uint16_t permissions;             // Bitmask of schema permissions
    uint16_t default_charset;         // CharacterSet enum (0 = inherit)
    uint16_t reserved;
    uint32_t default_collation_id;    // Collation ID (0 = inherit)
    uint32_t acl_oid;                 // ❌ TOAST reference for ACL (NOT IMPLEMENTED)
    uint32_t search_path_oid;         // ❌ TOAST reference for search path (NOT IMPLEMENTED)
    uint64_t created_time;            // Unix timestamp (microseconds)
    uint64_t last_modified_time;      // Unix timestamp (microseconds)
    uint32_t is_valid;                // 1 = valid, 0 = deleted
    uint32_t padding;                 // Alignment
};
```

**Size**: ~1,144 bytes per record

#### In-Memory Structure (SchemaInfo)

```cpp
struct SchemaInfo {
    ID schema_id;
    std::string schema_name;
    std::string owner;
    uint16_t default_tablespace_id = 0;
    uint16_t permissions = 0;
    uint16_t default_charset = 0;
    uint32_t default_collation_id = 0;
    uint32_t acl_oid = 0;             // ❌ NOT USED
    uint32_t search_path_oid = 0;     // ❌ NOT USED
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

**Cache**: `std::unordered_map<ID, SchemaInfo> schema_cache_`

---

### 2. Tables Table (`tables_page`)

Stores table metadata.

#### Disk Structure (TableRecord)

```cpp
enum class TableType : uint8_t {
    HEAP = 0,              // Regular heap table
    INDEX = 1,             // Index-organized table (NOT IMPLEMENTED)
    TEMPORARY = 2,         // Temporary table (NOT IMPLEMENTED)
    EXTERNAL = 3,          // External table (NOT IMPLEMENTED)
    MATERIALIZED_VIEW = 4, // Materialized view (NOT IMPLEMENTED)
    TOAST = 5              // TOAST table (NOT IMPLEMENTED)
};

struct TableRecord {
    ID table_id;                      // UUIDv7 unique identifier
    ID schema_id;                     // Parent schema ID
    char table_name[512];             // UTF-8, max 128 characters
    uint32_t root_page;               // Root page of table heap
    uint32_t column_count;            // Number of columns
    uint64_t row_count;               // Estimated row count
    uint8_t table_type;               // TableType enum
    uint8_t has_toast;                // 1 if table has TOAST (NOT IMPLEMENTED)
    uint16_t tablespace_id;           // Tablespace ID (0 = default)
    uint16_t default_charset;         // CharacterSet enum (0 = inherit)
    uint16_t reserved1;
    uint32_t default_collation_id;    // Collation ID (0 = inherit)
    uint32_t storage_params_oid;      // ❌ TOAST reference (NOT IMPLEMENTED)
    uint64_t created_time;            // Unix timestamp (microseconds)
    uint64_t last_modified_time;      // Unix timestamp (microseconds)
    uint32_t is_valid;                // 1 = valid, 0 = deleted
    uint32_t padding;
};
```

**Size**: ~580 bytes per record

#### In-Memory Structure (TableInfo)

```cpp
struct TableInfo {
    ID table_id;
    ID schema_id;
    std::string table_name;
    uint32_t root_page = 0;
    uint32_t column_count = 0;
    uint64_t row_count = 0;
    TableType table_type = TableType::HEAP;
    bool has_toast = false;           // ❌ NOT IMPLEMENTED
    ID toast_table_id;                // ❌ NOT IMPLEMENTED
    uint16_t tablespace_id = 0;
    uint16_t default_charset = 0;
    uint32_t default_collation_id = 0;
    uint32_t storage_params_oid = 0;  // ❌ NOT USED
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;

    // ONLINE migration (IMPLEMENTED)
    bool migration_in_progress = false;
    ID migration_id;
    uint64_t migration_xid = 0;
    uint16_t migration_target_ts = 0;
    uint8_t migration_phase = 0;
};
```

**Cache**: `std::unordered_map<ID, TableInfo> table_cache_`

---

### 3. Columns Table (`columns_page`)

Stores column definitions for all tables.

#### Disk Structure (ColumnRecord)

```cpp
struct ColumnRecord {
    ID table_id;                      // Parent table ID
    ID column_id;                     // UUIDv7 unique identifier
    char column_name[512];            // UTF-8, max 128 characters
    uint16_t ordinal;                 // Column position in table (0-based)
    uint16_t data_type;               // Type code
    uint32_t type_precision;          // DECIMAL precision, VARCHAR length, VECTOR dimensions
    uint32_t type_scale;              // DECIMAL scale
    uint32_t max_length;              // Legacy field (use type_precision)
    uint8_t nullable;                 // 1 = NULL allowed
    uint8_t has_default;              // 1 = has default value
    uint8_t is_primary_key;           // 1 = part of primary key
    uint8_t is_unique;                // 1 = unique constraint
    uint8_t is_foreign_key;           // 1 = foreign key (NOT ENFORCED)
    uint8_t is_generated;             // 1 = generated column (NOT IMPLEMENTED)
    uint8_t storage_type;             // TOAST storage strategy (NOT IMPLEMENTED)
    uint8_t with_timezone;            // TIMESTAMP: 1 = WITH TIME ZONE
    uint8_t reserved2;
    uint16_t charset;                 // Character set (0 = inherit)
    uint16_t timezone_hint;           // Timezone ID for display (0 = connection default)
    uint32_t collation_id;            // Collation ID (0 = inherit)
    char default_value[128];          // Serialized default (inline)
    uint32_t default_value_oid;       // ❌ TOAST reference for large defaults (NOT IMPLEMENTED)
    uint32_t check_expr_oid;          // ❌ TOAST reference for check expressions (NOT IMPLEMENTED)
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~700 bytes per record

#### In-Memory Structure (ColumnInfo)

```cpp
struct ColumnInfo {
    ID table_id;
    ID column_id;
    std::string column_name;
    uint16_t ordinal = 0;
    uint16_t data_type = 0;
    uint32_t type_precision = 0;
    uint32_t type_scale = 0;
    uint32_t max_length = 0;          // Legacy
    bool nullable = true;
    bool has_default = false;
    bool is_primary_key = false;
    bool is_unique = false;
    bool is_foreign_key = false;      // ❌ NOT ENFORCED
    bool is_generated = false;        // ❌ NOT IMPLEMENTED
    uint8_t storage_type = 0;         // ❌ NOT IMPLEMENTED
    bool with_timezone = false;
    uint16_t charset = 0;
    uint16_t timezone_hint = 0;
    uint32_t collation_id = 0;
    std::string default_value;
    uint32_t default_value_oid = 0;   // ❌ NOT USED
    uint32_t check_expr_oid = 0;      // ❌ NOT USED
    uint64_t created_time = 0;
};
```

**Cache**: `std::unordered_map<ID, std::vector<ColumnInfo>> column_cache_` (keyed by table_id)

---

### 4. Indexes Table (`indexes_page`)

Stores index metadata.

#### Disk Structure (IndexRecord)

```cpp
enum class IndexType : uint8_t {
    BTREE = 0,        // ✅ B-tree index
    HASH = 1,         // ✅ Hash index
    VECTOR = 2,       // ✅ HNSW vector similarity (alias: HNSW)
    FULLTEXT = 3,     // ❌ Full-text search (NOT IMPLEMENTED)
    GIN = 4,          // ⚠️ Generalized Inverted Index (PARTIAL - 60% complete)
    GIST = 5,         // ⚠️ Generalized Search Tree (PARTIAL)
    BRIN = 6,         // ⚠️ Block Range Index (PARTIAL)
    RTREE = 7,        // ✅ R-tree spatial index
    SPGIST = 8,       // ⚠️ Space-Partitioned GiST (PARTIAL)
    BITMAP = 9,       // ✅ Bitmap index
    COLUMNSTORE = 10, // ✅ Columnstore index
    LSM = 11          // ✅ LSM-Tree
};

struct IndexRecord {
    ID index_id;                      // UUIDv7 unique identifier
    ID table_id;                      // Parent table ID
    char index_name[512];             // UTF-8, max 128 characters
    uint32_t root_page;               // Root page of index
    uint8_t index_type;               // IndexType enum
    uint8_t is_unique;                // 1 = unique index
    uint16_t column_count;            // Number of columns
    ID column_ids[16];                // Column IDs (max 16)
    uint32_t index_params_oid;        // ❌ TOAST reference for params (NOT IMPLEMENTED)
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~320 bytes per record

#### In-Memory Structure (IndexInfo)

```cpp
struct IndexInfo {
    ID index_id;
    ID table_id;
    std::string index_name;
    uint32_t root_page = 0;
    uint16_t tablespace_id = 0;
    IndexType index_type = IndexType::BTREE;
    bool is_unique = false;
    std::vector<ID> column_ids;
    uint32_t index_params_oid = 0;    // ❌ NOT USED
    uint64_t created_time = 0;
    uint32_t collation_id = 101;      // Default: utf8_general_ci

    // R-tree parameters
    uint32_t rtree_max_entries = 50;

    // Expression & Filtered Indexes (IMPLEMENTED)
    bool is_expression_index = false;
    bool is_partial_index = false;
    uint32_t expression_oid = 0;      // ❌ TOAST (NOT IMPLEMENTED)
    uint32_t predicate_oid = 0;       // ❌ TOAST (NOT IMPLEMENTED)
    std::vector<std::string> expression_strings;
    std::string predicate_string;
    std::vector<uint8_t> expression_data;   // Inline serialization
    std::vector<uint8_t> predicate_data;    // Inline serialization
};
```

**Cache**: `std::unordered_map<ID, IndexInfo> index_cache_`

---

### 5. Constraints Table (`constraints_page`)

**Status**: ❌ **NOT IMPLEMENTED** (structure defined, no DDL support)

#### Disk Structure (ConstraintRecord)

```cpp
enum class ConstraintType : uint8_t {
    PRIMARY_KEY = 0,  // ❌ Primary key (NOT IMPLEMENTED)
    FOREIGN_KEY = 1,  // ❌ Foreign key (NOT IMPLEMENTED)
    UNIQUE = 2,       // ❌ Unique (NOT IMPLEMENTED)
    CHECK = 3,        // ❌ Check constraint (NOT IMPLEMENTED)
    NOT_NULL = 4,     // ❌ Not null (NOT IMPLEMENTED)
    DEFAULT = 5,      // ❌ Default value (NOT IMPLEMENTED)
    EXCLUSION = 6     // ❌ Exclusion constraint (NOT IMPLEMENTED)
};

struct ConstraintRecord {
    ID constraint_id;
    ID table_id;
    char constraint_name[512];        // UTF-8, max 128 characters
    uint8_t constraint_type;          // ConstraintType enum
    uint8_t is_deferrable;            // Can defer to end of transaction
    uint8_t initially_deferred;       // Initially deferred or immediate
    uint8_t reserved_flags;
    uint16_t column_count;            // Number of columns involved
    ID column_ids[16];                // Columns (max 16)
    ID referenced_table_id;           // For foreign keys
    uint16_t referenced_column_count;
    ID referenced_column_ids[16];     // FK referenced columns
    uint32_t check_expr_oid;          // TOAST reference for check expression
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~660 bytes per record

**Cache**: ❌ NOT IMPLEMENTED

**DDL Support**: None (no ALTER TABLE ADD CONSTRAINT, etc.)

---

### 6. Sequences Table (`sequences_page`)

**Status**: ✅ **FULLY IMPLEMENTED** (November 3, 2025)

#### Disk Structure (SequenceRecord)

```cpp
struct SequenceRecord {
    ID sequence_id;                   // UUIDv7 unique identifier
    ID schema_id;                     // Parent schema ID
    char sequence_name[512];          // UTF-8, max 128 characters
    int64_t current_value;            // Current sequence value
    int64_t increment_by;             // Increment amount
    int64_t min_value;                // Minimum value
    int64_t max_value;                // Maximum value
    int64_t cache_size;               // Cache size for performance
    uint8_t cycle;                    // 1 = cycle on overflow
    uint8_t reserved[7];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~580 bytes per record

#### In-Memory Structure (SequenceInfo + SequenceState)

```cpp
struct SequenceInfo {
    ID sequence_id;
    ID schema_id;
    std::string name;
    int64_t current_value;
    int64_t increment_by;
    int64_t min_value;
    int64_t max_value;
    int64_t start_value;
    int64_t cache_size;
    bool cycle;
    uint64_t created_time;
    uint64_t last_modified_time;
};

// Runtime state (atomic operations)
struct SequenceState {
    ID sequence_id;
    std::string name;
    std::atomic<int64_t> current_value;  // Thread-safe atomic
    int64_t increment_by;
    int64_t min_value;
    int64_t max_value;
    bool cycle;
    std::mutex config_mutex;             // Protect ALTER SEQUENCE
};
```

**Cache**:
- `std::unordered_map<ID, std::shared_ptr<SequenceState>> sequence_cache_`
- `std::unordered_map<std::string, ID> sequence_name_to_id_`

**DDL Support**:
- ✅ CREATE SEQUENCE
- ✅ ALTER SEQUENCE
- ✅ DROP SEQUENCE
- ✅ NEXTVAL(), CURRVAL(), SETVAL()

---

### 7. Views Table (`views_page`)

**Status**: ✅ **FULLY IMPLEMENTED** (November 7, 2025)

#### Disk Structure (ViewRecord)

```cpp
struct ViewRecord {
    ID view_id;                       // UUIDv7 unique identifier
    ID schema_id;                     // Parent schema ID
    char view_name[512];              // UTF-8, max 128 characters
    uint32_t definition_oid;          // ❌ TOAST reference (NOT USED - stored inline)
    uint8_t is_materialized;          // ❌ 1 = materialized view (NOT IMPLEMENTED)
    uint8_t reserved[3];
    uint64_t created_time;
    uint64_t last_refreshed;          // ❌ For materialized views (NOT IMPLEMENTED)
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~552 bytes per record

**Note**: View definitions are currently stored **in-memory only** (not persisted to disk).

#### In-Memory Structure (ViewInfo)

```cpp
struct ViewInfo {
    ID view_id;
    ID schema_id;
    std::string name;
    std::string definition;           // SELECT query text (stored in-memory only)
    bool check_option;                // ❌ Parsed but NOT ENFORCED
    std::vector<std::string> column_names;  // Optional explicit columns
    uint64_t created_time;
    uint64_t last_modified_time;
};
```

**Cache**:
- `std::unordered_map<ID, ViewInfo> view_cache_`
- `std::unordered_map<std::string, ID> view_name_to_id_`

**DDL Support**:
- ✅ CREATE VIEW
- ✅ CREATE OR REPLACE VIEW
- ✅ DROP VIEW [IF EXISTS] [CASCADE | RESTRICT]
- ✅ View expansion in query planner
- ✅ Querying views (SELECT FROM view_name)
- ✅ Recursive view expansion
- ✅ Cycle detection

**Not Implemented**:
- ❌ Updatable views (INSERT/UPDATE/DELETE)
- ❌ Materialized views
- ❌ WITH CHECK OPTION enforcement
- ❌ Persistent storage (views lost on restart)

---

### 8. Triggers Table (`triggers_page`)

**Status**: ❌ **NOT IMPLEMENTED** (structure defined, no DDL support)

#### Disk Structure (TriggerRecord)

```cpp
struct TriggerRecord {
    ID trigger_id;
    ID table_id;
    char trigger_name[512];           // UTF-8, max 128 characters
    uint8_t trigger_timing;           // 0=BEFORE, 1=AFTER, 2=INSTEAD OF
    uint8_t trigger_events;           // Bitmask: 0x01=INSERT, 0x02=UPDATE, 0x04=DELETE
    uint8_t for_each_row;             // 1 = FOR EACH ROW, 0 = FOR EACH STATEMENT
    uint8_t enabled;                  // 1 = enabled
    uint32_t condition_oid;           // TOAST reference for WHEN condition
    uint32_t action_oid;              // TOAST reference for trigger action
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~556 bytes per record

**Cache**: ❌ NOT IMPLEMENTED

**DDL Support**: None (no CREATE TRIGGER, etc.)

---

### 9. Permissions Table (`permissions_page`)

**Status**: ❌ **NOT IMPLEMENTED** (structure defined, no GRANT/REVOKE)

#### Disk Structure (PermissionRecord)

```cpp
struct PermissionRecord {
    ID permission_id;
    ID object_id;                     // Schema, table, view, sequence ID
    char grantee[128];                // User/role granted permission
    uint8_t object_type;              // 0=SCHEMA, 1=TABLE, 2=VIEW, 3=SEQUENCE
    uint8_t reserved[3];
    uint32_t privileges;              // Bitmask of privileges
    uint8_t grant_option;             // 1 = WITH GRANT OPTION
    uint8_t reserved2[3];
    char grantor[128];                // User who granted permission
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~300 bytes per record

**Cache**: ❌ NOT IMPLEMENTED

**DDL Support**: None (no GRANT, REVOKE, etc.)

---

### 10. Statistics Table (`statistics_page`)

**Status**: ⚠️ **PARTIALLY IMPLEMENTED** (storage only, no ANALYZE support)

#### Disk Structure (StatisticsRecord)

```cpp
struct StatisticsRecord {
    ID stats_id;
    ID table_id;
    ID column_id;
    int64_t n_distinct;               // Number of distinct values
    float null_frac;                  // Fraction of null values
    float avg_width;                  // Average width in bytes
    uint32_t most_common_vals_oid;    // ❌ TOAST reference for MCVs (NOT IMPLEMENTED)
    uint32_t histogram_bounds_oid;    // ❌ TOAST reference for histogram (NOT IMPLEMENTED)
    uint64_t last_analyzed;           // Timestamp of last ANALYZE
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~72 bytes per record

**Cache**: ❌ NOT IMPLEMENTED (statistics manager exists but limited functionality)

**DDL Support**: ⚠️ ANALYZE statement parsed but not fully functional

---

### 11. Timezones Table (`timezones_page`)

**Status**: ⚠️ **PARTIALLY IMPLEMENTED** (structure defined, minimal usage)

#### Disk Structure (TimezoneRecord)

```cpp
struct TimezoneRecord {
    uint16_t timezone_id;             // Unique timezone ID
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

### 12. Character Sets Table (`charsets_page`)

**Status**: ⚠️ **PARTIALLY IMPLEMENTED** (pg_charset)

#### Disk Structure (CharsetRecord)

```cpp
struct CharsetRecord {
    uint16_t charset_id;              // Matches CharacterSet enum
    char name[64];                    // e.g., "utf8", "latin1"
    char description[128];            // Human-readable description
    uint8_t min_bytes;                // Minimum bytes per character
    uint8_t max_bytes;                // Maximum bytes per character
    uint8_t variable_width;           // 1 = variable, 0 = fixed
    uint8_t reserved1;
    uint32_t default_collation_id;    // Default collation
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

### 13. Collations Table (`collation_defs_page`)

**Status**: ⚠️ **PARTIALLY IMPLEMENTED** (pg_collation)

#### Disk Structure (CollationRecord)

```cpp
struct CollationRecord {
    uint32_t collation_id;
    char name[128];                   // e.g., "utf8_general_ci"
    uint16_t charset_id;              // Associated character set
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

## In-Memory Structures

### Catalog Manager Caches

All caches are protected by `std::mutex catalog_mutex_`:

```cpp
class CatalogManager {
private:
    // Schema cache
    std::unordered_map<ID, SchemaInfo> schema_cache_;
    uint32_t schema_count_ = 0;

    // Table cache
    std::unordered_map<ID, TableInfo> table_cache_;
    uint32_t table_count_ = 0;

    // Column cache (keyed by table_id)
    std::unordered_map<ID, std::vector<ColumnInfo>> column_cache_;

    // Index cache
    std::unordered_map<ID, IndexInfo> index_cache_;

    // Sequence cache (thread-safe atomic operations)
    std::unordered_map<ID, std::shared_ptr<SequenceState>> sequence_cache_;
    std::unordered_map<std::string, ID> sequence_name_to_id_;
    std::mutex sequence_cache_mutex_;
    std::mutex sequence_name_mutex_;

    // View cache
    std::unordered_map<ID, ViewInfo> view_cache_;
    std::unordered_map<std::string, ID> view_name_to_id_;
    std::mutex view_cache_mutex_;

    // TRUNCATE job tracking
    std::unordered_map<uint64_t, std::shared_ptr<TruncateJob>> truncate_jobs_;
    std::mutex truncate_jobs_mutex_;
    uint64_t next_job_id_ = 1;
};
```

### Cache Load Strategy

On `Database::open()`:
1. Read catalog root page
2. Load all schemas
3. Load all tables
4. Load columns for each table
5. Load all indexes
6. ❌ Sequences NOT loaded (created on-demand)
7. ❌ Views NOT loaded (created on-demand, lost on restart)

---

## Missing/Deferred Features

### Critical Missing DDL (ALPHA Phase 1 Gaps)

1. **Constraints**
   - ❌ ALTER TABLE ADD CONSTRAINT
   - ❌ PRIMARY KEY enforcement (parsed but not enforced)
   - ❌ FOREIGN KEY enforcement (parsed but not enforced)
   - ❌ UNIQUE constraints (beyond indexes)
   - ❌ CHECK constraints (parsed but not enforced)
   - ❌ NOT NULL constraints (parsed but not enforced)
   - ❌ DEFAULT constraints (parsed but not enforced)

2. **Security/Permissions**
   - ❌ GRANT statement (0% complete)
   - ❌ REVOKE statement (0% complete)
   - ❌ Role management (0% complete)
   - ❌ User management (0% complete)
   - ❌ Row-level security (0% complete)

3. **Triggers**
   - ❌ CREATE TRIGGER (0% complete)
   - ❌ DROP TRIGGER (0% complete)
   - ❌ Trigger execution (0% complete)
   - ❌ BEFORE/AFTER/INSTEAD OF (0% complete)

4. **Advanced DDL**
   - ❌ ALTER TABLE (limited - only ADD COLUMN works)
   - ❌ ALTER COLUMN (0% complete)
   - ❌ DROP COLUMN (0% complete)
   - ❌ RENAME TABLE/COLUMN (0% complete)
   - ❌ CREATE TYPE (domain types, composite types)
   - ❌ CREATE SCHEMA (hard-coded schemas only)
   - ❌ DROP SCHEMA (0% complete)

### Partial Implementations

1. **Views**
   - ✅ DDL operations (CREATE/DROP)
   - ✅ Query expansion
   - ❌ Updatable views
   - ❌ Materialized views
   - ❌ WITH CHECK OPTION enforcement
   - ❌ Persistent storage

2. **Indexes**
   - ✅ B-tree (100%)
   - ✅ Hash (100%)
   - ✅ HNSW/Vector (100%)
   - ✅ R-tree (100%)
   - ✅ Bitmap (100%)
   - ✅ Columnstore (100%)
   - ✅ LSM-Tree (100%)
   - ⚠️ GIN (60% - basic operations, missing advanced features)
   - ⚠️ GiST (60%)
   - ⚠️ SP-GiST (60%)
   - ⚠️ BRIN (60%)
   - ❌ Full-text (0%)

3. **Statistics**
   - ⚠️ ANALYZE statement (parsed, limited functionality)
   - ❌ Automatic statistics gathering (0%)
   - ❌ Histogram bounds (0%)
   - ❌ Most common values (MCVs) (0%)
   - ⚠️ Query planner uses basic heuristics only

4. **Character Sets & Collations**
   - ✅ UTF-8 encoding (100%)
   - ⚠️ Collation support (basic, not configurable)
   - ❌ Multiple character sets (0%)
   - ❌ Collation-aware comparisons (partial)

---

## Recommendations

### High Priority Additions

1. **Add `PUBLIC` Schema**
   - Currently missing from default schemas
   - Required for PostgreSQL compatibility
   - User tables should default to PUBLIC, not [sys]

2. **Implement Constraint Enforcement**
   - PRIMARY KEY enforcement
   - FOREIGN KEY enforcement with CASCADE/RESTRICT
   - UNIQUE constraints
   - CHECK constraints
   - NOT NULL constraints
   - DEFAULT values

3. **Add Dependency Tracking**
   - New table: `dependencies_page` or enhance existing tables
   - Track view → table dependencies
   - Track foreign key → table dependencies
   - Enable proper CASCADE behavior

4. **Implement Security Layer**
   - Populate permissions table
   - GRANT/REVOKE statements
   - Role hierarchy
   - Row-level security (future)

### Schema Improvements

1. **Add Dependencies Table**
   ```cpp
   struct DependencyRecord {
       ID dependency_id;
       ID dependent_object_id;      // Object that depends on something
       uint8_t dependent_type;       // VIEW, TABLE, INDEX, etc.
       ID referenced_object_id;      // Object being depended on
       uint8_t referenced_type;      // TABLE, VIEW, SEQUENCE, etc.
       uint8_t dependency_type;      // NORMAL, AUTO, INTERNAL, PIN
       uint64_t created_time;
       uint32_t is_valid;
   };
   ```

2. **Add Procedures/Functions Table**
   ```cpp
   struct ProcedureRecord {
       ID procedure_id;
       ID schema_id;
       char procedure_name[512];
       uint8_t procedure_type;       // FUNCTION, PROCEDURE, TRIGGER_FUNC
       uint32_t definition_oid;      // TOAST reference for procedure body
       uint32_t params_oid;          // TOAST reference for parameters
       uint64_t created_time;
       uint32_t is_valid;
   };
   ```

3. **Enhance View Persistence**
   - Store view definition in `definition_oid` TOAST reference
   - Add view → table dependencies
   - Track column types in view metadata

### Catalog Root Page Updates

Recommended additions to `CatalogRootPage`:

```cpp
uint32_t dependencies_page;     // NEW: Object dependencies
uint32_t procedures_page;        // NEW: Stored procedures/functions
uint32_t roles_page;             // NEW: Role definitions
uint32_t users_page;             // NEW: User accounts
uint32_t tablespaces_page;       // NEW: Tablespace definitions
uint32_t databases_page;         // NEW: Multi-database support (future)
```

### Missing System Views (PostgreSQL Compatibility)

Consider adding in-memory views for metadata queries:

- `information_schema.tables`
- `information_schema.columns`
- `information_schema.views`
- `information_schema.sequences`
- `pg_catalog.pg_class`
- `pg_catalog.pg_attribute`
- `pg_catalog.pg_index`
- `pg_catalog.pg_constraint`

---

## Summary

### Current State

- **Fully Implemented**: Schemas, Tables, Columns, Indexes (most types), Sequences, Views (DDL + query)
- **Partially Implemented**: Statistics, Timezones, Charsets, Collations, some Index types
- **Not Implemented**: Constraints, Permissions, Triggers, most ALTER operations
- **Critical Gap**: No dependency tracking, no constraint enforcement

### Size Estimates (16KB pages)

| Table | Record Size | Records/Page | Notes |
|-------|-------------|--------------|-------|
| Schemas | ~1,144 bytes | ~14 | Low volume (8 default) |
| Tables | ~580 bytes | ~28 | Medium volume |
| Columns | ~700 bytes | ~23 | High volume |
| Indexes | ~320 bytes | ~51 | Medium volume |
| Constraints | ~660 bytes | ~24 | NOT USED |
| Sequences | ~580 bytes | ~28 | Low volume |
| Views | ~552 bytes | ~29 | Low volume (in-memory) |
| Triggers | ~556 bytes | ~29 | NOT USED |
| Permissions | ~300 bytes | ~54 | NOT USED |
| Statistics | ~72 bytes | ~227 | Low usage |

### Recommendations Priority

1. **Critical**: Add dependency tracking
2. **Critical**: Implement constraint enforcement
3. **High**: Add PUBLIC schema by default
4. **High**: Persist view definitions
5. **Medium**: Implement GRANT/REVOKE
6. **Medium**: Complete ALTER TABLE operations
7. **Low**: Trigger support
8. **Low**: Multi-database support

---

**End of Document**

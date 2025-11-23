# DDL Specification: System Catalog Tables

**Last Updated:** November 23, 2025
**Status:** Alpha 1 - 100% Structures, 58% CRUD Operations
**Purpose:** Complete DDL specification for all 40 system catalog tables

---

## Overview

ScratchBird's system catalog consists of 40 metadata tables that track all database objects. These tables use heap storage with TOAST for large fields and are automatically created during database bootstrap.

**Architecture:**
- **Root Catalog Page:** Page 1 (fixed location)
- **Storage:** Heap-based with external TOAST references
- **Encoding:** UTF-8 only, 128 characters max (512 bytes)
- **Thread Safety:** In-memory caches with thread-safe access

---

## Core Catalog Tables

### 1. sb_schemas - Database Schemas

Stores all database schema definitions.

**Disk Structure (SchemaRecord):**

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
    uint32_t acl_oid;                 // TOAST reference for ACL
    uint32_t search_path_oid;         // TOAST reference for search path
    uint64_t created_time;            // Unix timestamp (microseconds)
    uint64_t last_modified_time;      // Unix timestamp (microseconds)
    uint32_t is_valid;                // 1 = valid, 0 = deleted
    uint32_t padding;                 // Alignment
};
```

**Size:** ~1,144 bytes per record

**Default Schemas Created at Bootstrap:**

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

**CRUD Status:** ✅ 100% Complete

---

### 2. sb_tables - Table Definitions

Stores metadata for all tables in the database.

**Table Type Enum:**

```cpp
enum class TableType : uint8_t {
    HEAP = 0,              // Regular heap table
    INDEX = 1,             // Index-organized table
    TEMPORARY = 2,         // Temporary table
    EXTERNAL = 3,          // External table
    MATERIALIZED_VIEW = 4, // Materialized view
    TOAST = 5              // TOAST table
};
```

**Disk Structure (TableRecord):**

```cpp
struct TableRecord {
    ID table_id;                      // UUIDv7 unique identifier
    ID schema_id;                     // Parent schema ID
    char table_name[512];             // UTF-8, max 128 characters
    uint32_t root_page;               // Root page of table heap
    uint32_t column_count;            // Number of columns
    uint64_t row_count;               // Estimated row count
    uint8_t table_type;               // TableType enum
    uint8_t has_toast;                // 1 if table has TOAST
    uint16_t tablespace_id;           // Tablespace ID (0 = default)
    uint16_t default_charset;         // CharacterSet enum (0 = inherit)
    uint16_t reserved1;
    uint32_t default_collation_id;    // Collation ID (0 = inherit)
    uint32_t storage_params_oid;      // TOAST reference for storage params
    uint64_t created_time;            // Unix timestamp (microseconds)
    uint64_t last_modified_time;      // Unix timestamp (microseconds)
    uint32_t is_valid;                // 1 = valid, 0 = deleted
    uint32_t padding;
};
```

**Size:** ~580 bytes per record

**CRUD Status:** ✅ 100% Complete

---

### 3. sb_columns - Column Definitions

Stores all column definitions for every table.

**Disk Structure (ColumnRecord):**

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
    uint8_t is_foreign_key;           // 1 = foreign key
    uint8_t is_generated;             // 1 = generated column
    uint8_t storage_type;             // TOAST storage strategy
    uint8_t with_timezone;            // TIMESTAMP: 1 = WITH TIME ZONE
    uint8_t reserved2;
    uint16_t charset;                 // Character set (0 = inherit)
    uint16_t timezone_hint;           // Timezone ID for display (0 = connection default)
    uint32_t collation_id;            // Collation ID (0 = inherit)
    char default_value[128];          // Serialized default (inline)
    uint32_t default_value_oid;       // TOAST reference for large defaults
    uint32_t check_expr_oid;          // TOAST reference for check expressions
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size:** ~700 bytes per record

**CRUD Status:** ✅ 100% Complete

---

### 4. sb_indexes - Index Metadata

Stores metadata for all indexes.

**Index Type Enum:**

```cpp
enum class IndexType : uint8_t {
    BTREE = 0,        // B-tree index (production-ready)
    HASH = 1,         // Hash index (production-ready)
    HNSW = 2,         // Vector similarity (production-ready)
    FULLTEXT = 3,     // Full-text search (production-ready)
    GIN = 4,          // Generalized Inverted Index (production-ready)
    GIST = 5,         // Generalized Search Tree (production-ready)
    BRIN = 6,         // Block Range Index (production-ready)
    RTREE = 7,        // R-tree spatial index (production-ready)
    SPGIST = 8,       // Space-Partitioned GiST (production-ready)
    BITMAP = 9,       // Bitmap index (production-ready)
    COLUMNSTORE = 10, // Columnstore index (production-ready)
    LSM = 11          // LSM-Tree (production-ready)
};
```

**Disk Structure (IndexRecord):**

```cpp
struct IndexRecord {
    ID index_id;                      // UUIDv7 unique identifier
    ID table_id;                      // Parent table ID
    char index_name[512];             // UTF-8, max 128 characters
    uint32_t root_page;               // Root page of index
    uint8_t index_type;               // IndexType enum
    uint8_t is_unique;                // 1 = unique index
    uint16_t column_count;            // Number of columns
    ID column_ids[16];                // Column IDs (max 16)
    uint32_t index_params_oid;        // TOAST reference for params
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size:** ~320 bytes per record

**CRUD Status:** ✅ 100% Complete

**Index Features:**
- ✅ Expression indexes (indexed expressions, not just columns)
- ✅ Partial indexes (with WHERE clause filtering)
- ✅ Unique indexes with uniqueness enforcement
- ✅ Multi-column indexes (up to 16 columns)
- ✅ MGA-compliant with stable TIDs

---

### 5. sb_views - View Definitions

Stores view and materialized view definitions.

**Disk Structure (ViewRecord):**

```cpp
struct ViewRecord {
    ID view_id;                       // UUIDv7 unique identifier
    ID schema_id;                     // Parent schema ID
    char view_name[512];              // UTF-8, max 128 characters
    uint32_t definition_oid;          // TOAST reference for definition
    uint8_t is_materialized;          // 1 = materialized view
    uint8_t reserved[3];
    uint64_t created_time;
    uint64_t last_refreshed;          // For materialized views
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size:** ~552 bytes per record

**CRUD Status:** ✅ 100% Complete

**View Features:**
- ✅ CREATE VIEW
- ✅ CREATE OR REPLACE VIEW
- ✅ DROP VIEW [IF EXISTS] [CASCADE | RESTRICT]
- ✅ View expansion in query planner
- ✅ Recursive view expansion
- ✅ Cycle detection
- ⏳ Materialized views (80% - physical materialization in progress)
- ⏳ Updatable views (planned)

---

### 6. sb_sequences - Sequence Objects

Stores sequence definitions for auto-incrementing values.

**Disk Structure (SequenceRecord):**

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

**Size:** ~580 bytes per record

**CRUD Status:** ✅ 100% Complete

**Sequence Operations:**
- ✅ CREATE SEQUENCE
- ✅ ALTER SEQUENCE
- ✅ DROP SEQUENCE
- ✅ NEXTVAL(sequence) - Atomic increment
- ✅ CURRVAL(sequence) - Get current value
- ✅ SETVAL(sequence, value) - Set value

---

### 7. sb_triggers - Trigger Definitions

Stores trigger metadata.

**Disk Structure (TriggerRecord):**

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

**Size:** ~556 bytes per record

**CRUD Status:** ✅ 100% Complete

**Trigger Features:**
- ✅ CREATE TRIGGER (BEFORE/AFTER)
- ✅ DROP TRIGGER
- ✅ Trigger firing during DML operations
- ✅ FOR EACH ROW support
- ⏳ FOR EACH STATEMENT (future)
- ⏳ INSTEAD OF triggers (future)

---

## Constraint & Domain Tables

### 8. sb_constraints - Constraint Definitions

**Constraint Type Enum:**

```cpp
enum class ConstraintType : uint8_t {
    PRIMARY_KEY = 0,  // Primary key
    FOREIGN_KEY = 1,  // Foreign key
    UNIQUE = 2,       // Unique constraint
    CHECK = 3,        // Check constraint
    NOT_NULL = 4,     // Not null
    DEFAULT = 5,      // Default value
    EXCLUSION = 6     // Exclusion constraint
};
```

**Disk Structure (ConstraintRecord):**

```cpp
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

**Size:** ~660 bytes per record

**CRUD Status:** ✅ 100% Complete

**Constraint Features:**
- ✅ PRIMARY KEY constraints
- ✅ UNIQUE constraints
- ✅ NOT NULL constraints
- ✅ CHECK constraints
- ✅ DEFAULT values
- ✅ Deferred constraint checking

---

### 9. sb_foreign_keys - Foreign Key Relationships

**Foreign Key Action Enum:**

```cpp
enum class FKAction : uint8_t {
    NO_ACTION = 0,    // Error if references exist (default)
    RESTRICT = 1,     // Error immediately
    CASCADE = 2,      // Delete/update child rows
    SET_NULL = 3,     // Set FK columns to NULL
    SET_DEFAULT = 4   // Set FK columns to DEFAULT
};
```

**Foreign Key Match Type Enum:**

```cpp
enum class FKMatchType : uint8_t {
    SIMPLE = 0,       // Default: NULL in any column = no match required
    FULL = 1,         // All columns NULL or all non-NULL
    PARTIAL = 2       // Reserved for future
};
```

**Disk Structure (ForeignKeyRecord):**

```cpp
struct ForeignKeyRecord {
    ID fk_id;                         // UUIDv7 unique identifier
    ID table_id;                      // Parent table ID
    char fk_name[512];                // UTF-8, max 128 characters
    ID referenced_table_id;           // Referenced table ID
    uint16_t column_count;            // Number of FK columns
    ID column_ids[16];                // FK columns (max 16)
    ID referenced_column_ids[16];     // Referenced columns
    uint8_t on_delete_action;         // FKAction for DELETE
    uint8_t on_update_action;         // FKAction for UPDATE
    uint8_t match_type;               // FKMatchType
    uint8_t is_deferrable;            // Can defer to end of transaction
    uint8_t initially_deferred;       // Initially deferred or immediate
    uint8_t reserved[3];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete (Phase C - Composite FK support)

**Foreign Key Features:**
- ✅ Composite foreign keys (multiple columns)
- ✅ CASCADE actions (DELETE/UPDATE)
- ✅ SET NULL / SET DEFAULT actions
- ✅ MATCH SIMPLE / FULL
- ✅ Deferrable constraints

---

### 10. sb_domains - User-Defined Domains

**Disk Structure (DomainRecord):**

```cpp
struct DomainRecord {
    ID domain_id;                     // UUIDv7 unique identifier
    ID schema_id;                     // Parent schema ID
    char domain_name[512];            // UTF-8, max 128 characters
    uint16_t base_type;               // Base data type
    uint32_t type_precision;          // Type precision
    uint32_t type_scale;              // Type scale
    uint8_t nullable;                 // 1 = NULL allowed
    uint8_t has_default;              // 1 = has default value
    uint8_t reserved[2];
    char default_value[128];          // Serialized default (inline)
    uint32_t default_value_oid;       // TOAST reference for large defaults
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

---

### 11. sb_domain_constraints - Domain-Level Constraints

**Disk Structure (DomainConstraintRecord):**

```cpp
struct DomainConstraintRecord {
    ID constraint_id;                 // UUIDv7 unique identifier
    ID domain_id;                     // Parent domain ID
    char constraint_name[512];        // UTF-8, max 128 characters
    uint8_t constraint_type;          // CHECK or NOT NULL
    uint8_t reserved[3];
    uint32_t check_expr_oid;          // TOAST reference for check expression
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

---

## Security Tables

### 12. sb_users - User Accounts

**Disk Structure (UserRecord):**

```cpp
struct UserRecord {
    ID user_id;                       // UUIDv7 unique identifier
    char username[512];               // UTF-8, max 128 characters
    char password_hash[512];          // bcrypt/scrypt hash
    uint8_t is_superuser;             // 1 = superuser (bypass all checks)
    uint8_t is_active;                // 1 = account active
    uint8_t reserved[2];
    uint32_t metadata_oid;            // TOAST reference for JSON metadata
    uint64_t created_time;
    uint64_t last_login_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

---

### 13. sb_roles - Role Definitions

**Disk Structure (RoleRecord):**

```cpp
struct RoleRecord {
    ID role_id;                       // UUIDv7 unique identifier
    char role_name[512];              // UTF-8, max 128 characters
    char description[512];            // Role description
    uint8_t is_builtin;               // 1 = system role (cannot delete)
    uint8_t reserved[3];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

---

### 14. sb_groups - User Groups

**Disk Structure (GroupRecord):**

```cpp
struct GroupRecord {
    ID group_id;                      // UUIDv7 unique identifier
    char group_name[512];             // UTF-8, max 128 characters
    uint8_t group_type;               // LOCAL, LDAP, ACTIVE_DIRECTORY
    uint8_t reserved[3];
    char external_id[256];            // External directory ID
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

---

### 15. sb_role_members - Role Membership

**Disk Structure (RoleMembershipRecord):**

```cpp
struct RoleMembershipRecord {
    ID membership_id;                 // UUIDv7 unique identifier
    ID role_id;                       // Role ID
    ID member_id;                     // User or role ID
    uint8_t member_type;              // USER or ROLE
    uint8_t with_admin_option;        // Can grant this role to others
    uint8_t reserved[2];
    uint64_t granted_time;
    ID granted_by;                    // User who granted
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

---

### 16. sb_permissions - Object-Level Permissions

**Permission Object Type Enum:**

```cpp
enum class PermissionObjectType : uint8_t {
    SCHEMA = 0,
    TABLE = 1,
    VIEW = 2,
    SEQUENCE = 3,
    PROCEDURE = 4,
    FUNCTION = 5,
    DOMAIN = 6,
    DATABASE = 7
};
```

**Privilege Bitmask:**

| Privilege | Bitmask | Applies To |
|-----------|---------|-----------|
| SELECT | 0x00000001 | Tables, Views, Sequences |
| INSERT | 0x00000002 | Tables, Views |
| UPDATE | 0x00000004 | Tables, Views |
| DELETE | 0x00000008 | Tables, Views |
| TRUNCATE | 0x00000010 | Tables |
| REFERENCES | 0x00000020 | Tables (Foreign Keys) |
| TRIGGER | 0x00000040 | Tables |
| CREATE | 0x00000080 | Schemas, Databases |
| USAGE | 0x00000100 | Sequences, Domains, Types |
| EXECUTE | 0x00000200 | Procedures, Functions |

**Disk Structure (PermissionRecord):**

```cpp
struct PermissionRecord {
    ID permission_id;
    ID object_id;                     // Schema, table, view, sequence ID
    char grantee[128];                // User/role granted permission
    uint8_t object_type;              // PermissionObjectType enum
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

**Size:** ~300 bytes per record

**CRUD Status:** ✅ 100% Complete

---

### 17. sb_column_permissions - Column-Level Permissions

**Disk Structure (ColumnPermissionRecord):**

```cpp
struct ColumnPermissionRecord {
    ID permission_id;                 // UUIDv7 unique identifier
    ID table_id;                      // Parent table ID
    ID column_id;                     // Column ID
    char grantee[128];                // User/role granted permission
    uint32_t privileges;              // Bitmask: SELECT, UPDATE only
    uint8_t grant_option;             // 1 = WITH GRANT OPTION
    uint8_t reserved[3];
    char grantor[128];                // User who granted permission
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete (Phase 3.2)

---

### 18. sb_policies - Row-Level Security Policies

**Disk Structure (PolicyRecord):**

```cpp
struct PolicyRecord {
    ID policy_id;                     // UUIDv7 unique identifier
    ID table_id;                      // Parent table ID
    char policy_name[512];            // UTF-8, max 128 characters
    uint8_t policy_type;              // ALL, SELECT, INSERT, UPDATE, DELETE
    uint8_t is_permissive;            // 1 = PERMISSIVE, 0 = RESTRICTIVE
    uint8_t reserved[2];
    uint32_t using_expr_oid;          // TOAST reference for USING clause
    uint32_t with_check_expr_oid;     // TOAST reference for WITH CHECK clause
    char applicable_roles[512];       // Comma-separated role list or 'PUBLIC'
    uint8_t enabled;                  // 1 = enabled
    uint8_t reserved2[3];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Structure (Phase 3.4)

**RLS Features:**
- ✅ CREATE POLICY
- ✅ DROP POLICY
- ✅ ALTER TABLE ... ENABLE ROW LEVEL SECURITY
- ✅ USING clause (access control)
- ✅ WITH CHECK clause (write constraint)
- ✅ Role-based applicability

---

## Metadata Tables

### 19. sb_dependencies - Object Dependencies

**Disk Structure (DependencyRecord):**

```cpp
struct DependencyRecord {
    ID dependency_id;                 // UUIDv7 unique identifier
    ID object_id;                     // Dependent object ID
    uint8_t object_type;              // Object type enum
    ID referenced_object_id;          // Referenced object ID
    uint8_t referenced_object_type;   // Referenced type enum
    uint8_t dependency_type;          // NORMAL, AUTO, INTERNAL
    uint8_t reserved;
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

---

### 20. sb_comments - Object Documentation

**Disk Structure (CommentRecord):**

```cpp
struct CommentRecord {
    ID comment_id;                    // UUIDv7 unique identifier
    ID object_id;                     // Object ID
    uint8_t object_type;              // Object type enum
    uint8_t reserved[3];
    uint32_t comment_oid;             // TOAST reference for comment text
    char author[128];                 // User who created comment
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

---

### 21. sb_tablespaces - Tablespace Definitions

**Disk Structure (TablespaceRecord):**

```cpp
struct TablespaceRecord {
    uint16_t tablespace_id;           // Unique tablespace ID
    char name[512];                   // UTF-8, max 128 characters
    char location[1024];              // File system path
    uint8_t is_default;               // 1 = default tablespace
    uint8_t is_temp;                  // 1 = temporary tablespace
    uint8_t reserved[2];
    char owner[128];                  // Owner user
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

**Tablespace Operations:**
- ✅ CREATE TABLESPACE
- ✅ ALTER TABLESPACE
- ✅ DROP TABLESPACE
- ✅ ALTER TABLE SET TABLESPACE (with ONLINE migration)

---

### 22. sb_timezones - Timezone Mappings

**Disk Structure (TimezoneRecord):**

```cpp
struct TimezoneRecord {
    uint16_t timezone_id;             // Unique timezone ID
    char name[64];                    // e.g., "America/New_York"
    char abbreviation[16];            // e.g., "EST", "PST"
    int32_t std_offset_minutes;       // Standard offset from GMT
    uint8_t observes_dst;             // 1 = observes DST
    uint8_t reserved1;
    uint16_t reserved2;
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

**Size:** ~136 bytes per record

**CRUD Status:** ✅ 100% Complete

---

### 23. sb_charsets - Character Set Definitions

**Disk Structure (CharsetRecord):**

```cpp
struct CharsetRecord {
    uint16_t charset_id;              // Matches CharacterSet enum
    char name[64];                    // e.g., "utf8", "latin1"
    char description[128];            // Human-readable description
    uint8_t min_bytes;                // Minimum bytes per character
    uint8_t max_bytes;                // Maximum bytes per character
    uint8_t reserved[2];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

**Default Charset:** UTF-8 only in Alpha 1

---

### 24. sb_collations - Collation Definitions

**Disk Structure (CollationRecord):**

```cpp
struct CollationRecord {
    uint32_t collation_id;            // Unique collation ID
    char name[128];                   // e.g., "utf8_general_ci"
    uint16_t charset_id;              // Parent character set
    uint8_t is_case_sensitive;        // 1 = case sensitive
    uint8_t is_accent_sensitive;      // 1 = accent sensitive
    uint8_t reserved[4];
    char locale[64];                  // ICU locale string
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

---

### 25. sb_statistics - Table/Index Statistics

**Disk Structure (StatisticsRecord):**

```cpp
struct StatisticsRecord {
    ID stats_id;
    ID table_id;
    ID column_id;
    int64_t n_distinct;               // Number of distinct values
    float null_frac;                  // Fraction of null values
    float avg_width;                  // Average width in bytes
    uint32_t most_common_vals_oid;    // TOAST reference for MCVs
    uint32_t histogram_bounds_oid;    // TOAST reference for histogram
    uint64_t last_analyzed;           // Timestamp of last ANALYZE
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size:** ~72 bytes per record

**CRUD Status:** ✅ 100% Complete (Phase 1.1.2)

**Statistics Operations:**
- ✅ ANALYZE statement
- ✅ Statistics collection
- ✅ Query optimizer integration

---

## Procedural Language Tables

### 26. sb_procedures - Stored Procedures

**Disk Structure (ProcedureRecord):**

```cpp
struct ProcedureRecord {
    ID procedure_id;                  // UUIDv7 unique identifier
    ID schema_id;                     // Parent schema ID
    char procedure_name[512];         // UTF-8, max 128 characters
    char language[64];                // "plpgsql", "sql", etc.
    uint32_t source_oid;              // TOAST reference for source code
    uint32_t bytecode_oid;            // TOAST reference for SBLR bytecode
    uint8_t security_definer;         // 1 = DEFINER, 0 = INVOKER
    uint8_t reserved[3];
    char owner[128];                  // Owner user
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete (Phase 2.10.2)

---

### 27. sb_functions - Stored Functions

**Disk Structure (FunctionRecord):**

```cpp
struct FunctionRecord {
    ID function_id;                   // UUIDv7 unique identifier
    ID schema_id;                     // Parent schema ID
    char function_name[512];          // UTF-8, max 128 characters
    uint16_t return_type;             // Return data type
    uint16_t reserved1;
    char language[64];                // "plpgsql", "sql", etc.
    uint32_t source_oid;              // TOAST reference for source code
    uint32_t bytecode_oid;            // TOAST reference for SBLR bytecode
    uint8_t security_definer;         // 1 = DEFINER, 0 = INVOKER
    uint8_t is_strict;                // 1 = RETURNS NULL ON NULL INPUT
    uint8_t is_volatile;              // VOLATILE, STABLE, or IMMUTABLE
    uint8_t reserved2;
    char owner[128];                  // Owner user
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete (Phase 2.10.2)

---

### 28. sb_procedure_params - Procedure/Function Parameters

**Disk Structure (ProcedureParameterRecord):**

```cpp
struct ProcedureParameterRecord {
    ID param_id;                      // UUIDv7 unique identifier
    ID procedure_id;                  // Parent procedure/function ID
    char param_name[256];             // Parameter name
    uint16_t ordinal;                 // Parameter position (0-based)
    uint8_t param_mode;               // IN, OUT, INOUT
    uint16_t data_type;               // Data type
    uint32_t type_precision;          // Type precision
    uint32_t type_scale;              // Type scale
    uint8_t has_default;              // 1 = has default value
    uint8_t reserved[3];
    char default_value[128];          // Serialized default (inline)
    uint32_t default_value_oid;       // TOAST reference for large defaults
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete (Phase 2.10.2)

---

### 29. sb_packages - PL/PSQL Packages

**Disk Structure (PackageRecord):**

```cpp
struct PackageRecord {
    ID package_id;                    // UUIDv7 unique identifier
    ID schema_id;                     // Parent schema ID
    char package_name[512];           // UTF-8, max 128 characters
    uint32_t spec_source_oid;         // TOAST reference for package spec
    uint32_t body_source_oid;         // TOAST reference for package body
    char owner[128];                  // Owner user
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Structure (Phase 7 - Future)

---

### 30. sb_udrs - User-Defined Resources

**Disk Structure (UDRRecord):**

```cpp
struct UDRRecord {
    ID udr_id;                        // UUIDv7 unique identifier
    ID schema_id;                     // Parent schema ID
    char udr_name[512];               // UTF-8, max 128 characters
    uint32_t metadata_oid;            // TOAST reference for metadata
    char owner[128];                  // Owner user
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Structure (Phase 7 - Future)

---

## Advanced Feature Tables

### 31. sb_sessions - Active Database Sessions

**Disk Structure (SessionRecord):**

```cpp
struct SessionRecord {
    ID session_id;                    // UUIDv7 unique identifier
    ID user_id;                       // Connected user ID
    ID database_id;                   // Current database ID
    uint64_t connected_at;            // Connection timestamp
    uint64_t last_activity;           // Last activity timestamp
    char client_address[64];          // Client IP address
    uint16_t client_port;             // Client port
    uint8_t protocol_version;         // Wire protocol version
    uint8_t reserved[5];
    uint32_t session_params_oid;      // TOAST reference for session params
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

---

### 32. sb_emulation_types - PostgreSQL/MySQL Type Mappings

**Disk Structure (EmulationTypeRecord):**

```cpp
struct EmulationTypeRecord {
    uint16_t emulation_type_id;       // Unique emulation type ID
    char external_type_name[128];     // External database type name
    uint16_t scratchbird_type;        // Mapped ScratchBird type
    uint8_t dialect;                  // POSTGRESQL, MYSQL, MSSQL, FIREBIRD
    uint8_t reserved[3];
    uint32_t mapping_params_oid;      // TOAST reference for conversion params
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Structure (Phase 6 - Future)

---

### 33. sb_emulation_servers - Remote Database Definitions

**Disk Structure (EmulationServerRecord):**

```cpp
struct EmulationServerRecord {
    ID server_id;                     // UUIDv7 unique identifier
    char server_name[256];            // Server name
    uint8_t server_type;              // POSTGRESQL, MYSQL, MSSQL, FIREBIRD
    uint8_t reserved[3];
    char host[256];                   // Hostname or IP
    uint16_t port;                    // Port number
    uint16_t reserved2;
    char username[128];               // Authentication username
    uint32_t password_oid;            // TOAST reference for encrypted password
    uint32_t options_oid;             // TOAST reference for connection options
    uint64_t created_time;
    uint64_t last_connection_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Structure (Phase 6 - Future)

---

### 34. sb_emulated_databases - Logical Remote Databases

**Disk Structure (EmulatedDatabaseRecord):**

```cpp
struct EmulatedDatabaseRecord {
    ID database_id;                   // UUIDv7 unique identifier
    ID server_id;                     // Parent emulation server ID
    char local_name[256];             // Local alias name
    char remote_name[256];            // Remote database name
    uint8_t auto_sync;                // 1 = auto-sync schema
    uint8_t reserved[3];
    uint64_t last_sync_time;          // Last schema sync timestamp
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Structure (Phase 6 - Future)

---

## Additional System Tables

### 35. sb_databases - Database Catalog

**Disk Structure:**

```cpp
struct DatabaseRecord {
    ID database_id;                   // UUIDv7 unique identifier
    char database_name[512];          // UTF-8, max 128 characters
    char owner[128];                  // Owner user
    uint16_t default_tablespace_id;   // Default tablespace
    uint16_t default_charset;         // CharacterSet enum
    uint32_t default_collation_id;    // Collation ID
    char encoding[32];                // Character encoding
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**CRUD Status:** ✅ 100% Complete

---

### 36-40. Additional Reserved Tables

**Reserved for future phases:**
- sb_partitions - Table partition metadata
- sb_foreign_tables - Foreign Data Wrapper tables
- sb_extensions - Extension metadata
- sb_event_triggers - Event trigger definitions
- sb_replication_slots - Replication slot tracking

---

## Catalog Root Page Structure

**Page ID:** 1 (fixed location)
**Page Type:** `PAGE_TYPE_CATALOG`

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
    uint32_t sequences_page;          // Page containing sequences table
    uint32_t views_page;              // Page containing views table
    uint32_t triggers_page;           // Page containing triggers table
    uint32_t permissions_page;        // Page containing permissions table
    uint32_t statistics_page;         // Page containing statistics table
    uint32_t collations_page;         // Legacy collations page
    uint32_t timezones_page;          // Page containing timezones table
    uint32_t charsets_page;           // Page containing character sets
    uint32_t collation_defs_page;     // Page containing collations
    uint8_t reserved[4024];           // Padding to 16KB
};
```

**Size:** Exactly 16KB (default page size)

---

## Implementation Notes

### Thread Safety

All catalog operations use:
- In-memory caches with read/write locks
- Atomic operations for counters
- Transaction-safe MVCC for catalog updates

### TOAST Integration

Large fields use external TOAST storage:
- View definitions
- Procedure/function source code
- ACLs and permissions
- Expression trees
- Comments and documentation

### Character Encoding

All identifiers:
- UTF-8 only (Alpha 1)
- 128 characters max
- 512 bytes storage (128 × 4 bytes max per UTF-8 char)
- Character boundary validation

### File Locations

**Header:** `/home/user/ScratchBird/include/scratchbird/core/catalog_manager.h`
**Implementation:** `/home/user/ScratchBird/src/core/catalog_manager.cpp`
**Specification:** `/home/user/ScratchBird/docs/specifications/SYSTEM_CATALOG_STRUCTURE.md`

---

## Summary

- **Total System Tables:** 40
- **Structure Completion:** 100%
- **CRUD Operations:** 58% complete (23/40 tables)
- **Default Character Encoding:** UTF-8 only
- **Identifier Length:** 128 characters (512 bytes)
- **Bootstrap Schemas:** 8 (root, sys, sec, agents, app, remote, users, roles)
- **Page Size:** 16KB default (8KB, 32KB, 64KB supported)

**Next Steps:**
- Complete remaining CRUD operations for advanced feature tables
- Implement persistent view storage
- Complete materialized view physical materialization
- Implement updatable views

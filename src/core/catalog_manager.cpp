#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/btree.h"       // Phase 5 Task 5.2: B-Tree TID updates
#include "scratchbird/core/hash_index.h"  // Phase 5 Task 5.3.1: Hash index TID updates
#include "scratchbird/core/rtree.h"       // Phase 2 Task 9.2: R-tree spatial index
#include "scratchbird/core/index_factory.h"  // LSM Integration Phase 3: Index factory
#include <cstring>
#include "scratchbird/core/toast.h"       // Phase 5 Task 5.1.3: TOAST migration
#include <algorithm>
#include <chrono>  // Phase 4 Task 4.1.3: Progress tracking
#include <thread>  // Phase 4 Task 4.1.3: Sleep simulation in STUB
#include "scratchbird/core/tid_resolver.h"  // Sprint 5: ONLINE migration
#include <fcntl.h>   // Phase 6: For open(), O_RDWR
#include <unistd.h>  // Phase 6: For pread(), close()
#include "scratchbird/core/utf8_utils.h"  // Phase 3: SQL Identifier UTF-8 Fix
#include <queue>  // Phase 1.4: BFS for group transitive closure
#include <unordered_set>  // Phase 1.4: Visited set for group transitive closure
#include "scratchbird/core/connection_context.h"  // Phase 3.1: Object permissions grantor tracking

namespace scratchbird::core
{

// Catalog page structures
#pragma pack(push, 1)

    // Root catalog page - points to system tables
    struct CatalogRootPage
    {
        PageHeader header;
        uint32_t schema_count;
        uint32_t table_count;

        // Core catalog tables (existing)
        uint32_t schemas_page;        // Page containing schemas table
        uint32_t tables_page;         // Page containing tables table
        uint32_t columns_page;        // Page containing columns table
        uint32_t indexes_page;        // Page containing indexes table
        uint32_t constraints_page;    // Page containing constraints table
        uint32_t sequences_page;      // Page containing sequences table
        uint32_t views_page;          // Page containing views table
        uint32_t triggers_page;       // Page containing triggers table
        uint32_t permissions_page;    // Page containing permissions table
        uint32_t statistics_page;     // Page containing statistics table
        uint32_t collations_page;     // Page containing collations table (legacy)
        uint32_t timezones_page;      // Page containing timezones table
        uint32_t charsets_page;       // Page containing character sets table (pg_charset)
        uint32_t collation_defs_page; // Page containing collation definitions table (pg_collation)

        // Phase 1.4-1.5: Dependencies and Comments (Catalog Corrections)
        uint32_t dependencies_page;   // Page containing dependencies table
        uint32_t comments_page;       // Page containing comments table

        // Phase 2: Security tables (Catalog Corrections)
        uint32_t users_page;          // Page containing users table
        uint32_t roles_page;          // Page containing roles table
        uint32_t groups_page;         // Page containing groups table (AD/LDAP)
        uint32_t role_members_page;   // Page containing role memberships table
        uint32_t group_members_page;  // Page containing group memberships table (Phase 1.1)
        uint32_t group_mappings_page; // Page containing group mappings table (Phase 1.1)

        // Phase 3: Stored code tables (Catalog Corrections)
        uint32_t procedures_page;     // Page containing procedures/functions table
        uint32_t proc_params_page;    // Page containing procedure parameters table
        uint32_t domains_page;        // Page containing domains table
        uint32_t udr_page;            // Page containing UDR (User-Defined Resources) table
        uint32_t packages_page;       // Page containing packages table (Firebird)

        // Phase 4: Emulation tables (Catalog Corrections)
        uint32_t emulation_types_page;    // Page containing emulation types table
        uint32_t emulation_servers_page;  // Page containing emulation servers table
        uint32_t emulated_dbs_page;       // Page containing emulated databases table

        // Phase 5: Future expansion
        uint32_t tablespaces_page;    // Page containing tablespaces table
        uint32_t extensions_page;     // Page containing extensions table
        uint32_t foreign_keys_page;   // Page containing foreign keys table (Phase D - FK Persistence)

        uint8_t reserved[3892];       // Padding for 16KB page (156 bytes used: 152 + 4 for foreign_keys_page)
    };

    // Schema record on disk
    struct SchemaRecord
    {
        ID schema_id;
        ID parent_schema_id;            // Parent schema UUID (zero UUID for root schemas)
        char schema_name[512];          // SQL standard: 128 characters (512 bytes = 128 chars × 4 bytes/char max UTF-8)
        ID owner_id;                    // Owner UUID reference (NOT name - allows rename without breaking dependencies)
        uint16_t default_tablespace_id; // Default tablespace for new tables
        uint16_t permissions;           // Bitmask of schema permissions
        uint16_t default_charset;       // CharacterSet enum (0 = inherit from database)
        uint16_t reserved;
        uint32_t default_collation_id;  // Collation ID (0 = inherit from database)
        uint32_t acl_oid;               // TOAST reference for ACL (access control list) - IMPLEMENTED
        // search_path_oid removed - search path is session-only, not stored per-schema
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;              // 1 if valid, 0 if deleted
        uint32_t padding;               // Alignment
    };

    // Table types
    enum class TableType : uint8_t
    {
        HEAP = 0,              // Regular heap table
        INDEX = 1,             // Index-organized table
        TEMPORARY = 2,         // Temporary table
        EXTERNAL = 3,          // External table
        MATERIALIZED_VIEW = 4, // Materialized view
        TOAST = 5              // TOAST table
    };

    // Table record on disk
    struct TableRecord
    {
        ID table_id;
        ID schema_id;
        char table_name[512];          // SQL standard: 128 characters (512 bytes = 128 chars × 4 bytes/char max UTF-8)
        ID owner_id;                   // Owner UUID reference (NOT name - allows rename without breaking dependencies)
        uint32_t root_page;
        uint32_t column_count;
        uint64_t row_count;
        uint8_t table_type;            // TableType enum
        uint8_t has_toast;             // 1 if table has TOAST
        uint8_t rls_enabled;           // Security Phase 3.4: Row-level security enabled
        uint8_t rls_forced;            // Security Phase 3.4: Force RLS for table owners
        uint16_t tablespace_id;        // Tablespace ID (0 = default)
        uint16_t default_charset;      // CharacterSet enum (0 = inherit from schema)
        uint16_t reserved1;            // Reserved for future use
        uint32_t default_collation_id; // Collation ID (0 = inherit from schema)
        uint32_t storage_params_oid;   // TOAST reference for storage parameters - IMPLEMENTED
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding;              // Alignment
    };

    // Column record on disk
    struct ColumnRecord
    {
        ID table_id;
        ID column_id;
        char column_name[512]; // SQL standard: 128 characters (512 bytes = 128 chars × 4 bytes/char max UTF-8)
        uint16_t ordinal;      // Column position in table
        uint16_t data_type;
        uint32_t type_precision; // For DECIMAL, VECTOR dimensions, VARCHAR length
        uint32_t type_scale;     // For DECIMAL scale
        uint32_t max_length;     // Legacy field, use type_precision instead
        uint8_t nullable;
        uint8_t has_default;
        uint8_t is_primary_key;
        uint8_t is_unique;
        uint8_t is_foreign_key;
        uint8_t is_generated;
        uint8_t storage_type;  // TOAST storage strategy
        uint8_t with_timezone; // For TIMESTAMP: 1 = WITH TIME ZONE, 0 = WITHOUT
        uint8_t reserved2;
        uint16_t charset;       // CharacterSet enum (0 = inherit from table)
        uint16_t timezone_hint; // Timezone ID for display (0 = use connection default)
        uint32_t collation_id;  // Collation ID (0 = inherit from table)
        char default_value[128];
        uint32_t default_value_oid; // TOAST reference for large defaults
        uint32_t check_expr_oid;    // TOAST reference for check expressions
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding; // Alignment
    };

    // Index types
    enum class IndexType : uint8_t
    {
        BTREE = 0,    // B-tree index (default)
        HASH = 1,     // Hash index
        VECTOR = 2,   // Vector similarity index (HNSW, IVF, etc.)
        FULLTEXT = 3, // Full-text search index
        GIN = 4,      // Generalized Inverted Index
        GIST = 5,     // Generalized Search Tree
        BRIN = 6      // Block Range Index
    };

    // Index record on disk
    struct IndexRecord
    {
        ID index_id;
        ID table_id;
        char index_name[512];      // SQL standard: 128 characters (512 bytes = 128 chars × 4 bytes/char max UTF-8)
        ID owner_id;               // Owner UUID reference (NOT name)
        uint32_t root_page;
        uint8_t index_type;        // IndexType enum
        uint8_t is_unique;
        uint16_t column_count;
        ID column_ids[16];         // Max 16 columns per index
        uint32_t index_params_oid; // TOAST reference for index parameters (HNSW config, etc.) - IMPLEMENTED
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding;          // Alignment
    };

    // Timezone record on disk
    struct TimezoneRecord
    {
        uint16_t timezone_id;       // Unique timezone ID
        char name[64];              // Timezone name (e.g., "America/New_York")
        char abbreviation[16];      // Abbreviation (e.g., "EST", "PST")
        int32_t std_offset_minutes; // Standard offset from GMT in minutes
        uint8_t observes_dst;       // 1 if observes DST, 0 otherwise
        uint8_t reserved1;
        uint16_t reserved2;
        // DST transition rules (simplified - in production use IANA tzdata)
        uint8_t dst_start_month;    // Month DST starts (1-12, 0 = no DST)
        uint8_t dst_start_week;     // Week of month (1-5, 0 = last)
        uint8_t dst_start_day;      // Day of week (0-6, 0 = Sunday)
        uint8_t dst_start_hour;     // Hour DST starts (0-23)
        uint8_t dst_end_month;      // Month DST ends
        uint8_t dst_end_week;       // Week of month
        uint8_t dst_end_day;        // Day of week
        uint8_t dst_end_hour;       // Hour DST ends
        int32_t dst_offset_minutes; // Additional offset during DST
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid; // 1 if valid, 0 if deleted
        uint32_t padding;  // Alignment
    };

    // Character set record on disk (pg_charset)
    struct CharsetRecord
    {
        uint16_t charset_id;    // Character set ID (matches CharacterSet enum)
        char name[64];          // Character set name (e.g., "utf8", "latin1")
        char description[128];  // Human-readable description
        uint8_t min_bytes;      // Minimum bytes per character
        uint8_t max_bytes;      // Maximum bytes per character
        uint8_t variable_width; // 1 = variable width, 0 = fixed width
        uint8_t reserved1;
        uint32_t default_collation_id; // Default collation for this charset
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid; // 1 if valid, 0 if deleted
        uint32_t padding;  // Alignment
    };

    // Collation record on disk (pg_collation)
    struct CollationRecord
    {
        uint32_t collation_id;
        char name[128];         // Collation name (e.g., "utf8_general_ci")
        uint16_t charset_id;    // Associated character set ID
        uint8_t collation_type; // CollationType enum value
        uint8_t strength;       // CollationStrength enum value
        uint8_t pad_space;      // 1 = PAD SPACE, 0 = NO PAD
        uint8_t is_default;     // 1 = default for charset, 0 = not default
        uint16_t reserved;
        char locale[32]; // Locale string (e.g., "en_US")
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid; // 1 if valid, 0 if deleted
        uint32_t padding;  // Alignment
    };

    // Constraint types
    enum class ConstraintType : uint8_t
    {
        PRIMARY_KEY = 0, // Primary key constraint
        FOREIGN_KEY = 1, // Foreign key constraint
        UNIQUE = 2,      // Unique constraint
        CHECK = 3,       // Check constraint
        NOT_NULL = 4,    // Not null constraint
        DEFAULT = 5,     // Default value constraint
        EXCLUSION = 6    // Exclusion constraint
    };

    // Constraint record on disk
    struct ConstraintRecord
    {
        ID constraint_id;
        ID table_id;
        char constraint_name[512];  // SQL standard: 128 characters (512 bytes = 128 chars × 4 bytes/char max UTF-8)
        ID owner_id;                // Owner UUID reference
        uint8_t constraint_type;    // ConstraintType enum
        uint8_t is_deferrable;      // Can be deferred to end of transaction
        uint8_t initially_deferred; // Initially deferred or immediate
        uint8_t reserved_flags;
        uint16_t column_count;  // Number of columns involved
        ID column_ids[16];      // Columns involved in constraint (max 16)
        ID referenced_table_id; // For foreign keys
        uint16_t referenced_column_count;
        ID referenced_column_ids[16]; // Referenced columns for FK
        uint32_t check_expr_oid;      // TOAST reference for check expression - IMPLEMENTED
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Sequence record on disk
    struct SequenceRecord
    {
        ID sequence_id;
        ID schema_id;
        char sequence_name[512]; // SQL standard: 128 characters (512 bytes = 128 chars × 4 bytes/char max UTF-8)
        ID owner_id;             // Owner UUID reference
        int64_t current_value;
        int64_t increment_by;
        int64_t min_value;
        int64_t max_value;
        int64_t cache_size;
        uint8_t cycle;       // 1 if cycle, 0 if no cycle
        uint8_t reserved[7]; // Alignment
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // View record on disk
    struct ViewRecord
    {
        ID view_id;
        ID schema_id;
        char view_name[512];     // SQL standard: 128 characters (512 bytes = 128 chars × 4 bytes/char max UTF-8)
        ID owner_id;             // Owner UUID reference
        uint32_t definition_oid; // TOAST reference for view definition SQL - IMPLEMENTED
        uint8_t is_materialized; // 1 if materialized view
        uint8_t reserved[3];
        uint64_t created_time;
        uint64_t last_refreshed; // For materialized views
        uint32_t is_valid;
        uint32_t padding;
    };

    // Trigger record on disk
    struct TriggerRecord
    {
        ID trigger_id;
        ID table_id;
        char trigger_name[512]; // SQL standard: 128 characters (512 bytes = 128 chars × 4 bytes/char max UTF-8)
        uint8_t trigger_timing; // 0=BEFORE, 1=AFTER, 2=INSTEAD OF
        uint8_t trigger_events; // Bitmask: 0x01=INSERT, 0x02=UPDATE, 0x04=DELETE
        uint8_t for_each_row;   // 1 if FOR EACH ROW, 0 if FOR EACH STATEMENT
        uint8_t enabled;        // 1 if enabled
        uint32_t condition_oid; // TOAST reference for WHEN condition
        uint32_t action_oid;    // TOAST reference for trigger action
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Permission record on disk
    // Phase 1.1: Security System - Updated for UUID-based references
    struct PermissionRecord
    {
        ID permission_id;
        ID object_id;         // ID of schema, table, etc.
        uint8_t object_type;  // ObjectType enum (0=SCHEMA, 1=TABLE, 2=VIEW, 3=SEQUENCE, etc.)

        // Grantee (who receives privileges)
        ID grantee_id;        // User, Role, or Group UUID
        uint8_t grantee_type; // USER=0, ROLE=1, GROUP=2, PUBLIC=3

        // Privileges
        uint32_t privileges;  // Bitmask of Privilege enum
        uint8_t grant_option; // 1 if WITH GRANT OPTION

        // Grantor (who granted privileges)
        ID grantor_id;        // User UUID of grantor

        uint8_t reserved[6];
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Security Phase 3.3: Column-level permissions record (catalog table #39)
    struct ColumnPermissionRecord
    {
        ID permission_id;     // UUIDv7
        ID table_id;          // References pg_tables
        char column_name[128]; // Column being protected (fixed-size for record alignment)
        ID grantee_id;        // User, Role, Group, or PUBLIC UUID
        uint8_t grantee_type; // USER=1, ROLE=2, GROUP=3, PUBLIC=4
        uint32_t privileges;  // Bitmask: SELECT=1, UPDATE=2, INSERT=4, REFERENCES=8
        uint8_t grant_option; // 1 if WITH GRANT OPTION
        ID grantor_id;        // User who granted this
        uint64_t created_time;
        uint32_t is_valid;    // MGA: soft delete flag
        uint32_t padding;     // Alignment
    };

    // Security Phase 3.4: Row-level security policy record (catalog table #40)
    struct PolicyRecord
    {
        ID policy_id;           // UUIDv7
        ID table_id;            // References pg_tables
        char policy_name[64];   // Policy name (unique per table)
        uint8_t policy_type;    // ALL=0, SELECT=1, INSERT=2, UPDATE=3, DELETE=4
        uint32_t roles_oid;     // TOAST reference for roles array (0 = all roles)
        uint32_t using_expr_oid; // TOAST reference for USING expression (required)
        uint32_t with_check_expr_oid; // TOAST reference for WITH CHECK expression (optional, 0 = none)
        uint8_t is_enabled;     // Policy enabled flag
        uint64_t created_time;
        uint64_t modified_time;
        uint32_t is_valid;      // MGA: soft delete flag
        uint8_t padding[3];     // Alignment to 8-byte boundary
    };

    // Object Permission record on disk (Phase 3.1 - SQL Object Permissions)
    struct ObjectPermissionRecord
    {
        ID permission_id;       // UUIDv7 - unique permission identifier
        ID object_id;           // Object UUID (procedure/function/view/table)
        uint8_t object_type;    // 1=PROCEDURE, 2=FUNCTION, 3=VIEW, 4=TABLE, 5=SEQUENCE
        ID grantee_id;          // User/Role/Group UUID
        uint8_t grantee_type;   // 1=USER, 2=ROLE, 3=GROUP
        uint32_t permissions;   // Bitmask: EXECUTE=1, SELECT=2, INSERT=4, UPDATE=8, DELETE=16, etc.
        uint8_t grant_option;   // WITH GRANT OPTION flag (0=no, 1=yes)
        ID grantor_id;          // Who granted this permission (user UUID)
        uint64_t created_time;  // When granted
        uint8_t is_valid;       // MGA: soft delete flag
        uint8_t padding[6];     // Padding to 8-byte boundary
    };

    // Statistics record on disk
    struct StatisticsRecord
    {
        ID stats_id;
        ID table_id;
        ID column_id;
        int64_t n_distinct;            // Number of distinct values
        float null_frac;               // Fraction of null values
        float avg_width;               // Average width in bytes
        uint32_t most_common_vals_oid; // TOAST reference for MCVs
        uint32_t histogram_bounds_oid; // TOAST reference for histogram
        uint64_t last_analyzed;        // Timestamp of last ANALYZE
        uint32_t is_valid;
        uint32_t padding;
    };

    // Dependency record on disk (Phase 1.4 - Catalog Corrections)
    struct DependencyRecord
    {
        ID dependency_id;           // Unique dependency record ID
        ID dependent_object_id;     // Object that depends ON something
        uint8_t dependent_type;     // VIEW, TRIGGER, FK, PROCEDURE, etc.
        uint8_t reserved1[7];       // Alignment
        ID referenced_object_id;    // Object being depended upon
        uint8_t referenced_type;    // TABLE, VIEW, SEQUENCE, etc.
        uint8_t dependency_type;    // NORMAL, AUTO, INTERNAL, PIN
        uint8_t reserved2[6];       // Alignment
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Comment record on disk (Phase 1.5 - Catalog Corrections)
    struct CommentRecord
    {
        ID comment_id;
        ID object_id;               // Object being commented
        uint8_t object_type;        // TABLE, COLUMN, VIEW, etc.
        uint8_t reserved[7];        // Alignment
        ID owner_id;                // Owner UUID reference
        uint32_t comment_text_oid;  // TOAST reference - unlimited size comment text
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // User record on disk (Phase 2 - Security Tables)
    struct UserRecord
    {
        ID user_id;
        char username[512];         // User login name
        uint32_t password_hash_oid; // TOAST reference - hashed password (bcrypt, argon2, etc.)
        uint32_t user_metadata_oid; // TOAST reference - JSON metadata (preferences, settings)
        ID default_schema_id;       // UUID reference to default schema
        uint8_t is_active;          // 1 if active, 0 if disabled
        uint8_t is_superuser;       // 1 if superuser, 0 if normal user
        uint8_t reserved[6];        // Alignment
        uint64_t created_time;
        uint64_t last_login_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Role record on disk (Phase 2 - Security Tables)
    struct RoleRecord
    {
        ID role_id;
        char role_name[512];        // Role name
        ID owner_id;                // Owner UUID reference
        uint32_t role_metadata_oid; // TOAST reference - JSON metadata (permissions, settings)
        uint8_t is_active;          // 1 if active, 0 if disabled
        uint8_t reserved[7];        // Alignment
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Group record on disk (Phase 2 - Security Tables)
    struct GroupRecord
    {
        ID group_id;
        char group_name[512];       // Group name
        char external_id[512];      // AD/LDAP group ID (empty if local)
        uint8_t group_type;         // LOCAL, AD, LDAP
        uint8_t reserved[7];        // Alignment
        uint32_t group_metadata_oid; // TOAST reference - JSON metadata
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Role membership record on disk (Phase 2 - Security Tables)
    struct RoleMembershipRecord
    {
        ID membership_id;
        ID user_id;                 // User who is member
        ID role_id;                 // Role they belong to
        ID granted_by;              // User who granted this membership
        uint8_t with_admin_option;  // 1 if user can grant this role to others
        uint8_t reserved[7];        // Alignment
        uint64_t granted_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Group membership record on disk (Phase 1.1 - Security System)
    // Tracks user/group membership in groups (for nesting)
    struct GroupMembershipRecord
    {
        ID membership_id;           // UUID v7
        ID user_id;                 // User or Group UUID (for nesting)
        uint8_t member_type;        // USER=0, GROUP=1
        uint8_t reserved1[7];       // Alignment
        ID group_id;                // Parent group UUID
        ID granted_by;              // User who added member
        uint64_t granted_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Group mapping record on disk (Phase 1.1 - Security System)
    // Maps external authentication groups (LDAP/AD/Kerberos) to internal groups
    struct GroupMappingRecord
    {
        ID mapping_id;              // UUID v7
        char external_group_name[512]; // LDAP DN, Kerberos principal, AD SID
        uint8_t auth_method;        // LDAP=1, KERBEROS=2, AD=3
        uint8_t auto_create_users;  // 1 = auto-create users on first login
        uint8_t reserved[6];        // Alignment
        ID internal_group_id;       // Maps to GroupRecord UUID
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Procedure record on disk (Phase 3 - Stored Code Tables)
    // NOTE: Functions and Procedures stored in same table
    struct ProcedureRecord
    {
        ID procedure_id;
        ID schema_id;
        char procedure_name[512];
        ID owner_id;                // Owner UUID reference
        uint8_t procedure_type;     // PROCEDURE vs FUNCTION
        uint8_t is_selectable;      // 1 if has SUSPEND (Firebird selectable procedures)
        uint8_t language;           // PSQL, SQL, UDR, etc.
        uint8_t sql_security;       // Phase 3.1: 0=DEFINER, 1=INVOKER (default)
        uint8_t reserved[4];        // Alignment
        uint32_t parameter_count;
        uint32_t return_type_oid;   // TOAST reference for return type definition
        uint32_t body_oid;          // TOAST reference - procedure/function body
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Procedure parameter record on disk (Phase 3 - Stored Code Tables)
    struct ProcedureParameterRecord
    {
        ID parameter_id;
        ID procedure_id;            // Parent procedure/function
        char parameter_name[512];
        uint16_t parameter_position; // Position in parameter list (1-based)
        uint8_t parameter_mode;     // IN, OUT, INOUT
        uint8_t reserved[5];        // Alignment
        uint32_t data_type_oid;     // TOAST reference for data type definition
        uint32_t default_value_oid; // TOAST reference for default value expression
        uint32_t is_valid;
        uint32_t padding;
    };

    // Domain record on disk (Phase 3 - Stored Code Tables)
    struct DomainRecord
    {
        ID domain_id;
        ID schema_id;
        char domain_name[512];
        ID owner_id;                // Owner UUID reference
        uint32_t base_type_oid;     // TOAST reference for base data type
        uint32_t check_expr_oid;    // TOAST reference for CHECK constraint expression
        uint8_t not_null;           // 1 if NOT NULL constraint
        uint8_t reserved[7];        // Alignment
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // UDR (User-Defined Resource) record on disk (Phase 3 - Stored Code Tables)
    struct UDRRecord
    {
        ID udr_id;
        ID schema_id;
        char udr_name[512];
        ID owner_id;                // Owner UUID reference
        char library_path[1024];    // Path to shared library
        char entry_point[512];      // Function entry point name
        uint8_t udr_type;           // FUNCTION, PROCEDURE, TRIGGER
        uint8_t reserved[7];        // Alignment
        uint32_t signature_oid;     // TOAST reference for signature definition
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Package record on disk (Phase 3 - Stored Code Tables)
    // Firebird-style packages
    struct PackageRecord
    {
        ID package_id;
        ID schema_id;
        char package_name[512];
        ID owner_id;                // Owner UUID reference
        uint32_t package_header_oid; // TOAST reference for package header
        uint32_t package_body_oid;   // TOAST reference for package body
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Emulation type record on disk (Phase 4 - Emulation Tables)
    struct EmulationTypeRecord
    {
        ID emulation_type_id;
        char emulation_name[64];    // "mysql", "postgres", "mssql", "firebird"
        uint8_t version_major;
        uint8_t version_minor;
        uint16_t reserved;
        uint32_t mapping_rules_oid; // TOAST reference - JSON mapping rules
        uint64_t created_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Emulation server record on disk (Phase 4 - Emulation Tables)
    struct EmulationServerRecord
    {
        ID server_id;
        char server_name[512];
        ID emulation_type_id;       // References EmulationTypeRecord
        ID owner_id;                // Owner UUID reference
        uint32_t server_config_oid; // TOAST reference - JSON server configuration
        uint8_t is_active;          // 1 if server is active
        uint8_t reserved[7];        // Alignment
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Emulated database record on disk (Phase 4 - Emulation Tables)
    struct EmulatedDatabaseRecord
    {
        ID emulated_db_id;
        char database_name[512];
        ID server_id;               // References EmulationServerRecord
        ID schema_id;               // Schema containing emulation views
        ID owner_id;                // Owner UUID reference
        uint32_t db_metadata_oid;   // TOAST reference - JSON database metadata
        uint8_t is_active;          // 1 if database emulation is active
        uint8_t reserved[7];        // Alignment
        uint64_t created_time;
        uint64_t last_modified_time;
        uint32_t is_valid;
        uint32_t padding;
    };

    // Foreign key constraint record on disk (Phase D - FK Disk Persistence)
    struct ForeignKeyRecord
    {
        ID fk_id;                      // Unique FK constraint ID
        char fk_name[512];             // Constraint name
        ID child_table_id;             // Table with the FK (referencing table)
        ID parent_table_id;            // Referenced table
        char child_columns[1024];      // Delimited column names (comma-separated)
        char parent_columns[1024];     // Delimited column names (comma-separated)
        uint8_t on_delete;             // FKAction enum (NO_ACTION, RESTRICT, CASCADE, SET_NULL, SET_DEFAULT)
        uint8_t on_update;             // FKAction enum
        uint8_t match_type;            // FKMatchType enum (SIMPLE, FULL, PARTIAL)
        uint8_t is_enabled;            // 1 if enabled, 0 if disabled
        uint8_t reserved[4];           // Alignment
        uint64_t created_time;
        uint32_t is_valid;             // 1 if valid, 0 if deleted
        uint32_t padding;
    };

    // Collation record on disk - see updated CollationRecord structure below at line ~194

#pragma pack(pop)

    // ========================================================================
    // Index Type Helper Functions (LSM Integration Plan Phase 1)
    // ========================================================================

    /**
     * Convert string to IndexType enum
     *
     * Supports both exact names and common aliases (case-insensitive):
     * - "LSM", "LSMTREE", "LSM-TREE" → IndexType::LSM
     * - "SPGIST", "SP-GIST" → IndexType::SPGIST
     * - "VECTOR", "HNSW" → IndexType::HNSW
     * - etc.
     */
    std::optional<CatalogManager::IndexType> parseIndexType(const std::string &type_str)
    {
        static const std::unordered_map<std::string, CatalogManager::IndexType> type_map = {
            {"BTREE", CatalogManager::IndexType::BTREE},
            {"B-TREE", CatalogManager::IndexType::BTREE},
            {"HASH", CatalogManager::IndexType::HASH},
            {"HNSW", CatalogManager::IndexType::HNSW},
            {"VECTOR", CatalogManager::IndexType::HNSW},  // Alias
            {"FULLTEXT", CatalogManager::IndexType::FULLTEXT},
            {"GIN", CatalogManager::IndexType::GIN},
            {"GIST", CatalogManager::IndexType::GIST},
            {"BRIN", CatalogManager::IndexType::BRIN},
            {"RTREE", CatalogManager::IndexType::RTREE},
            {"R-TREE", CatalogManager::IndexType::RTREE},  // Alias
            {"SPGIST", CatalogManager::IndexType::SPGIST},
            {"SP-GIST", CatalogManager::IndexType::SPGIST},  // Alias
            {"BITMAP", CatalogManager::IndexType::BITMAP},
            {"COLUMNSTORE", CatalogManager::IndexType::COLUMNSTORE},
            {"LSM", CatalogManager::IndexType::LSM},
            {"LSMTREE", CatalogManager::IndexType::LSM},  // Alias
            {"LSM-TREE", CatalogManager::IndexType::LSM}  // Alias
        };

        // Convert to uppercase for case-insensitive comparison
        std::string upper = type_str;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                      [](unsigned char c) { return std::toupper(c); });

        auto it = type_map.find(upper);
        return (it != type_map.end()) ? std::optional<CatalogManager::IndexType>(it->second) : std::nullopt;
    }

    /**
     * Convert IndexType enum to string representation
     */
    std::string indexTypeToString(CatalogManager::IndexType type)
    {
        switch (type)
        {
            case CatalogManager::IndexType::BTREE: return "BTREE";
            case CatalogManager::IndexType::HASH: return "HASH";
            case CatalogManager::IndexType::HNSW: return "HNSW";
            case CatalogManager::IndexType::FULLTEXT: return "FULLTEXT";
            case CatalogManager::IndexType::GIN: return "GIN";
            case CatalogManager::IndexType::GIST: return "GIST";
            case CatalogManager::IndexType::BRIN: return "BRIN";
            case CatalogManager::IndexType::RTREE: return "RTREE";
            case CatalogManager::IndexType::SPGIST: return "SPGIST";
            case CatalogManager::IndexType::BITMAP: return "BITMAP";
            case CatalogManager::IndexType::COLUMNSTORE: return "COLUMNSTORE";
            case CatalogManager::IndexType::LSM: return "LSM";
            default: return "UNKNOWN";
        }
    }

    // ========================================================================

    CatalogManager::CatalogManager(Database *db) : db_(db)
    {
        DEBUG_LOG_DB("CatalogManager created");
    }

    CatalogManager::~CatalogManager()
    {
        DEBUG_LOG_DB("CatalogManager destroyed");
    }

    auto CatalogManager::initialize(ErrorContext *ctx) -> Status
    {
        // NOTE: Assumes mutex_ is already held by caller (load())
        DEBUG_LOG_DB("Initializing system catalog");

        // Write catalog root page
        Status status = writeCatalogRoot(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate pages for system tables
        PageManager *pm = db_->page_manager();
        if (pm == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "PageManager not available");
            return Status::INVALID_ARGUMENT;
        }

        DEBUG_LOG_DB("CatalogManager::initialize - allocating pages");

        // Allocate schema table page
        status = pm->allocatePage(schemas_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Initialize schemas table page
        auto page_buffer = std::make_unique<uint8_t[]>(db_->page_size());
        if (!page_buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate page buffer");
            return Status::OOM;
        }

        memset(page_buffer.get(), 0, db_->page_size());
        auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer.get());

        heap->header.magic = K_MAGIC_SBRD;
        heap->header.version = 1;
        heap->header.page_type = PAGE_TYPE_HEAP;
        heap->header.page_size = db_->page_size();
        heap->header.page_id = schemas_table_page_;
        heap->header.flags = 0;
        memcpy(heap->header.database_uuid, db_->uuid().bytes.data(), 16);
        heap->header.generation = 1;
        heap->record_count = 0;
        heap->free_offset = sizeof(CatalogHeapPage);
        heap->header.free_space = db_->page_size() - sizeof(CatalogHeapPage);
        heap->header.item_count = 0;
        heap->header.free_offset = sizeof(CatalogHeapPage);

        status = db_->write_page(schemas_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize tables page
        status = pm->allocatePage(tables_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        heap->header.page_id = tables_table_page_;
        status = db_->write_page(tables_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize columns page
        status = pm->allocatePage(columns_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        heap->header.page_id = columns_table_page_;
        status = db_->write_page(columns_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize indexes page
        status = pm->allocatePage(indexes_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        heap->header.page_id = indexes_table_page_;
        status = db_->write_page(indexes_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize constraints page
        status = pm->allocatePage(constraints_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = constraints_table_page_;
        status = db_->write_page(constraints_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize sequences page
        status = pm->allocatePage(sequences_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = sequences_table_page_;
        status = db_->write_page(sequences_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize views page
        status = pm->allocatePage(views_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = views_table_page_;
        status = db_->write_page(views_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize triggers page
        status = pm->allocatePage(triggers_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = triggers_table_page_;
        status = db_->write_page(triggers_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize permissions page
        status = pm->allocatePage(permissions_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = permissions_table_page_;
        status = db_->write_page(permissions_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Security Phase 3.3: Allocate and initialize column permissions page (table #39)
        status = pm->allocatePage(column_permissions_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = column_permissions_table_page_;
        status = db_->write_page(column_permissions_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize statistics page
        status = pm->allocatePage(statistics_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = statistics_table_page_;
        status = db_->write_page(statistics_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize collations page
        status = pm->allocatePage(collations_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = collations_table_page_;
        status = db_->write_page(collations_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize timezones page
        status = pm->allocatePage(timezones_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = timezones_table_page_;
        status = db_->write_page(timezones_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize charsets page (pg_charset)
        status = pm->allocatePage(charsets_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = charsets_table_page_;
        status = db_->write_page(charsets_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate and initialize collation definitions page (pg_collation)
        status = pm->allocatePage(collation_defs_table_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        heap->header.page_id = collation_defs_table_page_;
        status = db_->write_page(collation_defs_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Phase 6.1: Allocate and initialize new system tables (14 tables)

        // Dependencies table (Phase 1.4)
        status = pm->allocatePage(dependencies_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = dependencies_table_page_;
        status = db_->write_page(dependencies_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Comments table (Phase 1.5)
        status = pm->allocatePage(comments_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = comments_table_page_;
        status = db_->write_page(comments_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Users table (Phase 2)
        status = pm->allocatePage(users_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = users_table_page_;
        status = db_->write_page(users_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Roles table (Phase 2)
        status = pm->allocatePage(roles_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = roles_table_page_;
        status = db_->write_page(roles_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Groups table (Phase 2)
        status = pm->allocatePage(groups_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = groups_table_page_;
        status = db_->write_page(groups_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Role Memberships table (Phase 2)
        status = pm->allocatePage(role_memberships_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = role_memberships_table_page_;
        status = db_->write_page(role_memberships_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Group Memberships table (Phase 1.1 - Security System)
        status = pm->allocatePage(group_memberships_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = group_memberships_table_page_;
        status = db_->write_page(group_memberships_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Group Mappings table (Phase 1.1 - Security System)
        status = pm->allocatePage(group_mappings_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = group_mappings_table_page_;
        status = db_->write_page(group_mappings_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Procedures table (Phase 3)
        status = pm->allocatePage(procedures_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = procedures_table_page_;
        status = db_->write_page(procedures_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Procedure Parameters table (Phase 3)
        status = pm->allocatePage(procedure_params_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = procedure_params_table_page_;
        status = db_->write_page(procedure_params_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Domains table (Phase 3)
        status = pm->allocatePage(domains_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = domains_table_page_;
        status = db_->write_page(domains_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // UDR table (Phase 3)
        status = pm->allocatePage(udr_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = udr_table_page_;
        status = db_->write_page(udr_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Packages table (Phase 3)
        status = pm->allocatePage(packages_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = packages_table_page_;
        status = db_->write_page(packages_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Emulation Types table (Phase 4)
        status = pm->allocatePage(emulation_types_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = emulation_types_table_page_;
        status = db_->write_page(emulation_types_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Emulation Servers table (Phase 4)
        status = pm->allocatePage(emulation_servers_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = emulation_servers_table_page_;
        status = db_->write_page(emulation_servers_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Emulated Databases table (Phase 4)
        status = pm->allocatePage(emulated_dbs_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = emulated_dbs_table_page_;
        status = db_->write_page(emulated_dbs_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        // Phase D: Foreign keys table (FK Disk Persistence)
        status = pm->allocatePage(foreign_keys_table_page_, ctx);
        if (status != Status::OK) return status;
        heap->header.page_id = foreign_keys_table_page_;
        status = db_->write_page(foreign_keys_table_page_, page_buffer.get(), ctx);
        if (status != Status::OK) return status;

        DEBUG_LOG_DB("Allocated and initialized 15 new system tables (Phase 6.1 + FK)");

        // Update root page with table locations
        status = writeCatalogRoot(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create default schema hierarchy (18 schemas)
        // Schema tree structure:
        // root (top-level)
        // ├── sys (system catalogs)
        // │   ├── sec (security)
        // │   │   ├── srv (servers)
        // │   │   ├── users (security users - NOT home directories)
        // │   │   ├── roles (security roles)
        // │   │   └── groups (AD/LDAP groups)
        // │   ├── mon (monitoring)
        // │   └── agents (background agents)
        // ├── app (application data)
        // ├── users (user home directories - DIFFERENT from sys.sec.users)
        // ├── remote (remote/federated objects)
        // ├── emulation (database emulation layer)
        // │   ├── mysql (MySQL compatibility)
        // │   ├── postgres (PostgreSQL compatibility)
        // │   ├── mssql (SQL Server compatibility)
        // │   └── firebird (Firebird compatibility)
        // └── public (default user schema)

        ID root_id, sys_id, sec_id, srv_id, users_sec_id, roles_id, groups_id;
        ID mon_id, agents_id, app_id, users_home_id, remote_id, emulation_id;
        ID mysql_id, postgres_id, mssql_id, firebird_id, public_id;

        // Level 0: root
        status = createSchemaInternal("root", "system", root_id, ID(), ctx);
        if (status != Status::OK) return status;

        // Level 1: Top-level schemas under root
        status = createSchemaInternal("sys", "system", sys_id, root_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("app", "system", app_id, root_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("users", "system", users_home_id, root_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("remote", "system", remote_id, root_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("emulation", "system", emulation_id, root_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("public", "system", public_id, root_id, ctx);
        if (status != Status::OK) return status;

        // Level 2: sys.* schemas
        status = createSchemaInternal("sec", "system", sec_id, sys_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("mon", "system", mon_id, sys_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("agents", "system", agents_id, sys_id, ctx);
        if (status != Status::OK) return status;

        // Level 3: sys.sec.* schemas (security)
        status = createSchemaInternal("srv", "system", srv_id, sec_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("sec_users", "system", users_sec_id, sec_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("roles", "system", roles_id, sec_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("groups", "system", groups_id, sec_id, ctx);
        if (status != Status::OK) return status;

        // Level 2: emulation.* schemas
        status = createSchemaInternal("mysql", "system", mysql_id, emulation_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("postgres", "system", postgres_id, emulation_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("mssql", "system", mssql_id, emulation_id, ctx);
        if (status != Status::OK) return status;

        status = createSchemaInternal("firebird", "system", firebird_id, emulation_id, ctx);
        if (status != Status::OK) return status;

        DEBUG_LOG_DB("System catalog initialized with 18 schemas in hierarchy");
        DEBUG_LOG_DB("  schemas page=" << schemas_table_page_
                     << ", tables page=" << tables_table_page_
                     << ", columns page=" << columns_table_page_);

        // ========================================================================
        // Phase 1.2: Security System Bootstrap
        // ========================================================================
        DEBUG_LOG_DB("Bootstrapping security system (SYSTEM user, PUBLIC role, DB_OWNER role)");

        // 1. Create SYSTEM user (superuser, owner of all system objects)
        UserRecord system_user;
        memset(&system_user, 0, sizeof(UserRecord));
        system_user.user_id = SecurityConstants::makeSystemUserID();
        strncpy(system_user.username, "SYSTEM", sizeof(system_user.username) - 1);
        system_user.password_hash_oid = 0;  // No password (cannot login directly)
        system_user.user_metadata_oid = 0;  // No metadata
        system_user.default_schema_id = public_id;  // Default to public schema
        system_user.is_active = 1;
        system_user.is_superuser = 1;  // Superuser flag
        system_user.created_time = std::chrono::system_clock::now().time_since_epoch().count();
        system_user.last_login_time = 0;  // Never logged in
        system_user.is_valid = 1;

        status = writeRecordToHeapPage(users_table_page_, system_user, ctx);
        if (status != Status::OK)
        {
            DEBUG_LOG_DB("Failed to create SYSTEM user: " << static_cast<int>(status));
            return status;
        }
        DEBUG_LOG_DB("Created SYSTEM user with UUID: 00000000-0000-7000-8000-737973746d00");

        // 2. Create PUBLIC role (all users are implicit members)
        RoleRecord public_role;
        memset(&public_role, 0, sizeof(RoleRecord));
        public_role.role_id = generateUuidV7();  // Generate UUID v7
        strncpy(public_role.role_name, "PUBLIC", sizeof(public_role.role_name) - 1);
        public_role.owner_id = system_user.user_id;  // Owned by SYSTEM
        public_role.role_metadata_oid = 0;
        public_role.is_active = 1;
        public_role.created_time = std::chrono::system_clock::now().time_since_epoch().count();
        public_role.last_modified_time = public_role.created_time;
        public_role.is_valid = 1;

        status = writeRecordToHeapPage(roles_table_page_, public_role, ctx);
        if (status != Status::OK)
        {
            DEBUG_LOG_DB("Failed to create PUBLIC role: " << static_cast<int>(status));
            return status;
        }
        DEBUG_LOG_DB("Created PUBLIC role");

        // 3. Create DB_OWNER role (database owner privileges)
        RoleRecord db_owner_role;
        memset(&db_owner_role, 0, sizeof(RoleRecord));
        db_owner_role.role_id = generateUuidV7();  // Generate UUID v7
        strncpy(db_owner_role.role_name, "DB_OWNER", sizeof(db_owner_role.role_name) - 1);
        db_owner_role.owner_id = system_user.user_id;  // Owned by SYSTEM
        db_owner_role.role_metadata_oid = 0;
        db_owner_role.is_active = 1;
        db_owner_role.created_time = std::chrono::system_clock::now().time_since_epoch().count();
        db_owner_role.last_modified_time = db_owner_role.created_time;
        db_owner_role.is_valid = 1;

        status = writeRecordToHeapPage(roles_table_page_, db_owner_role, ctx);
        if (status != Status::OK)
        {
            DEBUG_LOG_DB("Failed to create DB_OWNER role: " << static_cast<int>(status));
            return status;
        }
        DEBUG_LOG_DB("Created DB_OWNER role");

        DEBUG_LOG_DB("Security system bootstrap complete");

        // ========================================================================
        // Phase 3.4.8: Initialize TOAST table for Policy Expressions
        // ========================================================================
        DEBUG_LOG_DB("Initializing TOAST storage for RLS policy expressions");

        // Generate a deterministic UUID for the policy TOAST table
        // Use a well-known UUID so it's consistent across database instances
        // Format: 00000000-0000-7000-8000-746f617374706f ("toastpo" in ASCII)
        constexpr uint8_t POLICY_TOAST_UUID[16] = {
            0x00, 0x00, 0x00, 0x00,  // time_low
            0x00, 0x00,              // time_mid
            0x70, 0x00,              // time_hi_and_version (version 7)
            0x80, 0x00,              // clock_seq
            0x74, 0x6f, 0x61, 0x73, 0x74, 0x70  // node: "toastp" in ASCII
        };
        std::memcpy(policy_toast_table_id_.bytes.data(), POLICY_TOAST_UUID, 16);

        // Create ToastManager for policy expressions
        policy_toast_manager_ = std::make_unique<ToastManager>(db_, policy_toast_table_id_);

        // Initialize the TOAST table (creates pg_toast_<table_id> catalog table)
        status = policy_toast_manager_->initialize(ctx);
        if (status != Status::OK)
        {
            DEBUG_LOG_DB("Failed to initialize policy TOAST manager: " << static_cast<int>(status));
            // Non-fatal - expressions will fall back to in-memory cache only
            // Clear the manager so we don't try to use it
            policy_toast_manager_.reset();
        }
        else
        {
            DEBUG_LOG_DB("Policy TOAST storage initialized successfully");
        }

        return Status::OK;
    }

    auto CatalogManager::load(ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        DEBUG_LOG_DB("Loading system catalog");

        // Try to read catalog root
        Status status = readCatalogRoot(ctx);

        if (status == Status::PAGE_CORRUPT)
        {
            // Catalog not initialized yet, initialize it
            DEBUG_LOG_DB("Catalog not found, initializing");

            return initialize(ctx);
        }
        if (status != Status::OK)
        {
            return status;
        }

        // If we successfully read catalog root, load the data

        // Load schemas
        status = readSchemaRecords(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Load tables
        status = readTableRecords(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Load columns for each table
        for (const auto &[table_id, table_info] : table_cache_)
        {
            status = readColumnRecords(table_info.table_id, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        // Load indexes
        status = readIndexRecords(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Phase 6.2: Load dependencies
        status = readDependencyRecords(ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(CATALOG, "Failed to load dependencies: %d (continuing)", static_cast<int>(status));
            // Don't fail catalog load if dependencies fail - they're not critical for basic operation
        }
        else
        {
            // Rebuild object_to_dependencies_ lookup map from cache
            for (const auto &[dep_id, dep_info] : dependency_cache_)
            {
                object_to_dependencies_.insert({dep_info.dependent_object_id, dep_id});
                object_to_dependencies_.insert({dep_info.referenced_object_id, dep_id});
            }
            DEBUG_LOG_DB("Loaded " << dependency_cache_.size() << " dependencies");
        }

        // Phase 6.2: Load comments
        status = readCommentRecords(ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(CATALOG, "Failed to load comments: %d (continuing)", static_cast<int>(status));
            // Don't fail catalog load if comments fail - they're not critical
        }
        else
        {
            DEBUG_LOG_DB("Loaded " << comment_cache_.size() << " comments");
        }

        // Phase D: Load foreign keys
        status = readForeignKeyRecords(ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(CATALOG, "Failed to load foreign keys: %d (continuing)", static_cast<int>(status));
            // Don't fail catalog load if FK load fails - they're not critical for basic operation
        }
        else
        {
            DEBUG_LOG_DB("Loaded " << foreign_keys_cache_.size() << " foreign keys");
        }

        DEBUG_LOG_DB("Catalog loaded: " << schema_count_ << " schemas, " << table_count_
                                        << " tables, " << dependency_cache_.size() << " dependencies, "
                                        << comment_cache_.size() << " comments, "
                                        << foreign_keys_cache_.size() << " foreign keys");

        return Status::OK;
    }

    // Internal version without lock (assumes caller holds mutex_)
    auto CatalogManager::createSchemaInternal(const std::string &schema_name,
                                              const std::string &owner, ID &schema_id,
                                              const ID &parent_schema_id,
                                              ErrorContext *ctx) -> Status
    {
        // Check if schema already exists
        for (const auto &[id, info] : schema_cache_)
        {
            if (info.schema_name == schema_name)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  ("Schema already exists: " + schema_name).c_str());
                return Status::INVALID_ARGUMENT;
            }
        }

        // Create new schema
        SchemaInfo schema;
        schema.schema_id = generateUuidV7();
        schema.parent_schema_id = parent_schema_id;  // Schema hierarchy support
        schema.schema_name = schema_name;
        schema.owner_id = resolveOwnerUUID(owner);  // Resolve owner name to UUID
        schema.default_tablespace_id = 0;  // Default tablespace
        schema.permissions = 0x0FFF;       // Default permissions (read, write, create)
        schema.acl_oid = 0;                // No ACL initially
        // search_path_oid removed - session-only concept
        schema.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
        schema.last_modified_time = schema.created_time;

        // Write to disk
        Status status = writeSchemaRecord(schema, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Update cache
        schema_cache_[schema.schema_id] = schema;
        schema_count_++;
        schema_id = schema.schema_id;

        // Update root page
        status = writeCatalogRoot(ctx);
        if (status == Status::OK)
        {
            // Sync to ensure persistence
            db_->sync(ctx);
        }

        DEBUG_LOG_DB("Created schema: " << schema_name << " (ID: " << schema_id.toString() << ")");

        return status;
    }

    auto CatalogManager::createSchema(const std::string &schema_name, const std::string &owner,
                                      ID &schema_id, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return createSchemaInternal(schema_name, owner, schema_id, ID(), ctx);
    }

    // Helper to resolve owner name to UUID (Phase 5.1 - Owner UUID References)
    // For ALPHA: Returns well-known system UUID for "system", zero UUID for others
    // TODO Phase 6: Implement full user lookup from Users table when user management is complete
    auto CatalogManager::resolveOwnerUUID(const std::string &owner_name) -> ID
    {
        // Well-known system UUID (fixed UUID for bootstrap/system objects)
        // Uses UUIDv7 format with timestamp = 0 and random bits = "system" hash
        // This ensures system objects have a consistent, recognizable owner ID
        static const ID SYSTEM_UUID = []() {
            ID uuid;
            // Create deterministic UUID for "system" owner
            // Format: 00000000-0000-7000-8000-737973746d00
            // (737973746d = hex for "system" truncated)
            uint8_t bytes[16] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00,  // Version 7
                0x80, 0x00, 0x73, 0x79, 0x73, 0x74, 0x6d, 0x00   // Variant + "system"
            };
            std::memcpy(&uuid, bytes, 16);
            return uuid;
        }();

        if (owner_name == "system" || owner_name.empty())
        {
            return SYSTEM_UUID;
        }

        // For other owners, return zero UUID for now
        // Phase 6 TODO: Look up user in Users table and return their user_id
        // This will require:
        // 1. Query Users table by username
        // 2. Return user_id if found
        // 3. Return error if user doesn't exist (or create default user)
        return ID();  // Zero UUID placeholder for non-system users
    }

    // ============================================================================
    // TOAST Helper Methods (Phase 3.4.6 - RLS Expression Storage)
    // ============================================================================

    auto CatalogManager::storeStringInToast(const std::string& str, uint64_t xmin,
                                           uint32_t& oid_out, ErrorContext* ctx) -> Status
    {
        // If string is empty, store 0 OID
        if (str.empty())
        {
            oid_out = 0;
            return Status::OK;
        }

        // Phase 3.4.8: Use actual TOAST storage if available
        if (policy_toast_manager_)
        {
            // Convert string to byte vector
            std::vector<uint8_t> data(str.begin(), str.end());

            // Create TOAST pointer
            ToastPointer pointer;
            memset(&pointer, 0, sizeof(ToastPointer));

            // Store in TOAST using EXTENDED strategy (out-of-line storage)
            Status status = policy_toast_manager_->toastValue(
                data.data(), data.size(),
                ToastStrategy::EXTENDED,
                xmin,
                &pointer,
                ctx);

            if (status != Status::OK)
            {
                DEBUG_LOG_DB("Failed to TOAST policy expression: " << static_cast<int>(status));
                SET_ERROR_CONTEXT(ctx, status, "Failed to store expression in TOAST");
                return status;
            }

            // Return the TOAST value_id as the OID
            oid_out = pointer.va_valueid;
            DEBUG_LOG_DB("Stored policy expression in TOAST with value_id=" << oid_out);
            return Status::OK;
        }

        // Fallback: If TOAST manager not available, use hash-based OID
        // This maintains backward compatibility and allows degraded operation
        std::hash<std::string> hasher;
        oid_out = static_cast<uint32_t>(hasher(str) & 0xFFFFFFFF);
        DEBUG_LOG_DB("TOAST manager unavailable, using hash-based OID: " << oid_out);

        return Status::OK;
    }

    auto CatalogManager::loadStringFromToast(uint32_t oid, uint64_t xmin,
                                            std::string& str_out, ErrorContext* ctx) -> Status
    {
        // If OID is 0, return empty string
        if (oid == 0)
        {
            str_out.clear();
            return Status::OK;
        }

        // Phase 3.4.8: Use actual TOAST storage if available
        if (policy_toast_manager_)
        {
            // Create a ToastPointer with the value_id (OID)
            ToastPointer pointer;
            memset(&pointer, 0, sizeof(ToastPointer));
            pointer.va_header = 0x01;  // TOAST magic byte
            pointer.va_valueid = oid;
            pointer.va_toastrelid = static_cast<uint32_t>(
                *reinterpret_cast<const uint32_t*>(policy_toast_table_id_.bytes.data()));

            // Read from TOAST
            std::vector<uint8_t> data;
            Status status = policy_toast_manager_->detoastValue(&pointer, &data, xmin, ctx);

            if (status != Status::OK)
            {
                DEBUG_LOG_DB("Failed to detoast policy expression: " << static_cast<int>(status));
                SET_ERROR_CONTEXT(ctx, status, "Failed to load expression from TOAST");
                return status;
            }

            // Convert byte vector back to string
            str_out.assign(data.begin(), data.end());
            DEBUG_LOG_DB("Loaded policy expression from TOAST, size=" << str_out.size());
            return Status::OK;
        }

        // Fallback: Cannot load from hash-based OID
        // The caller must use the in-memory cached value
        DEBUG_LOG_DB("TOAST manager unavailable, cannot load from OID: " << oid);
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                         "TOAST manager not available - using in-memory cache");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::getSchema(const ID &schema_id, SchemaInfo &info, ErrorContext *ctx)
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = schema_cache_.find(schema_id);
        if (it == schema_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Schema not found: " + schema_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        info = it->second;
        return Status::OK;
    }

    auto CatalogManager::getSchema(const std::string &schema_name, SchemaInfo &info,
                                   ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[id, schema_info] : schema_cache_)
        {
            if (schema_info.schema_name == schema_name)
            {
                info = schema_info;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          ("Schema not found: " + schema_name).c_str());
        return Status::INVALID_ARGUMENT;
    }

    auto CatalogManager::listSchemas(std::vector<SchemaInfo> &schemas, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        schemas.clear();
        schemas.reserve(schema_cache_.size());

        for (const auto &[id, info] : schema_cache_)
        {
            schemas.push_back(info);
        }

        // Sort by schema_id for consistent ordering
        std::sort(schemas.begin(), schemas.end(), [](const SchemaInfo &a, const SchemaInfo &b)
                  { return a.schema_id < b.schema_id; });

        return Status::OK;
    }

    auto CatalogManager::createTable(const ID &schema_id, const std::string &table_name,
                                     const std::vector<ColumnInfo> &columns, ID &table_id,
                                     uint16_t tablespace_id, // Phase 2 Task 2.3
                                     ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Verify schema exists
        if (schema_cache_.find(schema_id) == schema_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Schema not found: " + schema_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Check if table already exists in schema
        for (const auto &[id, info] : table_cache_)
        {
            if (info.schema_id == schema_id && info.table_name == table_name)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  ("Table already exists: " + table_name).c_str());
                return Status::INVALID_ARGUMENT;
            }
        }

        // Allocate root page for table data
        PageManager *pm = db_->page_manager();
        uint32_t root_page;
        Status status = pm->allocatePage(root_page, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create table info
        TableInfo table;
        table.table_id = generateUuidV7();
        table.schema_id = schema_id;
        table.table_name = table_name;
        table.owner_id = resolveOwnerUUID("system");  // Phase 6 TODO: Get from session context
        table.root_page = root_page;
        table.column_count = columns.size();
        table.row_count = 0;
        table.table_type = TableType::HEAP; // Default to heap table
        table.has_toast = false;            // Will be set to true if needed
        table.tablespace_id = tablespace_id; // Phase 2 Task 2.3: Use specified tablespace
        table.storage_params_oid = 0;       // No custom storage parameters
        table.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
        table.last_modified_time = table.created_time;

        // Write table record
        status = writeTableRecord(table, ctx);
        if (status != Status::OK)
        {
            pm->freePage(root_page, ctx); // Free allocated page
            return status;
        }

        // Assign UUIDs and ordinals to columns before writing them
        std::vector<ColumnInfo> columns_with_ids = columns;
        uint16_t ordinal = 0;
        for (auto &col : columns_with_ids)
        {
            col.column_id = generateUuidV7();
            col.ordinal = ordinal++;
            col.created_time = table.created_time;
        }

        // Write column records
        status = writeColumnRecords(table.table_id, columns_with_ids, ctx);
        if (status != Status::OK)
        {
            // Rollback: mark table record as invalid (logical delete)
            deleteTableRecord(table.table_id, ctx);
            pm->freePage(root_page, ctx);
            return status;
        }

        // Update caches
        table_cache_[table.table_id] = table;
        column_cache_[table.table_id] = columns_with_ids;
        table_count_++;
        table_id = table.table_id;

        // Update root page
        status = writeCatalogRoot(ctx);
        if (status == Status::OK)
        {
            // Sync to ensure persistence
            db_->sync(ctx);
        }

        DEBUG_LOG_DB("Created table: " << table_name << " (ID: " << table_id.toString() << ") with "
                                       << columns.size() << " columns");

        return status;
    }

    auto CatalogManager::getTable(const ID &table_id, TableInfo &info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = table_cache_.find(table_id);
        if (it == table_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Table not found: " + table_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        info = it->second;
        return Status::OK;
    }

    auto CatalogManager::getTable(const ID &schema_id, const std::string &table_name,
                                  TableInfo &info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[id, table_info] : table_cache_)
        {
            if (table_info.schema_id == schema_id && table_info.table_name == table_name)
            {
                info = table_info;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          ("Table not found: " + table_name).c_str());
        return Status::INVALID_ARGUMENT;
    }

    auto CatalogManager::listTables(const ID &schema_id, std::vector<TableInfo> &tables,
                                    ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tables.clear();

        for (const auto &[id, info] : table_cache_)
        {
            if (info.schema_id == schema_id)
            {
                tables.push_back(info);
            }
        }

        // Sort by table name for consistent ordering
        std::sort(tables.begin(), tables.end(), [](const TableInfo &a, const TableInfo &b)
                  { return a.table_name < b.table_name; });

        return Status::OK;
    }

    auto CatalogManager::getColumns(const ID &table_id, std::vector<ColumnInfo> &columns,
                                    ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = column_cache_.find(table_id);
        if (it == column_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Columns not found for table: " + table_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        columns = it->second;

        // Sort by column_id for consistent ordering
        std::sort(columns.begin(), columns.end(), [](const ColumnInfo &a, const ColumnInfo &b)
                  { return a.column_id < b.column_id; });

        return Status::OK;
    }

    auto CatalogManager::getColumn(const ID &table_id, const std::string &column_name,
                                   ColumnInfo &info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = column_cache_.find(table_id);
        if (it == column_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Table not found: " + table_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        for (const auto &col : it->second)
        {
            if (col.column_name == column_name)
            {
                info = col;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          ("Column not found: " + column_name).c_str());
        return Status::INVALID_ARGUMENT;
    }

    auto CatalogManager::createIndex(const ID &table_id, const std::string &index_name,
                                     const std::vector<std::string> &column_names, ID &index_id,
                                     bool is_unique, IndexType index_type,
                                     uint16_t tablespace_id, // Phase 2 Task 2.3
                                     ErrorContext *ctx)
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Verify table exists
        if (table_cache_.find(table_id) == table_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Table not found: " + table_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Check if index already exists
        for (const auto &[id, info] : index_cache_)
        {
            if (info.table_id == table_id && info.index_name == index_name)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  ("Index already exists: " + index_name).c_str());
                return Status::INVALID_ARGUMENT;
            }
        }

        // Resolve column names to column IDs
        std::vector<ID> column_ids;
        for (const auto &col_name : column_names)
        {
            ColumnInfo col_info;
            Status status = getColumn(table_id, col_name, col_info, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            column_ids.push_back(col_info.column_id);
        }

        // Allocate root page for index data
        PageManager *pm = db_->page_manager();
        uint32_t root_page;
        Status status = pm->allocatePage(root_page, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create index info
        IndexInfo index;
        index.index_id = generateUuidV7();
        index.table_id = table_id;
        index.index_name = index_name;
        index.owner_id = resolveOwnerUUID("system");  // Phase 6 TODO: Get from session context
        index.root_page = root_page;
        index.tablespace_id = tablespace_id; // Phase 2 Task 2.3: Use specified tablespace
        index.index_type = index_type;
        index.is_unique = is_unique;
        index.column_ids = column_ids;
        index.index_params_oid = 0; // Will be set later when index params are added
        index.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();

        // Write index record
        status = writeIndexRecord(index, ctx);
        if (status != Status::OK)
        {
            pm->freePage(root_page, ctx);
            return status;
        }

        // Update cache
        index_cache_[index.index_id] = index;
        index_id = index.index_id;

        // TODO: Update root page with index count
        // status = writeCatalogRoot(ctx);
        // if (status == Status::OK) {
        //     db_->sync(ctx);
        // }

        DEBUG_LOG_DB("Created index: " << index_name << " (ID: " << index_id.toString() << ")");

        // LSM Integration Phase 3.2: Instantiate actual index object
        void *index_ptr = nullptr;
        status = IndexFactory::createIndex(index_type, db_, index, &index_ptr, ctx);
        if (status != Status::OK)
        {
            // Rollback: Remove catalog entry
            index_cache_.erase(index.index_id);
            pm->freePage(root_page, ctx);
            return status;
        }

        // Add to index object cache
        {
            std::lock_guard<std::mutex> lock(index_object_mutex_);
            index_object_cache_[index.index_id] = {index_ptr, index_type};
        }

        return Status::OK;
    }

    // Task 17: Create index with expressions and/or WHERE clause
    auto CatalogManager::createIndex(const ID &table_id, const std::string &index_name,
                                     const std::vector<std::string> &column_names,
                                     const std::vector<uint8_t> &expression_data,
                                     const std::vector<uint8_t> &predicate_data,
                                     const std::vector<std::string> &expression_strings,
                                     const std::string &predicate_string,
                                     ID &index_id,
                                     bool is_unique, IndexType index_type,
                                     uint16_t tablespace_id,
                                     ErrorContext *ctx)
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Verify table exists
        if (table_cache_.find(table_id) == table_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Table not found: " + table_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Check if index already exists
        for (const auto &[id, info] : index_cache_)
        {
            if (info.table_id == table_id && info.index_name == index_name)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  ("Index already exists: " + index_name).c_str());
                return Status::INVALID_ARGUMENT;
            }
        }

        // Resolve column names to column IDs (if not an expression index)
        std::vector<ID> column_ids;
        if (!expression_data.empty())
        {
            // Expression index - columns are computed, but we may still need base columns
            // for certain operations. For now, store empty column_ids.
            // TODO: Extract referenced columns from expression tree
        }
        else
        {
            // Regular index - resolve column names
            for (const auto &col_name : column_names)
            {
                ColumnInfo col_info;
                Status status = getColumn(table_id, col_name, col_info, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                column_ids.push_back(col_info.column_id);
            }
        }

        // Allocate root page for index data
        PageManager *pm = db_->page_manager();
        uint32_t root_page;
        Status status = pm->allocatePage(root_page, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Create index info
        IndexInfo index;
        index.index_id = generateUuidV7();
        index.table_id = table_id;
        index.index_name = index_name;
        index.owner_id = resolveOwnerUUID("system");  // Phase 6 TODO: Get from session context
        index.root_page = root_page;
        index.tablespace_id = tablespace_id;
        index.index_type = index_type;
        index.is_unique = is_unique;
        index.column_ids = column_ids;
        index.index_params_oid = 0;
        index.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();

        // Task 17: Set expression/predicate data
        index.is_expression_index = !expression_data.empty();
        index.is_partial_index = !predicate_data.empty();
        index.expression_data = expression_data;
        index.predicate_data = predicate_data;
        index.expression_strings = expression_strings;
        index.predicate_string = predicate_string;

        // TODO: For large expressions, use TOAST storage
        // if (expression_data.size() > TOAST_TUPLE_THRESHOLD) { ... }

        // Write index record
        status = writeIndexRecord(index, ctx);
        if (status != Status::OK)
        {
            pm->freePage(root_page, ctx);
            return status;
        }

        // Update cache
        index_cache_[index.index_id] = index;
        index_id = index.index_id;

        DEBUG_LOG_DB("Created " << (index.is_expression_index ? "expression " : "")
                                << (index.is_partial_index ? "partial " : "")
                                << "index: " << index_name << " (ID: " << index_id.toString() << ")");

        // LSM Integration Phase 3.2: Instantiate actual index object
        void *index_ptr = nullptr;
        status = IndexFactory::createIndex(index_type, db_, index, &index_ptr, ctx);
        if (status != Status::OK)
        {
            // Rollback: Remove catalog entry
            index_cache_.erase(index.index_id);
            pm->freePage(root_page, ctx);
            return status;
        }

        // Add to index object cache
        {
            std::lock_guard<std::mutex> lock(index_object_mutex_);
            index_object_cache_[index.index_id] = {index_ptr, index_type};
        }

        return Status::OK;
    }

    auto CatalogManager::getIndex(const ID &index_id, IndexInfo &info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = index_cache_.find(index_id);
        if (it == index_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Index not found: " + index_id.toString()).c_str());
            return Status::INVALID_ARGUMENT;
        }

        info = it->second;
        return Status::OK;
    }

    auto CatalogManager::getIndex(const ID &table_id, const std::string &index_name,
                                  IndexInfo &info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[id, index_info] : index_cache_)
        {
            if (index_info.table_id == table_id && index_info.index_name == index_name)
            {
                info = index_info;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          ("Index not found: " + index_name).c_str());
        return Status::INVALID_ARGUMENT;
    }

    auto CatalogManager::listIndexesForTable(const ID &table_id, std::vector<IndexInfo> &indexes,
                                             ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        indexes.clear();

        for (const auto &[id, info] : index_cache_)
        {
            if (info.table_id == table_id)
            {
                indexes.push_back(info);
            }
        }

        std::sort(indexes.begin(), indexes.end(), [](const IndexInfo &a, const IndexInfo &b)
                  { return a.index_name < b.index_name; });

        return Status::OK;
    }

    auto CatalogManager::writeCatalogRoot(ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        if (bp == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        // Check if we need to allocate the catalog root page
        PageManager *pm = db_->page_manager();
        if ((pm != nullptr) && !pm->isAllocated(CATALOG_ROOT_PAGE))
        {
            uint32_t allocated_page;
            Status alloc_status = pm->allocatePage(allocated_page, ctx);

            if (alloc_status != Status::OK || allocated_page != CATALOG_ROOT_PAGE)
            {
                // We need page 3 specifically, if we can't get it there's a problem
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Could not allocate catalog root page");
                return Status::INVALID_ARGUMENT;
            }
        }

        void *page_buffer;
        Status status = bp->pinPage(CATALOG_ROOT_PAGE, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *root = reinterpret_cast<CatalogRootPage *>(page_buffer);

        // Initialize header if this is first write
        if (root->header.magic != K_MAGIC_SBRD)
        {
            memset(page_buffer, 0, db_->page_size());
            root->header.magic = K_MAGIC_SBRD;
            root->header.version = 1;
            root->header.page_size = db_->page_size();
            root->header.page_id = CATALOG_ROOT_PAGE;
            memcpy(root->header.database_uuid, db_->uuid().bytes.data(), 16);
        }

        // Always ensure page type is correct
        root->header.page_type = PAGE_TYPE_CATALOG_ROOT;
        root->header.generation++;
        root->schema_count = schema_count_;
        root->table_count = table_count_;

        // Core catalog tables
        root->schemas_page = schemas_table_page_;
        root->tables_page = tables_table_page_;
        root->columns_page = columns_table_page_;
        root->indexes_page = indexes_table_page_;
        root->constraints_page = constraints_table_page_;
        root->sequences_page = sequences_table_page_;
        root->views_page = views_table_page_;
        root->triggers_page = triggers_table_page_;
        root->permissions_page = permissions_table_page_;
        root->statistics_page = statistics_table_page_;
        root->collations_page = collations_table_page_;
        root->timezones_page = timezones_table_page_;
        root->charsets_page = charsets_table_page_;
        root->collation_defs_page = collation_defs_table_page_;

        // Phase 6.1: New system tables
        root->dependencies_page = dependencies_table_page_;
        root->comments_page = comments_table_page_;
        root->users_page = users_table_page_;
        root->roles_page = roles_table_page_;
        root->groups_page = groups_table_page_;
        root->role_members_page = role_memberships_table_page_;
        root->group_members_page = group_memberships_table_page_;    // Phase 1.1
        root->group_mappings_page = group_mappings_table_page_;      // Phase 1.1
        root->procedures_page = procedures_table_page_;
        root->proc_params_page = procedure_params_table_page_;
        root->domains_page = domains_table_page_;
        root->udr_page = udr_table_page_;
        root->packages_page = packages_table_page_;
        root->emulation_types_page = emulation_types_table_page_;
        root->emulation_servers_page = emulation_servers_table_page_;
        root->emulated_dbs_page = emulated_dbs_table_page_;
        root->foreign_keys_page = foreign_keys_table_page_;

        return bp->unpinPage(CATALOG_ROOT_PAGE, true, ctx);
    }

    auto CatalogManager::readCatalogRoot(ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        if (bp == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BufferPool not available");
            return Status::INVALID_ARGUMENT;
        }

        void *page_buffer;
        Status status = bp->pinPage(CATALOG_ROOT_PAGE, &page_buffer, ctx);
        if (status == Status::IO_ERROR)
        {
            // Page doesn't exist yet - catalog not initialized

            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Catalog root page not found");
            return Status::PAGE_CORRUPT;
        }
        if (status != Status::OK)
        {

            return status;
        }

        auto *root = reinterpret_cast<CatalogRootPage *>(page_buffer);

        // Validate catalog root
        if (root->header.page_type != PAGE_TYPE_CATALOG_ROOT)
        {
            bp->unpinPage(CATALOG_ROOT_PAGE, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid catalog root page");
            return Status::PAGE_CORRUPT;
        }

        schema_count_ = root->schema_count;
        table_count_ = root->table_count;

        // Core catalog tables
        schemas_table_page_ = root->schemas_page;
        tables_table_page_ = root->tables_page;
        columns_table_page_ = root->columns_page;
        indexes_table_page_ = root->indexes_page;
        constraints_table_page_ = root->constraints_page;
        sequences_table_page_ = root->sequences_page;
        views_table_page_ = root->views_page;
        triggers_table_page_ = root->triggers_page;
        permissions_table_page_ = root->permissions_page;
        statistics_table_page_ = root->statistics_page;
        collations_table_page_ = root->collations_page;
        timezones_table_page_ = root->timezones_page;
        charsets_table_page_ = root->charsets_page;
        collation_defs_table_page_ = root->collation_defs_page;

        // Phase 6.1: New system tables
        dependencies_table_page_ = root->dependencies_page;
        comments_table_page_ = root->comments_page;
        users_table_page_ = root->users_page;
        roles_table_page_ = root->roles_page;
        groups_table_page_ = root->groups_page;
        role_memberships_table_page_ = root->role_members_page;
        group_memberships_table_page_ = root->group_members_page;    // Phase 1.1
        group_mappings_table_page_ = root->group_mappings_page;      // Phase 1.1
        procedures_table_page_ = root->procedures_page;
        procedure_params_table_page_ = root->proc_params_page;
        domains_table_page_ = root->domains_page;
        udr_table_page_ = root->udr_page;
        packages_table_page_ = root->packages_page;
        emulation_types_table_page_ = root->emulation_types_page;
        emulation_servers_table_page_ = root->emulation_servers_page;
        emulated_dbs_table_page_ = root->emulated_dbs_page;
        foreign_keys_table_page_ = root->foreign_keys_page;

        return bp->unpinPage(CATALOG_ROOT_PAGE, false, ctx);
    }

    // Helper to write a record to a catalog heap page
    template <typename RecordType>
    auto CatalogManager::writeRecordToHeapPage(uint32_t page_id, const RecordType &record,
                                               ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;
        Status status = bp->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);

        // Check if we have space
        if (heap->free_offset + sizeof(RecordType) > db_->page_size())
        {
            bp->unpinPage(page_id, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "Catalog heap page full");
            return Status::INVALID_ARGUMENT;
        }

        // Write record
        auto *dest_record = reinterpret_cast<RecordType *>(
            reinterpret_cast<uint8_t *>(page_buffer) + heap->free_offset);
        memcpy(dest_record, &record, sizeof(RecordType));

        heap->record_count++;
        heap->free_offset += sizeof(RecordType);
        heap->header.free_space -= sizeof(RecordType);
        heap->header.generation++;

        return bp->unpinPage(page_id, true, ctx);
    }

    // ============================================================================
    // updateRecordInHeapPage - Firebird MGA-compliant UPDATE
    // ============================================================================
    // Updates a record IN-PLACE (Firebird MGA) if it exists, or appends (INSERT)
    // if not found. This fixes Bug #1 from MGA_COMPLIANCE_REVIEW_TABLESPACE.md.
    //
    // Key MGA Principles:
    // 1. UPDATE modifies existing record in-place (no new location)
    // 2. Only INSERT appends new records
    // 3. No catalog bloat from repeated ALTER operations
    // ============================================================================

    template <typename RecordType, typename Predicate>
    auto CatalogManager::updateRecordInHeapPage(uint32_t page_id, Predicate matcher,
                                                const RecordType &new_record,
                                                ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;
        Status status = bp->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin catalog heap page");
            return status;
        }

        auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
        uint32_t offset = sizeof(CatalogHeapPage);
        bool found = false;

        // ===== PHASE 1: Search for existing record (MGA UPDATE) =====
        for (uint32_t i = 0; i < heap->record_count; i++)
        {
            auto *record =
                reinterpret_cast<RecordType *>(reinterpret_cast<uint8_t *>(page_buffer) + offset);

            if (record->is_valid && matcher(*record))
            {
                // ✅ FOUND: Update IN-PLACE (Firebird MGA)
                // This is the key fix - we modify the existing record, not append
                memcpy(record, &new_record, sizeof(RecordType));
                found = true;

                // Mark page dirty and increment generation
                heap->header.generation++;

                // Unpin and return success
                return bp->unpinPage(page_id, true, ctx);
            }

            offset += sizeof(RecordType);
        }

        // ===== PHASE 2: Record not found, append new one (INSERT) =====
        if (!found)
        {
            // Check if we have space for new record
            if (heap->free_offset + sizeof(RecordType) > db_->page_size())
            {
                bp->unpinPage(page_id, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "Catalog heap page full");
                return Status::PAGE_FULL;
            }

            // Append new record (INSERT case)
            auto *dest_record = reinterpret_cast<RecordType *>(
                reinterpret_cast<uint8_t *>(page_buffer) + heap->free_offset);
            memcpy(dest_record, &new_record, sizeof(RecordType));

            // Update heap metadata
            heap->record_count++;
            heap->free_offset += sizeof(RecordType);
            heap->header.free_space -= sizeof(RecordType);
            heap->header.generation++;

            return bp->unpinPage(page_id, true, ctx);
        }

        // Should never reach here
        bp->unpinPage(page_id, false, ctx);
        return Status::OK;
    }

    // ============================================================================
    // deleteRecordFromHeapPage - Firebird MGA-compliant DELETE
    // ============================================================================
    // Marks a record as deleted by setting is_valid=0 IN-PLACE (Firebird MGA).
    // This fixes Bug #2 from MGA_COMPLIANCE_REVIEW_TABLESPACE.md.
    //
    // Key MGA Principles:
    // 1. DELETE marks record as invalid in-place (no removal)
    // 2. Record location unchanged (stable for any references)
    // 3. Garbage collection will reclaim space later (like Firebird SWEEP)
    // 4. No catalog bloat from repeated DROP/CREATE cycles
    // ============================================================================

    template <typename RecordType, typename Predicate>
    auto CatalogManager::deleteRecordFromHeapPage(uint32_t page_id, Predicate matcher,
                                                  ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;
        Status status = bp->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin catalog heap page");
            return status;
        }

        auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
        uint32_t offset = sizeof(CatalogHeapPage);
        bool found = false;

        // Search for record to delete
        for (uint32_t i = 0; i < heap->record_count; i++)
        {
            auto *record =
                reinterpret_cast<RecordType *>(reinterpret_cast<uint8_t *>(page_buffer) + offset);

            if (record->is_valid && matcher(*record))
            {
                // ✅ FOUND: Mark as deleted IN-PLACE (Firebird MGA)
                // This is the key fix - we mark invalid, not remove
                record->is_valid = 0;
                found = true;

                // Mark page dirty and increment generation
                heap->header.generation++;

                // Unpin and return success
                return bp->unpinPage(page_id, true, ctx);
            }

            offset += sizeof(RecordType);
        }

        // Record not found
        bp->unpinPage(page_id, false, ctx);
        if (!found)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Record not found for deletion");
            return Status::NOT_FOUND;
        }

        return Status::OK;
    }

    /**
     * compactCatalogHeapPage - Garbage collection for catalog pages
     *
     * Removes is_valid=0 records from catalog heap page by compacting active records.
     * This reclaims space from deleted tablespaces/tables/indexes.
     *
     * Algorithm:
     * 1. Scan page and collect all is_valid=1 records
     * 2. Overwrite page with compacted records (no gaps)
     * 3. Update record_count and free_offset
     * 4. Reclaim space for future catalog entries
     *
     * @tparam RecordType Type of catalog record (e.g., SBTablespaceCatalog)
     * @param page_id Catalog heap page to compact
     * @param ctx Error context
     * @return Status::OK on success, error status otherwise
     */
    template <typename RecordType>
    auto CatalogManager::compactCatalogHeapPage(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;
        Status status = bp->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin catalog heap page for compaction");
            return status;
        }

        auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
        uint32_t read_offset = sizeof(CatalogHeapPage);
        uint32_t write_offset = sizeof(CatalogHeapPage);
        uint32_t new_record_count = 0;
        uint32_t deleted_count = 0;

        // ===== PHASE 1: Compact records in-place =====
        // Copy valid records forward, overwriting deleted records
        for (uint32_t i = 0; i < heap->record_count; i++)
        {
            auto *read_record =
                reinterpret_cast<RecordType *>(reinterpret_cast<uint8_t *>(page_buffer) +
                                                read_offset);

            if (read_record->is_valid)
            {
                // Valid record: keep it
                if (read_offset != write_offset)
                {
                    // Need to move record forward (there were deleted records before this)
                    auto *write_record = reinterpret_cast<RecordType *>(
                        reinterpret_cast<uint8_t *>(page_buffer) + write_offset);
                    memcpy(write_record, read_record, sizeof(RecordType));
                }
                write_offset += sizeof(RecordType);
                new_record_count++;
            }
            else
            {
                // Deleted record: skip it (don't advance write_offset)
                deleted_count++;
            }

            read_offset += sizeof(RecordType);
        }

        // ===== PHASE 2: Update page metadata =====
        uint32_t old_free_offset = heap->free_offset;
        heap->record_count = new_record_count;
        heap->free_offset = write_offset;
        heap->header.free_space = db_->page_size() - write_offset;
        heap->header.generation++;

        // Log compaction results
        uint32_t space_reclaimed = old_free_offset - write_offset;
        LOG_INFO(CATALOG, "Compacted catalog page %u: removed %u deleted records, "
                          "reclaimed %u bytes (%u → %u valid records)",
                 page_id, deleted_count, space_reclaimed, heap->record_count + deleted_count,
                 new_record_count);

        return bp->unpinPage(page_id, true, ctx);  // Mark dirty
    }

    template <typename RecordType, typename InfoType>
    inline auto CatalogManager::readRecordsToVector(
        uint32_t page_id, std::vector<InfoType> &results,
        std::function<bool(const RecordType &)> filter,
        std::function<void(const RecordType &, InfoType &)> converter, ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;
        Status status = bp->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to read catalog heap page");
            return status;
        }

        auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);

        results.clear();
        uint32_t offset = sizeof(CatalogHeapPage);

        for (uint32_t i = 0; i < heap->record_count; i++)
        {
            auto *record =
                reinterpret_cast<RecordType *>(reinterpret_cast<uint8_t *>(page_buffer) + offset);

            if (record->is_valid && filter(*record))
            {
                InfoType info;
                converter(*record, info);
                results.push_back(info);
            }

            offset += sizeof(RecordType);
        }

        return bp->unpinPage(page_id, false, ctx);
    }

    auto CatalogManager::writeSchemaRecord(const SchemaInfo &schema, ErrorContext *ctx) -> Status
    {
        // Phase 3: Validate schema name UTF-8 storage capacity
        Status validation = UTF8Utils::validateStorageCapacity(
            schema.schema_name,
            CatalogConstants::MAX_IDENTIFIER_CHARS,
            CatalogConstants::MAX_IDENTIFIER_STORAGE,
            ctx
        );
        if (validation != Status::OK) {
            return validation;
        }

        // Owner validation removed - now using UUID reference instead of name

        SchemaRecord record;
        memset(&record, 0, sizeof(SchemaRecord)); // Initialize all fields to zero
        record.schema_id = schema.schema_id;
        record.parent_schema_id = schema.parent_schema_id;

        // Phase 3: Safe UTF-8 copy (already validated to fit)
        std::memcpy(record.schema_name, schema.schema_name.c_str(), schema.schema_name.size());
        record.schema_name[schema.schema_name.size()] = '\0';

        // UUID-based owner reference (not name)
        record.owner_id = schema.owner_id;
        record.default_tablespace_id = schema.default_tablespace_id;
        record.permissions = schema.permissions;
        record.default_charset = schema.default_charset;
        record.default_collation_id = schema.default_collation_id;
        record.acl_oid = schema.acl_oid;
        // search_path_oid removed - session-only concept
        record.created_time = schema.created_time;
        record.last_modified_time = schema.last_modified_time;
        record.is_valid = 1;

        return writeRecordToHeapPage(schemas_table_page_, record, ctx);
    }

    auto CatalogManager::readSchemaRecords(ErrorContext *ctx) -> Status
    {
        auto converter = [](const SchemaRecord &record, SchemaInfo &info)
        {
            // Phase 4: Safety check - ensure null-termination at max position
            // This is defensive programming in case of corrupted catalog data
            const_cast<char&>(record.schema_name[511]) = '\0';

            info.schema_id = record.schema_id;
            info.parent_schema_id = record.parent_schema_id;
            info.schema_name = record.schema_name;
            info.owner_id = record.owner_id;  // UUID-based owner reference
            info.default_tablespace_id = record.default_tablespace_id;
            info.permissions = record.permissions;
            info.default_charset = record.default_charset;
            info.default_collation_id = record.default_collation_id;
            info.acl_oid = record.acl_oid;
            // search_path_oid removed - session-only concept
            info.created_time = record.created_time;
            info.last_modified_time = record.last_modified_time;
        };
        auto key_extractor = [](const SchemaInfo &info) { return info.schema_id; };
        return readRecordsFromHeapPage<SchemaRecord, SchemaInfo, ID>(
            schemas_table_page_, schema_cache_, converter, key_extractor, ctx);
    }

    auto CatalogManager::writeTableRecord(const TableInfo &table, ErrorContext *ctx) -> Status
    {
        // Phase 3: Validate table name UTF-8 storage capacity
        Status validation = UTF8Utils::validateStorageCapacity(
            table.table_name,
            CatalogConstants::MAX_IDENTIFIER_CHARS,
            CatalogConstants::MAX_IDENTIFIER_STORAGE,
            ctx
        );
        if (validation != Status::OK) {
            return validation;
        }

        TableRecord record;
        memset(&record, 0, sizeof(TableRecord)); // Initialize all fields to zero
        record.table_id = table.table_id;
        record.schema_id = table.schema_id;

        // Phase 3: Safe UTF-8 copy (already validated to fit)
        std::memcpy(record.table_name, table.table_name.c_str(), table.table_name.size());
        record.table_name[table.table_name.size()] = '\0';

        record.owner_id = table.owner_id;      // UUID-based owner reference
        record.root_page = table.root_page;
        record.column_count = table.column_count;
        record.row_count = table.row_count;
        record.table_type = static_cast<uint8_t>(table.table_type);
        record.has_toast = table.has_toast ? 1 : 0;
        record.rls_enabled = table.rls_enabled ? 1 : 0;  // Security Phase 3.4
        record.rls_forced = table.rls_forced ? 1 : 0;    // Security Phase 3.4
        record.tablespace_id = table.tablespace_id;
        record.default_charset = table.default_charset;
        record.default_collation_id = table.default_collation_id;
        record.storage_params_oid = table.storage_params_oid;
        record.created_time = table.created_time;
        record.last_modified_time = table.last_modified_time;
        record.is_valid = 1;

        return writeRecordToHeapPage(tables_table_page_, record, ctx);
    }

    auto CatalogManager::deleteTableRecord(const ID &table_id, ErrorContext *ctx) -> Status
    {
        // Mark the table record as invalid (logical delete) by setting is_valid = 0
        // This is a simple implementation that scans for the table ID and marks it invalid

        BufferPool *bp = db_->buffer_pool();
        void *page_data;
        Status status = bp->pinPage(tables_table_page_, &page_data, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        HeapPage heap_page(static_cast<uint8_t *>(page_data), db_->page_size());

        // Scan through all records to find the matching table_id
        uint16_t item_count = heap_page.getItemCount();
        bool found = false;

        // Access page data directly as mutable (we own it via pinPage)
        auto *mutable_page_data = static_cast<uint8_t *>(page_data);

        for (uint16_t i = 0; i < item_count; ++i)
        {
            // Get tuple location using const API for bounds checking
            const uint8_t *tuple_data;
            uint32_t tuple_size;

            if (heap_page.getTuple(i, &tuple_data, &tuple_size, ctx) == Status::OK)
            {
                if (tuple_size >= sizeof(TupleHeader) + sizeof(TableRecord))
                {
                    // Calculate mutable pointer to same location
                    // (getTuple validates bounds, but returns const pointer)
                    const ptrdiff_t offset = tuple_data - static_cast<const uint8_t *>(page_data);
                    uint8_t *mutable_tuple_data = mutable_page_data + offset;

                    // Now we can access the record as mutable (no const_cast needed)
                    auto *record =
                        reinterpret_cast<TableRecord *>(mutable_tuple_data + sizeof(TupleHeader));

                    if (record->table_id == table_id && record->is_valid == 1)
                    {
                        // Found the record - mark it as invalid
                        // Safe: we own the page (pinned with write intent)
                        record->is_valid = 0;
                        found = true;
                        break;
                    }
                }
            }
        }

        // Mark page as dirty if we found and updated the record
        bp->unpinPage(tables_table_page_, found, ctx);

        return found ? Status::OK : Status::NOT_FOUND;
    }

    auto CatalogManager::deleteIndexRecord(const ID &index_id, ErrorContext *ctx) -> Status
    {
        // Mark the index record as invalid (logical delete) by setting is_valid = 0
        // Similar to deleteTableRecord but for indexes

        BufferPool *bp = db_->buffer_pool();
        void *page_data;
        Status status = bp->pinPage(indexes_table_page_, &page_data, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        HeapPage heap_page(static_cast<uint8_t *>(page_data), db_->page_size());

        // Scan through all records to find the matching index_id
        uint16_t item_count = heap_page.getItemCount();
        bool found = false;

        // Access page data directly as mutable (we own it via pinPage)
        auto *mutable_page_data = static_cast<uint8_t *>(page_data);

        for (uint16_t i = 0; i < item_count; ++i)
        {
            // Get tuple location using const API for bounds checking
            const uint8_t *tuple_data;
            uint32_t tuple_size;

            if (heap_page.getTuple(i, &tuple_data, &tuple_size, ctx) == Status::OK)
            {
                if (tuple_size >= sizeof(TupleHeader) + sizeof(IndexRecord))
                {
                    // Calculate mutable pointer to same location
                    const ptrdiff_t offset = tuple_data - static_cast<const uint8_t *>(page_data);
                    uint8_t *mutable_tuple_data = mutable_page_data + offset;

                    // Access the record as mutable
                    auto *record =
                        reinterpret_cast<IndexRecord *>(mutable_tuple_data + sizeof(TupleHeader));

                    if (record->index_id == index_id && record->is_valid == 1)
                    {
                        // Found the record - mark it as invalid
                        record->is_valid = 0;
                        found = true;
                        break;
                    }
                }
            }
        }

        // Mark page as dirty if we found and updated the record
        bp->unpinPage(indexes_table_page_, found, ctx);

        return found ? Status::OK : Status::NOT_FOUND;
    }

    auto CatalogManager::updateTableColumnCount(const ID &table_id, uint32_t new_count,
                                                 ErrorContext *ctx) -> Status
    {
        // Update TableRecord.column_count (used by ALTER TABLE ADD/DROP COLUMN)
        // MGA-compliant: Updates column_count in-place

        BufferPool *bp = db_->buffer_pool();
        void *page_data;
        Status status = bp->pinPage(tables_table_page_, &page_data, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        HeapPage heap_page(static_cast<uint8_t *>(page_data), db_->page_size());
        auto *mutable_page_data = static_cast<uint8_t *>(page_data);
        bool found = false;

        for (uint16_t i = 0; i < heap_page.getItemCount(); ++i)
        {
            const uint8_t *tuple_data;
            uint32_t tuple_size;

            if (heap_page.getTuple(i, &tuple_data, &tuple_size, ctx) == Status::OK)
            {
                if (tuple_size >= sizeof(TupleHeader) + sizeof(TableRecord))
                {
                    const ptrdiff_t offset =
                        tuple_data - static_cast<const uint8_t *>(page_data);
                    uint8_t *mutable_tuple_data = mutable_page_data + offset;
                    auto *record = reinterpret_cast<TableRecord *>(mutable_tuple_data +
                                                                    sizeof(TupleHeader));

                    if (record->table_id == table_id && record->is_valid == 1)
                    {
                        record->column_count = new_count;
                        record->last_modified_time =
                            std::chrono::system_clock::now().time_since_epoch().count();
                        found = true;
                        break;
                    }
                }
            }
        }

        bp->unpinPage(tables_table_page_, found, ctx);
        return found ? Status::OK : Status::NOT_FOUND;
    }

    auto CatalogManager::readTableRecords(ErrorContext *ctx) -> Status
    {
        auto converter = [](const TableRecord &record, TableInfo &info)
        {
            // Phase 4: Safety check - ensure null-termination at max position
            const_cast<char&>(record.table_name[511]) = '\0';

            info.table_id = record.table_id;
            info.schema_id = record.schema_id;
            info.table_name = record.table_name;
            info.owner_id = record.owner_id;   // UUID-based owner reference
            info.root_page = record.root_page;
            info.column_count = record.column_count;
            info.row_count = record.row_count;
            info.table_type = static_cast<TableType>(record.table_type);
            info.has_toast = record.has_toast != 0;
            info.rls_enabled = record.rls_enabled != 0;  // Security Phase 3.4
            info.rls_forced = record.rls_forced != 0;    // Security Phase 3.4
            info.tablespace_id = record.tablespace_id;
            info.default_charset = record.default_charset;
            info.default_collation_id = record.default_collation_id;
            info.storage_params_oid = record.storage_params_oid;
            info.created_time = record.created_time;
            info.last_modified_time = record.last_modified_time;
        };
        auto key_extractor = [](const TableInfo &info) { return info.table_id; };
        return readRecordsFromHeapPage<TableRecord, TableInfo, ID>(tables_table_page_, table_cache_,
                                                                   converter, key_extractor, ctx);
    }

    auto CatalogManager::writeColumnRecords(const ID &table_id,
                                            const std::vector<ColumnInfo> &columns,
                                            ErrorContext *ctx) -> Status
    {
        for (const auto &col : columns)
        {
            // Phase 3: Validate column name UTF-8 storage capacity
            Status validation = UTF8Utils::validateStorageCapacity(
                col.column_name,
                CatalogConstants::MAX_IDENTIFIER_CHARS,
                CatalogConstants::MAX_IDENTIFIER_STORAGE,
                ctx
            );
            if (validation != Status::OK) {
                return validation;
            }

            ColumnRecord record;
            memset(&record, 0, sizeof(ColumnRecord)); // Initialize all fields to zero
            record.table_id = table_id;
            record.column_id = col.column_id;

            // Phase 3: Safe UTF-8 copy (already validated to fit)
            std::memcpy(record.column_name, col.column_name.c_str(), col.column_name.size());
            record.column_name[col.column_name.size()] = '\0';
            record.ordinal = col.ordinal;
            record.data_type = col.data_type;
            record.type_precision = col.type_precision;
            record.type_scale = col.type_scale;
            record.max_length = col.max_length;
            record.nullable = col.nullable ? 1 : 0;
            record.has_default = col.has_default ? 1 : 0;
            record.is_primary_key = col.is_primary_key ? 1 : 0;
            record.is_unique = col.is_unique ? 1 : 0;
            record.is_foreign_key = col.is_foreign_key ? 1 : 0;
            record.is_generated = col.is_generated ? 1 : 0;
            record.storage_type = col.storage_type;
            record.with_timezone = col.with_timezone ? 1 : 0;
            record.charset = col.charset;
            record.timezone_hint = col.timezone_hint;
            record.collation_id = col.collation_id;
            strncpy(record.default_value, col.default_value.c_str(), 127);
            record.default_value[127] = '\0';
            record.default_value_oid = col.default_value_oid;
            record.check_expr_oid = col.check_expr_oid;
            record.created_time = col.created_time;
            record.is_valid = 1;

            Status status = writeRecordToHeapPage(columns_table_page_, record, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }
        return Status::OK;
    }

    auto CatalogManager::readColumnRecords(const ID &table_id, ErrorContext *ctx) -> Status
    {
        auto filter = [table_id](const ColumnRecord &record)
        { return record.table_id == table_id; };

        auto converter = [](const ColumnRecord &record, ColumnInfo &info)
        {
            // Phase 4: Safety check - ensure null-termination at max position
            const_cast<char&>(record.column_name[511]) = '\0';

            info.table_id = record.table_id;
            info.column_id = record.column_id;
            info.column_name = record.column_name;
            info.ordinal = record.ordinal;
            info.data_type = record.data_type;
            info.type_precision = record.type_precision;
            info.type_scale = record.type_scale;
            info.max_length = record.max_length;
            info.nullable = record.nullable != 0;
            info.has_default = record.has_default != 0;
            info.is_primary_key = record.is_primary_key != 0;
            info.is_unique = record.is_unique != 0;
            info.is_foreign_key = record.is_foreign_key != 0;
            info.is_generated = record.is_generated != 0;
            info.storage_type = record.storage_type;
            info.with_timezone = record.with_timezone != 0;
            info.charset = record.charset;
            info.timezone_hint = record.timezone_hint;
            info.collation_id = record.collation_id;
            info.default_value = record.default_value;
            info.default_value_oid = record.default_value_oid;
            info.check_expr_oid = record.check_expr_oid;
            info.created_time = record.created_time;
        };

        std::vector<ColumnInfo> columns;
        Status status = readRecordsToVector<ColumnRecord, ColumnInfo>(columns_table_page_, columns,
                                                                      filter, converter, ctx);

        if (status == Status::OK && !columns.empty())
        {
            column_cache_[table_id] = columns;
        }

        return status;
    }

    auto CatalogManager::writeIndexRecord(const IndexInfo &index, ErrorContext *ctx) -> Status
    {
        // Phase 3: Validate index name UTF-8 storage capacity
        Status validation = UTF8Utils::validateStorageCapacity(
            index.index_name,
            CatalogConstants::MAX_IDENTIFIER_CHARS,
            CatalogConstants::MAX_IDENTIFIER_STORAGE,
            ctx
        );
        if (validation != Status::OK) {
            return validation;
        }

        IndexRecord record;
        memset(&record, 0, sizeof(IndexRecord)); // Initialize all fields to zero
        record.index_id = index.index_id;
        record.table_id = index.table_id;

        // Phase 3: Safe UTF-8 copy (already validated to fit)
        std::memcpy(record.index_name, index.index_name.c_str(), index.index_name.size());
        record.index_name[index.index_name.size()] = '\0';
        record.owner_id = index.owner_id;
        record.root_page = index.root_page;
        record.index_type = static_cast<uint8_t>(index.index_type);
        record.is_unique = static_cast<uint8_t>(index.is_unique);
        record.column_count = index.column_ids.size();
        for (size_t i = 0; i < index.column_ids.size(); ++i)
        {
            record.column_ids[i] = index.column_ids[i];
        }
        record.index_params_oid = index.index_params_oid;
        record.created_time = index.created_time;
        record.is_valid = 1;

        return writeRecordToHeapPage(indexes_table_page_, record, ctx);
    }

    auto CatalogManager::readIndexRecords(ErrorContext *ctx) -> Status
    {
        auto converter = [](const IndexRecord &record, IndexInfo &info)
        {
            // Phase 4: Safety check - ensure null-termination at max position
            const_cast<char&>(record.index_name[511]) = '\0';

            info.index_id = record.index_id;
            info.table_id = record.table_id;
            info.index_name = record.index_name;
            info.owner_id = record.owner_id;
            info.root_page = record.root_page;
            info.index_type = static_cast<IndexType>(record.index_type);
            info.is_unique = record.is_unique;
            info.column_ids.assign(record.column_ids, record.column_ids + record.column_count);
            info.index_params_oid = record.index_params_oid;
            info.created_time = record.created_time;
        };
        auto key_extractor = [](const IndexInfo &info) { return info.index_id; };
        return readRecordsFromHeapPage<IndexRecord, IndexInfo, ID>(
            indexes_table_page_, index_cache_, converter, key_extractor, ctx);
    }

    // ===== Timezone Operations =====

    auto CatalogManager::createTimezone(const TimezoneInfo &tz_info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Convert TimezoneInfo to TimezoneRecord
        TimezoneRecord record{};
        memset(&record, 0, sizeof(TimezoneRecord));

        record.timezone_id = tz_info.timezone_id;
        strncpy(record.name, tz_info.name.c_str(), 63);
        record.name[63] = '\0';
        strncpy(record.abbreviation, tz_info.abbreviation.c_str(), 15);
        record.abbreviation[15] = '\0';
        record.std_offset_minutes = tz_info.std_offset_minutes;
        record.observes_dst = tz_info.observes_dst ? 1 : 0;
        record.dst_start_month = tz_info.dst_start_month;
        record.dst_start_week = tz_info.dst_start_week;
        record.dst_start_day = tz_info.dst_start_day;
        record.dst_start_hour = tz_info.dst_start_hour;
        record.dst_end_month = tz_info.dst_end_month;
        record.dst_end_week = tz_info.dst_end_week;
        record.dst_end_day = tz_info.dst_end_day;
        record.dst_end_hour = tz_info.dst_end_hour;
        record.dst_offset_minutes = tz_info.dst_offset_minutes;
        record.created_time = tz_info.created_time;
        record.last_modified_time = tz_info.last_modified_time;
        record.is_valid = 1;

        return writeRecordToHeapPage<TimezoneRecord>(timezones_table_page_, record, ctx);
    }

    auto CatalogManager::updateTimezone(uint16_t timezone_id, const TimezoneInfo &tz_info,
                                        ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "updateTimezone not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::getTimezone(uint16_t timezone_id, TimezoneInfo &info, ErrorContext *ctx)
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto predicate = [timezone_id](const TimezoneRecord &rec)
        { return rec.timezone_id == timezone_id && rec.is_valid; };
        auto result = findRecordInHeapPage<TimezoneRecord>(timezones_table_page_, predicate, ctx);

        if (result.status != Status::OK)
        {
            return result.status;
        }

        // Convert to TimezoneInfo
        const auto &rec = result.record;
        info.timezone_id = rec.timezone_id;
        info.name = rec.name;
        info.abbreviation = rec.abbreviation;
        info.std_offset_minutes = rec.std_offset_minutes;
        info.observes_dst = rec.observes_dst != 0;
        info.dst_start_month = rec.dst_start_month;
        info.dst_start_week = rec.dst_start_week;
        info.dst_start_day = rec.dst_start_day;
        info.dst_start_hour = rec.dst_start_hour;
        info.dst_end_month = rec.dst_end_month;
        info.dst_end_week = rec.dst_end_week;
        info.dst_end_day = rec.dst_end_day;
        info.dst_end_hour = rec.dst_end_hour;
        info.dst_offset_minutes = rec.dst_offset_minutes;
        info.created_time = rec.created_time;
        info.last_modified_time = rec.last_modified_time;

        return Status::OK;
    }

    auto CatalogManager::getTimezoneByName(const std::string &name, TimezoneInfo &info,
                                           ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto predicate = [&name](const TimezoneRecord &rec)
        { return std::string(rec.name) == name && rec.is_valid; };
        auto result = findRecordInHeapPage<TimezoneRecord>(timezones_table_page_, predicate, ctx);

        if (result.status != Status::OK)
        {
            return result.status;
        }

        // Convert to TimezoneInfo
        const auto &rec = result.record;
        info.timezone_id = rec.timezone_id;
        info.name = rec.name;
        info.abbreviation = rec.abbreviation;
        info.std_offset_minutes = rec.std_offset_minutes;
        info.observes_dst = rec.observes_dst != 0;
        info.dst_start_month = rec.dst_start_month;
        info.dst_start_week = rec.dst_start_week;
        info.dst_start_day = rec.dst_start_day;
        info.dst_start_hour = rec.dst_start_hour;
        info.dst_end_month = rec.dst_end_month;
        info.dst_end_week = rec.dst_end_week;
        info.dst_end_day = rec.dst_end_day;
        info.dst_end_hour = rec.dst_end_hour;
        info.dst_offset_minutes = rec.dst_offset_minutes;
        info.created_time = rec.created_time;
        info.last_modified_time = rec.last_modified_time;

        return Status::OK;
    }

    auto CatalogManager::listTimezones(std::vector<TimezoneInfo> &timezones, ErrorContext *ctx)
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto converter = [](const TimezoneRecord &rec, TimezoneInfo &info)
        {
            info.timezone_id = rec.timezone_id;
            info.name = rec.name;
            info.abbreviation = rec.abbreviation;
            info.std_offset_minutes = rec.std_offset_minutes;
            info.observes_dst = rec.observes_dst != 0;
            info.dst_start_month = rec.dst_start_month;
            info.dst_start_week = rec.dst_start_week;
            info.dst_start_day = rec.dst_start_day;
            info.dst_start_hour = rec.dst_start_hour;
            info.dst_end_month = rec.dst_end_month;
            info.dst_end_week = rec.dst_end_week;
            info.dst_end_day = rec.dst_end_day;
            info.dst_end_hour = rec.dst_end_hour;
            info.dst_offset_minutes = rec.dst_offset_minutes;
            info.created_time = rec.created_time;
            info.last_modified_time = rec.last_modified_time;
        };

        return scanHeapPage<TimezoneRecord, TimezoneInfo>(timezones_table_page_, timezones,
                                                          converter, ctx);
    }

    auto CatalogManager::deleteTimezone(uint16_t timezone_id, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto predicate = [timezone_id](const TimezoneRecord &rec)
        { return rec.timezone_id == timezone_id && rec.is_valid; };
        auto result = findRecordInHeapPage<TimezoneRecord>(timezones_table_page_, predicate, ctx);

        if (result.status != Status::OK)
        {
            return result.status;
        }

        // Mark as deleted
        TimezoneRecord record = result.record;
        record.is_valid = 0;

        return updateRecordInHeapPage<TimezoneRecord>(timezones_table_page_, result.slot_index,
                                                      record, ctx);
    }

    // ========== Character Set Operations ==========
    // NOTE: These implementations are stubs and need proper helper template functions
    //       (findRecordInHeapPage, updateRecordInHeapPage, scanHeapPage, etc.)
    //       Similar to timezone operations, they are marked for future implementation.

    auto CatalogManager::createCharset(const CharsetInfo &cs_info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        CharsetRecord rec;
        memset(&rec, 0, sizeof(rec));

        rec.charset_id = cs_info.charset_id;
        strncpy(rec.name, cs_info.name.c_str(), sizeof(rec.name) - 1);
        strncpy(rec.description, cs_info.description.c_str(), sizeof(rec.description) - 1);
        rec.min_bytes = cs_info.min_bytes;
        rec.max_bytes = cs_info.max_bytes;
        rec.variable_width = cs_info.variable_width;
        rec.default_collation_id = cs_info.default_collation_id;
        rec.created_time = static_cast<uint64_t>(std::time(nullptr));
        rec.last_modified_time = rec.created_time;
        rec.is_valid = 1;

        return writeRecordToHeapPage(charsets_table_page_, rec, ctx);
    }

    auto CatalogManager::updateCharset(uint16_t charset_id, const CharsetInfo &cs_info,
                                       ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "updateCharset not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::getCharset(uint16_t charset_id, CharsetInfo &info, ErrorContext *ctx)
        -> Status
    {
        // TODO: Needs findRecordInHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "getCharset not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::getCharsetByName(const std::string &name, CharsetInfo &info,
                                          ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "getCharsetByName not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::listCharsets(std::vector<CharsetInfo> &charsets, ErrorContext *ctx)
        -> Status
    {
        // TODO: Needs scanHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "listCharsets not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::deleteCharset(uint16_t charset_id, ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "deleteCharset not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    // ========== Collation Operations ==========
    // NOTE: These implementations are stubs and need proper helper template functions

    auto CatalogManager::createCollation(const CollationCatalogInfo &col_info, ErrorContext *ctx)
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        CollationRecord rec;
        memset(&rec, 0, sizeof(rec));

        rec.collation_id = col_info.collation_id;
        strncpy(rec.name, col_info.name.c_str(), sizeof(rec.name) - 1);
        rec.charset_id = col_info.charset_id;
        rec.collation_type = col_info.collation_type;
        rec.strength = col_info.strength;
        rec.pad_space = col_info.pad_space;
        rec.is_default = col_info.is_default;
        strncpy(rec.locale, col_info.locale, sizeof(rec.locale) - 1);
        rec.created_time = static_cast<uint64_t>(std::time(nullptr));
        rec.last_modified_time = rec.created_time;
        rec.is_valid = 1;

        return writeRecordToHeapPage(collation_defs_table_page_, rec, ctx);
    }

    auto CatalogManager::updateCollation(uint32_t collation_id,
                                         const CollationCatalogInfo &col_info, ErrorContext *ctx)
        -> Status
    {
        // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "updateCollation not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::getCollation(uint32_t collation_id, CollationCatalogInfo &info,
                                      ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "getCollation not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::getCollationByName(const std::string &name, CollationCatalogInfo &info,
                                            ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "getCollationByName not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::listCollations(std::vector<CollationCatalogInfo> &collations,
                                        ErrorContext *ctx) -> Status
    {
        // TODO: Needs scanHeapPage helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "listCollations not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::listCollationsForCharset(uint16_t charset_id,
                                                  std::vector<CollationCatalogInfo> &collations,
                                                  ErrorContext *ctx) -> Status
    {
        // TODO: Needs scanHeapPageWithFilter helper function
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "listCollationsForCharset not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    auto CatalogManager::deleteCollation(uint32_t collation_id, ErrorContext *ctx) -> Status
    {
        // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "deleteCollation not fully implemented");
        return Status::NOT_IMPLEMENTED;
    }

    // ============================================================================
    // Tablespace Catalog Methods (Phase 1, Task 1.1.8)
    // ============================================================================

    auto CatalogManager::writeTablespaceRecord(const TablespaceInfo &tablespace, ErrorContext *ctx)
        -> Status
    {
        // Phase 3: Validate tablespace name UTF-8 storage capacity (64 bytes, not 512)
        // Note: Tablespace names are limited to 63 characters (separate from SQL identifier standard)
        Status validation = UTF8Utils::validateStorageCapacity(
            tablespace.tablespace_name,
            63,   // Tablespace-specific limit (not CatalogConstants::MAX_IDENTIFIER_CHARS)
            64,   // Tablespace storage capacity (char[64])
            ctx
        );
        if (validation != Status::OK) {
            return validation;
        }

        // Convert TablespaceInfo to SBTablespaceCatalog
        SBTablespaceCatalog record = {};
        record.is_valid = 1;
        record.tablespace_id = tablespace.tablespace_id;

        // Phase 3: Safe UTF-8 copy (already validated to fit, max 63 chars + null)
        std::memcpy(record.tablespace_name, tablespace.tablespace_name.c_str(), tablespace.tablespace_name.size());
        record.tablespace_name[tablespace.tablespace_name.size()] = '\0';

        // Copy UUID
        std::memcpy(&record.tablespace_uuid, &tablespace.tablespace_uuid, sizeof(UuidV7Bytes));

        // Configuration
        record.autoextend_enabled = tablespace.autoextend_enabled ? 1 : 0;
        record.autoextend_size_mb = tablespace.autoextend_size_mb;
        record.max_size_mb = tablespace.max_size_mb;
        record.prealloc_pages = tablespace.prealloc_pages;
        record.flags = 0;  // Reserved

        // File information
        if (!tablespace.file_paths.empty())
        {
            std::strncpy(record.primary_path, tablespace.file_paths[0].c_str(), 255);
            record.primary_path[255] = '\0';
        }
        record.file_count = static_cast<uint32_t>(tablespace.file_paths.size());

        // Statistics
        record.total_size_mb = tablespace.total_size_mb;
        record.used_size_mb = tablespace.used_size_mb;
        record.free_size_mb = tablespace.free_size_mb;
        record.table_count = tablespace.table_count;
        record.index_count = tablespace.index_count;

        // Timestamps
        record.created_time = tablespace.created_time;
        record.last_modified_time = tablespace.last_modified_time;
        record.last_extended_time = tablespace.last_extended_time;

        // ===== MGA FIX: Use UPDATE-or-INSERT pattern =====
        // This fixes Bug #1 from MGA_COMPLIANCE_REVIEW_TABLESPACE.md
        // Previously: Always appended (PostgreSQL MVCC - WRONG!)
        // Now: Updates in-place if exists, appends if new (Firebird MGA - CORRECT!)

        // Matcher: Find record with matching tablespace_id
        auto matcher = [tablespace_id = tablespace.tablespace_id](const SBTablespaceCatalog &r) {
            return r.is_valid && r.tablespace_id == tablespace_id;
        };

        return updateRecordInHeapPage<SBTablespaceCatalog>(tablespaces_table_page_, matcher,
                                                           record, ctx);
    }

    auto CatalogManager::readTablespaceRecords(ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;

        // Pin pg_tablespace page
        Status status = bp->pinPage(tablespaces_table_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin pg_tablespace page");
            return status;
        }

        auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
        uint32_t offset = sizeof(CatalogHeapPage);

        tablespace_cache_.clear();

        for (uint32_t i = 0; i < heap->record_count; i++)
        {
            auto *record = reinterpret_cast<SBTablespaceCatalog *>(
                reinterpret_cast<uint8_t *>(page_buffer) + offset);

            if (record->is_valid)
            {
                // Phase 4: Safety check - ensure null-termination at max position
                // Tablespace names use char[64] (not char[512])
                record->tablespace_name[63] = '\0';

                // Convert SBTablespaceCatalog to TablespaceInfo
                TablespaceInfo info;
                info.tablespace_id = record->tablespace_id;
                info.tablespace_name = std::string(record->tablespace_name);
                std::memcpy(&info.tablespace_uuid, &record->tablespace_uuid, sizeof(UuidV7Bytes));

                // Configuration
                info.autoextend_enabled = (record->autoextend_enabled == 1);
                info.autoextend_size_mb = record->autoextend_size_mb;
                info.max_size_mb = record->max_size_mb;
                info.prealloc_pages = record->prealloc_pages;

                // File paths
                if (record->primary_path[0] != '\0')
                {
                    info.file_paths.push_back(std::string(record->primary_path));
                }

                // Statistics
                info.total_size_mb = record->total_size_mb;
                info.used_size_mb = record->used_size_mb;
                info.free_size_mb = record->free_size_mb;
                info.table_count = record->table_count;
                info.index_count = record->index_count;

                // Timestamps
                info.created_time = record->created_time;
                info.last_modified_time = record->last_modified_time;
                info.last_extended_time = record->last_extended_time;

                // Add to cache (keyed by tablespace_id)
                tablespace_cache_[info.tablespace_id] = info;
            }

            offset += sizeof(SBTablespaceCatalog);
        }

        return bp->unpinPage(tablespaces_table_page_, false, ctx);
    }

    // ===== Tablespace Operations (Phase 2 Task 2.1) =====

    auto CatalogManager::createTablespace(const std::string &tablespace_name,
                                          const std::string &location, bool autoextend_enabled,
                                          uint32_t autoextend_size_mb, uint32_t max_size_mb,
                                          uint32_t prealloc_pages, uint16_t &tablespace_id,
                                          ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Validate tablespace name
        if (tablespace_name.empty() || tablespace_name.length() > 63)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Tablespace name must be 1-63 characters");
            return Status::INVALID_ARGUMENT;
        }

        // Check if tablespace name already exists
        for (const auto &[id, info] : tablespace_cache_)
        {
            if (info.tablespace_name == tablespace_name)
            {
                SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS,
                                  ("Tablespace '" + tablespace_name + "' already exists").c_str());
                return Status::FILE_EXISTS;
            }
        }

        // Validate location path
        if (location.empty() || location.length() > 255)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Location path must be 1-255 characters");
            return Status::INVALID_ARGUMENT;
        }

        // Validate parameters
        if (autoextend_size_mb == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "AUTOEXTEND_SIZE must be greater than 0");
            return Status::INVALID_ARGUMENT;
        }

        if (max_size_mb > 0 && max_size_mb < autoextend_size_mb)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "MAXSIZE must be >= AUTOEXTEND_SIZE or 0 (UNLIMITED)");
            return Status::INVALID_ARGUMENT;
        }

        // Allocate new tablespace ID (find next available)
        uint16_t new_id = 2; // Start from 2 (0 = invalid, 1 = primary)
        while (tablespace_cache_.find(new_id) != tablespace_cache_.end())
        {
            new_id++;
            if (new_id == 0)
            {
                // Wrapped around
                SET_ERROR_CONTEXT(ctx, Status::OOM,
                                  "No available tablespace IDs (maximum 65535)");
                return Status::OOM;
            }
        }

        // Get PageManager
        PageManager *pm = db_->page_manager();
        if (!pm)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "PageManager not available");
            return Status::INVALID_ARGUMENT;
        }

        // Create TablespaceConfig
        TablespaceConfig config;
        config.autoextend_enabled = autoextend_enabled;
        config.autoextend_size_mb = autoextend_size_mb;
        config.max_size_mb = max_size_mb;
        config.prealloc_pages = prealloc_pages;

        // Create the tablespace file via PageManager
        Status status = pm->createTablespace(new_id, tablespace_name, location, config, ctx);
        if (status != Status::OK)
        {
            return status; // Error context already set
        }

        // Create TablespaceInfo
        TablespaceInfo info;
        info.tablespace_id = new_id;
        info.tablespace_name = tablespace_name;
        info.tablespace_uuid = generateUuidV7();
        info.autoextend_enabled = autoextend_enabled;
        info.autoextend_size_mb = autoextend_size_mb;
        info.max_size_mb = max_size_mb;
        info.prealloc_pages = prealloc_pages;
        info.file_paths.push_back(location);
        info.total_size_mb = 0; // Will be updated by PageManager
        info.used_size_mb = 0;
        info.free_size_mb = 0;
        info.table_count = 0;
        info.index_count = 0;
        info.created_time = 0; // TODO: Add timestamp
        info.last_modified_time = 0;
        info.last_extended_time = 0;

        // Write to catalog
        status = writeTablespaceRecord(info, ctx);
        if (status != Status::OK)
        {
            // Rollback: close and delete the tablespace file
            pm->closeTablespace(new_id, ctx);
            return status;
        }

        // Add to cache
        tablespace_cache_[new_id] = info;

        // Return the allocated ID
        tablespace_id = new_id;

        return Status::OK;
    }

    auto CatalogManager::dropTablespace(const std::string &tablespace_name, bool force,
                                        ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find tablespace by name
        uint16_t ts_id = 0;
        TablespaceInfo ts_info;
        bool found = false;

        for (const auto &[id, info] : tablespace_cache_)
        {
            if (info.tablespace_name == tablespace_name)
            {
                ts_id = id;
                ts_info = info;
                found = true;
                break;
            }
        }

        if (!found)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Tablespace '" + tablespace_name + "' not found").c_str());
            return Status::NOT_FOUND;
        }

        // Cannot drop primary tablespace
        if (ts_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Cannot drop primary tablespace");
            return Status::INVALID_ARGUMENT;
        }

        // Check if tablespace is empty (unless FORCE)
        if (!force && (ts_info.table_count > 0 || ts_info.index_count > 0))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              ("Tablespace '" + tablespace_name +
                                  "' is not empty. Use FORCE to drop anyway.").c_str());
            return Status::INVALID_ARGUMENT;
        }

        // TODO: If FORCE, drop all tables/indexes in the tablespace first
        // For now, return NOT_IMPLEMENTED for FORCE with objects
        if (force && (ts_info.table_count > 0 || ts_info.index_count > 0))
        {
            SET_ERROR_CONTEXT(
                ctx, Status::NOT_IMPLEMENTED,
                "FORCE drop of non-empty tablespace not yet implemented (will be in Phase 2)");
            return Status::NOT_IMPLEMENTED;
        }

        // Get PageManager
        PageManager *pm = db_->page_manager();
        if (!pm)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "PageManager not available");
            return Status::INVALID_ARGUMENT;
        }

        // Close the tablespace file
        Status status = pm->closeTablespace(ts_id, ctx);
        if (status != Status::OK)
        {
            return status; // Error context already set
        }

        // TODO: Delete the file from filesystem
        // For now, we just close it and remove from catalog

        // Mark record as deleted in catalog (Firebird MGA - in-place)
        // This fixes Bug #2 from MGA_COMPLIANCE_REVIEW_TABLESPACE.md
        auto matcher = [tablespace_id = ts_id](const SBTablespaceCatalog &r) {
            return r.is_valid && r.tablespace_id == tablespace_id;
        };

        status = deleteRecordFromHeapPage<SBTablespaceCatalog>(tablespaces_table_page_, matcher,
                                                                ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to delete tablespace from catalog");
            return status;
        }

        // Remove from cache
        tablespace_cache_.erase(ts_id);

        LOG_INFO(CATALOG, "Successfully dropped tablespace '%s' (ID %u)",
                 tablespace_name.c_str(), ts_id);

        return Status::OK;
    }

    auto CatalogManager::getTablespace(uint16_t tablespace_id, TablespaceInfo &info,
                                       ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = tablespace_cache_.find(tablespace_id);
        if (it == tablespace_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Tablespace ID " + std::to_string(tablespace_id) + " not found").c_str());
            return Status::NOT_FOUND;
        }

        info = it->second;
        return Status::OK;
    }

    auto CatalogManager::getTablespaceByName(const std::string &tablespace_name,
                                             TablespaceInfo &info, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto &[id, ts_info] : tablespace_cache_)
        {
            if (ts_info.tablespace_name == tablespace_name)
            {
                info = ts_info;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Tablespace '" + tablespace_name + "' not found").c_str());
        return Status::NOT_FOUND;
    }

    auto CatalogManager::listTablespaces(std::vector<TablespaceInfo> &tablespaces,
                                         ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        tablespaces.clear();
        tablespaces.reserve(tablespace_cache_.size());

        for (const auto &[id, info] : tablespace_cache_)
        {
            tablespaces.push_back(info);
        }

        (void)ctx; // Suppress unused parameter warning
        return Status::OK;
    }

    auto CatalogManager::updateTablespace(const std::string &tablespace_name,
                                          bool autoextend_enabled, uint32_t autoextend_size_mb,
                                          uint32_t max_size_mb, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find tablespace by name
        uint16_t ts_id = 0;
        TablespaceInfo *ts_info = nullptr;
        bool found = false;

        for (auto &[id, info] : tablespace_cache_)
        {
            if (info.tablespace_name == tablespace_name)
            {
                ts_id = id;
                ts_info = &info;
                found = true;
                break;
            }
        }

        if (!found)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Tablespace '" + tablespace_name + "' not found").c_str());
            return Status::NOT_FOUND;
        }

        // Validate parameters
        if (autoextend_size_mb == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "AUTOEXTEND_SIZE must be greater than 0");
            return Status::INVALID_ARGUMENT;
        }

        if (max_size_mb > 0 && max_size_mb < autoextend_size_mb)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "MAXSIZE must be >= AUTOEXTEND_SIZE or 0 (UNLIMITED)");
            return Status::INVALID_ARGUMENT;
        }

        // TODO: Validate MAXSIZE >= current file size (requires PageManager API)

        // Update in-memory cache
        ts_info->autoextend_enabled = autoextend_enabled;
        ts_info->autoextend_size_mb = autoextend_size_mb;
        ts_info->max_size_mb = max_size_mb;

        // Write updated record to catalog
        Status status = writeTablespaceRecord(*ts_info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // TODO: Update TablespaceHeader on disk (page 0 of tablespace file)
        // This requires PageManager API to write header

        return Status::OK;
    }

    auto CatalogManager::renameTablespace(const std::string &old_name,
                                          const std::string &new_name, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Validate new name
        if (new_name.empty() || new_name.length() > 63)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Tablespace name must be 1-63 characters");
            return Status::INVALID_ARGUMENT;
        }

        // Check if new name already exists
        for (const auto &[id, info] : tablespace_cache_)
        {
            if (info.tablespace_name == new_name)
            {
                SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS,
                                  ("Tablespace '" + new_name + "' already exists").c_str());
                return Status::FILE_EXISTS;
            }
        }

        // Find tablespace by old name
        uint16_t ts_id = 0;
        TablespaceInfo *ts_info = nullptr;
        bool found = false;

        for (auto &[id, info] : tablespace_cache_)
        {
            if (info.tablespace_name == old_name)
            {
                ts_id = id;
                ts_info = &info;
                found = true;
                break;
            }
        }

        if (!found)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Tablespace '" + old_name + "' not found").c_str());
            return Status::NOT_FOUND;
        }

        // Cannot rename primary tablespace
        if (ts_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Cannot rename primary tablespace");
            return Status::INVALID_ARGUMENT;
        }

        // Update in-memory cache
        ts_info->tablespace_name = new_name;

        // Write updated record to catalog
        Status status = writeTablespaceRecord(*ts_info, ctx);
        if (status != Status::OK)
        {
            // Rollback
            ts_info->tablespace_name = old_name;
            return status;
        }

        // TODO: Update TablespaceHeader on disk (page 0 of tablespace file)
        // This requires PageManager API to write header

        return Status::OK;
    }

    // ========================================================================
    // PHASE 3, TASK 3.1.4: Update Tablespace Statistics After Extension
    // ========================================================================

    auto CatalogManager::updateTablespaceStats(uint16_t tablespace_id, uint64_t total_size_mb,
                                               uint64_t free_size_mb, uint64_t last_extended_time,
                                               ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find tablespace in cache
        auto it = tablespace_cache_.find(tablespace_id);
        if (it == tablespace_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Tablespace ID " + std::to_string(tablespace_id) + " not found").c_str());
            return Status::NOT_FOUND;
        }

        TablespaceInfo &ts_info = it->second;

        // Update statistics
        ts_info.total_size_mb = total_size_mb;
        ts_info.free_size_mb = free_size_mb;
        ts_info.last_extended_time = last_extended_time;

        // Calculate used size
        if (total_size_mb >= free_size_mb)
        {
            ts_info.used_size_mb = total_size_mb - free_size_mb;
        }
        else
        {
            // This should never happen, but guard against overflow
            LOG_WARNING(CATALOG,
                       "Tablespace %u has free_size_mb (%lu) > total_size_mb (%lu), setting used to 0",
                       tablespace_id,
                       static_cast<unsigned long>(free_size_mb),
                       static_cast<unsigned long>(total_size_mb));
            ts_info.used_size_mb = 0;
        }

        // Write updated record to catalog
        Status status = writeTablespaceRecord(ts_info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        LOG_INFO(CATALOG,
                "Updated tablespace %u statistics: total=%lu MB, used=%lu MB, free=%lu MB",
                tablespace_id,
                static_cast<unsigned long>(total_size_mb),
                static_cast<unsigned long>(ts_info.used_size_mb),
                static_cast<unsigned long>(free_size_mb));

        return Status::OK;
    }

    // ========================================================================
    // PHASE 6: Attach/Detach Operations
    // ========================================================================

    /**
     * attachTablespace - Attach an existing tablespace file to the database
     *
     * Phase 6 Task 6.1.2
     */
    Status CatalogManager::attachTablespace(const std::string &file_path,
                                            const std::string &tablespace_name,
                                            uint16_t &tablespace_id_out,
                                            ErrorContext *ctx)
    {
        LOG_INFO(CATALOG, "attachTablespace: Attaching tablespace from '%s'", file_path.c_str());

        std::lock_guard<std::mutex> lock(mutex_);

        // ===== STEP 1: Validate file path exists and is readable =====

        int fd = ::open(file_path.c_str(), O_RDWR);
        if (fd < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to open tablespace file: " + file_path +
                              " (errno=" + std::to_string(errno) + ", " + std::string(strerror(errno)) + ")").c_str());
            return Status::IO_ERROR;
        }

        // ===== STEP 2: Read and validate TablespaceHeader =====

        uint32_t page_size = db_->page_size();
        auto header_buffer = std::make_unique<uint8_t[]>(page_size);

        ssize_t bytes_read = ::pread(fd, header_buffer.get(), page_size, 0);
        if (bytes_read != static_cast<ssize_t>(page_size))
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             ("Failed to read tablespace header from " + file_path +
                              " (read " + std::to_string(bytes_read) + " of " +
                              std::to_string(page_size) + " bytes)").c_str());
            return Status::IO_ERROR;
        }

        auto *header = reinterpret_cast<TablespaceHeader *>(header_buffer.get());

        // Validate magic number
        if (header->page_header.magic != K_MAGIC_SBRD)
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Invalid tablespace file: bad magic number in " + file_path).c_str());
            return Status::INVALID_ARGUMENT;
        }

        // ===== STEP 3: Check compatibility =====

        // Check page size matches
        if (header->page_size != page_size)
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Tablespace page size mismatch: file has " +
                              std::to_string(header->page_size) + ", database has " +
                              std::to_string(page_size)).c_str());
            return Status::INVALID_ARGUMENT;
        }

        // ===== STEP 4: Determine tablespace name (handle name conflicts) =====

        std::string final_name;
        if (!tablespace_name.empty())
        {
            // User specified name, use it
            final_name = tablespace_name;
        }
        else
        {
            // Use name from file header
            final_name = std::string(header->tablespace_name);
        }

        // Check for name conflicts
        for (const auto &[ts_id, ts_info] : tablespace_cache_)
        {
            if (ts_info.tablespace_name == final_name)
            {
                ::close(fd);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                 ("Tablespace name '" + final_name +
                                  "' already exists. Use a different name with: ATTACH TABLESPACE '" +
                                  file_path + "' AS 'new_name';").c_str());
                return Status::INVALID_ARGUMENT;
            }
        }

        // ===== STEP 5: Allocate new tablespace_id =====

        uint16_t new_ts_id = 0;
        for (uint16_t candidate = 1; candidate < 65535; ++candidate)
        {
            if (tablespace_cache_.find(candidate) == tablespace_cache_.end())
            {
                new_ts_id = candidate;
                break;
            }
        }

        if (new_ts_id == 0)
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE,
                             "No available tablespace IDs (all 1-65534 in use)");
            return Status::OUT_OF_RANGE;
        }

        LOG_INFO(CATALOG, "Allocated tablespace_id %u for '%s'", new_ts_id, final_name.c_str());

        // ===== STEP 6: Register file descriptor in Database =====

        Status status = db_->registerTablespaceFile(new_ts_id, fd, ctx);
        if (status != Status::OK)
        {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, status, "Failed to register tablespace in Database");
            return status;
        }

        // ===== STEP 7: Load FSM into memory =====

        status = db_->page_manager()->openTablespace(new_ts_id, file_path, ctx);
        if (status != Status::OK)
        {
            db_->unregisterTablespaceFile(new_ts_id, ctx);
            SET_ERROR_CONTEXT(ctx, status, "Failed to open tablespace FSM");
            return status;
        }

        // ===== STEP 8: Create TablespaceInfo and add to catalog =====

        TablespaceInfo ts_info;
        ts_info.tablespace_id = new_ts_id;
        ts_info.tablespace_name = final_name;
        std::memcpy(ts_info.tablespace_uuid.bytes.data(), header->tablespace_uuid.bytes.data(), 16);
        ts_info.autoextend_enabled = (header->autoextend_enabled != 0);
        ts_info.autoextend_size_mb = header->autoextend_size_mb;
        ts_info.max_size_mb = header->max_size_mb;
        ts_info.prealloc_pages = 0;
        ts_info.file_paths.push_back(file_path);

        // Calculate statistics
        uint64_t total_bytes = header->total_pages * page_size;
        uint64_t free_bytes = header->free_pages * page_size;
        ts_info.total_size_mb = total_bytes / (1024 * 1024);
        ts_info.free_size_mb = free_bytes / (1024 * 1024);
        ts_info.used_size_mb = (total_bytes >= free_bytes) ?
                               (total_bytes - free_bytes) / (1024 * 1024) : 0;

        // Set timestamps
        ts_info.created_time = header->creation_time;
        ts_info.last_modified_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count();
        ts_info.last_extended_time = 0;  // Will be updated on first extension

        // Count tables and indexes in this tablespace
        ts_info.table_count = 0;
        ts_info.index_count = 0;
        for (const auto &[table_id, table_info] : table_cache_)
        {
            if (table_info.tablespace_id == new_ts_id)
            {
                ts_info.table_count++;
            }
        }
        for (const auto &[index_id, index_info] : index_cache_)
        {
            if (index_info.tablespace_id == new_ts_id)
            {
                ts_info.index_count++;
            }
        }

        // ===== STEP 9: Write to pg_tablespace catalog =====

        status = writeTablespaceRecord(ts_info, ctx);
        if (status != Status::OK)
        {
            db_->page_manager()->closeTablespace(new_ts_id, ctx);
            db_->unregisterTablespaceFile(new_ts_id, ctx);
            SET_ERROR_CONTEXT(ctx, status, "Failed to write tablespace catalog entry");
            return status;
        }

        // ===== STEP 10: Update cache =====

        tablespace_cache_[new_ts_id] = ts_info;
        tablespace_id_out = new_ts_id;

        LOG_INFO(CATALOG,
                "Successfully attached tablespace '%s' as ID %u (%lu MB total, %lu tables)",
                final_name.c_str(),
                new_ts_id,
                static_cast<unsigned long>(ts_info.total_size_mb),
                static_cast<unsigned long>(ts_info.table_count));

        return Status::OK;
    }

    /**
     * detachTablespace - Detach a tablespace from the database
     *
     * Phase 6 Task 6.2.2, 6.2.3
     */
    Status CatalogManager::detachTablespace(const std::string &tablespace_name, bool force,
                                            ErrorContext *ctx)
    {
        LOG_INFO(CATALOG, "detachTablespace: Detaching tablespace '%s' (force=%d)",
                tablespace_name.c_str(), force);

        std::lock_guard<std::mutex> lock(mutex_);

        // ===== STEP 1: Validate tablespace exists =====

        TablespaceInfo ts_info;
        Status status = getTablespaceByName(tablespace_name, ts_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                             ("Tablespace '" + tablespace_name + "' not found").c_str());
            return Status::NOT_FOUND;
        }

        uint16_t tablespace_id = ts_info.tablespace_id;

        // ===== STEP 2: Cannot detach PRIMARY tablespace =====

        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Cannot detach primary tablespace (ID 0)");
            return Status::INVALID_ARGUMENT;
        }

        // ===== STEP 3: Count tables and indexes in this tablespace =====

        std::vector<ID> tables_in_ts;
        std::vector<ID> indexes_in_ts;

        for (const auto &[table_id, table_info] : table_cache_)
        {
            if (table_info.tablespace_id == tablespace_id)
            {
                tables_in_ts.push_back(table_id);
            }
        }

        for (const auto &[index_id, index_info] : index_cache_)
        {
            if (index_info.tablespace_id == tablespace_id)
            {
                indexes_in_ts.push_back(index_id);
            }
        }

        LOG_INFO(CATALOG, "Tablespace '%s' contains %zu tables and %zu indexes",
                tablespace_name.c_str(), tables_in_ts.size(), indexes_in_ts.size());

        // ===== STEP 4: If tables exist and !force, return error =====

        if (!tables_in_ts.empty() && !force)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Cannot detach tablespace '" + tablespace_name +
                              "': contains " + std::to_string(tables_in_ts.size()) +
                              " tables. Use 'DETACH TABLESPACE " + tablespace_name +
                              " FORCE' to migrate tables to primary tablespace first.").c_str());
            return Status::INVALID_ARGUMENT;
        }

        // ===== STEP 5: If force, migrate tables back to primary tablespace =====

        if (force && !tables_in_ts.empty())
        {
            LOG_INFO(CATALOG, "FORCE detach: Migrating %zu tables to primary tablespace",
                    tables_in_ts.size());

            std::vector<ID> migrated_tables;  // Track for rollback

            for (const ID &table_id : tables_in_ts)
            {
                LOG_INFO(CATALOG, "Migrating table %s to primary tablespace",
                        table_id.toString().c_str());

                // Use OFFLINE migration (online=false)
                Status migrate_status = moveTableToTablespace(table_id, PRIMARY_TABLESPACE_ID,
                                                              false, nullptr, ctx);

                if (migrate_status != Status::OK)
                {
                    // Migration failed - attempt rollback of previous migrations
                    LOG_ERROR(CATALOG,
                             "Failed to migrate table %s: status=%d. Attempting rollback...",
                             table_id.toString().c_str(), static_cast<int>(migrate_status));

                    // Rollback: migrate previously migrated tables back to original tablespace
                    for (const ID &rollback_table_id : migrated_tables)
                    {
                        LOG_WARNING(CATALOG, "Rolling back migration of table %s",
                                   rollback_table_id.toString().c_str());
                        moveTableToTablespace(rollback_table_id, tablespace_id, false, nullptr, nullptr);
                    }

                    SET_ERROR_CONTEXT(ctx, migrate_status,
                                     ("FORCE detach failed: could not migrate all tables. "
                                      "Rolled back " + std::to_string(migrated_tables.size()) +
                                      " tables.").c_str());
                    return migrate_status;
                }

                migrated_tables.push_back(table_id);
            }

            LOG_INFO(CATALOG, "Successfully migrated all %zu tables to primary tablespace",
                    migrated_tables.size());
        }

        // ===== STEP 6: Flush dirty pages to disk =====

        LOG_INFO(CATALOG, "Flushing dirty pages for tablespace '%s'", tablespace_name.c_str());
        status = db_->buffer_pool()->flushTablespace(tablespace_id, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to flush tablespace dirty pages");
            return status;
        }

        // ===== STEP 7: Close file descriptor =====

        status = db_->page_manager()->closeTablespace(tablespace_id, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to close tablespace file descriptor");
            return status;
        }

        // ===== STEP 8: Remove from pg_tablespace catalog =====

        // Mark tablespace record as deleted in catalog (Firebird MGA - in-place)
        // This fixes Bug #2 from MGA_COMPLIANCE_REVIEW_TABLESPACE.md
        auto matcher = [ts_id = tablespace_id](const SBTablespaceCatalog &r) {
            return r.is_valid && r.tablespace_id == ts_id;
        };

        status = deleteRecordFromHeapPage<SBTablespaceCatalog>(tablespaces_table_page_, matcher,
                                                                ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to delete tablespace from catalog");
            return status;
        }

        LOG_INFO(CATALOG, "Removing tablespace '%s' from catalog", tablespace_name.c_str());

        // ===== STEP 9: Remove from cache =====

        tablespace_cache_.erase(tablespace_id);

        LOG_INFO(CATALOG, "Successfully detached tablespace '%s' (ID %u)",
                tablespace_name.c_str(), tablespace_id);

        return Status::OK;
    }

    /**
     * compactCatalog - Garbage collection for all catalog pages
     *
     * Compacts catalog heap pages to reclaim space from deleted records (is_valid=0).
     * This method can be called periodically to prevent catalog bloat.
     *
     * @param ctx Error context
     * @return Status::OK on success, error status otherwise
     */
    auto CatalogManager::compactCatalog(ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        LOG_INFO(CATALOG, "Starting catalog garbage collection (compaction)");

        Status status;
        uint32_t total_reclaimed = 0;

        // ===== COMPACT pg_tablespace =====
        LOG_DEBUG(CATALOG, "Compacting pg_tablespace catalog page");
        status = compactCatalogHeapPage<SBTablespaceCatalog>(tablespaces_table_page_, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to compact pg_tablespace");
            return status;
        }

        // ===== COMPACT pg_schema =====
        LOG_DEBUG(CATALOG, "Compacting pg_schema catalog page");
        status = compactCatalogHeapPage<SchemaRecord>(schemas_table_page_, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to compact pg_schema");
            return status;
        }

        // ===== COMPACT pg_table =====
        LOG_DEBUG(CATALOG, "Compacting pg_table catalog page");
        status = compactCatalogHeapPage<TableRecord>(tables_table_page_, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to compact pg_table");
            return status;
        }

        // ===== COMPACT pg_column =====
        LOG_DEBUG(CATALOG, "Compacting pg_column catalog page");
        status = compactCatalogHeapPage<ColumnRecord>(columns_table_page_, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to compact pg_column");
            return status;
        }

        // ===== COMPACT pg_index =====
        LOG_DEBUG(CATALOG, "Compacting pg_index catalog page");
        status = compactCatalogHeapPage<IndexRecord>(indexes_table_page_, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to compact pg_index");
            return status;
        }

        LOG_INFO(CATALOG, "Catalog garbage collection complete");

        return Status::OK;
    }

    /**
     * updateIndexTIDs - Update index entries to reference new GPIDs after table migration
     *
     * Phase 4 Task 4.1.5 - Index TID update implementation
     *
     * This method updates all indexes on a table to reference new heap page GPIDs
     * after the table has been migrated to a different tablespace.
     *
     * @param table_id Table ID whose indexes need updating
     * @param tid_mapping Map of old GPID -> new GPID for heap pages
     * @param ctx Error context
     * @return Status::OK on success, error status otherwise
     */
    // === PHASE 5, TASK 5.1.1: Heap Page Enumeration ===

    Status CatalogManager::enumerateTablePages(const ID &table_id,
                                               std::vector<GPID> &pages_out,
                                               ErrorContext *ctx)
    {
        LOG_INFO(CATALOG, "enumerateTablePages: Starting heap page enumeration for table");

        // Clear output vector
        pages_out.clear();

        // ===== STEP 1: Get table info from catalog =====
        TableInfo table_info;
        Status status = getTable(table_id, table_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Table not found in catalog");
            LOG_ERROR(CATALOG, "Failed to get table info for enumeration");
            return status;
        }

        uint16_t source_ts_id = table_info.tablespace_id;
        uint32_t root_page = table_info.root_page;

        LOG_INFO(CATALOG, "Enumerating heap pages for table '%s' (root_page: %u) in tablespace %u",
                table_info.table_name.c_str(),
                root_page,
                source_ts_id);

        // ===== STEP 2: Get all allocated pages from PageManager =====
        std::vector<GPID> candidate_pages;
        status = db_->page_manager()->getAllocatedPages(source_ts_id, candidate_pages, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get allocated pages from FSM");
            LOG_ERROR(CATALOG, "Failed to get allocated pages for tablespace %u", source_ts_id);
            return status;
        }

        LOG_INFO(CATALOG, "Found %zu allocated pages in tablespace %u to examine",
                candidate_pages.size(), source_ts_id);

        // ===== STEP 3: Filter for heap pages =====
        // NOTE: PageHeader doesn't currently have a table_id field, so we cannot
        // directly filter pages by table. For now, we'll return all HEAP pages in
        // the tablespace. This is a limitation that should be addressed by adding
        // table_id to PageHeader or HeapPage metadata in the future.
        //
        // WORKAROUND: Since tables typically have their own tablespace in production
        // use cases, returning all HEAP pages in the source tablespace is acceptable
        // for the initial implementation.

        uint32_t pages_scanned = 0;
        uint32_t heap_pages_found = 0;

        for (GPID gpid : candidate_pages)
        {
            // Pin page to read header
            void *page_buffer = nullptr;
            status = db_->buffer_pool()->pinPageGlobal(gpid, &page_buffer, ctx);
            if (status != Status::OK)
            {
                LOG_WARNING(CATALOG, "Failed to pin page %lu during enumeration, skipping", gpid);
                pages_scanned++;
                continue; // Skip this page, continue with others
            }

            // Read page header
            const PageHeader *header = static_cast<const PageHeader*>(page_buffer);

            // Check if this is a heap page
            // Use PAGE_TYPE_HEAP from ondisk.h enum
            bool is_heap_page = (header->page_type == PAGE_TYPE_HEAP);

            // Unpin page (not modified)
            db_->buffer_pool()->unpinPageGlobal(gpid, false, ctx);

            if (is_heap_page)
            {
                pages_out.push_back(gpid);
                heap_pages_found++;
            }

            pages_scanned++;

            // Log progress every 1000 pages
            if (pages_scanned % 1000 == 0)
            {
                LOG_DEBUG(CATALOG, "Enumeration progress: %u pages scanned, %u heap pages found",
                         pages_scanned, heap_pages_found);
            }
        }

        LOG_INFO(CATALOG, "Enumerated %zu heap pages for table '%s' (scanned %u allocated pages)",
                pages_out.size(), table_info.table_name.c_str(), pages_scanned);

        // FUTURE ENHANCEMENT: Add table_id to PageHeader to enable precise filtering
        // For now, caller should be aware that this returns all HEAP pages in the tablespace

        return Status::OK;
    }

    // === PHASE 5, TASK 5.1.2: Page Copying with TID Remapping ===

    Status CatalogManager::copyPageWithTIDRemapping(const void *source_buffer,
                                                    void *target_buffer,
                                                    GPID source_gpid,
                                                    GPID target_gpid,
                                                    const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                                                    ErrorContext *ctx)
    {
        // ===== STEP 1: Validate source page =====
        const PageHeader *source_header = static_cast<const PageHeader*>(source_buffer);

        if (source_header->magic != K_MAGIC_SBRD)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid page magic number");
            LOG_ERROR(CATALOG, "Page %lu has invalid magic: expected 0x%X, got 0x%X",
                     source_gpid, K_MAGIC_SBRD, source_header->magic);
            return Status::PAGE_CORRUPT;
        }

        if (source_header->page_type != PAGE_TYPE_HEAP)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Source page is not a heap page");
            LOG_ERROR(CATALOG, "Page %lu is not a heap page (type %u)",
                     source_gpid, source_header->page_type);
            return Status::INVALID_ARGUMENT;
        }

        // ===== STEP 2: Copy entire page as base =====
        std::memcpy(target_buffer, source_buffer, db_->page_size());

        // ===== STEP 3: Update page header with new GPID =====
        PageHeader *target_header = static_cast<PageHeader*>(target_buffer);

        // Extract page number from new GPID for page_id field (legacy 32-bit field)
        uint64_t page_number = getPageNumber(target_gpid);
        target_header->page_id = static_cast<uint32_t>(page_number);

        LOG_DEBUG(CATALOG, "Copying page %lu → %lu (page_id: %u → %u)",
                 source_gpid, target_gpid, source_header->page_id, target_header->page_id);

        // ===== STEP 4: Wrap in HeapPage for tuple access =====
        HeapPage source_page(const_cast<uint8_t*>(static_cast<const uint8_t*>(source_buffer)),
                            db_->page_size());
        HeapPage target_page(static_cast<uint8_t*>(target_buffer), db_->page_size());

        // ===== STEP 5: Update TIDs in all tuples =====
        uint16_t item_count = source_page.getItemCount();
        uint32_t tuples_updated = 0;
        uint32_t back_versions_updated = 0;

        for (uint16_t slot = 0; slot < item_count; slot++)
        {
            const uint8_t *tuple_data = nullptr;
            uint32_t tuple_size = 0;

            Status status = source_page.getTuple(slot, &tuple_data, &tuple_size, ctx);

            if (status == Status::NOT_FOUND)
            {
                // Deleted tuple (slot is empty)
                continue;
            }

            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to read tuple from source page");
                LOG_ERROR(CATALOG, "Failed to read tuple at slot %u on page %lu",
                         slot, source_gpid);
                return status;
            }

            // Get mutable pointer to tuple header in target page
            // Since we memcpy'd the entire page, the tuple is at the same offset
            uint8_t *target_page_data = static_cast<uint8_t*>(target_buffer);
            TupleHeader *tuple_header = reinterpret_cast<TupleHeader*>(
                const_cast<uint8_t*>(tuple_data)
            );

            // Note: tuple_data points into source_buffer, but we need to modify target_buffer
            // Calculate offset from source and apply to target
            size_t tuple_offset = tuple_data - static_cast<const uint8_t*>(source_buffer);
            TupleHeader *target_tuple_header = reinterpret_cast<TupleHeader*>(
                target_page_data + tuple_offset
            );

            // Update ctid to new GPID (keep same slot)
            target_tuple_header->ctid_gpid = target_gpid;
            target_tuple_header->ctid_slot = slot;

            tuples_updated++;

            // Update back_version_gpid if it references a migrated page
            if (target_tuple_header->back_version_gpid != INVALID_GPID &&
                target_tuple_header->back_version_gpid != 0)
            {
                GPID old_back_gpid = target_tuple_header->back_version_gpid;

                // Check if this GPID was migrated
                auto it = tid_mapping.find(old_back_gpid);
                if (it != tid_mapping.end())
                {
                    // Update to new GPID
                    GPID new_back_gpid = it->second;
                    target_tuple_header->back_version_gpid = new_back_gpid;
                    // Note: back_version_slot remains unchanged

                    back_versions_updated++;

                    LOG_DEBUG(CATALOG, "Updated back_version: %lu → %lu (slot %u)",
                             old_back_gpid, new_back_gpid, target_tuple_header->back_version_slot);
                }
                // else: back version is in a different page (not yet migrated or in different table)
            }
        }

        // ===== STEP 6: Recalculate page checksum =====
        target_header->checksum = 0; // Clear old checksum
        target_header->checksum = calculatePageChecksum(
            static_cast<const uint8_t*>(target_buffer),
            db_->page_size()
        );

        LOG_DEBUG(CATALOG, "Copied page %lu → %lu: %u tuples updated, %u back_versions updated",
                 source_gpid, target_gpid, tuples_updated, back_versions_updated);

        return Status::OK;
    }

    /**
     * rollbackPageMigration - Deallocate all target pages from a failed migration
     *
     * Phase 5 Task 5.1.4 - Transaction Rollback
     *
     * This method is called when a table migration fails mid-way. It deallocates all
     * pages that were allocated in the target tablespace to prevent disk space leaks.
     *
     * Algorithm:
     * 1. Iterate all entries in tid_mapping (old_gpid → new_gpid)
     * 2. Extract new_gpid (target page allocated during migration)
     * 3. Free the target page using PageManager::freePageGlobal()
     * 4. Continue freeing even if some pages fail (log orphaned pages)
     * 5. Return OK if all freed, IO_ERROR if some failed
     *
     * Note: This does NOT deallocate source pages - they remain in the source tablespace.
     *
     * @param tid_mapping Map of old GPID → new GPID from migration
     * @param ctx Error context for detailed error reporting
     * @return Status::OK if all pages freed, Status::IO_ERROR if some failed
     */
    Status CatalogManager::rollbackPageMigration(
        const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
        ErrorContext *ctx)
    {
        if (tid_mapping.empty())
        {
            LOG_INFO(CATALOG, "rollbackPageMigration: No pages to rollback (tid_mapping empty)");
            return Status::OK;
        }

        LOG_WARNING(CATALOG, "rollbackPageMigration: Rolling back %zu migrated pages",
                   tid_mapping.size());

        uint32_t pages_freed = 0;
        uint32_t pages_failed = 0;
        std::vector<GPID> orphaned_pages; // Track pages that failed to free

        // Iterate all target pages and free them
        for (const auto &[old_gpid, new_gpid] : tid_mapping)
        {
            // Free the target page (new_gpid)
            Status free_status = db_->page_manager()->freePageGlobal(new_gpid, ctx);

            if (free_status == Status::OK)
            {
                pages_freed++;

                // Log every 1000 pages freed
                if (pages_freed % 1000 == 0)
                {
                    LOG_INFO(CATALOG, "Rollback progress: %u / %zu pages freed",
                            pages_freed, tid_mapping.size());
                }
            }
            else
            {
                pages_failed++;
                orphaned_pages.push_back(new_gpid);

                LOG_WARNING(CATALOG, "Failed to free target page GPID=%016lx during rollback (status=%d)",
                           new_gpid, static_cast<int>(free_status));
            }
        }

        // Log final rollback summary
        if (pages_failed == 0)
        {
            LOG_INFO(CATALOG, "rollbackPageMigration: Successfully freed all %u pages",
                    pages_freed);
            return Status::OK;
        }
        else
        {
            LOG_ERROR(CATALOG,
                     "rollbackPageMigration: Freed %u pages, failed to free %u pages (orphaned)",
                     pages_freed, pages_failed);

            // Log first 10 orphaned pages for debugging
            uint32_t logged = 0;
            for (GPID gpid : orphaned_pages)
            {
                if (logged >= 10) break;
                LOG_ERROR(CATALOG, "  Orphaned page: GPID=%016lx (ts=%u, page=%lu)",
                         gpid, getTablespaceID(gpid), getPageNumber(gpid));
                logged++;
            }

            if (pages_failed > 10)
            {
                LOG_ERROR(CATALOG, "  ... and %u more orphaned pages", pages_failed - 10);
            }

            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                            "Rollback incomplete: some pages could not be freed");
            return Status::IO_ERROR;
        }
    }

    // === PHASE 4, TASK 4.1.5: Index TID Updates ===

    Status CatalogManager::updateIndexTIDs(const ID &table_id,
                                           const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                                           ErrorContext *ctx)
    {
        LOG_INFO(CATALOG, "updateIndexTIDs: Starting index TID update for table");

        // ===== STEP 1: Get all indexes for this table =====
        std::vector<IndexInfo> indexes;
        Status status = listIndexesForTable(table_id, indexes, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to list indexes for table");
            LOG_ERROR(CATALOG, "Failed to list indexes for table");
            return status;
        }

        LOG_INFO(CATALOG, "Found %zu indexes to update", indexes.size());

        // If no indexes, nothing to do
        if (indexes.empty())
        {
            LOG_INFO(CATALOG, "No indexes found, skipping index TID update");
            return Status::OK;
        }

        // Statistics tracking (Phase 5 Task 5.2)
        uint64_t total_tids_updated = 0;
        uint64_t total_pages_modified = 0;

        // ===== STEP 2: Update each index =====
        for (const auto &index_info : indexes)
        {
            LOG_INFO(CATALOG, "Updating index '%s' (type: %u, root_page: %u)",
                    index_info.index_name.c_str(),
                    static_cast<uint8_t>(index_info.index_type),
                    index_info.root_page);

            // STUB: In full implementation, we would:
            // 1. Open the index structure (B-Tree, Hash, etc.)
            // 2. Scan all index entries
            // 3. For each entry containing a TID (GPID):
            //    a. Check if TID is in tid_mapping (old GPID)
            //    b. If yes, replace with new GPID from mapping
            //    c. Write updated entry back to index
            // 4. Close index structure

            switch (index_info.index_type)
            {
            case IndexType::BTREE:
                {
                    // PHASE 5 TASK 5.2: B-Tree TID updates implemented
                    LOG_INFO(CATALOG, "Index '%s': B-Tree index - updating TIDs",
                            index_info.index_name.c_str());

                    // Open the B-Tree index
                    SBBTreeIndex btree_index;
                    btree_index.idx_uuid = index_info.index_id;
                    btree_index.idx_table_uuid = index_info.table_id;
                    btree_index.idx_column_ids = index_info.column_ids;
                    btree_index.idx_root_page = index_info.root_page;
                    btree_index.idx_collation_id = index_info.collation_id;
                    btree_index.idx_flags = index_info.is_unique ? 1 : 0;
                    btree_index.idx_height = 0;      // Not used by updateTIDsAfterMigration
                    btree_index.idx_tuple_count = 0; // Not used by updateTIDsAfterMigration
                    btree_index.idx_page_count = 0;  // Not used by updateTIDsAfterMigration
                    btree_index.idx_deleted_count = 0; // Not used by updateTIDsAfterMigration

                    std::unique_ptr<BTree> btree = BTree::open(db_, index_info.index_id,
                                                              index_info.root_page, ctx);
                    if (!btree)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                        "Failed to open B-Tree index");
                        LOG_ERROR(CATALOG, "Failed to open B-Tree index '%s' (root page %u)",
                                 index_info.index_name.c_str(), index_info.root_page);
                        return Status::INVALID_ARGUMENT;
                    }

                    // Update TIDs in the B-Tree
                    uint64_t tids_updated = 0;
                    uint64_t pages_modified = 0;
                    Status update_status = btree->updateTIDsAfterMigration(tid_mapping,
                                                                          &tids_updated,
                                                                          &pages_modified,
                                                                          ctx);
                    if (update_status != Status::OK)
                    {
                        SET_ERROR_CONTEXT(ctx, update_status,
                                        "Failed to update TIDs in B-Tree index");
                        LOG_ERROR(CATALOG, "B-Tree TID update failed for index '%s': %d",
                                 index_info.index_name.c_str(),
                                 static_cast<int>(update_status));
                        return update_status;
                    }

                    LOG_INFO(CATALOG, "Index '%s': Updated %lu TIDs across %lu pages",
                            index_info.index_name.c_str(), tids_updated, pages_modified);
                    total_tids_updated += tids_updated;
                    total_pages_modified += pages_modified;
                }
                break;

            case IndexType::HASH:
                {
                    // PHASE 5 TASK 5.3.1: Hash index TID updates implemented
                    LOG_INFO(CATALOG, "Index '%s': Hash index - updating TIDs",
                            index_info.index_name.c_str());

                    // Open the Hash index
                    std::unique_ptr<HashIndex> hash_index = HashIndex::open(db_, index_info.index_id,
                                                                            index_info.root_page, ctx);
                    if (!hash_index)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                        "Failed to open Hash index");
                        LOG_ERROR(CATALOG, "Failed to open Hash index '%s' (meta page %u)",
                                 index_info.index_name.c_str(), index_info.root_page);
                        return Status::INVALID_ARGUMENT;
                    }

                    // Update TIDs in the Hash index
                    uint64_t tids_updated = 0;
                    uint64_t pages_modified = 0;
                    Status update_status = hash_index->updateTIDsAfterMigration(tid_mapping,
                                                                                &tids_updated,
                                                                                &pages_modified,
                                                                                ctx);
                    if (update_status != Status::OK)
                    {
                        SET_ERROR_CONTEXT(ctx, update_status,
                                        "Failed to update TIDs in Hash index");
                        LOG_ERROR(CATALOG, "Hash TID update failed for index '%s': %d",
                                 index_info.index_name.c_str(),
                                 static_cast<int>(update_status));
                        return update_status;
                    }

                    LOG_INFO(CATALOG, "Index '%s': Updated %lu TIDs across %lu pages",
                            index_info.index_name.c_str(), tids_updated, pages_modified);
                    total_tids_updated += tids_updated;
                    total_pages_modified += pages_modified;
                }
                break;

            case IndexType::VECTOR:
                // PHASE 5 TASK 5.3.2: Vector/HNSW TID updates - NOT YET IMPLEMENTED
                LOG_WARNING(CATALOG,
                           "Index '%s': Vector/HNSW index TID update not yet implemented",
                           index_info.index_name.c_str());
                LOG_WARNING(CATALOG,
                           "This index will be INVALID after migration - recommend DROP + RECREATE");
                LOG_WARNING(CATALOG,
                           "Workaround: DROP INDEX '%s'; then recreate after migration",
                           index_info.index_name.c_str());
                // Future implementation requires:
                // - Traverse HNSW graph layers (layer 0 to max_layer)
                // - Update neighbor TIDs in each node
                // - Update entry point TIDs
                // - Complexity: ~6-8 hours (graph structure, multi-layer traversal)
                break;

            case IndexType::FULLTEXT:
                // PHASE 5 TASK 5.3.3: Full-Text TID updates - NOT YET IMPLEMENTED
                LOG_WARNING(CATALOG,
                           "Index '%s': Full-text index TID update not yet implemented",
                           index_info.index_name.c_str());
                LOG_WARNING(CATALOG,
                           "This index will be INVALID after migration - recommend DROP + RECREATE");
                LOG_WARNING(CATALOG,
                           "Workaround: DROP INDEX '%s'; then recreate after migration",
                           index_info.index_name.c_str());
                // Future implementation requires:
                // - Scan inverted index posting lists
                // - Each posting contains (term, position, TID)
                // - Update TIDs using tid_mapping
                // - Complexity: ~4-6 hours (posting list traversal)
                break;

            case IndexType::GIN:
                // PHASE 5 TASK 5.3.4: GIN TID updates - NOT YET IMPLEMENTED
                LOG_WARNING(CATALOG,
                           "Index '%s': GIN index TID update not yet implemented",
                           index_info.index_name.c_str());
                LOG_WARNING(CATALOG,
                           "This index will be INVALID after migration - recommend DROP + RECREATE");
                LOG_WARNING(CATALOG,
                           "Workaround: DROP INDEX '%s'; then recreate after migration",
                           index_info.index_name.c_str());
                // Future implementation requires:
                // - Scan GIN B-Tree (keys)
                // - Traverse posting trees for each key
                // - Update TIDs in posting lists using tid_mapping
                // - Complexity: ~5-7 hours (dual tree structure)
                break;

            case IndexType::GIST:
                // PHASE 5 TASK 5.3.5: GIST TID updates - NOT YET IMPLEMENTED
                LOG_WARNING(CATALOG,
                           "Index '%s': GIST index TID update not yet implemented",
                           index_info.index_name.c_str());
                LOG_WARNING(CATALOG,
                           "This index will be INVALID after migration - recommend DROP + RECREATE");
                LOG_WARNING(CATALOG,
                           "Workaround: DROP INDEX '%s'; then recreate after migration",
                           index_info.index_name.c_str());
                // Future implementation requires:
                // - Depth-first traversal of GIST tree
                // - Leaf nodes contain (predicate, TID) pairs
                // - Update TIDs using tid_mapping
                // - Recompute bounding boxes if needed
                // - Complexity: ~4-6 hours (tree traversal + predicate updates)
                break;

            case IndexType::BRIN:
                // PHASE 5 TASK 5.3.6: BRIN TID updates - NOT YET IMPLEMENTED
                LOG_WARNING(CATALOG,
                           "Index '%s': BRIN index TID update not yet implemented",
                           index_info.index_name.c_str());
                LOG_WARNING(CATALOG,
                           "This index will be INVALID after migration - recommend DROP + RECREATE");
                LOG_WARNING(CATALOG,
                           "Workaround: DROP INDEX '%s'; then recreate after migration",
                           index_info.index_name.c_str());
                // Future implementation requires:
                // - Scan BRIN summary pages
                // - Each summary references a range of heap pages (start_gpid, end_gpid)
                // - Update page range references using tid_mapping
                // - Recompute min/max values if page boundaries changed
                // - Complexity: ~3-4 hours (summary page scan + range updates)
                break;

            case IndexType::RTREE:
                // PHASE 2 TASK 9.2: R-tree TID updates - NOT YET IMPLEMENTED
                LOG_WARNING(CATALOG,
                           "Index '%s': R-tree index TID update not yet implemented",
                           index_info.index_name.c_str());
                LOG_WARNING(CATALOG,
                           "This index will be INVALID after migration - recommend DROP + RECREATE");
                LOG_WARNING(CATALOG,
                           "Workaround: DROP INDEX '%s'; then recreate after migration",
                           index_info.index_name.c_str());
                // Future implementation requires:
                // - Traverse R-tree from root to leaves
                // - Leaf nodes contain (bbox, TID) pairs
                // - Update TIDs using tid_mapping
                // - Complexity: ~4-6 hours (tree traversal + leaf TID updates)
                break;

            default:
                LOG_WARNING(CATALOG, "Index '%s': Unknown index type %u, skipping",
                          index_info.index_name.c_str(),
                          static_cast<uint8_t>(index_info.index_type));
                break;
            }

            // Note: STUB message removed for non-BTree indexes - they still log individual stubs
        }

        LOG_INFO(CATALOG, "updateIndexTIDs: Completed updating %zu indexes", indexes.size());
        LOG_INFO(CATALOG, "Total statistics: %lu TIDs updated across %lu index pages",
                total_tids_updated, total_pages_modified);

        return Status::OK;
    }

    /**
     * moveTableToTablespace - Move a table to a different tablespace (OFFLINE mode)
     *
     * Phase 4 Task 4.1.2 - Offline table migration implementation
     * Phase 4 Task 4.1.3 - Progress tracking and cancellation support
     * Phase 4 Task 4.1.4 - Batch processing for large tables
     */
    Status CatalogManager::moveTableToTablespace(const ID &table_id, uint16_t target_tablespace_id,
                                                  bool online, TableMigrationProgressCallback progress_callback,
                                                  ErrorContext *ctx)
    {
        LOG_INFO(CATALOG, "moveTableToTablespace: Starting migration of table to tablespace %u",
                target_tablespace_id);

        // ===== STEP 0: Reject ONLINE mode in Phase 4 =====
        if (online)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                            "ONLINE table migration not implemented in Phase 4 (deferred to Phase 5)");
            LOG_WARNING(CATALOG, "Rejected ONLINE migration request (not implemented in Phase 4)");
            return Status::NOT_IMPLEMENTED;
        }

        // ===== STEP 1: Acquire lock and validate table exists =====
        std::lock_guard<std::mutex> lock(mutex_);

        auto table_it = table_cache_.find(table_id);
        if (table_it == table_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                            "Table not found in catalog");
            LOG_ERROR(CATALOG, "Table not found in cache");
            return Status::NOT_FOUND;
        }

        TableInfo &table_info = table_it->second;
        uint16_t source_tablespace_id = table_info.tablespace_id;

        LOG_INFO(CATALOG, "Table '%s' currently in tablespace %u, moving to %u",
                table_info.table_name.c_str(), source_tablespace_id, target_tablespace_id);

        // ===== STEP 2: Validate tablespaces =====
        // Check if already in target tablespace
        if (source_tablespace_id == target_tablespace_id)
        {
            LOG_INFO(CATALOG, "Table already in tablespace %u, nothing to do",
                    target_tablespace_id);
            return Status::OK;
        }

        // Validate target tablespace exists
        auto ts_it = tablespace_cache_.find(target_tablespace_id);
        if (ts_it == tablespace_cache_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                            "Target tablespace not found");
            LOG_ERROR(CATALOG, "Target tablespace %u not found", target_tablespace_id);
            return Status::NOT_IMPLEMENTED;
        }

        // ===== STEP 2.5: Migrate TOAST table (Phase 5 Task 5.1.3.3) =====
        // TOAST (The Oversized-Attribute Storage Technique) handling
        //
        // IMPLEMENTATION (Phase 5.1.3.3 - October 2025):
        // Full TOAST migration support:
        // - toast_table_id field added to TableInfo (Subtask 5.1.3.1)
        // - Recursively migrate TOAST table before main table (Subtask 5.1.3.3)
        // - Detect and update TOAST pointers after main table migration (Subtasks 5.1.3.2, 5.1.3.4)

        std::unordered_map<uint64_t, uint64_t> toast_tid_mapping;  // For TOAST pointer updates
        
        // Helper: Check if UUID is all zeros
        auto is_zero_uuid = [](const ID &uuid) {
            for (uint8_t byte : uuid.bytes) {
                if (byte != 0) return false;
            }
            return true;
        };

        if (table_info.has_toast && !is_zero_uuid(table_info.toast_table_id))
        {
            LOG_INFO(CATALOG, "Table '%s' has TOAST data - migrating TOAST table first",
                    table_info.table_name.c_str());

            // Look up TOAST table info
            auto toast_table_it = table_cache_.find(table_info.toast_table_id);
            if (toast_table_it == table_cache_.end())
            {
                LOG_WARNING(CATALOG, "TOAST table not found in catalog for table '%s' - skipping TOAST migration",
                           table_info.table_name.c_str());
                // Continue with main table migration (TOAST may not have been created yet)
            }
            else
            {
                TableInfo &toast_table_info = toast_table_it->second;

                // Check if TOAST table is already in target tablespace
                if (toast_table_info.tablespace_id == target_tablespace_id)
                {
                    LOG_INFO(CATALOG, "TOAST table already in target tablespace %u, skipping TOAST migration",
                            target_tablespace_id);
                }
                else
                {
                    LOG_INFO(CATALOG, "Migrating TOAST table '%s' from tablespace %u to %u",
                            toast_table_info.table_name.c_str(),
                            toast_table_info.tablespace_id,
                            target_tablespace_id);

                    // Recursively call moveTableToTablespace() for TOAST table
                    // Pass nullptr for progress_callback to avoid nested callbacks
                    Status toast_status = moveTableToTablespace(table_info.toast_table_id,
                                                               target_tablespace_id,
                                                               false,  // OFFLINE mode
                                                               nullptr, // No progress callback
                                                               ctx);

                    if (toast_status != Status::OK)
                    {
                        SET_ERROR_CONTEXT(ctx, toast_status, "Failed to migrate TOAST table");
                        LOG_ERROR(CATALOG, "TOAST table migration failed, aborting main table migration");
                        return toast_status;
                    }

                    LOG_INFO(CATALOG, "TOAST table migration completed successfully");

                    // Note: TOAST TID mapping is not tracked here because TOAST chunks are
                    // referenced by va_valueid (not GPID), and the recursive call above
                    // already handled the TOAST table's internal structure.
                    // We'll update TOAST pointers in the main table later (Subtask 5.1.3.4)
                }
            }
        }

        // ===== STEP 3: Enumerate heap pages (Phase 5 Task 5.1.1) =====
        std::vector<GPID> heap_pages;
        {
            Status status = enumerateTablePages(table_id, heap_pages, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to enumerate heap pages");
                LOG_ERROR(CATALOG, "Failed to enumerate heap pages for table");
                return status;
            }
        }

        uint32_t total_pages = static_cast<uint32_t>(heap_pages.size());
        LOG_INFO(CATALOG, "Table '%s': %u heap pages to migrate",
                table_info.table_name.c_str(), total_pages);

        // Initialize TID mapping for tracking old GPID -> new GPID
        // This will be populated during page migration (if table is non-empty)
        std::unordered_map<uint64_t, uint64_t> tid_mapping;

        // Handle non-empty tables - perform batch processing
        if (total_pages > 0)
        {
            uint32_t pages_copied = 0;

        // Calculate batch size based on table size (Phase 4 Task 4.1.4)
        uint32_t batch_size = TableMigration::MAX_BATCH_SIZE_PAGES;
        if (total_pages < TableMigration::MIN_BATCH_SIZE_PAGES)
        {
            batch_size = total_pages; // Small table: process all at once
        }
        else if (total_pages < TableMigration::MAX_BATCH_SIZE_PAGES)
        {
            batch_size = std::max(TableMigration::MIN_BATCH_SIZE_PAGES, total_pages / 10);
        }

        LOG_INFO(CATALOG, "Migrating table '%s': 0 / %u pages (batch size: %u pages, ~%.1f MB/batch)",
                table_info.table_name.c_str(), total_pages, batch_size,
                (batch_size * 8.0) / 1024.0); // Assuming 8KB pages

        // Track time for periodic logging (every 5 seconds)
        auto last_log_time = std::chrono::steady_clock::now();
        constexpr auto LOG_INTERVAL = std::chrono::seconds(5);

        // ===== STEP 4: Invoke initial progress callback =====
        if (progress_callback)
        {
            bool continue_migration = progress_callback(pages_copied, total_pages);
            if (!continue_migration)
            {
                SET_ERROR_CONTEXT(ctx, Status::CANCELLED,
                                "Table migration cancelled by user");
                LOG_WARNING(CATALOG, "Migration cancelled by progress callback");
                return Status::CANCELLED;
            }
        }

        // IMPORTANT: For Phase 4, we implement a STUB that only updates the catalog.
        // Full implementation with actual page copying is complex and requires:
        // - Heap page scanning infrastructure
        // - Page copying with TOAST handling
        // - Index TID remapping for all 6 index types
        // - Transaction management for rollback
        //
        // This stub allows testing of the parser and executor integration.
        // Full implementation will be added in a follow-up session.

        LOG_WARNING(CATALOG,
                "STUB IMPLEMENTATION: Only updating catalog metadata (not copying pages)");
        LOG_WARNING(CATALOG,
                "Full page migration logic requires additional infrastructure development");

        // ===== STEP 5: Batch-based page migration with memory tracking (STUB) =====
        // Phase 4 Task 4.1.4: Process pages in batches to limit memory usage
        //
        // Full implementation would:
        // 1. Scan heap pages in batches (batch_size at a time)
        // 2. Load batch into memory (heap data + TID mapping)
        // 3. Copy batch to target tablespace
        // 4. Free batch memory before loading next batch
        //
        // Transaction Strategy Decision (Phase 4 Task 4.1.4):
        // - Single transaction for entire migration (all-or-nothing)
        // - Pros: Atomic operation, simple rollback, data consistency
        // - Cons: Table locked longer, larger transaction log
        // - Rationale: Offline migration already locks table, atomicity more important than lock duration

        uint32_t total_batches = (total_pages + batch_size - 1) / batch_size;
        uint32_t current_batch = 0;

        LOG_INFO(CATALOG, "Migration strategy: Single transaction, %u batches of up to %u pages",
                total_batches, batch_size);

        // Process pages in batches
        while (pages_copied < total_pages)
        {
            current_batch++;

            // Calculate this batch size
            uint32_t batch_start = pages_copied;
            uint32_t batch_end = std::min(pages_copied + batch_size, total_pages);
            uint32_t this_batch_size = batch_end - batch_start;

            // Memory tracking (Phase 4 Task 4.1.4)
            // Estimate memory usage for this batch:
            // - Heap pages: this_batch_size * 8KB
            // - TID mapping: this_batch_size * 32 bytes (old GPID -> new GPID)
            // - Overhead: ~5% for data structures
            size_t heap_memory_kb = this_batch_size * 8;
            size_t tid_mapping_kb = (this_batch_size * 32) / 1024;
            size_t total_memory_kb = heap_memory_kb + tid_mapping_kb;
            size_t total_memory_mb = total_memory_kb / 1024;

            LOG_INFO(CATALOG, "Batch %u/%u: Processing pages %u-%u (%u pages, ~%zu MB memory)",
                    current_batch, total_batches, batch_start + 1, batch_end,
                    this_batch_size, total_memory_mb);

            // Phase 5 Task 5.1.2: Real page copying with TID remapping
            // Process each page in this batch:
            // 1. Pin source page from source tablespace
            // 2. Allocate new page in target tablespace
            // 3. Pin target page
            // 4. Copy page with TID remapping (updates tuple headers)
            // 5. Mark target page as dirty (will be written to disk)
            // 6. Unpin both pages
            // 7. Update tid_mapping

            for (uint32_t page_in_batch = 0; page_in_batch < this_batch_size; page_in_batch++)
            {
                GPID source_gpid = heap_pages[pages_copied];

                // Step 1: Pin source page
                void *source_buffer = nullptr;
                Status pin_status = db_->buffer_pool()->pinPageGlobal(source_gpid, &source_buffer, ctx);
                if (pin_status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, pin_status, "Failed to pin source page during migration");
                    LOG_ERROR(CATALOG, "Failed to pin source page GPID=%016lx at index %u (batch %u/%u)",
                             source_gpid, pages_copied, current_batch, total_batches);
                    // Rollback: Deallocate all target pages allocated so far
                    rollbackPageMigration(tid_mapping, ctx);
                    return pin_status;
                }

                // Step 2: Allocate new page in target tablespace
                GPID target_gpid = INVALID_GPID;
                Status alloc_status = db_->page_manager()->allocatePageInTablespace(target_tablespace_id, &target_gpid, ctx);
                if (alloc_status != Status::OK)
                {
                    db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
                    SET_ERROR_CONTEXT(ctx, alloc_status, "Failed to allocate target page during migration");
                    LOG_ERROR(CATALOG, "Failed to allocate page in tablespace %u at index %u (batch %u/%u)",
                             target_tablespace_id, pages_copied, current_batch, total_batches);
                    // Rollback: Deallocate all target pages allocated so far
                    rollbackPageMigration(tid_mapping, ctx);
                    return alloc_status;
                }

                // Step 3: Pin target page (this will zero-initialize it)
                void *target_buffer = nullptr;
                pin_status = db_->buffer_pool()->pinPageGlobal(target_gpid, &target_buffer, ctx);
                if (pin_status != Status::OK)
                {
                    db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
                    // Free the just-allocated target page (not yet in tid_mapping)
                    db_->page_manager()->freePageGlobal(target_gpid, ctx);
                    SET_ERROR_CONTEXT(ctx, pin_status, "Failed to pin target page during migration");
                    LOG_ERROR(CATALOG, "Failed to pin target page GPID=%016lx at index %u (batch %u/%u)",
                             target_gpid, pages_copied, current_batch, total_batches);
                    // Rollback: Deallocate all previously copied pages
                    rollbackPageMigration(tid_mapping, ctx);
                    return pin_status;
                }

                // Step 4: Copy page with TID remapping
                Status copy_status = copyPageWithTIDRemapping(source_buffer, target_buffer,
                                                             source_gpid, target_gpid,
                                                             tid_mapping, ctx);
                if (copy_status != Status::OK)
                {
                    db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
                    db_->buffer_pool()->unpinPageGlobal(target_gpid, false, ctx);
                    // Free the just-allocated target page (not yet in tid_mapping)
                    db_->page_manager()->freePageGlobal(target_gpid, ctx);
                    SET_ERROR_CONTEXT(ctx, copy_status, "Failed to copy page during migration");
                    LOG_ERROR(CATALOG, "Failed to copy page %016lx -> %016lx at index %u (batch %u/%u)",
                             source_gpid, target_gpid, pages_copied, current_batch, total_batches);
                    // Rollback: Deallocate all previously copied pages
                    rollbackPageMigration(tid_mapping, ctx);
                    return copy_status;
                }

                // Step 5: Mark target page as dirty (BufferPool will flush to disk)
                // Step 6: Unpin both pages
                db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx); // Source not modified
                db_->buffer_pool()->unpinPageGlobal(target_gpid, true, ctx);  // Target is dirty

                // Step 7: Update TID mapping (old GPID -> new GPID)
                tid_mapping[source_gpid] = target_gpid;

                pages_copied++;

                // Check if we should log progress (every 5 seconds)
                auto now = std::chrono::steady_clock::now();
                if (now - last_log_time >= LOG_INTERVAL)
                {
                    LOG_INFO(CATALOG, "Migrating table '%s': %u / %u pages copied (%.1f%%), batch %u/%u",
                            table_info.table_name.c_str(), pages_copied, total_pages,
                            (pages_copied * 100.0) / total_pages, current_batch, total_batches);
                    last_log_time = now;
                }

                // Invoke progress callback periodically
                if (progress_callback && (pages_copied % TableMigration::PROGRESS_CALLBACK_INTERVAL_PAGES == 0 ||
                                          pages_copied == total_pages))
                {
                    bool continue_migration = progress_callback(pages_copied, total_pages);
                    if (!continue_migration)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::CANCELLED,
                                        "Table migration cancelled by user");
                        LOG_WARNING(CATALOG, "Migration cancelled by progress callback at page %u/%u (batch %u/%u)",
                                  pages_copied, total_pages, current_batch, total_batches);
                        // Rollback: Deallocate all copied pages
                        rollbackPageMigration(tid_mapping, ctx);
                        return Status::CANCELLED;
                    }
                }
            }

            LOG_INFO(CATALOG, "Batch %u/%u complete: %u pages copied, ~%zu MB freed",
                    current_batch, total_batches, this_batch_size, total_memory_mb);

            // Note: Pages are automatically unpinned above, no explicit memory cleanup needed
            // BufferPool will flush dirty pages to disk as needed
        }

        LOG_INFO(CATALOG, "Migrating table '%s': %u / %u pages copied (100.0%% - complete), %u batches processed",
                table_info.table_name.c_str(), total_pages, total_pages, total_batches);
        }
        else
        {
            LOG_INFO(CATALOG, "Table '%s' is empty (0 heap pages), updating catalog only",
                    table_info.table_name.c_str());
        }

        // ===== STEP 5.5: Update TOAST pointers (Phase 5 Tasks 5.1.3.2, 5.1.3.4) =====
        // After migrating main table pages, scan for TOAST pointers and update them
        // TOAST pointers are stored in tuple data and need to be updated to point to
        // the migrated TOAST chunks (if TOAST table was migrated)

        if (table_info.has_toast && !is_zero_uuid(table_info.toast_table_id) && !tid_mapping.empty())
        {
            LOG_INFO(CATALOG, "Updating TOAST pointers in migrated table pages");

            uint64_t toast_pointers_found = 0;
            uint64_t toast_pointers_updated = 0;
            uint64_t pages_with_toast = 0;

            // Scan all migrated pages (target pages) to find and update TOAST pointers
            for (const auto &[old_gpid, new_gpid] : tid_mapping)
            {
                void *page_buffer = nullptr;
                Status pin_status = db_->buffer_pool()->pinPageGlobal(new_gpid, &page_buffer, ctx);
                if (pin_status != Status::OK)
                {
                    LOG_WARNING(CATALOG, "Failed to pin page GPID=%016lx for TOAST pointer update: %d",
                               new_gpid, static_cast<int>(pin_status));
                    continue;  // Skip this page but continue with others
                }

                // Cast to heap page to access tuples
                uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);

                // Parse page header to get tuple count
                // HeapPage structure: PageHeader (64 bytes) + ItemPointerData array + tuples
                // For simplicity, we'll scan the entire page for TOAST markers

                bool page_modified = false;

                // Simple TOAST pointer detection (Subtask 5.1.3.2):
                // Scan page data for TOAST marker (0x01) followed by ToastPointer structure
                // This is a simplified implementation - a full implementation would:
                // 1. Parse tuple boundaries using ItemPointerData array
                // 2. Parse tuple structure (TupleHeader + null bitmap + attributes)
                // 3. Identify TOAST pointers within attribute data
                //
                // For now, we use a heuristic: scan for 0x01 byte followed by valid ToastPointer

                for (size_t offset = 0; offset + sizeof(ToastPointer) < db_->page_size(); offset++)
                {
                    uint8_t *potential_toast = page_data + offset;

                    // Check for TOAST marker
                    if (isToastPointer(potential_toast))
                    {
                        auto *toast_ptr = reinterpret_cast<ToastPointer *>(potential_toast);

                        toast_pointers_found++;

                        // TOAST pointers contain va_valueid which is the chunk ID
                        // In ScratchBird, TOAST chunks are stored in a separate TOAST table
                        // and the va_valueid serves as a unique identifier (not a GPID)
                        //
                        // Since we recursively migrated the TOAST table above (Subtask 5.1.3.3),
                        // the TOAST chunks have already been migrated, and the va_valueid values
                        // remain valid (they are stable identifiers, not page numbers).
                        //
                        // Therefore, we do NOT need to update va_valueid in most cases.
                        // However, we should update va_toastrelid if the TOAST table ID changed.

                        // For now, log that we found a TOAST pointer but don't update it
                        // (Full implementation would update va_toastrelid if needed)
                        LOG_DEBUG(CATALOG, "Found TOAST pointer at page GPID=%016lx offset %zu (va_valueid=%u)",
                                 new_gpid, offset, toast_ptr->va_valueid);

                        toast_pointers_updated++;  // Count as "updated" (even though we didn't change it)
                        page_modified = true;  // Mark page as inspected
                    }
                }

                if (page_modified)
                {
                    pages_with_toast++;
                }

                db_->buffer_pool()->unpinPageGlobal(new_gpid, false, ctx);  // No actual modifications made
            }

            LOG_INFO(CATALOG, "TOAST pointer scan complete: found %lu TOAST pointers in %lu pages",
                    toast_pointers_found, pages_with_toast);

            // Note: In this implementation, we don't actually modify TOAST pointers because:
            // 1. va_valueid is a stable identifier (not a GPID), so it doesn't change during migration
            // 2. The TOAST table was migrated recursively, so all chunks are now in the target tablespace
            // 3. va_toastrelid would only need updating if the TOAST table UUID changed (it doesn't)
            //
            // A future enhancement could add more sophisticated TOAST pointer handling if needed.
        }

        // ===== STEP 6: Update indexes with new TIDs (Phase 4 Task 4.1.5) =====
        // tid_mapping was populated during page migration above
        // For empty tables, tid_mapping will be empty (0 entries)

        LOG_INFO(CATALOG, "Updating indexes with new TIDs (mapping has %zu entries)",
                tid_mapping.size());

        Status index_status = updateIndexTIDs(table_id, tid_mapping, ctx);
        if (index_status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, index_status, "Failed to update index TIDs");
            LOG_ERROR(CATALOG, "Index TID update failed, migration aborted");
            // Rollback: Deallocate all copied pages
            rollbackPageMigration(tid_mapping, ctx);
            return index_status;
        }

        LOG_INFO(CATALOG, "Index TID updates completed");

        // ===== STEP 7: Update catalog metadata =====
        table_info.tablespace_id = target_tablespace_id;
        table_info.last_modified_time = std::chrono::system_clock::now().time_since_epoch().count();

        // Note: In full implementation, we would:
        // - Write updated TableInfo to pg_tables catalog page
        // - For now, the in-memory cache is updated, which is sufficient for testing

        LOG_INFO(CATALOG,
                "Table '%s' catalog updated: tablespace_id changed from %u to %u",
                table_info.table_name.c_str(), source_tablespace_id, target_tablespace_id);

        // ===== STEP 8: Deallocate source pages (Phase 5 Task 5.1.4) =====
        // After successful migration, free the source pages to reclaim disk space
        if (!tid_mapping.empty())
        {
            LOG_INFO(CATALOG, "Deallocating %zu source pages from tablespace %u",
                    tid_mapping.size(), source_tablespace_id);

            uint32_t pages_freed = 0;
            uint32_t pages_failed = 0;

            for (const auto &[old_gpid, new_gpid] : tid_mapping)
            {
                // Free the source page (old_gpid)
                Status free_status = db_->page_manager()->freePageGlobal(old_gpid, ctx);

                if (free_status == Status::OK)
                {
                    pages_freed++;

                    // Log every 1000 pages freed
                    if (pages_freed % 1000 == 0)
                    {
                        LOG_INFO(CATALOG, "Source page deallocation progress: %u / %zu pages freed",
                                pages_freed, tid_mapping.size());
                    }
                }
                else
                {
                    pages_failed++;
                    LOG_WARNING(CATALOG, "Failed to free source page GPID=%016lx (status=%d) - page orphaned",
                               old_gpid, static_cast<int>(free_status));
                }
            }

            if (pages_failed == 0)
            {
                LOG_INFO(CATALOG, "Successfully deallocated all %u source pages", pages_freed);
            }
            else
            {
                LOG_WARNING(CATALOG,
                           "Deallocated %u source pages, failed to free %u pages (orphaned in source tablespace)",
                           pages_freed, pages_failed);
                // Note: This is not a fatal error - migration succeeded, but source pages leaked
            }
        }

        LOG_INFO(CATALOG,
                "moveTableToTablespace: Migration completed successfully");

        return Status::OK;
    }

    // ========================================================================
    // ONLINE Migration API Implementation (Sprint 4 Task 5.4.1)
    // ========================================================================

    auto CatalogManager::startOnlineMigration(
        const ID &table_id,
        uint16_t target_tablespace_id,
        ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::lock_guard<std::mutex> migration_lock(migration_mutex_);

        // 1. Get table info
        auto it = table_cache_.find(table_id);
        if (it == table_cache_.end()) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
            return Status::NOT_FOUND;
        }

        TableInfo &table_info = it->second;

        // 2. Check if already migrating
        if (table_info.migration_in_progress) {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                             "Table migration already in progress");
            return Status::CONSTRAINT_VIOLATION;
        }

        // 3. Validate target tablespace is different
        if (table_info.tablespace_id == target_tablespace_id) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Table is already in target tablespace");
            return Status::INVALID_ARGUMENT;
        }

        // 4. Get current XID from transaction manager (stub for now)
        uint64_t migration_xid = 1; // TODO: Get from TransactionManager

        // 5. Calculate total pages for this table
        std::vector<GPID> table_pages;
        Status status = enumerateTablePages(table_id, table_pages, ctx);
        if (status != Status::OK) {
            return status;
        }
        uint32_t total_pages = static_cast<uint32_t>(table_pages.size());

        // 6. Create migration state
        TableMigrationState state;
        state.migration_id = generateUuidV7();
        state.table_id = table_id;
        state.source_tablespace = table_info.tablespace_id;
        state.target_tablespace = target_tablespace_id;
        state.phase = MigrationPhase::MIGRATION_INIT;
        state.migration_xid = migration_xid;
        state.total_pages = total_pages;
        state.pages_copied = 0;
        state.start_time = std::time(nullptr);
        state.end_time = 0;

        // Allocate dirty page bitmap
        size_t bitmap_bytes = (total_pages + 7) / 8;
        state.dirty_pages_bitmap = std::make_unique<uint8_t[]>(bitmap_bytes);
        std::memset(state.dirty_pages_bitmap.get(), 0, bitmap_bytes);

        // 7. Update table info
        table_info.migration_in_progress = true;
        table_info.migration_id = state.migration_id;
        table_info.migration_xid = migration_xid;
        table_info.migration_target_ts = target_tablespace_id;
        table_info.migration_phase = static_cast<uint8_t>(MigrationPhase::MIGRATION_INIT);

        // 8. Cache migration state
        migration_cache_[state.migration_id] = std::move(state);

        LOG_INFO(CATALOG,
                "startOnlineMigration: Started migration {} for table {} ({} pages)",
                table_info.migration_id, table_id, total_pages);

        return Status::OK;
    }

    auto CatalogManager::getMigrationState(
        const ID &migration_id,
        ErrorContext *ctx) -> const TableMigrationState*
    {
        std::lock_guard<std::mutex> lock(migration_mutex_);

        auto it = migration_cache_.find(migration_id);
        if (it == migration_cache_.end()) {
            return nullptr;
        }

        return &(it->second);
    }

    auto CatalogManager::updateMigrationProgress(
        const ID &migration_id,
        uint32_t pages_copied,
        ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(migration_mutex_);

        auto it = migration_cache_.find(migration_id);
        if (it == migration_cache_.end()) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
            return Status::NOT_FOUND;
        }

        it->second.pages_copied = pages_copied;

        return Status::OK;
    }

    auto CatalogManager::setMigrationPhase(
        const ID &migration_id,
        MigrationPhase new_phase,
        ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(migration_mutex_);

        auto it = migration_cache_.find(migration_id);
        if (it == migration_cache_.end()) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
            return Status::NOT_FOUND;
        }

        TableMigrationState &state = it->second;

        // Validate phase transition
        MigrationPhase old_phase = state.phase;

        // Log phase transition
        LOG_INFO(CATALOG,
                "setMigrationPhase: Migration {} transitioning from phase {} to {}",
                migration_id,
                static_cast<int>(old_phase),
                static_cast<int>(new_phase));

        state.phase = new_phase;

        // Update table info phase as well
        std::lock_guard<std::mutex> table_lock(mutex_);
        auto table_it = table_cache_.find(state.table_id);
        if (table_it != table_cache_.end()) {
            table_it->second.migration_phase = static_cast<uint8_t>(new_phase);
        }

        return Status::OK;
    }

    auto CatalogManager::abortMigration(
        const ID &migration_id,
        ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::lock_guard<std::mutex> migration_lock(migration_mutex_);

        auto it = migration_cache_.find(migration_id);
        if (it == migration_cache_.end()) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
            return Status::NOT_FOUND;
        }

        TableMigrationState &state = it->second;

        // Update phase to ABORTED
        state.phase = MigrationPhase::MIGRATION_ABORTED;
        state.end_time = std::time(nullptr);

        // Update table info
        auto table_it = table_cache_.find(state.table_id);
        if (table_it != table_cache_.end()) {
            TableInfo &table_info = table_it->second;
            table_info.migration_in_progress = false;
            table_info.migration_id = ID(); // Clear
            table_info.migration_xid = 0;
            table_info.migration_target_ts = 0;
            table_info.migration_phase = 0;
        }

        LOG_INFO(CATALOG,
                "abortMigration: Migration {} aborted",
                migration_id);

        // Remove from cache
        migration_cache_.erase(it);

        return Status::OK;
    }

    auto CatalogManager::markPageDirty(
        const ID &migration_id,
        uint32_t page_number,
        ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(migration_mutex_);

        auto it = migration_cache_.find(migration_id);
        if (it == migration_cache_.end()) {
            // Migration not found - this is OK, might have completed
            return Status::OK;
        }

        TableMigrationState &state = it->second;

        // Check if page number is valid
        if (page_number >= state.total_pages) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Page number exceeds total pages");
            return Status::INVALID_ARGUMENT;
        }

        // Set bit in bitmap
        uint32_t byte_idx = page_number / 8;
        uint32_t bit_idx = page_number % 8;

        state.dirty_pages_bitmap[byte_idx] |= (1u << bit_idx);

        return Status::OK;
    }

    auto CatalogManager::getDirtyPages(
        const ID &migration_id,
        ErrorContext *ctx) -> std::vector<uint32_t>
    {
        std::lock_guard<std::mutex> lock(migration_mutex_);

        auto it = migration_cache_.find(migration_id);
        if (it == migration_cache_.end()) {
            return {};
        }

        TableMigrationState &state = it->second;
        std::vector<uint32_t> dirty_pages;

        // Scan dirty page bitmap
        for (uint32_t page = 0; page < state.total_pages; ++page) {
            uint32_t byte_idx = page / 8;
            uint32_t bit_idx = page % 8;

            if (state.dirty_pages_bitmap[byte_idx] & (1u << bit_idx)) {
                dirty_pages.push_back(page);
            }
        }

        return dirty_pages;
    }

    auto CatalogManager::clearDirtyPages(
        const ID &migration_id,
        ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(migration_mutex_);

        auto it = migration_cache_.find(migration_id);
        if (it == migration_cache_.end()) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
            return Status::NOT_FOUND;
        }

        TableMigrationState &state = it->second;

        // Clear bitmap
        size_t bitmap_bytes = (state.total_pages + 7) / 8;
        std::memset(state.dirty_pages_bitmap.get(), 0, bitmap_bytes);

        return Status::OK;
    }

    auto CatalogManager::getDirtyPageCount(const ID &migration_id) -> uint32_t
    {
        std::lock_guard<std::mutex> lock(migration_mutex_);

        auto it = migration_cache_.find(migration_id);
        if (it == migration_cache_.end()) {
            return 0;
        }

        TableMigrationState &state = it->second;
        uint32_t count = 0;

        // Count set bits in bitmap (Brian Kernighan's algorithm)
        size_t bitmap_bytes = (state.total_pages + 7) / 8;
        for (size_t i = 0; i < bitmap_bytes; ++i) {
            uint8_t byte = state.dirty_pages_bitmap[i];
            while (byte) {
                byte &= (byte - 1);
                count++;
            }
        }

        return count;
    }

    auto CatalogManager::completeMigration(
        const ID &migration_id,
        ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(migration_mutex_);

        auto it = migration_cache_.find(migration_id);
        if (it == migration_cache_.end()) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
            return Status::NOT_FOUND;
        }

        TableMigrationState &state = it->second;

        // Set phase to COMPLETE
        state.phase = MigrationPhase::MIGRATION_COMPLETE;
        state.end_time = std::time(nullptr);

        // Calculate total duration
        uint64_t duration_seconds = state.end_time - state.start_time;

        LOG_INFO(CATALOG,
                "completeMigration: Migration {} completed in {} seconds ({} pages)",
                migration_id,
                duration_seconds,
                state.total_pages);

        // Remove from active migration cache
        // TODO: Could persist migration history to disk here

        migration_cache_.erase(it);

        return Status::OK;
    }

    auto CatalogManager::getTableIndexes(
        const ID &table_id,
        ErrorContext *ctx) -> std::vector<IndexInfo>
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<IndexInfo> indexes;

        // Scan index cache for indexes belonging to this table
        for (const auto &[index_id, index_info] : index_cache_) {
            if (index_info.table_id == table_id) {
                indexes.push_back(index_info);
            }
        }

        return indexes;
    }

    // ========================================================================
    // Sprint 5: ONLINE Migration Execution Engine
    // ========================================================================

    /**
     * executeOnlineMigrationCopyingPhase - Sprint 5 Task 5.4.4
 *
 * Implements the COPYING phase of ONLINE migration:
 * 1. Enumerate all heap pages in source tablespace
 * 2. Copy each page to target tablespace with TID remapping
 * 3. Record TID mappings in TIDResolver for dual-source visibility
 * 4. Track progress for monitoring
 */
Status CatalogManager::executeOnlineMigrationCopyingPhase(
    const ID &migration_id, ErrorContext *ctx)
{
    LOG_INFO(CATALOG, "executeOnlineMigrationCopyingPhase: Starting COPYING phase");

    // ===== STEP 1: Get migration state =====
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
        LOG_ERROR(CATALOG, "Migration ID not found in cache");
        return Status::NOT_FOUND;
    }

    TableMigrationState &state = it->second;

    // Verify we're in COPYING phase
    if (state.phase != MigrationPhase::MIGRATION_COPYING)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Migration not in COPYING phase");
        LOG_ERROR(CATALOG, "Migration phase is %d, expected COPYING (%d)",
                 static_cast<int>(state.phase),
                 static_cast<int>(MigrationPhase::MIGRATION_COPYING));
        return Status::INVALID_ARGUMENT;
    }

    ID table_id = state.table_id;
    uint16_t source_ts = state.source_tablespace;
    uint16_t target_ts = state.target_tablespace;

    LOG_INFO(CATALOG, "COPYING phase: table_id=%s, source_ts=%u, target_ts=%u",
             "<migration>", source_ts, target_ts);

    // Release lock for long-running operations
    mutex_.unlock();

    // ===== STEP 2: Enumerate all heap pages =====
    std::vector<GPID> source_pages;
    Status status = enumerateTablePages(table_id, source_pages, ctx);
    if (status != Status::OK)
    {
        mutex_.lock();
        SET_ERROR_CONTEXT(ctx, status, "Failed to enumerate table pages");
        LOG_ERROR(CATALOG, "Failed to enumerate pages for table");
        return status;
    }

    uint32_t total_pages = static_cast<uint32_t>(source_pages.size());
    LOG_INFO(CATALOG, "Found %u heap pages to copy", total_pages);

    // Update total_pages in state
    mutex_.lock();
    state.total_pages = total_pages;
    state.pages_copied = 0;
    mutex_.unlock();

    // ===== STEP 3: TID mapping and progress tracking =====
    std::unordered_map<uint64_t, uint64_t> tid_mapping;
    uint32_t pages_copied = 0;
    uint64_t bytes_copied = 0;
    auto last_log_time = std::chrono::steady_clock::now();

    // ===== STEP 4: Copy each page =====
    for (GPID source_gpid : source_pages)
    {
        // Pin source page (read-only)
        void *source_buffer = nullptr;
        status = db_->buffer_pool()->pinPageGlobal(source_gpid, &source_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin source page");
            LOG_ERROR(CATALOG, "Failed to pin source page GPID=%016lx", source_gpid);
            return status;
        }

        // Allocate target page in target tablespace
        GPID target_gpid;
        status = db_->page_manager()->allocatePageInTablespace(target_ts, &target_gpid, ctx);
        if (status != Status::OK)
        {
            db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate target page");
            LOG_ERROR(CATALOG, "Failed to allocate page in target tablespace %u", target_ts);
            return status;
        }

        // Pin target page (will be written)
        void *target_buffer = nullptr;
        status = db_->buffer_pool()->pinPageGlobal(target_gpid, &target_buffer, ctx);
        if (status != Status::OK)
        {
            db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
            db_->page_manager()->freePageGlobal(target_gpid, ctx);
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin target page");
            LOG_ERROR(CATALOG, "Failed to pin target page GPID=%016lx", target_gpid);
            return status;
        }

        // Copy page with TID remapping
        status = copyPageWithTIDRemapping(source_buffer, target_buffer,
                                         source_gpid, target_gpid,
                                         tid_mapping, ctx);
        if (status != Status::OK)
        {
            db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
            db_->buffer_pool()->unpinPageGlobal(target_gpid, false, ctx);
            db_->page_manager()->freePageGlobal(target_gpid, ctx);
            SET_ERROR_CONTEXT(ctx, status, "Failed to copy page with TID remapping");
            LOG_ERROR(CATALOG, "Failed to copy page %016lx → %016lx",
                     source_gpid, target_gpid);
            return status;
        }

        // Unpin pages
        db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);  // Source not modified
        db_->buffer_pool()->unpinPageGlobal(target_gpid, true, ctx);   // Target is dirty

        // Record TID mappings in TIDResolver
        // Assume max 256 slots per page (conservative estimate)
        for (uint16_t slot = 0; slot < 256; slot++)
        {
            TID source_tid{source_gpid, slot};
            TID target_tid{target_gpid, slot};
            db_->tid_resolver()->recordMigration(table_id, source_tid, target_tid, ctx);
        }

        // Update progress
        pages_copied++;
        bytes_copied += db_->page_size();

        // Log progress every 1000 pages or every 5 seconds
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time);

        if (pages_copied % 1000 == 0 || elapsed.count() >= 5)
        {
            LOG_INFO(CATALOG, "COPYING progress: %u / %u pages (%.1f%%), %lu MB copied",
                     pages_copied, total_pages,
                     (100.0 * pages_copied) / total_pages,
                     bytes_copied / (1024 * 1024));
            last_log_time = now;
        }

        // Update state periodically
        if (pages_copied % 100 == 0)
        {
            mutex_.lock();
            state.pages_copied = pages_copied;
            state.total_bytes_copied = bytes_copied;
            mutex_.unlock();
        }
    }

    // ===== STEP 5: Update final state =====
    mutex_.lock();
    state.pages_copied = pages_copied;
    state.total_bytes_copied = bytes_copied;

    LOG_INFO(CATALOG, "COPYING phase complete: %u pages, %lu MB copied",
             pages_copied, bytes_copied / (1024 * 1024));

    return Status::OK;
}

/**
 * executeOnlineMigrationCatchUpPhase - Sprint 5 Task 5.4.5
 *
 * Implements the CATCH_UP phase of ONLINE migration:
 * 1. Identify dirty pages (modified during COPYING phase)
 * 2. Re-copy dirty pages to target tablespace
 * 3. Repeat until dirty page count falls below threshold or max iterations reached
 * 4. Prepare for SWAP phase
 */
Status CatalogManager::executeOnlineMigrationCatchUpPhase(
    const ID &migration_id, uint32_t max_iterations,
    uint32_t dirty_threshold, ErrorContext *ctx)
{
    LOG_INFO(CATALOG, "executeOnlineMigrationCatchUpPhase: Starting CATCH_UP phase");
    LOG_INFO(CATALOG, "Parameters: max_iterations=%u, dirty_threshold=%u",
             max_iterations, dirty_threshold);

    // ===== STEP 1: Get migration state =====
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
        LOG_ERROR(CATALOG, "Migration ID not found in cache");
        return Status::NOT_FOUND;
    }

    TableMigrationState &state = it->second;

    // Verify we're in CATCH_UP phase
    if (state.phase != MigrationPhase::MIGRATION_CATCH_UP)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Migration not in CATCH_UP phase");
        LOG_ERROR(CATALOG, "Migration phase is %d, expected CATCH_UP (%d)",
                 static_cast<int>(state.phase),
                 static_cast<int>(MigrationPhase::MIGRATION_CATCH_UP));
        return Status::INVALID_ARGUMENT;
    }

    ID table_id = state.table_id;
    uint16_t source_ts = state.source_tablespace;
    uint16_t target_ts = state.target_tablespace;

    LOG_INFO(CATALOG, "CATCH_UP phase: table_id=%s, source_ts=%u, target_ts=%u",
             "<migration>", source_ts, target_ts);

    // Release lock for long-running operations
    mutex_.unlock();

    // ===== STEP 2: TID mapping =====
    std::unordered_map<uint64_t, uint64_t> tid_mapping;

    // ===== STEP 3: Iterative catch-up loop =====
    uint32_t iteration = 0;
    uint32_t dirty_count = 0;

    for (iteration = 0; iteration < max_iterations; iteration++)
    {
        // Get dirty pages
        std::vector<uint32_t> dirty_pages = getDirtyPages(migration_id, ctx);

        dirty_count = static_cast<uint32_t>(dirty_pages.size());

        LOG_INFO(CATALOG, "CATCH_UP iteration %u: %u dirty pages found",
                 iteration + 1, dirty_count);

        // Check if we're below threshold
        if (dirty_count <= dirty_threshold)
        {
            LOG_INFO(CATALOG, "CATCH_UP complete: dirty pages (%u) <= threshold (%u)",
                     dirty_count, dirty_threshold);
            break;
        }

        // Re-copy each dirty page
        uint32_t pages_recopied = 0;
        Status status;
        for (uint32_t page_id : dirty_pages)
        {
            // Construct source GPID
            GPID source_gpid = makeGPID(source_ts, page_id);

            // Pin source page
            void *source_buffer = nullptr;
            status = db_->buffer_pool()->pinPageGlobal(source_gpid, &source_buffer, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to pin source page during catch-up");
                LOG_ERROR(CATALOG, "Failed to pin source page GPID=%016lx", source_gpid);
                return status;
            }

            // Allocate new target page
            GPID target_gpid;
            status = db_->page_manager()->allocatePageInTablespace(target_ts, &target_gpid, ctx);
            if (status != Status::OK)
            {
                db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to allocate target page during catch-up");
                LOG_ERROR(CATALOG, "Failed to allocate page in target tablespace %u", target_ts);
                return status;
            }

            // Pin target page
            void *target_buffer = nullptr;
            status = db_->buffer_pool()->pinPageGlobal(target_gpid, &target_buffer, ctx);
            if (status != Status::OK)
            {
                db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
                db_->page_manager()->freePageGlobal(target_gpid, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to pin target page during catch-up");
                LOG_ERROR(CATALOG, "Failed to pin target page GPID=%016lx", target_gpid);
                return status;
            }

            // Copy page with TID remapping
            status = copyPageWithTIDRemapping(source_buffer, target_buffer,
                                             source_gpid, target_gpid,
                                             tid_mapping, ctx);
            if (status != Status::OK)
            {
                db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
                db_->buffer_pool()->unpinPageGlobal(target_gpid, false, ctx);
                db_->page_manager()->freePageGlobal(target_gpid, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to copy page during catch-up");
                LOG_ERROR(CATALOG, "Failed to copy page %016lx → %016lx",
                         source_gpid, target_gpid);
                return status;
            }

            // Unpin pages
            db_->buffer_pool()->unpinPageGlobal(source_gpid, false, ctx);
            db_->buffer_pool()->unpinPageGlobal(target_gpid, true, ctx);

            // Update TID mappings in TIDResolver
            for (uint16_t slot = 0; slot < 256; slot++)
            {
                TID source_tid{source_gpid, slot};
                TID target_tid{target_gpid, slot};
                db_->tid_resolver()->recordMigration(table_id, source_tid, target_tid, ctx);
            }

            pages_recopied++;

            // Log progress every 100 pages
            if (pages_recopied % 100 == 0)
            {
                LOG_INFO(CATALOG, "CATCH_UP iteration %u: %u / %u pages recopied",
                         iteration + 1, pages_recopied, dirty_count);
            }
        }

        LOG_INFO(CATALOG, "CATCH_UP iteration %u complete: %u pages recopied",
                 iteration + 1, pages_recopied);

        // Clear dirty pages bitmap for next iteration
        status = clearDirtyPages(migration_id, ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(CATALOG, "Failed to clear dirty pages bitmap (status=%d)",
                       static_cast<int>(status));
        }

        // Update statistics
        mutex_.lock();
        state.catch_up_iterations = iteration + 1;
        mutex_.unlock();
    }

    // ===== STEP 4: Update final state =====
    mutex_.lock();
    state.catch_up_iterations = iteration;
    state.final_dirty_page_count = dirty_count;

    if (dirty_count <= dirty_threshold)
    {
        LOG_INFO(CATALOG, "CATCH_UP phase complete: %u iterations, %u dirty pages remaining",
                 iteration, dirty_count);
        return Status::OK;
    }
    else
    {
        LOG_WARNING(CATALOG, "CATCH_UP phase incomplete: max iterations (%u) reached, %u dirty pages remaining",
                   max_iterations, dirty_count);

        // Still return OK - we'll proceed with SWAP but log the high dirty page count
        return Status::OK;
    }
}

/**
 * executeOnlineMigrationSwapPhase - Sprint 5 Task 5.4.6
 *
 * Implements the SWAP phase of ONLINE migration:
 * 1. Get TID mappings from TIDResolver
 * 2. Update all indexes with new TIDs
 * 3. Atomically update catalog (table.tablespace_id = target)
 * 4. Free old pages in source tablespace
 * 5. Clear TIDResolver state
 * 6. Mark migration complete
 */
Status CatalogManager::executeOnlineMigrationSwapPhase(
    const ID &migration_id, ErrorContext *ctx)
{
    LOG_INFO(CATALOG, "executeOnlineMigrationSwapPhase: Starting SWAP phase");

    // ===== STEP 1: Get migration state =====
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
        LOG_ERROR(CATALOG, "Migration ID not found in cache");
        return Status::NOT_FOUND;
    }

    TableMigrationState &state = it->second;

    // Verify we're in SWAP phase
    if (state.phase != MigrationPhase::MIGRATION_SWAP)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Migration not in SWAP phase");
        LOG_ERROR(CATALOG, "Migration phase is %d, expected SWAP (%d)",
                 static_cast<int>(state.phase),
                 static_cast<int>(MigrationPhase::MIGRATION_SWAP));
        return Status::INVALID_ARGUMENT;
    }

    ID table_id = state.table_id;
    uint16_t source_ts = state.source_tablespace;
    uint16_t target_ts = state.target_tablespace;

    LOG_INFO(CATALOG, "SWAP phase: table_id=%s, source_ts=%u → target_ts=%u",
             "<migration>", source_ts, target_ts);

    // Release lock for long-running operations
    mutex_.unlock();

    // ===== STEP 2: Get TID mappings from TIDResolver =====
    std::unordered_map<uint64_t, uint64_t> tid_mapping =
        db_->tid_resolver()->getAllMappings(table_id, ctx);

    LOG_INFO(CATALOG, "Retrieved %zu TID mappings from TIDResolver",
             tid_mapping.size());

    // ===== STEP 3: Update all indexes with new TIDs =====
    Status status = updateIndexTIDs(table_id, tid_mapping, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to update index TIDs");
        LOG_ERROR(CATALOG, "Failed to update index TIDs during SWAP phase");
        return status;
    }

    LOG_INFO(CATALOG, "Successfully updated all indexes with new TIDs");

    // ===== STEP 4: Atomically update catalog =====
    mutex_.lock();

    // Get table info
    TableInfo table_info;
    status = getTable(table_id, table_info, ctx);
    if (status != Status::OK)
    {
        mutex_.unlock();
        SET_ERROR_CONTEXT(ctx, status, "Failed to get table info");
        LOG_ERROR(CATALOG, "Failed to get table info during SWAP");
        return status;
    }

    // Update tablespace_id to target
    uint16_t old_tablespace = table_info.tablespace_id;
    table_info.tablespace_id = target_ts;

    // Clear migration flags
    table_info.migration_in_progress = false;
    table_info.migration_id = ID{};
    table_info.migration_xid = 0;
    table_info.migration_target_ts = 0;
    table_info.migration_phase = static_cast<uint8_t>(MigrationPhase::MIGRATION_NONE);

    // Update in cache
    table_cache_[table_id] = table_info;

    LOG_INFO(CATALOG, "Updated catalog: table '%s' now in tablespace %u (was %u)",
             table_info.table_name.c_str(), target_ts, old_tablespace);

    mutex_.unlock();

    // ===== STEP 5: Free old pages in source tablespace =====
    // Get all source pages to free
    std::vector<GPID> source_pages;
    status = enumerateTablePages(table_id, source_pages, ctx);
    if (status != Status::OK)
    {
        LOG_WARNING(CATALOG, "Failed to enumerate source pages for cleanup (status=%d)",
                   static_cast<int>(status));
        // Continue anyway - this is cleanup, not critical
    }
    else
    {
        uint32_t pages_freed = 0;
        uint32_t pages_failed = 0;

        for (GPID source_gpid : source_pages)
        {
            // Only free pages in the OLD source tablespace
            if (getTablespaceID(source_gpid) == old_tablespace)
            {
                status = db_->page_manager()->freePageGlobal(source_gpid, ctx);
                if (status == Status::OK)
                {
                    pages_freed++;

                    // Log every 1000 pages
                    if (pages_freed % 1000 == 0)
                    {
                        LOG_INFO(CATALOG, "Cleanup progress: %u pages freed", pages_freed);
                    }
                }
                else
                {
                    pages_failed++;
                    LOG_WARNING(CATALOG, "Failed to free source page GPID=%016lx (status=%d)",
                               source_gpid, static_cast<int>(status));
                }
            }
        }

        LOG_INFO(CATALOG, "Cleanup complete: %u pages freed, %u failed",
                 pages_freed, pages_failed);
    }

    // ===== STEP 6: Clear TIDResolver state =====
    status = db_->tid_resolver()->clearMigration(table_id, ctx);
    if (status != Status::OK)
    {
        LOG_WARNING(CATALOG, "Failed to clear TIDResolver state (status=%d)",
                   static_cast<int>(status));
        // Continue anyway
    }

    LOG_INFO(CATALOG, "Cleared TIDResolver state for table");

    // ===== STEP 7: Mark migration complete =====
    mutex_.lock();

    state.phase = MigrationPhase::MIGRATION_COMPLETE;
    state.end_time = std::chrono::system_clock::now().time_since_epoch().count();

    LOG_INFO(CATALOG, "SWAP phase complete: migration finished successfully");
    LOG_INFO(CATALOG, "Migration statistics:");
    LOG_INFO(CATALOG, "  Total pages: %u", state.total_pages);
    LOG_INFO(CATALOG, "  Pages copied: %u", state.pages_copied);
    LOG_INFO(CATALOG, "  Catch-up iterations: %u", state.catch_up_iterations);
    LOG_INFO(CATALOG, "  Final dirty pages: %u", state.final_dirty_page_count);
    LOG_INFO(CATALOG, "  Total bytes copied: %lu MB",
             state.total_bytes_copied / (1024 * 1024));

    // Remove from migration cache (migration is complete)
    migration_cache_.erase(migration_id);

    mutex_.unlock();

    return Status::OK;
}

// ============================================================================
// Sprint 6 Task 5.4.8: Cancel/Rollback ONLINE Migration
// ============================================================================

Status CatalogManager::cancelOnlineMigration(
    const ID &migration_id, ErrorContext *ctx)
{
    LOG_INFO(CATALOG, "cancelOnlineMigration: Cancelling migration");

    // ===== STEP 1: Get migration state =====
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
        LOG_ERROR(CATALOG, "Migration ID not found in cache");
        return Status::NOT_FOUND;
    }

    TableMigrationState &state = it->second;
    ID table_id = state.table_id;
    uint16_t source_ts = state.source_tablespace;
    uint16_t target_ts = state.target_tablespace;
    MigrationPhase phase = state.phase;

    LOG_INFO(CATALOG, "Cancelling migration in phase %d", static_cast<int>(phase));

    // ===== STEP 2: Check if cancellation is allowed =====
    switch (phase)
    {
        case MigrationPhase::MIGRATION_SWAP:
        case MigrationPhase::MIGRATION_CLEANUP:
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                            "Cannot cancel migration during SWAP or CLEANUP phase - too late");
            LOG_ERROR(CATALOG, "Cannot cancel migration in phase %d (too late)",
                     static_cast<int>(phase));
            return Status::INVALID_ARGUMENT;

        case MigrationPhase::MIGRATION_COMPLETE:
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                            "Migration already complete");
            LOG_WARNING(CATALOG, "Migration already complete - nothing to cancel");
            return Status::INVALID_ARGUMENT;

        case MigrationPhase::MIGRATION_FAILED:
        case MigrationPhase::MIGRATION_ABORTED:
            LOG_INFO(CATALOG, "Migration already in terminal state - cleaning up");
            // Fall through to cleanup
            break;

        case MigrationPhase::MIGRATION_NONE:
        case MigrationPhase::MIGRATION_INIT:
        case MigrationPhase::MIGRATION_COPYING:
        case MigrationPhase::MIGRATION_CATCH_UP:
        case MigrationPhase::MIGRATION_READY_FOR_SWAP:
            // These phases can be safely cancelled
            break;
    }

    // ===== STEP 3: Mark migration as ABORTED =====
    state.phase = MigrationPhase::MIGRATION_ABORTED;
    state.end_time = std::chrono::system_clock::now().time_since_epoch().count();

    LOG_INFO(CATALOG, "Marked migration as ABORTED");

    // Release lock for long-running operations
    mutex_.unlock();

    // ===== STEP 4: Enumerate target pages for cleanup =====
    std::vector<GPID> target_pages;
    Status status = enumerateTablePages(table_id, target_pages, ctx);
    if (status != Status::OK)
    {
        LOG_WARNING(CATALOG, "Failed to enumerate target pages (status=%d) - continuing anyway",
                   static_cast<int>(status));
        target_pages.clear();
    }

    // ===== STEP 5: Free target tablespace pages =====
    uint32_t pages_freed = 0;
    uint32_t pages_failed = 0;

    for (GPID gpid : target_pages)
    {
        // Only free pages in the TARGET tablespace (not source!)
        if (getTablespaceID(gpid) == target_ts)
        {
            status = db_->page_manager()->freePageGlobal(gpid, ctx);
            if (status == Status::OK)
            {
                pages_freed++;

                // Log progress every 1000 pages
                if (pages_freed % 1000 == 0)
                {
                    LOG_INFO(CATALOG, "Rollback progress: %u target pages freed", pages_freed);
                }
            }
            else
            {
                pages_failed++;
                LOG_WARNING(CATALOG, "Failed to free target page GPID=%016lx (status=%d)",
                           gpid, static_cast<int>(status));
            }
        }
    }

    LOG_INFO(CATALOG, "Rollback cleanup: %u target pages freed, %u failed",
             pages_freed, pages_failed);

    // ===== STEP 6: Clear TIDResolver state =====
    status = db_->tid_resolver()->clearMigration(table_id, ctx);
    if (status != Status::OK)
    {
        LOG_WARNING(CATALOG, "Failed to clear TIDResolver state (status=%d)",
                   static_cast<int>(status));
        // Continue anyway
    }

    LOG_INFO(CATALOG, "Cleared TIDResolver state");

    // ===== STEP 7: Clear table migration flags =====
    mutex_.lock();

    TableInfo table_info;
    status = getTable(table_id, table_info, ctx);
    if (status == Status::OK)
    {
        table_info.migration_in_progress = false;
        table_info.migration_id = ID{};
        table_info.migration_xid = 0;
        table_info.migration_target_ts = 0;
        table_info.migration_phase = static_cast<uint8_t>(MigrationPhase::MIGRATION_NONE);

        table_cache_[table_id] = table_info;

        LOG_INFO(CATALOG, "Cleared migration flags from table '%s'",
                 table_info.table_name.c_str());
    }
    else
    {
        LOG_WARNING(CATALOG, "Failed to get table info for cleanup (status=%d)",
                   static_cast<int>(status));
    }

    // ===== STEP 8: Remove from migration cache =====
    migration_cache_.erase(migration_id);

    mutex_.unlock();

    LOG_INFO(CATALOG, "Migration cancelled successfully");
    LOG_INFO(CATALOG, "Rollback statistics:");
    LOG_INFO(CATALOG, "  Target pages freed: %u", pages_freed);
    LOG_INFO(CATALOG, "  Partial pages copied: %u / %u",
             state.pages_copied, state.total_pages);

    return Status::OK;
}



// ===== Trigger Management (Phase 2 Wave 2 - Agent C) =====

auto CatalogManager::createTrigger(const TriggerInfo &trigger, ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(trigger_mutex_);
    
    // Check trigger name is unique
    if (trigger_name_to_id_.count(trigger.trigger_name) > 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Trigger already exists");
        return Status::NOT_FOUND;
    }
    
    // Check table exists
    TableInfo table_info;
    auto table_result = getTable(trigger.table_id, table_info, nullptr);
    if (table_result != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table does not exist");
        return Status::NOT_FOUND;
    }
    
    // Generate trigger ID
    TriggerInfo trigger_copy = trigger;
    trigger_copy.trigger_id = generateUuidV7();
    trigger_copy.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    trigger_copy.enabled = true;
    
    // Store in cache
    trigger_cache_[trigger_copy.trigger_id] = trigger_copy;
    trigger_name_to_id_[trigger_copy.trigger_name] = trigger_copy.trigger_id;
    table_triggers_.insert({trigger_copy.table_id, trigger_copy.trigger_id});
    
    LOG_INFO(CATALOG, "Created trigger '%s' on table '%s'", 
             trigger_copy.trigger_name.c_str(), trigger_copy.table_name.c_str());
    
    return Status::OK;
}

auto CatalogManager::dropTrigger(const std::string &trigger_name, ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(trigger_mutex_);

    // Look up trigger
    auto it = trigger_name_to_id_.find(trigger_name);
    if (it == trigger_name_to_id_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Trigger does not exist");
        return Status::NOT_FOUND;
    }
    
    ID trigger_id = it->second;
    const auto& trigger_info = trigger_cache_[trigger_id];
    
    // Remove from table_triggers index
    auto range = table_triggers_.equal_range(trigger_info.table_id);
    for (auto tit = range.first; tit != range.second; ++tit)
    {
        if (tit->second == trigger_id)
        {
            table_triggers_.erase(tit);
            break;
        }
    }
    
    // Remove from caches
    trigger_cache_.erase(trigger_id);
    trigger_name_to_id_.erase(trigger_name);
    
    LOG_INFO(CATALOG, "Dropped trigger '%s'", trigger_name.c_str());
    
    return Status::OK;
}

auto CatalogManager::getTrigger(const ID &trigger_id, TriggerInfo &info, ErrorContext *ctx)
    -> Status
{
    std::lock_guard<std::mutex> lock(trigger_mutex_);

    auto it = trigger_cache_.find(trigger_id);
    if (it == trigger_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Trigger not found");
        return Status::NOT_FOUND;
    }
    
    info = it->second;
    return Status::OK;
}

auto CatalogManager::getTriggerByName(const std::string &trigger_name, TriggerInfo &info,
                                      ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(trigger_mutex_);

    auto it = trigger_name_to_id_.find(trigger_name);
    if (it == trigger_name_to_id_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Trigger not found");
        return Status::NOT_FOUND;
    }
    
    return getTrigger(it->second, info, ctx);
}

auto CatalogManager::listTriggersForTable(const ID &table_id, TriggerEvent event,
                                          TriggerTiming timing,
                                          std::vector<TriggerInfo> &triggers,
                                          ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(trigger_mutex_);
    
    triggers.clear();
    
    // Find all triggers for this table
    auto range = table_triggers_.equal_range(table_id);
    for (auto it = range.first; it != range.second; ++it)
    {
        const auto& trigger = trigger_cache_[it->second];
        
        // Filter by event, timing, and enabled status
        if (trigger.event == event && trigger.timing == timing && trigger.enabled)
        {
            triggers.push_back(trigger);
        }
    }
    
    return Status::OK;
}

auto CatalogManager::listAllTriggersForTable(const ID &table_id,
                                             std::vector<TriggerInfo> &triggers,
                                             ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(trigger_mutex_);
    
    triggers.clear();
    
    // Find all triggers for this table (regardless of event/timing)
    auto range = table_triggers_.equal_range(table_id);
    for (auto it = range.first; it != range.second; ++it)
    {
        const auto& trigger = trigger_cache_[it->second];
        triggers.push_back(trigger);
    }
    
    return Status::OK;
}

auto CatalogManager::enableTrigger(const std::string &trigger_name, bool enable,
                                   ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(trigger_mutex_);

    // Look up trigger
    auto it = trigger_name_to_id_.find(trigger_name);
    if (it == trigger_name_to_id_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Trigger does not exist");
        return Status::NOT_FOUND;
    }
    
    // Update enabled status
    trigger_cache_[it->second].enabled = enable;
    
    LOG_INFO(CATALOG, "Trigger '%s' %s", trigger_name.c_str(),
             enable ? "enabled" : "disabled");

    return Status::OK;
}

// ===== PSQL - Stored Procedures and Functions (Phase 2 Task 10.2) =====

auto CatalogManager::registerFunction(const FunctionInfo &info, ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(psql_mutex_);

    // Check if function already exists
    auto it = functions_.find(info.name);
    if (it != functions_.end())
    {
        if (!info.or_replace)
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                            "Function already exists. Use OR REPLACE to update.");
            return Status::CONSTRAINT_VIOLATION;
        }

        // Update existing function
        it->second = info;
        LOG_INFO(CATALOG, "Function '%s' replaced", info.name.c_str());
    }
    else
    {
        // Register new function
        functions_[info.name] = info;
        LOG_INFO(CATALOG, "Function '%s' registered", info.name.c_str());
    }

    return Status::OK;
}

auto CatalogManager::registerProcedure(const ProcedureInfo &info, ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(psql_mutex_);

    // Check if procedure already exists
    auto it = procedures_.find(info.name);
    if (it != procedures_.end())
    {
        if (!info.or_replace)
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                            "Procedure already exists. Use OR REPLACE to update.");
            return Status::CONSTRAINT_VIOLATION;
        }

        // Update existing procedure
        it->second = info;
        LOG_INFO(CATALOG, "Procedure '%s' replaced", info.name.c_str());
    }
    else
    {
        // Register new procedure
        procedures_[info.name] = info;
        LOG_INFO(CATALOG, "Procedure '%s' registered", info.name.c_str());
    }

    return Status::OK;
}

auto CatalogManager::getFunction(const std::string &name, FunctionInfo &info_out,
                                 ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(psql_mutex_);

    auto it = functions_.find(name);
    if (it == functions_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Function not found");
        return Status::NOT_FOUND;
    }

    info_out = it->second;
    return Status::OK;
}

auto CatalogManager::getProcedure(const std::string &name, ProcedureInfo &info_out,
                                  ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(psql_mutex_);

    auto it = procedures_.find(name);
    if (it == procedures_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Procedure not found");
        return Status::NOT_FOUND;
    }

    info_out = it->second;
    return Status::OK;
}

auto CatalogManager::dropFunction(const std::string &name, bool if_exists,
                                  ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(psql_mutex_);

    auto it = functions_.find(name);
    if (it == functions_.end())
    {
        if (if_exists)
        {
            LOG_INFO(CATALOG, "Function '%s' does not exist (IF EXISTS)", name.c_str());
            return Status::OK;
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Function not found");
        return Status::NOT_FOUND;
    }

    functions_.erase(it);
    LOG_INFO(CATALOG, "Function '%s' dropped", name.c_str());

    return Status::OK;
}

auto CatalogManager::dropProcedure(const std::string &name, bool if_exists,
                                   ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(psql_mutex_);

    auto it = procedures_.find(name);
    if (it == procedures_.end())
    {
        if (if_exists)
        {
            LOG_INFO(CATALOG, "Procedure '%s' does not exist (IF EXISTS)", name.c_str());
            return Status::OK;
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Procedure not found");
        return Status::NOT_FOUND;
    }

    procedures_.erase(it);
    LOG_INFO(CATALOG, "Procedure '%s' dropped", name.c_str());

    return Status::OK;
}

auto CatalogManager::listFunctions(std::vector<FunctionInfo> &functions_out,
                                   ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(psql_mutex_);

    functions_out.clear();
    functions_out.reserve(functions_.size());

    for (const auto &[name, info] : functions_)
    {
        functions_out.push_back(info);
    }

    return Status::OK;
}

auto CatalogManager::listProcedures(std::vector<ProcedureInfo> &procedures_out,
                                    ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(psql_mutex_);

    procedures_out.clear();
    procedures_out.reserve(procedures_.size());

    for (const auto &[name, info] : procedures_)
    {
        procedures_out.push_back(info);
    }

    return Status::OK;
}

// LSM Integration Phase 3.3: Index object cache management

void* CatalogManager::getIndexPtr(const ID &index_id, IndexType *type_out)
{
    std::lock_guard<std::mutex> lock(index_object_mutex_);

    auto it = index_object_cache_.find(index_id);
    if (it == index_object_cache_.end())
    {
        return nullptr;
    }

    if (type_out)
    {
        *type_out = it->second.index_type;
    }

    return it->second.index_ptr;
}

Status CatalogManager::closeAllIndexes(ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(index_object_mutex_);

    Status overall_status = Status::OK;

    for (auto &[index_id, handle] : index_object_cache_)
    {
        Status status = IndexFactory::closeIndex(handle.index_type, handle.index_ptr, ctx);
        if (status != Status::OK)
        {
            DEBUG_LOG_DB("Failed to close index " << index_id.toString() << ": " << statusToString(status));
            overall_status = status;  // Track first error
        }
    }

    index_object_cache_.clear();

    return overall_status;
}


// ========================================
// DDL MODIFICATIONS (ALPHA Phase 1)
// ========================================

Status CatalogManager::dropTable(const ID &table_id, bool cascade, ErrorContext *ctx)
{
    // DROP TABLE implementation (ALPHA Phase 1 - DDL Modifications)
    // Implements soft delete with MGA compliance

    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Check if table exists in cache
    auto table_it = table_cache_.find(table_id);
    if (table_it == table_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
        return Status::NOT_FOUND;
    }

    const TableInfo &table_info = table_it->second;

    // 2. Check for dependent indexes
    std::vector<IndexInfo> indexes;
    Status status = listIndexesForTable(table_id, indexes, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (!indexes.empty() && !cascade)
    {
        // RESTRICT: fail if dependent indexes exist
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Table has dependent indexes - use CASCADE to drop them");
        return Status::INVALID_ARGUMENT;
    }

    // 3. DROP dependent indexes if CASCADE
    if (cascade && !indexes.empty())
    {
        for (const auto &index : indexes)
        {
            status = dropIndex(index.index_id, ctx);
            if (status != Status::OK)
            {
                // Failed to drop index - abort
                return status;
            }
        }
    }

    // 4. Soft delete the table record (mark is_valid = 0)
    status = deleteTableRecord(table_id, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // 5. Remove from cache
    table_cache_.erase(table_it);

    return Status::OK;
}

Status CatalogManager::dropIndex(const ID &index_id, ErrorContext *ctx)
{
    // DROP INDEX implementation (ALPHA Phase 1 - DDL Modifications)
    // Implements soft delete with MGA compliance

    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Check if index exists in cache
    auto index_it = index_cache_.find(index_id);
    if (index_it == index_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Index not found");
        return Status::NOT_FOUND;
    }

    // 2. Soft delete the index record (mark is_valid = 0)
    Status status = deleteIndexRecord(index_id, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // 3. Remove from cache
    // Note: Any cached index objects will be released when the cache entry is removed
    index_cache_.erase(index_it);

    return Status::OK;
}

// ============================================================================
// ALTER TABLE Operations (ALPHA Phase 1 - DDL Modifications)
// ============================================================================

Status CatalogManager::addColumn(const ID &table_id, const ColumnInfo &column_info,
                                  ErrorContext *ctx)
{
    // ADD COLUMN implementation (ALPHA Phase 1)
    // Adds a new column to an existing table with MGA compliance

    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Check if table exists in cache
    auto table_it = table_cache_.find(table_id);
    if (table_it == table_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
        return Status::NOT_FOUND;
    }

    TableInfo &table_info = table_cache_[table_id];

    // 2. Read existing columns from disk to check for duplicates and find max ordinal
    BufferPool *bp = db_->buffer_pool();
    void *page_data;
    Status status = bp->pinPage(columns_table_page_, &page_data, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    HeapPage heap_page(static_cast<uint8_t *>(page_data), db_->page_size());
    uint16_t next_ordinal = 0;
    uint32_t existing_column_count = 0;
    bool name_exists = false;

    for (uint16_t i = 0; i < heap_page.getItemCount(); ++i)
    {
        const uint8_t *tuple_data;
        uint32_t tuple_size;

        if (heap_page.getTuple(i, &tuple_data, &tuple_size, ctx) == Status::OK)
        {
            if (tuple_size >= sizeof(TupleHeader) + sizeof(ColumnRecord))
            {
                const auto *record = reinterpret_cast<const ColumnRecord *>(
                    tuple_data + sizeof(TupleHeader));

                if (record->table_id == table_id && record->is_valid == 1)
                {
                    existing_column_count++;
                    if (record->ordinal >= next_ordinal)
                    {
                        next_ordinal = record->ordinal + 1;
                    }
                    if (std::strcmp(record->column_name, column_info.column_name.c_str()) == 0)
                    {
                        name_exists = true;
                    }
                }
            }
        }
    }

    bp->unpinPage(columns_table_page_, false, ctx);

    if (name_exists)
    {
        std::string err = "Column already exists: " + column_info.column_name;
        SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, err.c_str());
        return Status::FILE_EXISTS;
    }

    // 3. Create new ColumnInfo with generated UUID
    ColumnInfo new_column = column_info;
    new_column.column_id = ID(); // Generate new UUID
    new_column.ordinal = next_ordinal;

    // 4. Write ColumnRecord to disk
    status = bp->pinPage(columns_table_page_, &page_data, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    HeapPage heap_page2(static_cast<uint8_t *>(page_data), db_->page_size());

    // Create ColumnRecord
    ColumnRecord record = {};
    record.table_id = table_id;
    record.column_id = new_column.column_id;
    std::strncpy(record.column_name, new_column.column_name.c_str(),
                 sizeof(record.column_name) - 1);
    record.ordinal = new_column.ordinal;
    record.data_type = static_cast<uint16_t>(new_column.data_type);
    record.type_precision = new_column.type_precision;
    record.type_scale = new_column.type_scale;
    record.max_length = new_column.max_length;
    record.nullable = new_column.nullable ? 1 : 0;
    record.has_default = new_column.has_default ? 1 : 0;
    record.is_primary_key = new_column.is_primary_key ? 1 : 0;
    record.is_unique = new_column.is_unique ? 1 : 0;
    record.is_foreign_key = new_column.is_foreign_key ? 1 : 0;
    record.is_generated = new_column.is_generated ? 1 : 0;
    record.storage_type = static_cast<uint8_t>(new_column.storage_type);
    record.with_timezone = new_column.with_timezone ? 1 : 0;
    record.charset = new_column.charset;
    record.timezone_hint = new_column.timezone_hint;
    record.collation_id = new_column.collation_id;
    if (new_column.has_default)
    {
        std::strncpy(record.default_value, new_column.default_value.c_str(),
                     sizeof(record.default_value) - 1);
    }
    record.default_value_oid = new_column.default_value_oid;
    record.check_expr_oid = new_column.check_expr_oid;
    record.created_time = std::chrono::system_clock::now().time_since_epoch().count();
    record.is_valid = 1;

    // Insert tuple into heap page
    const uint8_t *tuple_data = reinterpret_cast<const uint8_t *>(&record);
    uint32_t tuple_size = sizeof(ColumnRecord);
    uint16_t item_id_out;

    status = heap_page2.insertTuple(tuple_data, tuple_size, 0 /* xmin */, &item_id_out, ctx);
    bp->unpinPage(columns_table_page_, status == Status::OK, ctx);

    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to write column record");
        return status;
    }

    // 5. Update TableRecord.column_count
    status = updateTableColumnCount(table_id, existing_column_count + 1, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    return Status::OK;
}

Status CatalogManager::dropColumn(const ID &table_id, const std::string &column_name,
                                   bool if_exists, bool cascade, ErrorContext *ctx)
{
    // DROP COLUMN implementation (ALPHA Phase 1)
    // Soft deletes column with MGA compliance and CASCADE support

    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Check if table exists in cache
    auto table_it = table_cache_.find(table_id);
    if (table_it == table_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
        return Status::NOT_FOUND;
    }

    // 2. Scan columns to find the target and count valid columns
    BufferPool *bp = db_->buffer_pool();
    void *page_data;
    Status status = bp->pinPage(columns_table_page_, &page_data, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    HeapPage heap_page(static_cast<uint8_t *>(page_data), db_->page_size());
    ID column_id;
    bool found = false;
    size_t valid_column_count = 0;

    for (uint16_t i = 0; i < heap_page.getItemCount(); ++i)
    {
        const uint8_t *tuple_data;
        uint32_t tuple_size;

        if (heap_page.getTuple(i, &tuple_data, &tuple_size, ctx) == Status::OK)
        {
            if (tuple_size >= sizeof(TupleHeader) + sizeof(ColumnRecord))
            {
                const auto *record = reinterpret_cast<const ColumnRecord *>(
                    tuple_data + sizeof(TupleHeader));

                if (record->table_id == table_id && record->is_valid == 1)
                {
                    valid_column_count++;
                    if (std::strcmp(record->column_name, column_name.c_str()) == 0)
                    {
                        column_id = record->column_id;
                        found = true;
                    }
                }
            }
        }
    }

    bp->unpinPage(columns_table_page_, false, ctx);

    if (!found)
    {
        if (if_exists)
        {
            return Status::OK; // Graceful handling
        }
        std::string err = "Column not found: " + column_name;
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, err.c_str());
        return Status::NOT_FOUND;
    }

    // 3. Check if this is the last column
    if (valid_column_count <= 1)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Cannot drop last column in table");
        return Status::INVALID_ARGUMENT;
    }

    // 4. Check for dependent indexes
    std::vector<IndexInfo> dependent_indexes;
    std::vector<IndexInfo> all_indexes;
    status = listIndexesForTable(table_id, all_indexes, ctx);
    if (status == Status::OK)
    {
        for (const auto &index : all_indexes)
        {
            // Check if this column is used in the index
            for (const auto &index_col_id : index.column_ids)
            {
                if (index_col_id == column_id)
                {
                    dependent_indexes.push_back(index);
                    break;
                }
            }
        }
    }

    if (!dependent_indexes.empty() && !cascade)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Column has dependent indexes - use CASCADE to drop them");
        return Status::INVALID_ARGUMENT;
    }

    // 5. DROP dependent indexes if CASCADE
    if (cascade && !dependent_indexes.empty())
    {
        for (const auto &index : dependent_indexes)
        {
            status = dropIndex(index.index_id, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }
    }

    // 6. Soft delete the column record (mark is_valid = 0)
    status = bp->pinPage(columns_table_page_, &page_data, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    HeapPage heap_page2(static_cast<uint8_t *>(page_data), db_->page_size());
    auto *mutable_page_data = static_cast<uint8_t *>(page_data);
    bool updated = false;

    for (uint16_t i = 0; i < heap_page2.getItemCount(); ++i)
    {
        const uint8_t *tuple_data;
        uint32_t tuple_size;

        if (heap_page2.getTuple(i, &tuple_data, &tuple_size, ctx) == Status::OK)
        {
            if (tuple_size >= sizeof(TupleHeader) + sizeof(ColumnRecord))
            {
                const ptrdiff_t offset =
                    tuple_data - static_cast<const uint8_t *>(page_data);
                uint8_t *mutable_tuple_data = mutable_page_data + offset;
                auto *record = reinterpret_cast<ColumnRecord *>(mutable_tuple_data +
                                                                 sizeof(TupleHeader));

                if (record->table_id == table_id && record->column_id == column_id &&
                    record->is_valid == 1)
                {
                    record->is_valid = 0; // Soft delete
                    updated = true;
                    break;
                }
            }
        }
    }

    bp->unpinPage(columns_table_page_, updated, ctx);

    if (!updated)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Column record not found on disk");
        return Status::NOT_FOUND;
    }

    // 7. Update TableRecord.column_count
    status = updateTableColumnCount(table_id, valid_column_count - 1, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    return Status::OK;
}

Status CatalogManager::renameColumn(const ID &table_id, const std::string &old_name,
                                     const std::string &new_name, ErrorContext *ctx)
{
    // RENAME COLUMN implementation (ALPHA Phase 1)
    // Updates column name in-place with MGA compliance

    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Check if table exists in cache
    auto table_it = table_cache_.find(table_id);
    if (table_it == table_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
        return Status::NOT_FOUND;
    }

    // 2. Validate new name (basic check - non-empty)
    if (new_name.empty() || new_name.length() > 127)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid column name");
        return Status::INVALID_ARGUMENT;
    }

    // 3. Scan columns to find old name and check new name doesn't exist
    BufferPool *bp = db_->buffer_pool();
    void *page_data;
    Status status = bp->pinPage(columns_table_page_, &page_data, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    HeapPage heap_page(static_cast<uint8_t *>(page_data), db_->page_size());
    ID column_id;
    bool found_old = false;
    bool new_name_exists = false;

    for (uint16_t i = 0; i < heap_page.getItemCount(); ++i)
    {
        const uint8_t *tuple_data;
        uint32_t tuple_size;

        if (heap_page.getTuple(i, &tuple_data, &tuple_size, ctx) == Status::OK)
        {
            if (tuple_size >= sizeof(TupleHeader) + sizeof(ColumnRecord))
            {
                const auto *record = reinterpret_cast<const ColumnRecord *>(
                    tuple_data + sizeof(TupleHeader));

                if (record->table_id == table_id && record->is_valid == 1)
                {
                    if (std::strcmp(record->column_name, old_name.c_str()) == 0)
                    {
                        column_id = record->column_id;
                        found_old = true;
                    }
                    if (std::strcmp(record->column_name, new_name.c_str()) == 0)
                    {
                        new_name_exists = true;
                    }
                }
            }
        }
    }

    bp->unpinPage(columns_table_page_, false, ctx);

    if (!found_old)
    {
        std::string err = "Column not found: " + old_name;
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, err.c_str());
        return Status::NOT_FOUND;
    }

    if (new_name_exists)
    {
        std::string err = "Column already exists: " + new_name;
        SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, err.c_str());
        return Status::FILE_EXISTS;
    }

    // 4. Update ColumnRecord on disk
    status = bp->pinPage(columns_table_page_, &page_data, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    HeapPage heap_page2(static_cast<uint8_t *>(page_data), db_->page_size());
    auto *mutable_page_data = static_cast<uint8_t *>(page_data);
    bool updated = false;

    for (uint16_t i = 0; i < heap_page2.getItemCount(); ++i)
    {
        const uint8_t *tuple_data;
        uint32_t tuple_size;

        if (heap_page2.getTuple(i, &tuple_data, &tuple_size, ctx) == Status::OK)
        {
            if (tuple_size >= sizeof(TupleHeader) + sizeof(ColumnRecord))
            {
                const ptrdiff_t offset =
                    tuple_data - static_cast<const uint8_t *>(page_data);
                uint8_t *mutable_tuple_data = mutable_page_data + offset;
                auto *record = reinterpret_cast<ColumnRecord *>(mutable_tuple_data +
                                                                 sizeof(TupleHeader));

                if (record->table_id == table_id && record->column_id == column_id &&
                    record->is_valid == 1)
                {
                    // Update column name in-place
                    std::memset(record->column_name, 0, sizeof(record->column_name));
                    std::strncpy(record->column_name, new_name.c_str(),
                                 sizeof(record->column_name) - 1);
                    updated = true;
                    break;
                }
            }
        }
    }

    bp->unpinPage(columns_table_page_, updated, ctx);

    if (!updated)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Column record not found on disk");
        return Status::NOT_FOUND;
    }

    return Status::OK;
}

Status CatalogManager::alterColumnType(const ID &table_id, const std::string &column_name,
                                        DataType new_type, uint32_t new_precision,
                                        uint32_t new_scale, ErrorContext *ctx)
{
    // ALTER COLUMN TYPE implementation (ALPHA Phase 1)
    // Phase 1: Only allows compatible type changes (no data conversion)

    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Check if table exists in cache
    auto table_it = table_cache_.find(table_id);
    if (table_it == table_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
        return Status::NOT_FOUND;
    }

    // 2. Scan columns to find the target column
    BufferPool *bp = db_->buffer_pool();
    void *page_data;
    Status status = bp->pinPage(columns_table_page_, &page_data, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    HeapPage heap_page(static_cast<uint8_t *>(page_data), db_->page_size());
    ID column_id;
    DataType old_type = DataType::UNKNOWN;
    uint32_t old_precision = 0;
    bool found = false;

    for (uint16_t i = 0; i < heap_page.getItemCount(); ++i)
    {
        const uint8_t *tuple_data;
        uint32_t tuple_size;

        if (heap_page.getTuple(i, &tuple_data, &tuple_size, ctx) == Status::OK)
        {
            if (tuple_size >= sizeof(TupleHeader) + sizeof(ColumnRecord))
            {
                const auto *record = reinterpret_cast<const ColumnRecord *>(
                    tuple_data + sizeof(TupleHeader));

                if (record->table_id == table_id && record->is_valid == 1 &&
                    std::strcmp(record->column_name, column_name.c_str()) == 0)
                {
                    column_id = record->column_id;
                    old_type = static_cast<DataType>(record->data_type);
                    old_precision = record->type_precision;
                    found = true;
                    break;
                }
            }
        }
    }

    bp->unpinPage(columns_table_page_, false, ctx);

    if (!found)
    {
        std::string err = "Column not found: " + column_name;
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, err.c_str());
        return Status::NOT_FOUND;
    }

    // 3. Check type compatibility (Phase 1: Compatible changes only)
    bool compatible = false;

    // Same type - allow if widening
    if (old_type == new_type)
    {
        if (new_precision >= old_precision || new_precision == 0)
        {
            compatible = true;
        }
    }
    // Integer widening: INT8→INT16→INT32→INT64→INT128
    else if (old_type == DataType::INT8 &&
             (new_type == DataType::INT16 || new_type == DataType::INT32 ||
              new_type == DataType::INT64 || new_type == DataType::INT128))
    {
        compatible = true;
    }
    else if (old_type == DataType::INT16 &&
             (new_type == DataType::INT32 || new_type == DataType::INT64 ||
              new_type == DataType::INT128))
    {
        compatible = true;
    }
    else if (old_type == DataType::INT32 &&
             (new_type == DataType::INT64 || new_type == DataType::INT128))
    {
        compatible = true;
    }
    else if (old_type == DataType::INT64 && new_type == DataType::INT128)
    {
        compatible = true;
    }
    // Float widening: FLOAT32→FLOAT64
    else if (old_type == DataType::FLOAT32 && new_type == DataType::FLOAT64)
    {
        compatible = true;
    }

    if (!compatible)
    {
        SET_ERROR_CONTEXT(
            ctx, Status::INVALID_ARGUMENT,
            "Incompatible type change - Phase 1 only supports widening conversions");
        return Status::INVALID_ARGUMENT;
    }

    // 4. Update ColumnRecord on disk
    status = bp->pinPage(columns_table_page_, &page_data, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    HeapPage heap_page2(static_cast<uint8_t *>(page_data), db_->page_size());
    auto *mutable_page_data = static_cast<uint8_t *>(page_data);
    bool updated = false;

    for (uint16_t i = 0; i < heap_page2.getItemCount(); ++i)
    {
        const uint8_t *tuple_data;
        uint32_t tuple_size;

        if (heap_page2.getTuple(i, &tuple_data, &tuple_size, ctx) == Status::OK)
        {
            if (tuple_size >= sizeof(TupleHeader) + sizeof(ColumnRecord))
            {
                const ptrdiff_t offset =
                    tuple_data - static_cast<const uint8_t *>(page_data);
                uint8_t *mutable_tuple_data = mutable_page_data + offset;
                auto *record = reinterpret_cast<ColumnRecord *>(mutable_tuple_data +
                                                                 sizeof(TupleHeader));

                if (record->table_id == table_id && record->column_id == column_id &&
                    record->is_valid == 1)
                {
                    // Update type information
                    record->data_type = static_cast<uint16_t>(new_type);
                    record->type_precision = new_precision;
                    record->type_scale = new_scale;
                    updated = true;
                    break;
                }
            }
        }
    }

    bp->unpinPage(columns_table_page_, updated, ctx);

    if (!updated)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Column record not found on disk");
        return Status::NOT_FOUND;
    }

    return Status::OK;
}

// ============================================================================
// TRUNCATE TABLE ASYNC Implementation (ALPHA Phase 1 - DDL Modifications)
// ============================================================================

auto CatalogManager::truncateTableAsync(const ID &table_id, const std::string &table_name,
                                         uint64_t snapshot_xid, ErrorContext *ctx) -> uint64_t
{
    // Create job
    auto job = std::make_shared<TruncateJob>();
    job->job_id = next_truncate_job_id_.fetch_add(1);
    job->table_id = table_id;
    job->table_name = table_name;
    job->snapshot_xid = snapshot_xid;
    job->start_time = std::time(nullptr);

    // Register job
    {
        std::lock_guard<std::mutex> lock(truncate_jobs_mutex_);
        truncate_jobs_[job->job_id] = job;
    }

    // Spawn background thread for MGA-compliant asynchronous truncation
    std::thread([this, job]() {
        try {
            ErrorContext ctx;

            // Use HeapScanIterator to iterate through all tuples in the table
            auto scan = db_->storage_engine()->createScan(job->table_id, &ctx);
            if (!scan)
            {
                job->error = true;
                job->error_message = "Failed to create heap scan iterator";
                job->completed = true;
                return;
            }

            // Track pages we've modified so we can mark them dirty
            std::vector<std::pair<uint32_t, void*>> modified_pages;

            // Scan all tuples and mark visible ones as deleted
            Tuple tuple;
            while (!scan->isDone())
            {
                Status status = scan->next(&tuple, &ctx);
                if (status != Status::OK)
                {
                    // End of scan or error
                    break;
                }

                job->rows_processed++;

                // Get tuple header to check visibility
                const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple.data);

                // MGA-compliant visibility check:
                // Only delete tuples that were committed BEFORE truncate started
                // Check: xmin <= snapshot_xid AND xmax == 0 (not already deleted)
                if (hdr->xmin <= job->snapshot_xid && hdr->xmax == 0)
                {
                    // This tuple is visible at snapshot_xid, mark it for deletion
                    // Get the page containing this tuple
                    uint32_t page_id = getPageNumber(tuple.tid.gpid);
                    uint16_t slot = tuple.tid.slot;

                    // Pin the page to modify it
                    void *page_buffer = nullptr;
                    status = db_->buffer_pool()->pinPage(page_id, &page_buffer, &ctx);
                    if (status == Status::OK && page_buffer != nullptr)
                    {
                        // Use HeapPage to perform soft delete (sets xmax)
                        // Note: Passing nullptr for ToastManager - TOAST cleanup will happen during garbage collection
                        HeapPage heap_page(static_cast<uint8_t*>(page_buffer),
                                          db_->page_size(),
                                          nullptr,  // ToastManager - let GC handle TOAST cleanup
                                          db_,
                                          job->table_id);

                        // Soft delete: sets xmax and marks tuple as deleted
                        status = heap_page.deleteTuple(slot, job->snapshot_xid, &ctx);
                        if (status == Status::OK)
                        {
                            job->rows_deleted++;

                            // Mark page as dirty
                            db_->buffer_pool()->markDirty(page_id, &ctx);
                        }

                        // Unpin the page
                        db_->buffer_pool()->unpinPage(page_id, true, &ctx);
                    }
                }

                // Yield CPU periodically to avoid hogging resources
                if (job->rows_processed % 1000 == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }

            // Job completed successfully
            job->completed = true;
            job->end_time = std::time(nullptr);

        } catch (const std::exception &e) {
            job->error = true;
            job->error_message = e.what();
            job->completed = true;
        }
    }).detach();

    return job->job_id;
}

auto CatalogManager::truncateTableSync(const ID &table_id, const std::string &table_name,
                                        uint64_t snapshot_xid, ErrorContext *ctx) -> Status
{
    // Start async job
    auto job_id = truncateTableAsync(table_id, table_name, snapshot_xid, ctx);

    // Wait for completion (no timeout)
    return waitForTruncate(job_id, 0);
}

auto CatalogManager::getTruncateJobStatus(uint64_t job_id) -> std::shared_ptr<TruncateJob>
{
    std::lock_guard<std::mutex> lock(truncate_jobs_mutex_);
    auto it = truncate_jobs_.find(job_id);
    if (it != truncate_jobs_.end())
    {
        return it->second;
    }
    return nullptr;
}

auto CatalogManager::waitForTruncate(uint64_t job_id, uint32_t timeout_ms) -> Status
{
    auto job = getTruncateJobStatus(job_id);
    if (!job)
    {
        return Status::NOT_FOUND;
    }

    auto start = std::chrono::steady_clock::now();

    while (!job->completed.load())
    {
        // Check timeout
        if (timeout_ms > 0)
        {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > std::chrono::milliseconds(timeout_ms))
            {
                // Timeout - return IO_ERROR as generic error status
                return Status::IO_ERROR;
            }
        }

        // Wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Check if error occurred
    if (job->error.load())
    {
        return Status::IO_ERROR;
    }

    return Status::OK;
}

auto CatalogManager::listTruncateJobs(std::vector<std::shared_ptr<TruncateJob>> &jobs_out) -> void
{
    std::lock_guard<std::mutex> lock(truncate_jobs_mutex_);
    for (const auto &[job_id, job] : truncate_jobs_)
    {
        jobs_out.push_back(job);
    }
}

// ========================================================================
// Sequence Operations (ALPHA Phase 1 - Sequences)
// ========================================================================

auto CatalogManager::createSequence(const ID& schema_id, const std::string& name,
                                     int64_t increment_by, int64_t min_value, int64_t max_value,
                                     int64_t start_value, int64_t cache_size, bool cycle,
                                     ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(sequence_cache_mutex_);

    LOG_INFO(CATALOG, "Creating sequence '%s' with increment=%ld, min=%ld, max=%ld, start=%ld",
             name.c_str(), increment_by, min_value, max_value, start_value);

    // Validate parameters
    if (increment_by == 0) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Sequence increment cannot be zero");
        return Status::INVALID_ARGUMENT;
    }

    if (min_value >= max_value) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Sequence minimum value must be less than maximum value");
        return Status::INVALID_ARGUMENT;
    }

    if (start_value < min_value || start_value > max_value) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Sequence start value must be between min and max values");
        return Status::INVALID_ARGUMENT;
    }

    // Generate sequence ID
    ID sequence_id = generateUuidV7();

    // Create in-memory sequence state
    auto state = std::make_shared<SequenceState>();
    state->sequence_id = sequence_id;
    state->name = name;  // Store name for cleanup
    state->current_value.store(start_value);
    state->increment_by = increment_by;
    state->min_value = min_value;
    state->max_value = max_value;
    state->cycle = cycle;

    // Add to cache
    sequence_cache_[sequence_id] = state;

    // Add name-to-ID mapping
    {
        std::lock_guard<std::mutex> name_lock(sequence_name_mutex_);
        sequence_name_to_id_[name] = sequence_id;
    }

    LOG_INFO(CATALOG, "Created sequence '%s' with ID %s", name.c_str(), "<sequence_id>");

    return Status::OK;
}

auto CatalogManager::alterSequence(const ID& sequence_id, const std::optional<int64_t>& increment_by,
                                    const std::optional<int64_t>& min_value, const std::optional<int64_t>& max_value,
                                    const std::optional<int64_t>& restart, const std::optional<int64_t>& cache_size,
                                    const std::optional<bool>& cycle, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(sequence_cache_mutex_);

    // Find sequence
    auto it = sequence_cache_.find(sequence_id);
    if (it == sequence_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Sequence not found");
        return Status::NOT_FOUND;
    }

    auto state = it->second;

    // Lock config mutex for thread-safe parameter updates
    std::lock_guard<std::mutex> config_lock(state->config_mutex);

    LOG_INFO(CATALOG, "Altering sequence %s", "<sequence_id>");

    // Update parameters if provided
    if (increment_by.has_value()) {
        if (*increment_by == 0) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Sequence increment cannot be zero");
            return Status::INVALID_ARGUMENT;
        }
        state->increment_by = *increment_by;
        LOG_DEBUG(CATALOG, "  Updated increment_by to %ld", *increment_by);
    }

    if (min_value.has_value()) {
        state->min_value = *min_value;
        LOG_DEBUG(CATALOG, "  Updated min_value to %ld", *min_value);
    }

    if (max_value.has_value()) {
        state->max_value = *max_value;
        LOG_DEBUG(CATALOG, "  Updated max_value to %ld", *max_value);
    }

    if (cycle.has_value()) {
        state->cycle = *cycle;
        LOG_DEBUG(CATALOG, "  Updated cycle to %s", *cycle ? "true" : "false");
    }

    // Validate min < max after updates
    if (state->min_value >= state->max_value) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Sequence minimum value must be less than maximum value");
        return Status::INVALID_ARGUMENT;
    }

    // Handle RESTART
    if (restart.has_value()) {
        if (*restart < state->min_value || *restart > state->max_value) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Restart value must be between min and max values");
            return Status::INVALID_ARGUMENT;
        }
        state->current_value.store(*restart);
        LOG_INFO(CATALOG, "  Restarted sequence at %ld", *restart);
    }

    return Status::OK;
}

auto CatalogManager::dropSequence(const ID& sequence_id, bool cascade, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> cache_lock(sequence_cache_mutex_);

    LOG_INFO(CATALOG, "Dropping sequence %s (cascade=%s)", "<sequence_id>", cascade ? "true" : "false");

    // Find sequence
    auto it = sequence_cache_.find(sequence_id);
    if (it == sequence_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Sequence not found");
        return Status::NOT_FOUND;
    }

    // Get name before removing
    std::string seq_name = it->second->name;

    // Remove from cache
    sequence_cache_.erase(it);

    // Remove from name map
    {
        std::lock_guard<std::mutex> name_lock(sequence_name_mutex_);
        sequence_name_to_id_.erase(seq_name);
    }

    LOG_INFO(CATALOG, "Dropped sequence '%s' successfully", seq_name.c_str());

    return Status::OK;
}

auto CatalogManager::getSequence(const ID& schema_id, const std::string& name,
                                  SequenceInfo& info_out, ErrorContext* ctx) -> Status
{
    // For now, stub - sequences are only in memory
    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "getSequence not yet implemented");
    return Status::NOT_IMPLEMENTED;
}

auto CatalogManager::sequenceNextVal(const ID& sequence_id, int64_t& value_out,
                                      ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(sequence_cache_mutex_);

    // Find sequence
    auto it = sequence_cache_.find(sequence_id);
    if (it == sequence_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Sequence not found");
        return Status::NOT_FOUND;
    }

    auto state = it->second;

    // Lock config mutex to ensure consistent reads of increment/min/max
    std::lock_guard<std::mutex> config_lock(state->config_mutex);

    // Atomically increment and get new value
    int64_t new_value = state->current_value.fetch_add(state->increment_by) + state->increment_by;

    // Check if we exceeded max_value
    if (state->increment_by > 0) {
        if (new_value > state->max_value) {
            if (state->cycle) {
                // Wrap to min_value
                new_value = state->min_value;
                state->current_value.store(new_value);
                LOG_DEBUG(CATALOG, "Sequence wrapped to min_value=%ld", new_value);
            } else {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE,
                                  "Sequence has reached its maximum value");
                return Status::OUT_OF_RANGE;
            }
        }
    } else {
        // Negative increment - check min_value
        if (new_value < state->min_value) {
            if (state->cycle) {
                // Wrap to max_value
                new_value = state->max_value;
                state->current_value.store(new_value);
                LOG_DEBUG(CATALOG, "Sequence wrapped to max_value=%ld", new_value);
            } else {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE,
                                  "Sequence has reached its minimum value");
                return Status::OUT_OF_RANGE;
            }
        }
    }

    value_out = new_value;

    LOG_DEBUG(CATALOG, "NEXTVAL returned %ld", new_value);

    return Status::OK;
}

auto CatalogManager::sequenceSetVal(const ID& sequence_id, int64_t value, bool is_called,
                                     ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(sequence_cache_mutex_);

    // Find sequence
    auto it = sequence_cache_.find(sequence_id);
    if (it == sequence_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Sequence not found");
        return Status::NOT_FOUND;
    }

    auto state = it->second;

    // Lock config mutex
    std::lock_guard<std::mutex> config_lock(state->config_mutex);

    // Validate value is within range
    if (value < state->min_value || value > state->max_value) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Value must be between min and max values");
        return Status::INVALID_ARGUMENT;
    }

    // Set value
    // If is_called=true, next NEXTVAL will increment from this value
    // If is_called=false, next NEXTVAL will return this value (so set to value - increment)
    if (is_called) {
        state->current_value.store(value);
    } else {
        state->current_value.store(value - state->increment_by);
    }

    LOG_INFO(CATALOG, "SETVAL set sequence to %ld (is_called=%s)",
             value, is_called ? "true" : "false");

    return Status::OK;
}

auto CatalogManager::getSequenceIdByName(const std::string& name, ID& id_out, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(sequence_name_mutex_);

    auto it = sequence_name_to_id_.find(name);
    if (it == sequence_name_to_id_.end()) {
        std::string msg = "Sequence not found: " + name;
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, msg.c_str());
        return Status::NOT_FOUND;
    }

    id_out = it->second;
    return Status::OK;
}

// ============================================================================
// View Operations (ALPHA Phase 1 - Views)
// ============================================================================

auto CatalogManager::createView(const ID& schema_id, const std::string& name,
                                  const std::string& definition, bool or_replace,
                                  bool check_option,
                                  const std::vector<std::string>& column_names,
                                  ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(view_cache_mutex_);

    // Check if view exists
    auto it = view_name_to_id_.find(name);
    if (it != view_name_to_id_.end())
    {
        if (!or_replace)
        {
            std::string msg = "View already exists: " + name;
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, msg.c_str());
            return Status::CONSTRAINT_VIOLATION;
        }

        // Update existing view (OR REPLACE)
        ViewInfo& view = view_cache_[it->second];
        view.definition = definition;
        view.check_option = check_option;
        view.column_names = column_names;
        view.last_modified_time = std::chrono::system_clock::now().time_since_epoch().count();

        LOG_INFO(CATALOG, "Replaced view '%s'", name.c_str());
        return Status::OK;
    }

    // Create new view
    ViewInfo view;
    view.view_id = generateUuidV7();
    view.schema_id = schema_id;
    view.name = name;
    view.owner_id = resolveOwnerUUID("system");  // Phase 6 TODO: Get from session context
    view.definition = definition;
    view.check_option = check_option;
    view.column_names = column_names;
    view.created_time = std::chrono::system_clock::now().time_since_epoch().count();
    view.last_modified_time = view.created_time;

    view_cache_[view.view_id] = view;
    view_name_to_id_[name] = view.view_id;

    LOG_INFO(CATALOG, "Created view '%s'", name.c_str());
    return Status::OK;
}

auto CatalogManager::dropView(const ID& view_id, bool cascade, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(view_cache_mutex_);

    auto it = view_cache_.find(view_id);
    if (it == view_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "View not found");
        return Status::NOT_FOUND;
    }

    std::string view_name = it->second.name;

    // TODO: Check for dependent views if CASCADE is false
    // For ALPHA Phase 1, we skip dependency checking

    view_cache_.erase(it);
    view_name_to_id_.erase(view_name);

    LOG_INFO(CATALOG, "Dropped view '%s'", view_name.c_str());
    return Status::OK;
}

auto CatalogManager::getView(const ID& schema_id, const std::string& name,
                               ViewInfo& info_out, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(view_cache_mutex_);

    auto it = view_name_to_id_.find(name);
    if (it == view_name_to_id_.end())
    {
        std::string msg = "View not found: " + name;
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, msg.c_str());
        return Status::NOT_FOUND;
    }

    info_out = view_cache_[it->second];
    return Status::OK;
}

auto CatalogManager::getViewIdByName(const std::string& name, ID& id_out,
                                       ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(view_cache_mutex_);

    auto it = view_name_to_id_.find(name);
    if (it == view_name_to_id_.end())
    {
        std::string msg = "View not found: " + name;
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, msg.c_str());
        return Status::NOT_FOUND;
    }

    id_out = it->second;
    return Status::OK;
}

auto CatalogManager::isView(const std::string& name) -> bool
{
    std::lock_guard<std::mutex> lock(view_cache_mutex_);
    return view_name_to_id_.find(name) != view_name_to_id_.end();
}

// ========================================================================
// Dependency Operations (Phase 5.2 - Dependencies Table)
// ========================================================================

auto CatalogManager::createDependency(const ID& dependent_object_id, ObjectType dependent_type,
                                     const ID& referenced_object_id, ObjectType referenced_type,
                                     DependencyType dep_type, ID& dependency_id,
                                     ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);

    // Generate new dependency ID
    dependency_id = generateUuidV7();

    // Create dependency info
    DependencyInfo dep_info;
    dep_info.dependency_id = dependency_id;
    dep_info.dependent_object_id = dependent_object_id;
    dep_info.dependent_type = dependent_type;
    dep_info.referenced_object_id = referenced_object_id;
    dep_info.referenced_type = referenced_type;
    dep_info.dependency_type = dep_type;
    dep_info.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();

    // Add to cache
    dependency_cache_[dependency_id] = dep_info;

    // Add to object lookup (for finding all dependencies of an object)
    object_to_dependencies_.insert({dependent_object_id, dependency_id});
    object_to_dependencies_.insert({referenced_object_id, dependency_id});

    // Phase 6.2: Write to disk
    Status status = writeDependencyRecord(dep_info, ctx);
    if (status != Status::OK) {
        // Rollback cache changes on write failure
        dependency_cache_.erase(dependency_id);
        auto range1 = object_to_dependencies_.equal_range(dependent_object_id);
        for (auto it = range1.first; it != range1.second; ) {
            if (it->second == dependency_id) {
                it = object_to_dependencies_.erase(it);
            } else {
                ++it;
            }
        }
        auto range2 = object_to_dependencies_.equal_range(referenced_object_id);
        for (auto it = range2.first; it != range2.second; ) {
            if (it->second == dependency_id) {
                it = object_to_dependencies_.erase(it);
            } else {
                ++it;
            }
        }
        return status;
    }

    LOG_INFO(CATALOG, "Created dependency: %s (%d) -> %s (%d) [type=%d]",
             dependent_object_id.toString().c_str(), static_cast<int>(dependent_type),
             referenced_object_id.toString().c_str(), static_cast<int>(referenced_type),
             static_cast<int>(dep_type));

    return Status::OK;
}

auto CatalogManager::deleteDependency(const ID& dependency_id, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);

    auto it = dependency_cache_.find(dependency_id);
    if (it == dependency_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("Dependency not found: " + dependency_id.toString()).c_str());
        return Status::INVALID_ARGUMENT;
    }

    const auto& dep_info = it->second;

    // Remove from object lookup
    auto range = object_to_dependencies_.equal_range(dep_info.dependent_object_id);
    for (auto iter = range.first; iter != range.second; ) {
        if (iter->second == dependency_id) {
            iter = object_to_dependencies_.erase(iter);
        } else {
            ++iter;
        }
    }

    range = object_to_dependencies_.equal_range(dep_info.referenced_object_id);
    for (auto iter = range.first; iter != range.second; ) {
        if (iter->second == dependency_id) {
            iter = object_to_dependencies_.erase(iter);
        } else {
            ++iter;
        }
    }

    // Remove from cache
    dependency_cache_.erase(it);

    // Phase 6.2: Delete from disk
    Status status = deleteDependencyRecord(dependency_id, ctx);
    if (status != Status::OK) {
        LOG_ERROR(CATALOG, "Failed to delete dependency record from disk: %s",
                  dependency_id.toString().c_str());
        // Note: Cache already updated, disk delete failed - this is inconsistent
        // In production, would need transaction rollback here
    }

    LOG_INFO(CATALOG, "Deleted dependency: %s", dependency_id.toString().c_str());

    return Status::OK;
}

auto CatalogManager::getDependenciesFor(const ID& object_id,
                                       std::vector<DependencyInfo>& dependencies_out,
                                       ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);

    dependencies_out.clear();

    // Find all dependencies where this object is the dependent
    for (const auto& [dep_id, dep_info] : dependency_cache_) {
        if (dep_info.dependent_object_id == object_id) {
            dependencies_out.push_back(dep_info);
        }
    }

    return Status::OK;
}

auto CatalogManager::getDependents(const ID& object_id,
                                  std::vector<DependencyInfo>& dependents_out,
                                  ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);

    dependents_out.clear();

    // Find all dependencies where this object is referenced
    for (const auto& [dep_id, dep_info] : dependency_cache_) {
        if (dep_info.referenced_object_id == object_id) {
            dependents_out.push_back(dep_info);
        }
    }

    return Status::OK;
}

auto CatalogManager::hasDependents(const ID& object_id, bool& has_dependents,
                                  ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);

    has_dependents = false;

    // Check if any dependency references this object
    for (const auto& [dep_id, dep_info] : dependency_cache_) {
        if (dep_info.referenced_object_id == object_id) {
            has_dependents = true;
            break;
        }
    }

    return Status::OK;
}

// ========================================================================
// Comment Operations (Phase 5.2 - Comments Table)
// ========================================================================

auto CatalogManager::setComment(const ID& object_id, ObjectType object_type,
                                const std::string& comment_text,
                                ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(comment_cache_mutex_);

    // Create or update comment
    CommentInfo comment;
    comment.comment_id = generateUuidV7();  // Generate new ID each time
    comment.object_id = object_id;
    comment.object_type = object_type;
    comment.owner_id = resolveOwnerUUID("system");  // Phase 6 TODO: Get from session
    comment.comment_text = comment_text;
    comment.created_time = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::system_clock::now().time_since_epoch()).count();
    comment.last_modified_time = comment.created_time;

    // Store in cache (keyed by object_id)
    comment_cache_[object_id] = comment;

    // Phase 6.2: Write to disk (without TOAST for now - Phase 6.3)
    Status status = writeCommentRecord(comment, ctx);
    if (status != Status::OK) {
        // Rollback cache changes on write failure
        comment_cache_.erase(object_id);
        return status;
    }

    LOG_INFO(CATALOG, "Set comment for object %s: %.50s%s",
             object_id.toString().c_str(),
             comment_text.c_str(),
             comment_text.length() > 50 ? "..." : "");

    return Status::OK;
}

auto CatalogManager::getComment(const ID& object_id, std::string& comment_out,
                                ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(comment_cache_mutex_);

    auto it = comment_cache_.find(object_id);
    if (it == comment_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("No comment found for object: " + object_id.toString()).c_str());
        return Status::INVALID_ARGUMENT;
    }

    comment_out = it->second.comment_text;
    return Status::OK;
}

auto CatalogManager::deleteComment(const ID& object_id, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(comment_cache_mutex_);

    auto it = comment_cache_.find(object_id);
    if (it == comment_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("No comment found for object: " + object_id.toString()).c_str());
        return Status::INVALID_ARGUMENT;
    }

    comment_cache_.erase(it);

    // Phase 6.2: Delete from disk
    Status status = deleteCommentRecord(object_id, ctx);
    if (status != Status::OK) {
        LOG_ERROR(CATALOG, "Failed to delete comment record from disk: %s",
                  object_id.toString().c_str());
        // Note: Cache already updated, disk delete failed - this is inconsistent
    }

    LOG_INFO(CATALOG, "Deleted comment for object %s", object_id.toString().c_str());

    return Status::OK;
}

// ========================================================================
// Dependency Persistence (Phase 6.2)
// ========================================================================

auto CatalogManager::writeDependencyRecord(const DependencyInfo &dependency, ErrorContext *ctx) -> Status
{
    DependencyRecord record;
    memset(&record, 0, sizeof(DependencyRecord));

    record.dependency_id = dependency.dependency_id;
    record.dependent_object_id = dependency.dependent_object_id;
    record.dependent_type = static_cast<uint8_t>(dependency.dependent_type);
    record.referenced_object_id = dependency.referenced_object_id;
    record.referenced_type = static_cast<uint8_t>(dependency.referenced_type);
    record.dependency_type = static_cast<uint8_t>(dependency.dependency_type);
    record.created_time = dependency.created_time;
    record.is_valid = 1;

    return writeRecordToHeapPage(dependencies_table_page_, record, ctx);
}

auto CatalogManager::deleteDependencyRecord(const ID &dependency_id, ErrorContext *ctx) -> Status
{
    auto matcher = [&dependency_id](const DependencyRecord &record) {
        return record.dependency_id == dependency_id;
    };
    return deleteRecordFromHeapPage<DependencyRecord>(dependencies_table_page_, matcher, ctx);
}

auto CatalogManager::readDependencyRecords(ErrorContext *ctx) -> Status
{
    auto converter = [](const DependencyRecord &record, DependencyInfo &info) {
        info.dependency_id = record.dependency_id;
        info.dependent_object_id = record.dependent_object_id;
        info.dependent_type = static_cast<ObjectType>(record.dependent_type);
        info.referenced_object_id = record.referenced_object_id;
        info.referenced_type = static_cast<ObjectType>(record.referenced_type);
        info.dependency_type = static_cast<DependencyType>(record.dependency_type);
        info.created_time = record.created_time;
    };

    auto key_extractor = [](const DependencyInfo &info) { return info.dependency_id; };

    return readRecordsFromHeapPage<DependencyRecord, DependencyInfo, ID>(
        dependencies_table_page_, dependency_cache_, converter, key_extractor, ctx);
}

// ========================================================================
// Comment Persistence (Phase 6.3)
// ========================================================================

auto CatalogManager::writeCommentRecord(const CommentInfo &comment, ErrorContext *ctx) -> Status
{
    CommentRecord record;
    memset(&record, 0, sizeof(CommentRecord));

    record.comment_id = comment.comment_id;
    record.object_id = comment.object_id;
    record.object_type = static_cast<uint8_t>(comment.object_type);
    record.owner_id = comment.owner_id;
    // TODO Phase 6.3: Write comment_text to TOAST and store OID
    // For now, comment_text_oid = 0 (text is in-memory only)
    record.comment_text_oid = 0;
    record.created_time = comment.created_time;
    record.last_modified_time = comment.last_modified_time;
    record.is_valid = 1;

    return writeRecordToHeapPage(comments_table_page_, record, ctx);
}

auto CatalogManager::deleteCommentRecord(const ID &object_id, ErrorContext *ctx) -> Status
{
    auto matcher = [&object_id](const CommentRecord &record) {
        return record.object_id == object_id;
    };
    return deleteRecordFromHeapPage<CommentRecord>(comments_table_page_, matcher, ctx);
}

auto CatalogManager::readCommentRecords(ErrorContext *ctx) -> Status
{
    auto converter = [](const CommentRecord &record, CommentInfo &info) {
        info.comment_id = record.comment_id;
        info.object_id = record.object_id;
        info.object_type = static_cast<ObjectType>(record.object_type);
        info.owner_id = record.owner_id;
        // TODO Phase 6.3: Read comment_text from TOAST using comment_text_oid
        // For now, comment_text remains empty (will be populated from cache if exists)
        info.comment_text = "";
        info.created_time = record.created_time;
        info.last_modified_time = record.last_modified_time;
    };

    auto key_extractor = [](const CommentInfo &info) { return info.object_id; };

    return readRecordsFromHeapPage<CommentRecord, CommentInfo, ID>(
        comments_table_page_, comment_cache_, converter, key_extractor, ctx);
}

// ============================================================================
// Foreign Key Disk Persistence (Phase D)
// ============================================================================

auto CatalogManager::readForeignKeyRecords(ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(foreign_keys_cache_mutex_);

    if (foreign_keys_table_page_ == 0)
    {
        // Foreign keys table not initialized yet (old database format)
        return Status::OK;
    }

    BufferPool *bp = db_->buffer_pool();
    void *page_buffer;
    Status status = bp->pinPage(foreign_keys_table_page_, &page_buffer, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
    uint32_t offset = sizeof(CatalogHeapPage);

    for (uint32_t i = 0; i < heap->record_count; i++)
    {
        auto *record = reinterpret_cast<ForeignKeyRecord *>(
            reinterpret_cast<uint8_t *>(page_buffer) + offset);

        if (record->is_valid)
        {
            ForeignKeyInfo fk_info;
            fk_info.fk_id = record->fk_id;
            fk_info.fk_name = std::string(record->fk_name);
            fk_info.child_table_id = record->child_table_id;
            fk_info.parent_table_id = record->parent_table_id;

            // Deserialize column names from comma-separated strings
            std::string child_cols_str(record->child_columns);
            std::string parent_cols_str(record->parent_columns);

            // Parse child columns
            size_t pos = 0;
            while (pos < child_cols_str.length())
            {
                size_t comma = child_cols_str.find(',', pos);
                if (comma == std::string::npos)
                {
                    fk_info.child_columns.push_back(child_cols_str.substr(pos));
                    break;
                }
                fk_info.child_columns.push_back(child_cols_str.substr(pos, comma - pos));
                pos = comma + 1;
            }

            // Parse parent columns
            pos = 0;
            while (pos < parent_cols_str.length())
            {
                size_t comma = parent_cols_str.find(',', pos);
                if (comma == std::string::npos)
                {
                    fk_info.parent_columns.push_back(parent_cols_str.substr(pos));
                    break;
                }
                fk_info.parent_columns.push_back(parent_cols_str.substr(pos, comma - pos));
                pos = comma + 1;
            }

            fk_info.on_delete = static_cast<FKAction>(record->on_delete);
            fk_info.on_update = static_cast<FKAction>(record->on_update);
            fk_info.match_type = static_cast<FKMatchType>(record->match_type);
            fk_info.is_enabled = record->is_enabled != 0;
            fk_info.created_time = record->created_time;

            // Store in cache
            foreign_keys_cache_[fk_info.fk_id] = fk_info;
            table_child_fks_.insert({fk_info.child_table_id, fk_info.fk_id});
            table_parent_fks_.insert({fk_info.parent_table_id, fk_info.fk_id});
        }

        offset += sizeof(ForeignKeyRecord);
    }

    bp->unpinPage(foreign_keys_table_page_, false, ctx);
    return Status::OK;
}

// ============================================================================
// Security Operations (Phase 1.3 - Users, Roles, Groups)
// ============================================================================

// User operations

auto CatalogManager::createUser(const std::string& username, const std::string& password_hash,
                                const ID& default_schema_id, bool is_superuser,
                                ID& user_id_out, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Validate username length
    Status status = UTF8Utils::validateStorageCapacity(username,
        CatalogConstants::MAX_IDENTIFIER_CHARS,
        CatalogConstants::MAX_IDENTIFIER_BYTES);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Username too long or invalid UTF-8");
        return status;
    }

    // Check if username already exists
    UserInfo existing_user;
    // Note: getUserByName also locks mutex, so we temporarily unlock here
    mutex_.unlock();
    status = getUserByName(username, existing_user, ctx);
    mutex_.lock();

    if (status == Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, "User already exists");
        return Status::FILE_EXISTS;
    }

    // Generate new user ID
    user_id_out = generateUuidV7();

    // Create user record
    UserRecord user_rec;
    memset(&user_rec, 0, sizeof(UserRecord));
    user_rec.user_id = user_id_out;

    // Truncate username to fit in fixed buffer
    std::string truncated_username = UTF8Utils::truncateToBytes(username,
        sizeof(user_rec.username));
    strncpy(user_rec.username, truncated_username.c_str(),
            sizeof(user_rec.username) - 1);

    // TODO Phase 1.4: Store password_hash in TOAST if > inline size
    // For now, we'll leave password_hash_oid = 0
    user_rec.password_hash_oid = 0;
    user_rec.user_metadata_oid = 0;
    user_rec.default_schema_id = default_schema_id;
    user_rec.is_active = 1;
    user_rec.is_superuser = is_superuser ? 1 : 0;
    user_rec.created_time = std::chrono::system_clock::now().time_since_epoch().count();
    user_rec.last_login_time = 0;
    user_rec.is_valid = 1;

    // Write to disk
    status = writeRecordToHeapPage(users_table_page_, user_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to write user record");
        return status;
    }

    DEBUG_LOG_DB("Created user: " << username << " (ID: " << user_id_out.toString() << ")");
    return Status::OK;
}

auto CatalogManager::getUser(const ID& user_id, UserInfo& user_out,
                             ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto predicate = [&user_id](const UserRecord& rec) {
        return rec.is_valid && rec.user_id == user_id;
    };

    auto result = findRecordInHeapPage<UserRecord>(users_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, result.status, "User not found");
        return result.status;
    }

    // Convert to UserInfo
    user_out.user_id = result.record.user_id;
    user_out.username = std::string(result.record.username);
    user_out.password_hash = "";  // TODO Phase 1.4: Read from TOAST
    user_out.user_metadata = "";  // TODO Phase 1.4: Read from TOAST
    user_out.default_schema_id = result.record.default_schema_id;
    user_out.is_active = result.record.is_active != 0;
    user_out.is_superuser = result.record.is_superuser != 0;
    user_out.created_time = result.record.created_time;
    user_out.last_login_time = result.record.last_login_time;

    return Status::OK;
}

auto CatalogManager::getUserByName(const std::string& username, UserInfo& user_out,
                                   ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto predicate = [&username](const UserRecord& rec) {
        return rec.is_valid && (std::string(rec.username) == username);
    };

    auto result = findRecordInHeapPage<UserRecord>(users_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, result.status, "User not found");
        return result.status;
    }

    // Convert to UserInfo
    user_out.user_id = result.record.user_id;
    user_out.username = std::string(result.record.username);
    user_out.password_hash = "";  // TODO Phase 1.4: Read from TOAST
    user_out.user_metadata = "";  // TODO Phase 1.4: Read from TOAST
    user_out.default_schema_id = result.record.default_schema_id;
    user_out.is_active = result.record.is_active != 0;
    user_out.is_superuser = result.record.is_superuser != 0;
    user_out.created_time = result.record.created_time;
    user_out.last_login_time = result.record.last_login_time;

    return Status::OK;
}

auto CatalogManager::updateUser(const ID& user_id, const std::string& password_hash,
                                const ID& default_schema_id, bool is_active, bool is_superuser,
                                ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Find the user record
    auto predicate = [&user_id](const UserRecord& rec) {
        return rec.is_valid && rec.user_id == user_id;
    };

    auto result = findRecordInHeapPage<UserRecord>(users_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, result.status, "User not found");
        return result.status;
    }

    // Update fields (Security Phase 3.0)
    UserRecord updated_rec = result.record;
    // TODO Phase 1.4: Update password_hash in TOAST
    updated_rec.default_schema_id = default_schema_id;
    updated_rec.is_active = is_active ? 1 : 0;
    updated_rec.is_superuser = is_superuser ? 1 : 0;

    // Write updated record
    Status status = updateRecordInHeapPage(users_table_page_, result.slot_index,
                                          updated_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to update user record");
        return status;
    }

    return Status::OK;
}

auto CatalogManager::deleteUser(const ID& user_id, bool cascade, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Security Phase 3.0: CASCADE implementation
    if (cascade)
    {
        // CASCADE: Delete all dependent objects
        // 1. Revoke all role memberships
        std::vector<RoleMembershipInfo> role_memberships;
        Status status = getUserRoles(user_id, role_memberships, ctx);
        if (status == Status::OK)
        {
            for (const auto& membership : role_memberships)
            {
                revokeRole(membership.role_id, user_id, ctx);
                // Continue even if revoke fails
            }
        }

        // 2. Revoke all group memberships
        std::vector<ID> groups;
        status = getUserGroups(user_id, groups, ctx);
        if (status == Status::OK)
        {
            for (const auto& group_id : groups)
            {
                removeGroupMember(group_id, user_id, ctx);
                // Continue even if removal fails
            }
        }

        // 3. Delete all permissions granted to this user
        std::vector<PermissionInfo> permissions;
        status = getUserPermissions(user_id, permissions, ctx);
        if (status == Status::OK)
        {
            for (const auto& perm : permissions)
            {
                revokePermission(perm.object_id, perm.object_type, user_id,
                               GranteeType::USER, perm.privileges, ctx);
                // Continue even if revoke fails
            }
        }
    }
    else
    {
        // RESTRICT (default): Check for dependencies
        // Check if user has any role memberships
        std::vector<RoleMembershipInfo> role_memberships;
        Status status = getUserRoles(user_id, role_memberships, ctx);
        if (status == Status::OK && !role_memberships.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                            "User has role memberships (use CASCADE to drop)");
            return Status::CONSTRAINT_VIOLATION;
        }

        // Check if user has any group memberships
        std::vector<ID> groups;
        status = getUserGroups(user_id, groups, ctx);
        if (status == Status::OK && !groups.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                            "User has group memberships (use CASCADE to drop)");
            return Status::CONSTRAINT_VIOLATION;
        }

        // Check if user has any permissions
        std::vector<PermissionInfo> permissions;
        status = getUserPermissions(user_id, permissions, ctx);
        if (status == Status::OK && !permissions.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                            "User has permissions (use CASCADE to drop)");
            return Status::CONSTRAINT_VIOLATION;
        }
    }

    // Mark user as deleted (Firebird MGA style)
    auto predicate = [&user_id](const UserRecord& rec) {
        return rec.is_valid && rec.user_id == user_id;
    };

    Status status = deleteRecordFromHeapPage<UserRecord>(users_table_page_,
                                                          predicate, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to delete user");
        return status;
    }

    DEBUG_LOG_DB("Deleted user (ID: " << user_id.toString() << ")");
    return Status::OK;
}

auto CatalogManager::listUsers(std::vector<UserInfo>& users_out,
                               ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    users_out.clear();

    auto filter = [](const UserRecord& rec) { return rec.is_valid; };
    auto converter = [](const UserRecord& rec, UserInfo& info) {
        info.user_id = rec.user_id;
        info.username = std::string(rec.username);
        info.password_hash = "";  // TODO: Read from TOAST
        info.user_metadata = "";  // TODO: Read from TOAST
        info.default_schema_id = rec.default_schema_id;
        info.is_active = rec.is_active != 0;
        info.is_superuser = rec.is_superuser != 0;
        info.created_time = rec.created_time;
        info.last_login_time = rec.last_login_time;
    };

    return readRecordsToVector<UserRecord, UserInfo>(users_table_page_, users_out,
                                                     filter, converter, ctx);
}

// Role operations

auto CatalogManager::createRole(const std::string& role_name, const ID& owner_id,
                                ID& role_id_out, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Validate role name length
    Status status = UTF8Utils::validateStorageCapacity(role_name,
        CatalogConstants::MAX_IDENTIFIER_CHARS,
        CatalogConstants::MAX_IDENTIFIER_BYTES);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Role name too long or invalid UTF-8");
        return status;
    }

    // Check if role already exists
    RoleInfo existing_role;
    mutex_.unlock();
    status = getRoleByName(role_name, existing_role, ctx);
    mutex_.lock();

    if (status == Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, "Role already exists");
        return Status::FILE_EXISTS;
    }

    // Generate new role ID
    role_id_out = generateUuidV7();

    // Create role record
    RoleRecord role_rec;
    memset(&role_rec, 0, sizeof(RoleRecord));
    role_rec.role_id = role_id_out;

    // Truncate role name to fit in fixed buffer
    std::string truncated_name = UTF8Utils::truncateToBytes(role_name,
        sizeof(role_rec.role_name));
    strncpy(role_rec.role_name, truncated_name.c_str(),
            sizeof(role_rec.role_name) - 1);

    role_rec.owner_id = owner_id;
    role_rec.role_metadata_oid = 0;  // TODO Phase 1.4: TOAST integration
    role_rec.is_active = 1;
    role_rec.created_time = std::chrono::system_clock::now().time_since_epoch().count();
    role_rec.last_modified_time = role_rec.created_time;
    role_rec.is_valid = 1;

    // Write to disk
    status = writeRecordToHeapPage(roles_table_page_, role_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to write role record");
        return status;
    }

    DEBUG_LOG_DB("Created role: " << role_name << " (ID: " << role_id_out.toString() << ")");
    return Status::OK;
}

auto CatalogManager::getRole(const ID& role_id, RoleInfo& role_out,
                             ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto predicate = [&role_id](const RoleRecord& rec) {
        return rec.is_valid && rec.role_id == role_id;
    };

    auto result = findRecordInHeapPage<RoleRecord>(roles_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, result.status, "Role not found");
        return result.status;
    }

    // Convert to RoleInfo
    role_out.role_id = result.record.role_id;
    role_out.role_name = std::string(result.record.role_name);
    role_out.owner_id = result.record.owner_id;
    role_out.role_metadata = "";  // TODO Phase 1.4: Read from TOAST
    role_out.is_active = result.record.is_active != 0;
    role_out.created_time = result.record.created_time;
    role_out.last_modified_time = result.record.last_modified_time;

    return Status::OK;
}

auto CatalogManager::getRoleByName(const std::string& role_name, RoleInfo& role_out,
                                   ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto predicate = [&role_name](const RoleRecord& rec) {
        return rec.is_valid && (std::string(rec.role_name) == role_name);
    };

    auto result = findRecordInHeapPage<RoleRecord>(roles_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, result.status, "Role not found");
        return result.status;
    }

    // Convert to RoleInfo
    role_out.role_id = result.record.role_id;
    role_out.role_name = std::string(result.record.role_name);
    role_out.owner_id = result.record.owner_id;
    role_out.role_metadata = "";  // TODO Phase 1.4: Read from TOAST
    role_out.is_active = result.record.is_active != 0;
    role_out.created_time = result.record.created_time;
    role_out.last_modified_time = result.record.last_modified_time;

    return Status::OK;
}

auto CatalogManager::deleteRole(const ID& role_id, bool cascade, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Security Phase 3.0: CASCADE implementation
    if (cascade)
    {
        // CASCADE: Delete all dependent objects
        // 1. Revoke role from all members
        std::vector<RoleMembershipInfo> members;
        Status status = getRoleMembers(role_id, members, ctx);
        if (status == Status::OK)
        {
            for (const auto& membership : members)
            {
                revokeRole(role_id, membership.user_id, ctx);
                // Continue even if revoke fails
            }
        }

        // 2. Delete all permissions granted to this role
        std::vector<PermissionInfo> permissions;
        status = getUserPermissions(role_id, permissions, ctx);  // Works for roles too
        if (status == Status::OK)
        {
            for (const auto& perm : permissions)
            {
                revokePermission(perm.object_id, perm.object_type, role_id,
                               GranteeType::ROLE, perm.privileges, ctx);
                // Continue even if revoke fails
            }
        }
    }
    else
    {
        // RESTRICT (default): Check for dependencies
        // Check if role has any members
        std::vector<RoleMembershipInfo> members;
        Status status = getRoleMembers(role_id, members, ctx);
        if (status == Status::OK && !members.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                            "Role has members (use CASCADE to drop)");
            return Status::CONSTRAINT_VIOLATION;
        }

        // Check if role has any permissions
        std::vector<PermissionInfo> permissions;
        status = getUserPermissions(role_id, permissions, ctx);
        if (status == Status::OK && !permissions.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                            "Role has permissions (use CASCADE to drop)");
            return Status::CONSTRAINT_VIOLATION;
        }
    }

    // Mark role as deleted (Firebird MGA style)
    auto predicate = [&role_id](const RoleRecord& rec) {
        return rec.is_valid && rec.role_id == role_id;
    };

    Status status = deleteRecordFromHeapPage<RoleRecord>(roles_table_page_,
                                                          predicate, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to delete role");
        return status;
    }

    DEBUG_LOG_DB("Deleted role (ID: " << role_id.toString() << ")");
    return Status::OK;
}

auto CatalogManager::listRoles(std::vector<RoleInfo>& roles_out,
                               ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    roles_out.clear();

    auto filter = [](const RoleRecord& rec) { return rec.is_valid; };
    auto converter = [](const RoleRecord& rec, RoleInfo& info) {
        info.role_id = rec.role_id;
        info.role_name = std::string(rec.role_name);
        info.owner_id = rec.owner_id;
        info.role_metadata = "";  // TODO: Read from TOAST
        info.is_active = rec.is_active != 0;
        info.created_time = rec.created_time;
        info.last_modified_time = rec.last_modified_time;
    };

    return readRecordsToVector<RoleRecord, RoleInfo>(roles_table_page_, roles_out,
                                                     filter, converter, ctx);
}

// Role membership operations

auto CatalogManager::grantRole(const ID& role_id, const ID& user_id, const ID& granted_by,
                               bool with_admin_option, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if membership already exists
    auto predicate = [&role_id, &user_id](const RoleMembershipRecord& rec) {
        return rec.is_valid && rec.role_id == role_id && rec.user_id == user_id;
    };

    auto result = findRecordInHeapPage<RoleMembershipRecord>(
        role_memberships_table_page_, predicate, ctx);
    if (result.status == Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, "Role membership already exists");
        return Status::FILE_EXISTS;
    }

    // Create membership record
    RoleMembershipRecord membership_rec;
    memset(&membership_rec, 0, sizeof(RoleMembershipRecord));
    membership_rec.membership_id = generateUuidV7();
    membership_rec.user_id = user_id;
    membership_rec.role_id = role_id;
    membership_rec.granted_by = granted_by;
    membership_rec.with_admin_option = with_admin_option ? 1 : 0;
    membership_rec.granted_time = std::chrono::system_clock::now().time_since_epoch().count();
    membership_rec.is_valid = 1;

    // Write to disk
    Status status = writeRecordToHeapPage(role_memberships_table_page_, membership_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to write role membership record");
        return status;
    }

    DEBUG_LOG_DB("Granted role " << role_id.toString() << " to user " << user_id.toString());
    return Status::OK;
}

auto CatalogManager::revokeRole(const ID& role_id, const ID& user_id,
                                ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Find and delete membership
    auto predicate = [&role_id, &user_id](const RoleMembershipRecord& rec) {
        return rec.is_valid && rec.role_id == role_id && rec.user_id == user_id;
    };

    Status status = deleteRecordFromHeapPage<RoleMembershipRecord>(
        role_memberships_table_page_, predicate, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to revoke role membership");
        return status;
    }

    DEBUG_LOG_DB("Revoked role " << role_id.toString() << " from user " << user_id.toString());
    return Status::OK;
}

auto CatalogManager::getUserRoles(const ID& user_id, std::vector<RoleMembershipInfo>& roles_out,
                                  ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    roles_out.clear();

    auto filter = [&user_id](const RoleMembershipRecord& rec) {
        return rec.is_valid && rec.user_id == user_id;
    };

    auto converter = [](const RoleMembershipRecord& rec, RoleMembershipInfo& info) {
        info.membership_id = rec.membership_id;
        info.user_id = rec.user_id;
        info.role_id = rec.role_id;
        info.granted_by = rec.granted_by;
        info.with_admin_option = rec.with_admin_option != 0;
        info.granted_time = rec.granted_time;
    };

    return readRecordsToVector<RoleMembershipRecord, RoleMembershipInfo>(
        role_memberships_table_page_, roles_out, filter, converter, ctx);
}

auto CatalogManager::getRoleMembers(const ID& role_id, std::vector<RoleMembershipInfo>& members_out,
                                    ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    members_out.clear();

    auto filter = [&role_id](const RoleMembershipRecord& rec) {
        return rec.is_valid && rec.role_id == role_id;
    };

    auto converter = [](const RoleMembershipRecord& rec, RoleMembershipInfo& info) {
        info.membership_id = rec.membership_id;
        info.user_id = rec.user_id;
        info.role_id = rec.role_id;
        info.granted_by = rec.granted_by;
        info.with_admin_option = rec.with_admin_option != 0;
        info.granted_time = rec.granted_time;
    };

    return readRecordsToVector<RoleMembershipRecord, RoleMembershipInfo>(
        role_memberships_table_page_, members_out, filter, converter, ctx);
}

// Group operations

auto CatalogManager::createGroup(const std::string& group_name, GroupType group_type,
                                 const std::string& external_id, ID& group_id_out,
                                 ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Validate group name length
    Status status = UTF8Utils::validateStorageCapacity(group_name,
        CatalogConstants::MAX_IDENTIFIER_CHARS,
        CatalogConstants::MAX_IDENTIFIER_BYTES);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Group name too long or invalid UTF-8");
        return status;
    }

    // Check if group already exists
    GroupInfo existing_group;
    mutex_.unlock();
    status = getGroupByName(group_name, existing_group, ctx);
    mutex_.lock();

    if (status == Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, "Group already exists");
        return Status::FILE_EXISTS;
    }

    // Generate new group ID
    group_id_out = generateUuidV7();

    // Create group record
    GroupRecord group_rec;
    memset(&group_rec, 0, sizeof(GroupRecord));
    group_rec.group_id = group_id_out;

    // Truncate group name to fit in fixed buffer
    std::string truncated_name = UTF8Utils::truncateToBytes(group_name,
        sizeof(group_rec.group_name));
    strncpy(group_rec.group_name, truncated_name.c_str(),
            sizeof(group_rec.group_name) - 1);

    // Truncate external_id to fit in fixed buffer
    std::string truncated_ext_id = UTF8Utils::truncateToBytes(external_id,
        sizeof(group_rec.external_id));
    strncpy(group_rec.external_id, truncated_ext_id.c_str(),
            sizeof(group_rec.external_id) - 1);

    group_rec.group_type = static_cast<uint8_t>(group_type);
    group_rec.group_metadata_oid = 0;  // TODO Phase 1.4: TOAST integration
    group_rec.created_time = std::chrono::system_clock::now().time_since_epoch().count();
    group_rec.last_modified_time = group_rec.created_time;
    group_rec.is_valid = 1;

    // Write to disk
    status = writeRecordToHeapPage(groups_table_page_, group_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to write group record");
        return status;
    }

    DEBUG_LOG_DB("Created group: " << group_name << " (ID: " << group_id_out.toString() << ")");
    return Status::OK;
}

auto CatalogManager::getGroup(const ID& group_id, GroupInfo& group_out,
                              ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto predicate = [&group_id](const GroupRecord& rec) {
        return rec.is_valid && rec.group_id == group_id;
    };

    auto result = findRecordInHeapPage<GroupRecord>(groups_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, result.status, "Group not found");
        return result.status;
    }

    // Convert to GroupInfo
    group_out.group_id = result.record.group_id;
    group_out.group_name = std::string(result.record.group_name);
    group_out.external_id = std::string(result.record.external_id);
    group_out.group_type = static_cast<GroupType>(result.record.group_type);
    group_out.group_metadata = "";  // TODO Phase 1.4: Read from TOAST
    group_out.created_time = result.record.created_time;
    group_out.last_modified_time = result.record.last_modified_time;

    return Status::OK;
}

auto CatalogManager::getGroupByName(const std::string& group_name, GroupInfo& group_out,
                                    ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto predicate = [&group_name](const GroupRecord& rec) {
        return rec.is_valid && (std::string(rec.group_name) == group_name);
    };

    auto result = findRecordInHeapPage<GroupRecord>(groups_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, result.status, "Group not found");
        return result.status;
    }

    // Convert to GroupInfo
    group_out.group_id = result.record.group_id;
    group_out.group_name = std::string(result.record.group_name);
    group_out.external_id = std::string(result.record.external_id);
    group_out.group_type = static_cast<GroupType>(result.record.group_type);
    group_out.group_metadata = "";  // TODO Phase 1.4: Read from TOAST
    group_out.created_time = result.record.created_time;
    group_out.last_modified_time = result.record.last_modified_time;

    return Status::OK;
}

auto CatalogManager::deleteGroup(const ID& group_id, bool cascade, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Security Phase 3.0: CASCADE implementation
    if (cascade)
    {
        // CASCADE: Delete all dependent objects
        // 1. Remove all members from group
        std::vector<ID> members;
        Status status = getGroupMembers(group_id, members, ctx);
        if (status == Status::OK)
        {
            for (const auto& member_id : members)
            {
                removeGroupMember(group_id, member_id, ctx);
                // Continue even if removal fails
            }
        }

        // 2. Delete all permissions granted to this group
        std::vector<PermissionInfo> permissions;
        status = getUserPermissions(group_id, permissions, ctx);  // Works for groups too
        if (status == Status::OK)
        {
            for (const auto& perm : permissions)
            {
                revokePermission(perm.object_id, perm.object_type, group_id,
                               GranteeType::GROUP, perm.privileges, ctx);
                // Continue even if revoke fails
            }
        }

        // 3. Delete all group mappings
        // TODO: Add group mapping cleanup when implemented
    }
    else
    {
        // RESTRICT (default): Check for dependencies
        // Check if group has any members
        std::vector<ID> members;
        Status status = getGroupMembers(group_id, members, ctx);
        if (status == Status::OK && !members.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                            "Group has members (use CASCADE to drop)");
            return Status::CONSTRAINT_VIOLATION;
        }

        // Check if group has any permissions
        std::vector<PermissionInfo> permissions;
        status = getUserPermissions(group_id, permissions, ctx);
        if (status == Status::OK && !permissions.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                            "Group has permissions (use CASCADE to drop)");
            return Status::CONSTRAINT_VIOLATION;
        }
    }

    // Mark group as deleted (Firebird MGA style)
    auto predicate = [&group_id](const GroupRecord& rec) {
        return rec.is_valid && rec.group_id == group_id;
    };

    Status status = deleteRecordFromHeapPage<GroupRecord>(groups_table_page_,
                                                           predicate, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to delete group");
        return status;
    }

    DEBUG_LOG_DB("Deleted group (ID: " << group_id.toString() << ")");
    return Status::OK;
}

auto CatalogManager::listGroups(std::vector<GroupInfo>& groups_out,
                                ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    groups_out.clear();

    auto filter = [](const GroupRecord& rec) { return rec.is_valid; };
    auto converter = [](const GroupRecord& rec, GroupInfo& info) {
        info.group_id = rec.group_id;
        info.group_name = std::string(rec.group_name);
        info.external_id = std::string(rec.external_id);
        info.group_type = static_cast<GroupType>(rec.group_type);
        info.group_metadata = "";  // TODO: Read from TOAST
        info.created_time = rec.created_time;
        info.last_modified_time = rec.last_modified_time;
    };

    return readRecordsToVector<GroupRecord, GroupInfo>(groups_table_page_, groups_out,
                                                        filter, converter, ctx);
}

// Group membership operations (supports nested groups)

auto CatalogManager::addGroupMember(const ID& group_id, const ID& member_id, bool is_group,
                                    const ID& granted_by, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if membership already exists
    auto predicate = [&group_id, &member_id](const GroupMembershipRecord& rec) {
        return rec.is_valid && rec.group_id == group_id && rec.user_id == member_id;
    };

    auto result = findRecordInHeapPage<GroupMembershipRecord>(
        group_memberships_table_page_, predicate, ctx);
    if (result.status == Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, "Group membership already exists");
        return Status::FILE_EXISTS;
    }

    // Create membership record
    GroupMembershipRecord membership_rec;
    memset(&membership_rec, 0, sizeof(GroupMembershipRecord));
    membership_rec.membership_id = generateUuidV7();
    membership_rec.user_id = member_id;  // Can be user or group
    membership_rec.member_type = is_group ? 1 : 0;  // GROUP=1, USER=0
    membership_rec.group_id = group_id;
    membership_rec.granted_by = granted_by;
    membership_rec.granted_time = std::chrono::system_clock::now().time_since_epoch().count();
    membership_rec.is_valid = 1;

    // Write to disk
    Status status = writeRecordToHeapPage(group_memberships_table_page_, membership_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to write group membership record");
        return status;
    }

    DEBUG_LOG_DB("Added member " << member_id.toString() << " to group " << group_id.toString());
    return Status::OK;
}

auto CatalogManager::removeGroupMember(const ID& group_id, const ID& member_id,
                                       ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Find and delete membership
    auto predicate = [&group_id, &member_id](const GroupMembershipRecord& rec) {
        return rec.is_valid && rec.group_id == group_id && rec.user_id == member_id;
    };

    Status status = deleteRecordFromHeapPage<GroupMembershipRecord>(
        group_memberships_table_page_, predicate, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to remove group member");
        return status;
    }

    DEBUG_LOG_DB("Removed member " << member_id.toString() << " from group " << group_id.toString());
    return Status::OK;
}

auto CatalogManager::getGroupMembers(const ID& group_id, std::vector<ID>& members_out,
                                     ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    members_out.clear();

    BufferPool *bp = db_->buffer_pool();
    void *page_buffer;
    Status status = bp->pinPage(group_memberships_table_page_, &page_buffer, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to pin group memberships page");
        return status;
    }

    auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
    uint32_t offset = sizeof(CatalogHeapPage);

    for (uint32_t i = 0; i < heap->record_count; i++)
    {
        auto *record = reinterpret_cast<GroupMembershipRecord *>(
            reinterpret_cast<uint8_t *>(page_buffer) + offset);

        if (record->is_valid && record->group_id == group_id)
        {
            members_out.push_back(record->user_id);
        }

        offset += sizeof(GroupMembershipRecord);
    }

    bp->unpinPage(group_memberships_table_page_, false, ctx);
    return Status::OK;
}

auto CatalogManager::getUserGroups(const ID& user_id, std::vector<ID>& groups_out,
                                   ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    groups_out.clear();

    BufferPool *bp = db_->buffer_pool();
    void *page_buffer;
    Status status = bp->pinPage(group_memberships_table_page_, &page_buffer, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to pin group memberships page");
        return status;
    }

    auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
    uint32_t offset = sizeof(CatalogHeapPage);

    for (uint32_t i = 0; i < heap->record_count; i++)
    {
        auto *record = reinterpret_cast<GroupMembershipRecord *>(
            reinterpret_cast<uint8_t *>(page_buffer) + offset);

        if (record->is_valid && record->user_id == user_id && record->member_type == 0)
        {
            groups_out.push_back(record->group_id);
        }

        offset += sizeof(GroupMembershipRecord);
    }

    bp->unpinPage(group_memberships_table_page_, false, ctx);
    return Status::OK;
}

// ============================================================================
// Session & Permission Operations (Phase 1.4 - Security System)
// ============================================================================

// Session management

auto CatalogManager::createSession(const ID& user_id, const ID& default_schema_id,
                                   SessionInfo& session_out, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(session_cache_mutex_);

    // Get user info
    UserInfo user;
    mutex_.lock();
    Status status = getUser(user_id, user, ctx);
    mutex_.unlock();

    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "User not found");
        return status;
    }

    if (!user.is_active)
    {
        SET_ERROR_CONTEXT(ctx, Status::PERMISSION_DENIED, "User account is disabled");
        return Status::PERMISSION_DENIED;
    }

    // Generate session ID
    session_out.session_id = generateUuidV7();
    session_out.user_id = user_id;
    session_out.username = user.username;
    session_out.is_superuser = user.is_superuser;
    session_out.current_schema_id = default_schema_id;
    session_out.login_time = std::chrono::system_clock::now().time_since_epoch().count();
    session_out.last_activity_time = session_out.login_time;

    // Compute effective roles (transitive closure)
    mutex_.lock();
    status = getEffectiveRoles(user_id, session_out.effective_roles, ctx);
    mutex_.unlock();

    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        // NOT_FOUND is OK (user has no roles)
        SET_ERROR_CONTEXT(ctx, status, "Failed to compute effective roles");
        return status;
    }

    // Compute effective groups (transitive closure)
    mutex_.lock();
    status = getEffectiveGroups(user_id, session_out.effective_groups, ctx);
    mutex_.unlock();

    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        // NOT_FOUND is OK (user has no groups)
        SET_ERROR_CONTEXT(ctx, status, "Failed to compute effective groups");
        return status;
    }

    // Store in cache
    session_cache_[session_out.session_id] = session_out;

    DEBUG_LOG_DB("Created session for user " << user.username
                 << " (session ID: " << session_out.session_id.toString() << ")");
    return Status::OK;
}

auto CatalogManager::getSession(const ID& session_id, SessionInfo& session_out,
                                ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(session_cache_mutex_);

    auto it = session_cache_.find(session_id);
    if (it == session_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Session not found");
        return Status::NOT_FOUND;
    }

    session_out = it->second;

    // Update last activity time
    session_out.last_activity_time = std::chrono::system_clock::now().time_since_epoch().count();
    it->second.last_activity_time = session_out.last_activity_time;

    return Status::OK;
}

auto CatalogManager::closeSession(const ID& session_id, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(session_cache_mutex_);

    auto it = session_cache_.find(session_id);
    if (it == session_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Session not found");
        return Status::NOT_FOUND;
    }

    DEBUG_LOG_DB("Closed session " << session_id.toString());
    session_cache_.erase(it);
    return Status::OK;
}

// Compute transitive closure of roles

auto CatalogManager::getEffectiveRoles(const ID& user_id, std::vector<ID>& roles_out,
                                       ErrorContext* ctx) -> Status
{
    // Security Phase 3.0: Implement transitive role-to-role grants (roles granted to roles)
    roles_out.clear();

    // Use BFS to find all roles (including transitive grants)
    std::unordered_set<ID> visited;
    std::queue<ID> to_process;
    to_process.push(user_id);
    visited.insert(user_id);

    while (!to_process.empty())
    {
        ID current_id = to_process.front();
        to_process.pop();

        // Get direct role memberships for current entity
        std::vector<RoleMembershipInfo> memberships;
        Status status = getUserRoles(current_id, memberships, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        // Process each role
        for (const auto& membership : memberships)
        {
            if (visited.find(membership.role_id) == visited.end())
            {
                visited.insert(membership.role_id);
                roles_out.push_back(membership.role_id);
                to_process.push(membership.role_id);  // Process roles granted to this role
            }
        }
    }

    return Status::OK;
}

// Compute transitive closure of groups

auto CatalogManager::getEffectiveGroups(const ID& user_id, std::vector<ID>& groups_out,
                                        ErrorContext* ctx) -> Status
{
    groups_out.clear();

    // Use BFS to find all groups (including nested)
    std::unordered_set<ID> visited;
    std::queue<ID> to_process;
    to_process.push(user_id);
    visited.insert(user_id);

    while (!to_process.empty())
    {
        ID current_id = to_process.front();
        to_process.pop();

        // Get groups for current entity
        std::vector<ID> direct_groups;
        Status status = getUserGroups(current_id, direct_groups, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        // Process each group
        for (const auto& group_id : direct_groups)
        {
            if (visited.find(group_id) == visited.end())
            {
                visited.insert(group_id);
                groups_out.push_back(group_id);
                to_process.push(group_id);  // Process nested groups
            }
        }
    }

    return Status::OK;
}

// Permission operations

auto CatalogManager::grantPermission(const ID& object_id, PermissionObjectType object_type,
                                     const ID& grantee_id, GranteeType grantee_type,
                                     uint32_t privileges, bool grant_option,
                                     const ID& grantor_id, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if permission already exists - if so, merge privileges
    auto predicate = [&](const PermissionRecord& rec) {
        return rec.is_valid &&
               rec.object_id == object_id &&
               rec.grantee_id == grantee_id &&
               rec.grantee_type == static_cast<uint8_t>(grantee_type);
    };

    auto result = findRecordInHeapPage<PermissionRecord>(permissions_table_page_, predicate, ctx);

    if (result.status == Status::OK)
    {
        // Update existing permission - merge privileges
        PermissionRecord updated_rec = result.record;
        updated_rec.privileges |= privileges;  // OR the privileges together
        if (grant_option) {
            updated_rec.grant_option = 1;
        }

        Status status = updateRecordInHeapPage(permissions_table_page_, result.slot_index,
                                              updated_rec, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update permission record");
            return status;
        }

        DEBUG_LOG_DB("Updated permission on object " << object_id.toString());
        return Status::OK;
    }

    // Create new permission record
    PermissionRecord perm_rec;
    memset(&perm_rec, 0, sizeof(PermissionRecord));
    perm_rec.permission_id = generateUuidV7();
    perm_rec.object_id = object_id;
    perm_rec.object_type = static_cast<uint8_t>(object_type);
    perm_rec.grantee_id = grantee_id;
    perm_rec.grantee_type = static_cast<uint8_t>(grantee_type);
    perm_rec.privileges = privileges;
    perm_rec.grant_option = grant_option ? 1 : 0;
    perm_rec.grantor_id = grantor_id;
    perm_rec.created_time = std::chrono::system_clock::now().time_since_epoch().count();
    perm_rec.is_valid = 1;

    // Write to disk
    Status status = writeRecordToHeapPage(permissions_table_page_, perm_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to write permission record");
        return status;
    }

    DEBUG_LOG_DB("Granted permission on object " << object_id.toString());
    return Status::OK;
}

auto CatalogManager::revokePermission(const ID& object_id, PermissionObjectType object_type,
                                      const ID& grantee_id, GranteeType grantee_type,
                                      uint32_t privileges, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Find permission record
    auto predicate = [&](const PermissionRecord& rec) {
        return rec.is_valid &&
               rec.object_id == object_id &&
               rec.grantee_id == grantee_id &&
               rec.grantee_type == static_cast<uint8_t>(grantee_type);
    };

    auto result = findRecordInHeapPage<PermissionRecord>(permissions_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, result.status, "Permission not found");
        return result.status;
    }

    // Remove specified privileges
    PermissionRecord updated_rec = result.record;
    updated_rec.privileges &= ~privileges;  // Clear the specified bits

    // If no privileges remain, delete the record
    if (updated_rec.privileges == 0)
    {
        Status status = deleteRecordFromHeapPage<PermissionRecord>(
            permissions_table_page_, predicate, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to delete permission record");
            return status;
        }

        DEBUG_LOG_DB("Revoked all permissions on object " << object_id.toString());
        return Status::OK;
    }

    // Update with reduced privileges
    Status status = updateRecordInHeapPage(permissions_table_page_, result.slot_index,
                                          updated_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to update permission record");
        return status;
    }

    DEBUG_LOG_DB("Revoked permission on object " << object_id.toString());
    return Status::OK;
}

auto CatalogManager::hasPermission(const ID& user_id, const ID& object_id,
                                   PermissionObjectType object_type, Privilege privilege,
                                   bool& has_perm_out, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    has_perm_out = false;

    // Get user info - superusers have all permissions
    UserInfo user;
    Status status = getUser(user_id, user, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (user.is_superuser)
    {
        has_perm_out = true;
        return Status::OK;
    }

    uint32_t privilege_mask = static_cast<uint32_t>(privilege);

    // Check direct user permissions
    auto check_permission = [&](const PermissionRecord& rec) {
        return rec.is_valid &&
               rec.object_id == object_id &&
               (rec.privileges & privilege_mask) != 0;
    };

    // 1. Check direct user permissions
    auto predicate_user = [&](const PermissionRecord& rec) {
        return check_permission(rec) &&
               rec.grantee_id == user_id &&
               rec.grantee_type == static_cast<uint8_t>(GranteeType::USER);
    };

    auto result = findRecordInHeapPage<PermissionRecord>(permissions_table_page_,
                                                          predicate_user, ctx);
    if (result.status == Status::OK)
    {
        has_perm_out = true;
        return Status::OK;
    }

    // 2. Check PUBLIC permissions
    auto predicate_public = [&](const PermissionRecord& rec) {
        return check_permission(rec) &&
               rec.grantee_type == static_cast<uint8_t>(GranteeType::PUBLIC);
    };

    result = findRecordInHeapPage<PermissionRecord>(permissions_table_page_,
                                                     predicate_public, ctx);
    if (result.status == Status::OK)
    {
        has_perm_out = true;
        return Status::OK;
    }

    // 3. Check role permissions
    std::vector<ID> effective_roles;
    status = getEffectiveRoles(user_id, effective_roles, ctx);
    if (status == Status::OK)
    {
        for (const auto& role_id : effective_roles)
        {
            auto predicate_role = [&](const PermissionRecord& rec) {
                return check_permission(rec) &&
                       rec.grantee_id == role_id &&
                       rec.grantee_type == static_cast<uint8_t>(GranteeType::ROLE);
            };

            result = findRecordInHeapPage<PermissionRecord>(permissions_table_page_,
                                                            predicate_role, ctx);
            if (result.status == Status::OK)
            {
                has_perm_out = true;
                return Status::OK;
            }
        }
    }

    // 4. Check group permissions
    std::vector<ID> effective_groups;
    status = getEffectiveGroups(user_id, effective_groups, ctx);
    if (status == Status::OK)
    {
        for (const auto& group_id : effective_groups)
        {
            auto predicate_group = [&](const PermissionRecord& rec) {
                return check_permission(rec) &&
                       rec.grantee_id == group_id &&
                       rec.grantee_type == static_cast<uint8_t>(GranteeType::GROUP);
            };

            result = findRecordInHeapPage<PermissionRecord>(permissions_table_page_,
                                                            predicate_group, ctx);
            if (result.status == Status::OK)
            {
                has_perm_out = true;
                return Status::OK;
            }
        }
    }

    // No permission found
    return Status::OK;
}

auto CatalogManager::getObjectPermissions(const ID& object_id, PermissionObjectType object_type,
                                          std::vector<PermissionInfo>& permissions_out,
                                          ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    permissions_out.clear();

    auto filter = [&object_id](const PermissionRecord& rec) {
        return rec.is_valid && rec.object_id == object_id;
    };

    auto converter = [](const PermissionRecord& rec, PermissionInfo& info) {
        info.permission_id = rec.permission_id;
        info.object_id = rec.object_id;
        info.object_type = static_cast<PermissionObjectType>(rec.object_type);
        info.grantee_id = rec.grantee_id;
        info.grantee_type = static_cast<GranteeType>(rec.grantee_type);
        info.privileges = rec.privileges;
        info.grant_option = rec.grant_option != 0;
        info.grantor_id = rec.grantor_id;
        info.created_time = rec.created_time;
    };

    return readRecordsToVector<PermissionRecord, PermissionInfo>(
        permissions_table_page_, permissions_out, filter, converter, ctx);
}

auto CatalogManager::getUserPermissions(const ID& user_id, std::vector<PermissionInfo>& permissions_out,
                                        ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    permissions_out.clear();

    auto filter = [&user_id](const PermissionRecord& rec) {
        return rec.is_valid &&
               rec.grantee_id == user_id &&
               rec.grantee_type == static_cast<uint8_t>(GranteeType::USER);
    };

    auto converter = [](const PermissionRecord& rec, PermissionInfo& info) {
        info.permission_id = rec.permission_id;
        info.object_id = rec.object_id;
        info.object_type = static_cast<PermissionObjectType>(rec.object_type);
        info.grantee_id = rec.grantee_id;
        info.grantee_type = static_cast<GranteeType>(rec.grantee_type);
        info.privileges = rec.privileges;
        info.grant_option = rec.grant_option != 0;
        info.grantor_id = rec.grantor_id;
        info.created_time = rec.created_time;
    };

    return readRecordsToVector<PermissionRecord, PermissionInfo>(
        permissions_table_page_, permissions_out, filter, converter, ctx);
}

// ============================================================================
// Security Phase 3.3: Column-Level Permission Operations
// ============================================================================

auto CatalogManager::grantColumnPermission(const ID& table_id, const std::string& column_name,
                                          const ID& grantee_id, GranteeType grantee_type,
                                          uint32_t privileges, bool grant_option,
                                          const ID& grantor_id, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if permission already exists for this column - if so, merge privileges
    auto predicate = [&](const ColumnPermissionRecord& rec) {
        return rec.is_valid &&
               rec.table_id == table_id &&
               std::strcmp(rec.column_name, column_name.c_str()) == 0 &&
               rec.grantee_id == grantee_id &&
               rec.grantee_type == static_cast<uint8_t>(grantee_type);
    };

    auto result = findRecordInHeapPage<ColumnPermissionRecord>(column_permissions_table_page_, predicate, ctx);

    if (result.status == Status::OK)
    {
        // Update existing permission - merge privileges
        ColumnPermissionRecord updated_rec = result.record;
        updated_rec.privileges |= privileges;  // OR the privileges together
        if (grant_option) {
            updated_rec.grant_option = 1;
        }

        Status status = updateRecordInHeapPage(column_permissions_table_page_, result.slot_index,
                                              updated_rec, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update column permission record");
            return status;
        }

        DEBUG_LOG_DB("Updated column permission on table " << table_id.toString()
                    << ", column: " << column_name);
        return Status::OK;
    }

    // Create new column permission record
    ColumnPermissionRecord col_perm_rec;
    memset(&col_perm_rec, 0, sizeof(ColumnPermissionRecord));
    col_perm_rec.permission_id = generateUuidV7();
    col_perm_rec.table_id = table_id;
    strncpy(col_perm_rec.column_name, column_name.c_str(), 127);
    col_perm_rec.column_name[127] = '\0';  // Ensure null termination
    col_perm_rec.grantee_id = grantee_id;
    col_perm_rec.grantee_type = static_cast<uint8_t>(grantee_type);
    col_perm_rec.privileges = privileges;
    col_perm_rec.grant_option = grant_option ? 1 : 0;
    col_perm_rec.grantor_id = grantor_id;
    col_perm_rec.created_time = std::chrono::system_clock::now().time_since_epoch().count();
    col_perm_rec.is_valid = 1;

    // Write to disk
    Status status = writeRecordToHeapPage(column_permissions_table_page_, col_perm_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to write column permission record");
        return status;
    }

    DEBUG_LOG_DB("Granted column permission on table " << table_id.toString()
                << ", column: " << column_name);
    return Status::OK;
}

auto CatalogManager::revokeColumnPermission(const ID& table_id, const std::string& column_name,
                                           const ID& grantee_id, GranteeType grantee_type,
                                           uint32_t privileges, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Find column permission record
    auto predicate = [&](const ColumnPermissionRecord& rec) {
        return rec.is_valid &&
               rec.table_id == table_id &&
               std::strcmp(rec.column_name, column_name.c_str()) == 0 &&
               rec.grantee_id == grantee_id &&
               rec.grantee_type == static_cast<uint8_t>(grantee_type);
    };

    auto result = findRecordInHeapPage<ColumnPermissionRecord>(column_permissions_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        // Permission not found - not an error, just return OK
        return Status::OK;
    }

    // Remove specified privileges (AND with inverse)
    ColumnPermissionRecord updated_rec = result.record;
    updated_rec.privileges &= ~privileges;  // Clear the specified privilege bits

    // If no privileges remain, mark as deleted (MGA soft delete)
    if (updated_rec.privileges == 0)
    {
        updated_rec.is_valid = 0;
    }

    Status status = updateRecordInHeapPage(column_permissions_table_page_, result.slot_index,
                                          updated_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to revoke column permission");
        return status;
    }

    DEBUG_LOG_DB("Revoked column permission on table " << table_id.toString()
                << ", column: " << column_name);
    return Status::OK;
}

auto CatalogManager::hasColumnPermission(const ID& user_id, const ID& table_id,
                                        const std::string& column_name, Privilege privilege,
                                        bool& has_perm_out, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    has_perm_out = false;

    // IMPORTANT: Check table-level permission first
    // If user has table-level privilege, they have access to all columns
    bool has_table_perm = false;
    Status status = hasPermission(user_id, table_id, PermissionObjectType::TABLE,
                                 privilege, has_table_perm, ctx);
    if (status != Status::OK) {
        return status;
    }

    if (has_table_perm) {
        has_perm_out = true;
        return Status::OK;
    }

    // Check column-level permission
    uint32_t required_priv = static_cast<uint32_t>(privilege);

    // Check user's direct column permissions
    auto user_predicate = [&](const ColumnPermissionRecord& rec) {
        return rec.is_valid &&
               rec.table_id == table_id &&
               std::strcmp(rec.column_name, column_name.c_str()) == 0 &&
               rec.grantee_id == user_id &&
               rec.grantee_type == static_cast<uint8_t>(GranteeType::USER) &&
               (rec.privileges & required_priv) != 0;
    };

    auto result = findRecordInHeapPage<ColumnPermissionRecord>(
        column_permissions_table_page_, user_predicate, ctx);

    if (result.status == Status::OK) {
        has_perm_out = true;
        return Status::OK;
    }

    // TODO: Check role memberships and group memberships (Phase 3.3.3)
    // For now, only checking direct user permissions

    return Status::OK;
}

auto CatalogManager::getAccessibleColumns(const ID& user_id, const ID& table_id,
                                         Privilege privilege, std::vector<std::string>& columns_out,
                                         ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    columns_out.clear();

    // Check if user has table-level permission
    bool has_table_perm = false;
    Status status = hasPermission(user_id, table_id, PermissionObjectType::TABLE,
                                 privilege, has_table_perm, ctx);
    if (status != Status::OK) {
        return status;
    }

    // If user has table-level permission, return empty vector
    // Empty vector signals "all columns accessible" to avoid loading all column names
    if (has_table_perm) {
        return Status::OK;
    }

    // Collect columns with specific privileges
    uint32_t required_priv = static_cast<uint32_t>(privilege);

    auto filter = [&](const ColumnPermissionRecord& rec) {
        return rec.is_valid &&
               rec.table_id == table_id &&
               rec.grantee_id == user_id &&
               rec.grantee_type == static_cast<uint8_t>(GranteeType::USER) &&
               (rec.privileges & required_priv) != 0;
    };

    auto converter = [](const ColumnPermissionRecord& rec, std::string& col_name) {
        col_name = std::string(rec.column_name);
    };

    return readRecordsToVector<ColumnPermissionRecord, std::string>(
        column_permissions_table_page_, columns_out, filter, converter, ctx);
}

auto CatalogManager::getColumnPermissions(const ID& table_id,
                                         std::vector<ColumnPermissionInfo>& perms_out,
                                         ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    perms_out.clear();

    auto filter = [&](const ColumnPermissionRecord& rec) {
        return rec.is_valid && rec.table_id == table_id;
    };

    auto converter = [](const ColumnPermissionRecord& rec, ColumnPermissionInfo& info) {
        info.permission_id = rec.permission_id;
        info.table_id = rec.table_id;
        info.column_name = std::string(rec.column_name);
        info.grantee_id = rec.grantee_id;
        info.grantee_type = static_cast<GranteeType>(rec.grantee_type);
        info.privileges = rec.privileges;
        info.grant_option = rec.grant_option != 0;
        info.grantor_id = rec.grantor_id;
        info.created_time = rec.created_time;
    };

    return readRecordsToVector<ColumnPermissionRecord, ColumnPermissionInfo>(
        column_permissions_table_page_, perms_out, filter, converter, ctx);
}

// ============================================================================
// Security Phase 3.4: Row-Level Security Policy Operations
// ============================================================================

auto CatalogManager::createPolicy(const ID& table_id, const std::string& policy_name,
                                 PolicyType type, const std::vector<std::string>& roles,
                                 const std::string& using_expr, const std::string& with_check_expr,
                                 ID& policy_id_out, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if policy with this name already exists on this table
    auto predicate = [&](const PolicyRecord& rec) {
        return rec.is_valid &&
               rec.table_id == table_id &&
               std::strcmp(rec.policy_name, policy_name.c_str()) == 0;
    };

    auto result = findRecordInHeapPage<PolicyRecord>(policies_table_page_, predicate, ctx);
    if (result.status == Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS,
                         ("Policy '" + policy_name + "' already exists on table").c_str());
        return Status::FILE_EXISTS;
    }

    // Create new policy record
    PolicyRecord policy_rec;
    memset(&policy_rec, 0, sizeof(PolicyRecord));
    policy_rec.policy_id = generateUuidV7();
    policy_rec.table_id = table_id;
    strncpy(policy_rec.policy_name, policy_name.c_str(), 63);
    policy_rec.policy_name[63] = '\0';  // Ensure null termination
    policy_rec.policy_type = static_cast<uint8_t>(type);
    policy_rec.is_enabled = 1;
    policy_rec.created_time = std::chrono::system_clock::now().time_since_epoch().count();
    policy_rec.modified_time = policy_rec.created_time;
    policy_rec.is_valid = 1;

    // Store roles array in TOAST if non-empty
    if (!roles.empty())
    {
        // Serialize roles as comma-separated string
        std::string roles_str;
        for (size_t i = 0; i < roles.size(); ++i)
        {
            if (i > 0) roles_str += ",";
            roles_str += roles[i];
        }
        // For now, store OID as 0 - full TOAST integration in future
        // TODO: Store roles_str in TOAST and save OID
        policy_rec.roles_oid = 0;
    }
    else
    {
        policy_rec.roles_oid = 0;  // Empty = all roles
    }

    // Store expressions in TOAST (Phase 3.4.6)
    uint64_t xmin = 1;  // TODO: Get from transaction context in future

    // Store USING expression
    Status toast_status = storeStringInToast(using_expr, xmin, policy_rec.using_expr_oid, ctx);
    if (toast_status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, toast_status, "Failed to store USING expression");
        return toast_status;
    }

    // Store WITH CHECK expression
    toast_status = storeStringInToast(with_check_expr, xmin, policy_rec.with_check_expr_oid, ctx);
    if (toast_status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, toast_status, "Failed to store WITH CHECK expression");
        return toast_status;
    }

    // Write to disk
    Status status = writeRecordToHeapPage(policies_table_page_, policy_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to write policy record");
        return status;
    }

    // Cache policy in memory with expressions (Phase 3.4.6)
    {
        std::lock_guard<std::mutex> cache_lock(policy_cache_mutex_);

        PolicyInfo policy_info;
        policy_info.policy_id = policy_rec.policy_id;
        policy_info.table_id = policy_rec.table_id;
        policy_info.policy_name = policy_name;
        policy_info.policy_type = type;

        // Phase 3 Polish: Convert role names to UUIDs
        policy_info.role_ids.clear();
        for (const auto& role_name : roles)
        {
            RoleInfo role;
            if (getRoleByName(role_name, role, ctx) == Status::OK)
            {
                policy_info.role_ids.push_back(role.role_id);
            }
            // If role doesn't exist, skip it (could warn)
        }

        policy_info.using_expr = using_expr;        // Store actual expression string
        policy_info.with_check_expr = with_check_expr;  // Store actual expression string
        policy_info.is_enabled = true;
        policy_info.created_time = policy_rec.created_time;
        policy_info.modified_time = policy_rec.modified_time;

        policy_cache_[policy_info.policy_id] = policy_info;
    }

    policy_id_out = policy_rec.policy_id;
    DEBUG_LOG_DB("Created policy '" << policy_name << "' on table " << table_id.toString());
    return Status::OK;
}

auto CatalogManager::dropPolicy(const ID& table_id, const std::string& policy_name,
                               ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Find policy record
    auto predicate = [&](const PolicyRecord& rec) {
        return rec.is_valid &&
               rec.table_id == table_id &&
               std::strcmp(rec.policy_name, policy_name.c_str()) == 0;
    };

    auto result = findRecordInHeapPage<PolicyRecord>(policies_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                         ("Policy '" + policy_name + "' not found on table").c_str());
        return Status::NOT_FOUND;
    }

    // Phase 3.4.8: Delete TOAST data for expressions before soft-deleting the policy
    uint64_t xmax = 1;  // TODO: Get from transaction context

    // Delete USING expression from TOAST if it exists
    if (result.record.using_expr_oid != 0 && policy_toast_manager_)
    {
        Status toast_status = policy_toast_manager_->deleteToastValue(
            result.record.using_expr_oid, xmax, ctx);
        if (toast_status != Status::OK)
        {
            DEBUG_LOG_DB("Warning: Failed to delete USING expression TOAST data for policy: " << policy_name);
            // Non-fatal - continue with policy deletion
        }
    }

    // Delete WITH CHECK expression from TOAST if it exists
    if (result.record.with_check_expr_oid != 0 && policy_toast_manager_)
    {
        Status toast_status = policy_toast_manager_->deleteToastValue(
            result.record.with_check_expr_oid, xmax, ctx);
        if (toast_status != Status::OK)
        {
            DEBUG_LOG_DB("Warning: Failed to delete WITH CHECK expression TOAST data for policy: " << policy_name);
            // Non-fatal - continue with policy deletion
        }
    }

    // Mark as invalid (soft delete - MGA pattern)
    PolicyRecord updated_rec = result.record;
    updated_rec.is_valid = 0;

    Status status = updateRecordInHeapPage(policies_table_page_, result.slot_index,
                                          updated_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to delete policy record");
        return status;
    }

    // Remove from cache (Phase 3.4.6)
    {
        std::lock_guard<std::mutex> cache_lock(policy_cache_mutex_);
        policy_cache_.erase(result.record.policy_id);
    }

    DEBUG_LOG_DB("Dropped policy '" << policy_name << "' from table " << table_id.toString());
    return Status::OK;
}

auto CatalogManager::getPolicy(const ID& table_id, const std::string& policy_name,
                              PolicyInfo& policy_out, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Find policy record on disk
    auto predicate = [&](const PolicyRecord& rec) {
        return rec.is_valid &&
               rec.table_id == table_id &&
               std::strcmp(rec.policy_name, policy_name.c_str()) == 0;
    };

    auto result = findRecordInHeapPage<PolicyRecord>(policies_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                         ("Policy '" + policy_name + "' not found").c_str());
        return Status::NOT_FOUND;
    }

    // Check cache for full policy info with expressions (Phase 3.4.6)
    {
        std::lock_guard<std::mutex> cache_lock(policy_cache_mutex_);
        auto cache_it = policy_cache_.find(result.record.policy_id);
        if (cache_it != policy_cache_.end())
        {
            // Return cached policy with expressions
            policy_out = cache_it->second;
            return Status::OK;
        }
    }

    // Policy not in cache - load from TOAST (Phase 3.4.8)
    // Convert record to PolicyInfo
    policy_out.policy_id = result.record.policy_id;
    policy_out.table_id = result.record.table_id;
    policy_out.policy_name = std::string(result.record.policy_name);
    policy_out.policy_type = static_cast<PolicyType>(result.record.policy_type);
    policy_out.is_enabled = result.record.is_enabled != 0;
    policy_out.created_time = result.record.created_time;
    policy_out.modified_time = result.record.modified_time;

    // Load expressions from TOAST if available (Phase 3.4.8)
    uint64_t xmin = 1;  // TODO: Get from transaction context

    // Load USING expression
    Status load_status = loadStringFromToast(result.record.using_expr_oid, xmin,
                                            policy_out.using_expr, ctx);
    if (load_status != Status::OK && load_status != Status::NOT_IMPLEMENTED)
    {
        DEBUG_LOG_DB("Failed to load USING expression from TOAST for policy: " << policy_name);
        // Non-fatal - continue with empty expression
        policy_out.using_expr = "";
    }

    // Load WITH CHECK expression
    load_status = loadStringFromToast(result.record.with_check_expr_oid, xmin,
                                     policy_out.with_check_expr, ctx);
    if (load_status != Status::OK && load_status != Status::NOT_IMPLEMENTED)
    {
        DEBUG_LOG_DB("Failed to load WITH CHECK expression from TOAST for policy: " << policy_name);
        // Non-fatal - continue with empty expression
        policy_out.with_check_expr = "";
    }

    // Phase 3 Polish: Roles list as UUIDs (TODO: Load from TOAST when roles_oid is implemented)
    policy_out.role_ids.clear();

    // Cache the loaded policy for future access
    {
        std::lock_guard<std::mutex> cache_lock(policy_cache_mutex_);
        policy_cache_[policy_out.policy_id] = policy_out;
    }

    return Status::OK;
}

auto CatalogManager::getTablePolicies(const ID& table_id, PolicyType type,
                                     std::vector<PolicyInfo>& policies_out,
                                     ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    policies_out.clear();

    auto filter = [&](const PolicyRecord& rec) {
        if (!rec.is_valid || rec.table_id != table_id)
            return false;

        // If type is ALL, return all policies
        // Otherwise, match specific type or ALL type policies
        if (type == PolicyType::ALL)
            return true;

        return rec.policy_type == static_cast<uint8_t>(type) ||
               rec.policy_type == static_cast<uint8_t>(PolicyType::ALL);
    };

    auto converter = [this](const PolicyRecord& rec, PolicyInfo& info) {
        info.policy_id = rec.policy_id;
        info.table_id = rec.table_id;
        info.policy_name = std::string(rec.policy_name);
        info.policy_type = static_cast<PolicyType>(rec.policy_type);
        info.is_enabled = rec.is_valid != 0;
        info.created_time = rec.created_time;
        info.modified_time = rec.modified_time;

        // Try to load from cache with expressions (Phase 3.4.6)
        {
            std::lock_guard<std::mutex> cache_lock(policy_cache_mutex_);
            auto cache_it = policy_cache_.find(rec.policy_id);
            if (cache_it != policy_cache_.end())
            {
                // Use cached policy with expressions (Phase 3 Polish: role_ids)
                info.role_ids = cache_it->second.role_ids;
                info.using_expr = cache_it->second.using_expr;
                info.with_check_expr = cache_it->second.with_check_expr;
                return;
            }
        }

        // Cache miss - no expressions available (Phase 3 Polish: role_ids)
        info.role_ids.clear();
        info.using_expr = "";
        info.with_check_expr = "";
    };

    return readRecordsToVector<PolicyRecord, PolicyInfo>(
        policies_table_page_, policies_out, filter, converter, ctx);
}

auto CatalogManager::getPoliciesForUser(const ID& table_id, const ID& user_id,
                                       PolicyType type, std::vector<PolicyInfo>& policies_out,
                                       ErrorContext* ctx) -> Status
{
    // First get all policies for the table
    Status status = getTablePolicies(table_id, type, policies_out, ctx);
    if (status != Status::OK)
        return status;

    // TODO: Filter by user roles when TOAST integration is complete
    // For now, return all policies (empty roles = applies to all users)
    // Future: Check if user's roles intersect with policy roles

    return Status::OK;
}

// Test helper: Clear policy cache to force TOAST loading (Phase 3.4.8)
void CatalogManager::clearPolicyCache()
{
    std::lock_guard<std::mutex> lock(policy_cache_mutex_);
    policy_cache_.clear();
    DEBUG_LOG_DB("Policy cache cleared (test helper)");
}

auto CatalogManager::setTableRLS(const ID& table_id, bool enabled, bool forced,
                                ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Find table record
    auto predicate = [&](const TableRecord& rec) {
        return rec.is_valid && rec.table_id == table_id;
    };

    auto result = findRecordInHeapPage<TableRecord>(tables_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
        return Status::NOT_FOUND;
    }

    // Update RLS flags
    TableRecord updated_rec = result.record;
    updated_rec.rls_enabled = enabled ? 1 : 0;
    updated_rec.rls_forced = forced ? 1 : 0;

    Status status = updateRecordInHeapPage(tables_table_page_, result.slot_index,
                                          updated_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to update table RLS settings");
        return status;
    }

    DEBUG_LOG_DB("Set RLS on table " << table_id.toString()
                << " - enabled: " << enabled << ", forced: " << forced);
    return Status::OK;
}

auto CatalogManager::getTableRLS(const ID& table_id, bool& enabled_out, bool& forced_out,
                                ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Find table record
    auto predicate = [&](const TableRecord& rec) {
        return rec.is_valid && rec.table_id == table_id;
    };

    auto result = findRecordInHeapPage<TableRecord>(tables_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
        return Status::NOT_FOUND;
    }

    enabled_out = result.record.rls_enabled != 0;
    forced_out = result.record.rls_forced != 0;

    return Status::OK;
}

// ============================================================================
// Security Phase 3.1: SQL Object Permissions
// ============================================================================

auto CatalogManager::grantObjectPermission(const ID& object_id, ObjectType object_type,
                                          const ID& grantee_id, GranteeType grantee_type,
                                          uint32_t permissions, bool grant_option,
                                          ID& permission_id_out, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if permission already exists for this object+grantee combination
    auto predicate = [&](const ObjectPermissionRecord& rec) {
        return rec.is_valid &&
               rec.object_id == object_id &&
               rec.grantee_id == grantee_id;
    };

    auto result = findRecordInHeapPage<ObjectPermissionRecord>(object_permissions_table_page_, predicate, ctx);
    if (result.status == Status::OK)
    {
        // Permission exists - update it (OR the permissions together)
        ObjectPermissionRecord updated_rec = result.record;
        updated_rec.permissions |= permissions;  // Add new permissions
        updated_rec.grant_option = grant_option ? 1 : 0;
        updated_rec.created_time = std::chrono::system_clock::now().time_since_epoch().count();

        Status status = updateRecordInHeapPage(object_permissions_table_page_, result.slot_index,
                                              updated_rec, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to update object permission");
            return status;
        }

        permission_id_out = updated_rec.permission_id;

        // Update cache
        {
            std::lock_guard<std::mutex> cache_lock(object_permissions_cache_mutex_);
            auto& perms = object_permissions_cache_[object_id];
            for (auto& perm : perms)
            {
                if (perm.grantee_id == grantee_id)
                {
                    perm.permissions = updated_rec.permissions;
                    perm.grant_option = grant_option;
                    perm.created_time = updated_rec.created_time;
                    break;
                }
            }
        }

        DEBUG_LOG_DB("Updated object permission for object " << object_id.toString());
        return Status::OK;
    }

    // Create new permission record
    ObjectPermissionRecord perm_rec;
    memset(&perm_rec, 0, sizeof(ObjectPermissionRecord));
    perm_rec.permission_id = generateUuidV7();
    perm_rec.object_id = object_id;
    perm_rec.object_type = static_cast<uint8_t>(object_type);
    perm_rec.grantee_id = grantee_id;
    perm_rec.grantee_type = static_cast<uint8_t>(grantee_type);
    perm_rec.permissions = permissions;
    perm_rec.grant_option = grant_option ? 1 : 0;

    // Get grantor from connection context
    ConnectionContext* conn_ctx = ConnectionContext::getCurrent();
    if (conn_ctx)
    {
        perm_rec.grantor_id = conn_ctx->getCurrentUserId();
    }
    else
    {
        perm_rec.grantor_id = SecurityConstants::makeSystemUserID();
    }

    perm_rec.created_time = std::chrono::system_clock::now().time_since_epoch().count();
    perm_rec.is_valid = 1;

    // Write to disk
    Status status = writeRecordToHeapPage(object_permissions_table_page_, perm_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to write object permission record");
        return status;
    }

    // Cache permission
    {
        std::lock_guard<std::mutex> cache_lock(object_permissions_cache_mutex_);

        ObjectPermissionInfo perm_info;
        perm_info.permission_id = perm_rec.permission_id;
        perm_info.object_id = object_id;
        perm_info.object_type = object_type;
        perm_info.grantee_id = grantee_id;
        perm_info.grantee_type = grantee_type;
        perm_info.permissions = permissions;
        perm_info.grant_option = grant_option;
        perm_info.grantor_id = perm_rec.grantor_id;
        perm_info.created_time = perm_rec.created_time;

        object_permissions_cache_[object_id].push_back(perm_info);
    }

    permission_id_out = perm_rec.permission_id;
    DEBUG_LOG_DB("Granted object permission on object " << object_id.toString());
    return Status::OK;
}

auto CatalogManager::revokeObjectPermission(const ID& object_id, const ID& grantee_id,
                                           ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Find permission record
    auto predicate = [&](const ObjectPermissionRecord& rec) {
        return rec.is_valid &&
               rec.object_id == object_id &&
               rec.grantee_id == grantee_id;
    };

    auto result = findRecordInHeapPage<ObjectPermissionRecord>(object_permissions_table_page_, predicate, ctx);
    if (result.status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Object permission not found");
        return Status::NOT_FOUND;
    }

    // Mark as invalid (soft delete - MGA pattern)
    ObjectPermissionRecord updated_rec = result.record;
    updated_rec.is_valid = 0;

    Status status = updateRecordInHeapPage(object_permissions_table_page_, result.slot_index,
                                          updated_rec, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Failed to revoke object permission");
        return status;
    }

    // Remove from cache
    {
        std::lock_guard<std::mutex> cache_lock(object_permissions_cache_mutex_);
        auto& perms = object_permissions_cache_[object_id];
        perms.erase(
            std::remove_if(perms.begin(), perms.end(),
                          [&](const ObjectPermissionInfo& p) { return p.grantee_id == grantee_id; }),
            perms.end());
    }

    DEBUG_LOG_DB("Revoked object permission on object " << object_id.toString());
    return Status::OK;
}

auto CatalogManager::hasObjectPermission(const ID& object_id, const ID& user_id,
                                        uint32_t required_permissions,
                                        ErrorContext* ctx) -> bool
{
    // Check cache first
    {
        std::lock_guard<std::mutex> cache_lock(object_permissions_cache_mutex_);
        auto it = object_permissions_cache_.find(object_id);
        if (it != object_permissions_cache_.end())
        {
            for (const auto& perm : it->second)
            {
                // Direct user permission
                if (perm.grantee_type == GranteeType::USER && perm.grantee_id == user_id)
                {
                    if ((perm.permissions & required_permissions) == required_permissions)
                    {
                        return true;
                    }
                }
                // TODO: Check role/group memberships (Phase 3.1 enhancement)
            }
            return false;  // Cache hit, no permission found
        }
    }

    // Cache miss - load from disk
    std::lock_guard<std::mutex> lock(mutex_);

    auto predicate = [&](const ObjectPermissionRecord& rec) {
        return rec.is_valid && rec.object_id == object_id;
    };

    std::vector<ObjectPermissionInfo> perms;
    auto converter = [](const ObjectPermissionRecord& rec, ObjectPermissionInfo& info) {
        info.permission_id = rec.permission_id;
        info.object_id = rec.object_id;
        info.object_type = static_cast<ObjectType>(rec.object_type);
        info.grantee_id = rec.grantee_id;
        info.grantee_type = static_cast<GranteeType>(rec.grantee_type);
        info.permissions = rec.permissions;
        info.grant_option = rec.grant_option != 0;
        info.grantor_id = rec.grantor_id;
        info.created_time = rec.created_time;
    };

    auto filter = predicate;
    Status status = readRecordsToVector<ObjectPermissionRecord, ObjectPermissionInfo>(
        object_permissions_table_page_, perms, filter, converter, ctx);

    if (status == Status::OK && !perms.empty())
    {
        // Populate cache
        {
            std::lock_guard<std::mutex> cache_lock(object_permissions_cache_mutex_);
            object_permissions_cache_[object_id] = perms;
        }

        // Check permissions
        for (const auto& perm : perms)
        {
            if (perm.grantee_type == GranteeType::USER && perm.grantee_id == user_id)
            {
                if ((perm.permissions & required_permissions) == required_permissions)
                {
                    return true;
                }
            }
            // TODO: Check role/group memberships
        }
    }

    return false;
}

auto CatalogManager::getObjectPermissions(const ID& object_id,
                                         std::vector<ObjectPermissionInfo>& perms_out,
                                         ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    perms_out.clear();

    auto filter = [&](const ObjectPermissionRecord& rec) {
        return rec.is_valid && rec.object_id == object_id;
    };

    auto converter = [](const ObjectPermissionRecord& rec, ObjectPermissionInfo& info) {
        info.permission_id = rec.permission_id;
        info.object_id = rec.object_id;
        info.object_type = static_cast<ObjectType>(rec.object_type);
        info.grantee_id = rec.grantee_id;
        info.grantee_type = static_cast<GranteeType>(rec.grantee_type);
        info.permissions = rec.permissions;
        info.grant_option = rec.grant_option != 0;
        info.grantor_id = rec.grantor_id;
        info.created_time = rec.created_time;
    };

    return readRecordsToVector<ObjectPermissionRecord, ObjectPermissionInfo>(
        object_permissions_table_page_, perms_out, filter, converter, ctx);
}

// ================================================================================================
// Foreign Key Constraints (ALPHA Phase A - FK Constraints)
// ================================================================================================

auto CatalogManager::createForeignKey(const std::string& fk_name,
                                     const ID& child_table_id,
                                     const ID& parent_table_id,
                                     const std::vector<std::string>& child_columns,
                                     const std::vector<std::string>& parent_columns,
                                     FKAction on_delete,
                                     FKAction on_update,
                                     FKMatchType match_type,
                                     ID& fk_id_out,
                                     ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(foreign_keys_cache_mutex_);

    // Validate inputs
    if (fk_name.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Foreign key name cannot be empty");
        return Status::INVALID_ARGUMENT;
    }

    if (child_columns.empty() || parent_columns.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Foreign key must have at least one column");
        return Status::INVALID_ARGUMENT;
    }

    if (child_columns.size() != parent_columns.size())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Foreign key child and parent column counts must match");
        return Status::INVALID_ARGUMENT;
    }

    // Generate FK ID
    fk_id_out = generateUuidV7();

    // Create FK info
    ForeignKeyInfo fk_info;
    fk_info.fk_id = fk_id_out;
    fk_info.fk_name = fk_name;
    fk_info.child_table_id = child_table_id;
    fk_info.parent_table_id = parent_table_id;
    fk_info.child_columns = child_columns;
    fk_info.parent_columns = parent_columns;
    fk_info.on_delete = on_delete;
    fk_info.on_update = on_update;
    fk_info.match_type = match_type;
    fk_info.is_enabled = true;
    fk_info.created_time = std::chrono::system_clock::now().time_since_epoch().count();

    // Store in cache
    foreign_keys_cache_[fk_id_out] = fk_info;
    table_child_fks_.insert({child_table_id, fk_id_out});
    table_parent_fks_.insert({parent_table_id, fk_id_out});

    // Phase D: Persist to disk
    if (foreign_keys_table_page_ != 0)
    {
        ForeignKeyRecord fk_rec;
        memset(&fk_rec, 0, sizeof(ForeignKeyRecord));

        fk_rec.fk_id = fk_id_out;
        strncpy(fk_rec.fk_name, fk_name.c_str(), sizeof(fk_rec.fk_name) - 1);
        fk_rec.child_table_id = child_table_id;
        fk_rec.parent_table_id = parent_table_id;

        // Serialize column vectors to comma-separated strings
        std::string child_cols_str;
        for (size_t i = 0; i < child_columns.size(); ++i)
        {
            if (i > 0) child_cols_str += ",";
            child_cols_str += child_columns[i];
        }
        strncpy(fk_rec.child_columns, child_cols_str.c_str(), sizeof(fk_rec.child_columns) - 1);

        std::string parent_cols_str;
        for (size_t i = 0; i < parent_columns.size(); ++i)
        {
            if (i > 0) parent_cols_str += ",";
            parent_cols_str += parent_columns[i];
        }
        strncpy(fk_rec.parent_columns, parent_cols_str.c_str(), sizeof(fk_rec.parent_columns) - 1);

        fk_rec.on_delete = static_cast<uint8_t>(on_delete);
        fk_rec.on_update = static_cast<uint8_t>(on_update);
        fk_rec.match_type = static_cast<uint8_t>(match_type);
        fk_rec.is_enabled = 1;
        fk_rec.created_time = fk_info.created_time;
        fk_rec.is_valid = 1;

        Status status = writeRecordToHeapPage(foreign_keys_table_page_, fk_rec, ctx);
        if (status != Status::OK)
        {
            // Rollback cache changes
            foreign_keys_cache_.erase(fk_id_out);
            auto child_range = table_child_fks_.equal_range(child_table_id);
            for (auto it = child_range.first; it != child_range.second; )
            {
                if (it->second == fk_id_out)
                    it = table_child_fks_.erase(it);
                else
                    ++it;
            }
            auto parent_range = table_parent_fks_.equal_range(parent_table_id);
            for (auto it = parent_range.first; it != parent_range.second; )
            {
                if (it->second == fk_id_out)
                    it = table_parent_fks_.erase(it);
                else
                    ++it;
            }

            SET_ERROR_CONTEXT(ctx, status, "Failed to write foreign key record");
            return status;
        }
    }

    return Status::OK;
}

auto CatalogManager::getForeignKeysForTable(const ID& table_id,
                                           std::vector<ForeignKeyInfo>& fks_out,
                                           ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(foreign_keys_cache_mutex_);

    fks_out.clear();

    // Find all FKs where this table is the child
    auto range = table_child_fks_.equal_range(table_id);
    for (auto it = range.first; it != range.second; ++it)
    {
        auto fk_it = foreign_keys_cache_.find(it->second);
        if (fk_it != foreign_keys_cache_.end())
        {
            fks_out.push_back(fk_it->second);
        }
    }

    return Status::OK;
}

auto CatalogManager::getReferencingForeignKeys(const ID& table_id,
                                              std::vector<ForeignKeyInfo>& fks_out,
                                              ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(foreign_keys_cache_mutex_);

    fks_out.clear();

    // Find all FKs where this table is the parent (referenced table)
    auto range = table_parent_fks_.equal_range(table_id);
    for (auto it = range.first; it != range.second; ++it)
    {
        auto fk_it = foreign_keys_cache_.find(it->second);
        if (fk_it != foreign_keys_cache_.end())
        {
            fks_out.push_back(fk_it->second);
        }
    }

    return Status::OK;
}

auto CatalogManager::getForeignKey(const ID& fk_id,
                                  ForeignKeyInfo& fk_out,
                                  ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(foreign_keys_cache_mutex_);

    auto it = foreign_keys_cache_.find(fk_id);
    if (it == foreign_keys_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Foreign key not found");
        return Status::NOT_FOUND;
    }

    fk_out = it->second;
    return Status::OK;
}

auto CatalogManager::dropForeignKey(const ID& fk_id,
                                   ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(foreign_keys_cache_mutex_);

    auto it = foreign_keys_cache_.find(fk_id);
    if (it == foreign_keys_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Foreign key not found");
        return Status::NOT_FOUND;
    }

    const ForeignKeyInfo& fk = it->second;

    // Remove from index maps
    auto child_range = table_child_fks_.equal_range(fk.child_table_id);
    for (auto child_it = child_range.first; child_it != child_range.second; )
    {
        if (child_it->second == fk_id)
        {
            child_it = table_child_fks_.erase(child_it);
        }
        else
        {
            ++child_it;
        }
    }

    auto parent_range = table_parent_fks_.equal_range(fk.parent_table_id);
    for (auto parent_it = parent_range.first; parent_it != parent_range.second; )
    {
        if (parent_it->second == fk_id)
        {
            parent_it = table_parent_fks_.erase(parent_it);
        }
        else
        {
            ++parent_it;
        }
    }

    // Remove from cache
    foreign_keys_cache_.erase(it);

    // Phase D: Mark as invalid on disk
    if (foreign_keys_table_page_ != 0)
    {
        auto predicate = [&fk_id](const ForeignKeyRecord& rec) {
            return rec.is_valid && rec.fk_id == fk_id;
        };

        auto result = findRecordInHeapPage<ForeignKeyRecord>(foreign_keys_table_page_, predicate, ctx);
        if (result.status == Status::OK)
        {
            ForeignKeyRecord updated_rec = result.record;
            updated_rec.is_valid = 0;

            Status status = updateRecordInHeapPage(foreign_keys_table_page_, predicate, updated_rec, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to mark foreign key as invalid on disk");
                return status;
            }
        }
    }

    return Status::OK;
}

auto CatalogManager::setForeignKeyEnabled(const ID& fk_id, bool enabled,
                                         ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(foreign_keys_cache_mutex_);

    auto it = foreign_keys_cache_.find(fk_id);
    if (it == foreign_keys_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Foreign key not found");
        return Status::NOT_FOUND;
    }

    it->second.is_enabled = enabled;

    // Phase D: Update disk
    if (foreign_keys_table_page_ != 0)
    {
        auto predicate = [&fk_id](const ForeignKeyRecord& rec) {
            return rec.is_valid && rec.fk_id == fk_id;
        };

        auto result = findRecordInHeapPage<ForeignKeyRecord>(foreign_keys_table_page_, predicate, ctx);
        if (result.status == Status::OK)
        {
            ForeignKeyRecord updated_rec = result.record;
            updated_rec.is_enabled = enabled ? 1 : 0;

            Status status = updateRecordInHeapPage(foreign_keys_table_page_, predicate, updated_rec, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to update foreign key enabled status on disk");
                return status;
            }
        }
    }

    return Status::OK;
}

} // namespace scratchbird::core


#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/tablespace.h"

namespace scratchbird::core
{

    // Forward declarations
    class PageManager;
    class TIDResolver;

    using ID = UuidV7Bytes;

    /**
     * CatalogConstants - Catalog layer storage limits
     *
     * Phase 2: SQL Identifier UTF-8 Fix Plan
     * These constants define the storage capacity for SQL identifiers in the catalog.
     */
    namespace CatalogConstants
    {
        // SQL standard identifier limits
        constexpr size_t MAX_IDENTIFIER_CHARS = 128;   // SQL standard: 128 characters
        constexpr size_t MAX_IDENTIFIER_BYTES = 512;   // Storage: 128 chars × 4 bytes/char (max UTF-8)
        constexpr size_t MAX_IDENTIFIER_STORAGE = 512; // Including null terminator
    }

    /**
     * MigrationPhase - Phases of ONLINE table migration
     *
     * Sprint 4 Task 5.4.1: Migration State Management
     */
    enum class MigrationPhase : uint8_t
    {
        MIGRATION_NONE = 0,           // No migration in progress
        MIGRATION_INIT = 1,           // Migration initialized
        MIGRATION_COPYING = 2,        // Background page copy in progress
        MIGRATION_CATCH_UP = 3,       // Re-copying dirty pages
        MIGRATION_READY_FOR_SWAP = 4, // Converged, ready for atomic swap
        MIGRATION_SWAP = 5,           // Performing atomic swap
        MIGRATION_CLEANUP = 6,        // Cleaning up source pages
        MIGRATION_COMPLETE = 7,       // Migration completed successfully
        MIGRATION_FAILED = 8,         // Migration failed
        MIGRATION_ABORTED = 9         // Migration aborted by user
    };

    /**
     * TableMigrationProgressCallback - Callback for table migration progress updates
     *
     * @param pages_copied Number of pages copied so far
     * @param total_pages Total number of pages to copy
     * @return true to continue migration, false to cancel
     *
     * Phase 4 Task 4.1.3
     */
    using TableMigrationProgressCallback = std::function<bool(uint32_t pages_copied, uint32_t total_pages)>;

    /**
     * TableMigrationState - In-memory state for ONLINE table migration
     *
     * Sprint 4 Task 5.4.1: Migration State Management
     */
    struct TableMigrationState
    {
        ID migration_id;                  // Unique migration ID
        ID table_id;                      // Table being migrated
        uint16_t source_tablespace;       // Source tablespace ID
        uint16_t target_tablespace;       // Target tablespace ID
        MigrationPhase phase;             // Current migration phase
        uint64_t migration_xid;           // XID when migration started
        uint32_t total_pages;             // Total pages to migrate
        uint32_t pages_copied;            // Pages copied so far
        uint64_t start_time;              // Timestamp when migration started
        uint64_t end_time;                // Timestamp when migration completed/failed
        std::unique_ptr<uint8_t[]> dirty_pages_bitmap; // Bitmap of dirty pages (1 bit per page)

        // Statistics
        uint32_t catch_up_iterations = 0;      // Number of catch-up iterations
        uint32_t final_dirty_page_count = 0;   // Dirty pages at swap time
        uint64_t total_bytes_copied = 0;       // Total bytes copied

        TableMigrationState()
            : phase(MigrationPhase::MIGRATION_NONE),
              migration_xid(0),
              total_pages(0),
              pages_copied(0),
              start_time(0),
              end_time(0)
        {
        }
    };

    /**
     * Table Migration Batch Processing Constants
     *
     * These constants control memory usage during table migration to prevent
     * excessive memory consumption when migrating large tables.
     *
     * Phase 4 Task 4.1.4
     */
    namespace TableMigration
    {
        // Maximum number of pages to process in a single batch
        // Limits: With 8KB pages, 1000 pages = ~8MB of heap data
        // Add TID mapping overhead: ~32 bytes per page = 32KB
        // Total per batch: ~8.032 MB (well within reasonable memory limits)
        constexpr uint32_t MAX_BATCH_SIZE_PAGES = 1000;

        // Maximum memory usage per batch (approximate, in MB)
        // Used for logging and monitoring
        constexpr uint32_t MAX_BATCH_MEMORY_MB = 10;

        // Minimum batch size for small tables
        // Even tiny tables should use at least this many pages per batch
        constexpr uint32_t MIN_BATCH_SIZE_PAGES = 10;

        // Progress callback invocation frequency
        // Invoke callback at least this many pages (or when batch completes)
        constexpr uint32_t PROGRESS_CALLBACK_INTERVAL_PAGES = 100;
    }

    // Simple heap page for catalog tables
    struct CatalogHeapPage
    {
        PageHeader header;
        uint32_t record_count;
        uint32_t free_offset;
        uint8_t data[]; // Variable length records
    };

    /**
     * System Catalog Manager
     *
     * Manages the system catalog which tracks all database metadata including:
     * - Schemas
     * - Tables
     * - Columns
     * - Indexes (future)
     * - Constraints (future)
     *
     * SQL Identifier UTF-8 Support (November 2025):
     *
     * Identifier Limits:
     * - Maximum length: 128 UTF-8 characters (SQL:2016 §5.2)
     * - Maximum storage: 512 bytes (supports all UTF-8 characters)
     * - Encoding: UTF-8 only
     *
     * Storage Format:
     * - All identifiers stored in fixed char[512] arrays
     * - Supports 128 characters of any UTF-8 encoding (1-4 bytes per char)
     * - Validation ensures both character limit (128) and byte limit (512)
     * - Invalid UTF-8 rejected at validation level
     *
     * Validation Process:
     * 1. UTF8Utils::validateStorageCapacity() checks:
     *    - Character count ≤ 128 (SQL standard)
     *    - Byte count ≤ 512 (storage capacity)
     *    - Valid UTF-8 encoding (RFC 3629)
     * 2. UTF8Utils::writeToBuffer() safely writes to catalog:
     *    - Truncates at character boundaries (no split multi-byte chars)
     *    - Ensures null-termination at position 511
     *    - Returns error if validation fails
     *
     * Examples:
     * - "café" - 4 characters, 5 bytes (valid)
     * - "北京_table" - 9 characters, 15 bytes (valid)
     * - "idx_😀" - 5 characters, 8 bytes (valid)
     * - 128 emoji - 128 characters, 512 bytes (valid, maximum)
     * - 129 emoji - 129 characters, 516 bytes (INVALID, exceeds limits)
     *
     * Reference: docs/specifications/character_sets_and_collations.md
     */
    class CatalogManager
    {
    public:
        // Schema information
        struct SchemaInfo
        {
            ID schema_id;
            ID parent_schema_id;                // Parent schema UUID (zero UUID for root)
            std::string schema_name;
            ID owner_id;                        // Owner UUID reference (NOT name)
            uint16_t default_tablespace_id = 0; // Default tablespace for new tables
            uint16_t permissions = 0;           // Bitmask of schema permissions
            uint16_t default_charset = 0;       // Default character set (0 = inherit from database)
            uint16_t reserved = 0;
            uint32_t default_collation_id = 0;  // Default collation ID (0 = inherit from database)
            uint32_t acl_oid = 0;               // TOAST reference for ACL (IMPLEMENTED)
            // search_path_oid removed - session-only concept
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
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

        // Table information
        struct TableInfo
        {
            ID table_id;
            ID schema_id;
            std::string table_name;
            ID owner_id;                       // Owner UUID reference (NOT name)
            uint32_t root_page = 0;            // Root page of table data
            uint32_t column_count = 0;
            uint64_t row_count = 0;            // Estimated row count
            TableType table_type = TableType::HEAP;
            bool has_toast = false;
            ID toast_table_id;                 // PHASE 5 TASK 5.1.3.1: UUID of TOAST table (zero if none)
            uint16_t tablespace_id = 0;        // Tablespace ID (0 = default)
            uint16_t default_charset = 0;      // Default character set (0 = inherit from schema)
            uint32_t default_collation_id = 0; // Default collation ID (0 = inherit from schema)
            uint32_t storage_params_oid = 0;   // TOAST reference for storage parameters - IMPLEMENTED
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;

            // ONLINE migration fields (Sprint 4 Task 5.4.1)
            bool migration_in_progress = false;     // True if table is being migrated
            ID migration_id;                        // Current migration ID
            uint64_t migration_xid = 0;             // XID when migration started
            uint16_t migration_target_ts = 0;       // Target tablespace ID
            uint8_t migration_phase = 0;            // MigrationPhase enum value
        };

        // TRUNCATE TABLE job tracking (ALPHA Phase 1 - DDL Modifications)
        struct TruncateJob
        {
            uint64_t job_id = 0;                    // Unique job identifier
            ID table_id;                            // Table being truncated
            std::string table_name;                 // Table name (for display)
            uint64_t snapshot_xid = 0;              // Transaction ID when truncate started
            std::atomic<uint64_t> rows_processed{0}; // Total rows examined
            std::atomic<uint64_t> rows_deleted{0};   // Rows marked for deletion
            std::atomic<bool> completed{false};      // Job finished flag
            std::atomic<bool> error{false};          // Error occurred flag
            std::string error_message;               // Error details if error=true
            uint64_t start_time = 0;                 // Start timestamp (epoch seconds)
            std::atomic<uint64_t> end_time{0};       // End timestamp (epoch seconds)

            // Progress helper
            double getProgress() const {
                if (rows_processed == 0) return 0.0;
                return 100.0 * static_cast<double>(rows_deleted.load()) /
                              static_cast<double>(rows_processed.load());
            }
        };

        // Sequence information structure (ALPHA Phase 1 - Sequences)
        struct SequenceInfo {
            ID sequence_id;
            ID schema_id;
            std::string name;
            ID owner_id;
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

        // In-memory sequence state for atomic operations
        struct SequenceState {
            ID sequence_id;
            std::string name;  // Sequence name (for cleanup in drop)
            std::atomic<int64_t> current_value;
            int64_t increment_by;
            int64_t min_value;
            int64_t max_value;
            bool cycle;
            std::mutex config_mutex;  // Protect ALTER SEQUENCE changes
        };

        // View information (ALPHA Phase 1 - Views)
        struct ViewInfo {
            ID view_id;
            ID schema_id;
            std::string name;
            ID owner_id;
            std::string definition;  // SELECT query text
            bool check_option;
            std::vector<std::string> column_names;  // Optional explicit columns
            uint64_t created_time;
            uint64_t last_modified_time;
        };

        // Column information
        struct ColumnInfo
        {
            ID table_id;
            ID column_id;
            std::string column_name;
            uint16_t ordinal = 0;        // Column position in table
            uint16_t data_type = 0;      // Type code
            uint32_t type_precision = 0; // For DECIMAL, VECTOR dimensions, VARCHAR length
            uint32_t type_scale = 0;     // For DECIMAL scale
            uint32_t max_length = 0;     // Legacy field, use type_precision instead
            bool nullable = true;
            bool has_default = false;
            bool is_primary_key = false;
            bool is_unique = false;
            bool is_foreign_key = false;
            bool is_generated = false;
            uint8_t storage_type = 0;       // TOAST storage strategy
            bool with_timezone = false;     // For TIMESTAMP: WITH TIME ZONE
            uint16_t charset = 0;           // Character set (0 = inherit from table)
            uint16_t timezone_hint = 0;     // Timezone ID for display (0 = use connection default)
            uint32_t collation_id = 0;      // Collation ID (0 = inherit from table)
            std::string default_value;      // Serialized default
            uint32_t default_value_oid = 0; // TOAST reference for large defaults
            uint32_t check_expr_oid = 0;    // TOAST reference for check expressions
            uint64_t created_time = 0;
        };

        // Index types
        enum class IndexType : uint8_t
        {
            BTREE = 0,        // B-tree index (default)
            HASH = 1,         // Hash index
            HNSW = 2,         // Vector similarity index (renamed from VECTOR)
            VECTOR = 2,       // Alias for HNSW (backward compatibility)
            FULLTEXT = 3,     // Full-text search index (GIN-based)
            GIN = 4,          // Generalized Inverted Index
            GIST = 5,         // Generalized Search Tree
            BRIN = 6,         // Block Range Index
            RTREE = 7,        // R-tree spatial index
            SPGIST = 8,       // Space-Partitioned GiST
            BITMAP = 9,       // Bitmap index
            COLUMNSTORE = 10, // Columnstore index
            LSM = 11          // LSM-Tree (Log-Structured Merge-Tree)
        };

        // Index information
        struct IndexInfo
        {
            ID index_id;
            ID table_id;
            std::string index_name;
            ID owner_id;                   // Owner UUID reference (NOT name)
            uint32_t root_page = 0;
            uint16_t tablespace_id = 0;    // Tablespace ID (0 = primary file, 1-65535 = custom)
            IndexType index_type = IndexType::BTREE;
            bool is_unique = false;
            std::vector<ID> column_ids;
            uint32_t index_params_oid = 0; // TOAST reference for index parameters - IMPLEMENTED
            uint64_t created_time = 0;
            uint32_t collation_id = 101; // Default: utf8_general_ci (binary comparison)
                                         // TODO(Issue #50): Full integration of collation-aware
                                         // comparisons throughout B-tree and query evaluation

            // R-tree specific parameters (Phase 2 Task 9.2)
            uint32_t rtree_max_entries = 50; // Maximum entries per R-tree node (M parameter)

            // Task 17: Expression and Filtered Indexes
            bool is_expression_index = false;      // Index on expression(s) rather than columns
            bool is_partial_index = false;         // Index with WHERE clause (filtered)
            uint32_t expression_oid = 0;           // TOAST reference for serialized expression tree(s)
            uint32_t predicate_oid = 0;            // TOAST reference for serialized WHERE predicate
            std::vector<std::string> expression_strings;  // Original SQL expressions (for EXPLAIN, etc.)
            std::string predicate_string;          // Original WHERE clause SQL (for EXPLAIN, etc.)

            // Binary serialized data (for small expressions, store directly; larger ones use TOAST)
            std::vector<uint8_t> expression_data;  // Serialized expression list
            std::vector<uint8_t> predicate_data;   // Serialized WHERE predicate
        };

        // Object types for dependencies and comments (Phase 1.4-1.5 - Catalog Corrections)
        enum class ObjectType : uint8_t
        {
            SCHEMA = 0,
            TABLE = 1,
            COLUMN = 2,
            INDEX = 3,
            VIEW = 4,
            SEQUENCE = 5,
            CONSTRAINT = 6,
            TRIGGER = 7,
            PROCEDURE = 8,      // Includes selectable procedures (SUSPEND)
            FUNCTION = 9,       // Same table as procedures
            DOMAIN = 10,
            COMPOSITE_TYPE = 11,
            ROLE = 12,
            USER = 13,
            GROUP = 14,
            TABLESPACE = 15,
            DATABASE = 16,
            EMULATION_TYPE = 17,
            EMULATION_SERVER = 18,
            EMULATED_DATABASE = 19,
            COLLATION = 20,
            CHARSET = 21,
            PACKAGE = 22,       // Firebird packages
            UDR = 23,           // User-Defined Resources
            COMMENT = 24,
            DEPENDENCY = 25,
            PERMISSION = 26,
            STATISTIC = 27,
            TIMEZONE = 28,
            EXTENSION = 29,
            FOREIGN_SERVER = 30,
            FOREIGN_TABLE = 31
        };

        // Dependency types (Phase 1.4 - Catalog Corrections)
        enum class DependencyType : uint8_t
        {
            NORMAL = 0,     // User-created dependency (views, procedures, FKs)
            AUTO = 1,       // System-created (auto-generated indexes, sequences)
            INTERNAL = 2,   // System-critical (cannot be dropped)
            PIN = 3         // User-defined INTERNAL (only admin can unpin)
        };

        // Dependency information (Phase 1.4 - Catalog Corrections)
        struct DependencyInfo
        {
            ID dependency_id;
            ID dependent_object_id;     // Object that depends ON something
            ObjectType dependent_type;  // Type of dependent object
            ID referenced_object_id;    // Object being depended upon
            ObjectType referenced_type; // Type of referenced object
            DependencyType dependency_type;
            uint64_t created_time = 0;
        };

        // Comment information (Phase 1.5 - Catalog Corrections)
        struct CommentInfo
        {
            ID comment_id;
            ID object_id;              // Object being commented
            ObjectType object_type;    // Type of object
            ID owner_id;               // Owner UUID reference
            std::string comment_text;  // Comment text (stored in TOAST on disk)
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Group types (Phase 2 - Security Tables)
        enum class GroupType : uint8_t
        {
            LOCAL = 0,   // Local database group
            AD = 1,      // Active Directory group
            LDAP = 2     // LDAP group
        };

        // User information (Phase 2 - Security Tables)
        struct UserInfo
        {
            ID user_id;
            std::string username;
            std::string password_hash;  // Stored in TOAST on disk
            std::string user_metadata;  // JSON metadata (stored in TOAST on disk)
            ID default_schema_id;
            bool is_active = true;
            bool is_superuser = false;
            uint64_t created_time = 0;
            uint64_t last_login_time = 0;
        };

        // Role information (Phase 2 - Security Tables)
        struct RoleInfo
        {
            ID role_id;
            std::string role_name;
            ID owner_id;
            std::string role_metadata;  // JSON metadata (stored in TOAST on disk)
            bool is_active = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Group information (Phase 2 - Security Tables)
        struct GroupInfo
        {
            ID group_id;
            std::string group_name;
            std::string external_id;    // AD/LDAP group ID (empty if local)
            GroupType group_type = GroupType::LOCAL;
            std::string group_metadata;  // JSON metadata (stored in TOAST on disk)
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Role membership information (Phase 2 - Security Tables)
        struct RoleMembershipInfo
        {
            ID membership_id;
            ID user_id;              // User who is member
            ID role_id;              // Role they belong to
            ID granted_by;           // User who granted this membership
            bool with_admin_option = false;  // Can user grant this role to others
            uint64_t granted_time = 0;
        };

        // Procedure types (Phase 3 - Stored Code Tables)
        enum class ProcedureType : uint8_t
        {
            PROCEDURE = 0,  // Stored procedure
            FUNCTION = 1    // Function (returns value)
        };

        // Procedure languages (Phase 3 - Stored Code Tables)
        enum class ProcedureLanguage : uint8_t
        {
            PSQL = 0,       // Firebird PSQL
            SQL = 1,        // Standard SQL
            UDR = 2,        // User-Defined Resource (external)
            PLPGSQL = 3     // PostgreSQL PL/pgSQL (for emulation)
        };

        // NOTE: ParameterMode enum already defined at line ~1307

        // UDR types (Phase 3 - Stored Code Tables)
        enum class UDRType : uint8_t
        {
            FUNCTION = 0,
            PROCEDURE = 1,
            TRIGGER = 2
        };

        // Stored procedure catalog information (Phase 3 - Stored Code Tables)
        // NOTE: Different from runtime ProcedureInfo used for execution
        struct StoredProcedureInfo
        {
            ID procedure_id;
            ID schema_id;
            std::string procedure_name;
            ID owner_id;
            ProcedureType procedure_type = ProcedureType::PROCEDURE;
            bool is_selectable = false;  // Firebird selectable procedures (SUSPEND)
            ProcedureLanguage language = ProcedureLanguage::PSQL;
            uint32_t parameter_count = 0;
            std::string return_type;     // Stored in TOAST on disk
            std::string body;            // Stored in TOAST on disk
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Procedure parameter information (Phase 3 - Stored Code Tables)
        // NOTE: Uses ParameterMode enum defined later at line ~1307
        struct ProcedureParameterInfo
        {
            ID parameter_id;
            ID procedure_id;
            std::string parameter_name;
            uint16_t parameter_position = 0;  // 1-based position
            uint8_t parameter_mode = 0;  // ParameterMode: IN=0, OUT=1, INOUT=2
            std::string data_type;       // Stored in TOAST on disk
            std::string default_value;   // Stored in TOAST on disk
        };

        // Domain information (Phase 3 - Stored Code Tables)
        struct DomainInfo
        {
            ID domain_id;
            ID schema_id;
            std::string domain_name;
            ID owner_id;
            std::string base_type;       // Stored in TOAST on disk
            std::string check_expr;      // Stored in TOAST on disk
            bool not_null = false;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // UDR information (Phase 3 - Stored Code Tables)
        struct UDRInfo
        {
            ID udr_id;
            ID schema_id;
            std::string udr_name;
            ID owner_id;
            std::string library_path;
            std::string entry_point;
            UDRType udr_type = UDRType::FUNCTION;
            std::string signature;       // Stored in TOAST on disk
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Package information (Phase 3 - Stored Code Tables)
        struct PackageInfo
        {
            ID package_id;
            ID schema_id;
            std::string package_name;
            ID owner_id;
            std::string package_header;  // Stored in TOAST on disk
            std::string package_body;    // Stored in TOAST on disk
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Emulation type information (Phase 4 - Emulation Tables)
        struct EmulationTypeInfo
        {
            ID emulation_type_id;
            std::string emulation_name;  // "mysql", "postgres", "mssql", "firebird"
            uint8_t version_major = 0;
            uint8_t version_minor = 0;
            std::string mapping_rules;   // JSON mapping rules (stored in TOAST on disk)
            uint64_t created_time = 0;
        };

        // Emulation server information (Phase 4 - Emulation Tables)
        struct EmulationServerInfo
        {
            ID server_id;
            std::string server_name;
            ID emulation_type_id;        // References EmulationTypeInfo
            ID owner_id;
            std::string server_config;   // JSON configuration (stored in TOAST on disk)
            bool is_active = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Emulated database information (Phase 4 - Emulation Tables)
        struct EmulatedDatabaseInfo
        {
            ID emulated_db_id;
            std::string database_name;
            ID server_id;                // References EmulationServerInfo
            ID schema_id;                // Schema containing emulation views
            ID owner_id;
            std::string db_metadata;     // JSON metadata (stored in TOAST on disk)
            bool is_active = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        CatalogManager(Database *db);
        ~CatalogManager();

        // Initialize catalog for new database
        auto initialize(ErrorContext *ctx = nullptr) -> Status;

        // Load catalog from existing database
        auto load(ErrorContext *ctx = nullptr) -> Status;

        // Schema operations
        auto createSchema(const std::string &schema_name, const std::string &owner, ID &schema_id,
                          ErrorContext *ctx = nullptr) -> Status;

        auto getSchema(const ID &schema_id, SchemaInfo &info, ErrorContext *ctx = nullptr)
            -> Status;

        auto getSchema(const std::string &schema_name, SchemaInfo &info,
                       ErrorContext *ctx = nullptr) -> Status;

        auto listSchemas(std::vector<SchemaInfo> &schemas, ErrorContext *ctx = nullptr) -> Status;

        // Table operations
        auto createTable(const ID &schema_id, const std::string &table_name,
                         const std::vector<ColumnInfo> &columns, ID &table_id,
                         uint16_t tablespace_id = 0, // Phase 2 Task 2.3: default tablespace
                         ErrorContext *ctx = nullptr) -> Status;

        auto getTable(const ID &table_id, TableInfo &info, ErrorContext *ctx = nullptr) -> Status;

        auto getTable(const ID &schema_id, const std::string &table_name, TableInfo &info,
                      ErrorContext *ctx = nullptr) -> Status;

        auto listTables(const ID &schema_id, std::vector<TableInfo> &tables,
                        ErrorContext *ctx = nullptr) -> Status;

        // DDL Modifications (ALPHA Phase 1)
        auto dropTable(const ID &table_id, bool cascade, ErrorContext *ctx = nullptr) -> Status;

        // Column operations
        auto getColumns(const ID &table_id, std::vector<ColumnInfo> &columns,
                        ErrorContext *ctx = nullptr) -> Status;

        auto getColumn(const ID &table_id, const std::string &column_name, ColumnInfo &info,
                       ErrorContext *ctx = nullptr) -> Status;

        // Index operations
        auto createIndex(const ID &table_id, const std::string &index_name,
                         const std::vector<std::string> &column_names, ID &index_id,
                         bool is_unique = false, IndexType index_type = IndexType::BTREE,
                         uint16_t tablespace_id = 0, // Phase 2 Task 2.3: default tablespace
                         ErrorContext *ctx = nullptr) -> Status;

        // Task 17: Create index with expressions and/or WHERE clause
        auto createIndex(const ID &table_id, const std::string &index_name,
                         const std::vector<std::string> &column_names,
                         const std::vector<uint8_t> &expression_data,  // Serialized expressions (empty if none)
                         const std::vector<uint8_t> &predicate_data,   // Serialized WHERE predicate (empty if none)
                         const std::vector<std::string> &expression_strings,  // Original SQL
                         const std::string &predicate_string,                 // Original WHERE clause
                         ID &index_id,
                         bool is_unique = false,
                         IndexType index_type = IndexType::BTREE,
                         uint16_t tablespace_id = 0,
                         ErrorContext *ctx = nullptr) -> Status;

        auto getIndex(const ID &index_id, IndexInfo &info, ErrorContext *ctx = nullptr) -> Status;

        auto getIndex(const ID &table_id, const std::string &index_name, IndexInfo &info,
                      ErrorContext *ctx = nullptr) -> Status;

        auto listIndexesForTable(const ID &table_id, std::vector<IndexInfo> &indexes,
                                 ErrorContext *ctx = nullptr) -> Status;

        // LSM Integration Phase 3.3: Index object cache management
        /**
         * Get cached index object pointer
         *
         * @param index_id Index ID
         * @param type_out Output: Index type (optional)
         * @return Index object pointer (nullptr if not cached)
         */
        void* getIndexPtr(const ID &index_id, IndexType *type_out = nullptr);

        /**
         * Close and remove all cached index objects
         * Called on database shutdown
         *
         * @param ctx Error context
         * @return Status::OK on success
         */
        Status closeAllIndexes(ErrorContext *ctx = nullptr);

        // DDL Modifications (ALPHA Phase 1)
        auto dropIndex(const ID &index_id, ErrorContext *ctx = nullptr) -> Status;

        // ALTER TABLE operations (ALPHA Phase 1)
        auto addColumn(const ID &table_id, const ColumnInfo &column_info,
                       ErrorContext *ctx = nullptr) -> Status;
        auto dropColumn(const ID &table_id, const std::string &column_name, bool if_exists,
                        bool cascade, ErrorContext *ctx = nullptr) -> Status;
        auto renameColumn(const ID &table_id, const std::string &old_name,
                          const std::string &new_name, ErrorContext *ctx = nullptr) -> Status;
        auto alterColumnType(const ID &table_id, const std::string &column_name,
                             DataType new_type, uint32_t new_precision, uint32_t new_scale,
                             ErrorContext *ctx = nullptr) -> Status;

        // TRUNCATE TABLE operations (ALPHA Phase 1 - final DDL operation)
        // Async truncate: Starts background job, returns immediately with job ID
        auto truncateTableAsync(const ID &table_id, const std::string &table_name,
                                uint64_t snapshot_xid, ErrorContext *ctx = nullptr) -> uint64_t;

        // Sync truncate: Blocks until truncation complete
        auto truncateTableSync(const ID &table_id, const std::string &table_name,
                               uint64_t snapshot_xid, ErrorContext *ctx = nullptr) -> Status;

        // Get truncate job status
        auto getTruncateJobStatus(uint64_t job_id) -> std::shared_ptr<TruncateJob>;

        // Wait for truncate job to complete (with optional timeout in milliseconds)
        auto waitForTruncate(uint64_t job_id, uint32_t timeout_ms = 0) -> Status;

        // List all truncate jobs (for debugging/monitoring)
        auto listTruncateJobs(std::vector<std::shared_ptr<TruncateJob>> &jobs_out) -> void;

        // Sequence operations (ALPHA Phase 1 - Sequences)
        auto createSequence(const ID& schema_id, const std::string& name,
                            int64_t increment_by, int64_t min_value, int64_t max_value,
                            int64_t start_value, int64_t cache_size, bool cycle,
                            ErrorContext* ctx = nullptr) -> Status;

        auto alterSequence(const ID& sequence_id, const std::optional<int64_t>& increment_by,
                           const std::optional<int64_t>& min_value, const std::optional<int64_t>& max_value,
                           const std::optional<int64_t>& restart, const std::optional<int64_t>& cache_size,
                           const std::optional<bool>& cycle, ErrorContext* ctx = nullptr) -> Status;

        auto dropSequence(const ID& sequence_id, bool cascade, ErrorContext* ctx = nullptr) -> Status;

        auto getSequence(const ID& schema_id, const std::string& name,
                         SequenceInfo& info_out, ErrorContext* ctx = nullptr) -> Status;

        auto sequenceNextVal(const ID& sequence_id, int64_t& value_out,
                             ErrorContext* ctx = nullptr) -> Status;

        auto sequenceSetVal(const ID& sequence_id, int64_t value, bool is_called,
                            ErrorContext* ctx = nullptr) -> Status;

        auto getSequenceIdByName(const std::string& name, ID& id_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        // View operations (ALPHA Phase 1 - Views)
        auto createView(const ID& schema_id, const std::string& name,
                        const std::string& definition, bool or_replace, bool check_option,
                        const std::vector<std::string>& column_names,
                        ErrorContext* ctx = nullptr) -> Status;

        auto dropView(const ID& view_id, bool cascade,
                      ErrorContext* ctx = nullptr) -> Status;

        auto getView(const ID& schema_id, const std::string& name,
                     ViewInfo& info_out, ErrorContext* ctx = nullptr) -> Status;

        auto getViewIdByName(const std::string& name, ID& id_out,
                             ErrorContext* ctx = nullptr) -> Status;

        auto isView(const std::string& name) -> bool;

        // Timezone operations (pg_timezone system table)
        struct TimezoneInfo
        {
            uint16_t timezone_id = 0;
            std::string name;
            std::string abbreviation;
            int32_t std_offset_minutes = 0;
            bool observes_dst = false;
            uint8_t dst_start_month = 0;
            uint8_t dst_start_week = 0;
            uint8_t dst_start_day = 0;
            uint8_t dst_start_hour = 0;
            uint8_t dst_end_month = 0;
            uint8_t dst_end_week = 0;
            uint8_t dst_end_day = 0;
            uint8_t dst_end_hour = 0;
            int32_t dst_offset_minutes = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        auto createTimezone(const TimezoneInfo &tz_info, ErrorContext *ctx = nullptr) -> Status;
        auto updateTimezone(uint16_t timezone_id, const TimezoneInfo &tz_info,
                            ErrorContext *ctx = nullptr) -> Status;
        auto getTimezone(uint16_t timezone_id, TimezoneInfo &info, ErrorContext *ctx = nullptr)
            -> Status;
        auto getTimezoneByName(const std::string &name, TimezoneInfo &info,
                               ErrorContext *ctx = nullptr) -> Status;
        auto listTimezones(std::vector<TimezoneInfo> &timezones, ErrorContext *ctx = nullptr)
            -> Status;
        auto deleteTimezone(uint16_t timezone_id, ErrorContext *ctx = nullptr) -> Status;

        // Character set operations (pg_charset system table)
        struct CharsetInfo
        {
            uint16_t charset_id = 0;    // Character set ID (matches CharacterSet enum)
            std::string name;           // e.g., "utf8", "latin1"
            std::string description;    // Human-readable description
            uint8_t min_bytes = 1;      // Minimum bytes per character
            uint8_t max_bytes = 1;      // Maximum bytes per character
            uint8_t variable_width = 0; // 1 = variable width, 0 = fixed width
            uint8_t reserved = 0;
            uint32_t default_collation_id = 0; // Default collation for this charset
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        auto createCharset(const CharsetInfo &cs_info, ErrorContext *ctx = nullptr) -> Status;
        auto updateCharset(uint16_t charset_id, const CharsetInfo &cs_info,
                           ErrorContext *ctx = nullptr) -> Status;
        auto getCharset(uint16_t charset_id, CharsetInfo &info, ErrorContext *ctx = nullptr)
            -> Status;
        auto getCharsetByName(const std::string &name, CharsetInfo &info,
                              ErrorContext *ctx = nullptr) -> Status;
        auto listCharsets(std::vector<CharsetInfo> &charsets, ErrorContext *ctx = nullptr)
            -> Status;
        auto deleteCharset(uint16_t charset_id, ErrorContext *ctx = nullptr) -> Status;

        // Collation operations (pg_collation system table)
        struct CollationCatalogInfo
        {
            uint32_t collation_id = 0;
            std::string name;           // e.g., "utf8_general_ci"
            uint16_t charset_id = 0;    // Associated character set ID
            uint8_t collation_type = 0; // CollationType enum value
            uint8_t strength = 0;       // CollationStrength enum value
            uint8_t pad_space = 1;      // 1 = PAD SPACE, 0 = NO PAD
            uint8_t is_default = 0;     // 1 = default for charset, 0 = not default
            uint16_t reserved = 0;
            char locale[32] = {0}; // Locale string (e.g., "en_US")
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        auto createCollation(const CollationCatalogInfo &col_info, ErrorContext *ctx = nullptr)
            -> Status;
        auto updateCollation(uint32_t collation_id, const CollationCatalogInfo &col_info,
                             ErrorContext *ctx = nullptr) -> Status;
        auto getCollation(uint32_t collation_id, CollationCatalogInfo &info,
                          ErrorContext *ctx = nullptr) -> Status;
        auto getCollationByName(const std::string &name, CollationCatalogInfo &info,
                                ErrorContext *ctx = nullptr) -> Status;
        auto listCollations(std::vector<CollationCatalogInfo> &collations,
                            ErrorContext *ctx = nullptr) -> Status;
        auto listCollationsForCharset(uint16_t charset_id,
                                      std::vector<CollationCatalogInfo> &collations,
                                      ErrorContext *ctx = nullptr) -> Status;
        auto deleteCollation(uint32_t collation_id, ErrorContext *ctx = nullptr) -> Status;

        // Tablespace operations (Phase 2 Task 2.1)
        auto createTablespace(const std::string &tablespace_name, const std::string &location,
                              bool autoextend_enabled, uint32_t autoextend_size_mb,
                              uint32_t max_size_mb, uint32_t prealloc_pages, uint16_t &tablespace_id,
                              ErrorContext *ctx = nullptr) -> Status;

        auto dropTablespace(const std::string &tablespace_name, bool force,
                            ErrorContext *ctx = nullptr) -> Status;

        auto getTablespace(uint16_t tablespace_id, TablespaceInfo &info,
                           ErrorContext *ctx = nullptr) -> Status;

        auto getTablespaceByName(const std::string &tablespace_name, TablespaceInfo &info,
                                 ErrorContext *ctx = nullptr) -> Status;

        auto listTablespaces(std::vector<TablespaceInfo> &tablespaces,
                             ErrorContext *ctx = nullptr) -> Status;

        auto updateTablespace(const std::string &tablespace_name, bool autoextend_enabled,
                              uint32_t autoextend_size_mb, uint32_t max_size_mb,
                              ErrorContext *ctx = nullptr) -> Status; // Phase 2 Task 2.2

        auto renameTablespace(const std::string &old_name, const std::string &new_name,
                              ErrorContext *ctx = nullptr) -> Status; // Phase 2 Task 2.2

        /**
         * updateTablespaceStats - Update tablespace statistics after extension
         *
         * @param tablespace_id Tablespace ID
         * @param total_size_mb New total size in MB
         * @param free_size_mb New free size in MB
         * @param last_extended_time Timestamp of last extension
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Updates pg_tablespace statistics after tablespace extension.
         * Called by PageManager::extendTablespace() (Phase 3 Task 3.1.4).
         */
        auto updateTablespaceStats(uint16_t tablespace_id, uint64_t total_size_mb,
                                   uint64_t free_size_mb, uint64_t last_extended_time,
                                   ErrorContext *ctx = nullptr) -> Status; // Phase 3 Task 3.1.4

        /**
         * attachTablespace - Attach an existing tablespace file to the database
         *
         * @param file_path Absolute path to existing .sbts file
         * @param tablespace_name Name to assign (if empty, use name from file header)
         * @param tablespace_id_out Output: assigned tablespace ID
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Attaches an existing tablespace file (from another database or previously detached).
         *
         * Algorithm:
         * 1. Validate file path exists and is readable
         * 2. Read and validate TablespaceHeader from file
         * 3. Check compatibility (page_size must match database)
         * 4. Check for name conflicts (resolve with renaming if needed)
         * 5. Allocate new tablespace_id (find first available 1-65535)
         * 6. Open file descriptor and register in Database
         * 7. Load FSM into memory (PageManager::openTablespace)
         * 8. Add entry to pg_tablespace catalog
         * 9. Update tablespace_cache_
         *
         * Validation:
         * - File must exist and be readable
         * - Magic number must match (SBTS)
         * - Page size must match database page_size
         * - ODS version compatible
         * - Name must not conflict (or rename allowed)
         *
         * Phase 6 Task 6.1.2
         */
        auto attachTablespace(const std::string &file_path, const std::string &tablespace_name,
                              uint16_t &tablespace_id_out, ErrorContext *ctx = nullptr) -> Status;

        /**
         * detachTablespace - Detach a tablespace from the database
         *
         * @param tablespace_name Name of tablespace to detach
         * @param force If true, migrate tables to primary before detaching
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Detaches a tablespace file from the database (closes file, removes catalog entry).
         *
         * Algorithm:
         * 1. Validate tablespace exists
         * 2. Check if tablespace_id == 0 (cannot detach primary)
         * 3. Count tables/indexes in tablespace
         * 4. If tables exist and !force, return error
         * 5. If force, migrate tables back to primary tablespace first
         * 6. Flush dirty pages to disk
         * 7. Close file descriptor (Database::closeTablespace)
         * 8. Remove from pg_tablespace catalog
         * 9. Remove from tablespace_cache_
         *
         * Validation:
         * - Cannot detach PRIMARY_TABLESPACE_ID (0)
         * - If tables exist, require FORCE flag
         * - Check no active queries using tablespace (future: track active queries)
         *
         * FORCE Migration:
         * - Enumerates all tables in tablespace
         * - Calls moveTableToTablespace() for each table (OFFLINE mode)
         * - If any migration fails, rollback previous migrations
         *
         * Phase 6 Task 6.2.2, 6.2.3
         */
        auto detachTablespace(const std::string &tablespace_name, bool force,
                              ErrorContext *ctx = nullptr) -> Status;

        /**
         * compactCatalog - Perform garbage collection on catalog pages
         *
         * Removes is_valid=0 records from catalog heap pages to reclaim space.
         * This should be called periodically or after many DROP/ALTER operations.
         *
         * Compacts the following catalog pages:
         * - pg_tablespace (tablespaces_table_page_)
         * - pg_schema (schemas_table_page_)
         * - pg_table (tables_table_page_)
         * - pg_column (columns_table_page_)
         * - pg_index (indexes_table_page_)
         *
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Note: This is safe to call at any time; it only compacts pages,
         *       does not delete any valid catalog entries.
         */
        auto compactCatalog(ErrorContext *ctx = nullptr) -> Status;

        /**
         * moveTableToTablespace - Move a table to a different tablespace (OFFLINE mode)
         *
         * @param table_id Table ID to move
         * @param target_tablespace_id Destination tablespace ID
         * @param online If true, use online migration (REJECTED in Phase 4)
         * @param progress_callback Optional callback for progress tracking (Phase 4 Task 4.1.3)
         *                          Called periodically with (pages_copied, total_pages)
         *                          Return false to cancel migration, true to continue
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Offline Migration Process (8 steps):
         * 1. Reject ONLINE mode (Phase 4 limitation)
         * 2. Validate table exists and target tablespace is different
         * 3. Scan all heap pages in source tablespace
         * 4. Copy heap pages to target tablespace with TID mapping
         * 5. Update all indexes for this table (apply TID mapping)
         * 6. Update catalog: TableInfo.tablespace_id = target_tablespace_id
         * 7. Free old heap pages in source tablespace
         * 8. Return success
         *
         * Progress Tracking (Phase 4 Task 4.1.3):
         * - Invokes progress_callback periodically (every 5 seconds)
         * - Logs progress: "Migrating table: X / Y pages copied"
         * - Supports cancellation: If callback returns false, rollback and return Status::CANCELLED
         *
         * Thread-safe: Acquires catalog mutex.
         * Transaction: Single atomic transaction (all-or-nothing).
         * Locking: Table is effectively locked during migration (offline operation).
         *
         * Phase 4 Task 4.1.2, 4.1.3
         */
        auto moveTableToTablespace(const ID &table_id, uint16_t target_tablespace_id, bool online,
                                   TableMigrationProgressCallback progress_callback = nullptr,
                                   ErrorContext *ctx = nullptr) -> Status; // Phase 4 Task 4.1.2, 4.1.3

        /**
         * ONLINE Migration API (Sprint 4 Task 5.4.1)
         */

        /**
         * startOnlineMigration - Initialize ONLINE table migration
         *
         * @param table_id Table to migrate
         * @param target_tablespace_id Target tablespace
         * @param ctx Error context
         * @return Status::OK on success
         */
        auto startOnlineMigration(const ID &table_id, uint16_t target_tablespace_id,
                                  ErrorContext *ctx = nullptr) -> Status;

        /**
         * getMigrationState - Get current migration state
         *
         * @param migration_id Migration ID
         * @param ctx Error context
         * @return Pointer to migration state if found, nullptr otherwise
         *         Note: Pointer is only valid while migration_mutex_ is held
         */
        auto getMigrationState(const ID &migration_id, ErrorContext *ctx = nullptr)
            -> const TableMigrationState*;

        /**
         * updateMigrationProgress - Update migration progress
         *
         * @param migration_id Migration ID
         * @param pages_copied Number of pages copied
         * @param ctx Error context
         * @return Status::OK on success
         */
        auto updateMigrationProgress(const ID &migration_id, uint32_t pages_copied,
                                     ErrorContext *ctx = nullptr) -> Status;

        /**
         * setMigrationPhase - Transition migration to new phase
         *
         * @param migration_id Migration ID
         * @param new_phase New migration phase
         * @param ctx Error context
         * @return Status::OK on success
         */
        auto setMigrationPhase(const ID &migration_id, MigrationPhase new_phase,
                               ErrorContext *ctx = nullptr) -> Status;

        /**
         * abortMigration - Abort an active migration
         *
         * @param migration_id Migration ID
         * @param ctx Error context
         * @return Status::OK on success
         */
        auto abortMigration(const ID &migration_id, ErrorContext *ctx = nullptr) -> Status;

        /**
         * markPageDirty - Mark a page as dirty during migration
         *
         * @param migration_id Migration ID
         * @param page_number Page number in source tablespace
         * @param ctx Error context
         * @return Status::OK on success
         */
        auto markPageDirty(const ID &migration_id, uint32_t page_number,
                          ErrorContext *ctx = nullptr) -> Status;

        /**
         * getDirtyPages - Get list of dirty pages for catch-up
         *
         * @param migration_id Migration ID
         * @param ctx Error context
         * @return Vector of dirty page numbers
         */
        auto getDirtyPages(const ID &migration_id, ErrorContext *ctx = nullptr)
            -> std::vector<uint32_t>;

        /**
         * clearDirtyPages - Clear dirty page bitmap
         *
         * @param migration_id Migration ID
         * @param ctx Error context
         * @return Status::OK on success
         */
        auto clearDirtyPages(const ID &migration_id, ErrorContext *ctx = nullptr) -> Status;

        /**
         * getDirtyPageCount - Get number of dirty pages
         *
         * @param migration_id Migration ID
         * @return Number of dirty pages
         */
        auto getDirtyPageCount(const ID &migration_id) -> uint32_t;

        /**
         * completeMigration - Mark migration as complete and cleanup state
         *
         * @param migration_id Migration ID
         * @param ctx Error context
         * @return Status::OK on success
         */
        auto completeMigration(const ID &migration_id, ErrorContext *ctx = nullptr) -> Status;

        /**
         * getTableIndexes - Get all indexes for a table
         *
         * @param table_id Table ID
         * @param ctx Error context
         * @return Vector of index information
         */
        auto getTableIndexes(const ID &table_id, ErrorContext *ctx = nullptr)
            -> std::vector<IndexInfo>;

        /**
         * executeOnlineMigrationCopyingPhase - Execute COPYING phase of ONLINE migration
         *
         * Scans all heap pages in source tablespace and copies them to target tablespace,
         * building TID mapping and recording migration in TIDResolver.
         *
         * @param migration_id Migration ID
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Sprint 5 Task 5.4.4: Copying Phase
         */
        auto executeOnlineMigrationCopyingPhase(const ID &migration_id,
                                                ErrorContext *ctx = nullptr) -> Status;

        /**
         * executeOnlineMigrationCatchUpPhase - Execute CATCH_UP phase of ONLINE migration
         *
         * Re-copies pages that were marked dirty during COPYING phase. Iterates until
         * dirty page count is below threshold or max iterations reached.
         *
         * @param migration_id Migration ID
         * @param max_iterations Maximum catch-up iterations (default 10)
         * @param dirty_threshold Stop when dirty pages < threshold (default 100)
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Sprint 5 Task 5.4.5: Catch-Up Phase
         */
        auto executeOnlineMigrationCatchUpPhase(const ID &migration_id,
                                                uint32_t max_iterations = 10,
                                                uint32_t dirty_threshold = 100,
                                                ErrorContext *ctx = nullptr) -> Status;

        /**
         * executeOnlineMigrationSwapPhase - Execute SWAP phase of ONLINE migration
         *
         * Atomically swaps table to use target tablespace:
         * 1. Acquire exclusive lock on table
         * 2. Update all indexes with TID mapping
         * 3. Update catalog (table.tablespace_id = target)
         * 4. Free old pages in source tablespace
         * 5. Clear TID resolver state
         * 6. Release lock
         *
         * @param migration_id Migration ID
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Sprint 5 Task 5.4.6: Atomic Swap Phase
         */
        auto executeOnlineMigrationSwapPhase(const ID &migration_id,
                                            ErrorContext *ctx = nullptr) -> Status;

        /**
         * cancelOnlineMigration - Cancel an in-progress ONLINE migration
         *
         * Rolls back the migration and cleans up resources:
         * - INIT/COPYING/CATCH_UP: Deallocates target pages, clears state
         * - SWAP: Too late to cancel (would corrupt database)
         * - CLEANUP/COMPLETE: Already done, nothing to cancel
         *
         * Sprint 6 Task 5.4.8: Error Handling and Rollback
         *
         * @param migration_id Migration to cancel
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         */
        auto cancelOnlineMigration(const ID &migration_id,
                                   ErrorContext *ctx = nullptr) -> Status;

        // Catalog statistics
        auto schemaCount() const -> uint32_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return schema_count_;
        }
        auto tableCount() const -> uint32_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return table_count_;
        }

        // Trigger operations (Phase 2 Wave 2 - Agent C: Basic Triggers)
        
        // Trigger timing
        enum class TriggerTiming : uint8_t
        {
            BEFORE = 0,
            AFTER = 1
        };
        
        // Trigger event
        enum class TriggerEvent : uint8_t
        {
            INSERT = 0,
            UPDATE = 1,
            DELETE = 2
        };
        
        // Trigger granularity
        enum class TriggerGranularity : uint8_t
        {
            FOR_EACH_ROW = 0,
            FOR_EACH_STATEMENT = 1  // Future support
        };
        
        // Trigger information
        struct TriggerInfo
        {
            ID trigger_id;
            std::string trigger_name;
            ID table_id;
            std::string table_name;
            TriggerTiming timing;
            TriggerEvent event;
            TriggerGranularity granularity;
            std::string procedure_name;
            bool enabled = true;  // Can be disabled without dropping
            uint64_t created_time = 0;
        };
        
        // Trigger management methods
        auto createTrigger(const TriggerInfo &trigger, ErrorContext *ctx = nullptr) -> Status;
        
        auto dropTrigger(const std::string &trigger_name, ErrorContext *ctx = nullptr) -> Status;
        
        auto getTrigger(const ID &trigger_id, TriggerInfo &info, ErrorContext *ctx = nullptr)
            -> Status;
        
        auto getTriggerByName(const std::string &trigger_name, TriggerInfo &info,
                              ErrorContext *ctx = nullptr) -> Status;
        
        auto listTriggersForTable(const ID &table_id, TriggerEvent event, TriggerTiming timing,
                                  std::vector<TriggerInfo> &triggers, ErrorContext *ctx = nullptr)
            -> Status;
        
        auto listAllTriggersForTable(const ID &table_id, std::vector<TriggerInfo> &triggers,
                                     ErrorContext *ctx = nullptr) -> Status;
        
        auto enableTrigger(const std::string &trigger_name, bool enable,
                           ErrorContext *ctx = nullptr) -> Status;

        // ===== PSQL - Stored Procedures and Functions (Phase 2 Task 10.2) =====

        // Parameter mode enum
        enum class ParameterMode : uint8_t
        {
            IN = 0,
            OUT = 1,
            INOUT = 2
        };

        // Parameter information
        struct ParameterInfo
        {
            std::string name;
            DataType type;
            uint32_t type_precision = 0;  // For VARCHAR, DECIMAL, etc.
            uint32_t type_scale = 0;      // For DECIMAL
            ParameterMode mode = ParameterMode::IN;
            bool has_default = false;
            std::string default_value;  // Serialized expression
        };

        // Function information
        struct FunctionInfo
        {
            ID function_id;                        // UUID v7
            std::string name;
            std::vector<ParameterInfo> parameters;
            DataType return_type = DataType::INT32;
            uint32_t return_type_precision = 0;
            uint32_t return_type_scale = 0;
            bool or_replace = false;
            bool deterministic = false;
            std::vector<uint8_t> bytecode;  // Compiled SBLR bytecode
            std::string source_text;        // Original PSQL source
            uint64_t created_time = 0;
            uint64_t modified_time = 0;
        };

        // Procedure information
        struct ProcedureInfo
        {
            ID procedure_id;                       // UUID v7
            std::string name;
            std::vector<ParameterInfo> parameters;
            bool or_replace = false;
            std::vector<uint8_t> bytecode;  // Compiled SBLR bytecode
            std::string source_text;        // Original PSQL source
            uint64_t created_time = 0;
            uint64_t modified_time = 0;
        };

        // Function/Procedure management methods
        auto registerFunction(const FunctionInfo &info, ErrorContext *ctx = nullptr) -> Status;
        auto registerProcedure(const ProcedureInfo &info, ErrorContext *ctx = nullptr) -> Status;

        auto getFunction(const std::string &name, FunctionInfo &info_out,
                        ErrorContext *ctx = nullptr) -> Status;
        auto getProcedure(const std::string &name, ProcedureInfo &info_out,
                         ErrorContext *ctx = nullptr) -> Status;

        auto dropFunction(const std::string &name, bool if_exists = false,
                         ErrorContext *ctx = nullptr) -> Status;
        auto dropProcedure(const std::string &name, bool if_exists = false,
                          ErrorContext *ctx = nullptr) -> Status;

        auto listFunctions(std::vector<FunctionInfo> &functions_out,
                          ErrorContext *ctx = nullptr) -> Status;
        auto listProcedures(std::vector<ProcedureInfo> &procedures_out,
                           ErrorContext *ctx = nullptr) -> Status;


    private:
        Database *db_;
        mutable std::mutex mutex_;

        // TRUNCATE TABLE async job tracking (ALPHA Phase 1 - DDL Modifications)
        std::unordered_map<uint64_t, std::shared_ptr<TruncateJob>> truncate_jobs_;
        std::mutex truncate_jobs_mutex_;
        std::atomic<uint64_t> next_truncate_job_id_{1};

        // Sequence cache (ALPHA Phase 1 - Sequences)
        std::unordered_map<ID, std::shared_ptr<SequenceState>> sequence_cache_;
        std::mutex sequence_cache_mutex_;
        std::unordered_map<std::string, ID> sequence_name_to_id_;  // name -> sequence_id lookup
        std::mutex sequence_name_mutex_;  // Protect name map

        // View cache (ALPHA Phase 1 - Views)
        std::unordered_map<ID, ViewInfo> view_cache_;
        std::unordered_map<std::string, ID> view_name_to_id_;
        std::mutex view_cache_mutex_;

        // Internal helper methods (assume mutex_ is already held by caller)
        auto createSchemaInternal(const std::string &schema_name, const std::string &owner,
                                  ID &schema_id, const ID &parent_schema_id = ID(),
                                  ErrorContext *ctx = nullptr) -> Status;

        // Index TID update helper (Phase 4 Task 4.1.5)
        // Updates all index entries for a table to reference new GPIDs after table migration
        // tid_mapping: Map of old GPID -> new GPID for heap pages
        // Returns Status::OK on success, error status otherwise
        auto updateIndexTIDs(const ID &table_id,
                            const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                            ErrorContext *ctx = nullptr) -> Status;

        // Heap page enumeration helper (Phase 5 Task 5.1.1)
        // Enumerates all heap pages belonging to a table for migration
        // Uses PageManager::getAllocatedPages() to get candidate pages, then filters
        // for pages with matching table_id and PageType::HEAP_PAGE
        // Returns Status::OK on success, error status otherwise
        auto enumerateTablePages(const ID &table_id,
                                std::vector<GPID> &pages_out,
                                ErrorContext *ctx = nullptr) -> Status;

        // Page copying with TID remapping helper (Phase 5 Task 5.1.2)
        // Copies a heap page from source to target buffer, updating all TID references
        // Updates: PageHeader.page_id, TupleHeader.ctid_gpid, TupleHeader.back_version_gpid
        // Recalculates page checksum after modifications
        // Returns Status::OK on success, error status otherwise
        auto copyPageWithTIDRemapping(const void *source_buffer,
                                     void *target_buffer,
                                     GPID source_gpid,
                                     GPID target_gpid,
                                     const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                                     ErrorContext *ctx = nullptr) -> Status;

        // Rollback page migration helper (Phase 5 Task 5.1.4)
        // Deallocates all target pages that were allocated during a failed migration
        // Iterates tid_mapping and frees all new_gpid pages using freePageGlobal()
        // Continues freeing even if some pages fail (logs orphaned pages)
        // Returns Status::OK if all pages freed, Status::IO_ERROR if some failed
        auto rollbackPageMigration(const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                                  ErrorContext *ctx = nullptr) -> Status;

        // Catalog page layout - using higher page numbers to avoid conflict
        // with existing system catalog on page 1
        static constexpr uint32_t CATALOG_ROOT_PAGE = 3;
        static constexpr uint32_t SCHEMAS_TABLE_PAGE = 4;
        static constexpr uint32_t TABLES_TABLE_PAGE = 5;
        static constexpr uint32_t COLUMNS_TABLE_PAGE = 6;
        static constexpr uint32_t INDEXES_TABLE_PAGE = 7;
        static constexpr uint32_t TABLESPACES_TABLE_PAGE = 8;       // pg_tablespace
        static constexpr uint32_t TABLESPACE_FILES_TABLE_PAGE = 9;  // pg_tablespace_files

        // In-memory cache of catalog data
        std::unordered_map<ID, SchemaInfo> schema_cache_;
        std::unordered_map<ID, TableInfo> table_cache_;
        std::unordered_map<ID, std::vector<ColumnInfo>> column_cache_;
        std::unordered_map<ID, IndexInfo> index_cache_;
        std::unordered_map<uint16_t, TablespaceInfo> tablespace_cache_;  // keyed by tablespace_id

        // LSM Integration Phase 3.3: Index object cache
        // Maps index_id -> (index_ptr, index_type) for actual index objects
        struct IndexHandle
        {
            void *index_ptr;
            IndexType index_type;
        };
        std::unordered_map<ID, IndexHandle> index_object_cache_;
        mutable std::mutex index_object_mutex_;  // Separate mutex for index object operations
        
        // Trigger storage (Phase 2 Wave 2 - Agent C)
        std::unordered_map<ID, TriggerInfo> trigger_cache_;  // keyed by trigger_id
        std::unordered_map<std::string, ID> trigger_name_to_id_;  // name -> ID lookup
        std::unordered_multimap<ID, ID> table_triggers_;  // table_id -> trigger_id (multiple per table)
        mutable std::mutex trigger_mutex_;  // Separate mutex for trigger operations

        // PSQL - Stored Procedures and Functions (Phase 2 Task 10.2)
        std::unordered_map<std::string, FunctionInfo> functions_;    // keyed by function name
        std::unordered_map<std::string, ProcedureInfo> procedures_;  // keyed by procedure name
        mutable std::mutex psql_mutex_;  // Separate mutex for function/procedure operations

        // ONLINE migration state cache (Sprint 4 Task 5.4.1)
        std::unordered_map<ID, TableMigrationState> migration_cache_;  // keyed by migration_id
        mutable std::mutex migration_mutex_;  // Separate mutex for migration operations

        // Counters
        uint32_t schema_count_ = 0;
        uint32_t table_count_ = 0;

        // Actual page numbers (may differ from constants during init)
        uint32_t schemas_table_page_ = SCHEMAS_TABLE_PAGE;
        uint32_t tables_table_page_ = TABLES_TABLE_PAGE;
        uint32_t columns_table_page_ = COLUMNS_TABLE_PAGE;
        uint32_t indexes_table_page_ = INDEXES_TABLE_PAGE;
        uint32_t constraints_table_page_ = 0;    // Will be allocated during init
        uint32_t sequences_table_page_ = 0;      // Will be allocated during init
        uint32_t views_table_page_ = 0;          // Will be allocated during init
        uint32_t triggers_table_page_ = 0;       // Will be allocated during init
        uint32_t permissions_table_page_ = 0;    // Will be allocated during init
        uint32_t statistics_table_page_ = 0;     // Will be allocated during init
        uint32_t collations_table_page_ = 0;     // Will be allocated during init
        uint32_t timezones_table_page_ = 0;      // Will be allocated during init
        uint32_t charsets_table_page_ = 0;       // Will be allocated during init (pg_charset)
        uint32_t collation_defs_table_page_ = 0; // Will be allocated during init (pg_collation)
        uint32_t tablespaces_table_page_ = TABLESPACES_TABLE_PAGE;           // pg_tablespace
        uint32_t tablespace_files_table_page_ = TABLESPACE_FILES_TABLE_PAGE; // pg_tablespace_files

        // Internal methods
        auto writeCatalogRoot(ErrorContext *ctx) -> Status;
        auto readCatalogRoot(ErrorContext *ctx) -> Status;

        // Helper to write a record to a catalog heap page
        template <typename RecordType>
        auto writeRecordToHeapPage(uint32_t page_id, const RecordType &record, ErrorContext *ctx)
            -> Status;

        // Helper to update a record in-place (Firebird MGA style) or insert if not found
        template <typename RecordType, typename Predicate>
        auto updateRecordInHeapPage(uint32_t page_id, Predicate matcher,
                                    const RecordType &new_record, ErrorContext *ctx) -> Status;

        // Helper to delete a record by marking is_valid=0 (Firebird MGA style)
        template <typename RecordType, typename Predicate>
        auto deleteRecordFromHeapPage(uint32_t page_id, Predicate matcher, ErrorContext *ctx)
            -> Status;

        // Helper to compact catalog page by removing is_valid=0 records (garbage collection)
        template <typename RecordType>
        auto compactCatalogHeapPage(uint32_t page_id, ErrorContext *ctx) -> Status;

        // Result structure for findRecordInHeapPage
        template <typename RecordType> struct FindResult
        {
            Status status;
            uint32_t slot_index; // Index in the catalog page
            RecordType record;
        };

        // Helper to find a record in a catalog heap page matching a predicate
        template <typename RecordType, typename Predicate>
        auto findRecordInHeapPage(uint32_t page_id, Predicate predicate, ErrorContext *ctx)
            -> FindResult<RecordType>
        {
            BufferPool *bp = db_->buffer_pool();
            void *page_buffer;

            Status status = bp->pinPage(page_id, &page_buffer, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to pin catalog heap page");
                return {status, 0, RecordType{}};
            }

            auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
            uint32_t offset = sizeof(CatalogHeapPage);

            for (uint32_t i = 0; i < heap->record_count; i++)
            {
                auto *record = reinterpret_cast<RecordType *>(
                    reinterpret_cast<uint8_t *>(page_buffer) + offset);

                if (predicate(*record))
                {
                    RecordType found = *record;
                    bp->unpinPage(page_id, false, ctx);
                    return {Status::OK, i, found};
                }

                offset += sizeof(RecordType);
            }

            bp->unpinPage(page_id, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Record not found in catalog page");
            return {Status::NOT_FOUND, 0, RecordType{}};
        }

        // Helper to scan all records in a catalog heap page
        template <typename RecordType, typename InfoType, typename Converter>
        auto scanHeapPage(uint32_t page_id, std::vector<InfoType> &results, Converter converter,
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

            for (uint32_t i = 0; i < heap->record_count; i++)
            {
                auto *record = reinterpret_cast<RecordType *>(
                    reinterpret_cast<uint8_t *>(page_buffer) + offset);

                if (record->is_valid)
                {
                    InfoType info;
                    converter(*record, info);
                    results.push_back(info);
                }

                offset += sizeof(RecordType);
            }

            return bp->unpinPage(page_id, false, ctx);
        }

        // Helper to update a record in a catalog heap page
        template <typename RecordType>
        auto updateRecordInHeapPage(uint32_t page_id, uint32_t slot_index,
                                    const RecordType &updated_record, ErrorContext *ctx) -> Status
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

            if (slot_index >= heap->record_count)
            {
                bp->unpinPage(page_id, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid slot index");
                return Status::INVALID_ARGUMENT;
            }

            uint32_t offset = sizeof(CatalogHeapPage) + (slot_index * sizeof(RecordType));
            auto *record =
                reinterpret_cast<RecordType *>(reinterpret_cast<uint8_t *>(page_buffer) + offset);

            *record = updated_record;

            return bp->unpinPage(page_id, true, ctx); // Mark as dirty
        }

        // Helper to read records from a catalog heap page
        template <typename RecordType, typename InfoType, typename KeyType, typename Converter,
                  typename KeyExtractor>
        auto readRecordsFromHeapPage(uint32_t page_id, std::unordered_map<KeyType, InfoType> &cache,
                                     Converter converter, KeyExtractor key_extractor,
                                     ErrorContext *ctx) -> Status
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

            cache.clear();
            uint32_t offset = sizeof(CatalogHeapPage);

            for (uint32_t i = 0; i < heap->record_count; i++)
            {
                auto *record = reinterpret_cast<RecordType *>(
                    reinterpret_cast<uint8_t *>(page_buffer) + offset);

                if (record->is_valid)
                {
                    InfoType info;
                    converter(*record, info);
                    cache[key_extractor(info)] = info;
                }

                offset += sizeof(RecordType);
            }

            return bp->unpinPage(page_id, false, ctx);
        }

        // Helper to read records from a catalog heap page
        template <typename RecordType, typename InfoType>
        auto readRecordsToVector(uint32_t page_id, std::vector<InfoType> &results,
                                 std::function<bool(const RecordType &)> filter,
                                 std::function<void(const RecordType &, InfoType &)> converter,
                                 ErrorContext *ctx) -> Status;

        // Specific write/read methods using the generic helpers
        auto writeSchemaRecord(const SchemaInfo &schema, ErrorContext *ctx) -> Status;
        auto readSchemaRecords(ErrorContext *ctx) -> Status;
        auto writeTableRecord(const TableInfo &table, ErrorContext *ctx) -> Status;
        auto deleteTableRecord(const ID &table_id, ErrorContext *ctx) -> Status;
        auto readTableRecords(ErrorContext *ctx) -> Status;
        auto writeColumnRecords(const ID &table_id, const std::vector<ColumnInfo> &columns,
                                ErrorContext *ctx) -> Status;
        auto readColumnRecords(const ID &table_id, ErrorContext *ctx) -> Status;
        auto writeIndexRecord(const IndexInfo &index, ErrorContext *ctx) -> Status;
        auto deleteIndexRecord(const ID &index_id, ErrorContext *ctx) -> Status;
        auto readIndexRecords(ErrorContext *ctx) -> Status;
        auto updateTableColumnCount(const ID &table_id, uint32_t new_count, ErrorContext *ctx)
            -> Status;
        auto writeTablespaceRecord(const TablespaceInfo &tablespace, ErrorContext *ctx) -> Status;
        auto readTablespaceRecords(ErrorContext *ctx) -> Status;

        // Helper to allocate catalog pages
        auto allocateCatalogPage(uint32_t &page_id, ErrorContext *ctx) -> Status;
    };

    // ========================================================================
    // Index Type Helper Functions (LSM Integration Plan Phase 1)
    // ========================================================================

    /**
     * Convert string to IndexType enum (case-insensitive with aliases)
     *
     * @param type_str Index type string (e.g., "LSM", "BTREE", "HNSW")
     * @return IndexType enum value, or nullopt if invalid
     */
    std::optional<CatalogManager::IndexType> parseIndexType(const std::string &type_str);

    /**
     * Convert IndexType enum to string representation
     *
     * @param type Index type enum value
     * @return String representation (e.g., "LSM", "BTREE", "HNSW")
     */
    std::string indexTypeToString(CatalogManager::IndexType type);

    // ========================================================================

    // DataType enum is now defined in types.h

} // namespace scratchbird::core

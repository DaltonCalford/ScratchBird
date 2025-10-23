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
     */
    class CatalogManager
    {
    public:
        // Schema information
        struct SchemaInfo
        {
            ID schema_id;
            std::string schema_name;
            std::string owner;
            uint16_t default_tablespace_id = 0; // Default tablespace for new tables
            uint16_t permissions = 0;           // Bitmask of schema permissions
            uint16_t default_charset = 0;       // Default character set (0 = inherit from database)
            uint16_t reserved = 0;
            uint32_t default_collation_id = 0; // Default collation ID (0 = inherit from database)
            uint32_t acl_oid = 0;              // TOAST reference for ACL (access control list)
            uint32_t search_path_oid = 0;      // TOAST reference for search path
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
            uint32_t root_page = 0; // Root page of table data
            uint32_t column_count = 0;
            uint64_t row_count = 0; // Estimated row count
            TableType table_type = TableType::HEAP;
            bool has_toast = false;
            ID toast_table_id;                 // PHASE 5 TASK 5.1.3.1: UUID of TOAST table (zero if none)
            uint16_t tablespace_id = 0;        // Tablespace ID (0 = default)
            uint16_t default_charset = 0;      // Default character set (0 = inherit from schema)
            uint32_t default_collation_id = 0; // Default collation ID (0 = inherit from schema)
            uint32_t storage_params_oid = 0;   // TOAST reference for storage parameters
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;

            // ONLINE migration fields (Sprint 4 Task 5.4.1)
            bool migration_in_progress = false;     // True if table is being migrated
            ID migration_id;                        // Current migration ID
            uint64_t migration_xid = 0;             // XID when migration started
            uint16_t migration_target_ts = 0;       // Target tablespace ID
            uint8_t migration_phase = 0;            // MigrationPhase enum value
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
            BTREE = 0,    // B-tree index (default)
            HASH = 1,     // Hash index
            VECTOR = 2,   // Vector similarity index (HNSW, IVF, etc.)
            FULLTEXT = 3, // Full-text search index
            GIN = 4,      // Generalized Inverted Index
            GIST = 5,     // Generalized Search Tree
            BRIN = 6      // Block Range Index
        };

        // Index information
        struct IndexInfo
        {
            ID index_id;
            ID table_id;
            std::string index_name;
            uint32_t root_page = 0;
            uint16_t tablespace_id = 0;    // Tablespace ID (0 = primary file, 1-65535 = custom)
            IndexType index_type = IndexType::BTREE;
            bool is_unique = false;
            std::vector<ID> column_ids;
            uint32_t index_params_oid = 0; // TOAST reference for index parameters
            uint64_t created_time = 0;
            uint32_t collation_id = 101; // Default: utf8_general_ci (binary comparison)
                                         // TODO(Issue #50): Full integration of collation-aware
                                         // comparisons throughout B-tree and query evaluation
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

        auto getIndex(const ID &index_id, IndexInfo &info, ErrorContext *ctx = nullptr) -> Status;

        auto getIndex(const ID &table_id, const std::string &index_name, IndexInfo &info,
                      ErrorContext *ctx = nullptr) -> Status;

        auto listIndexesForTable(const ID &table_id, std::vector<IndexInfo> &indexes,
                                 ErrorContext *ctx = nullptr) -> Status;

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

    private:
        Database *db_;
        mutable std::mutex mutex_;

        // Internal helper methods (assume mutex_ is already held by caller)
        auto createSchemaInternal(const std::string &schema_name, const std::string &owner,
                                  ID &schema_id, ErrorContext *ctx = nullptr) -> Status;

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
        auto readIndexRecords(ErrorContext *ctx) -> Status;
        auto writeTablespaceRecord(const TablespaceInfo &tablespace, ErrorContext *ctx) -> Status;
        auto readTablespaceRecords(ErrorContext *ctx) -> Status;

        // Helper to allocate catalog pages
        auto allocateCatalogPage(uint32_t &page_id, ErrorContext *ctx) -> Status;
    };

    // DataType enum is now defined in types.h

} // namespace scratchbird::core

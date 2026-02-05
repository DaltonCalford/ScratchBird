/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <map>
#include <optional>
#include <deque>
#include <cstring>  // for std::memcpy
#include <array>
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/tablespace.h"

namespace scratchbird::core
{

    // Forward declarations
    class PageManager;
    class TIDResolver;
    class ToastManager;
    struct DomainInfo;
    struct LockSnapshot;
    struct AuditEvent;
    struct AuditQuery;

    using ID = UuidV7Bytes;

    // P1-9: Hash function for std::pair (for constraint name lookup)
    template<typename T1, typename T2>
    struct PairHash
    {
        std::size_t operator()(const std::pair<T1, T2>& p) const
        {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            // Combine hashes using a standard method
            return h1 ^ (h2 << 1);
        }
    };

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
     * IdentifierUtils - SQL identifier comparison utilities (Firebird-style)
     *
     * Firebird SQL identifier rules:
     * - Unquoted identifiers: case-insensitive, compared UPPER() to UPPER()
     * - Quoted identifiers ("Name"): case-sensitive, compared as-is
     *
     * For name conflict detection, we compare UPPER() to UPPER() unless
     * both identifiers are delimited (quoted), in which case we compare as-is.
     */
    namespace IdentifierUtils
    {
        // Convert string to uppercase (ASCII-only, sufficient for SQL identifiers)
        inline std::string toUpper(const std::string& str)
        {
            std::string result = str;
            for (char& c : result) {
                if (c >= 'a' && c <= 'z') {
                    c = static_cast<char>(c - 32);
                }
            }
            return result;
        }

        // Compare two SQL identifiers for conflict detection
        // Returns true if names conflict (would be treated as same object)
        // Rules:
        // - If BOTH are delimited: exact case-sensitive comparison
        // - Otherwise: case-insensitive (UPPER vs UPPER) comparison
        inline bool namesConflict(const std::string& name1, bool delimited1,
                                  const std::string& name2, bool delimited2)
        {
            if (delimited1 && delimited2) {
                // Both are case-sensitive: exact match required for conflict
                return name1 == name2;
            }
            // At least one is case-insensitive: compare UPPER to UPPER
            return toUpper(name1) == toUpper(name2);
        }

        // Compare a search name against a stored name for lookup
        // Returns true if names match for lookup purposes
        // Rules:
        // - If stored is delimited: search must match exactly
        // - If stored is not delimited: case-insensitive lookup
        inline bool namesMatch(const std::string& search_name, bool search_delimited,
                               const std::string& stored_name, bool stored_delimited)
        {
            if (stored_delimited) {
                // Stored is case-sensitive: must match exactly
                return search_name == stored_name;
            }
            // Stored is case-insensitive: compare UPPER to UPPER
            return toUpper(search_name) == toUpper(stored_name);
        }
    }

    // Schema/object path resolution (core)
    enum class PathType : uint8_t
    {
        UNQUALIFIED = 0,
        CURRENT = 1,
        PARENT = 2,
        ABSOLUTE = 3
    };

    struct ObjectPath
    {
        PathType type = PathType::UNQUALIFIED;
        bool no_search_path = false;  // True when !: disables search path
        std::vector<std::string> components;
    };

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
     * MigrationHistoryInfo - Persisted record of completed table migrations
     *
     * WP-2 CAT-L2: Migration history persistence
     *
     * This structure records completed migrations for audit and diagnostics.
     * Unlike TableMigrationState (in-memory only), this is persisted to disk.
     */
    struct MigrationHistoryInfo
    {
        ID history_id;              // Unique history record ID
        ID migration_id;            // Original migration ID
        ID table_id;                // Table that was migrated
        uint16_t source_tablespace; // Source tablespace ID
        uint16_t target_tablespace; // Target tablespace ID
        MigrationPhase final_phase; // Final phase (COMPLETE, FAILED, ABORTED)
        uint64_t migration_xid;     // XID when migration started
        uint32_t total_pages;       // Total pages migrated
        uint32_t pages_copied;      // Pages actually copied
        uint64_t start_time;        // Timestamp when migration started
        uint64_t end_time;          // Timestamp when migration completed/failed
        uint32_t catch_up_iterations; // Number of catch-up iterations
        uint64_t total_bytes_copied;  // Total bytes copied
        uint8_t is_valid;           // MGA: 1 = valid, 0 = deleted
        uint8_t padding[7];         // Alignment padding
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

    // Simple heap page for catalog tables (supports overflow pages)
    struct CatalogHeapPage
    {
        PageHeader header;
        uint32_t record_count;
        uint32_t free_offset;
        uint32_t next_page;     // Next page in chain (0 = no more pages)
        uint32_t reserved;      // Alignment padding
        uint8_t data[];         // Variable length records
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
        using CatalogMutex = std::recursive_mutex;

        // Schema types for hierarchical namespace (Phase B - Schema Architecture)
        enum class SchemaType : uint8_t
        {
            SYSTEM = 0,          // /sys/* - System management schemas
            USER_HOME = 1,       // /users/{username}/* - User home directories
            REMOTE_NATIVE = 2,   // /remote/scratchbird/* - Remote ScratchBird mounts
            REMOTE_EMULATED = 3, // /remote/emulated/* - Emulated foreign servers
            PUBLIC = 4,          // /public - Default public schema
            APPLICATION = 5      // User-created application schemas
        };

        // Schema information
        struct SchemaInfo
        {
            ID schema_id;
            ID parent_schema_id;                // Parent schema UUID (zero UUID for root)
            std::string schema_name;            // Short name (not full path)
            bool name_is_delimited = false;     // True if name was double-quoted (case-sensitive)
            std::string full_path;              // Cached full dotted path (e.g., "emulation.firebird")
            SchemaType schema_type = SchemaType::APPLICATION;
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

        enum class TempMetadataScope : uint8_t
        {
            NONE = 0,
            GLOBAL = 1,
            SESSION = 2
        };

        enum class TempDataScope : uint8_t
        {
            NONE = 0,
            SESSION = 1,
            TRANSACTION = 2
        };

        enum class TempOnCommitAction : uint8_t
        {
            NONE = 0,
            DELETE_ROWS = 1,
            PRESERVE_ROWS = 2,
            DROP = 3
        };

        // Table information
        struct TableInfo
        {
            ID table_id;
            ID schema_id;
            std::string table_name;
            bool name_is_delimited = false;    // True if name was double-quoted (case-sensitive)
            ID owner_id;                       // Owner UUID reference (NOT name)
            GPID root_gpid = 0;                // Root page of table data (GPID)
            uint32_t column_count = 0;
            uint64_t row_count = 0;            // Estimated row count
            TableType table_type = TableType::HEAP;
            TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
            TempDataScope temp_data_scope = TempDataScope::NONE;
            TempOnCommitAction temp_on_commit = TempOnCommitAction::NONE;
            ID creating_session_id{};          // Session UUID for session-scoped temp metadata
            uint64_t creating_transaction_id = 0;
            ID temp_parent_table_id{};         // Internal temp instance parent table (optional)
            ID temp_schema_id{};               // Session-local temp schema (optional)
            bool has_toast = false;
            ID toast_table_id;                 // PHASE 5 TASK 5.1.3.1: UUID of TOAST table (zero if none)
            uint16_t tablespace_id = 0;        // Tablespace ID (0 = default)
            uint16_t default_charset = 0;      // Default character set (0 = inherit from schema)
            uint32_t default_collation_id = 0; // Default collation ID (0 = inherit from schema)
            uint32_t storage_params_oid = 0;   // TOAST reference for storage parameters - IMPLEMENTED
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            uint64_t policy_epoch = 0;            // Security policy epoch (Plan 03)

            // ONLINE migration fields (Sprint 4 Task 5.4.1)
            bool migration_in_progress = false;     // True if table is being migrated
            ID migration_id;                        // Current migration ID
            uint64_t migration_xid = 0;             // XID when migration started
            uint16_t migration_target_ts = 0;       // Target tablespace ID
            uint8_t migration_phase = 0;            // MigrationPhase enum value

            // Security Phase 3.4: Row-level security settings
            bool rls_enabled = false;               // Row-level security enabled
            bool rls_forced = false;                // Force RLS for table owners
        };

        struct TableCreateOptions
        {
            TableType table_type = TableType::HEAP;
            TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
            TempDataScope temp_data_scope = TempDataScope::NONE;
            TempOnCommitAction temp_on_commit = TempOnCommitAction::NONE;
            ID creating_session_id{};
            uint64_t creating_transaction_id = 0;
            ID temp_parent_table_id{};
            ID temp_schema_id{};
            bool force_table_id = false;
            ID forced_table_id{};
        };

        struct TempObjectOptions
        {
            TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
            ID creating_session_id{};
            uint64_t creating_transaction_id = 0;
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
            bool name_is_delimited = false;    // True if name was double-quoted (case-sensitive)
            ID owner_id;
            ID owned_by_table_id{};
            ID owned_by_column_id{};
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
            ID creating_session_id{};
            uint64_t creating_transaction_id = 0;
        };

        // In-memory sequence state for atomic operations
        struct SequenceState {
            ID sequence_id;
            ID schema_id;  // WP-2 CAT-M1: Track schema for cascade drop
            std::string name;  // Sequence name (for cleanup in drop)
            bool name_is_delimited = false;  // True if name was double-quoted (case-sensitive)
            ID owner_id;  // Owner UUID reference
            ID owned_by_table_id{};
            ID owned_by_column_id{};
            std::atomic<int64_t> current_value;
            int64_t increment_by;
            int64_t min_value;
            int64_t max_value;
            int64_t start_value = 0;
            int64_t cache_size = 1;
            bool cycle;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
            ID creating_session_id{};
            uint64_t creating_transaction_id = 0;
            std::mutex config_mutex;  // Protect ALTER SEQUENCE changes
        };

        // P2-18: Materialized view refresh strategy
        enum class MVRefreshStrategy : uint8_t {
            COMPLETE = 0,       // Full refresh - truncate and repopulate (default)
            INCREMENTAL = 1,    // Incremental refresh - only changed rows
            FAST = 2            // Fast refresh using change log
        };

        // View information (ALPHA Phase 1 - Views)
        struct ViewInfo {
            ID view_id;
            ID schema_id;
            std::string name;
            bool name_is_delimited = false;    // True if name was double-quoted (case-sensitive)
            ID owner_id;
            std::string definition;  // SELECT query text
            bool check_option;
            bool security_definer = false;    // SECURITY DEFINER view
            bool security_barrier = false;    // SECURITY BARRIER view
            std::vector<std::string> column_names;  // Optional explicit columns
            uint64_t created_time;
            uint64_t last_modified_time;
            TempMetadataScope temp_metadata_scope = TempMetadataScope::NONE;
            ID creating_session_id{};
            uint64_t creating_transaction_id = 0;

            // ALPHA Phase 1 - Materialized Views
            bool materialized;              // True if this is a materialized view
            ID materialized_table_id;       // Physical table storing the materialized data (if materialized)
            uint64_t last_refresh_time;     // Timestamp of last REFRESH (0 if never refreshed)

            // P2-18: Advanced refresh options
            MVRefreshStrategy refresh_strategy;  // How to refresh this MV
            bool refresh_on_commit;          // Refresh automatically on source table changes
            std::vector<ID> base_table_ids;  // Tables this MV depends on (for incremental refresh)
            ID change_log_table_id;          // Table tracking changes for fast refresh
            bool supports_concurrent;        // Can be refreshed concurrently

            ViewInfo() : check_option(false), materialized(false), last_refresh_time(0),
                         refresh_strategy(MVRefreshStrategy::COMPLETE), refresh_on_commit(false),
                         supports_concurrent(true) {}
        };

        // Generated column types (ALPHA Phase 1 - Constraint Features)
        enum class GeneratedColumnType : uint8_t
        {
            NOT_GENERATED = 0,  // Regular column
            STORED = 1,         // GENERATED ALWAYS AS ... STORED
            VIRTUAL = 2         // GENERATED ALWAYS AS ... VIRTUAL
        };

        // Column information
        struct ColumnInfo
        {
            ID table_id;
            ID column_id;
            std::string column_name;
            bool name_is_delimited = false;    // True if name was double-quoted (case-sensitive)
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

            // GENERATED column fields (ALPHA Phase 1 - Constraint Features)
            GeneratedColumnType generated_type = GeneratedColumnType::NOT_GENERATED;
            std::string generation_expression;  // SQL expression (or serialized bytecode)
            uint32_t generation_expr_oid = 0;   // TOAST reference for large expressions
            std::vector<uint16_t> dependent_columns;  // Column ordinals this depends on

            // IDENTITY column fields (ALPHA Phase 1 - Constraint Features)
            bool is_identity = false;           // Is this an IDENTITY column?
            bool identity_always = true;        // true=ALWAYS (cannot override), false=BY DEFAULT (can override)
            ID identity_sequence_id;            // Associated sequence ID (zero if not identity)

            uint8_t storage_type = 0;       // TOAST storage strategy
            bool with_timezone = false;     // For TIMESTAMP: WITH TIME ZONE
            uint16_t charset = 0;           // Character set (0 = inherit from table)
            ID domain_id;                   // WP-2 CAT-M7: Domain ID (zero if not domain-based)
            bool is_array = false;          // Array column flag (true when column stores array values)
            uint32_t array_size = 0;        // Fixed array size (0 = unspecified/unbounded)
            uint16_t timezone_hint = 0;     // Timezone ID for display (0 = use connection default)
            uint32_t collation_id = 0;      // Collation ID (0 = inherit from table)
            std::string default_value;      // Serialized default (simple literals)
            std::string default_expr;       // DEFAULT expression (hex bytecode, ALPHA Phase A)
            uint32_t default_value_oid = 0; // TOAST reference for large defaults
            std::string check_expr;         // CHECK constraint expression (hex bytecode)
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
            LSM = 11,         // LSM-Tree (Log-Structured Merge-Tree)
            IVF = 12,         // IVF (Inverted File) vector index
            ZONEMAP = 13      // Zone map (min/max) index
        };

        // Plan 01 Task E: Index states for shadow rebuild + versioning
        enum class IndexState : uint8_t
        {
            BUILDING = 0,   // Index is being built (not yet visible to queries)
            ACTIVE = 1,     // Index is active and available for use
            RETIRED = 2,    // Index is retired (old version after rebuild)
            FAILED = 3,     // Index build failed
            INACTIVE = 4    // Index disabled via ALTER INDEX
        };

        // Index information
        struct IndexInfo
        {
            ID index_id;
            ID table_id;
            std::string index_name;
            bool name_is_delimited = false;    // True if name was double-quoted (case-sensitive)
            ID owner_id;                   // Owner UUID reference (NOT name)
            GPID root_gpid = 0;            // Root page of index (GPID)
            uint16_t tablespace_id = 0;    // Tablespace ID (0 = primary file, 1-65535 = custom)
            IndexType index_type = IndexType::BTREE;
            bool is_unique = false;
            std::vector<ID> column_ids;
            std::vector<ID> include_column_ids;
            uint32_t index_params_oid = 0; // TOAST reference for index parameters - IMPLEMENTED
            uint64_t created_time = 0;
            uint32_t collation_id = 101; // Default: utf8_general_ci (binary comparison)
                                         // Collation-aware comparisons are handled by CharsetManager

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

            // Phase 2: Dependency tracking ID for cleanup
            ID dependency_id;                      // Dependency: index → table (AUTO)

            // Plan 01 Task E: Shadow index rebuild + versioning
            ID logical_index_id;                   // Stable logical index UUID across rebuilds
            uint8_t state = 1;                     // 0=BUILDING, 1=ACTIVE, 2=RETIRED, 3=FAILED, 4=INACTIVE (default ACTIVE)
            uint64_t valid_from_xid = 0;           // XID when new txns can use this index (0 = immediately)
            uint64_t retired_xid = 0;              // XID after which no new txns use this index (0 = not retired)
            uint64_t build_started_time = 0;
            uint64_t build_completed_time = 0;
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
            EXCEPTION = 24,     // Firebird-style exceptions
            COMMENT = 25,
            DEPENDENCY = 26,
            PERMISSION = 27,
            STATISTIC = 28,
            TIMEZONE = 29,
            EXTENSION = 30,
            FOREIGN_SERVER = 31,
            FOREIGN_TABLE = 32,
            USER_MAPPING = 33,      // Phase B: FDW user mapping
            SERVER_REGISTRY = 34,   // Phase B: Distributed MVCC server registry
            UDR_ENGINE = 35,        // Phase B: UDR engine plugin
            UDR_MODULE = 36,        // Phase B: UDR module
            CLUSTER = 37,           // Phase B: Distributed MVCC cluster
            SYNONYM = 38,           // Phase B: Cross-schema pointer/alias
            POLICY = 39,            // Row-level security policy
            JOB = 40,               // Scheduler job
            UNKNOWN = 255           // Sentinel for resolver filters/unknown type
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

        // Object definition storage (DDL source + bytecode)
        struct ObjectDefinitionInfo
        {
            ID object_id;
            ObjectType object_type;
            std::string ddl_text;              // Original DDL SQL
            std::vector<uint8_t> bytecode;     // Compiled SBLR bytecode
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Foreign Key referential actions (ALPHA Phase A - FK Constraints)
        enum class FKAction : uint8_t
        {
            NO_ACTION = 0,  // Default: error if references exist
            RESTRICT = 1,   // Error immediately if references exist
            CASCADE = 2,    // Delete/update child rows
            SET_NULL = 3,   // Set FK columns to NULL
            SET_DEFAULT = 4 // Set FK columns to DEFAULT
        };

        // Foreign Key match types (ALPHA Phase A - FK Constraints)
        enum class FKMatchType : uint8_t
        {
            SIMPLE = 0,   // Default: NULL in any column = no match required
            FULL = 1,     // All columns NULL or all non-NULL
            PARTIAL = 2   // Not implemented (reserved)
        };

        // Foreign Key information (ALPHA Phase A - FK Constraints)
        struct ForeignKeyInfo
        {
            ID fk_id;                          // Unique FK constraint ID
            std::string fk_name;               // Constraint name
            ID child_table_id;                 // Table with the FK (referencing table)
            ID parent_table_id;                // Referenced table
            std::vector<std::string> child_columns;  // FK column names in child table
            std::vector<std::string> parent_columns; // Referenced column names in parent
            FKAction on_delete = FKAction::NO_ACTION;      // Action on DELETE of parent row
            FKAction on_update = FKAction::NO_ACTION;      // Action on UPDATE of parent key
            FKMatchType match_type = FKMatchType::SIMPLE;  // Match type (SIMPLE, FULL, PARTIAL)
            bool is_enabled = true;            // Can be disabled temporarily

            // ALPHA Phase 1 - Deferred constraint checking
            bool is_deferrable = false;        // Can constraint checking be deferred?
            bool initially_deferred = false;   // Defer by default in new transactions?

            uint64_t created_time = 0;

            // Phase 2: Dependency tracking IDs for cleanup
            ID child_dependency_id;            // Dependency: FK → child table (AUTO)
            ID parent_dependency_id;           // Dependency: FK → parent table (NORMAL)
        };

        // P1-9: Constraint types for unified constraints table
        enum class ConstraintType : uint8_t
        {
            PRIMARY_KEY = 0,  // PRIMARY KEY constraint
            UNIQUE = 1,       // UNIQUE constraint
            CHECK = 2,        // CHECK constraint
            FOREIGN_KEY = 3,  // FOREIGN KEY constraint
            NOT_NULL = 4,     // NOT NULL constraint (column-level)
            EXCLUSION = 5     // EXCLUSION constraint (PostgreSQL extension)
        };

        // P1-9: Unified constraint information for sb_constraints table
        struct ConstraintInfo
        {
            ID constraint_id;                  // Unique constraint ID
            std::string constraint_name;       // Constraint name (may be system-generated)
            bool name_is_delimited = false;    // True if name was double-quoted (case-sensitive)
            ID table_id;                       // Table this constraint applies to
            ConstraintType constraint_type;    // Type of constraint

            // Column information (for PK, UNIQUE, NOT NULL, CHECK)
            std::vector<std::string> column_names;  // Columns involved in constraint

            // CHECK constraint specific
            std::string check_expression;      // CHECK constraint SQL expression
            uint32_t check_expr_oid = 0;      // TOAST reference for large expressions

            // FOREIGN KEY specific
            ID referenced_table_id;            // For FK: parent table
            std::vector<std::string> referenced_columns;  // For FK: parent columns
            FKAction on_delete = FKAction::NO_ACTION;
            FKAction on_update = FKAction::NO_ACTION;
            FKMatchType match_type = FKMatchType::SIMPLE;

            // EXCLUSION constraint specific (PostgreSQL extension)
            std::string exclusion_operator;    // Operator for exclusion (e.g., "&&", "=")
            std::string index_method;          // Index method (GIST, etc.)

            // Common fields
            bool is_deferrable = false;        // Can constraint be deferred?
            bool initially_deferred = false;   // Defer by default?
            bool is_enabled = true;            // Can be disabled
            bool is_validated = true;          // Has constraint been validated?
            bool is_system_generated = false;  // System-generated name?

            ID owner_id;                       // User who created constraint
            uint64_t created_time = 0;
            uint64_t validated_time = 0;       // When constraint was last validated
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

        // Minimal user info without TOAST access (password/metadata omitted).
        struct BasicUserInfo
        {
            ID user_id;
            std::string username;
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
            ID default_schema_id{};     // Home schema for role
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
            ID default_schema_id{};      // Home schema for group
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // WP-2 CAT-L1: Authentication method for group mappings
        enum class AuthMethod : uint8_t
        {
            LDAP = 1,
            KERBEROS = 2,
            ACTIVE_DIRECTORY = 3
        };

        // WP-2 CAT-L1: Group mapping for external authentication
        struct GroupMappingInfo
        {
            ID mapping_id;
            std::string external_group_name;  // LDAP DN, Kerberos principal, AD SID
            AuthMethod auth_method = AuthMethod::LDAP;
            bool auto_create_users = false;  // Auto-create users on first login
            ID internal_group_id;  // Maps to internal GroupInfo
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Scheduler job types (WS-4 Scheduler)
        enum class JobType : uint8_t
        {
            SQL = 0,
            PROCEDURE = 1,
            EXTERNAL = 2
        };

        enum class JobClass : uint8_t
        {
            UNSPECIFIED = 0,
            LOCAL_SAFE = 1,
            LEADER_ONLY = 2,
            QUORUM_REQUIRED = 3
        };

        enum class ScheduleKind : uint8_t
        {
            CRON = 0,
            AT = 1,
            EVERY = 2
        };

        enum class JobState : uint8_t
        {
            ENABLED = 0,
            DISABLED = 1,
            PAUSED = 2
        };

        enum class JobRunState : uint8_t
        {
            PENDING = 0,
            RUNNING = 1,
            COMPLETED = 2,
            FAILED = 3,
            CANCELLED = 4
        };

        enum class JobOnCompletion : uint8_t
        {
            PRESERVE = 0,
            DROP = 1
        };

        struct JobInfo
        {
            ID job_id;
            std::string job_name;
            std::string description;
            JobClass job_class = JobClass::UNSPECIFIED;
            JobType job_type = JobType::SQL;
            std::string job_sql;
            ID procedure_uuid;
            std::string external_command;
            ScheduleKind schedule_kind = ScheduleKind::CRON;
            std::string cron_expression;
            int64_t interval_seconds = 0;
            uint64_t starts_at = 0;
            uint64_t ends_at = 0;
            std::string schedule_tz;
            uint64_t next_run_time = 0;
            JobOnCompletion on_completion = JobOnCompletion::PRESERVE;
            std::string partition_strategy;
            ID partition_shard_uuid;
            std::string partition_expression;
            uint32_t max_retries = 3;
            uint32_t retry_backoff_seconds = 60;
            uint32_t timeout_seconds = 3600;
            ID created_by_user_uuid;
            ID run_as_role_uuid;
            uint64_t created_at = 0;
            JobState state = JobState::ENABLED;
        };

        struct JobRunInfo
        {
            ID job_run_id;
            ID job_id;
            ID assigned_node_uuid;
            ID shard_uuid;
            uint64_t scheduled_time = 0;
            uint64_t started_at = 0;
            uint64_t completed_at = 0;
            JobRunState state = JobRunState::PENDING;
            uint32_t retry_count = 0;
            std::string result_message;
            std::vector<uint8_t> result_data;
            int64_t rows_affected = 0;
            int32_t error_code = 0;
        };

        struct JobDependencyInfo
        {
            ID job_id;
            ID depends_on_job_id;
            uint64_t created_time = 0;
        };

        struct JobSecretInfo
        {
            ID job_id;
            std::string secret_key;
            std::string secret_value;
            uint64_t created_time = 0;
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

        // Privilege types (Phase 1.4 - Security System)
        enum class Privilege : uint32_t
        {
            // Object privileges (bitmask)
            SELECT    = 0x00000001,  // Read data
            INSERT    = 0x00000002,  // Insert data
            UPDATE    = 0x00000004,  // Update data
            DELETE    = 0x00000008,  // Delete data
            TRUNCATE  = 0x00000010,  // Truncate table
            REFERENCES = 0x00000020, // Create foreign keys
            TRIGGER   = 0x00000040,  // Create triggers

            // Schema privileges
            CREATE    = 0x00000080,  // Create objects in schema
            USAGE     = 0x00000100,  // Use schema

            // Sequence privileges
            SEQUENCE_USAGE = 0x00000200,  // Use sequence
            SEQUENCE_UPDATE = 0x00000400, // Alter sequence

            // Administrative privileges
            EXECUTE   = 0x00000800,  // Execute procedure/function
            CONNECT   = 0x00001000,  // Connect to database
            TEMPORARY = 0x00002000,  // Create temp tables
            COPY_FILE = 0x00004000,  // Server-side COPY to/from files
            CREATE_JOB = 0x00008000, // Create jobs
            VIEW_JOB_HISTORY = 0x00010000, // View job history across users
            EXECUTE_EXTERNAL_JOB = 0x00020000, // Execute external jobs

            // Special privileges
            ALL       = 0xFFFFFFFF   // All privileges
        };

        // Object types for permissions (Phase 1.4 - Security System)
        enum class PermissionObjectType : uint8_t
        {
            SCHEMA = 0,
            TABLE = 1,
            VIEW = 2,
            SEQUENCE = 3,
            PROCEDURE = 4,
            FUNCTION = 5,
            DOMAIN = 6,
            DATABASE = 7,
            JOB = 8
        };

        // Grantee types for permissions (Phase 1.4 - Security System)
        enum class GranteeType : uint8_t
        {
            USER = 0,
            ROLE = 1,
            GROUP = 2,
            PUBLIC = 3  // Special: all users
        };

        // Permission information (Phase 1.4 - Security System)
        struct PermissionInfo
        {
            ID permission_id;
            ID object_id;                    // Object being granted permission on
            PermissionObjectType object_type;
            ID grantee_id;                   // Who receives the permission
            GranteeType grantee_type;
            uint32_t privileges;             // Bitmask of Privilege enum
            bool grant_option = false;       // Can grantee grant to others
            ID grantor_id;                   // Who granted the permission
            uint64_t created_time = 0;
        };

        struct DefaultPrivilegeInfo
        {
            ID default_privilege_id;
            ID schema_id;
            ID grantor_id;
            PermissionObjectType object_type;
            ID grantee_id;
            GranteeType grantee_type;
            uint32_t privileges;
            bool grant_option = false;
            uint64_t created_time = 0;
        };

        // Security Phase 3.3: Column-level permission information
        struct ColumnPermissionInfo
        {
            ID permission_id;
            ID table_id;                     // Table containing the column
            std::string column_name;         // Column being protected
            ID grantee_id;                   // Who receives the permission
            GranteeType grantee_type;
            uint32_t privileges;             // Bitmask of Privilege enum
            bool grant_option = false;       // Can grantee grant to others
            ID grantor_id;                   // Who granted the permission
            uint64_t created_time = 0;
        };

        // Security Phase 3.4: Row-level security policy information
        enum class PolicyType : uint8_t
        {
            ALL = 0,      // Apply to all operations
            SELECT = 1,   // Apply to SELECT operations
            INSERT = 2,   // Apply to INSERT operations
            UPDATE = 3,   // Apply to UPDATE operations
            DELETE = 4    // Apply to DELETE operations
        };

        struct PolicyInfo
        {
            ID policy_id;
            ID table_id;                     // Table this policy applies to
            std::string policy_name;         // Policy name (unique per table)
            PolicyType policy_type;          // Which operations this policy affects
            std::vector<ID> role_ids;        // Role UUIDs this policy applies to (empty = all) - Phase 3 Polish
            std::string using_expr;          // USING clause expression (for visibility)
            std::string with_check_expr;     // WITH CHECK clause expression (for modifications)
            bool is_enabled = true;          // Policy can be temporarily disabled
            uint64_t created_time = 0;
            uint64_t modified_time = 0;
        };

        // Object permission bitmask constants (Phase 3.1 - SQL Object Permissions)
        // Note: ObjectType and GranteeType enums already defined above (lines 424 and 593)
        static constexpr uint32_t PERM_EXECUTE = 0x0001;  // Execute procedure/function
        static constexpr uint32_t PERM_SELECT  = 0x0002;  // Select from view/table
        static constexpr uint32_t PERM_INSERT  = 0x0004;  // Insert into table
        static constexpr uint32_t PERM_UPDATE  = 0x0008;  // Update table
        static constexpr uint32_t PERM_DELETE  = 0x0010;  // Delete from table
        static constexpr uint32_t PERM_USAGE   = 0x0020;  // Use sequence

        struct ObjectPermissionInfo
        {
            ID permission_id;
            ID object_id;                    // Object this permission applies to
            ObjectType object_type;          // Type of object
            ID grantee_id;                   // Who receives the permission
            GranteeType grantee_type;        // Type of grantee
            uint32_t permissions;            // Bitmask of permissions
            bool grant_option = false;       // WITH GRANT OPTION
            ID grantor_id;                   // Who granted the permission
            uint64_t created_time = 0;
        };

        // Session timeout configuration (P1-12: Session Timeout)
        struct SessionTimeoutConfig
        {
            uint64_t idle_timeout_seconds = 3600;      // 1 hour default idle timeout
            uint64_t max_session_lifetime_seconds = 86400; // 24 hours default max lifetime
            bool enable_idle_timeout = true;           // Enable idle timeout checking
            bool enable_max_lifetime = true;           // Enable max lifetime checking
            bool enable_automatic_cleanup = true;      // Enable automatic cleanup of expired sessions
        };

        // AuthKey status lifecycle (Plan 03)
        enum class AuthKeyStatus : uint8_t
        {
            ACTIVE = 0,
            REVOKED = 1,
            EXPIRED = 2,
            SUSPENDED = 3
        };

        // AuthKey usage mode (Plan 03)
        enum class AuthKeyUsage : uint8_t
        {
            UNLIMITED = 0,
            LIMITED = 1,
            SINGLE_USE = 2
        };

        struct AuthKeyInfo
        {
            ID authkey_id;
            std::string issuer;
            uint64_t valid_from = 0;
            uint64_t valid_to = 0;
            uint32_t usage_limit = 0;
            uint32_t usage_count = 0;
            AuthKeyStatus status = AuthKeyStatus::ACTIVE;
            AuthKeyUsage usage_type = AuthKeyUsage::UNLIMITED;
            std::vector<ID> role_scope;
            std::vector<ID> group_scope;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Session information (Phase 1.4 - Security System)
        struct SessionInfo
        {
            ID session_id;                   // Unique session ID
            ID user_id;                      // Logged-in user
            std::string username;
            bool is_superuser = false;
            std::vector<ID> effective_roles; // All roles (direct + transitive)
            std::vector<ID> effective_groups; // All groups (direct + transitive)
            uint64_t login_time = 0;
            uint64_t last_activity_time = 0;
            ID current_schema_id;            // Current schema context
            ID authkey_id;                   // Bound AuthKey UUID (Plan 03)
            std::string emulation_mode;      // Dialect tag or emulation mode (Plan 03)
            uint64_t policy_epoch_global = 0;
            uint64_t policy_epoch_table = 0;

            // P1-12: Session timeout tracking
            bool is_expired = false;         // Whether session has been marked expired
            std::string expiration_reason;   // Reason for expiration (idle/lifetime)
        };

        // Dormant transaction tracking (Track 3.2 - Reattach + GC)
        enum class DormantStatementType : uint8_t
        {
            UNKNOWN = 0,
            DDL = 1,
            DML = 2,
            OTHER = 3
        };

        enum class DormantStatementStatus : uint8_t
        {
            UNKNOWN = 0,
            IN_PROGRESS = 1,
            COMPLETED = 2,
            FAILED = 3
        };

        enum class DormantTransactionState : uint8_t
        {
            DORMANT = 0,
            REATTACHED = 1,
            ROLLED_BACK = 2,
            EXPIRED = 3
        };

        enum class DormantAccessMode : uint8_t
        {
            READ_WRITE = 0,
            READ_ONLY = 1
        };

        enum class DormantWaitMode : uint8_t
        {
            WAIT = 0,
            NO_WAIT = 1
        };

        struct DormantTransactionInfo
        {
            ID dormant_id;                 // Reattach token (UUID v7)
            ID attachment_id;
            uint32_t proc_id = 0;          // ProcArray slot
            uint64_t txn_id = 0;           // MGA transaction ID
            ID session_id;                 // Protocol session UUID
            ID user_id;
            ID session_user_id;
            ID role_id;
            uint8_t isolation_level = 0;   // core::IsolationLevel enum value
            DormantAccessMode access_mode = DormantAccessMode::READ_WRITE;
            DormantWaitMode wait_mode = DormantWaitMode::WAIT;
            bool autocommit_mode = false;
            uint32_t lock_timeout_seconds = 0;
            ID current_schema_id;
            std::string session_settings;  // JSON string (search_path, dialect, parser version, etc.)
            std::string last_statement_text;
            uint64_t last_statement_hash = 0;
            DormantStatementType last_statement_type = DormantStatementType::UNKNOWN;
            DormantStatementStatus last_statement_status = DormantStatementStatus::UNKNOWN;
            DormantTransactionState state = DormantTransactionState::DORMANT;
            uint64_t start_time = 0;
            uint64_t last_activity_time = 0;
            uint64_t dormant_since = 0;
            uint64_t lease_expires_at = 0;
            uint64_t last_statement_time = 0;
            int64_t last_rows_affected = 0;
            uint32_t last_error_code = 0;
            std::string last_sqlstate;     // 5-char SQLSTATE if available
            ID server_instance_id;
            bool is_valid = true;
        };

        struct PreparedTransactionInfo
        {
            ID prepared_id;         // Internal UUID for catalog storage
            uint64_t txn_id = 0;    // MGA transaction ID
            std::string gid;        // Global transaction ID (2PC)
            ID owner_id;            // User that prepared the transaction
            ID database_id;         // Database UUID
            uint64_t prepared_time = 0; // Epoch micros
            bool is_valid = true;
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

        // Domain information removed - use DomainManager::DomainInfo instead

        // UDR information (Phase 3 - Stored Code Tables)
        struct UDRInfo
        {
            ID udr_id;
            ID schema_id;
            std::string udr_name;
            bool name_is_delimited = false;    // True if name was double-quoted (case-sensitive)
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
            bool name_is_delimited = false;    // True if name was double-quoted (case-sensitive)
            ID owner_id;
            std::string package_header;  // Stored in TOAST on disk
            std::string package_body;    // Stored in TOAST on disk
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Exception record persisted on disk (Phase 3)
        struct ExceptionRecord
        {
            ID exception_id;                 // UUID v7
            ID schema_id;                    // Schema containing the exception
            char name[CatalogConstants::MAX_IDENTIFIER_STORAGE]{}; // Exception name (UTF-8, truncated)
            uint32_t message_oid = 0;        // TOAST OID for message text
            ID owner_id;                     // Owner user UUID
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            uint8_t is_valid = 1;
            uint8_t name_is_delimited = 0;   // True if name was double-quoted (case-sensitive)
            uint8_t padding[6]{};
        };

        // Exception information (Phase 3 - Stored Code Tables)
        struct ExceptionInfo
        {
            ID exception_id;                 // UUID v7
            ID schema_id;                    // Schema containing the exception
            std::string name;                // Exception name
            bool name_is_delimited = false;  // True if name was double-quoted (case-sensitive)
            std::string message;             // Exception message text
            ID owner_id;                     // Owner user UUID
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        // Unified object lookup entry (for dependency resolution)
        struct ObjectLookup
        {
            ID object_id;
            ObjectType type;
            ID schema_id;
            std::string name;
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

        // ============================================================================
        // Phase B Structures - Hierarchical Schemas, FDW, Distributed MVCC, UDR
        // ============================================================================

        // Synonym - cross-schema pointer/alias (Phase B - Schema Architecture)
        struct SynonymInfo
        {
            ID synonym_id;                    // UUID v7
            ID schema_id;                     // Schema containing the synonym
            std::string synonym_name;         // Local name for the synonym
            bool name_is_delimited = false;   // True if name was double-quoted (case-sensitive)
            std::string target_path;          // Full dotted path to target object
            ObjectType target_type;           // TABLE, VIEW, SEQUENCE, PROCEDURE, FUNCTION, etc.
            ID owner_id;
            bool is_public = false;           // PUBLIC synonym (visible to all)
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Foreign Data Wrapper structures (Phase B - Wire Protocol Integration)

        // Foreign server represents a connection to an external data source
        struct ForeignServerInfo
        {
            ID server_id;                    // UUID v7
            std::string server_name;         // Unique server name
            std::string server_type;         // "postgresql", "mysql", "mssql", "firebird"
            std::string host;                // Server hostname
            uint16_t port = 0;               // Server port
            std::string connection_options;  // JSON connection parameters (stored in TOAST)
            ID owner_id;                     // Owner UUID
            bool is_active = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Foreign table maps to a table on the foreign server
        struct ForeignTableInfo
        {
            ID foreign_table_id;             // UUID v7
            ID schema_id;                    // Local schema
            std::string table_name;          // Local table name
            bool name_is_delimited = false;  // True if name was double-quoted (case-sensitive)
            ID foreign_server_id;            // References ForeignServerInfo
            std::string remote_schema;       // Remote schema name
            std::string remote_table;        // Remote table name
            std::string column_mapping;      // JSON column mapping (stored in TOAST)
            ID owner_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // User mapping for authentication to foreign server
        struct UserMappingInfo
        {
            ID mapping_id;                   // UUID v7
            ID user_id;                      // Local user
            ID foreign_server_id;            // Foreign server
            std::string remote_user;         // Username on remote server
            std::string remote_credentials;  // Encrypted credentials (stored in TOAST)
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Server Registry (Phase B - Distributed MVCC)
        // Tracks all nodes in a distributed database cluster

        enum class ServerRole : uint8_t
        {
            PRIMARY = 0,      // Primary/master node
            REPLICA = 1,      // Read replica
            STANDBY = 2,      // Hot standby
            WITNESS = 3       // Witness node (for quorum)
        };

        enum class ServerState : uint8_t
        {
            ONLINE = 0,       // Server is online and healthy
            OFFLINE = 1,      // Server is offline
            SYNCING = 2,      // Server is syncing/catching up
            MAINTENANCE = 3,  // Server in maintenance mode
            FAILED = 4        // Server has failed
        };

        struct ServerRegistryInfo
        {
            ID server_id;                    // UUID v7 (unique per server instance)
            std::string server_name;         // Human-readable name
            std::string host;                // Hostname or IP
            uint16_t port = 0;               // Listening port
            ServerRole role = ServerRole::PRIMARY;
            ServerState state = ServerState::ONLINE;
            uint64_t last_heartbeat = 0;     // Timestamp of last heartbeat
            uint64_t last_xid = 0;           // Last transaction ID processed
            uint64_t replication_lag_ms = 0; // Replication lag in milliseconds
            std::string cluster_id;          // Cluster this server belongs to
            std::string server_version;      // ScratchBird version
            std::string metadata;            // JSON metadata (stored in TOAST)
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // UDR Engine information (Phase B - UDR Plugin System)
        // Represents a language runtime that can execute UDR code

        enum class UDREngineType : uint8_t
        {
            NATIVE = 0,       // C/C++ native code
            JAVA = 1,         // Java (JVM)
            PYTHON = 2,       // Python
            JAVASCRIPT = 3,   // JavaScript (V8/Node)
            DOTNET = 4,       // .NET (CLR)
            LUA = 5,          // Lua
            WASM = 6          // WebAssembly
        };

        struct UDREngineInfo
        {
            ID engine_id;                    // UUID v7
            std::string engine_name;         // "native", "java", "python", etc.
            UDREngineType engine_type;
            std::string plugin_path;         // Path to engine plugin library
            std::string config;              // JSON configuration (stored in TOAST)
            bool is_active = true;
            bool is_default = false;         // Default engine for its type
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // UDR Module information (Phase B - UDR Plugin System)
        // Represents a loadable module containing UDR implementations

        struct UDRModuleInfo
        {
            ID module_id;                    // UUID v7
            std::string module_name;         // Module identifier
            ID engine_id;                    // Engine that runs this module
            std::string library_path;        // Path to module library/archive
            std::string checksum;            // SHA-256 checksum for verification
            std::string entry_point;         // Main entry point function
            std::string dependencies;        // JSON list of dependencies
            bool is_loaded = false;          // Currently loaded in memory
            bool is_validated = false;       // Passed security validation
            uint64_t loaded_count = 0;       // Number of times loaded
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // ============================================================================
        // End Phase B Structures
        // ============================================================================

        // ============================================================================
        // sb_statistic - Column Statistics for Query Optimizer (OPT-1, OPT-2)
        // ============================================================================

        // Column statistics information (similar to PostgreSQL's pg_statistic)
        // Fixed-size record for catalog storage - MCVs and histograms stored in TOAST
        struct StatisticInfo
        {
            ID statistic_id;              // Unique statistic ID
            ID table_id;                  // Table this column belongs to
            ID column_id;                 // Column ID
            uint16_t data_type = 0;       // DataType enum value
            uint16_t reserved1 = 0;

            // Basic statistics
            uint64_t num_rows = 0;        // Total rows in table at ANALYZE time
            uint64_t num_nulls = 0;       // Number of NULL values
            float null_fraction = 0.0f;   // Fraction of NULLs
            uint64_t num_distinct = 0;    // Number of distinct non-NULL values
            float avg_width = 0.0f;       // Average width in bytes

            // TOAST references for variable-length data
            uint32_t mcv_oid = 0;         // TOAST reference for MCV list (JSON)
            uint32_t histogram_oid = 0;   // TOAST reference for histogram (JSON)

            // Histogram metadata
            uint8_t histogram_type = 0;   // HistogramType enum (0=equal_height, 1=equal_width, 255=none)
            uint8_t padding[3] = {0};
            uint32_t histogram_bucket_count = 0;

            // Metadata
            uint64_t last_analyzed_time = 0;   // Timestamp of last ANALYZE
            uint64_t sample_size = 0;         // Number of rows sampled
            float sample_rate = 0.0f;         // Fraction of table sampled

            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        CatalogManager(Database *db);
        ~CatalogManager();

        Database *database() const
        {
            return db_;
        }

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

        // Phase A CRUD: Drop schema with optional cascade
        auto dropSchema(const ID &schema_id, bool cascade = false,
                        ErrorContext *ctx = nullptr) -> Status;

        auto updateSchemaOwner(const ID& schema_id, const std::string& owner,
                               ErrorContext* ctx = nullptr) -> Status;

        // Table operations
        auto createTable(const ID &schema_id, const std::string &table_name,
                         const std::vector<ColumnInfo> &columns, ID &table_id,
                         uint16_t tablespace_id = 0, // Phase 2 Task 2.3: default tablespace
                         ErrorContext *ctx = nullptr,
                         const TableCreateOptions* options = nullptr) -> Status;

        auto getTable(const ID &table_id, TableInfo &info, ErrorContext *ctx = nullptr) -> Status;

        auto getTable(const ID &schema_id, const std::string &table_name, TableInfo &info,
                      ErrorContext *ctx = nullptr) -> Status;

        auto listTables(const ID &schema_id, std::vector<TableInfo> &tables,
                        ErrorContext *ctx = nullptr) -> Status;

        auto listTemporaryTablesForSession(const ID &session_id, std::vector<TableInfo> &tables,
                                           ErrorContext *ctx = nullptr) -> Status;

        // DDL Modifications (ALPHA Phase 1)
        auto dropTable(const ID &table_id, bool cascade, ErrorContext *ctx = nullptr) -> Status;

        // Column operations
        auto getColumns(const ID &table_id, std::vector<ColumnInfo> &columns,
                        ErrorContext *ctx = nullptr,
                        uint32_t required_privilege = 0) -> Status;

        auto getColumn(const ID &table_id, const std::string &column_name, ColumnInfo &info,
                       ErrorContext *ctx = nullptr) -> Status;

        // Index operations
        auto createIndex(const ID &table_id, const std::string &index_name,
                         const std::vector<std::string> &column_names, ID &index_id,
                         bool is_unique = false, IndexType index_type = IndexType::BTREE,
                         uint16_t tablespace_id = 0, // Phase 2 Task 2.3: default tablespace
                         ErrorContext *ctx = nullptr) -> Status;

        auto createIndex(const ID &table_id, const std::string &index_name,
                         const std::vector<std::string> &column_names,
                         const std::vector<std::string> &include_column_names,
                         ID &index_id,
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

        auto createIndex(const ID &table_id, const std::string &index_name,
                         const std::vector<std::string> &column_names,
                         const std::vector<std::string> &include_column_names,
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
                                 ErrorContext *ctx = nullptr,
                                 bool include_inactive = true) -> Status;

        auto alterIndexState(const ID &index_id, IndexState state,
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

        // ===== Plan 01 Task E: Shadow index rebuild + versioning =====

        // Resolve existing logical index ID or generate a new UUIDv7
        auto generateLogicalIndexId(const ID &table_id, const std::string &index_name) -> ID;

        // Get the visible index version for a transaction XID
        // Returns the index_id of the version that should be used by txn_xid
        auto getVisibleIndexVersion(const ID &table_id, const std::string &index_name,
                                     uint64_t txn_xid, IndexInfo &info_out,
                                     ErrorContext *ctx = nullptr) -> Status;

        // Create a shadow index for rebuild (BUILDING state)
        // Returns the new shadow index ID
        auto createShadowIndex(const ID &existing_index_id, ID &shadow_index_id_out,
                               ErrorContext *ctx = nullptr) -> Status;

        // Promote shadow index to ACTIVE and retire the old version
        auto promoteShadowIndex(const ID &shadow_index_id, ErrorContext *ctx = nullptr) -> Status;

        // Garbage collect retired index versions (safe to delete)
        // Returns number of indexes GC'd
        auto gcRetiredIndexes(uint64_t *indexes_removed_out = nullptr,
                              ErrorContext *ctx = nullptr) -> Status;

        // ALTER TABLE operations (ALPHA Phase 1)
        auto addColumn(const ID &table_id, const ColumnInfo &column_info,
                       ErrorContext *ctx = nullptr) -> Status;
        auto dropColumn(const ID &table_id, const std::string &column_name, bool if_exists,
                        bool cascade, ErrorContext *ctx = nullptr) -> Status;
        auto renameColumn(const ID &table_id, const std::string &old_name,
                          const std::string &new_name, ErrorContext *ctx = nullptr) -> Status;
        auto alterColumnType(const ID &table_id, const std::string &column_name,
                             DataType new_type, uint32_t new_precision, uint32_t new_scale,
                             std::optional<uint16_t> new_charset_id = std::nullopt,
                             std::optional<uint32_t> new_collation_id = std::nullopt,
                             ErrorContext *ctx = nullptr) -> Status;
        auto updateColumnDefaultExpr(const ID &table_id, const std::string &column_name,
                                     const std::string &default_expr_hex, bool has_default,
                                     ErrorContext *ctx = nullptr) -> Status;
        auto updateColumnNullable(const ID &table_id, const std::string &column_name,
                                  bool nullable, ErrorContext *ctx = nullptr) -> Status;
        auto updateColumnPosition(const ID &table_id, const std::string &column_name,
                                  uint16_t new_position_1_based, ErrorContext *ctx = nullptr) -> Status;
        auto updateTableStorageParams(const ID& table_id, const std::string& storage_params,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto updateIndexParams(const ID &index_id, const std::string &index_params,
                               ErrorContext *ctx = nullptr) -> Status;
        auto renameObject(ObjectType object_type, const ID& object_id,
                          const std::string& new_name, ErrorContext* ctx = nullptr) -> Status;
        auto moveObject(ObjectType object_type, const ID& object_id,
                        const ID& target_schema_id,
                        const std::optional<std::string>& new_name = std::nullopt,
                        ErrorContext* ctx = nullptr) -> Status;

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
                            ErrorContext* ctx = nullptr,
                            const TempObjectOptions* temp_opts = nullptr,
                            const ID& owned_by_table_id = ID{},
                            const ID& owned_by_column_id = ID{}) -> Status;

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

        auto getSequenceIdByName(const ID& schema_id, const std::string& name, ID& id_out,
                                 ErrorContext* ctx = nullptr) -> Status;
        auto getSequenceIdByName(const std::string& name, ID& id_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        // WP-2 CAT-M1: List sequences by schema for CASCADE support
        auto listSequencesBySchema(const ID& schema_id, std::vector<ID>& sequence_ids_out,
                                   ErrorContext* ctx = nullptr) -> Status;

        // List sequences with full metadata for a schema (helper for information_schema).
        auto listSequences(const ID& schema_id, std::vector<SequenceInfo>& sequences_out,
                           ErrorContext* ctx = nullptr) -> Status;

        // Get sequence metadata by ID (helper for information_schema privileges).
        auto getSequenceById(const ID& sequence_id, SequenceInfo& info_out,
                             ErrorContext* ctx = nullptr) -> Status;

        auto listTemporarySequencesForSession(const ID& session_id,
                                              std::vector<SequenceInfo>& sequences_out,
                                              ErrorContext* ctx = nullptr) -> Status;

        // View operations (ALPHA Phase 1 - Views)
        auto createView(const ID& schema_id, const std::string& name,
                        const std::string& definition, bool or_replace, bool check_option,
                        bool materialized, const std::vector<std::string>& column_names,
                        const ID& materialized_table_id = ID{},
                        ErrorContext* ctx = nullptr,
                        const TempObjectOptions* temp_opts = nullptr) -> Status;

        auto dropView(const ID& view_id, bool cascade,
                      ErrorContext* ctx = nullptr) -> Status;

        auto refreshMaterializedView(const ID& view_id, bool concurrently,
                                      ErrorContext* ctx = nullptr) -> Status;  // ALPHA Phase 1 - Materialized Views

        // P2-18: Advanced refresh methods
        auto refreshMaterializedViewWithStrategy(const ID& view_id, MVRefreshStrategy strategy,
                                                 bool concurrently, ErrorContext* ctx = nullptr) -> Status;

        auto setMVRefreshStrategy(const ID& view_id, MVRefreshStrategy strategy,
                                  ErrorContext* ctx = nullptr) -> Status;

        auto setMVRefreshOnCommit(const ID& view_id, bool refresh_on_commit,
                                  ErrorContext* ctx = nullptr) -> Status;

        auto getMVRefreshStatus(const ID& view_id, uint64_t& last_refresh_time,
                                bool& is_stale, ErrorContext* ctx = nullptr) -> Status;

        auto refreshDependentMVs(const ID& base_table_id,
                                 ErrorContext* ctx = nullptr) -> Status;  // Cascade refresh

        auto getView(const ID& schema_id, const std::string& name,
                     ViewInfo& info_out, ErrorContext* ctx = nullptr) -> Status;

        auto getViewIdByName(const std::string& name, ID& id_out,
                             ErrorContext* ctx = nullptr) -> Status;

        // List all views (materialized and regular) within a schema
        auto listViewsForSchema(const ID& schema_id, std::vector<ViewInfo>& views_out,
                                ErrorContext* ctx = nullptr) -> Status;

        // OPT-5: Get view info directly by ID (for optimizer MV rewriting)
        auto getViewById(const ID& view_id, ViewInfo& info_out,
                        ErrorContext* ctx = nullptr) -> Status;

        // OPT-4: Get all materialized views (for MV candidate search)
        auto getAllMaterializedViews(std::vector<ViewInfo>& views_out,
                                     ErrorContext* ctx = nullptr) -> Status;

        auto listTemporaryViewsForSession(const ID& session_id,
                                          std::vector<ViewInfo>& views_out,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto isView(const std::string& name) -> bool;

        // Dependency operations (Phase 5.2 - Dependencies table)
        auto createDependency(const ID& dependent_object_id, ObjectType dependent_type,
                             const ID& referenced_object_id, ObjectType referenced_type,
                             DependencyType dep_type, ID& dependency_id,
                             ErrorContext* ctx = nullptr) -> Status;

        auto deleteDependency(const ID& dependency_id,
                             ErrorContext* ctx = nullptr) -> Status;

        auto getDependenciesFor(const ID& object_id,
                               std::vector<DependencyInfo>& dependencies_out,
                               ErrorContext* ctx = nullptr) -> Status;

        auto getDependents(const ID& object_id,
                          std::vector<DependencyInfo>& dependents_out,
                          ErrorContext* ctx = nullptr) -> Status;

        auto hasDependents(const ID& object_id, bool& has_dependents,
                          ErrorContext* ctx = nullptr) -> Status;

        auto listDependencies(std::vector<DependencyInfo>& dependencies_out,
                              ErrorContext* ctx = nullptr) -> Status;

        // Replace dependency set for a dependent object. Removes obsolete links and adds the provided set.
        auto replaceDependencies(const ID& dependent_object_id,
                                 ObjectType dependent_type,
                                 const std::vector<std::pair<ID, ObjectType>>& referenced_objects,
                                 ErrorContext* ctx = nullptr) -> Status;

        // Remove all dependencies where the object is the dependent (used on DROP).
        auto clearDependenciesFor(const ID& dependent_object_id,
                                  ErrorContext* ctx = nullptr) -> Status;

        // Comment operations (Phase 5.2 - Comments table)
        auto setComment(const ID& object_id, ObjectType object_type,
                       const std::string& comment_text,
                       ErrorContext* ctx = nullptr) -> Status;

        auto getComment(const ID& object_id, std::string& comment_out,
                       ErrorContext* ctx = nullptr) -> Status;

        auto listComments(std::vector<CommentInfo>& comments_out,
                         ErrorContext* ctx = nullptr) -> Status;

        auto deleteComment(const ID& object_id,
                          ErrorContext* ctx = nullptr) -> Status;

        // Object definition storage (DDL source + bytecode)
        auto setObjectDefinition(const ObjectDefinitionInfo& info,
                                 ErrorContext* ctx = nullptr) -> Status;
        auto getObjectDefinition(const ID& object_id,
                                 ObjectDefinitionInfo& info_out,
                                 ErrorContext* ctx = nullptr) -> Status;
        auto deleteObjectDefinition(const ID& object_id,
                                    ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // Domain dependency checking (Domain CRUD now in DomainManager)
        // ========================================================================

        // WP-2 CAT-M7: Find columns using a specific domain for DROP DOMAIN dependency check
        auto findColumnsByDomain(const ID& domain_id,
                                 std::vector<std::pair<ID, std::string>>& table_column_out,
                                 ErrorContext* ctx = nullptr) -> Status;
        auto findChildDomains(const ID& domain_id,
                              std::vector<DomainInfo>& child_domains_out,
                              ErrorContext* ctx = nullptr) -> Status;

        // Domain lookup wrappers (delegate to DomainManager)
        auto listDomains(const ID& schema_id,
                         std::vector<DomainInfo>& domains_out,
                         ErrorContext* ctx = nullptr) -> Status;
        auto getDomainByName(const ID& schema_id, const std::string& domain_name,
                             DomainInfo& info_out, ErrorContext* ctx = nullptr) -> Status;
        auto getDomainById(const ID& domain_id,
                           DomainInfo& info_out, ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // UDR Operations (Phase A CRUD - Catalog Cleanup)
        // ========================================================================

        auto createUDR(const ID& schema_id, const std::string& udr_name,
                       const std::string& library_path, const std::string& entry_point,
                       UDRType udr_type, const std::string& signature,
                       ID& udr_id_out, ErrorContext* ctx = nullptr) -> Status;

        auto getUDR(const ID& udr_id, UDRInfo& info_out,
                    ErrorContext* ctx = nullptr) -> Status;

        auto getUDRByName(const ID& schema_id, const std::string& udr_name,
                          UDRInfo& info_out, ErrorContext* ctx = nullptr) -> Status;

        auto updateUDR(const ID& udr_id,
                       const std::optional<std::string>& new_library_path,
                       const std::optional<std::string>& new_entry_point,
                       const std::optional<std::string>& new_signature,
                       ErrorContext* ctx = nullptr) -> Status;

        auto dropUDR(const ID& udr_id, bool cascade = false,
                     ErrorContext* ctx = nullptr) -> Status;

        auto listUDRs(const ID& schema_id, std::vector<UDRInfo>& udrs_out,
                      ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // Package Operations (Phase A CRUD - Catalog Cleanup)
        // ========================================================================

        auto createPackage(const ID& schema_id, const std::string& package_name,
                           const std::string& package_header, const std::string& package_body,
                           ID& package_id_out, ErrorContext* ctx = nullptr) -> Status;

        auto getPackage(const ID& package_id, PackageInfo& info_out,
                        ErrorContext* ctx = nullptr) -> Status;

        auto getPackageByName(const ID& schema_id, const std::string& package_name,
                              PackageInfo& info_out, ErrorContext* ctx = nullptr) -> Status;

        auto updatePackage(const ID& package_id,
                           const std::optional<std::string>& new_header,
                           const std::optional<std::string>& new_body,
                           ErrorContext* ctx = nullptr) -> Status;

        auto dropPackage(const ID& package_id, bool cascade = false,
                         ErrorContext* ctx = nullptr) -> Status;

        auto listPackages(const ID& schema_id, std::vector<PackageInfo>& packages_out,
                          ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // Exception Operations (Phase 3 - Stored Code Tables)
        // ========================================================================

        auto createException(const ID& schema_id, const std::string& name,
                             const std::string& message, ID& exception_id_out,
                             ErrorContext* ctx = nullptr) -> Status;

        auto getException(const ID& exception_id, ExceptionInfo& info_out,
                          ErrorContext* ctx = nullptr) -> Status;

        auto getExceptionByName(const ID& schema_id, const std::string& name,
                                ExceptionInfo& info_out, ErrorContext* ctx = nullptr) -> Status;

        auto dropException(const ID& exception_id, bool cascade = false,
                           ErrorContext* ctx = nullptr) -> Status;

        auto listExceptions(const ID& schema_id, std::vector<ExceptionInfo>& exceptions_out,
                            ErrorContext* ctx = nullptr) -> Status;

        // Unified object lookup (name → object_id/type/schema) for dependency resolution
        auto lookupObject(const ID& schema_id, const std::string& name,
                          ObjectLookup& out, ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // Emulation Type Operations (Phase A CRUD - Catalog Cleanup)
        // ========================================================================

        auto createEmulationType(const std::string& emulation_name,
                                 uint8_t version_major, uint8_t version_minor,
                                 const std::string& mapping_rules,
                                 ID& emulation_type_id_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        auto getEmulationType(const ID& emulation_type_id, EmulationTypeInfo& info_out,
                              ErrorContext* ctx = nullptr) -> Status;

        auto getEmulationTypeByName(const std::string& emulation_name,
                                    EmulationTypeInfo& info_out,
                                    ErrorContext* ctx = nullptr) -> Status;

        auto updateEmulationType(const ID& emulation_type_id,
                                 const std::optional<std::string>& new_mapping_rules,
                                 ErrorContext* ctx = nullptr) -> Status;

        auto dropEmulationType(const ID& emulation_type_id,
                               ErrorContext* ctx = nullptr) -> Status;

        auto listEmulationTypes(std::vector<EmulationTypeInfo>& types_out,
                                ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // Emulation Server Operations (Phase A CRUD - Catalog Cleanup)
        // ========================================================================

        auto createEmulationServer(const std::string& server_name,
                                   const ID& emulation_type_id,
                                   const std::string& server_config,
                                   ID& server_id_out,
                                   ErrorContext* ctx = nullptr) -> Status;

        auto getEmulationServer(const ID& server_id, EmulationServerInfo& info_out,
                                ErrorContext* ctx = nullptr) -> Status;

        auto getEmulationServerByName(const std::string& server_name,
                                      EmulationServerInfo& info_out,
                                      ErrorContext* ctx = nullptr) -> Status;

        auto updateEmulationServer(const ID& server_id,
                                   const std::optional<std::string>& new_config,
                                   const std::optional<bool>& is_active,
                                   ErrorContext* ctx = nullptr) -> Status;

        auto dropEmulationServer(const ID& server_id, bool cascade = false,
                                 ErrorContext* ctx = nullptr) -> Status;

        auto listEmulationServers(std::vector<EmulationServerInfo>& servers_out,
                                  ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // Emulated Database Operations (Phase A CRUD - Catalog Cleanup)
        // ========================================================================

        auto createEmulatedDatabase(const std::string& database_name,
                                    const ID& server_id, const ID& schema_id,
                                    const std::string& db_metadata,
                                    ID& emulated_db_id_out,
                                    ErrorContext* ctx = nullptr) -> Status;

        auto getEmulatedDatabase(const ID& emulated_db_id, EmulatedDatabaseInfo& info_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        auto getEmulatedDatabaseByName(const ID& server_id, const std::string& database_name,
                                       EmulatedDatabaseInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;

        auto updateEmulatedDatabase(const ID& emulated_db_id,
                                    const std::optional<std::string>& new_metadata,
                                    const std::optional<bool>& is_active,
                                    ErrorContext* ctx = nullptr) -> Status;

        auto renameEmulatedDatabase(const ID& emulated_db_id, const std::string& new_name,
                                    ErrorContext* ctx = nullptr) -> Status;

        auto updateEmulatedDatabaseOwner(const ID& emulated_db_id, const std::string& owner,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto dropEmulatedDatabase(const ID& emulated_db_id,
                                  ErrorContext* ctx = nullptr) -> Status;

        auto listEmulatedDatabases(const ID& server_id,
                                   std::vector<EmulatedDatabaseInfo>& databases_out,
                                   ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // Phase B CRUD Operations - Synonyms, FDW, Server Registry, UDR Engine/Module
        // ========================================================================

        // Synonym operations (Phase B - Schema Architecture)
        auto createSynonym(const ID& schema_id, const std::string& synonym_name,
                           const std::string& target_path, ObjectType target_type,
                           bool is_public, ID& synonym_id_out,
                           ErrorContext* ctx = nullptr) -> Status;
        auto getSynonym(const ID& synonym_id, SynonymInfo& synonym_out,
                        ErrorContext* ctx = nullptr) -> Status;
        auto getSynonymByName(const ID& schema_id, const std::string& synonym_name,
                              SynonymInfo& synonym_out,
                              ErrorContext* ctx = nullptr) -> Status;
        auto dropSynonym(const ID& synonym_id, ErrorContext* ctx = nullptr) -> Status;
        auto listSynonyms(const ID& schema_id, std::vector<SynonymInfo>& synonyms_out,
                          ErrorContext* ctx = nullptr) -> Status;
        auto listPublicSynonyms(std::vector<SynonymInfo>& synonyms_out,
                                ErrorContext* ctx = nullptr) -> Status;

        // Schema path resolution (Phase B - Schema Architecture)
        struct ResolvedObject
        {
            ID object_id;
            ObjectType object_type;
            ID schema_id;           // zero for global objects
            ID parent_object_id;    // table_id for table-scoped objects
            std::string object_name;
            std::string schema_path;
            std::string full_path;
            std::string dialect_tag;  // domains only
            std::string compat_name;  // domains only
        };

        struct ResolveOptions
        {
            bool allow_ambiguity = false;
            bool follow_synonyms = false;
            bool allow_search_path = true;
            std::string dialect_tag = "scratchbird";
            uint32_t required_privilege = 0;  // CatalogManager::Privilege bitmask (0 = no check)
        };

        struct ResolveFilter
        {
            ObjectType object_type = ObjectType::UNKNOWN;
            bool filter_schema_id = false;
            ID schema_id;
            bool filter_parent_object_id = false;
            ID parent_object_id;
            std::string schema_path_prefix;
            std::string name_prefix;
        };

        struct ResolverKey
        {
            ID scope_id;
            ObjectType object_type;
            std::string normalized_name;
            bool name_is_delimited = false;

            bool operator<(const ResolverKey& other) const
            {
                if (scope_id != other.scope_id) return scope_id < other.scope_id;
                if (object_type != other.object_type) return object_type < other.object_type;
                if (name_is_delimited != other.name_is_delimited)
                {
                    return name_is_delimited < other.name_is_delimited;
                }
                return normalized_name < other.normalized_name;
            }
        };

        auto resolveObjectPath(const ObjectPath& path, ObjectType expected_type,
                               const ResolveOptions& opts, ID& object_id_out,
                               ObjectType& type_out, ErrorContext* ctx = nullptr) -> Status;
        auto resolveObjectId(const ID& object_id, ResolvedObject& out,
                             ErrorContext* ctx = nullptr) -> Status;
        auto listResolvedObjects(const ResolveFilter& filter,
                                 std::vector<ResolvedObject>& out,
                                 ErrorContext* ctx = nullptr) -> Status;
        auto getSchemaPath(const ID& schema_id, std::string& path_out,
                           ErrorContext* ctx = nullptr) -> Status;
        auto createSchemaPath(const std::string& path, SchemaType type,
                              ID& leaf_schema_id_out,
                              ErrorContext* ctx = nullptr) -> Status;

        // Foreign Server operations (Phase B - FDW)
        auto createForeignServer(const std::string& server_name, const std::string& server_type,
                                 const std::string& host, uint16_t port,
                                 const std::string& connection_options,
                                 ID& server_id_out, ErrorContext* ctx = nullptr) -> Status;
        auto getForeignServer(const ID& server_id, ForeignServerInfo& server_out,
                              ErrorContext* ctx = nullptr) -> Status;
        auto getForeignServerByName(const std::string& server_name, ForeignServerInfo& server_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto updateForeignServer(const ID& server_id, const std::string& connection_options,
                                 bool is_active, ErrorContext* ctx = nullptr) -> Status;
        auto dropForeignServer(const ID& server_id, bool cascade, ErrorContext* ctx = nullptr) -> Status;
        auto listForeignServers(std::vector<ForeignServerInfo>& servers_out,
                                ErrorContext* ctx = nullptr) -> Status;

        // Foreign Table operations (Phase B - FDW)
        auto createForeignTable(const ID& schema_id, const std::string& table_name,
                                const ID& foreign_server_id, const std::string& remote_schema,
                                const std::string& remote_table, const std::string& column_mapping,
                                ID& table_id_out, ErrorContext* ctx = nullptr) -> Status;
        auto getForeignTable(const ID& foreign_table_id, ForeignTableInfo& table_out,
                             ErrorContext* ctx = nullptr) -> Status;
        auto dropForeignTable(const ID& foreign_table_id, ErrorContext* ctx = nullptr) -> Status;
        auto listForeignTables(const ID& schema_id, std::vector<ForeignTableInfo>& tables_out,
                               ErrorContext* ctx = nullptr) -> Status;

        // User Mapping operations (Phase B - FDW)
        auto createUserMapping(const ID& user_id, const ID& foreign_server_id,
                               const std::string& remote_user, const std::string& remote_credentials,
                               ID& mapping_id_out, ErrorContext* ctx = nullptr) -> Status;
        auto getUserMapping(const ID& user_id, const ID& foreign_server_id,
                            UserMappingInfo& mapping_out, ErrorContext* ctx = nullptr) -> Status;
        auto dropUserMapping(const ID& mapping_id, ErrorContext* ctx = nullptr) -> Status;

        // Server Registry operations (Phase B - Distributed MVCC)
        auto registerServer(const std::string& server_name, const std::string& host,
                            uint16_t port, ServerRole role, const std::string& cluster_id,
                            ID& server_id_out, ErrorContext* ctx = nullptr) -> Status;
        auto getRegisteredServer(const ID& server_id, ServerRegistryInfo& server_out,
                                 ErrorContext* ctx = nullptr) -> Status;
        auto getRegisteredServerByName(const std::string& server_name, ServerRegistryInfo& server_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto updateServerState(const ID& server_id, ServerState state,
                               ErrorContext* ctx = nullptr) -> Status;
        auto updateServerHeartbeat(const ID& server_id, uint64_t last_xid,
                                   ErrorContext* ctx = nullptr) -> Status;
        auto deregisterServer(const ID& server_id, ErrorContext* ctx = nullptr) -> Status;
        auto listRegisteredServers(const std::string& cluster_id,
                                   std::vector<ServerRegistryInfo>& servers_out,
                                   ErrorContext* ctx = nullptr) -> Status;
        auto listServersWithState(ServerState state, std::vector<ServerRegistryInfo>& servers_out,
                                  ErrorContext* ctx = nullptr) -> Status;
        auto getPrimaryServer(const std::string& cluster_id, ServerRegistryInfo& server_out,
                              ErrorContext* ctx = nullptr) -> Status;

        // UDR Engine operations (Phase B - UDR Plugin System)
        auto registerUDREngine(const std::string& engine_name, UDREngineType engine_type,
                               const std::string& plugin_path, const std::string& config,
                               ID& engine_id_out, ErrorContext* ctx = nullptr) -> Status;
        auto getUDREngine(const ID& engine_id, UDREngineInfo& engine_out,
                          ErrorContext* ctx = nullptr) -> Status;
        auto getUDREngineByName(const std::string& engine_name, UDREngineInfo& engine_out,
                                ErrorContext* ctx = nullptr) -> Status;
        auto updateUDREngine(const ID& engine_id, const std::string& config,
                             bool is_active, ErrorContext* ctx = nullptr) -> Status;
        auto dropUDREngine(const ID& engine_id, ErrorContext* ctx = nullptr) -> Status;
        auto listUDREngines(std::vector<UDREngineInfo>& engines_out,
                            ErrorContext* ctx = nullptr) -> Status;
        auto getDefaultUDREngine(UDREngineType type, UDREngineInfo& engine_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        // UDR Module operations (Phase B - UDR Plugin System)
        auto registerUDRModule(const std::string& module_name, const ID& engine_id,
                               const std::string& library_path, const std::string& entry_point,
                               ID& module_id_out, ErrorContext* ctx = nullptr) -> Status;
        auto getUDRModule(const ID& module_id, UDRModuleInfo& module_out,
                          ErrorContext* ctx = nullptr) -> Status;
        auto getUDRModuleByName(const std::string& module_name, UDRModuleInfo& module_out,
                                ErrorContext* ctx = nullptr) -> Status;
        auto validateUDRModule(const ID& module_id, ErrorContext* ctx = nullptr) -> Status;
        auto setUDRModuleLoaded(const ID& module_id, bool is_loaded,
                                ErrorContext* ctx = nullptr) -> Status;
        auto dropUDRModule(const ID& module_id, ErrorContext* ctx = nullptr) -> Status;
        auto listUDRModules(const ID& engine_id, std::vector<UDRModuleInfo>& modules_out,
                            ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // End Phase B CRUD Operations
        // ========================================================================

        // ========================================================================
        // Foreign Key Operations (ALPHA Phase A - FK Constraints)
        // ========================================================================

        // Create a foreign key constraint
        auto createForeignKey(const std::string& fk_name,
                             const ID& child_table_id,
                             const ID& parent_table_id,
                             const std::vector<std::string>& child_columns,
                             const std::vector<std::string>& parent_columns,
                             FKAction on_delete,
                             FKAction on_update,
                             FKMatchType match_type,
                             ID& fk_id_out,
                             bool is_deferrable = false,
                             bool initially_deferred = false,
                             ErrorContext* ctx = nullptr) -> Status;

        // Get foreign keys for a table (as child)
        auto getForeignKeysForTable(const ID& table_id,
                                   std::vector<ForeignKeyInfo>& fks_out,
                                   ErrorContext* ctx = nullptr) -> Status;

        // Get foreign keys that reference a table (as parent)
        auto getReferencingForeignKeys(const ID& table_id,
                                      std::vector<ForeignKeyInfo>& fks_out,
                                      ErrorContext* ctx = nullptr) -> Status;

        // Get a specific foreign key by ID
        auto getForeignKey(const ID& fk_id,
                          ForeignKeyInfo& fk_out,
                          ErrorContext* ctx = nullptr) -> Status;

        // Drop a foreign key constraint
        auto dropForeignKey(const ID& fk_id,
                           ErrorContext* ctx = nullptr) -> Status;

        // Enable/disable a foreign key
        auto setForeignKeyEnabled(const ID& fk_id, bool enabled,
                                 ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // P1-9: Constraint Operations (Unified Constraints Table)
        // ========================================================================

        // Create a constraint
        auto createConstraint(const ConstraintInfo& constraint,
                            ID& constraint_id_out,
                            ErrorContext* ctx = nullptr) -> Status;

        // Get a constraint by ID
        auto getConstraint(const ID& constraint_id,
                          ConstraintInfo& constraint_out,
                          ErrorContext* ctx = nullptr) -> Status;

        // Get a constraint by name and table
        auto getConstraintByName(const ID& table_id,
                                const std::string& constraint_name,
                                ConstraintInfo& constraint_out,
                                ErrorContext* ctx = nullptr) -> Status;

        // WP-5 EXEC-M4: Find a constraint by name globally (for SET CONSTRAINTS named)
        // Searches all tables for a constraint with the given name.
        // Returns first match if name is unique, or errors if ambiguous.
        auto findConstraintByNameGlobal(const std::string& constraint_name,
                                       ConstraintInfo& constraint_out,
                                       ErrorContext* ctx = nullptr) -> Status;

        // Get all constraints for a table
        auto getConstraintsForTable(const ID& table_id,
                                   std::vector<ConstraintInfo>& constraints_out,
                                   ErrorContext* ctx = nullptr) -> Status;

        // Get constraints of a specific type for a table
        auto getConstraintsByType(const ID& table_id,
                                 ConstraintType type,
                                 std::vector<ConstraintInfo>& constraints_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        // Update a constraint (enable/disable, defer settings, etc.)
        auto updateConstraint(const ID& constraint_id,
                            const ConstraintInfo& updated_constraint,
                            ErrorContext* ctx = nullptr) -> Status;

        // Drop a constraint
        auto dropConstraint(const ID& constraint_id,
                           ErrorContext* ctx = nullptr) -> Status;

        // Enable/disable a constraint
        auto setConstraintEnabled(const ID& constraint_id,
                                 bool enabled,
                                 ErrorContext* ctx = nullptr) -> Status;

        // Validate an existing constraint (check all rows)
        auto validateConstraint(const ID& constraint_id,
                               bool& is_valid_out,
                               std::string& violation_message_out,
                               ErrorContext* ctx = nullptr) -> Status;

        // Get constraints that reference a table (for CASCADE delete checks)
        auto getReferencingConstraints(const ID& table_id,
                                      std::vector<ConstraintInfo>& constraints_out,
                                      ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // Scheduler Job Operations (WS-4 Scheduler)
        // ========================================================================

        auto createJob(const JobInfo& job_in, ID& job_id_out,
                      ErrorContext* ctx = nullptr) -> Status;

        auto getJobByName(const std::string& job_name, JobInfo& job_out,
                         ErrorContext* ctx = nullptr) -> Status;

        auto getJob(const ID& job_id, JobInfo& job_out,
                   ErrorContext* ctx = nullptr) -> Status;

        auto updateJob(const JobInfo& job_in, ErrorContext* ctx = nullptr) -> Status;

        auto deleteJob(const ID& job_id, bool keep_history,
                      ErrorContext* ctx = nullptr) -> Status;

        auto listDueJobs(uint64_t now_ms, std::vector<JobInfo>& jobs_out,
                        ErrorContext* ctx = nullptr) -> Status;
        auto listJobs(std::vector<JobInfo>& jobs_out,
                     ErrorContext* ctx = nullptr) -> Status;

        auto createJobRun(const JobRunInfo& run_in, ID& run_id_out,
                         ErrorContext* ctx = nullptr) -> Status;

        auto updateJobRun(const JobRunInfo& run_in, ErrorContext* ctx = nullptr) -> Status;

        auto getJobRun(const ID& run_id, JobRunInfo& run_out,
                      ErrorContext* ctx = nullptr) -> Status;

        auto listJobRuns(const ID& job_id, std::vector<JobRunInfo>& runs_out,
                        ErrorContext* ctx = nullptr) -> Status;
        auto listJobRuns(std::vector<JobRunInfo>& runs_out,
                        ErrorContext* ctx = nullptr) -> Status;

        auto addJobDependencies(const ID& job_id,
                               const std::vector<ID>& depends_on,
                               ErrorContext* ctx = nullptr) -> Status;
        auto clearJobDependencies(const ID& job_id,
                               ErrorContext* ctx = nullptr) -> Status;

        auto listJobDependencies(const ID& job_id,
                                std::vector<JobDependencyInfo>& deps_out,
                                ErrorContext* ctx = nullptr) -> Status;

        auto storeJobSecret(const ID& job_id,
                            const std::string& secret_key,
                            const std::string& secret_value,
                            ErrorContext* ctx = nullptr) -> Status;
        auto getJobSecret(const ID& job_id,
                          const std::string& secret_key,
                          std::string& secret_value_out,
                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteJobSecret(const ID& job_id,
                             const std::string& secret_key,
                             ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // Security Operations (Phase 1.3 - Users, Roles, Groups)
        // ========================================================================

        // User operations
        auto createUser(const std::string& username, const std::string& password_hash,
                       const ID& default_schema_id, bool is_superuser,
                       ID& user_id_out, ErrorContext* ctx = nullptr) -> Status;

        auto ensureUserExists(const std::string& username, const std::string& password_hash,
                             const ID& default_schema_id, bool is_superuser,
                             ID& user_id_out, ErrorContext* ctx = nullptr) -> Status;

        auto getSystemUserId(ErrorContext* ctx = nullptr) -> ID;

        auto getUser(const ID& user_id, UserInfo& user_out,
                    ErrorContext* ctx = nullptr) -> Status;

        auto getUserBasic(const ID& user_id, BasicUserInfo& user_out,
                         ErrorContext* ctx = nullptr) -> Status;

        auto getUserByName(const std::string& username, UserInfo& user_out,
                          ErrorContext* ctx = nullptr) -> Status;

        auto updateUser(const ID& user_id, const std::string& password_hash,
                       const ID& default_schema_id, bool is_active, bool is_superuser,
                       ErrorContext* ctx = nullptr) -> Status;

        auto deleteUser(const ID& user_id, bool cascade = false, ErrorContext* ctx = nullptr) -> Status;

        auto listUsers(std::vector<UserInfo>& users_out,
                      ErrorContext* ctx = nullptr) -> Status;

        // Role operations
        auto createRole(const std::string& role_name, const ID& owner_id,
                       const ID& default_schema_id,
                       ID& role_id_out, ErrorContext* ctx = nullptr) -> Status;

        auto getRole(const ID& role_id, RoleInfo& role_out,
                    ErrorContext* ctx = nullptr) -> Status;

        auto getRoleByName(const std::string& role_name, RoleInfo& role_out,
                          ErrorContext* ctx = nullptr) -> Status;

        auto deleteRole(const ID& role_id, bool cascade = false, ErrorContext* ctx = nullptr) -> Status;

        // Phase A CRUD: Update role metadata
        auto updateRole(const ID& role_id, const std::optional<std::string>& new_name,
                        const std::optional<ID>& new_owner_id,
                        const std::optional<std::string>& new_metadata,
                        const std::optional<ID>& new_default_schema_id,
                        const std::optional<bool>& is_active,
                        ErrorContext* ctx = nullptr) -> Status;

        auto listRoles(std::vector<RoleInfo>& roles_out,
                      ErrorContext* ctx = nullptr) -> Status;

        // Role membership operations
        auto grantRole(const ID& role_id, const ID& user_id, const ID& granted_by,
                      bool with_admin_option, ErrorContext* ctx = nullptr) -> Status;

        auto revokeRole(const ID& role_id, const ID& user_id,
                       ErrorContext* ctx = nullptr) -> Status;

        auto getUserRoles(const ID& user_id, std::vector<RoleMembershipInfo>& roles_out,
                         ErrorContext* ctx = nullptr) -> Status;

        auto getRoleMembers(const ID& role_id, std::vector<RoleMembershipInfo>& members_out,
                           ErrorContext* ctx = nullptr) -> Status;

        // Group operations
        auto createGroup(const std::string& group_name, GroupType group_type,
                        const std::string& external_id,
                        const ID& default_schema_id,
                        ID& group_id_out,
                        ErrorContext* ctx = nullptr) -> Status;

        auto getGroup(const ID& group_id, GroupInfo& group_out,
                     ErrorContext* ctx = nullptr) -> Status;

        auto getGroupByName(const std::string& group_name, GroupInfo& group_out,
                           ErrorContext* ctx = nullptr) -> Status;

        auto deleteGroup(const ID& group_id, bool cascade = false, ErrorContext* ctx = nullptr) -> Status;

        // Phase A CRUD: Update group metadata
        auto updateGroup(const ID& group_id, const std::optional<std::string>& new_name,
                         const std::optional<GroupType>& new_type,
                         const std::optional<std::string>& new_external_id,
                         const std::optional<std::string>& new_metadata,
                         const std::optional<ID>& new_default_schema_id,
                         ErrorContext* ctx = nullptr) -> Status;

        auto listGroups(std::vector<GroupInfo>& groups_out,
                       ErrorContext* ctx = nullptr) -> Status;

        // Group membership operations (supports nested groups)
        auto addGroupMember(const ID& group_id, const ID& member_id, bool is_group,
                           const ID& granted_by, ErrorContext* ctx = nullptr) -> Status;

        auto removeGroupMember(const ID& group_id, const ID& member_id,
                              ErrorContext* ctx = nullptr) -> Status;

        auto getGroupMembers(const ID& group_id, std::vector<ID>& members_out,
                            ErrorContext* ctx = nullptr) -> Status;

        auto getUserGroups(const ID& user_id, std::vector<ID>& groups_out,
                          ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // WP-2 CAT-L1: Group Mapping CRUD Operations
        // ========================================================================

        auto createGroupMapping(const std::string& external_group_name,
                               AuthMethod auth_method, bool auto_create_users,
                               const ID& internal_group_id, ID& mapping_id_out,
                               ErrorContext* ctx = nullptr) -> Status;

        auto getGroupMapping(const ID& mapping_id, GroupMappingInfo& mapping_out,
                            ErrorContext* ctx = nullptr) -> Status;

        auto getGroupMappingByName(const std::string& external_group_name,
                                  AuthMethod auth_method,
                                  GroupMappingInfo& mapping_out,
                                  ErrorContext* ctx = nullptr) -> Status;

        auto listGroupMappings(std::vector<GroupMappingInfo>& mappings_out,
                              ErrorContext* ctx = nullptr) -> Status;

        auto listGroupMappingsForGroup(const ID& internal_group_id,
                                      std::vector<GroupMappingInfo>& mappings_out,
                                      ErrorContext* ctx = nullptr) -> Status;

        auto deleteGroupMapping(const ID& mapping_id,
                               ErrorContext* ctx = nullptr) -> Status;

        auto deleteGroupMappingsForGroup(const ID& internal_group_id,
                                        ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // Session & Permission Operations (Phase 1.4 - Security System)
        // ========================================================================

        // AuthKey management (Plan 03)
        auto createAuthKey(const AuthKeyInfo& authkey_in, ID& authkey_id_out,
                          ErrorContext* ctx = nullptr) -> Status;
        auto getAuthKey(const ID& authkey_id, AuthKeyInfo& authkey_out,
                       ErrorContext* ctx = nullptr) -> Status;
        auto revokeAuthKey(const ID& authkey_id, ErrorContext* ctx = nullptr) -> Status;
        auto consumeAuthKey(const ID& authkey_id, uint32_t uses = 1,
                           ErrorContext* ctx = nullptr) -> Status;
        auto listAuthKeys(std::vector<AuthKeyInfo>& authkeys_out,
                         ErrorContext* ctx = nullptr) -> Status;

        // Session management
        auto createSession(const ID& user_id, const ID& authkey_id,
                          const std::string& emulation_mode,
                          SessionInfo& session_out, ErrorContext* ctx = nullptr) -> Status;

        auto getSession(const ID& session_id, SessionInfo& session_out,
                       ErrorContext* ctx = nullptr) -> Status;

        auto listSessions(std::vector<SessionInfo>& sessions_out,
                         ErrorContext* ctx = nullptr) -> Status;

        auto closeSession(const ID& session_id, ErrorContext* ctx = nullptr) -> Status;

        // Runtime monitoring
        struct TransactionHistoryEntry
        {
            uint32_t thread_id = 0;
            uint64_t event_id = 0;
            uint64_t end_event_id = 0;
            uint64_t trx_id = 0;
            uint64_t timer_start = 0;
            uint64_t timer_end = 0;
            uint64_t timer_wait = 0;
            bool read_only = false;
            uint8_t isolation_level = 0;
            bool autocommit = false;
            bool committed = false;
        };

        struct WaitHistoryEntry
        {
            uint32_t thread_id = 0;
            uint64_t event_id = 0;
            uint64_t timer_start = 0;
            uint64_t timer_end = 0;
            uint64_t timer_wait = 0;
            uint64_t object_instance_begin = 0;
            bool timed_out = false;
        };

        static constexpr size_t kDigestHistogramBuckets = 18;
        static constexpr std::array<uint64_t, kDigestHistogramBuckets> kDigestHistogramUpperBounds = {
            1ULL,
            5ULL,
            10ULL,
            50ULL,
            100ULL,
            500ULL,
            1000ULL,
            5000ULL,
            10000ULL,
            50000ULL,
            100000ULL,
            500000ULL,
            1000000ULL,
            5000000ULL,
            10000000ULL,
            50000000ULL,
            100000000ULL,
            1000000000ULL
        };

        static size_t digestHistogramBucket(uint64_t value)
        {
            for (size_t i = 0; i < kDigestHistogramBuckets; ++i)
            {
                if (value <= kDigestHistogramUpperBounds[i])
                {
                    return i;
                }
            }
            return kDigestHistogramBuckets - 1;
        }

        static uint64_t digestHistogramUpperBound(size_t bucket)
        {
            if (bucket >= kDigestHistogramBuckets)
            {
                return kDigestHistogramUpperBounds[kDigestHistogramBuckets - 1];
            }
            return kDigestHistogramUpperBounds[bucket];
        }

        static uint64_t digestHistogramLowerBound(size_t bucket)
        {
            if (bucket == 0)
            {
                return 0;
            }
            if (bucket >= kDigestHistogramBuckets)
            {
                bucket = kDigestHistogramBuckets - 1;
            }
            return kDigestHistogramUpperBounds[bucket - 1] + 1;
        }

        struct StatementDigestEntry
        {
            std::string schema_name;
            std::string user_name;
            std::string host_name;
            std::string digest;
            std::string digest_text;
            uint64_t count_star = 0;
            uint64_t sum_timer_wait = 0;
            uint64_t min_timer_wait = 0;
            uint64_t max_timer_wait = 0;
            uint64_t sum_lock_time = 0;
            uint64_t sum_errors = 0;
            uint64_t sum_warnings = 0;
            uint64_t sum_rows_affected = 0;
            uint64_t sum_rows_sent = 0;
            uint64_t sum_rows_examined = 0;
            uint64_t sum_created_tmp_disk_tables = 0;
            uint64_t sum_created_tmp_tables = 0;
            uint64_t sum_select_full_join = 0;
            uint64_t sum_select_full_range_join = 0;
            uint64_t sum_select_range = 0;
            uint64_t sum_select_range_check = 0;
            uint64_t sum_select_scan = 0;
            uint64_t sum_sort_merge_passes = 0;
            uint64_t sum_sort_range = 0;
            uint64_t sum_sort_rows = 0;
            uint64_t sum_sort_scan = 0;
            uint64_t sum_no_index_used = 0;
            uint64_t sum_no_good_index_used = 0;
            uint64_t sum_cpu_time = 0;
            uint64_t max_controlled_memory = 0;
            uint64_t max_total_memory = 0;
            uint64_t count_secondary = 0;
            uint64_t first_seen = 0;
            uint64_t last_seen = 0;
            uint64_t quantile_95 = 0;
            uint64_t quantile_99 = 0;
            uint64_t quantile_999 = 0;
            std::string query_sample_text;
            uint64_t query_sample_seen = 0;
            uint64_t query_sample_timer_wait = 0;
            std::array<uint64_t, kDigestHistogramBuckets> histogram_counts{};
        };

        auto listLocks(std::vector<LockSnapshot>& locks_out,
                       ErrorContext* ctx = nullptr) -> Status;
        auto recordTransactionHistory(const TransactionHistoryEntry& entry,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto listTransactionHistory(std::vector<TransactionHistoryEntry>& entries_out,
                                    ErrorContext* ctx = nullptr) const -> Status;
        auto recordWaitHistory(const WaitHistoryEntry& entry,
                               ErrorContext* ctx = nullptr) -> Status;
        auto listWaitHistory(std::vector<WaitHistoryEntry>& entries_out,
                             ErrorContext* ctx = nullptr) const -> Status;
        auto recordStatementDigest(const StatementDigestEntry& entry,
                                   ErrorContext* ctx = nullptr) -> Status;
        auto listStatementDigestSummary(std::vector<StatementDigestEntry>& entries_out,
                                        ErrorContext* ctx = nullptr) const -> Status;
        auto listStatementDigestSummaryByAccount(std::vector<StatementDigestEntry>& entries_out,
                                                 ErrorContext* ctx = nullptr) const -> Status;
        auto listStatementDigestSummaryByUser(std::vector<StatementDigestEntry>& entries_out,
                                              ErrorContext* ctx = nullptr) const -> Status;
        auto listStatementDigestSummaryByHost(std::vector<StatementDigestEntry>& entries_out,
                                              ErrorContext* ctx = nullptr) const -> Status;
        auto getStatementDigestHistogramGlobal(std::array<uint64_t, kDigestHistogramBuckets>& counts_out,
                                               ErrorContext* ctx = nullptr) const -> Status;

        // P1-12: Session timeout management
        auto updateSessionActivity(const ID& session_id, ErrorContext* ctx = nullptr) -> Status;

        auto checkSessionTimeout(const ID& session_id, const SessionTimeoutConfig& config,
                                 bool& is_expired_out, std::string& reason_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        auto cleanupExpiredSessions(const SessionTimeoutConfig& config,
                                   uint32_t& cleaned_count_out,
                                   ErrorContext* ctx = nullptr) -> Status;

        auto setSessionTimeoutConfig(const SessionTimeoutConfig& config,
                                    ErrorContext* ctx = nullptr) -> Status;

        auto getSessionTimeoutConfig(SessionTimeoutConfig& config_out,
                                    ErrorContext* ctx = nullptr) -> Status;

        // Security policy epochs (Plan 03)
        auto getSecurityPolicyEpoch(uint64_t& epoch_out,
                                   ErrorContext* ctx = nullptr) -> Status;
        auto bumpSecurityPolicyEpoch(uint64_t& epoch_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto getTablePolicyEpoch(const ID& table_id, uint64_t& epoch_out,
                                ErrorContext* ctx = nullptr) -> Status;
        auto bumpTablePolicyEpoch(const ID& table_id, uint64_t& epoch_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        // Audit log persistence (Plan 03)
        auto appendAuditLog(const AuditEvent& event,
                            const std::array<uint8_t, 32>& hash_prev,
                            const std::array<uint8_t, 32>& hash_curr,
                            ErrorContext* ctx = nullptr) -> Status;
        auto queryAuditLog(const AuditQuery& query, std::vector<AuditEvent>& events_out,
                           ErrorContext* ctx = nullptr) -> Status;
        auto getAuditLogTail(uint64_t& last_event_id_out,
                             std::array<uint8_t, 32>& last_hash_out,
                             ErrorContext* ctx = nullptr) -> Status;

        // Dormant transaction persistence (Track 3.2)
        auto createDormantTransaction(DormantTransactionInfo& info,
                                     ErrorContext* ctx = nullptr) -> Status;

        auto getDormantTransaction(const ID& dormant_id, DormantTransactionInfo& info_out,
                                  ErrorContext* ctx = nullptr) -> Status;

        auto updateDormantTransaction(const DormantTransactionInfo& info,
                                     ErrorContext* ctx = nullptr) -> Status;

        auto deleteDormantTransaction(const ID& dormant_id,
                                     ErrorContext* ctx = nullptr) -> Status;

        auto listDormantTransactions(std::vector<DormantTransactionInfo>& dormants_out,
                                    ErrorContext* ctx = nullptr) -> Status;

        // Prepared transaction persistence (2PC)
        auto createPreparedTransaction(PreparedTransactionInfo& info,
                                      ErrorContext* ctx = nullptr) -> Status;

        auto getPreparedTransactionByGid(const std::string& gid,
                                        PreparedTransactionInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;

        auto deletePreparedTransaction(const std::string& gid,
                                      ErrorContext* ctx = nullptr) -> Status;

        auto listPreparedTransactions(std::vector<PreparedTransactionInfo>& prepared_out,
                                     ErrorContext* ctx = nullptr) -> Status;

        // Compute transitive closure of roles (including roles granted to roles)
        auto getEffectiveRoles(const ID& user_id, std::vector<ID>& roles_out,
                              ErrorContext* ctx = nullptr) -> Status;

        // Compute transitive closure of groups (including nested groups)
        auto getEffectiveGroups(const ID& user_id, std::vector<ID>& groups_out,
                               ErrorContext* ctx = nullptr) -> Status;

        // Permission operations
        auto grantPermission(const ID& object_id, PermissionObjectType object_type,
                            const ID& grantee_id, GranteeType grantee_type,
                            uint32_t privileges, bool grant_option,
                            const ID& grantor_id, ErrorContext* ctx = nullptr) -> Status;

        auto revokePermission(const ID& object_id, PermissionObjectType object_type,
                             const ID& grantee_id, GranteeType grantee_type,
                             uint32_t privileges, ErrorContext* ctx = nullptr) -> Status;

        // WP-5 EXEC-M5: Revoke permission with CASCADE support
        // When cascade=true, also revokes permissions that the grantee had granted to others
        auto revokePermissionCascade(const ID& object_id, PermissionObjectType object_type,
                                    const ID& grantee_id, GranteeType grantee_type,
                                    uint32_t privileges, ErrorContext* ctx = nullptr) -> Status;

        auto hasPermission(const ID& user_id, const ID& object_id,
                          PermissionObjectType object_type, Privilege privilege,
                          bool& has_perm_out, ErrorContext* ctx = nullptr) -> Status;

        auto getObjectPermissions(const ID& object_id, PermissionObjectType object_type,
                                 std::vector<PermissionInfo>& permissions_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        auto getUserPermissions(const ID& user_id, std::vector<PermissionInfo>& permissions_out,
                               ErrorContext* ctx = nullptr) -> Status;
        auto listPermissions(std::vector<PermissionInfo>& permissions_out,
                             ErrorContext* ctx = nullptr) -> Status;

        // Default privilege operations
        auto grantDefaultPrivilege(const ID& schema_id,
                                   const ID& grantor_id,
                                   PermissionObjectType object_type,
                                   const ID& grantee_id,
                                   GranteeType grantee_type,
                                   uint32_t privileges,
                                   bool grant_option,
                                   ErrorContext* ctx = nullptr) -> Status;

        auto revokeDefaultPrivilege(const ID& schema_id,
                                    const ID& grantor_id,
                                    PermissionObjectType object_type,
                                    const ID& grantee_id,
                                    GranteeType grantee_type,
                                    uint32_t privileges,
                                    ErrorContext* ctx = nullptr) -> Status;

        auto listDefaultPrivileges(const ID& schema_id,
                                   const ID& grantor_id,
                                   PermissionObjectType object_type,
                                   std::vector<DefaultPrivilegeInfo>& defaults_out,
                                   ErrorContext* ctx = nullptr) -> Status;

        auto applyDefaultPrivileges(const ID& schema_id,
                                    PermissionObjectType object_type,
                                    const ID& object_id,
                                    const ID& grantor_id,
                                    ErrorContext* ctx = nullptr) -> Status;

        // Security Phase 3.3: Column-level permission operations
        auto grantColumnPermission(const ID& table_id, const std::string& column_name,
                                  const ID& grantee_id, GranteeType grantee_type,
                                  uint32_t privileges, bool grant_option,
                                  const ID& grantor_id, ErrorContext* ctx = nullptr) -> Status;

        auto revokeColumnPermission(const ID& table_id, const std::string& column_name,
                                   const ID& grantee_id, GranteeType grantee_type,
                                   uint32_t privileges, ErrorContext* ctx = nullptr) -> Status;

        auto hasColumnPermission(const ID& user_id, const ID& table_id,
                                const std::string& column_name, Privilege privilege,
                                bool& has_perm_out, ErrorContext* ctx = nullptr) -> Status;

        auto getAccessibleColumns(const ID& user_id, const ID& table_id,
                                 Privilege privilege, std::vector<std::string>& columns_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        auto getColumnPermissions(const ID& table_id,
                                 std::vector<ColumnPermissionInfo>& perms_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        // Security Phase 3.4: Row-level security policy operations
        auto createPolicy(const ID& table_id, const std::string& policy_name,
                         PolicyType type, const std::vector<std::string>& roles,
                         const std::string& using_expr, const std::string& with_check_expr,
                         ID& policy_id_out, ErrorContext* ctx = nullptr) -> Status;

        auto dropPolicy(const ID& table_id, const std::string& policy_name,
                       ErrorContext* ctx = nullptr) -> Status;

        /**
         * Alter a policy - enable/disable or modify properties
         *
         * @param table_id Table ID
         * @param policy_name Policy name
         * @param is_enabled New enabled state (optional, -1 to not change)
         * @param using_expr New USING expression (empty to not change)
         * @param with_check_expr New WITH CHECK expression (empty to not change)
         * @param ctx Error context
         * @return Status OK if successful
         */
        auto alterPolicy(const ID& table_id, const std::string& policy_name,
                        int is_enabled, const std::string& using_expr,
                        const std::string& with_check_expr,
                        ErrorContext* ctx = nullptr) -> Status;

        auto getPolicy(const ID& table_id, const std::string& policy_name,
                      PolicyInfo& policy_out, ErrorContext* ctx = nullptr) -> Status;

        auto getTablePolicies(const ID& table_id, PolicyType type,
                             std::vector<PolicyInfo>& policies_out,
                             ErrorContext* ctx = nullptr) -> Status;

        auto getPoliciesForUser(const ID& table_id, const ID& user_id,
                               PolicyType type, std::vector<PolicyInfo>& policies_out,
                               ErrorContext* ctx = nullptr) -> Status;

        auto setTableRLS(const ID& table_id, bool enabled, bool forced,
                        ErrorContext* ctx = nullptr) -> Status;

        // Test helper: Clear policy cache to force TOAST loading (Phase 3.4.8)
        void clearPolicyCache();


        auto getTableRLS(const ID& table_id, bool& enabled_out, bool& forced_out,
                        ErrorContext* ctx = nullptr) -> Status;

        // Object permission operations (Phase 3.1 - SQL Object Permissions)
        auto grantObjectPermission(const ID& object_id, ObjectType object_type,
                                  const ID& grantee_id, GranteeType grantee_type,
                                  uint32_t permissions, bool grant_option,
                                  ID& permission_id_out, ErrorContext* ctx = nullptr) -> Status;

        auto revokeObjectPermission(const ID& object_id, const ID& grantee_id,
                                   ErrorContext* ctx = nullptr) -> Status;

        auto hasObjectPermission(const ID& object_id, const ID& user_id,
                                uint32_t required_permissions,
                                ErrorContext* ctx = nullptr) -> bool;

        auto getObjectPermissions(const ID& object_id,
                                 std::vector<ObjectPermissionInfo>& perms_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        // Timezone operations (sb_timezone system table)
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
        auto setTimezoneVersion(const std::string& version, ErrorContext* ctx = nullptr) -> Status;
        auto getTimezoneVersion(std::string& version_out, ErrorContext* ctx = nullptr) -> Status;
        auto setI18nResourceVersion(const std::string& version, ErrorContext* ctx = nullptr) -> Status;
        auto getI18nResourceVersion(std::string& version_out, ErrorContext* ctx = nullptr) -> Status;

        // ========================================================================
        // Statistics operations (sb_statistic system table - OPT-1, OPT-2)
        // ========================================================================

        /**
         * storeStatistic - Store column statistics to sb_statistic catalog
         *
         * @param info Statistics information to store
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Creates or updates statistics for a column. If statistics already exist
         * for the same (table_id, column_id), they are replaced.
         */
        auto storeStatistic(const StatisticInfo& info, ErrorContext* ctx = nullptr) -> Status;

        /**
         * getStatistic - Retrieve column statistics from sb_statistic catalog
         *
         * @param table_id Table ID
         * @param column_id Column ID
         * @param info_out Output statistics information
         * @param ctx Error context
         * @return Status::OK if found, Status::NOT_FOUND otherwise
         */
        auto getStatistic(const ID& table_id, const ID& column_id,
                          StatisticInfo& info_out, ErrorContext* ctx = nullptr) -> Status;

        /**
         * getStatisticsForTable - Get all column statistics for a table
         *
         * @param table_id Table ID
         * @param stats_out Output vector of statistics
         * @param ctx Error context
         * @return Status::OK on success
         */
        auto getStatisticsForTable(const ID& table_id,
                                   std::vector<StatisticInfo>& stats_out,
                                   ErrorContext* ctx = nullptr) -> Status;

        /**
         * deleteStatistic - Delete column statistics from sb_statistic catalog
         *
         * @param table_id Table ID
         * @param column_id Column ID
         * @param ctx Error context
         * @return Status::OK on success, Status::NOT_FOUND if not exists
         */
        auto deleteStatistic(const ID& table_id, const ID& column_id,
                             ErrorContext* ctx = nullptr) -> Status;

        /**
         * deleteStatisticsForTable - Delete all statistics for a table
         *
         * @param table_id Table ID
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Used when dropping a table to clean up associated statistics.
         */
        auto deleteStatisticsForTable(const ID& table_id,
                                      ErrorContext* ctx = nullptr) -> Status;

        // Character set operations (sb_charset system table)
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

        // Collation operations (sb_collation system table)
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
         * Updates sb_tablespace statistics after tablespace extension.
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
         * 8. Add entry to sb_tablespace catalog
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
                              bool validate, bool allow_uuid_mismatch,
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
         * 8. Remove from sb_tablespace catalog
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
         * - sb_tablespace (tablespaces_table_page_)
         * - sb_schema (schemas_table_page_)
         * - sb_table (tables_table_page_)
         * - sb_column (columns_table_page_)
         * - sb_index (indexes_table_page_)
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

        // =====================================================================
        // WP-2 CAT-L2: Migration History CRUD
        // =====================================================================

        /**
         * recordMigrationHistory - Persist a migration record to history table
         *
         * @param state Completed migration state to persist
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Called automatically by completeMigration().
         * Records are never deleted, only marked invalid for garbage collection.
         */
        auto recordMigrationHistory(const TableMigrationState &state,
                                   ErrorContext *ctx = nullptr) -> Status;

        /**
         * getMigrationHistory - Get a specific migration history record
         *
         * @param history_id History record ID
         * @param info_out Output structure
         * @param ctx Error context
         * @return Status::OK if found, Status::NOT_FOUND otherwise
         */
        auto getMigrationHistory(const ID &history_id,
                                MigrationHistoryInfo *info_out,
                                ErrorContext *ctx = nullptr) -> Status;

        /**
         * listMigrationHistory - List all migration history records
         *
         * @param ctx Error context
         * @return Vector of migration history records
         *
         * Returns all valid history records sorted by start_time descending.
         */
        auto listMigrationHistory(ErrorContext *ctx = nullptr)
            -> std::vector<MigrationHistoryInfo>;

        /**
         * listMigrationHistoryForTable - List migration history for a specific table
         *
         * @param table_id Table ID
         * @param ctx Error context
         * @return Vector of migration history records for the table
         */
        auto listMigrationHistoryForTable(const ID &table_id,
                                         ErrorContext *ctx = nullptr)
            -> std::vector<MigrationHistoryInfo>;

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
            std::lock_guard<CatalogMutex> lock(mutex_);
            return schema_count_;
        }
        auto tableCount() const -> uint32_t
        {
            std::lock_guard<CatalogMutex> lock(mutex_);
            return table_count_;
        }

        // Trigger operations (Phase 2 Wave 2 - Agent C: Basic Triggers)
        
        // Trigger timing
        enum class TriggerTiming : uint8_t
        {
            BEFORE = 0,
            AFTER = 1,
            INSTEAD_OF = 2
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
            bool name_is_delimited = false;    // True if name was double-quoted (case-sensitive)
            ID table_id;
            std::string table_name;
            TriggerTiming timing;
            uint8_t event_mask = 1u << static_cast<uint8_t>(TriggerEvent::INSERT);
            TriggerGranularity granularity;
            std::string procedure_name;
            bool enabled = true;  // Can be disabled without dropping
            uint64_t created_time = 0;

            // P2-8: Statement-level trigger support
            std::string old_table_alias;  // REFERENCING OLD TABLE AS name (empty if not specified)
            std::string new_table_alias;  // REFERENCING NEW TABLE AS name (empty if not specified)
            std::string when_expression;  // Optional WHEN condition (serialized bytecode)
        };

        // ===== Database Triggers (Firebird-style) =====
        // Database triggers fire on session/transaction events, not table operations

        // Database trigger event types (matches parser::DatabaseTriggerEvent)
        enum class DatabaseTriggerEvent : uint8_t
        {
            ON_CONNECT = 0,           // Fires when client connects to database
            ON_DISCONNECT = 1,        // Fires when client disconnects from database
            ON_TRANSACTION_START = 2, // Fires when transaction starts
            ON_TRANSACTION_COMMIT = 3,   // Fires when transaction commits
            ON_TRANSACTION_ROLLBACK = 4  // Fires when transaction rolls back
        };

        // Database trigger information
        struct DatabaseTriggerInfo
        {
            ID trigger_id;                        // UUID v7
            std::string trigger_name;             // Trigger name (unique)
            DatabaseTriggerEvent event;           // Which event fires this trigger
            bool active = true;                   // ACTIVE vs INACTIVE
            int32_t position = 0;                 // POSITION n (execution order, lower first)
            std::string procedure_name;           // Procedure to call: procedure_name()
            uint64_t created_time = 0;            // Creation timestamp
            ID owner_id;                          // Owner user UUID
        };

        // Database trigger management methods
        auto createDatabaseTrigger(const DatabaseTriggerInfo &trigger, ErrorContext *ctx = nullptr) -> Status;

        auto dropDatabaseTrigger(const std::string &trigger_name, ErrorContext *ctx = nullptr) -> Status;

        auto getDatabaseTrigger(const ID &trigger_id, DatabaseTriggerInfo &info, ErrorContext *ctx = nullptr)
            -> Status;

        auto getDatabaseTriggerByName(const std::string &trigger_name, DatabaseTriggerInfo &info,
                                      ErrorContext *ctx = nullptr) -> Status;

        auto listDatabaseTriggers(DatabaseTriggerEvent event, std::vector<DatabaseTriggerInfo> &triggers,
                                  ErrorContext *ctx = nullptr) -> Status;

        auto listAllDatabaseTriggers(std::vector<DatabaseTriggerInfo> &triggers,
                                     ErrorContext *ctx = nullptr) -> Status;

        auto enableDatabaseTrigger(const std::string &trigger_name, bool enable,
                                   ErrorContext *ctx = nullptr) -> Status;

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
            enum class SqlSecurity : uint8_t {
                DEFINER = 0,  // Execute with owner's privileges
                INVOKER = 1   // Execute with caller's privileges (default)
            };

            ID function_id;                        // UUID v7
            ID schema_id;                          // Owning schema UUID
            std::string name;
            bool name_is_delimited = false;        // True if name was double-quoted (case-sensitive)
            ID owner_id;                           // Phase 3.1: Owner user UUID
            std::vector<ParameterInfo> parameters;
            DataType return_type = DataType::INT32;
            uint32_t return_type_precision = 0;
            uint32_t return_type_scale = 0;
            bool or_replace = false;
            bool deterministic = false;
            SqlSecurity sql_security = SqlSecurity::INVOKER;  // Phase 3.1
            std::vector<uint8_t> bytecode;  // Compiled SBLR bytecode
            std::string source_text;        // Original PSQL source
            std::vector<std::pair<ID, ObjectType>> referenced_objects;  // Dependency targets
            uint64_t created_time = 0;
            uint64_t modified_time = 0;
        };

        // Procedure information
        struct ProcedureInfo
        {
            enum class SqlSecurity : uint8_t {
                DEFINER = 0,  // Execute with owner's privileges
                INVOKER = 1   // Execute with caller's privileges (default)
            };

            ID procedure_id;                       // UUID v7
            ID schema_id;                          // Owning schema UUID
            std::string name;
            bool name_is_delimited = false;        // True if name was double-quoted (case-sensitive)
            ID owner_id;                           // Phase 3.1: Owner user UUID
            std::vector<ParameterInfo> parameters;
            bool or_replace = false;
            SqlSecurity sql_security = SqlSecurity::INVOKER;  // Phase 3.1
            std::vector<uint8_t> bytecode;  // Compiled SBLR bytecode
            std::string source_text;        // Original PSQL source
            std::vector<std::pair<ID, ObjectType>> referenced_objects;  // Dependency targets
            uint64_t created_time = 0;
            uint64_t modified_time = 0;
        };

        // Function/Procedure management methods
        auto registerFunction(const FunctionInfo &info, ErrorContext *ctx = nullptr) -> Status;
        auto registerProcedure(const ProcedureInfo &info, ErrorContext *ctx = nullptr) -> Status;

        auto getFunction(const std::string &name, FunctionInfo &info_out,
                        ErrorContext *ctx = nullptr) -> Status;
        auto getFunctionById(const ID& function_id, FunctionInfo& info_out,
                             ErrorContext* ctx = nullptr) -> Status;
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

        // WP-5 EXEC-M6: Public TOAST loading for CHECK expressions and other catalog expressions
        // Load a string from TOAST storage using its OID
        // @param oid TOAST value ID
        // @param xmin Transaction ID for visibility (use 0 for catalog operations)
        // @param str_out Output string
        // @param ctx Error context
        // @return Status::OK on success
        auto loadStringFromToast(uint32_t oid, uint64_t xmin,
                                std::string& str_out, ErrorContext* ctx = nullptr) -> Status;

        // OPT-1/OPT-2: Public TOAST storage for statistics data (MCVs, histograms)
        // Store a string in TOAST and return its OID
        // @param str String to store
        // @param xmin Transaction ID (use 0 for catalog operations)
        // @param oid_out Output OID for the stored value
        // @param ctx Error context
        // @return Status::OK on success
        auto storeStringInToast(const std::string& str, uint64_t xmin,
                               uint32_t& oid_out, ErrorContext* ctx = nullptr) -> Status;

        // Initialize policy TOAST storage (must be called after StorageEngine is ready)
        auto initializePolicyToastIfNeeded(ErrorContext* ctx = nullptr) -> Status;

        // Plan 03B: Domains catalog page
        auto ensureDomainsTablePage(ErrorContext* ctx = nullptr) -> Status;
        auto domainsTablePage() const -> uint32_t
        {
            return domains_table_page_;
        }

        // Plan 03B: Encryption key catalog table
        auto ensureEncryptionKeysTable(ErrorContext* ctx = nullptr) -> Status;
        auto encryptionKeysTablePage() const -> uint32_t
        {
            return encryption_keys_table_page_;
        }

        // Policy TOAST table ID (used for expression storage)
        auto policyToastTableId() const -> const ID&
        {
            return policy_toast_table_id_;
        }

        // Plan 01 Task B: Heap page enumeration (now public)
        // Enumerates all heap pages belonging to a table
        // Filters by table_id field in PageHeader (ON_DISK_FORMAT.md v1.4.0)
        // Returns Status::OK on success, error status otherwise
        auto enumerateTablePages(const ID &table_id,
                                std::vector<GPID> &pages_out,
                                ErrorContext *ctx = nullptr) -> Status;

    private:
        // Resolver cache rebuild (Plan 02 - UUID resolution)
        auto rebuildResolverCache(ErrorContext* ctx = nullptr) -> Status;

        // Internal helper functions (assume mutex_ is already held)
        auto getColumnInternal(const ID &table_id, const std::string &column_name,
                               ColumnInfo &info, ErrorContext *ctx) -> Status;

        // Internal unlocked version of getUserByName - caller must hold mutex_
        auto getUserBasicUnlocked(const ID& user_id, BasicUserInfo& user_out,
                                 ErrorContext* ctx) -> Status;
        auto getUserByNameUnlocked(const std::string& username, UserInfo& user_out,
                                   ErrorContext* ctx) -> Status;
        auto getSystemUserIdUnlocked(ErrorContext* ctx) -> ID;

        // Internal unlocked version of dropIndex - caller must hold mutex_
        // Used by dropTable to avoid deadlock
        auto dropIndexInternal(const ID &index_id, ErrorContext *ctx) -> Status;

        // Internal logical index ID resolver/generator - caller must hold mutex_
        auto generateLogicalIndexIdUnlocked(const ID &table_id,
                                            const std::string &index_name) -> ID;

        // WP-2 CAT-M3: Extract column references from expression bytecode
        static void extractColumnRefsFromBytecode(const std::vector<uint8_t>& bytecode,
                                                   std::vector<std::string>& column_names_out);

        // Phase 1: Dependency Infrastructure - Helper methods for DROP operations

        // Dependency filter result - separates owned from blocking dependencies
        struct DependencyFilter {
            std::vector<DependencyInfo> owned;      // Auto-drop (indexes, triggers, etc.)
            std::vector<DependencyInfo> blocking;   // Error if exist (views, FKs, etc.)
        };

        // Resolved dependency name for error reporting (name resolution happens elsewhere)
        struct DependencyName {
            ObjectType dependent_type;
            std::string dependent_name;
        };

        // Convert ObjectType enum to user-friendly string
        static auto objectTypeToString(ObjectType type) -> std::string;

        // Get object name by ID and type (for error messages)
        auto getObjectName(const ID& object_id, ObjectType type,
                          ErrorContext* ctx) -> std::string;

        // Filter dependencies into owned vs blocking categories
        auto filterDependencies(const ID& owner_id, ObjectType owner_type,
                               const std::vector<DependencyInfo>& all_deps,
                               ErrorContext* ctx) -> DependencyFilter;

        // Resolve dependent object names for error reporting (call outside object-type locks).
        void resolveDependencyNames(const std::vector<DependencyInfo>& deps,
                                    std::vector<DependencyName>& names_out,
                                    ErrorContext* ctx);

        // Build detailed error message for blocked DROP operations
        auto buildDependencyErrorMessage(const std::string& object_name,
                                        ObjectType object_type,
                                        const std::vector<DependencyName>& blocking_deps) -> std::string;

    private:
        // Internal helpers (assume locks already held by caller)
        // These functions do NOT acquire locks - caller must hold appropriate mutexes
        void getDependentsInternal(const ID& object_id,
                                   std::vector<DependencyInfo>& dependents_out);

        void clearDependenciesForInternal(const ID& dependent_object_id,
                                         ErrorContext* ctx);

        auto getObjectNameInternal(const ID& object_id, ObjectType type,
                                   ErrorContext* ctx) -> std::string;

        void resolveDependencyNamesInternal(const std::vector<DependencyInfo>& deps,
                                           std::vector<DependencyName>& names_out,
                                           ErrorContext* ctx);

        void getDependenciesForInternal(const ID& object_id,
                                       std::vector<DependencyInfo>& dependencies_out);

        auto createDependencyInternal(const ID& dependent_object_id, ObjectType dependent_type,
                                     const ID& referenced_object_id, ObjectType referenced_type,
                                     DependencyType dep_type, ID& dependency_id,
                                     ErrorContext* ctx) -> Status;

        auto dropSequenceInternal(const ID& sequence_id, ErrorContext* ctx) -> Status;

        // Internal trigger helpers (assume trigger_mutex_ already held)
        auto getTriggerInternal(const ID& trigger_id, TriggerInfo& info, ErrorContext* ctx) -> Status;
        auto dropTriggerInternal(const std::string& trigger_name, ErrorContext* ctx) -> Status;

        // Scheduler job cache helpers (assume mutex_ already held)
        auto ensureJobCacheLoaded(ErrorContext* ctx) -> Status;
        auto ensureJobRunsCacheLoaded(ErrorContext* ctx) -> Status;
        void indexJobUnlocked(const JobInfo& job);
        void unindexJobUnlocked(const JobInfo& job);
        void indexJobRunUnlocked(const JobRunInfo& run);
        void unindexJobRunUnlocked(const JobRunInfo& run);
        static std::string normalizeJobName(const std::string& name);

        // Internal FK/constraint helpers (assume appropriate mutexes already held)
        auto deleteDependencyInternal(const ID& dependency_id, ErrorContext* ctx) -> Status;
        auto dropForeignKeyInternal(const ID& fk_id, ErrorContext* ctx) -> Status;
        auto dropConstraintInternal(const ID& constraint_id, ErrorContext* ctx) -> Status;
        auto getConstraintInternal(const ID& constraint_id, ConstraintInfo& constraint_out,
                                  ErrorContext* ctx) -> Status;

        // Internal sequence helpers (assume sequence_cache_mutex_ already held)
        auto getSequenceIdByNameInternal(const ID& schema_id, const std::string& name,
                                        ID& id_out, ErrorContext* ctx) -> Status;
        auto getSequenceInternal(const ID& schema_id, const std::string& name,
                                SequenceInfo& info_out, ErrorContext* ctx) -> Status;

        Database *db_;
        mutable CatalogMutex mutex_;
        mutable std::mutex history_mutex_;
        std::deque<TransactionHistoryEntry> transaction_history_;
        std::deque<WaitHistoryEntry> wait_history_;
        size_t transaction_history_limit_ = 1024;
        size_t wait_history_limit_ = 2048;
        mutable std::mutex digest_mutex_;
        std::unordered_map<std::string, StatementDigestEntry> digest_summary_;
        std::deque<std::string> digest_order_;
        size_t digest_summary_limit_ = 1024;
        std::array<uint64_t, kDigestHistogramBuckets> digest_histogram_global_{};
        std::unordered_map<std::string, StatementDigestEntry> digest_summary_by_account_;
        std::unordered_map<std::string, StatementDigestEntry> digest_summary_by_user_;
        std::unordered_map<std::string, StatementDigestEntry> digest_summary_by_host_;
        std::deque<std::string> digest_order_by_account_;
        std::deque<std::string> digest_order_by_user_;
        std::deque<std::string> digest_order_by_host_;

        // TRUNCATE TABLE async job tracking (ALPHA Phase 1 - DDL Modifications)
        std::unordered_map<uint64_t, std::shared_ptr<TruncateJob>> truncate_jobs_;
        std::mutex truncate_jobs_mutex_;
        std::atomic<uint64_t> next_truncate_job_id_{1};

        // Sequence cache (ALPHA Phase 1 - Sequences)
        std::unordered_map<ID, std::shared_ptr<SequenceState>> sequence_cache_;
        std::mutex sequence_cache_mutex_;
        std::unordered_map<std::pair<ID, std::string>, ID, PairHash<ID, std::string>>
            sequence_name_to_id_;  // (schema_id, normalized_name) -> sequence_id lookup
        std::mutex sequence_name_mutex_;  // Protect name map

        // View cache (ALPHA Phase 1 - Views)
        std::unordered_map<ID, ViewInfo> view_cache_;
        std::unordered_map<std::pair<ID, std::string>, ID, PairHash<ID, std::string>>
            view_name_to_id_;  // (schema_id, normalized_name) -> view_id lookup
        std::mutex view_cache_mutex_;

        // Dependency cache (Phase 5.2 - Dependencies table)
        std::unordered_map<ID, DependencyInfo> dependency_cache_;
        std::unordered_multimap<ID, ID> object_to_dependencies_;  // object_id -> dependency_ids
        std::mutex dependency_cache_mutex_;
        std::unordered_map<uint32_t, uint32_t> heap_page_tail_cache_;
        std::mutex heap_page_tail_mutex_;
        std::unordered_map<ID, ExceptionInfo, IDHash> exception_cache_;

        // Comment cache (Phase 5.2 - Comments table)
        std::unordered_map<ID, CommentInfo> comment_cache_;  // object_id -> CommentInfo
        std::mutex comment_cache_mutex_;

        // Object definition cache (DDL source + bytecode)
        std::unordered_map<ID, ObjectDefinitionInfo> object_definition_cache_;
        std::mutex object_definition_cache_mutex_;

        // Scheduler job cache/indexes (WS-4)
        bool job_cache_loaded_ = false;
        std::unordered_map<ID, JobInfo, IDHash> job_cache_;
        std::unordered_map<std::string, ID> job_name_index_;
        std::multimap<uint64_t, ID> job_due_index_;
        bool job_runs_cache_loaded_ = false;
        std::unordered_map<ID, JobRunInfo, IDHash> job_runs_cache_;
        std::unordered_multimap<ID, ID, IDHash> job_runs_by_job_;

        // Session cache (Phase 1.4 - Security System)
        std::unordered_map<ID, SessionInfo> session_cache_;  // session_id -> SessionInfo
        std::mutex session_cache_mutex_;

        // P1-12: Session timeout configuration
        SessionTimeoutConfig session_timeout_config_;
        std::mutex session_timeout_config_mutex_;

        // Policy cache (Phase 3.4.6 - RLS Expression Storage)
        std::unordered_map<ID, PolicyInfo> policy_cache_;  // policy_id -> PolicyInfo
        std::mutex policy_cache_mutex_;

        // Cached SYSTEM user UUID (resolved from users table)
        ID system_user_id_{};

        // TOAST table ID for policy expressions (Phase 3.4.8 - TOAST Persistence)
        ID policy_toast_table_id_{};  // UUID for sb_toast_policy table
        std::unique_ptr<ToastManager> policy_toast_manager_;  // TOAST manager for policy expressions
        std::unordered_map<uint32_t, std::string> toast_fallback_cache_;
        std::mutex toast_fallback_mutex_;
        uint32_t toast_fallback_next_oid_ = 1;

        // Object permissions cache (Phase 3.1 - SQL Object Permissions)
        std::unordered_map<ID, std::vector<ObjectPermissionInfo>> object_permissions_cache_;  // object_id -> permissions
        std::mutex object_permissions_cache_mutex_;

        // Foreign Key cache (ALPHA Phase A - FK Constraints)
        std::unordered_map<ID, ForeignKeyInfo> foreign_keys_cache_;  // fk_id -> ForeignKeyInfo
        std::unordered_multimap<ID, ID> table_child_fks_;  // child_table_id -> fk_ids
        std::unordered_multimap<ID, ID> table_parent_fks_;  // parent_table_id -> fk_ids
        std::mutex foreign_keys_cache_mutex_;

        // P1-9: Constraint cache (Unified Constraints Table)
        std::unordered_map<ID, ConstraintInfo> constraints_cache_;  // constraint_id -> ConstraintInfo
        std::unordered_multimap<ID, ID> table_constraints_;  // table_id -> constraint_ids
        std::unordered_map<std::pair<ID, std::string>, ID, PairHash<ID, std::string>>
            constraint_name_lookup_;  // (table_id, name) -> constraint_id
        std::mutex constraints_cache_mutex_;

        // Phase B caches - Synonyms, FDW, Server Registry, UDR Engine/Module
        std::unordered_map<ID, SynonymInfo> synonym_cache_;
        std::unordered_map<std::pair<ID, std::string>, ID, PairHash<ID, std::string>>
            synonym_name_lookup_;  // (schema_id, name) -> synonym_id
        std::vector<ID> public_synonyms_;  // List of public synonym IDs
        std::mutex synonym_cache_mutex_;

        std::unordered_map<ID, ForeignServerInfo> foreign_server_cache_;
        std::unordered_map<std::string, ID> foreign_server_name_to_id_;
        std::mutex foreign_server_cache_mutex_;

        std::unordered_map<ID, ForeignTableInfo> foreign_table_cache_;
        std::unordered_map<std::pair<ID, std::string>, ID, PairHash<ID, std::string>>
            foreign_table_name_lookup_;  // (schema_id, name) -> foreign_table_id
        std::mutex foreign_table_cache_mutex_;

        std::unordered_map<ID, UserMappingInfo> user_mapping_cache_;
        std::unordered_map<std::pair<ID, ID>, ID, PairHash<ID, ID>>
            user_mapping_lookup_;  // (user_id, server_id) -> mapping_id
        std::mutex user_mapping_cache_mutex_;

        std::unordered_map<ID, ServerRegistryInfo> server_registry_cache_;
        std::unordered_map<std::string, ID> server_registry_name_to_id_;
        std::mutex server_registry_cache_mutex_;

        std::unordered_map<ID, UDREngineInfo> udr_engine_cache_;
        std::unordered_map<std::string, ID> udr_engine_name_to_id_;
        std::mutex udr_engine_cache_mutex_;

        std::unordered_map<ID, UDRModuleInfo> udr_module_cache_;
        std::unordered_map<std::string, ID> udr_module_name_to_id_;
        std::mutex udr_module_cache_mutex_;

        // Internal helper methods (assume mutex_ is already held by caller)
        auto createSchemaInternal(const std::string &schema_name, const std::string &owner,
                                  ID &schema_id, const ID &parent_schema_id = ID(),
                                  ErrorContext *ctx = nullptr,
                                  const std::optional<ID> &forced_schema_id = std::nullopt) -> Status;

        // Helper to resolve owner name to UUID (Phase 5.1 - Owner UUID References)
        // Uses Users table lookup; "SYSTEM" resolves to the system user UUID.
        auto resolveOwnerUUID(const std::string &owner_name, ErrorContext* ctx) -> ID;

        // Note: storeStringInToast and loadStringFromToast are in public section (OPT-1/OPT-2, WP-5 EXEC-M6)

        // Index TID update helper (Phase 4 Task 4.1.5)
        // Updates all index entries for a table to reference new GPIDs after table migration
        // tid_mapping: Map of old GPID -> new GPID for heap pages
        // Returns Status::OK on success, error status otherwise
        auto updateIndexTIDs(const ID &table_id,
                             const std::unordered_map<TID, TID> &tid_mapping,
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
                                      const std::unordered_map<GPID, GPID> &page_mapping,
                                      std::unordered_map<TID, TID> *tid_mapping_out,
                                      ErrorContext *ctx = nullptr) -> Status;

        // Rollback page migration helper (Phase 5 Task 5.1.4)
        // Deallocates all target pages that were allocated during a failed migration
        // Iterates tid_mapping and frees all new_gpid pages using freePageGlobal()
        // Continues freeing even if some pages fail (logs orphaned pages)
        // Returns Status::OK if all pages freed, Status::IO_ERROR if some failed
        auto rollbackPageMigration(const std::unordered_map<GPID, GPID> &page_mapping,
                                    ErrorContext *ctx = nullptr) -> Status;

        // Catalog page layout - use higher page numbers (page 1 reserved for FSM).
        static constexpr uint32_t CATALOG_ROOT_PAGE = 3;
        static constexpr uint32_t SCHEMAS_TABLE_PAGE = 4;
        static constexpr uint32_t TABLES_TABLE_PAGE = 5;
        static constexpr uint32_t COLUMNS_TABLE_PAGE = 6;
        static constexpr uint32_t INDEXES_TABLE_PAGE = 7;
        static constexpr uint32_t TABLESPACES_TABLE_PAGE = 8;       // sb_tablespace
        static constexpr uint32_t TABLESPACE_FILES_TABLE_PAGE = 9;  // sb_tablespace_files

        // In-memory cache of catalog data
        std::unordered_map<ID, SchemaInfo> schema_cache_;
        std::unordered_map<ID, TableInfo> table_cache_;
        std::unordered_map<ID, std::vector<ColumnInfo>> column_cache_;
        std::unordered_map<ID, IndexInfo> index_cache_;
        std::unordered_map<uint16_t, TablespaceInfo> tablespace_cache_;  // keyed by tablespace_id

        // Resolver cache (Plan 02 - UUID resolution)
        std::unordered_map<ID, ResolvedObject, IDHash> resolver_by_id_;
        std::map<ResolverKey, ID> resolver_by_name_;
        std::unordered_map<std::pair<ID, std::string>, ID, PairHash<ID, std::string>>
            schema_name_lookup_;
        std::unordered_map<ID, ID, IDHash> schema_parent_lookup_;
        std::mutex resolver_cache_mutex_;

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

        // Database trigger storage (Firebird-style ON CONNECT/DISCONNECT/TRANSACTION events)
        std::unordered_map<ID, DatabaseTriggerInfo> db_trigger_cache_;  // keyed by trigger_id
        std::unordered_map<std::string, ID> db_trigger_name_to_id_;  // name -> ID lookup
        std::unordered_multimap<DatabaseTriggerEvent, ID> event_triggers_;  // event -> trigger_id (multiple per event)
        mutable std::mutex db_trigger_mutex_;  // Separate mutex for database trigger operations

        // PSQL - Stored Procedures and Functions (Phase 2 Task 10.2)
        std::unordered_map<std::string, FunctionInfo> functions_;    // keyed by function name
        std::unordered_map<std::string, ProcedureInfo> procedures_;  // keyed by procedure name
        mutable std::mutex psql_mutex_;  // Separate mutex for function/procedure operations

        // Statistics cache (OPT-1, OPT-2 - sb_statistic)
        // Key: combined hash of table_id and column_id
        std::unordered_map<uint64_t, StatisticInfo> statistic_cache_;  // getCacheKey(table_id, column_id) -> StatisticInfo
        mutable std::mutex statistic_mutex_;  // Separate mutex for statistics operations

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
        uint32_t column_permissions_table_page_ = 0; // Security Phase 3.3: Column-level permissions
        uint32_t default_privileges_table_page_ = 0; // Default privileges
        uint32_t policies_table_page_ = 0;       // Security Phase 3.4: Row-level security policies
        uint32_t object_permissions_table_page_ = 0; // Security Phase 3.1: SQL object permissions
        uint32_t statistics_table_page_ = 0;     // Will be allocated during init
        uint32_t collations_table_page_ = 0;     // Will be allocated during init
        uint32_t timezones_table_page_ = 0;      // Will be allocated during init
        uint32_t charsets_table_page_ = 0;       // Will be allocated during init (sb_charset)
        uint32_t collation_defs_table_page_ = 0; // Will be allocated during init (sb_collation)
        uint32_t tablespaces_table_page_ = TABLESPACES_TABLE_PAGE;           // sb_tablespace
        uint32_t tablespace_files_table_page_ = TABLESPACE_FILES_TABLE_PAGE; // sb_tablespace_files
        uint32_t extensions_table_page_ = 0;     // Extensions (Phase 5 placeholder)

        // Phase 6.1: New system table pages (16 new tables - added group_memberships and group_mappings)
        uint32_t dependencies_table_page_ = 0;      // Dependencies tracking (Phase 1.4)
        uint32_t comments_table_page_ = 0;          // Object comments (Phase 1.5)
        uint32_t object_definitions_table_page_ = 0; // Object DDL definitions (SQL + bytecode)
        uint32_t jobs_table_page_ = 0;              // Jobs (WS-4 Scheduler)
        uint32_t job_runs_table_page_ = 0;          // Job runs (WS-4 Scheduler)
        uint32_t job_dependencies_table_page_ = 0;  // Job dependencies (WS-4 Scheduler)
        uint32_t job_secrets_table_page_ = 0;       // Job secrets (WS-4 Scheduler)
        uint32_t users_table_page_ = 0;             // Users (Phase 2)
        uint32_t roles_table_page_ = 0;             // Roles (Phase 2)
        uint32_t groups_table_page_ = 0;            // Groups (Phase 2)
        uint32_t role_memberships_table_page_ = 0;  // Role memberships (Phase 2)
        uint32_t group_memberships_table_page_ = 0; // Group memberships (Phase 1.1 - Security System)
        uint32_t group_mappings_table_page_ = 0;    // Group mappings (Phase 1.1 - Security System)
        uint32_t procedures_table_page_ = 0;        // Stored procedures/functions (Phase 3)
        uint32_t procedure_params_table_page_ = 0;  // Procedure parameters (Phase 3)
        uint32_t domains_table_page_ = 0;           // User-defined domains (Phase 3)
        uint32_t udr_table_page_ = 0;               // UDR - external functions (Phase 3)
        uint32_t exceptions_table_page_ = 0;        // Exceptions (Phase 3)
        uint32_t packages_table_page_ = 0;          // Firebird packages (Phase 3)
        uint32_t emulation_types_table_page_ = 0;   // Emulation types (Phase 4)
        uint32_t emulation_servers_table_page_ = 0; // Emulation servers (Phase 4)
        uint32_t emulated_dbs_table_page_ = 0;      // Emulated databases (Phase 4)
        uint32_t foreign_keys_table_page_ = 0;      // Foreign keys (Phase D - FK Persistence)

        // Phase B system table pages
        uint32_t synonyms_table_page_ = 0;          // Synonyms (Phase B - Schema Architecture)
        uint32_t foreign_servers_table_page_ = 0;   // Foreign servers (Phase B - FDW)
        uint32_t foreign_tables_table_page_ = 0;    // Foreign tables (Phase B - FDW)
        uint32_t user_mappings_table_page_ = 0;     // User mappings (Phase B - FDW)
        uint32_t server_registry_table_page_ = 0;   // Server registry (Phase B - Distributed MVCC)
        uint32_t udr_engines_table_page_ = 0;       // UDR engines (Phase B - UDR Plugin)
        uint32_t udr_modules_table_page_ = 0;       // UDR modules (Phase B - UDR Plugin)
        uint32_t migration_history_table_page_ = 0; // Migration history (WP-2 CAT-L2)
        uint32_t dormant_transactions_table_page_ = 0; // Dormant transactions (Track 3.2)
        uint32_t prepared_transactions_table_page_ = 0; // Prepared transactions (2PC)
        uint32_t encryption_keys_table_page_ = 0;   // Encryption keys (Plan 03B)
        uint32_t authkeys_table_page_ = 0;          // AuthKeys (Plan 03)
        uint32_t sessions_table_page_ = 0;          // Sessions (Plan 03)
        uint32_t audit_log_table_page_ = 0;         // Audit log (Plan 03)
        uint32_t security_policy_epoch_table_page_ = 0; // Policy epoch (Plan 03)

        uint64_t security_policy_epoch_ = 0;

        // Internal methods
        auto writeCatalogRoot(ErrorContext *ctx) -> Status;
        auto readCatalogRoot(ErrorContext *ctx) -> Status;

        // Initialize TOAST for policy expressions (called without mutex to avoid deadlock)
        auto initializePolicyToast(ErrorContext *ctx) -> Status;

        /**
         * getMVRefreshSQL - Get SQL statements to refresh a materialized view
         *
         * WP-2 CAT-1/2: MV refresh implementation (executed at caller layer)
         *
         * @param view_id View ID
         * @param delete_sql_out Output for DELETE statement
         * @param insert_sql_out Output for INSERT statement
         * @param ctx Error context
         * @return Status::OK on success
         *
         * Returns the SQL statements needed to refresh a materialized view:
         * 1. DELETE FROM <storage_table> - Clear existing data
         * 2. INSERT INTO <storage_table> <view_definition> - Repopulate
         *
         * The caller (typically Executor) is responsible for executing these.
         * This design avoids circular dependencies between core and sblr.
         */
        auto getMVRefreshSQL(const ID &view_id,
                            std::string &delete_sql_out,
                            std::string &insert_sql_out,
                            ErrorContext *ctx = nullptr) -> Status;

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
        // Follows overflow page chain automatically
        template <typename RecordType, typename Predicate>
        auto findRecordInHeapPage(uint32_t page_id, Predicate predicate, ErrorContext *ctx)
            -> FindResult<RecordType>
        {
            BufferPool *bp = db_->buffer_pool();
            uint32_t current_page_id = page_id;

            while (current_page_id != 0)
            {
                void *page_buffer;
                Status status = bp->pinPage(current_page_id, &page_buffer, ctx);
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
                        bp->unpinPage(current_page_id, false, ctx);
                        return {Status::OK, i, found};
                    }

                    offset += sizeof(RecordType);
                }

                // Move to next page in chain
                uint32_t next_page = heap->next_page;
                bp->unpinPage(current_page_id, false, ctx);
                current_page_id = next_page;
            }

            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Record not found in catalog page");
            return {Status::NOT_FOUND, 0, RecordType{}};
        }

        // Helper to scan all records in a catalog heap page
        // Follows overflow page chain automatically
        template <typename RecordType, typename InfoType, typename Converter>
        auto scanHeapPage(uint32_t page_id, std::vector<InfoType> &results, Converter converter,
                          ErrorContext *ctx) -> Status
        {
            BufferPool *bp = db_->buffer_pool();
            uint32_t current_page_id = page_id;

            while (current_page_id != 0)
            {
                void *page_buffer;
                Status status = bp->pinPage(current_page_id, &page_buffer, ctx);
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

                // Move to next page in chain
                uint32_t next_page = heap->next_page;
                bp->unpinPage(current_page_id, false, ctx);
                current_page_id = next_page;
            }

            return Status::OK;
        }

        // Helper to scan filtered records in a catalog heap page
        // Follows overflow page chain automatically
        template <typename RecordType, typename InfoType, typename Filter, typename Converter>
        auto scanHeapPageWithFilter(uint32_t page_id, std::vector<InfoType> &results,
                                   Filter filter, Converter converter, ErrorContext *ctx) -> Status
        {
            BufferPool *bp = db_->buffer_pool();
            uint32_t current_page_id = page_id;

            while (current_page_id != 0)
            {
                void *page_buffer;
                Status status = bp->pinPage(current_page_id, &page_buffer, ctx);
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

                    // Only process valid records that match the filter
                    if (record->is_valid && filter(*record))
                    {
                        InfoType info;
                        converter(*record, info);
                        results.push_back(info);
                    }

                    offset += sizeof(RecordType);
                }

                // Move to next page in chain
                uint32_t next_page = heap->next_page;
                bp->unpinPage(current_page_id, false, ctx);
                current_page_id = next_page;
            }

            return Status::OK;
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
        // Follows overflow page chain automatically
        template <typename RecordType, typename InfoType, typename KeyType, typename Converter,
                  typename KeyExtractor>
        auto readRecordsFromHeapPage(uint32_t page_id, std::unordered_map<KeyType, InfoType> &cache,
                                     Converter converter, KeyExtractor key_extractor,
                                     ErrorContext *ctx) -> Status
        {
            BufferPool *bp = db_->buffer_pool();
            cache.clear();
            uint32_t current_page_id = page_id;

            while (current_page_id != 0)
            {
                void *page_buffer;
                Status status = bp->pinPage(current_page_id, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to read catalog heap page");
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
                        cache[key_extractor(info)] = info;
                    }

                    offset += sizeof(RecordType);
                }

                // Move to next page in chain
                uint32_t next_page = heap->next_page;
                bp->unpinPage(current_page_id, false, ctx);
                current_page_id = next_page;
            }

            return Status::OK;
        }

        // Helper to read records from a catalog heap page
        template <typename RecordType, typename InfoType>
        auto readRecordsToVector(uint32_t page_id, std::vector<InfoType> &results,
                                 std::function<bool(const RecordType &)> filter,
                                 std::function<void(const RecordType &, InfoType &)> converter,
                                 ErrorContext *ctx) -> Status;

    public:
        // Specific write/read methods using the generic helpers (public for testing)
        auto writeSchemaRecord(const SchemaInfo &schema, ErrorContext *ctx) -> Status;
        auto deleteSchemaRecord(const ID &schema_id, ErrorContext *ctx) -> Status;  // Phase A CRUD
        auto readSchemaRecords(ErrorContext *ctx) -> Status;
        auto writeTableRecord(const TableInfo &table, ErrorContext *ctx) -> Status;
        auto deleteTableRecord(const ID &table_id, ErrorContext *ctx) -> Status;
        auto readTableRecords(ErrorContext *ctx) -> Status;
        auto writeColumnRecords(const ID &table_id, const std::vector<ColumnInfo> &columns,
                                ErrorContext *ctx) -> Status;
        auto readColumnRecords(const ID &table_id, ErrorContext *ctx) -> Status;
        auto readSessionRecords(ErrorContext *ctx) -> Status;

        // Phase 6.2: Dependency persistence methods
        auto writeDependencyRecord(const DependencyInfo &dependency, ErrorContext *ctx) -> Status;
        auto deleteDependencyRecord(const ID &dependency_id, ErrorContext *ctx) -> Status;
        auto readDependencyRecords(ErrorContext *ctx) -> Status;

        // Phase 6.3: Comment persistence methods
        auto writeCommentRecord(const CommentInfo &comment, ErrorContext *ctx) -> Status;
        auto deleteCommentRecord(const ID &object_id, ErrorContext *ctx) -> Status;
        auto readCommentRecords(ErrorContext *ctx) -> Status;

        // Object definition persistence (DDL source + bytecode)
        auto writeObjectDefinitionRecord(const ObjectDefinitionInfo &definition,
                                         ErrorContext *ctx) -> Status;
        auto deleteObjectDefinitionRecord(const ID &object_id, ErrorContext *ctx) -> Status;
        auto readObjectDefinitionRecords(ErrorContext *ctx) -> Status;

        // Object persistence for sequences/views/triggers/procedures
        auto writeSequenceRecord(const SequenceState &state, ErrorContext *ctx) -> Status;
        auto readSequenceRecords(ErrorContext *ctx) -> Status;
        auto writeViewRecord(const ViewInfo &view, ErrorContext *ctx) -> Status;
        auto updateViewRecord(const ViewInfo &view, ErrorContext *ctx) -> Status;
        auto readViewRecords(ErrorContext *ctx) -> Status;
        auto writeTriggerRecord(const TriggerInfo &trigger, ErrorContext *ctx) -> Status;
        auto writeDatabaseTriggerRecord(const DatabaseTriggerInfo &trigger, ErrorContext *ctx) -> Status;
        auto readTriggerRecords(ErrorContext *ctx) -> Status;
        auto writeProcedureRecord(const ProcedureInfo &info, ErrorContext *ctx) -> Status;
        auto updateProcedureRecord(const ProcedureInfo &info, ErrorContext *ctx) -> Status;
        auto writeFunctionRecord(const FunctionInfo &info, ErrorContext *ctx) -> Status;
        auto updateFunctionRecord(const FunctionInfo &info, ErrorContext *ctx) -> Status;
        auto writeProcedureParameterRecords(const ID &procedure_id,
                                             const std::vector<ParameterInfo> &params,
                                             ErrorContext *ctx) -> Status;
        auto deleteProcedureParameterRecords(const ID &procedure_id, ErrorContext *ctx) -> Status;
        auto readProcedureRecords(ErrorContext *ctx) -> Status;
        auto readProcedureParameterRecords(ErrorContext *ctx) -> Status;

        // Phase B: Synonym and foreign table persistence
        auto writeSynonymRecord(const SynonymInfo &synonym, ErrorContext *ctx) -> Status;
        auto updateSynonymRecord(const SynonymInfo &synonym, ErrorContext *ctx) -> Status;
        auto readSynonymRecords(ErrorContext *ctx) -> Status;
        auto writeForeignTableRecord(const ForeignTableInfo &table, ErrorContext *ctx) -> Status;
        auto updateForeignTableRecord(const ForeignTableInfo &table, ErrorContext *ctx) -> Status;
        auto readForeignTableRecords(ErrorContext *ctx) -> Status;

        // P1-9: Constraint persistence
        auto writeConstraintRecord(const ConstraintInfo &constraint, ErrorContext *ctx) -> Status;
        auto updateConstraintRecord(const ConstraintInfo &constraint, ErrorContext *ctx) -> Status;
        auto deleteConstraintRecord(const ID &constraint_id, ErrorContext *ctx) -> Status;
        auto readConstraintRecords(ErrorContext *ctx) -> Status;

        // Phase B: Foreign server/user mapping persistence
        auto writeForeignServerRecord(const ForeignServerInfo &server, ErrorContext *ctx) -> Status;
        auto updateForeignServerRecord(const ForeignServerInfo &server, ErrorContext *ctx) -> Status;
        auto readForeignServerRecords(ErrorContext *ctx) -> Status;
        auto writeUserMappingRecord(const UserMappingInfo &mapping, ErrorContext *ctx) -> Status;
        auto updateUserMappingRecord(const UserMappingInfo &mapping, ErrorContext *ctx) -> Status;
        auto readUserMappingRecords(ErrorContext *ctx) -> Status;

        // Phase B: Server registry persistence
        auto writeServerRegistryRecord(const ServerRegistryInfo &server, ErrorContext *ctx) -> Status;
        auto updateServerRegistryRecord(const ServerRegistryInfo &server, ErrorContext *ctx) -> Status;
        auto readServerRegistryRecords(ErrorContext *ctx) -> Status;

        // Phase B: UDR engine/module persistence
        auto writeUDREngineRecord(const UDREngineInfo &engine, ErrorContext *ctx) -> Status;
        auto updateUDREngineRecord(const UDREngineInfo &engine, ErrorContext *ctx) -> Status;
        auto readUDREngineRecords(ErrorContext *ctx) -> Status;
        auto writeUDRModuleRecord(const UDRModuleInfo &module, ErrorContext *ctx) -> Status;
        auto updateUDRModuleRecord(const UDRModuleInfo &module, ErrorContext *ctx) -> Status;
        auto readUDRModuleRecords(ErrorContext *ctx) -> Status;

        // Phase D: Foreign key disk persistence
        auto readForeignKeyRecords(ErrorContext *ctx) -> Status;

        auto writeIndexRecord(const IndexInfo &index, ErrorContext *ctx) -> Status;
        auto deleteIndexRecord(const ID &index_id, ErrorContext *ctx) -> Status;
        auto readIndexRecords(ErrorContext *ctx) -> Status;
        auto updateTableColumnCount(const ID &table_id, uint32_t new_count, ErrorContext *ctx)
            -> Status;
        auto writeTablespaceRecord(const TablespaceInfo &tablespace, ErrorContext *ctx) -> Status;
        auto readTablespaceRecords(ErrorContext *ctx) -> Status;
        auto writeTablespaceFileRecord(uint16_t tablespace_id, uint16_t file_index,
                                       const std::string &file_path, uint64_t starting_page,
                                       uint64_t page_count, uint64_t max_pages, bool is_online,
                                       uint64_t created_time, uint64_t last_modified_time,
                                       ErrorContext *ctx) -> Status;
        auto writeTablespaceFileRecords(const TablespaceInfo &tablespace, ErrorContext *ctx) -> Status;
        auto readTablespaceFileRecords(ErrorContext *ctx) -> Status;
        auto deleteTablespaceFileRecords(uint16_t tablespace_id, ErrorContext *ctx) -> Status;
        auto updateTablespaceCounts(uint16_t tablespace_id, int64_t table_delta,
                                    int64_t index_delta, ErrorContext *ctx) -> Status;

        // OPT-1, OPT-2: Statistics persistence methods
        auto writeStatisticRecord(const StatisticInfo &info, ErrorContext *ctx) -> Status;
        auto deleteStatisticRecord(const ID &table_id, const ID &column_id, ErrorContext *ctx) -> Status;
        auto readStatisticRecords(ErrorContext *ctx) -> Status;
        auto getStatisticCacheKey(const ID &table_id, const ID &column_id) const -> uint64_t;

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
